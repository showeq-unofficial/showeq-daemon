#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <cstddef>
#include <cstdint>

class EQPacket;
class EQPacketOPCode;

// OpcodeStatsLogger taps EQPacket's decoded-packet signals and keeps a
// per-opcode tally of (count, direction, payload sizes). At destruction
// it writes a sorted report to the configured path. Wired by --opcode-stats
// FILE on the daemon CLI; primarily a patch-day diagnostic for finding
// the still-unresolved (id="ffff") opcodes by matching their payload
// sizes against known-struct sizes from everquest.h.
//
// Output format is plain text — designed to be eyeballed and grepped, not
// parsed. The trailing "Candidate matches" section pairs each unknown
// opcode with any known-struct it could plausibly be by size.
class OpcodeStatsLogger : public QObject {
    Q_OBJECT
public:
    OpcodeStatsLogger(EQPacket* packet, const QString& outPath,
                      QObject* parent = nullptr);
    ~OpcodeStatsLogger() override;

    // Writes the accumulated report. Called automatically from the
    // destructor; exposed so DaemonApp can also flush on aboutToQuit
    // before Qt children get torn down.
    void writeReport();

private slots:
    void onDecodedZonePacket(const uint8_t* data, size_t len, uint8_t dir,
                             uint16_t opcode, const EQPacketOPCode* entry);
    void onDecodedWorldPacket(const uint8_t* data, size_t len, uint8_t dir,
                              uint16_t opcode, const EQPacketOPCode* entry);

private:
    struct OpcodeStat {
        QString          name;        // empty = unknown
        QHash<uint8_t,int> dirCounts; // DIR_Server / DIR_Client
        QHash<int,int>   sizeCounts;  // payload size → count
        int              total = 0;
    };

    void record(QHash<uint16_t, OpcodeStat>& bucket, uint16_t opcode,
                size_t len, uint8_t dir, const EQPacketOPCode* entry);

    QString                       m_outPath;
    QHash<uint16_t, OpcodeStat>   m_zone;
    QHash<uint16_t, OpcodeStat>   m_world;
    bool                          m_written = false;
};
