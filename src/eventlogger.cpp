#include "eventlogger.h"

#include <QDateTime>
#include <QFile>
#include <QLoggingCategory>

#include "packet.h"
#include "packetinfo.h"

EventLogger::EventLogger(EQPacket* packet, const QString& outPath,
                         QObject* parent)
    : QObject(parent)
    , m_outPath(outPath)
    , m_file(std::make_unique<QFile>(outPath))
{
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("list-events: cannot open %s: %s",
                 qUtf8Printable(outPath),
                 qUtf8Printable(m_file->errorString()));
        m_file.reset();
        return;
    }
    m_out.setDevice(m_file.get());

    connect(packet,
            SIGNAL(decodedZonePacket(const uint8_t*, size_t, uint8_t,
                                     uint16_t, const EQPacketOPCode*)),
            this,
            SLOT(onDecodedZonePacket(const uint8_t*, size_t, uint8_t,
                                     uint16_t, const EQPacketOPCode*)));
    connect(packet,
            SIGNAL(decodedWorldPacket(const uint8_t*, size_t, uint8_t,
                                      uint16_t, const EQPacketOPCode*)),
            this,
            SLOT(onDecodedWorldPacket(const uint8_t*, size_t, uint8_t,
                                      uint16_t, const EQPacketOPCode*)));
    qInfo("list-events: writing per-packet timeline to %s",
          qUtf8Printable(outPath));
}

EventLogger::~EventLogger()
{
    m_out.flush();
}

void EventLogger::onDecodedZonePacket(const uint8_t* /*data*/, size_t len,
                                      uint8_t dir, uint16_t opcode,
                                      const EQPacketOPCode* entry)
{
    writeRow(dir, opcode, len, entry, "zone");
}

void EventLogger::onDecodedWorldPacket(const uint8_t* /*data*/, size_t len,
                                       uint8_t dir, uint16_t opcode,
                                       const EQPacketOPCode* entry)
{
    writeRow(dir, opcode, len, entry, "world");
}

void EventLogger::writeRow(uint8_t dir, uint16_t opcode, size_t len,
                           const EQPacketOPCode* entry, const char* stream)
{
    if (!m_file) return;
    const qint64 ms = QDateTime::currentMSecsSinceEpoch();
    const char dirChar = (dir == DIR_Client) ? 'C'
                       : (dir == DIR_Server) ? 'S'
                       : '?';
    const QString name = entry ? entry->name() : QStringLiteral("unknown");
    m_out << ms << ' ' << dirChar << ' '
          << QString("0x%1").arg(opcode, 4, 16, QLatin1Char('0')) << ' '
          << len << ' ' << stream << ' ' << name << '\n';
}
