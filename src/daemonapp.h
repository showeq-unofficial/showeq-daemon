#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <memory>

// Forward declarations of the extracted showeq-c types. We keep them out of
// this header to minimize the include footprint for files that only need the
// DaemonApp shape.
class DataLocationMgr;
class DateTimeMgr;
class EQPacket;
class EQStr;
class FilterMgr;
class GuildMgr;
class MapData;
class Player;
class Spells;
class SpawnShell;
class WsServer;
class ZoneMgr;

// DaemonApp is the top-level wiring hub for the daemon. It owns the packet
// capture + decoder + state managers and the WebSocket server that exposes
// them to clients.
//
// Phase 1 scope: minimum object graph to stream spawn positions to a client.
// That means EQPacket + ZoneMgr + Player + SpawnShell + their dependencies.
// Spell, group, chat, experience, combat, filter-notification subsystems are
// deferred to later phases.
class DaemonApp : public QObject {
    Q_OBJECT
public:
    struct Config {
        QString      device;
        QString      replay;
        // If set, opcode XML and other shared data is read from this
        // directory. Convenient for running against the in-tree conf/
        // without installing to PKGDATADIR.
        QString      configDir;
        // Overrides the zone-map search directory. Empty string triggers
        // the default cascade: ~/.showeq/maps → $configDir/maps.
        QString      mapsDir;
        QHostAddress listenHost;
        quint16      listenPort = 9090;
    };

    explicit DaemonApp(Config cfg, QObject* parent = nullptr);
    ~DaemonApp() override;

    // Brings up the WebSocket server, constructs the object graph, wires
    // opcodes, and starts the capture pipeline. Returns false on
    // unrecoverable setup failure (bad interface, port in use, missing
    // config files).
    bool start();

private slots:
    // Mirrors showeq-c/src/map.cpp:370 — MapMgr::loadZoneMap. Called on every
    // ZoneMgr::zoneChanged so SessionAdapter has fresh geometry to stream.
    void loadZoneMap(const QString& shortZoneName);

private:
    bool startServer();
    bool startCapture();
    void wireZoneMgr();
    void wireSpawnShell();

    Config                          m_cfg;

    std::unique_ptr<DataLocationMgr> m_dataLocationMgr;
    DateTimeMgr*                    m_dateTimeMgr   = nullptr;
    EQPacket*                       m_packet        = nullptr;
    Spells*                         m_spells        = nullptr;
    EQStr*                          m_eqStrings     = nullptr;
    ZoneMgr*                        m_zoneMgr       = nullptr;
    GuildMgr*                       m_guildMgr      = nullptr;
    Player*                         m_player        = nullptr;
    FilterMgr*                      m_filterMgr     = nullptr;
    SpawnShell*                     m_spawnShell    = nullptr;
    std::unique_ptr<MapData>        m_mapData;

    std::unique_ptr<WsServer>       m_ws;
};
