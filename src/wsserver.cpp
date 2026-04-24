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

bool WsServer::listen(const QHostAddress& host, quint16 port)
{
    return m_server->listen(host, port);
}

void WsServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QWebSocket* sock = m_server->nextPendingConnection();
        auto* session = new SessionAdapter(sock, this);
        connect(sock, &QWebSocket::disconnected,
                this, &WsServer::onSessionDisconnected);
        m_sessions.append(session);
        qInfo("ws client connected (%d total)", m_sessions.size());
    }
}

void WsServer::onSessionDisconnected()
{
    // Identify which session corresponds to the disconnected socket and
    // reap it. sender() is the QWebSocket that emitted `disconnected`.
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
