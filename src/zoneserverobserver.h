#ifndef ZONESERVEROBSERVER_H
#define ZONESERVEROBSERVER_H

// ZoneServerObserver listens on a non-primary Box's world S>C stream
// for OP_ZoneServerInfo (the world server's reply telling the client
// where to connect for the next zone). Parses the server port from
// the zoneServerInfoStruct payload and stamps Box.expected_zone_server_port
// so EQPacket::dispatchPacket can bind subsequent zone-stream traffic
// from this client_ip to this Box.
//
// Stage 3a of docs/MULTIBOX_PLAN.md. Parallel to NamePromoter.

#include <QObject>

#include <cstdint>
#include <functional>

class Box;
class EQPacketOPCode;
class EQPacketStream;

class ZoneServerObserver : public QObject {
    Q_OBJECT
public:
    // nowFn supplies the timestamp for zone_await_ms — EQPacket::nowMs (packet
    // time during --replay so the lookupByExpectedZone tiebreak is reproducible,
    // wall-clock live). Passed in rather than read directly to keep the observer
    // decoupled from EQPacket.
    ZoneServerObserver(Box* box, EQPacketStream* world_s2c,
                       std::function<qint64()> nowFn,
                       QObject* parent = nullptr);

private slots:
    void onDecodedPacket(const uint8_t* data, size_t len, uint8_t dir,
                         uint16_t opcode, const EQPacketOPCode* entry);

private:
    Box* m_box;
    std::function<qint64()> m_nowFn;
};

#endif // ZONESERVEROBSERVER_H
