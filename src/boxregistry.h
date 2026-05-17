#ifndef BOXREGISTRY_H
#define BOXREGISTRY_H

// BoxRegistry observes every EQ client (a "box") visible on the wire.
// Stage 1 of the multibox feature (docs/MULTIBOX_PLAN.md): purely
// observational — the existing single-client decode pipeline still
// runs against the primary (first-seen) box. Per-box stream / state
// splitting lands in Stage 2+.
//
// Identity is two-layer:
//   - wire routing key  (now)        : (client_ip, client_world_port,
//                                       server_world_port)
//   - stable id         (post-handshake): hash of charProfileStruct
//                                         .name once known. Not set in
//                                         Stage 1.

#include <netinet/in.h>

#include <QString>
#include <QVector>
#include <vector>

class Box {
public:
    in_addr_t client_ip          = 0;
    in_port_t client_world_port  = 0;
    in_port_t server_world_port  = 0;

    qint64    first_seen_ms      = 0;
    qint64    last_seen_ms       = 0;
    quint64   packet_count       = 0;

    // Placeholder until OP_PlayerProfile arrives (Stage 3+). Hex string
    // of the creation-time 5-tuple — stable for the box's lifetime.
    QString   box_id;

    // Stage 3+ : set from charProfileStruct.name. Empty in Stage 1.
    QString   display_name;

    // Human-readable summary for --list-boxes output.
    QString   summary() const;
};

class BoxRegistry {
public:
    // Called on every world-port packet seen. Creates a new Box on
    // first sighting of a (client_ip, client_world_port, server_world_port)
    // triple; updates last_seen / packet_count otherwise. Returns a
    // pointer to the (possibly newly created) Box. Pointer is stable
    // for the lifetime of the registry.
    Box* observe(in_addr_t client_ip,
                 in_port_t client_world_port,
                 in_port_t server_world_port,
                 qint64    now_ms);

    // The Box that the legacy single-client pipeline is decoding —
    // first sighting wins. nullptr until first observe(). Reserved for
    // Stage 2; not used in Stage 1.
    Box* primary();

    const std::vector<Box>& boxes() const { return m_boxes; }
    size_t size() const { return m_boxes.size(); }

    // Format the current registry as a multi-line string for stderr
    // logging via --list-boxes.
    QString dumpString() const;

private:
    // Vector + linear scan: box counts are O(1)-O(10) in practice;
    // hash map / RB-tree adds memory churn for no observable win.
    std::vector<Box> m_boxes;
};

#endif // BOXREGISTRY_H
