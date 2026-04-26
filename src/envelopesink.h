#pragma once

// Abstract output for the per-client envelope stream. Lets SessionAdapter
// stay agnostic to the transport so the regression harness can swap in a
// collecting sink without standing up a QWebSocket pair, and so the same
// adapter can later feed a `--record-golden` writer.

namespace seq::v1 { class Envelope; }

class IEnvelopeSink {
public:
    virtual ~IEnvelopeSink() = default;

    // Serialize and dispatch one envelope. SessionAdapter has already
    // populated `seq` and `server_ts_ms` by the time this is called.
    // Implementations are expected to be synchronous: the envelope must
    // be safely consumed (or copied) before send() returns, since
    // SessionAdapter often passes a stack-allocated message.
    virtual void send(const seq::v1::Envelope& env) = 0;
};
