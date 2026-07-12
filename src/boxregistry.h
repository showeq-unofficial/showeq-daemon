#ifndef BOXREGISTRY_H
#define BOXREGISTRY_H

// BoxRegistry observes every EQ client (a "box") visible on the wire.
//
// Stage 1: pure observation by L4 5-tuple.
// Stage 2: each non-primary Box owns its own world-stream pair so the
//          daemon can run a NamePromoter on each box's OP_EnterWorld
//          (world C>S, char name at offset 0). The primary box keeps
//          using the existing global stream pointers in EQPacket so
//          state-manager wiring is untouched. Zone-stream decoding
//          remains primary-only (Stage 3+ work).
//
// Identity is two-layer:
//   - wire routing key  (now)             : (client_ip,
//                                             client_world_port,
//                                             server_world_port)
//   - stable id (post OP_EnterWorld)      : hash of charProfileStruct
//                                             .name. Set by
//                                             NamePromoter (Stage 2).

#include <netinet/in.h>

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>
#include <vector>

class EQPacketStream;

// SCAFFOLD (character refactor — see docs / the BoxRegistry design note §07).
// The lifecycle a session moves through in the target model. Today this is a
// non-behavioral annotation stamped alongside the existing tuple+merge logic
// (routing and decode still key on the wire tuple and merged_into); it makes
// the state machine concrete so consumers can migrate onto it incrementally.
//   Pending    — observed by tuple, no character yet (the "Unknown" window)
//   Attached   — the character's live session; decodes the zone
//   Superseded — a newer session for the same character took over (detaching)
//   Reaped     — freed by evictStale
enum class SessionState : uint8_t { Pending, Attached, Superseded, Reaped };

class Box {
public:
    in_addr_t client_ip          = 0;
    in_port_t client_world_port  = 0;
    in_port_t server_world_port  = 0;

    qint64    first_seen_ms      = 0;
    qint64    last_seen_ms       = 0;
    quint64   packet_count       = 0;

    // First Box created is the "primary" — its packets keep flowing
    // through the existing global EQPacketStreams in EQPacket so all
    // the wireBoxPipeline handler wiring stays intact. Non-primary boxes
    // get the per-box streams below.
    bool      is_primary         = false;

    // Per-box world + zone streams. Non-null only on non-primary
    // boxes — the BoxCreatedHook installs them. Primary box re-uses
    // EQPacket's global streams so the existing handler wiring
    // stays intact in Stages 2/3a.
    EQPacketStream* world_c2s    = nullptr;
    EQPacketStream* world_s2c    = nullptr;
    EQPacketStream* zone_c2s     = nullptr;
    EQPacketStream* zone_s2c     = nullptr;

    // Set by the per-box ZoneServerObserver on the box's
    // world_s2c when OP_ZoneServerInfo arrives. Used by
    // EQPacket::dispatchPacket to bind incoming zone-traffic
    // 5-tuples to this box.
    in_addr_t       expected_zone_server_ip   = 0;
    in_port_t       expected_zone_server_port = 0;

    // Set when this box's first zone-stream packet binds the
    // ephemeral client port (via the (client_ip, server_port)
    // match against expected_zone_server_*).
    in_port_t       zone_client_port          = 0;
    in_port_t       zone_server_port_bound    = 0;

    // Wall-clock ms when this box last received OP_ZoneServerInfo (set by
    // ZoneServerObserver). When two same-host boxes await a zone on the
    // same server port, lookupByExpectedZone binds the MOST RECENTLY
    // notified one — the client connects immediately after being told
    // where to go, so the newest notification owns the next SessionRequest.
    qint64          zone_await_ms             = 0;

    // Placeholder until OP_EnterWorld arrives. Hex string of the
    // creation-time 5-tuple — stable for the box's lifetime.
    QString   box_id;

    // Set by NamePromoter from OP_EnterWorld payload @ offset 0
    // (zero-padded char name, 64-byte slot). Empty until promoted.
    QString   display_name;

    // box_id of the first-seen Box that holds this character. Set by
    // NamePromoter when promotion reveals a duplicate name (e.g. a
    // single client's zone rehandshakes open new 5-tuples that all
    // resolve to the same character). Merged boxes are hidden from
    // dumpString() and counted as aliases of their parent.
    QString   merged_into;

    bool      is_merged() const { return !merged_into.isEmpty(); }

    // SCAFFOLD (character refactor): where this session sits in the lifecycle
    // (SessionState above). Set by observe/promoteByName/evictStale; not yet
    // consulted by routing or decode.
    SessionState state = SessionState::Pending;

    // Human-readable summary for --list-boxes output.
    QString   summary() const;
};

// SCAFFOLD (character refactor): the target-model read shape. A character is a
// NAME (stable across zones) with exactly one live session — the box currently
// decoding. Today this is a VIEW computed over the box storage
// (BoxRegistry::characters); a later increment makes it the authoritative
// entity and retires merged_into / currentBoxFor.
struct Character {
    QString name;                 // display_name; empty for a still-anonymous box
    QString id;                   // stable id (the hashed box_id / anchor id)
    Box*    session    = nullptr; // the ATTACHED live session (== currentBoxFor)
    int     aliasCount = 0;       // superseded re-handshake sessions still around
};

class BoxRegistry : public QObject {
    Q_OBJECT
public:
    explicit BoxRegistry(QObject* parent = nullptr);

    // Fires once per newly-created Box. Stage 2: EQPacket installs a
    // hook that allocates per-box world streams + NamePromoter for
    // every non-primary box. The first Box created has is_primary=true
    // and the hook is a no-op for it (primary keeps using EQPacket's
    // global streams).
    using BoxCreatedHook = std::function<void(Box&)>;
    void setBoxCreatedHook(BoxCreatedHook h) { m_hook = std::move(h); }

    // Called on every world-port packet seen. Creates a new Box on
    // first sighting of a (client_ip, client_world_port,
    // server_world_port) triple; updates last_seen / packet_count
    // otherwise. Returns a pointer to the (possibly newly created)
    // Box. Pointers are stable for the lifetime of the registry
    // (Boxes live in unique_ptr).
    Box* observe(in_addr_t client_ip,
                 in_port_t client_world_port,
                 in_port_t server_world_port,
                 qint64    now_ms);

    // First Box seen — the legacy single-client pipeline decodes its
    // traffic via the existing global streams in EQPacket.
    Box* primary();

    // O(N) by box_id (skips merged boxes).
    Box* findById(const QString& box_id);

    // O(N) lookup by world 5-tuple. nullptr if not observed.
    Box* lookupByWorld(in_addr_t client_ip,
                       in_port_t client_world_port,
                       in_port_t server_world_port);

    // Find a Box whose zone_client_port/zone_server_port_bound matches
    // this 5-tuple. nullptr if no box has been bound to that zone
    // connection yet (caller should fall back to expected-server match).
    Box* lookupBoundZone(in_addr_t client_ip,
                         in_port_t client_zone_port,
                         in_port_t server_zone_port);

    // Find a Box whose `expected_zone_server_*` matches a zone-stream
    // SessionRequest target (client_ip, server_port). Used to bind a
    // freshly-observed zone tuple to the box that's about to use it.
    // nullptr if no box is expecting a zone connection to that endpoint.
    Box* lookupByExpectedZone(in_addr_t client_ip,
                              in_addr_t server_ip,
                              in_port_t server_zone_port);

    // Find a Box with this character name (skipping `exclude`). Used
    // by NamePromoter at promotion time to detect re-handshakes of
    // the same character. nullptr if no other Box has the name.
    Box* lookupByName(const QString& name, const Box* exclude = nullptr);

    // Promote a box to a character name (hashed box_id + display_name) and,
    // if another box already holds that character (a zone-change
    // re-handshake), merge this box into it. Idempotent. Returns the parent
    // if merged, else nullptr. Emits activeBoxChanged when the merge rolls
    // the active character's CURRENT decode box to this newest one. The
    // caller must pass the authoritative name (charProfileStruct.name) — NOT
    // Player::name(), which returns the "You" default until the per-Player
    // auto-detect flags settle (long after OP_PlayerProfile's newPlayer).
    Box* promoteByName(Box* box, const QString& name);

    // The CURRENT (latest-seen) box for a character, given any of its
    // box_ids. A character accumulates one box per zone session
    // (re-handshakes merge into the first); only the latest decodes the
    // live zone, so SessionAdapter resolves managers through this.
    Box* currentBoxFor(const QString& box_id);

    // SCAFFOLD (character refactor): the name-keyed read API the picker + the
    // SessionAdapter will migrate onto. characters() is one entry per distinct
    // identity — the new-model view of the box list. currentSessionFor resolves
    // a character's live session by NAME: the "pin to the character, not the
    // last-zoned session" primitive. Both are computed over the current storage
    // for now; a later increment makes Character the storage and these O(1).
    std::vector<Character> characters();
    Box* currentSessionFor(const QString& name);

    size_t size() const { return m_boxes.size(); }

    // Iterator access to walk every box's streams. The exposed type is the
    // unique_ptr-of-Box vector; callers use `for (auto& up : ...)`.
    const std::vector<std::unique_ptr<Box>>& boxes() const { return m_boxes; }

    // Count of distinct character identities — Boxes that aren't
    // merged into another.
    size_t distinctCount() const;

    // Iterate boxes (for --list-boxes dump).
    template <typename F>
    void forEach(F&& f) const {
        for (const auto& b : m_boxes) f(*b);
    }

    QString dumpString() const;

    // Active box drives which decode pipeline the SessionAdapter
    // streams to its client. v1 (Stage 4 of docs/MULTIBOX_PLAN.md):
    // cosmetic only — Stage 3 wires this to actual per-box state-
    // manager decode. The first non-merged Box becomes active on
    // creation; setActiveBoxId() emits changed().
    const QString& activeBoxId() const { return m_activeBoxId; }
    bool setActiveBoxId(const QString& box_id);

    // Called by NamePromoter after it has overwritten box->box_id
    // with the SHA-256 name hash. We replace m_activeBoxId if it was
    // tracking the now-stale placeholder id, then emit changed().
    void onPromoted(Box* box, const QString& old_box_id);

    // Called by NamePromoter post-merge so subscribers (SessionAdapter)
    // can re-emit BoxListUpdated. Also fired by observe() on every new
    // Box and by setActiveBoxId().
    void notifyChanged();

    // Reclaim boxes whose EQ session has gone idle (no packet seen for
    // ttl_ms). Each zone change opens a fresh world socket, so a long
    // multibox session accumulates one Box per character per zone; this
    // reaps the superseded ones. Eviction is group-aware (a character =
    // a non-merged parent + its merged re-handshake aliases):
    //   - superseded, stale aliases are reaped individually;
    //   - a whole character group is reaped once its LIVE decode box
    //     (currentBoxFor) is itself stale (the character logged off);
    //   - the primary box, the active character's live box, and every
    //     still-active character's live box are never reaped — and a
    //     non-merged parent is only removed when its whole group goes,
    //     so a surviving alias never orphans its identity anchor.
    // Emits boxAboutToBeRemoved(box) for each victim BEFORE freeing it
    // (subscribers hold raw Box* and must release per-box resources
    // first), then a single changed(). Returns the number removed.
    // now_ms / ttl_ms are caller-supplied (wall clock in production) so
    // the sweep is unit-testable with injected timestamps.
    int evictStale(qint64 now_ms, qint64 ttl_ms);

signals:
    // Fires every time the registry state changes (add, promote,
    // merge, active-switch). SessionAdapter listens to re-emit
    // BoxListUpdated.
    void changed();
    // Fires once per newly-created Box, AFTER the BoxCreatedHook has
    // populated its streams. DaemonApp listens here to wire opcode
    // dispatch on the box's per-box zone streams (Stage 3b of
    // docs/MULTIBOX_PLAN.md) — separate from the synchronous hook
    // EQPacket uses for stream allocation.
    void boxCreated(Box* box);
    // Fires once per Box about to be evicted by evictStale(), BEFORE the
    // Box is freed. EQPacket tears down the box's streams + observers and
    // DaemonApp tears down its ManagerSet in response — the reverse of the
    // boxCreated construction path. The Box* is still valid for the
    // duration of the slot; do not retain it past the call.
    void boxAboutToBeRemoved(Box* box);
    // Fires when the active box pointer changes.
    void activeBoxChanged(Box* oldBox, Box* newBox);

private:
    // unique_ptr → stable Box* across vector growth. We hand pointers
    // out to lookup callers AND to per-box stream connect()s.
    std::vector<std::unique_ptr<Box>> m_boxes;
    BoxCreatedHook m_hook;
    QString m_activeBoxId;

    // Inc 4 (character-registry) step 1: authoritative Character store, keyed by
    // the character's anchor id. Maintained in promoteByName (upsert) + evictStale
    // (prune whole-group reaps). currentSessionFor() reads from it; characters()
    // and currentBoxFor() still compute over the boxes this step (dual-maintained
    // with merged_into until the later sub-steps retire the merge anchors).
    std::vector<Character> m_characters;

    // Upsert the Character for anchor `id` with `name`, adopting `newSession` as
    // its live box (the just-promoted, newest session). Computes newest directly
    // (first_seen guard) — NOT via currentBoxFor, which now reads this store.
    void upsertCharacter(const QString& id, const QString& name, Box* newSession);
};

#endif // BOXREGISTRY_H
