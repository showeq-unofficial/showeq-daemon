#include "boxregistry.h"

#include "diagnosticmessages.h"

#include <QCryptographicHash>
#include <QHash>
#include <QHostAddress>

BoxRegistry::BoxRegistry(QObject* parent) : QObject(parent) {}

QString Box::summary() const
{
    const QString ip = QHostAddress(ntohl(client_ip)).toString();
    const char* primary = is_primary ? "*" : " ";
    // box_id already encodes identity (a hash of the character name once
    // promoted); never print the raw display_name — keep EQ character names
    // out of logs. promoted= conveys the same recon signal name-safely.
    return QStringLiteral("%1box %2  ip=%3  ports=%4->%5  pkts=%6  promoted=%7")
        .arg(primary)
        .arg(box_id)
        .arg(ip)
        .arg(ntohs(client_world_port))
        .arg(ntohs(server_world_port))
        .arg(packet_count)
        .arg(display_name.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"));
}

Box* BoxRegistry::observe(in_addr_t client_ip,
                          in_port_t client_world_port,
                          in_port_t server_world_port,
                          qint64    now_ms)
{
    if (Box* hit = lookupByWorld(client_ip,
                                 client_world_port,
                                 server_world_port)) {
        hit->last_seen_ms = now_ms;
        ++hit->packet_count;
        return hit;
    }

    auto b = std::make_unique<Box>();
    b->client_ip = client_ip;
    b->client_world_port = client_world_port;
    b->server_world_port = server_world_port;
    b->first_seen_ms = now_ms;
    b->last_seen_ms = now_ms;
    b->packet_count = 1;
    b->is_primary = m_boxes.empty();
    // Placeholder box_id; NamePromoter overwrites once OP_EnterWorld
    // arrives (Stage 2).
    b->box_id = QStringLiteral("p-%1-%2-%3")
        .arg(client_ip, 8, 16, QChar('0'))
        .arg(ntohs(client_world_port), 4, 16, QChar('0'))
        .arg(ntohs(server_world_port), 4, 16, QChar('0'));

    Box* raw = b.get();
    m_boxes.push_back(std::move(b));

    seqInfo("BoxRegistry: new box %s%s  ip=%s  client_port=%u  server_port=%u",
            raw->is_primary ? "*" : "",
            qUtf8Printable(raw->box_id),
            qUtf8Printable(QHostAddress(ntohl(raw->client_ip)).toString()),
            ntohs(raw->client_world_port),
            ntohs(raw->server_world_port));

    if (m_hook) m_hook(*raw);
    emit boxCreated(raw);

    // First non-merged Box becomes active by default. Subsequent
    // creations only fire changed() — active stays put until a client
    // explicitly switches via SetActiveBox.
    if (m_activeBoxId.isEmpty()) m_activeBoxId = raw->box_id;

    emit changed();
    return raw;
}

Box* BoxRegistry::findById(const QString& box_id)
{
    for (auto& b : m_boxes) {
        if (b->is_merged()) continue;
        if (b->box_id == box_id) return b.get();
    }
    return nullptr;
}

bool BoxRegistry::setActiveBoxId(const QString& box_id)
{
    Box* target = findById(box_id);
    if (!target) return false;
    if (m_activeBoxId == box_id) return true;
    Box* old = findById(m_activeBoxId);
    m_activeBoxId = box_id;
    emit activeBoxChanged(old, target);
    emit changed();
    return true;
}

void BoxRegistry::onPromoted(Box* box, const QString& old_box_id)
{
    if (!box) return;
    if (m_activeBoxId == old_box_id)
        m_activeBoxId = box->box_id;
    // If the promoted box turned out to be a merge target, its box_id
    // collides with an existing one; m_activeBoxId may point at the
    // pre-existing parent already — leave it as-is, the changed()
    // re-emit gives the client a fresh picture either way.
    emit changed();
}

void BoxRegistry::notifyChanged()
{
    emit changed();
}

Box* BoxRegistry::primary()
{
    return m_boxes.empty() ? nullptr : m_boxes.front().get();
}

Box* BoxRegistry::lookupByWorld(in_addr_t client_ip,
                                in_port_t client_world_port,
                                in_port_t server_world_port)
{
    for (auto& b : m_boxes) {
        if (b->client_ip == client_ip &&
            b->client_world_port == client_world_port &&
            b->server_world_port == server_world_port) {
            return b.get();
        }
    }
    return nullptr;
}

Box* BoxRegistry::lookupByName(const QString& name, const Box* exclude)
{
    if (name.isEmpty()) return nullptr;
    for (auto& b : m_boxes) {
        if (b.get() == exclude) continue;
        if (b->is_merged()) continue;
        if (b->display_name == name) return b.get();
    }
    return nullptr;
}

Box* BoxRegistry::promoteByName(Box* box, const QString& name)
{
    if (!box || name.isEmpty()) return nullptr;
    if (box->display_name == name) return nullptr;  // already promoted

    const QString old_box_id = box->box_id;
    box->display_name = name;
    // Stable, scrub-safe id = hash of the character name. The same
    // character (a re-handshake) hashes to the same box_id, so the picker
    // shows one entry; display_name carries the human-readable label.
    const QByteArray digest = QCryptographicHash::hash(
        name.toLatin1(), QCryptographicHash::Sha256);
    box->box_id =
        QStringLiteral("b-") + QString::fromLatin1(digest.left(8).toHex());

    Box* parent = lookupByName(name, box);
    if (parent) {
        box->merged_into = parent->box_id;
        seqInfo("BoxRegistry: box promoted, merged into character %s",
                qUtf8Printable(parent->box_id));
    } else {
        seqInfo("BoxRegistry: box promoted to character %s",
                qUtf8Printable(box->box_id));
    }
    onPromoted(box, old_box_id);

    // If the currently-active box is a stale, never-promoted placeholder
    // (the first-seen box was a partial/dead session that never resolved a
    // character — common when a capture/daemon starts mid-session), adopt
    // this newly-decoded DISTINCT character as the active one. Otherwise
    // SessionAdapter would stream an empty box while the real data lives in
    // a non-primary box.
    if (!parent) {
        Box* active = findById(m_activeBoxId);
        if (!active || active->display_name.isEmpty()) {
            m_activeBoxId = box->box_id;
        }
    }

    // If this box belongs to the ACTIVE character, the current decode box
    // just rolled to it (newest zone session) — nudge SessionAdapter to
    // rebind so the live view follows the character into its new zone.
    const QString charId = parent ? parent->box_id : box->box_id;
    if (m_activeBoxId == charId) {
        emit activeBoxChanged(nullptr, box);
    }
    emit changed();
    return parent;
}

Box* BoxRegistry::currentBoxFor(const QString& box_id)
{
    Box* parent = findById(box_id);
    if (!parent) return nullptr;
    Box* current = parent;
    // Among the character's boxes (the non-merged parent + every box merged
    // into it), the live one is the most recently seen.
    for (auto& b : m_boxes) {
        if (b->merged_into == parent->box_id &&
            b->first_seen_ms >= current->first_seen_ms) {
            current = b.get();
        }
    }
    return current;
}

Box* BoxRegistry::lookupBoundZone(in_addr_t client_ip,
                                  in_port_t client_zone_port,
                                  in_port_t server_zone_port)
{
    for (auto& b : m_boxes) {
        if (b->is_merged()) continue;
        if (b->client_ip == client_ip &&
            b->zone_client_port == client_zone_port &&
            b->zone_server_port_bound == server_zone_port) {
            return b.get();
        }
    }
    return nullptr;
}

Box* BoxRegistry::lookupByExpectedZone(in_addr_t client_ip,
                                       in_addr_t server_ip,
                                       in_port_t server_zone_port)
{
    // Among boxes that match (client_ip, server_port) and are NOT already
    // bound to a live zone session, pick the one notified most recently by
    // OP_ZoneServerInfo. The client connects immediately after being told
    // where to zone, so when two same-host boxes await the same server port
    // the newest notification owns the SessionRequest arriving now. Without
    // this tiebreak the first box in the list (always the primary) would
    // grab every session and the others would never bind.
    Box* best = nullptr;
    for (auto& b : m_boxes) {
        if (b->is_merged()) continue;
        if (b->client_ip != client_ip) continue;
        // Skip boxes already bound to a live zone session — ZoneServerObserver
        // clears the binding on a fresh OP_ZoneServerInfo so a re-zoning box
        // becomes available again.
        if (b->zone_client_port != 0) continue;
        // ZoneServerObserver defers hostname→IP resolution (sets only the
        // port), so expected_zone_server_ip is usually 0. Match on
        // (client_ip, server_port) in that case; only enforce the IP when
        // it's actually known.
        if (b->expected_zone_server_ip != 0 &&
            b->expected_zone_server_ip != server_ip) continue;
        if (b->expected_zone_server_port != server_zone_port) continue;
        if (!best || b->zone_await_ms > best->zone_await_ms) best = b.get();
    }
    return best;
}

size_t BoxRegistry::distinctCount() const
{
    size_t n = 0;
    for (const auto& b : m_boxes) if (!b->is_merged()) ++n;
    return n;
}

QString BoxRegistry::dumpString() const
{
    if (m_boxes.empty())
        return QStringLiteral("BoxRegistry: empty");

    // Count aliases per parent box_id so each parent line shows
    // "(+N rehandshakes)".
    QHash<QString, int> aliasCount;
    for (const auto& b : m_boxes) {
        if (b->is_merged()) ++aliasCount[b->merged_into];
    }

    QString out = QStringLiteral(
        "BoxRegistry: %1 distinct, %2 total (*=primary, +N=re-handshakes)\n")
        .arg(distinctCount())
        .arg(m_boxes.size());
    for (const auto& b : m_boxes) {
        if (b->is_merged()) continue;
        out += QStringLiteral("  ") + b->summary();
        const int extras = aliasCount.value(b->box_id, 0);
        if (extras > 0)
            out += QStringLiteral("  (+%1 re-handshakes)").arg(extras);
        out += QChar('\n');
    }
    return out;
}
