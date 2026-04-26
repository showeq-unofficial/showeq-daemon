#pragma once

#include <QFile>
#include <QString>
#include <memory>

#include "envelopesink.h"

namespace seq::v1 { class Envelope; }

// FileSink writes the envelope stream as length-delimited protobuf
// records to a file. Format: for each envelope, a 4-byte little-endian
// uint32 length, followed by `length` bytes of serialized Envelope.
//
// This is the on-disk format the regression harness uses for goldens.
// `--record-golden FILE` plumbs one of these into a daemon-internal
// SessionAdapter.
//
// **Time fields are zeroed before serialization** so two runs against the
// same .vpk produce byte-identical files. Specifically:
//   - Envelope.server_ts_ms is cleared (set in emitEnvelope from
//     QDateTime::currentMSecsSinceEpoch — wall-clock noise)
//   - BuffsUpdate.captured_ms is cleared (also wall-clock)
// A future `--record-envelopes-debug` mode could keep them; today's only
// use of FileSink is regression goldens, so zeroing is the right default.
class FileSink : public IEnvelopeSink {
public:
    explicit FileSink(const QString& path);
    ~FileSink() override;

    // True if the file is open and ready. False after a failed open() —
    // callers should treat this as a fatal startup error.
    bool isOpen() const;

    void send(const seq::v1::Envelope& env) override;

private:
    std::unique_ptr<QFile> m_file;
};
