#include "boxregistry.h"

#include "diagnosticmessages.h"

#include <QHash>
#include <QHostAddress>

BoxRegistry::BoxRegistry(QObject* parent) : QObject(parent) {}

QString Box::summary() const
{
    const QString ip = QHostAddress(ntohl(client_ip)).toString();
    const QString name = display_name.isEmpty() ? QStringLiteral("(unknown)")
                                                : display_name;
    const char* primary = is_primary ? "*" : " ";
    return QStringLiteral("%1box %2  ip=%3  ports=%4->%5  pkts=%6  name=%7")
        .arg(primary)
        .arg(box_id)
        .arg(ip)
        .arg(ntohs(client_world_port))
        .arg(ntohs(server_world_port))
        .arg(packet_count)
        .arg(name);
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

    // First non-merged Box becomes active by default. Subsequent
    // creations only fire changed() — active stays put until a client
    // explicitly switches via SetActiveBox.
    if (m_activeBoxId.isEmpty()) m_activeBoxId = raw->box_id;

    emit changed();
    return raw;
}

bool BoxRegistry::setActiveBoxId(const QString& box_id)
{
    // Validate against current non-merged boxes.
    for (auto& b : m_boxes) {
        if (b->is_merged()) continue;
        if (b->box_id == box_id) {
            if (m_activeBoxId == box_id) return true;
            m_activeBoxId = box_id;
            emit changed();
            return true;
        }
    }
    return false;
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
