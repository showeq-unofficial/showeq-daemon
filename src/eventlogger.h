#pragma once

#include <QObject>
#include <QString>
#include <QTextStream>
#include <cstddef>
#include <cstdint>
#include <memory>

class QFile;
class EQPacket;
class EQPacketOPCode;

// EventLogger taps EQPacket's decoded-packet signals and writes one line
// per packet to disk. Wired by --list-events FILE on the daemon CLI.
//
// Output format (whitespace-delimited, one packet per line):
//   <unix_ms>  <C|S>  0xXXXX  <bytes>  <name-or-"unknown">
//
// Designed for time-correlation hunts (which C>S opcode fired immediately
// before OP_PlayerProfile arrived with new aa_spent? what arrived during
// the boat handoff window?). External slicing with awk/grep/python.
//
// Timestamps come from QDateTime::currentMSecsSinceEpoch() at the moment
// the slot fires, which is correct for ordering but does NOT reflect the
// .vpk's recorded wall time during --replay (replay runs at "fast as
// possible", so all events cluster within milliseconds). Order is
// preserved, which is what time-correlation hunts actually need.
class EventLogger : public QObject {
    Q_OBJECT
public:
    EventLogger(EQPacket* packet, const QString& outPath,
                QObject* parent = nullptr);
    ~EventLogger() override;

private slots:
    void onDecodedZonePacket(const uint8_t* data, size_t len, uint8_t dir,
                             uint16_t opcode, const EQPacketOPCode* entry);
    void onDecodedWorldPacket(const uint8_t* data, size_t len, uint8_t dir,
                              uint16_t opcode, const EQPacketOPCode* entry);

private:
    void writeRow(uint8_t dir, uint16_t opcode, size_t len,
                  const EQPacketOPCode* entry, const char* stream);

    QString                  m_outPath;
    std::unique_ptr<QFile>   m_file;
    QTextStream              m_out;
};
