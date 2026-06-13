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

class Box;
class EQPacketOPCode;
class EQPacketStream;

class ZoneServerObserver : public QObject {
    Q_OBJECT
public:
    ZoneServerObserver(Box* box, EQPacketStream* world_s2c,
                       QObject* parent = nullptr);

private slots:
    void onDecodedPacket(const uint8_t* data, size_t len, uint8_t dir,
                         uint16_t opcode, const EQPacketOPCode* entry);

private:
    Box* m_box;
};

#endif // ZONESERVEROBSERVER_H
