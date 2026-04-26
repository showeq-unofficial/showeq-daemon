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
class CategoryMgr;
class CombatRouter;
class FileSink;
class FilterMgr;
class OpcodeStatsLogger;
class GroupMgr;
class GuildMgr;
class MapData;
class MessageFilters;
class MessageShell;
class Messages;
class Player;
class PrefsBroker;
class SessionAdapter;
class Spells;
class SpawnMonitor;
class SpawnShell;
class SpellShell;
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
        // If set, raw EQ packets are recorded to this .vpk path while
        // capturing — wraps the legacy XMLPreferences `[VPacket]` mode.
        QString      recordVpk;
        // If set, an internal SessionAdapter writes the envelope stream
        // it would have sent to a client into this file as length-
        // delimited seq.v1.Envelope protobuf. With --replay set, the
        // daemon exits at EOF (golden generation workflow).
        QString      recordGolden;
        // If set, OpcodeStatsLogger taps EQPacket's decoded-packet
        // signals and writes a per-opcode tally to this file at
        // shutdown — patch-day diagnostic for finding ffff opcodes.
        QString      opcodeStats;
        // True to skip the WebSocket server entirely (--no-listen on
        // the CLI). Useful for capture / replay / diagnostic runs
        // where no client connects and the listen port is just a
        // collision risk against the user's main daemon instance.
        bool         noListen = false;
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
    SpawnMonitor*                   m_spawnMonitor  = nullptr;
    SpellShell*                     m_spellShell    = nullptr;
    GroupMgr*                       m_groupMgr      = nullptr;
    CategoryMgr*                    m_categoryMgr   = nullptr;
    MessageFilters*                 m_messageFilters = nullptr;
    Messages*                       m_messages       = nullptr;
    MessageShell*                   m_messageShell   = nullptr;
    CombatRouter*                   m_combatRouter   = nullptr;
    PrefsBroker*                    m_prefsBroker    = nullptr;
    std::unique_ptr<MapData>        m_mapData;

    std::unique_ptr<WsServer>       m_ws;

    // Set when --record-golden is passed. The sink is owned here; the
    // adapter is parented to `this` so Qt cleans it up on shutdown.
    std::unique_ptr<FileSink>       m_goldenSink;
    SessionAdapter*                 m_goldenAdapter  = nullptr;

    // Set when --opcode-stats is passed. Parented to `this` so the
    // dtor's writeReport() runs as part of normal Qt teardown.
    OpcodeStatsLogger*              m_opcodeStats    = nullptr;
};
