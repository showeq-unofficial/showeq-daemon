#pragma once

#include <QList>
#include <QObject>
#include <cstdint>

#include "seq/v1/events.pb.h"

class QWebSocket;

// SessionAdapter is the per-client bridge between in-process QObject signals
// (SpawnShell::addItem, ZoneMgr::zoneChanged, Player::posChanged, ...) and
// the seq.v1 protobuf stream a connected client consumes.
//
// Snapshot/tail race -- read before editing:
// When a client subscribes, we MUST connect signals FIRST (with handlers
// buffering to m_buffered, not sending), THEN iterate SpawnShell state into a
// Snapshot, THEN drain m_buffered, THEN flip m_liveTailing. Any other order
// loses or duplicates spawns added during the iteration.
class SessionAdapter : public QObject {
    Q_OBJECT
public:
    explicit SessionAdapter(QWebSocket* sock, QObject* parent = nullptr);
    ~SessionAdapter() override;

    QWebSocket* socket() const { return m_sock; }

private slots:
    void onTextMessage(const QString& text);
    void onBinaryMessage(const QByteArray& bytes);

private:
    void send(const seq::v1::Envelope& env);

    QWebSocket*                  m_sock = nullptr;
    uint64_t                     m_seq = 0;
    bool                         m_liveTailing = false;
    QList<seq::v1::Envelope>     m_buffered;
};
