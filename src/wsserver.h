#pragma once

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <memory>

class QWebSocketServer;
class GroupMgr;
class MapData;
class MessageShell;
class SessionAdapter;
class SpawnShell;
class ZoneMgr;
class Player;

// WsServer owns a QWebSocketServer on the same event loop as the capture
// pipeline, so handlers never need to cross thread boundaries to serialize
// in-memory state. Each accepted connection gets its own SessionAdapter,
// which subscribes to state-manager signals and streams protobuf.
class WsServer : public QObject {
    Q_OBJECT
public:
    explicit WsServer(QObject* parent = nullptr);
    ~WsServer() override;

    // Called by DaemonApp after the state managers are constructed. The
    // pointers must outlive the WsServer; they're not owned.
    void setState(SpawnShell* ss, ZoneMgr* zm, Player* p, MapData* md,
                  MessageShell* ms, GroupMgr* gm);

    bool listen(const QHostAddress& host, quint16 port);

private slots:
    void onNewConnection();
    void onSessionDisconnected();

private:
    std::unique_ptr<QWebSocketServer>   m_server;
    QList<SessionAdapter*>              m_sessions;

    SpawnShell*    m_spawnShell    = nullptr;
    ZoneMgr*       m_zoneMgr       = nullptr;
    Player*        m_player        = nullptr;
    MapData*       m_mapData       = nullptr;
    MessageShell*  m_messageShell  = nullptr;
    GroupMgr*      m_groupMgr      = nullptr;
};
