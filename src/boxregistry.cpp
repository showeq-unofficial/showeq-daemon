#include "boxregistry.h"

#include "diagnosticmessages.h"

#include <algorithm>

#include <QCryptographicHash>
#include <QHash>
#include <QHostAddress>

namespace {
// SCAFFOLD (character refactor): stable label for --list-boxes / summary().
const char* stateName(SessionState s)
{
    switch (s) {
        case SessionState::Pending:    return "pending";
        case SessionState::Attached:   return "attached";
        case SessionState::Superseded: return "superseded";
        case SessionState::Reaped:     return "reaped";
    }
    return "?";
}
}  // namespace

BoxRegistry::BoxRegistry(QObject* parent) : QObject(parent) {}

QString Box::summary() const
{
    const QString ip = QHostAddress(ntohl(client_ip)).toString();
    const char* primary = is_primary ? "*" : " ";
    // box_id already encodes identity (a hash of the character name once
    // promoted); never print the raw display_name — keep EQ character names
    // out of logs. promoted= conveys the same recon signal name-safely.
    return QStringLiteral("%1box %2  ip=%3  ports=%4->%5  pkts=%6  promoted=%7  state=%8")
        .arg(primary)
        .arg(box_id)
        .arg(ip)
        .arg(ntohs(client_world_port))
        .arg(ntohs(server_world_port))
        .arg(packet_count)
        .arg(display_name.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"))
        .arg(QString::fromLatin1(stateName(state)));
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

    // Inc 4: maintain the Character store here — this is the ONE hook both
    // promotion paths funnel through (NamePromoter's OP_EnterWorld path sets
    // box_id/display_name/merged_into then calls onPromoted; BoxRegistry::
    // promoteByName does the same then calls onPromoted). charId = the anchor
    // (merged_into if this session merged into an earlier one, else its own id).
    // Runs BEFORE changed() so the SessionAdapter follow reads the fresh store.
    const QString charId = box->is_merged() ? box->merged_into : box->box_id;
    upsertCharacter(charId, box->display_name, box);

    emit changed();
}

void BoxRegistry::notifyChanged()
{
    emit changed();
}

int BoxRegistry::evictStale(qint64 now_ms, qint64 ttl_ms)
{
    if (ttl_ms <= 0) return 0;
    const qint64 cutoff = now_ms - ttl_ms;
    const QString activeId = m_activeBoxId;

    // Collect victims one character group at a time. A group is a
    // non-merged parent plus every Box merged into it; processing whole
    // groups is what keeps us from removing a parent (the identity anchor
    // every alias' merged_into points at) while one of its aliases lives.
    std::vector<Box*> victims;
    for (auto& up : m_boxes) {
        Box* parent = up.get();
        if (parent->is_merged()) continue;     // visited with its parent

        std::vector<Box*> group{parent};
        for (auto& a : m_boxes)
            if (a->merged_into == parent->box_id) group.push_back(a.get());

        // The live decode box of the character is the newest by first_seen
        // (currentBoxFor's rule) — SessionAdapter binds here, so it must
        // survive as long as the character is around.
        Box* current = parent;
        for (Box* b : group)
            if (b->first_seen_ms >= current->first_seen_ms) current = b;

        const bool isPrimaryGroup = parent->is_primary;  // primary never merges
        const bool isActiveGroup  = (parent->box_id == activeId);
        const bool currentFresh   = current->last_seen_ms >= cutoff;

        // The character is gone (logged off / camped) once even its live
        // box has been silent past the TTL. Reap the whole group then —
        // unless it's the primary or the box the user is actively viewing.
        const bool reapWholeGroup =
            !isPrimaryGroup && !isActiveGroup && !currentFresh;

        for (Box* b : group) {
            if (b->is_primary) continue;       // the global-stream box: never
            if (reapWholeGroup) { victims.push_back(b); continue; }
            // Group survives: reap only its stale, superseded aliases.
            // Keep the live box, any still-fresh box, and the parent (its
            // box_id anchors the aliases that remain).
            if (b == current) continue;
            if (b == parent)  continue;
            if (b->last_seen_ms >= cutoff) continue;
            victims.push_back(b);
        }
    }

    if (victims.empty()) return 0;

    // Let subscribers (EQPacket streams/observers, DaemonApp ManagerSets)
    // release their per-box resources while the Box is still valid.
    for (Box* v : victims) {
        seqInfo("BoxRegistry: evicting idle box %s", qUtf8Printable(v->box_id));
        v->state = SessionState::Reaped;   // SCAFFOLD (state machine): terminal
        emit boxAboutToBeRemoved(v);
    }

    // Now free the Boxes. Erase by identity — the per-victim erase is O(N)
    // but victim counts are small (a handful per sweep at most).
    for (Box* v : victims) {
        m_boxes.erase(
            std::remove_if(m_boxes.begin(), m_boxes.end(),
                [v](const std::unique_ptr<Box>& up) { return up.get() == v; }),
            m_boxes.end());
    }

    // Inc 4 step 1: drop Characters whose live session was reaped. evictStale
    // protects a character's current box, so its session is only a victim on a
    // whole-group reap (the character logged off / camped). Pointer comparison
    // only — the Box is freed but never dereferenced here.
    m_characters.erase(
        std::remove_if(m_characters.begin(), m_characters.end(),
            [&victims](const Character& c) {
                return std::find(victims.begin(), victims.end(), c.session)
                       != victims.end();
            }),
        m_characters.end());

    emit changed();
    return int(victims.size());
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

    // The Character store is maintained in onPromoted() (called above), so by
    // here currentBoxFor/currentSessionFor already resolve the fresh session.
    const QString charId = parent ? parent->box_id : box->box_id;

    // If this box belongs to the ACTIVE character, the current decode box just
    // rolled to it (newest zone session) — nudge SessionAdapter to rebind so the
    // live view follows the character into its new zone.
    if (m_activeBoxId == charId) {
        emit activeBoxChanged(nullptr, box);
    }

    // SCAFFOLD (state machine): reclassify this character's sessions — the live
    // one (currentBoxFor) is ATTACHED, every other is SUPERSEDED.
    Box* live = currentBoxFor(charId);
    for (auto& up : m_boxes) {
        Box* b = up.get();
        if (b->box_id != charId && b->merged_into != charId) continue;
        b->state = (b == live) ? SessionState::Attached : SessionState::Superseded;
    }

    emit changed();
    return parent;
}

Box* BoxRegistry::currentBoxFor(const QString& box_id)
{
    // Inc 4 step 2: Character-owned — the live session is the store's session.
    // The old merge-scan (newest box with merged_into == anchor) is now
    // maintained into c.session by upsertCharacter.
    for (const Character& c : m_characters)
        if (c.id == box_id)
            return c.session;
    // Not a promoted character (a Pending placeholder id, or unknown): resolve
    // the box itself — matches the old findById + no-merged-children path.
    return findById(box_id);
}

Box* BoxRegistry::lookupBoundZone(in_addr_t client_ip,
                                  in_port_t client_zone_port,
                                  in_port_t server_zone_port)
{
    for (auto& b : m_boxes) {
        // Do NOT skip merged boxes here. `merged_into` is an IDENTITY/UI
        // grouping (one picker entry per character); it says nothing about
        // the physical session. A re-handshake/zone-change box is promoted —
        // and thus merged into its character's parent — at OP_PlayerProfile,
        // which lands moments after its zone session binds. That box keeps its
        // OWN streams + ManagerSet and is still actively decoding; skipping it
        // would make the just-bound session unroutable, so its spawn burst and
        // ongoing position updates would be dropped (or grabbed by another
        // same-host box via lookupByExpectedZone). Routing keys on the unique
        // zone 5-tuple, which is identity-agnostic.
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
        // Merged boxes are NOT skipped: a box can reuse its world socket on a
        // re-zone (ZoneServerObserver clears its binding to await the next
        // SessionRequest) after it's already been identified+merged. It must
        // still be able to rebind its own session. The `zone_client_port != 0`
        // guard below already excludes boxes bound to a live session, so this
        // only ever re-binds a genuinely-available box. (Merge is identity,
        // not routing — see lookupBoundZone.)
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

// SCAFFOLD (character refactor): the new-model read API, computed over the
// current box storage. One Character per distinct identity (non-merged box);
// its live session is currentBoxFor. When storage flips to Character-owned this
// becomes a direct walk of the character map and merged_into disappears.
std::vector<Character> BoxRegistry::characters()
{
    std::vector<Character> out;
    for (auto& up : m_boxes) {
        Box* b = up.get();
        if (b->is_merged()) continue;          // aliases fold into their anchor
        Character c;
        c.name    = b->display_name;
        c.id      = b->box_id;
        c.session = currentBoxFor(b->box_id);  // the ATTACHED live session
        for (auto& a : m_boxes)
            if (a->merged_into == b->box_id) ++c.aliasCount;
        out.push_back(std::move(c));
    }
    return out;
}

void BoxRegistry::upsertCharacter(const QString& id, const QString& name,
                                  Box* newSession)
{
    for (Character& c : m_characters) {
        if (c.id == id) {
            c.name = name;
            // Adopt the newer session; a late-promoting OLDER box must not steal
            // the live session from a newer one (guard on first_seen).
            if (!c.session ||
                newSession->first_seen_ms >= c.session->first_seen_ms)
                c.session = newSession;
            return;
        }
    }
    Character c;
    c.name    = name;
    c.id      = id;
    c.session = newSession;
    m_characters.push_back(std::move(c));
}

Box* BoxRegistry::currentSessionFor(const QString& name)
{
    if (name.isEmpty()) return nullptr;
    // Inc 4 step 1: authoritative — resolve via the Character store (pin to the
    // character, not the last-zoned box). Equivalent to the old anchor-scan +
    // currentBoxFor, which is now maintained into c.session by upsertCharacter.
    for (const Character& c : m_characters)
        if (c.name == name)
            return c.session;
    return nullptr;
}
