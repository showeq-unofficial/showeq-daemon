#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QLoggingCategory>

#include "daemonapp.h"

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("showeq-daemon");
    QCoreApplication::setApplicationVersion("0.0.1");

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
        "Directory holding opcode XML and other shared config. Overrides "
        "PKGDATADIR; convenient for running from the build tree.", "dir");
    QCommandLineOption mapsDirOpt(QStringList{"m", "maps-dir"},
        "Directory holding zone .map / .txt files. Defaults to "
        "~/.showeq/maps (the legacy showeq-c location), falling back to "
        "$config-dir/maps.", "dir");
    QCommandLineOption recordVpkOpt(QStringList{"record-vpk"},
        "Record raw EQ packets to FILE.vpk for later --replay. Combine "
        "with --device for live capture.", "file");
    QCommandLineOption recordGoldenOpt(QStringList{"record-golden"},
        "Record the envelope stream a client would receive to FILE as "
        "length-delimited seq.v1.Envelope protobuf. With --replay, the "
        "daemon exits at EOF — the regression-harness golden workflow.",
        "file");

    parser.addOption(deviceOpt);
    parser.addOption(listenOpt);
    parser.addOption(replayOpt);
    parser.addOption(configDirOpt);
    parser.addOption(mapsDirOpt);
    parser.addOption(recordVpkOpt);
    parser.addOption(recordGoldenOpt);
    parser.process(app);

    DaemonApp::Config cfg;
    cfg.device       = parser.value(deviceOpt);
    cfg.replay       = parser.value(replayOpt);
    cfg.configDir    = parser.value(configDirOpt);
    cfg.mapsDir      = parser.value(mapsDirOpt);
    cfg.recordVpk    = parser.value(recordVpkOpt);
    cfg.recordGolden = parser.value(recordGoldenOpt);

    // Resolve record paths against cwd for the same reason --config-dir
    // is — under sudo, $HOME points at /root.
    if (!cfg.recordVpk.isEmpty() && QDir::isRelativePath(cfg.recordVpk)) {
        cfg.recordVpk = QFileInfo(cfg.recordVpk).absoluteFilePath();
    }
    if (!cfg.recordGolden.isEmpty() && QDir::isRelativePath(cfg.recordGolden)) {
        cfg.recordGolden = QFileInfo(cfg.recordGolden).absoluteFilePath();
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
    return app.exec();
}
