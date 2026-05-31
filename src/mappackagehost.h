#pragma once

#include <QString>
#include <cstdint>
#include <vector>

// IMapPackageHost is the narrow back-channel a SessionAdapter uses to query
// and mutate the daemon-global active map package. DaemonApp implements it;
// WsServer borrows the pointer (set via setMapPackageHost) and hands it to
// each SessionAdapter on connect.
//
// Kept abstract for the same reason as IEnvelopeSink: the adapter stays free
// of a DaemonApp include, and the relationship is testable with a fake host.
//
// A "map package" is a named subdirectory under a maps root holding a
// per-zone set of .map/.txt files (e.g. "brewall"). The flat maps root
// itself is the synthetic package id "default".
struct MapPackageInfo {
    QString  id;         // "default" for the flat root, else subdir name
    QString  label;      // human-readable label (== id for now)
    uint32_t zoneCount;  // count of base zone files (excludes _N layers)
};

class IMapPackageHost {
public:
    virtual ~IMapPackageHost() = default;

    // All discovered packages across the daemon's map search roots. Always
    // includes the synthetic "default" entry.
    virtual std::vector<MapPackageInfo> mapPackages() const = 0;

    // Currently-active package id ("default" = flat root).
    virtual QString activeMapPackage() const = 0;

    // Set + persist the active package (falls back to "default" if the id is
    // unknown), re-resolve the current zone's map, then broadcast a fresh
    // MapPackagesUpdate + ZoneChanged to all connected clients. Returns the
    // id that was actually applied.
    virtual QString setMapPackage(const QString& id) = 0;
};
