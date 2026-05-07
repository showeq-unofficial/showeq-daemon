#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QSocketNotifier>

#include <csignal>
#include <cstdio>
#include <sys/socket.h>
#include <unistd.h>

#include "daemonapp.h"

namespace {

// Replaces Qt's default "msg only" stderr handler with one that prepends an
// ISO-8601 UTC timestamp and a fixed-width level tag. Plain text by design —
// systemd-journald slurps stderr verbatim and `journalctl --output=json`
// already adds structured metadata around each line. JSON-in-message would
// just make grepping painful for a one-user LAN tool.
void messageHandler(QtMsgType type, const QMessageLogContext& ctx,
                    const QString& msg)
{
    const char* lvl;
    switch (type) {
        case QtDebugMsg:    lvl = "DEBUG"; break;
        case QtInfoMsg:     lvl = "INFO "; break;
        case QtWarningMsg:  lvl = "WARN "; break;
        case QtCriticalMsg: lvl = "ERROR"; break;
        case QtFatalMsg:    lvl = "FATAL"; break;
        default:            lvl = "?????"; break;
    }
    const QString ts =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    // Categories other than the default ("default") get a tag so it's
    // obvious which subsystem produced the line. Most of the daemon
    // logs through the default category today.
    const bool hasCat = ctx.category && qstrcmp(ctx.category, "default") != 0;
    if (hasCat) {
        std::fprintf(stderr, "%s [%s] [%s] %s\n",
                     qUtf8Printable(ts), lvl, ctx.category,
                     qUtf8Printable(msg));
    } else {
        std::fprintf(stderr, "%s [%s] %s\n",
                     qUtf8Printable(ts), lvl, qUtf8Printable(msg));
    }
    std::fflush(stderr);

    // Qt's default behavior on QtFatalMsg is to abort; preserve that.
    if (type == QtFatalMsg) {
        std::abort();
    }
}

// SIGINT/SIGTERM/SIGHUP bridged into the Qt event loop via a socketpair.
// Signal handlers write one byte ('Q' for quit, 'H' for handoff+quit);
// a QSocketNotifier on the read end dispatches on the main thread. This
// gives us clean Qt teardown (FileSink flush, OpcodeStatsLogger report,
// WsServer session drop) on Ctrl-C, `systemctl stop`, and SIGHUP reload.
int g_signalFd[2] = {-1, -1};

void sigQuit(int /*sig*/)
{
    char b = 'Q';
    ssize_t r = ::write(g_signalFd[0], &b, 1);
    (void)r;
}

void sigHup(int /*sig*/)
{
    char b = 'H';
    ssize_t r = ::write(g_signalFd[0], &b, 1);
    (void)r;
}

void installSignalBridge()
{
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_signalFd) != 0) {
        qWarning("socketpair failed; SIGINT/SIGTERM will be ungraceful");
        return;
    }
    struct sigaction sa{};
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = &sigQuit;
    ::sigaction(SIGINT, &sa, nullptr);
    ::sigaction(SIGTERM, &sa, nullptr);
    sa.sa_handler = &sigHup;
    ::sigaction(SIGHUP, &sa, nullptr);
    // QSocketNotifier is set up in main() after DaemonApp is constructed
    // so its activated lambda can capture the daemon for handoff export.
}

} // namespace

int main(int argc, char** argv)
{
    qInstallMessageHandler(messageHandler);

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("showeq-daemon");
    QCoreApplication::setApplicationVersion("0.0.1");

    installSignalBridge();

    QCommandLineParser parser;
    parser.setApplicationDescription("Headless ShowEQ packet-capture daemon");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption deviceOpt(QStringList{"d", "device"},
        "Network device to capture on (e.g. eth0). Required for live capture.",
        "device");
    QCommandLineOption listenOpt(QStringList{"l", "listen"},
        "Address:port to serve WebSocket clients on (default 127.0.0.1:9090).",
        "host:port", "127.0.0.1:9090");
    QCommandLineOption replayOpt(QStringList{"r", "replay"},
        "Replay a recorded .vpk file instead of live capture.", "file");
    QCommandLineOption configDirOpt(QStringList{"c", "config-dir"},
        "Directory holding opcode XML and other shared read-only config. "
        "Overrides the compiled-in PKGDATADIR; the writable user dir "
        "stays at ~/.showeq. Convenient for running from the build tree.",
        "dir");
    QCommandLineOption mapsDirOpt(QStringList{"m", "maps-dir"},
        "Directory holding zone .map / .txt files. Defaults to "
        "~/.showeq/maps (shared with showeq), falling back to "
        "$config-dir/maps.", "dir");
    QCommandLineOption recordVpkOpt(QStringList{"record-vpk"},
        "Record raw EQ packets to FILE.vpk for later --replay. Combine "
        "with --device for live capture.", "file");
    QCommandLineOption recordGoldenOpt(QStringList{"record-golden"},
        "Record the envelope stream a client would receive to FILE as "
        "length-delimited seq.v1.Envelope protobuf. With --replay, the "
        "daemon exits at EOF — the regression-harness golden workflow.",
        "file");
    QCommandLineOption opcodeStatsOpt(QStringList{"opcode-stats"},
        "Patch-day diagnostic: tally every decoded opcode (known + "
        "unknown) and write a sorted report with payload-size matches "
        "against known-struct sizes to FILE on shutdown.", "file");
    QCommandLineOption noListenOpt(QStringList{"no-listen"},
        "Skip the WebSocket server entirely. Useful for any session "
        "where no client connects: --record-vpk live captures, "
        "--replay + --record-golden runs, --opcode-stats diagnostics. "
        "Also avoids port-bind conflicts alongside another daemon.");
    QCommandLineOption useRustDecoderOpt(QStringList{"use-rust-decoder"},
        "Route every Rust-implemented opcode handler through the seq-bridge "
        "decoder instead of the C++ struct cast. Off by default; takes effect "
        "only when the binary was built with -DSEQ_USE_RUST=ON.");
    QCommandLineOption dumpPayloadOpt(QStringList{"dump-payload"},
        "Recon: write raw payload bytes of a chosen zone opcode to disk "
        "every time it fires. Format OPCODE:PATH (e.g. 0xdb56:/tmp/profile). "
        "Each match writes PATH.<N>.bin (1-indexed). Repeatable.",
        "spec");
    QCommandLineOption listEventsOpt(QStringList{"list-events"},
        "Recon: write one line per decoded packet to FILE — "
        "<unix_ms> <C|S> 0xXXXX <bytes> <stream> <name>. Use to time-"
        "correlate which opcode fired around an in-game event.",
        "file");

    parser.addOption(deviceOpt);
    parser.addOption(listenOpt);
    parser.addOption(replayOpt);
    parser.addOption(configDirOpt);
    parser.addOption(mapsDirOpt);
    parser.addOption(recordVpkOpt);
    parser.addOption(recordGoldenOpt);
    parser.addOption(opcodeStatsOpt);
    parser.addOption(noListenOpt);
    parser.addOption(useRustDecoderOpt);
    parser.addOption(dumpPayloadOpt);
    parser.addOption(listEventsOpt);
    parser.process(app);

    DaemonApp::Config cfg;
    cfg.device       = parser.value(deviceOpt);
    cfg.replay       = parser.value(replayOpt);
    cfg.configDir    = parser.value(configDirOpt);
    cfg.mapsDir      = parser.value(mapsDirOpt);
    cfg.recordVpk    = parser.value(recordVpkOpt);
    cfg.recordGolden = parser.value(recordGoldenOpt);
    cfg.opcodeStats  = parser.value(opcodeStatsOpt);
    cfg.noListen     = parser.isSet(noListenOpt);
    cfg.useRustDecoder = parser.isSet(useRustDecoderOpt);
    cfg.dumpPayload  = parser.values(dumpPayloadOpt);
    cfg.listEvents   = parser.value(listEventsOpt);
#ifndef SEQ_USE_RUST
    if (cfg.useRustDecoder) {
        qWarning("--use-rust-decoder ignored: this binary was built without "
                 "SEQ_USE_RUST. Reconfigure with -DSEQ_USE_RUST=ON to enable.");
        cfg.useRustDecoder = false;
    }
#endif

    // Resolve record paths against cwd for the same reason --config-dir
    // is — under sudo, $HOME points at /root.
    if (!cfg.recordVpk.isEmpty() && QDir::isRelativePath(cfg.recordVpk)) {
        cfg.recordVpk = QFileInfo(cfg.recordVpk).absoluteFilePath();
    }
    if (!cfg.recordGolden.isEmpty() && QDir::isRelativePath(cfg.recordGolden)) {
        cfg.recordGolden = QFileInfo(cfg.recordGolden).absoluteFilePath();
    }
    if (!cfg.opcodeStats.isEmpty() && QDir::isRelativePath(cfg.opcodeStats)) {
        cfg.opcodeStats = QFileInfo(cfg.opcodeStats).absoluteFilePath();
    }
    if (!cfg.listEvents.isEmpty() && QDir::isRelativePath(cfg.listEvents)) {
        cfg.listEvents = QFileInfo(cfg.listEvents).absoluteFilePath();
    }

    // Resolve --config-dir relative to the invocation cwd, not $HOME.
    // Under sudo, $HOME is /root, which would silently send DataLocationMgr
    // off into the wrong directory. QFileInfo::absoluteFilePath() uses the
    // process cwd, which is what users expect when they type `./conf`.
    if (!cfg.configDir.isEmpty() && QDir::isRelativePath(cfg.configDir)) {
        cfg.configDir = QFileInfo(cfg.configDir).absoluteFilePath();
    }
    if (!cfg.mapsDir.isEmpty() && QDir::isRelativePath(cfg.mapsDir)) {
        cfg.mapsDir = QFileInfo(cfg.mapsDir).absoluteFilePath();
    }

    const QString listen = parser.value(listenOpt);
    const int colon = listen.lastIndexOf(':');
    if (colon <= 0) {
        qCritical("invalid --listen value %s (expected host:port)",
                  qUtf8Printable(listen));
        return 2;
    }
    cfg.listenHost = QHostAddress(listen.left(colon));
    cfg.listenPort = static_cast<quint16>(listen.mid(colon + 1).toUInt());

    DaemonApp daemon(cfg);
    if (!daemon.start()) {
        return 1;
    }

    // Wire signal bytes to actions now that daemon is alive and can be
    // captured by the lambda. 'Q' → graceful quit; 'H' → export session
    // handoff state then quit (new binary reads it on startup).
    auto* notifier = new QSocketNotifier(g_signalFd[1], QSocketNotifier::Read, &app);
    QObject::connect(notifier, &QSocketNotifier::activated, [&daemon, &cfg] {
        char b = 'Q';
        ssize_t r = ::read(g_signalFd[1], &b, 1);
        (void)r;
        if (b == 'H') {
            qInfo("SIGHUP: exporting session handoff state");
            daemon.exportHandoffState(cfg.configDir);
            // Qt teardown crashes when invoked with live pcap state.
            // The handoff file is fully flushed; skip teardown and let
            // the OS reclaim resources. _exit() bypasses C++ dtors and
            // Qt cleanup but not stdio (already fflush'd by our handler).
            ::_exit(75);
        }
        qInfo("shutdown signal received, exiting");
        QCoreApplication::quit();
    });

    if (daemon.importHandoffState(cfg.configDir))
        qInfo("session resumed from handoff state");

    return app.exec();
}
