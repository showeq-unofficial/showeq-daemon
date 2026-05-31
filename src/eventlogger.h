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
// Timestamps come from EQPacket::currentPacketTimeMs(): during --replay that
// is the .vpk's recorded epoch time (second resolution), so a regenerated
// timeline matches the original capture's wall clock and is usable for time-
// windowing. In live capture that returns 0 and we fall back to
// QDateTime::currentMSecsSinceEpoch() (real ms). Each row is flushed
// immediately so an unclean process exit can't truncate the tail.
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

    EQPacket*                m_packet;
    QString                  m_outPath;
    std::unique_ptr<QFile>   m_file;
    QTextStream              m_out;
};
