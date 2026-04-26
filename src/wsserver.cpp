#include "wsserver.h"

#include <QByteArray>
#include <QLoggingCategory>
#include <QWebSocket>
#include <QWebSocketServer>

#include "envelopesink.h"
#include "sessionadapter.h"

#include "seq/v1/events.pb.h"

namespace {

// Production sink: serialize the envelope and push it across the socket as
// a single binary WebSocket frame. One per connection; lifetime tracked
// alongside the SessionAdapter in WsServer::m_sessions.
class WebSocketSink : public IEnvelopeSink {
public:
    explicit WebSocketSink(QWebSocket* sock) : m_sock(sock) {}

    void send(const seq::v1::Envelope& env) override
    {
        QByteArray buf;
        buf.resize(static_cast<int>(env.ByteSizeLong()));
        env.SerializeToArray(buf.data(), buf.size());
        m_sock->sendBinaryMessage(buf);
    }

private:
    QWebSocket* m_sock;
};

} // namespace

WsServer::WsServer(QObject* parent)
    : QObject(parent)
    , m_server(std::make_unique<QWebSocketServer>(
          QStringLiteral("showeq-daemon"),
          QWebSocketServer::NonSecureMode))
{
    connect(m_server.get(), &QWebSocketServer::newConnection,
            this, &WsServer::onNewConnection);
}

WsServer::~WsServer()
{
    for (auto& s : m_sessions) {
        delete s.sink;
    }
}

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
        auto* sink = new WebSocketSink(sock);
        auto* session = new SessionAdapter(sink, m_spawnShell,
                                           m_zoneMgr, m_player,
                                           m_mapData, m_messageShell,
                                           m_groupMgr, m_spellShell,
                                           m_combatRouter, m_categoryMgr,
                                           m_filterMgr, m_prefsBroker, this);

        // Forward QWebSocket inbound traffic to the adapter's public
        // handlers. Lambdas capture the adapter by value (raw pointer) —
        // safe because both die together in onSessionDisconnected before
        // the QWebSocket itself is deleteLater'd.
        connect(sock, &QWebSocket::textMessageReceived,
                session, [session](const QString& t) {
                    session->handleClientText(t);
                });
        connect(sock, &QWebSocket::binaryMessageReceived,
                session, [session](const QByteArray& b) {
                    session->handleClientBinary(b);
                });
        connect(sock, &QWebSocket::disconnected,
                this, &WsServer::onSessionDisconnected);

        m_sessions.insert(sock, Session{session, sink});
        qInfo("ws client connected (%d total)", m_sessions.size());
    }
}

void WsServer::onSessionDisconnected()
{
    auto* sock = qobject_cast<QWebSocket*>(sender());
    if (!sock) return;
    auto it = m_sessions.find(sock);
    if (it != m_sessions.end()) {
        it->adapter->deleteLater();
        delete it->sink;
        m_sessions.erase(it);
    }
    sock->deleteLater();
    qInfo("ws client disconnected (%d remaining)", m_sessions.size());
}
