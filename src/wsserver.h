#pragma once

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <memory>

class QWebSocket;
class QWebSocketServer;
class CategoryMgr;
class CombatRouter;
class FilterMgr;
class GroupMgr;
class IEnvelopeSink;
class MapData;
class MessageShell;
class Player;
class PrefsBroker;
class SessionAdapter;
class SpawnShell;
class SpellShell;
class ZoneMgr;

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
                  MessageShell* ms, GroupMgr* gm, SpellShell* sps,
                  CombatRouter* cr, CategoryMgr* cm, FilterMgr* fm,
                  PrefsBroker* pb);

    bool listen(const QHostAddress& host, quint16 port);

private slots:
    void onNewConnection();
    void onSessionDisconnected();

private:
    struct Session {
        SessionAdapter* adapter;
        IEnvelopeSink*  sink;
    };

    std::unique_ptr<QWebSocketServer>   m_server;
    QHash<QWebSocket*, Session>         m_sessions;

    SpawnShell*    m_spawnShell    = nullptr;
    ZoneMgr*       m_zoneMgr       = nullptr;
    Player*        m_player        = nullptr;
    MapData*       m_mapData       = nullptr;
    MessageShell*  m_messageShell  = nullptr;
    GroupMgr*      m_groupMgr      = nullptr;
    SpellShell*    m_spellShell    = nullptr;
    CombatRouter*  m_combatRouter  = nullptr;
    CategoryMgr*   m_categoryMgr   = nullptr;
    FilterMgr*     m_filterMgr     = nullptr;
    PrefsBroker*   m_prefsBroker   = nullptr;
};
