#pragma once

#include <QHash>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <memory>

class QTimer;
class QWebSocket;
class QWebSocketServer;
class CategoryMgr;
class CombatRouter;
class FilterMgr;
class GroupMgr;
class IEnvelopeSink;
class ItemCache;
class MapData;
class MessageShell;
class Player;
class PrefsBroker;
class SessionAdapter;
class SpawnMonitor;
class SpawnShell;
class SpellShell;
class ZoneMgr;

// WsServer owns a QWebSocketServer on the same event loop as the capture
// pipeline. It also brokers session resume:
//
//   - Each accepted socket starts in "pending" state. The first inbound
//     binary frame must be a Subscribe; WsServer parses it to decide
//     whether to attach to an existing detached SessionAdapter (resume)
//     or spin up a fresh one.
//   - On WebSocket disconnect, the SessionAdapter is NOT destroyed —
//     its sink swaps to a NoopSink and a 30s prune timer starts. The
//     adapter keeps consuming state-manager signals into its ring
//     buffer so a quick reconnect can replaySince(last_seq).
//   - On 30s timer fire (or explicit shutdown), the adapter is
//     destroyed.
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
                  PrefsBroker* pb, SpawnMonitor* sm, ItemCache* ic);

    bool listen(const QHostAddress& host, quint16 port);

private slots:
    void onNewConnection();

private:
    // Per-active-socket state. Until the first Subscribe lands, `adapter`
    // is null and only the socket is tracked.
    struct PendingSocket {
        QWebSocket*    sock;
    };

    // Per-session state, keyed by session_id. Outlives any one
    // QWebSocket so a brief disconnect can be resumed.
    struct Session {
        SessionAdapter* adapter;
        IEnvelopeSink*  liveSink;     // current sink (WebSocketSink while connected, NoopSink while detached)
        QWebSocket*     sock;         // null while detached
        QTimer*         pruneTimer;   // non-null while detached
    };

    void onPendingFirstBinary(QWebSocket* sock, const QByteArray& bytes);
    void onSocketDisconnected(QWebSocket* sock);
    void resumeSession(Session& s, QWebSocket* sock, quint64 lastSeq);
    void attachNewSession(QWebSocket* sock, const QString& sessionId);
    SessionAdapter* makeAdapter(IEnvelopeSink* sink, QObject* parent);
    QString generateSessionId() const;
    void pruneSession(const QString& id);

    std::unique_ptr<QWebSocketServer>   m_server;
    QHash<QWebSocket*, PendingSocket>   m_pending;
    QHash<QString, Session>             m_sessions;
    QHash<QWebSocket*, QString>         m_socketToSession;

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
    SpawnMonitor*  m_spawnMonitor  = nullptr;
    ItemCache*     m_itemCache     = nullptr;
};
