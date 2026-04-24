#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <cstdint>

#include "seq/v1/events.pb.h"

class QWebSocket;
class Item;
class SpawnShell;
class ZoneMgr;
class Player;

// SessionAdapter is the per-client bridge between in-process QObject signals
// (SpawnShell::addItem, ZoneMgr::zoneChanged, Player::posChanged, ...) and
// the seq.v1 protobuf stream a connected client consumes.
//
// Snapshot/tail race — read before editing:
// When a client subscribes, we MUST connect signals FIRST (with handlers
// buffering to m_buffered, not sending), THEN iterate SpawnShell state into
// a Snapshot, THEN drain m_buffered, THEN flip m_liveTailing. Any other
// order loses or duplicates spawns added during the iteration. See
// startStreaming() for the implementation.
class SessionAdapter : public QObject {
    Q_OBJECT
public:
    SessionAdapter(QWebSocket* sock,
                   SpawnShell* spawnShell,
                   ZoneMgr*    zoneMgr,
                   Player*     player,
                   QObject*    parent = nullptr);
    ~SessionAdapter() override;

    QWebSocket* socket() const { return m_sock; }

private slots:
    void onTextMessage(const QString& text);
    void onBinaryMessage(const QByteArray& bytes);

    // Signal handlers wired to the shared state managers. Before
    // m_liveTailing flips true, these push envelopes into m_buffered
    // instead of sending them.
    void onAddItem(const Item* item);
    void onDelItem(const Item* item);
    void onChangeItem(const Item* item, uint32_t changeType);
    void onKillSpawn(const Item* deceased, const Item* killer,
                     uint16_t killerId);
    void onZoneBegin(const QString& shortName);
    void onZoneChanged(const QString& shortName);

private:
    void startStreaming();
    void sendSnapshot();
    void emitEnvelope(seq::v1::Envelope&& env);
    void sendOrBuffer(seq::v1::Envelope&& env);

    QWebSocket*                  m_sock       = nullptr;
    SpawnShell*                  m_spawnShell = nullptr;
    ZoneMgr*                     m_zoneMgr    = nullptr;
    Player*                      m_player     = nullptr;

    bool                         m_subscribed = false;
    bool                         m_liveTailing = false;
    uint64_t                     m_seq = 0;
    QList<seq::v1::Envelope>     m_buffered;
};
