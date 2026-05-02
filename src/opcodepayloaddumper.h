#pragma once

#include <QObject>
#include <QString>
#include <cstddef>
#include <cstdint>

class EQPacket;
class EQPacketOPCode;

// OpcodePayloadDumper writes the raw decoded payload of a chosen opcode
// to disk every time that opcode fires. Wired by --dump-payload OPCODE:PATH
// on the daemon CLI; primarily a recon tool for "where does field X live
// inside opcode Y?" — pair two captures (e.g. L3 vs L60 mage) and
// byte-diff the resulting files to locate the field that changed.
//
// Each matching packet is written to <basePath>.<N>.bin (1-indexed) so
// repeated fires don't clobber each other and `cmp -l` works directly.
class OpcodePayloadDumper : public QObject {
    Q_OBJECT
public:
    OpcodePayloadDumper(EQPacket* packet, uint16_t opcode,
                        const QString& basePath, QObject* parent = nullptr);

private slots:
    void onDecodedZonePacket(const uint8_t* data, size_t len, uint8_t dir,
                             uint16_t opcode, const EQPacketOPCode* entry);

private:
    uint16_t m_opcode;
    QString  m_basePath;
    int      m_count = 0;
};
