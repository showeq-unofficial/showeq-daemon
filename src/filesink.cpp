#include "filesink.h"

#include <QByteArray>
#include <QLoggingCategory>

#include "seq/v1/events.pb.h"

FileSink::FileSink(const QString& path)
    : m_file(std::make_unique<QFile>(path))
{
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCritical("FileSink: cannot open %s for write: %s",
                  qUtf8Printable(path),
                  qUtf8Printable(m_file->errorString()));
        m_file.reset();
    }
}

FileSink::~FileSink()
{
    // QFile dtor flushes + closes; explicit flush here so any error gets
    // surfaced via the warning rather than silently dropped.
    if (m_file && m_file->isOpen()) {
        m_file->flush();
    }
}

bool FileSink::isOpen() const
{
    return m_file && m_file->isOpen();
}

void FileSink::send(const seq::v1::Envelope& env)
{
    if (!isOpen()) return;

    // Copy + scrub wall-clock fields so the serialized bytes are
    // deterministic for the same input .vpk — see filesink.h.
    seq::v1::Envelope scrubbed(env);
    scrubbed.set_server_ts_ms(0);
    if (scrubbed.has_buffs()) {
        scrubbed.mutable_buffs()->set_captured_ms(0);
    }

    const uint32_t len = static_cast<uint32_t>(scrubbed.ByteSizeLong());
    QByteArray buf;
    buf.resize(static_cast<int>(sizeof(uint32_t) + len));

    // Little-endian length prefix. Hardcoded byte ops avoid pulling in
    // QtCore endian helpers + keep the on-disk format obvious.
    auto* p = reinterpret_cast<uint8_t*>(buf.data());
    p[0] = static_cast<uint8_t>(len);
    p[1] = static_cast<uint8_t>(len >> 8);
    p[2] = static_cast<uint8_t>(len >> 16);
    p[3] = static_cast<uint8_t>(len >> 24);
    scrubbed.SerializeToArray(p + 4, len);

    m_file->write(buf);
    // Flush after every envelope so a SIGINT/SIGKILL during recording
    // leaves a complete prefix on disk. Captures are infrequent and the
    // envelope rate is well under syscall capacity, so the cost is fine.
    m_file->flush();
}
