#include "wsserver.h"

#include <QLoggingCategory>
#include <QWebSocket>
#include <QWebSocketServer>

#include "sessionadapter.h"

WsServer::WsServer(QObject* parent)
    : QObject(parent)
    , m_server(std::make_unique<QWebSocketServer>(
          QStringLiteral("showeq-daemon"),
          QWebSocketServer::NonSecureMode))
{
    connect(m_server.get(), &QWebSocketServer::newConnection,
            this, &WsServer::onNewConnection);
}

WsServer::~WsServer() = default;

void WsServer::setState(SpawnShell* ss, ZoneMgr* zm, Player* p, MapData* md,
                        MessageShell* ms, GroupMgr* gm, SpellShell* sps,
                        CombatRouter* cr, CategoryMgr* cm, FilterMgr* fm,
                        PrefsBroker* pb)
{
    m_spawnShell    = ss;
    m_zoneMgr       = zm;
    m_player        = p;
    m_mapData       = md;
    m_messageShell  = ms;
    m_groupMgr      = gm;
    m_spellShell    = sps;
    m_combatRouter  = cr;
    m_categoryMgr   = cm;
    m_filterMgr     = fm;
    m_prefsBroker   = pb;
}

bool WsServer::listen(const QHostAddress& host, quint16 port)
{
    return m_server->listen(host, port);
}

void WsServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QWebSocket* sock = m_server->nextPendingConnection();
        auto* session = new SessionAdapter(sock, m_spawnShell,
                                           m_zoneMgr, m_player,
                                           m_mapData, m_messageShell,
                                           m_groupMgr, m_spellShell,
                                           m_combatRouter, m_categoryMgr,
                                           m_filterMgr, m_prefsBroker, this);
        connect(sock, &QWebSocket::disconnected,
                this, &WsServer::onSessionDisconnected);
        m_sessions.append(session);
        qInfo("ws client connected (%d total)", m_sessions.size());
    }
}

void WsServer::onSessionDisconnected()
{
    auto* sock = qobject_cast<QWebSocket*>(sender());
    if (!sock) return;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ) {
        if ((*it)->socket() == sock) {
            (*it)->deleteLater();
            it = m_sessions.erase(it);
        } else {
            ++it;
        }
    }
    sock->deleteLater();
    qInfo("ws client disconnected (%d remaining)", m_sessions.size());
}
