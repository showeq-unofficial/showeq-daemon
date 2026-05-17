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
    // legacy connect2 wiring stays intact. Non-primary boxes get the
    // per-box streams below.
    bool      is_primary         = false;

    // Per-box world streams. Non-null only on non-primary boxes (the
    // BoxCreatedHook installs them). Lazy: zone streams stay global
    // until a later stage tackles same-host zone demux.
    EQPacketStream* world_c2s    = nullptr;
    EQPacketStream* world_s2c    = nullptr;

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

    // Human-readable summary for --list-boxes output.
    QString   summary() const;
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

    // O(N) lookup by world 5-tuple. nullptr if not observed.
    Box* lookupByWorld(in_addr_t client_ip,
                       in_port_t client_world_port,
                       in_port_t server_world_port);

    // Find a Box with this character name (skipping `exclude`). Used
    // by NamePromoter at promotion time to detect re-handshakes of
    // the same character. nullptr if no other Box has the name.
    Box* lookupByName(const QString& name, const Box* exclude = nullptr);

    size_t size() const { return m_boxes.size(); }

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

signals:
    void changed();

private:
    // unique_ptr → stable Box* across vector growth. We hand pointers
    // out to lookup callers AND to per-box stream connect()s.
    std::vector<std::unique_ptr<Box>> m_boxes;
    BoxCreatedHook m_hook;
    QString m_activeBoxId;
};

#endif // BOXREGISTRY_H
