#include "daemonapp.h"

#include <QLoggingCategory>

#include "wsserver.h"

DaemonApp::DaemonApp(Config cfg, QObject* parent)
    : QObject(parent)
    , m_cfg(std::move(cfg))
    , m_ws(std::make_unique<WsServer>(this))
{
}

DaemonApp::~DaemonApp() = default;

bool DaemonApp::start()
{
    if (!m_ws->listen(m_cfg.listenHost, m_cfg.listenPort)) {
        qCritical("failed to listen on %s:%u",
                  qUtf8Printable(m_cfg.listenHost.toString()),
                  m_cfg.listenPort);
        return false;
    }
    qInfo("showeq-daemon listening on %s:%u",
          qUtf8Printable(m_cfg.listenHost.toString()),
          m_cfg.listenPort);

    // TODO(phase-1): construct EQPacket with conf/*opcodes.xml, bind to
    // m_cfg.device or replay m_cfg.replay, wire SpawnShell/ZoneMgr/Player
    // exactly as EQInterface constructor does at showeq-c/src/interface.cpp.
    if (!m_cfg.device.isEmpty()) {
        qInfo("capture device: %s (not yet implemented)",
              qUtf8Printable(m_cfg.device));
    }
    if (!m_cfg.replay.isEmpty()) {
        qInfo("replay file: %s (not yet implemented)",
              qUtf8Printable(m_cfg.replay));
    }
    return true;
}
