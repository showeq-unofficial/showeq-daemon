#pragma once

#include <QObject>
#include <QHostAddress>
#include <QList>
#include <memory>

class QWebSocketServer;
class QWebSocket;
class SessionAdapter;

// WsServer owns a QWebSocketServer on the same event loop as the capture
// pipeline, so handlers never need to cross thread boundaries to serialize
// in-memory state. Each accepted connection gets its own SessionAdapter.
class WsServer : public QObject {
    Q_OBJECT
public:
    explicit WsServer(QObject* parent = nullptr);
    ~WsServer() override;

    bool listen(const QHostAddress& host, quint16 port);

private slots:
    void onNewConnection();
    void onSessionDisconnected();

private:
    std::unique_ptr<QWebSocketServer>   m_server;
    QList<SessionAdapter*>              m_sessions;
};
