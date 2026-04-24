#include "sessionadapter.h"

#include <QByteArray>
#include <QLoggingCategory>
#include <QWebSocket>

#include "seq/v1/client.pb.h"

SessionAdapter::SessionAdapter(QWebSocket* sock, QObject* parent)
    : QObject(parent)
    , m_sock(sock)
{
    connect(sock, &QWebSocket::textMessageReceived,
            this, &SessionAdapter::onTextMessage);
    connect(sock, &QWebSocket::binaryMessageReceived,
            this, &SessionAdapter::onBinaryMessage);
}

SessionAdapter::~SessionAdapter() = default;

void SessionAdapter::onTextMessage(const QString& text)
{
    // Text frames are reserved for debug / control; v1 clients speak binary
    // protobuf. Log and ignore.
    qInfo("ws text frame ignored: %s", qUtf8Printable(text));
}

void SessionAdapter::onBinaryMessage(const QByteArray& bytes)
{
    seq::v1::ClientEnvelope env;
    if (!env.ParseFromArray(bytes.constData(), bytes.size())) {
        qWarning("malformed ClientEnvelope (%d bytes)", bytes.size());
        return;
    }
    if (env.has_subscribe()) {
        // TODO(phase-1): honor Subscribe -- connect SpawnShell/ZoneMgr/Player
        // signals, build Snapshot, drain buffered, flip m_liveTailing.
        qInfo("Subscribe received (topics=%d)", env.subscribe().topics_size());
    }
}

void SessionAdapter::send(const seq::v1::Envelope& env)
{
    QByteArray buf;
    buf.resize(static_cast<int>(env.ByteSizeLong()));
    env.SerializeToArray(buf.data(), buf.size());
    m_sock->sendBinaryMessage(buf);
}
