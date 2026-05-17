#ifndef NAMEPROMOTER_H
#define NAMEPROMOTER_H

// NamePromoter listens on a Box's world client>server stream for
// OP_EnterWorld (opcode 0x0839, 72 bytes C>S). The payload holds the
// chosen character name at offset 0 (zero-padded in a 64-byte slot).
// On match it stamps the Box's display_name and overwrites the
// placeholder box_id with a stable hash of the name.
//
// Stage 2 of docs/MULTIBOX_PLAN.md. One NamePromoter per non-primary
// Box (the primary box's name is picked up separately by the existing
// Player wiring on the global zone streams; we still install one for
// it so the registry view shows the same labels everywhere).

#include <QObject>

class Box;
class EQPacketOPCode;
class EQPacketStream;

class NamePromoter : public QObject {
    Q_OBJECT
public:
    NamePromoter(Box* box, EQPacketStream* world_c2s,
                 QObject* parent = nullptr);

private slots:
    void onDecodedPacket(const uint8_t* data, size_t len, uint8_t dir,
                         uint16_t opcode, const EQPacketOPCode* entry);

private:
    Box* m_box;
};

#endif // NAMEPROMOTER_H
