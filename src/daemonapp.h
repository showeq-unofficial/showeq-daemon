#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <memory>

class WsServer;

// DaemonApp is the top-level wiring hub for the daemon. It owns the packet
// capture + decoder + state managers (EQPacket, SpawnShell, ZoneMgr, Player,
// ...) and the WebSocket server that exposes them to clients. It takes the
// place the monolithic `EQInterface` (showeq-c/src/interface.cpp) had as the
// central signal-wiring point -- minus all the Qt widget construction.
class DaemonApp : public QObject {
    Q_OBJECT
public:
    struct Config {
        QString      device;
        QString      replay;
        QHostAddress listenHost;
        quint16      listenPort = 9090;
    };

    explicit DaemonApp(Config cfg, QObject* parent = nullptr);
    ~DaemonApp() override;

    // Brings up the WebSocket server and begins capture. Returns false if
    // either step fails (bad interface, port in use, missing permissions).
    bool start();

private:
    Config m_cfg;
    std::unique_ptr<WsServer> m_ws;
    // TODO(phase-1): EQPacket, SpawnShell, ZoneMgr, Player, GroupMgr,
    // GuildMgr, FilterMgr live here once extraction lands.
};
