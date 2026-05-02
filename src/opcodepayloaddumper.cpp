#include "opcodepayloaddumper.h"

#include <QFile>
#include <QLoggingCategory>

#include "packet.h"
#include "packetinfo.h"

OpcodePayloadDumper::OpcodePayloadDumper(EQPacket* packet, uint16_t opcode,
                                         const QString& basePath,
                                         QObject* parent)
    : QObject(parent)
    , m_opcode(opcode)
    , m_basePath(basePath)
{
    connect(packet,
            SIGNAL(decodedZonePacket(const uint8_t*, size_t, uint8_t,
                                     uint16_t, const EQPacketOPCode*)),
            this,
            SLOT(onDecodedZonePacket(const uint8_t*, size_t, uint8_t,
                                     uint16_t, const EQPacketOPCode*)));
    qInfo("dump-payload: watching opcode 0x%04x, writing to %s.<N>.bin",
          opcode, qUtf8Printable(basePath));
}

void OpcodePayloadDumper::onDecodedZonePacket(const uint8_t* data, size_t len,
                                              uint8_t dir, uint16_t opcode,
                                              const EQPacketOPCode* entry)
{
    if (opcode != m_opcode) return;
    ++m_count;
    const QString path = QString("%1.%2.bin").arg(m_basePath).arg(m_count);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("dump-payload: cannot open %s: %s",
                 qUtf8Printable(path), qUtf8Printable(f.errorString()));
        return;
    }
    if (data && len > 0) {
        f.write(reinterpret_cast<const char*>(data),
                static_cast<qint64>(len));
    }
    qInfo("dump-payload: wrote %s (%s, dir=%u, %zu bytes)",
          qUtf8Printable(path),
          qUtf8Printable(entry ? entry->name() : QStringLiteral("unknown")),
          dir, len);
}
