#include "daemonapp.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QSet>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTimer>

#include "main.h"

#include "category.h"
#include "combatrouter.h"
#include "datalocationmgr.h"
#include "datetimemgr.h"
#include "eqstr.h"
#include "everquest.h"
#include "filesink.h"
#include "filtermgr.h"
#include "group.h"
#include "guild.h"
#include "itemcache.h"
#include "mapcore.h"
#include "messagefilter.h"
#include "messages.h"
#include "messageshell.h"
#include "opcodestats.h"
#include "opcodepayloaddumper.h"
#include "eventlogger.h"
#include "packet.h"
#include "packetstream.h"
#include "boxregistry.h"
#include "packetcommon.h"
#include "packetinfo.h"
#include "player.h"
#include "prefsbroker.h"
#include "sessionadapter.h"
#include "spawnmonitor.h"
#include "spawnshell.h"
#include "spells.h"
#include "spellshell.h"
#include "wsserver.h"
#include "protoencoder.h"
#include "xmlpreferences.h"
#include "zonemgr.h"
#include "zoneservermgr.h"

#include "seq/v1/client.pb.h"

namespace seq { void initGlobals(const QString& def, const QString& user); }

DaemonApp::DaemonApp(Config cfg, QObject* parent)
    : QObject(parent)
    , m_cfg(std::move(cfg))
    , m_mapData(std::make_unique<MapData>())
    , m_ws(std::make_unique<WsServer>(this))
{
}

DaemonApp::~DaemonApp()
{
    // The golden adapter's m_sink points at m_goldenSink (a unique_ptr
    // member). Members are torn down in reverse declaration order, but
    // m_goldenAdapter is a raw pointer cleaned up by ~QObject much
    // later — that would leave m_sink dangling. Tear down explicitly
    // here so the adapter stops before the sink it writes through.
    if (m_goldenAdapter) {
        delete m_goldenAdapter;
        m_goldenAdapter = nullptr;
    }
}

bool DaemonApp::start()
{
    if (!m_cfg.noListen) {
        if (!startServer()) {
            return false;
        }
    } else {
        qInfo("--no-listen: WebSocket server disabled");
    }

    // DataLocationMgr resolves file paths against ~/.showeq (user) and
    // PKGDATADIR (install prefix). The user dir is shared with showeq so
    // filters/, maps/, spawnpoints/ etc. interop directly; daemon-only
    // writes (prefs, future per-daemon state) go under ~/.showeq/daemon/ to
    // avoid colliding with showeq's root-level showeq.xml. When
    // --config-dir is passed, it substitutes for the read-only PKGDATADIR
    // slot (build-tree conf/ standing in for the install path) — the user
    // dir stays at ~/.showeq so writes still land in a writable location.
    if (!m_cfg.configDir.isEmpty()) {
        m_dataLocationMgr =
            std::make_unique<DataLocationMgr>(".showeq", m_cfg.configDir);
        qInfo("config dir: %s (overrides PKGDATADIR)",
              qUtf8Printable(m_cfg.configDir));
    } else {
        m_dataLocationMgr = std::make_unique<DataLocationMgr>(".showeq");
    }
    m_dataLocationMgr->setupUserDirectory();

    const QFileInfo defPref =
        m_dataLocationMgr->findExistingFile(".", "seqdef.xml", true, false);
    const QFileInfo userPref =
        m_dataLocationMgr->findWriteFile("daemon", "showeq-daemon.xml", true, true);
    seq::initGlobals(defPref.absoluteFilePath(), userPref.absoluteFilePath());

    // Cross-cutting helpers the extracted managers expect to find on the
    // QObject tree. Spells is optional — if spells_us.txt is missing the
    // daemon still runs, we just can't render spell names or compute
    // calc-from-level durations. Search cascade:
    //   1. DataLocationMgr (user dir / --config-dir / pkg dir)
    //   2. /usr/local/share/showeq/ (parallel showeq install — daemon
    //      doesn't ship its own copy of spells_us.txt)
    m_dateTimeMgr = new DateTimeMgr(this, "datetimemgr");
    m_zoneServerMgr = new ZoneServerMgr(this);
    QFileInfo spellsFile =
        m_dataLocationMgr->findExistingFile(".", "spells_us.txt");
    if (!spellsFile.exists()) {
        QFileInfo fi(QStringLiteral("/usr/local/share/showeq/spells_us.txt"));
        if (fi.exists()) spellsFile = fi;
    }
    if (spellsFile.exists()) {
        qInfo("loaded spells from %s", qUtf8Printable(spellsFile.absoluteFilePath()));
    } else {
        qInfo("no spells_us.txt found — spell names + durations will be empty");
    }
    m_spells    = new Spells(spellsFile.exists()
                              ? spellsFile.absoluteFilePath()
                              : QString());
    m_eqStrings = new EQStr();
    // EQ format-string table for OP_FormattedMessage / OP_SimpleMessage
    // payloads. Without it those handlers emit "Unknown: <id>: <args>"
    // because the format-id → template lookup returns nothing. Same
    // search cascade as spells_us.txt above.
    QFileInfo eqstrFile =
        m_dataLocationMgr->findExistingFile(".", "eqstr_us.txt");
    if (!eqstrFile.exists()) {
        QFileInfo fi(QStringLiteral("/usr/local/share/showeq/eqstr_us.txt"));
        if (fi.exists()) eqstrFile = fi;
    }
    if (eqstrFile.exists()) {
        m_eqStrings->load(eqstrFile.absoluteFilePath());
        qInfo("loaded eqstr from %s", qUtf8Printable(eqstrFile.absoluteFilePath()));
    } else {
        qInfo("no eqstr_us.txt found — formatted system messages will read \"Unknown: <id>\"");
    }

    // EQPacket reads `[VPacket] Filename` from pSEQPrefs to decide where
    // to record/playback. Set it before constructing EQPacket so both
    // the recordPackets and playbackPackets paths can find their file.
    // Recording (write) and replay (read) are mutually exclusive at the
    // VPacket layer, so if both flags were passed we reject early.
    if (!m_cfg.recordVpk.isEmpty() && !m_cfg.replay.isEmpty()) {
        qCritical("--record-vpk and --replay are mutually exclusive");
        return false;
    }
    if (!m_cfg.recordVpk.isEmpty()) {
        pSEQPrefs->setPrefString("Filename", "VPacket", m_cfg.recordVpk);
    } else if (!m_cfg.replay.isEmpty()) {
        pSEQPrefs->setPrefString("Filename", "VPacket", m_cfg.replay);
    }

    // CLI --device wins; otherwise consult the XML pref so the value the
    // user saved through the preferences UI persists across restarts.
    // Replay sessions ignore the device entirely.
    if (m_cfg.device.isEmpty() && m_cfg.replay.isEmpty()) {
        const QString xmlDev =
            pSEQPrefs->getPrefString("Device", "Network", QString());
        if (!xmlDev.isEmpty()) {
            m_cfg.device = xmlDev;
            qInfo("device from prefs: %s", qUtf8Printable(xmlDev));
        }
    }

    // EQPacket's ctor calls pcap_create/pcap_activate, which exit(1)s when
    // there's no device available. Skip capture setup entirely when the
    // user passed neither --device nor --replay; the daemon then serves
    // clients with an empty state — useful for smoke tests and local dev.
    if (!m_cfg.device.isEmpty() || !m_cfg.replay.isEmpty()) {
        if (!startCapture()) {
            return false;
        }
    }

    // Daemon-global managers shared into every per-box ManagerSet. These
    // are server-uniform / config / stateless, so all boxes share one
    // instance. Constructed before buildManagerSet() because the per-box
    // managers depend on them.
    const QFileInfo guildFile =
        m_dataLocationMgr->findWriteFile("tmp", "guilds2.dat");
    m_guildMgr = new GuildMgr(guildFile.absoluteFilePath(), this, "guildmgr");
    m_filterMgr = new FilterMgr(
        m_dataLocationMgr.get(),
        /*filterFile*/ "global.xml",
        /*caseSensitive*/ false);
    m_messageFilters = new MessageFilters(this, "messageFilters");
    m_messages = new Messages(m_dateTimeMgr, m_messageFilters,
                              this, "messages");

    // Per-box state managers. Multibox builds one ManagerSet per box so
    // each box decodes into its own game state; today a single active set
    // drives the decode pipeline. The m_* members track the ACTIVE set —
    // they're what loadZoneMap(), wireZoneMgr()/wireSpawnShell(), and the
    // SessionAdapter wiring read.
    m_activeManagers = buildManagerSet();
    const ManagerSet& active = m_activeManagers;
    m_zoneMgr      = active.zoneMgr;
    m_player       = active.player;
    m_spawnShell   = active.spawnShell;
    m_spawnMonitor = active.spawnMonitor;
    m_groupMgr     = active.groupMgr;
    m_messageShell = active.messageShell;
    m_spellShell   = active.spellShell;
    m_combatRouter = active.combatRouter;

    // Per-zone filter overlay for an already-known zone (e.g. replay mode
    // with the zone fixed). Needs the active ZoneMgr, so it runs after
    // buildManagerSet(). The signal it would emit has no listener yet.
    const QString shortZoneName = m_zoneMgr->shortZoneName();
    if (!shortZoneName.isEmpty()) {
        m_filterMgr->loadZone(shortZoneName);
    }

    // CategoryMgr loads user-defined Category groupings from the
    // pSEQPrefs XML preferences (section "CategoryMgr"). seqdef.xml ships
    // with a default set so the list is never empty.
    m_categoryMgr = new CategoryMgr(this, "categoryMgr");

    // Daemon-side itemId -> ItemTemplate cache. Persisted as JSON under
    // ~/.showeq/daemon/itemcache.json so worn-gear stats survive across
    // daemon restarts (we don't see OP_ItemPacket for items the user
    // hasn't moved this session). Wiring of the OP_ItemPacket signal
    // happens in wireZoneMgr() once m_packet is alive.
    //
    // --replay sessions skip persistence entirely so regression goldens
    // aren't contaminated by the user's real cache and replay-captured
    // items don't pollute the on-disk cache.
    m_itemCache = new ItemCache(this);
    if (m_cfg.replay.isEmpty()) {
        const QFileInfo cacheFile = m_dataLocationMgr->findWriteFile(
            "daemon", "itemcache.json", true, true);
        m_itemCache->setStorePath(cacheFile.absoluteFilePath());
    } else {
        qInfo("ItemCache: replay mode, persistence disabled");
    }

    // PrefsBroker is the curated XMLPreferences <-> wire bridge. Constructed
    // after pSEQPrefs is initialized but before any client can connect, so
    // the very first PrefsSnapshot reflects the on-disk state.
    m_prefsBroker = new PrefsBroker(this);
    // The broker triggers EQPacket::monitorDevice / monitorIPClient on
    // Network/* edits so changes apply mid-session (the user has to
    // zone for the new session-key handshake — same as showeq).
    // Null in --no-device + --no-replay smoke-test mode; the broker
    // handles that and just persists to XML.
    m_prefsBroker->setPacket(m_packet);

    // Resolve the active map package: CLI --map-package wins, else the
    // persisted [Maps] Package pref, else "default".
    if (!m_cfg.mapPackage.isEmpty()) {
        m_mapPackage = m_cfg.mapPackage;
    } else {
        m_mapPackage = pSEQPrefs->getPrefString("Package", "Maps",
                                                QStringLiteral("default"));
    }

    // Load the initial zone map if we already know the zone (e.g. replay
    // mode with zone already fixed). Otherwise loadZoneMap fires on the
    // first zone-resolving signal.
    if (!shortZoneName.isEmpty()) {
        loadZoneMap(shortZoneName);
    }
    // Camp+login takes the `zonePlayer -> emit zoneBegin` path; inter-zone
    // transitions take the `zoneChange(DIR_Server) -> emit zoneChanged`
    // path. Listen to both — loadZoneMap is idempotent if the zone hasn't
    // changed because clear()+reload yields the same MapData.
    connect(m_zoneMgr, SIGNAL(zoneBegin(const QString&)),
            this,      SLOT(loadZoneMap(const QString&)));
    connect(m_zoneMgr, SIGNAL(zoneChanged(const QString&)),
            this,      SLOT(loadZoneMap(const QString&)));

    // Same dual-signal wiring for the per-zone filter overlay. Without
    // this, FilterMgr::loadZone only fires once at startup and the
    // overlay file for the new zone is never re-read on transitions.
    connect(m_zoneMgr,   SIGNAL(zoneBegin(const QString&)),
            m_filterMgr, SLOT(loadZone(const QString&)));
    connect(m_zoneMgr,   SIGNAL(zoneChanged(const QString&)),
            m_filterMgr, SLOT(loadZone(const QString&)));

    // Let the WebSocket server hand these to each SessionAdapter it spawns.
    m_ws->setState(m_spawnShell, m_zoneMgr, m_player, m_mapData.get(),
                   m_messageShell, m_groupMgr, m_spellShell,
                   m_combatRouter, m_categoryMgr, m_filterMgr,
                   m_prefsBroker, m_spawnMonitor, m_itemCache,
                   m_dateTimeMgr, m_zoneServerMgr,
                   m_packet ? &m_packet->boxRegistry() : nullptr);
    m_ws->setMapPackageHost(this);
    m_ws->setManagerProvider(this);

    // --record-golden: spin up an internal SessionAdapter writing into a
    // FileSink. Subscribe is synthesized immediately so the on-disk
    // stream begins with a Snapshot, matching what a freshly-connected
    // real client would receive.
    if (!m_cfg.recordGolden.isEmpty()) {
        m_goldenSink = std::make_unique<FileSink>(m_cfg.recordGolden);
        if (!m_goldenSink->isOpen()) {
            return false;
        }
        m_goldenAdapter = new SessionAdapter(m_goldenSink.get(),
                                             m_spawnShell, m_zoneMgr, m_player,
                                             m_mapData.get(), m_messageShell,
                                             m_groupMgr, m_spellShell,
                                             m_combatRouter, m_categoryMgr,
                                             m_filterMgr, m_prefsBroker,
                                             m_spawnMonitor, m_itemCache,
                                             m_dateTimeMgr, m_zoneServerMgr,
                                             m_packet ? &m_packet->boxRegistry()
                                                      : nullptr,
                                             this);
        // The golden adapter writes the regression-harness .pbstream;
        // strip wall-clock fields so the tier-2 byte-cmp is stable
        // across runs.
        m_goldenAdapter->setDeterministic(true);
        m_goldenAdapter->setMapPackageHost(this);
        m_goldenAdapter->setManagerProvider(this);
        seq::v1::ClientEnvelope subEnv;
        subEnv.mutable_subscribe();
        QByteArray subBytes;
        subBytes.resize(static_cast<int>(subEnv.ByteSizeLong()));
        subEnv.SerializeToArray(subBytes.data(), subBytes.size());
        m_goldenAdapter->handleClientBinary(subBytes);
        qInfo("recording envelope golden to %s",
              qUtf8Printable(m_cfg.recordGolden));
    }

    if (m_packet) {
        // Tap decoded packets BEFORE the regular wiring so the logger
        // sees every dispatch (it doesn't matter for correctness — the
        // signal is broadcast — but keeping it adjacent to where the
        // packet pipeline starts makes the order obvious).
        if (!m_cfg.opcodeStats.isEmpty()) {
            m_opcodeStats = new OpcodeStatsLogger(m_packet, m_cfg.opcodeStats, this);
        }

        if (!m_cfg.listEvents.isEmpty()) {
            m_eventLogger = new EventLogger(m_packet, m_cfg.listEvents, this);
        }

        if (m_cfg.listBoxes) {
            // Stage 1 of multibox-sessions: stderr-dump the registry
            // every 5s so the user can verify two-client captures
            // surface both boxes. Final dump on aboutToQuit covers
            // the --replay EOF case (process exits before the next
            // timer tick).
            auto* boxTimer = new QTimer(this);
            connect(boxTimer, &QTimer::timeout, this, [this]() {
                qInfo().noquote() << m_packet->boxRegistry().dumpString();
            });
            boxTimer->start(5000);
            connect(QCoreApplication::instance(),
                    &QCoreApplication::aboutToQuit, this, [this]() {
                qInfo().noquote() << m_packet->boxRegistry().dumpString();
            });
        }

        // Periodically reclaim boxes whose EQ session has gone idle past the
        // TTL (default 10 min; --box-idle-ttl SECONDS, 0 disables). Each zone
        // change opens a fresh world socket, so a long multibox session piles
        // up one Box (with its own ManagerSet + streams) per character per
        // zone; this sweeps the superseded ones. Skipped for --replay: its
        // wall-clock last_seen stays fresh across a short playback and we
        // don't want eviction perturbing deterministic goldens.
        if (m_cfg.replay.isEmpty() && m_cfg.boxIdleTtlMs > 0) {
            // Sweep often enough to act promptly on a short TTL, but no more
            // than once a minute for the common long TTL.
            const qint64 ttl = m_cfg.boxIdleTtlMs;
            const int interval = int(ttl < 5000 ? 5000 : (ttl > 60000 ? 60000
                                                                       : ttl));
            auto* sweep = new QTimer(this);
            connect(sweep, &QTimer::timeout, this, [this]() {
                const int n = m_packet->boxRegistry().evictStale(
                    QDateTime::currentMSecsSinceEpoch(), m_cfg.boxIdleTtlMs);
                if (n > 0)
                    qInfo("BoxRegistry: evicted %d idle box session(s)", n);
            });
            sweep->start(interval);
        }

        for (const QString& spec : m_cfg.dumpPayload) {
            const int colon = spec.indexOf(':');
            if (colon <= 0) {
                qWarning("--dump-payload: malformed %s, expected OPCODE:PATH",
                         qUtf8Printable(spec));
                continue;
            }
            bool ok = false;
            const uint16_t op = static_cast<uint16_t>(
                spec.left(colon).toUInt(&ok, 0));
            if (!ok) {
                qWarning("--dump-payload: bad opcode %s",
                         qUtf8Printable(spec.left(colon)));
                continue;
            }
            m_payloadDumpers.append(
                new OpcodePayloadDumper(m_packet, op, spec.mid(colon + 1), this));
        }

        // Wire the active ManagerSet onto the four global decode streams
        // (the primary box aliases these). This is the single-box decode
        // path; non-primary boxes are wired per-box in onBoxCreated().
        wireBoxPipeline(m_packet->worldClientStream(),
                        m_packet->worldServerStream(),
                        m_packet->zoneClientStream(),
                        m_packet->zoneServerStream(),
                        m_activeManagers, /*wireGlobalSinks=*/true);

        // Build + wire a per-box ManagerSet whenever a new box appears.
        // Fires synchronously from BoxRegistry::observe (after the box's
        // streams are allocated), so a box is fully wired before the
        // packet that created it is routed to its streams.
        connect(&m_packet->boxRegistry(), &BoxRegistry::boxCreated,
                this, [this](Box* box) { onBoxCreated(box); });

        // Reclaim a box's ManagerSet when the registry evicts its idle
        // session. EQPacket tears down the matching streams/observers on
        // the same signal; order between the two slots doesn't matter
        // since each only frees objects it owns (via deleteLater).
        connect(&m_packet->boxRegistry(), &BoxRegistry::boxAboutToBeRemoved,
                this, [this](Box* box) { onBoxAboutToBeRemoved(box); });

        // On an active-box switch the newly-active box is already sitting in
        // its zone, so no zoneChanged fires and the shared MapData still holds
        // the PREVIOUS box's geometry — the re-snapshot would re-ship the old
        // map. Reload MapData for the new box's current zone here, resolving
        // the same managers SessionAdapter will. Connected at startup, this
        // runs before any per-client SessionAdapter::onActiveBoxChanged (those
        // attach when a ws client connects, strictly later), so sendSnapshot
        // reads fresh geometry.
        connect(&m_packet->boxRegistry(), &BoxRegistry::activeBoxChanged,
                this, [this](Box* old, Box* target) {
            // Only a genuine switch needs this. The first box becoming active
            // (old == nullptr, e.g. adopt-first-character) loads its map via
            // the normal zoneChanged path; reloading here would just re-clear
            // MapData mid-replay and flip goldens.
            if (!old || !target) return;
            const ManagerSet* ns = managersForBox(target->box_id);
            if (ns && ns->zoneMgr)
                loadZoneMap(ns->zoneMgr->shortZoneName());
        });

        // Replay normally quits at EOF (golden generation /
        // opcode-stats / --no-listen one-shots all want this). With
        // --wait-for-client, however, we're driving the web UI from a
        // recorded capture — playback must hold until a real
        // SessionAdapter is wired (otherwise the early-replay
        // envelopes hit a deferred adapter and get dropped) and the
        // daemon must stay running after EOF so the user can poke
        // at the final state.
        const bool isReplay = !m_cfg.replay.isEmpty();
        if (isReplay && !m_cfg.waitForClient) {
            connect(m_packet, &EQPacket::playbackFinished, this, [] {
                QTimer::singleShot(0, &QCoreApplication::quit);
            });
        }

        if (isReplay && m_cfg.waitForClient) {
            // Defer start until the first WsServer client subscribes.
            connect(m_ws.get(), &WsServer::firstClientSubscribed,
                    this, [this] {
                qInfo("--wait-for-client: client connected, starting replay");
                m_packet->start(10);
            });
            qInfo("--wait-for-client: replay paused, waiting for ws client");
        } else {
            m_packet->start(10);
            qInfo("capture pipeline running");
        }
    } else {
        qInfo("no --device or --replay — capture pipeline idle");
    }
    return true;
}

bool DaemonApp::startServer()
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
    return true;
}

bool DaemonApp::startCapture()
{
    const QFileInfo worldOpcodes =
        m_dataLocationMgr->findExistingFile(".", "worldopcodes.xml");
    const QFileInfo zoneOpcodes =
        m_dataLocationMgr->findExistingFile(".", "zoneopcodes.xml");
    if (!worldOpcodes.exists() || !zoneOpcodes.exists()) {
        qCritical("missing opcode XML (worldopcodes.xml / zoneopcodes.xml) "
                  "— check that conf/ is installed to PKGDATADIR");
        return false;
    }

    const bool hasReplay = !m_cfg.replay.isEmpty();
    const bool wantRecord = !m_cfg.recordVpk.isEmpty();
    // CLI --ip wins, then XML pref, then sentinel. Empty / sentinel
    // string == auto-detect next session, same semantics as showeq.
    QString clientIp = m_cfg.ip;
    if (clientIp.isEmpty()) {
        clientIp = pSEQPrefs->getPrefString("IP", "Network", AUTOMATIC_CLIENT_IP);
    }
    if (clientIp.isEmpty()) clientIp = AUTOMATIC_CLIENT_IP;
    m_packet = new EQPacket(
        worldOpcodes.absoluteFilePath(),
        zoneOpcodes.absoluteFilePath(),
        /*arqSeqGiveUp*/ 512,
        /*device*/ hasReplay ? QString() : m_cfg.device,
        /*ip*/ clientIp,
        /*mac*/ QStringLiteral("0"),
        /*realtime*/ false,
        /*snaplen*/ 2,
        /*buffersize*/ 4,
        /*sessionTracking*/ false,
        /*recordPackets*/ wantRecord,
        /*playbackPackets*/ hasReplay ? 1 : PLAYBACK_OFF,
        /*playbackSpeed*/ 0,
        this, "packet");
    if (wantRecord) {
        qInfo("recording raw packets to %s", qUtf8Printable(m_cfg.recordVpk));
    }
    return true;
}

ManagerSet DaemonApp::buildManagerSet()
{
    // Constructs one set of per-box state managers in the SAME order (and
    // with the same cross-manager connect()s) the daemon has always used,
    // so single-box decode output stays byte-identical. The daemon-global
    // managers (m_guildMgr, m_filterMgr, m_messages, m_messageFilters,
    // m_spells, m_eqStrings, m_dateTimeMgr, m_dataLocationMgr) must already
    // exist — they're shared into every set.
    ManagerSet ms;

    ms.zoneMgr = new ZoneMgr(this, "zonemgr");
    ms.player  = new Player(this, ms.zoneMgr, m_guildMgr);
    ms.spawnShell =
        new SpawnShell(*m_filterMgr, ms.zoneMgr, ms.player, m_guildMgr);

    // SpawnMonitor learns recurring NPC pop locations + respawn timers
    // from observed spawn/kill cycles (showeq interface.cpp:326). It
    // connects its own slots to the SpawnShell + ZoneMgr in its ctor.
    ms.spawnMonitor = new SpawnMonitor(m_dataLocationMgr.get(),
                                       ms.zoneMgr, ms.spawnShell,
                                       this, "spawnMonitor");
    // Persist on shutdown. saveSpawnPoints is a no-op unless modified, so
    // this is cheap; aboutToQuit fires from the event loop after quit().
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            ms.spawnMonitor, &SpawnMonitor::saveSpawnPoints);

    // GroupMgr tracks group members. Wiring matches showeq/src/
    // interface.cpp:593-615 — player profile signal, group opcode
    // handlers, and the spawn lifecycle slots.
    ms.groupMgr = new GroupMgr(ms.spawnShell, ms.player, this, "groupMgr");
    connect(ms.zoneMgr,    SIGNAL(playerProfile(const charProfileStruct*)),
            ms.groupMgr,   SLOT(player(const charProfileStruct*)));
    connect(ms.spawnShell, SIGNAL(addItem(const Item*)),
            ms.groupMgr,   SLOT(addItem(const Item*)));
    connect(ms.spawnShell, SIGNAL(delItem(const Item*)),
            ms.groupMgr,   SLOT(delItem(const Item*)));
    connect(ms.spawnShell, SIGNAL(killSpawn(const Item*, const Item*, uint16_t)),
            ms.groupMgr,   SLOT(killSpawn(const Item*)));
    // SpawnShell::clear() (zone change) bulk-frees spawns and emits only
    // clearItems() — without this the GroupMgr m_spawn pointers dangle past
    // the zone and fillGroupUpdate dereferences freed Spawns (UAF crash).
    connect(ms.spawnShell, SIGNAL(clearItems()),
            ms.groupMgr,   SLOT(clear()));

    // MessageShell parses chat / system / NPC text into structured
    // signals. Needs the global MessageFilters + Messages.
    ms.messageShell = new MessageShell(m_messages, m_eqStrings, m_spells,
                                       ms.zoneMgr, ms.spawnShell, ms.player,
                                       this, "messageShell");

    // SpellShell tracks active buffs / outgoing casts. Wires player
    // signals + clear-on-zone, mirroring showeq interface.cpp:967-988.
    ms.spellShell = new SpellShell(ms.player, ms.spawnShell, m_spells);
    ms.spellShell->setParent(this);
    connect(ms.player, SIGNAL(newPlayer()),
            ms.spellShell, SLOT(clear()));
    connect(ms.player, SIGNAL(buffLoad(const spellBuff*)),
            ms.spellShell, SLOT(buffLoad(const spellBuff*)));
    connect(ms.zoneMgr, SIGNAL(zoneChanged(const QString&)),
            ms.spellShell, SLOT(zoneChanged()));
    connect(ms.spawnShell, SIGNAL(killSpawn(const Item*, const Item*, uint16_t)),
            ms.spellShell, SLOT(killSpawn(const Item*)));

    // CombatRouter parses OP_Action2 into structured combat events.
    ms.combatRouter = new CombatRouter(ms.spawnShell, m_spells, this);

    return ms;
}

void DaemonApp::wireBoxPipeline(EQPacketStream* worldC2S, EQPacketStream* worldS2C,
                                EQPacketStream* zoneC2S, EQPacketStream* zoneS2C,
                                const ManagerSet& ms, bool wireGlobalSinks)
{
    // Wire one opcode→handler onto the stream(s) selected by (streamPair,
    // dir). Mirrors the legacy EQPacket::connect2/wireBox fan-out, so the
    // per-stream dispatcher install order is identical. The call sequence
    // below is the exact concatenation of the old wireZoneMgr() then
    // wireSpawnShell(), and is golden-sensitive (shared opcode+payload on
    // one stream dispatches in install order) — do not reorder.
    auto wire = [&](const QString& op, EQStreamPairs sp, uint8_t dir,
                    const char* payload, EQSizeCheckType szt,
                    const QObject* recv, const char* slot) {
        if (sp & SP_World) {
            if ((dir & DIR_Client) && worldC2S)
                worldC2S->connect2(op, payload, szt, recv, slot);
            if ((dir & DIR_Server) && worldS2C)
                worldS2C->connect2(op, payload, szt, recv, slot);
        }
        if (sp & SP_Zone) {
            if ((dir & DIR_Client) && zoneC2S)
                zoneC2S->connect2(op, payload, szt, recv, slot);
            if ((dir & DIR_Server) && zoneS2C)
                zoneS2C->connect2(op, payload, szt, recv, slot);
        }
    };

    // --- ZoneMgr: zone transitions + player profile.
    wire("OP_ZoneEntry", SP_Zone, DIR_Client,
         "ClientZoneEntryStruct", SZC_Match,
         ms.zoneMgr, SLOT(zoneEntryClient(const uint8_t*, size_t, uint8_t)));
    wire("OP_PlayerProfile", SP_Zone, DIR_Server,
         "charProfileStruct", SZC_Match,
         ms.zoneMgr, SLOT(zonePlayer(const uint8_t*, size_t)));
    wire("OP_ZoneChange", SP_Zone, DIR_Client | DIR_Server,
         "zoneChangeStruct", SZC_Match,
         ms.zoneMgr, SLOT(zoneChange(const uint8_t*, size_t, uint8_t)));
    wire("OP_NewZone", SP_Zone, DIR_Server,
         "newZoneStruct", SZC_Match,
         ms.zoneMgr, SLOT(zoneNew(const uint8_t*, size_t, uint8_t)));
    wire("OP_SendZonePoints", SP_Zone, DIR_Server,
         "zonePointsStruct", SZC_None,
         ms.zoneMgr, SLOT(zonePoints(const uint8_t*, size_t, uint8_t)));
    wire("OP_DzSwitchInfo", SP_Zone, DIR_Server,
         "dzSwitchInfo", SZC_None,
         ms.zoneMgr, SLOT(dynamicZonePoints(const uint8_t*, size_t, uint8_t)));
    wire("OP_DzInfo", SP_Zone, DIR_Server,
         "dzInfo", SZC_None,
         ms.zoneMgr, SLOT(dynamicZoneInfo(const uint8_t*, size_t, uint8_t)));

    // Cross-manager: profile feeds Player too (after GroupMgr, which is
    // connected in buildManagerSet() — preserves slot fire order).
    connect(ms.zoneMgr, SIGNAL(playerProfile(const charProfileStruct*)),
            ms.player,  SLOT(player(const charProfileStruct*)));

    // OP_ClientUpdate DIR_Client = this player's movement (playerSelfPos);
    // the DIR_Server playerSpawnPos variant goes to SpawnShell below.
    wire("OP_ClientUpdate", SP_Zone, DIR_Server | DIR_Client,
         "playerSelfPosStruct", SZC_Match,
         ms.player, SLOT(playerUpdateSelf(const uint8_t*, size_t, uint8_t)));

    // OP_ItemPacket / OP_TimeOfDay / OP_ZoneServerInfo feed daemon-GLOBAL
    // sinks. Only the active box wires them — otherwise every box's
    // (now unmuted) world/zone stream would re-fire them, duplicating
    // ZoneServer / EqTimeSync / ItemLearned envelopes to the client.
    if (wireGlobalSinks) {
        wire("OP_ItemPacket", SP_Zone, DIR_Server,
             "uint8_t", SZC_None,
             m_itemCache, SLOT(onItemPacket(const uint8_t*, size_t, uint8_t)));
        wire("OP_TimeOfDay", SP_Zone, DIR_Server,
             "timeOfDayStruct", SZC_Match,
             m_dateTimeMgr, SLOT(timeOfDay(const uint8_t*)));
        wire("OP_ZoneServerInfo", SP_World, DIR_Server,
             "zoneServerInfoStruct", SZC_Match,
             m_zoneServerMgr, SLOT(zoneServerInfo(const uint8_t*)));
    }

    // --- SpawnShell: spawn lifecycle + positions.
    wire("OP_GroundSpawn", SP_Zone, DIR_Server,
         "makeDropStruct", SZC_Modulus,
         ms.spawnShell, SLOT(newGroundItem(const uint8_t*, size_t, uint8_t)));
    wire("OP_ClickObject", SP_Zone, DIR_Server,
         "remDropStruct", SZC_Match,
         ms.spawnShell, SLOT(removeGroundItem(const uint8_t*, size_t, uint8_t)));
    wire("OP_SpawnDoor", SP_Zone, DIR_Server,
         "doorStruct", SZC_Modulus,
         ms.spawnShell, SLOT(newDoorSpawns(const uint8_t*, size_t, uint8_t)));
    wire("OP_ZoneEntry", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         ms.spawnShell, SLOT(zoneEntry(const uint8_t*, size_t)));
    wire("OP_MobUpdate", SP_Zone, DIR_Server | DIR_Client,
         "spawnPositionUpdate", SZC_Match,
         ms.spawnShell, SLOT(updateSpawns(const uint8_t*)));
    wire("OP_WearChange", SP_Zone, DIR_Server | DIR_Client,
         "SpawnUpdateStruct", SZC_Match,
         ms.spawnShell, SLOT(updateSpawnInfo(const uint8_t*)));
    // OP_SpawnAppearance2 type=0x2c = TLP mob-lock / FTE flag.
    wire("OP_SpawnAppearance2", SP_Zone, DIR_Server,
         "spawnAppearance2Struct", SZC_Match,
         ms.spawnShell, SLOT(updateSpawnLock(const uint8_t*)));
    wire("OP_HPUpdate", SP_Zone, DIR_Server | DIR_Client,
         "hpNpcUpdateStruct", SZC_Match,
         ms.spawnShell, SLOT(updateNpcHP(const uint8_t*)));
    wire("OP_MobHealth", SP_Zone, DIR_Server,
         "mobHealthStruct", SZC_Match,
         ms.spawnShell, SLOT(updateMobHealth(const uint8_t*)));

    // Player vitals — same opcodes also feed Player (filtered by self).
    // SpawnShell's OP_HPUpdate/OP_WearChange wires above MUST precede
    // these (shared opcode+payload on the same stream dispatches in order).
    wire("OP_HPUpdate", SP_Zone, DIR_Server | DIR_Client,
         "hpNpcUpdateStruct", SZC_Match,
         ms.player, SLOT(updateNpcHP(const uint8_t*)));
    wire("OP_ManaChange", SP_Zone, DIR_Server,
         "manaDecrementStruct", SZC_Match,
         ms.player, SLOT(manaChange(const uint8_t*)));
    wire("OP_Stamina", SP_Zone, DIR_Server,
         "staminaStruct", SZC_Match,
         ms.player, SLOT(updateStamina(const uint8_t*)));
    wire("OP_EndUpdate", SP_Zone, DIR_Server,
         "endUpdateStruct", SZC_Match,
         ms.player, SLOT(updateEndurance(const uint8_t*)));
    wire("OP_ExpUpdate", SP_Zone, DIR_Server,
         "expUpdateStruct", SZC_Match,
         ms.player, SLOT(updateExp(const uint8_t*)));
    wire("OP_LevelUpdate", SP_Zone, DIR_Server,
         "levelUpUpdateStruct", SZC_Match,
         ms.player, SLOT(updateLevel(const uint8_t*)));
    wire("OP_SkillUpdate", SP_Zone, DIR_Server,
         "skillIncStruct", SZC_Match,
         ms.player, SLOT(increaseSkill(const uint8_t*)));
    wire("OP_WearChange", SP_Zone, DIR_Server | DIR_Client,
         "SpawnUpdateStruct", SZC_Match,
         ms.player, SLOT(updateSpawnInfo(const uint8_t*)));
    wire("OP_DeleteSpawn", SP_Zone, DIR_Server | DIR_Client,
         "deleteSpawnStruct", SZC_Match,
         ms.spawnShell, SLOT(deleteSpawn(const uint8_t*)));
    wire("OP_SpawnRename", SP_Zone, DIR_Server,
         "spawnRenameStruct", SZC_Match,
         ms.spawnShell, SLOT(renameSpawn(const uint8_t*)));
    wire("OP_Illusion", SP_Zone, DIR_Server | DIR_Client,
         "spawnIllusionStruct", SZC_Match,
         ms.spawnShell, SLOT(illusionSpawn(const uint8_t*)));
    wire("OP_SpawnAppearance", SP_Zone, DIR_Server | DIR_Client,
         "spawnAppearanceStruct", SZC_Match,
         ms.spawnShell, SLOT(updateSpawnAppearance(const uint8_t*)));
    wire("OP_Death", SP_Zone, DIR_Server,
         "newCorpseStruct", SZC_Match,
         ms.spawnShell, SLOT(killSpawn(const uint8_t*)));
    wire("OP_Shroud", SP_Zone, DIR_Server,
         "spawnShroudSelf", SZC_None,
         ms.spawnShell, SLOT(shroudSpawn(const uint8_t*, size_t, uint8_t)));
    wire("OP_RemoveSpawn", SP_Zone, DIR_Server | DIR_Client,
         "removeSpawnStruct", SZC_None,
         ms.spawnShell, SLOT(removeSpawn(const uint8_t*, size_t, uint8_t)));
    wire("OP_Consider", SP_Zone, DIR_Server | DIR_Client,
         "considerStruct", SZC_Match,
         ms.spawnShell, SLOT(consMessage(const uint8_t*, size_t, uint8_t)));
    wire("OP_TargetMouse", SP_Zone, DIR_Server | DIR_Client,
         "clientTargetStruct", SZC_Match,
         ms.spawnShell, SLOT(clientTarget(const uint8_t*)));
    wire("OP_NpcMoveUpdate", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         ms.spawnShell, SLOT(npcMoveUpdate(const uint8_t*, size_t, uint8_t)));
    wire("OP_ClientUpdate", SP_Zone, DIR_Server,
         "playerSpawnPosStruct", SZC_Match,
         ms.spawnShell, SLOT(playerUpdate(const uint8_t*, size_t, uint8_t)));
    wire("OP_CorpseLocResponse", SP_Zone, DIR_Server,
         "corpseLocStruct", SZC_Match,
         ms.spawnShell, SLOT(corpseLoc(const uint8_t*)));

    // --- MessageShell: chat / system / NPC text.
    wire("OP_CommonMessage", SP_Zone, DIR_Client | DIR_Server,
         "channelMessageStruct", SZC_None,
         ms.messageShell, SLOT(channelMessage(const uint8_t*, size_t, uint8_t)));
    wire("OP_FormattedMessage", SP_Zone, DIR_Server,
         "formattedMessageStruct", SZC_None,
         ms.messageShell, SLOT(formattedMessage(const uint8_t*, size_t, uint8_t)));
    wire("OP_SimpleMessage", SP_Zone, DIR_Server,
         "simpleMessageStruct", SZC_Match,
         ms.messageShell, SLOT(simpleMessage(const uint8_t*, size_t, uint8_t)));
    wire("OP_SpecialMesg", SP_Zone, DIR_Server,
         "specialMessageStruct", SZC_None,
         ms.messageShell, SLOT(specialMessage(const uint8_t*, size_t, uint8_t)));

    // --- GroupMgr.
    wire("OP_GroupUpdate", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         ms.groupMgr, SLOT(groupUpdate(const uint8_t*, size_t)));
    wire("OP_GroupMemberList", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         ms.groupMgr, SLOT(groupMemberList(const uint8_t*, size_t)));
    wire("OP_GroupDisband", SP_Zone, DIR_Server,
         "groupDisbandStruct", SZC_None,
         ms.groupMgr, SLOT(removeGroupMember(const uint8_t*)));
    wire("OP_GroupDisband2", SP_Zone, DIR_Server,
         "groupDisbandStruct", SZC_None,
         ms.groupMgr, SLOT(removeGroupMember(const uint8_t*)));

    // --- SpellShell. (OP_SimpleMessage here is a SECOND receiver after
    // MessageShell above — order preserved.)
    wire("OP_CastSpell", SP_Zone, DIR_Server | DIR_Client,
         "startCastStruct", SZC_Match,
         ms.spellShell, SLOT(selfStartSpellCast(const uint8_t*)));
    wire("OP_Buff", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         ms.spellShell, SLOT(buff(const uint8_t*, size_t, uint8_t)));
    wire("OP_Action", SP_Zone, DIR_Server | DIR_Client,
         "actionStruct", SZC_Match,
         ms.spellShell, SLOT(action(const uint8_t*, size_t, uint8_t)));
    wire("OP_Action", SP_Zone, DIR_Server | DIR_Client,
         "actionAltStruct", SZC_Match,
         ms.spellShell, SLOT(action(const uint8_t*, size_t, uint8_t)));
    wire("OP_SimpleMessage", SP_Zone, DIR_Server,
         "simpleMessageStruct", SZC_Match,
         ms.spellShell, SLOT(simpleMessage(const uint8_t*, size_t, uint8_t)));

    // --- CombatRouter.
    wire("OP_Action2", SP_Zone, DIR_Client | DIR_Server,
         "action2Struct", SZC_Match,
         ms.combatRouter, SLOT(action2(const uint8_t*, size_t, uint8_t)));
}

void DaemonApp::onBoxCreated(Box* box)
{
    if (!box) return;
    if (box->is_primary) {
        // The primary box's four streams ARE the global streams, already
        // wired to the active ManagerSet in start(). Just record the
        // mapping so SessionAdapter can resolve it.
        m_boxManagers.insert(box, m_activeManagers);
    } else {
        // Every non-primary box decodes continuously into its OWN ManagerSet
        // (no mute gate), wired onto its own streams — so switching the
        // active box is a rebind, not a clear+resnapshot.
        const ManagerSet ms = buildManagerSet();
        m_boxManagers.insert(box, ms);
        // Reparent the set's managers under one per-box root so eviction
        // (BoxRegistry::evictStale → onBoxAboutToBeRemoved) reclaims them
        // with a single deleteLater. buildManagerSet() parents them to
        // `this`; SpawnShell takes no parent, so adopt it here too.
        auto* root = new QObject(this);
        m_boxManagerRoots.insert(box, root);
        for (QObject* m : {static_cast<QObject*>(ms.zoneMgr),
                           static_cast<QObject*>(ms.player),
                           static_cast<QObject*>(ms.spawnShell),
                           static_cast<QObject*>(ms.spellShell),
                           static_cast<QObject*>(ms.groupMgr),
                           static_cast<QObject*>(ms.messageShell),
                           static_cast<QObject*>(ms.combatRouter),
                           static_cast<QObject*>(ms.spawnMonitor)}) {
            if (m) m->setParent(root);
        }
        wireBoxPipeline(box->world_c2s, box->world_s2c,
                        box->zone_c2s, box->zone_s2c, ms,
                        /*wireGlobalSinks=*/false);
    }

    // Promote + merge the box by its CHARACTER name on every OP_PlayerProfile
    // (i.e. each zone-in). Read the AUTHORITATIVE name straight off
    // charProfileStruct.name — Player::name() returns the "You" default at
    // this point (its auto-detect flags haven't settled), which would
    // collapse every box into one bogus character. Re-handshakes of the same
    // character merge into one picker entry; promoteByName rolls the
    // character's current decode box to this newest zone session.
    if (ZoneMgr* zm = m_boxManagers[box].zoneMgr) {
        connect(zm, &ZoneMgr::playerProfile, this,
                [this, box](const charProfileStruct* p) {
            if (!p) return;
            const QString name =
                QString::fromLatin1(p->name,
                                    int(qstrnlen(p->name, sizeof(p->name))));
            m_packet->boxRegistry().promoteByName(box, name);
        });
    }

    // Keep the shared MapData in sync when the ACTIVE non-primary box zones.
    // The primary's zoneMgr is wired straight to loadZoneMap in start(); a
    // non-primary box that's promoted at login shows its name before its
    // OP_NewZone decodes, so a switch to it loads an empty map and the later
    // zoneChanged (which makes SessionAdapter re-send geometry) would still
    // read a stale MapData. Reload here so that late zone refreshes the map
    // with no second manual swap. Guarded on active so a background box zoning
    // doesn't clobber the active box's map.
    if (!box->is_primary) {
        if (ZoneMgr* zm = m_boxManagers[box].zoneMgr) {
            connect(zm, qOverload<const QString&>(&ZoneMgr::zoneChanged), this,
                    [this, box](const QString& zone) {
                BoxRegistry& reg = m_packet->boxRegistry();
                if (reg.currentBoxFor(reg.activeBoxId()) == box)
                    loadZoneMap(zone);
            });
        }
    }
}

void DaemonApp::onBoxAboutToBeRemoved(Box* box)
{
    if (!box || box->is_primary) return;   // primary reuses m_activeManagers
    // Drop the resolver record first so any sendBoxList re-emit triggered
    // by this eviction can't hand SessionAdapter a set that's being torn
    // down, then deleteLater the whole per-box manager subtree.
    m_boxManagers.remove(box);
    if (QObject* root = m_boxManagerRoots.take(box))
        root->deleteLater();
}

const ManagerSet* DaemonApp::managersForBox(const QString& boxId) const
{
    if (!m_packet) {
        return m_activeManagers.spawnShell ? &m_activeManagers : nullptr;
    }
    BoxRegistry& reg = m_packet->boxRegistry();
    // Resolve to the character's CURRENT (latest) decode box, so switching
    // to a character shows the zone it's in now, not a stale earlier one.
    const QString id = boxId.isEmpty() ? reg.activeBoxId() : boxId;
    const Box* b = id.isEmpty() ? reg.primary() : reg.currentBoxFor(id);
    if (!b) return nullptr;
    const auto it = m_boxManagers.find(b);
    return it != m_boxManagers.end() ? &it.value() : nullptr;
}

static QStringList mapSearchPaths(const QString& override,
                                  const DataLocationMgr* dlm)
{
    // Override wins; otherwise the DataLocationMgr cascade (user → pkg).
    // The user dir is ~/.showeq/maps — shared with showeq — so no
    // separate legacy fallback is needed.
    QStringList paths;
    if (!override.isEmpty()) {
        paths.append(override);
        return paths;
    }
    if (dlm) {
        paths.append(dlm->userDataDir("maps").absolutePath());
        paths.append(dlm->pkgDataDir("maps").absolutePath());
    }
    return paths;
}

static QFileInfo locateMap(const QStringList& dirs, const QString& filename)
{
    for (const QString& d : dirs) {
        QFileInfo fi(d + "/" + filename);
        if (fi.exists()) return fi;
    }
    return QFileInfo();
}

void DaemonApp::loadZoneMap(const QString& shortZoneName)
{
    m_mapData->clear();
    if (shortZoneName.isEmpty()) {
        return;
    }

    const QStringList roots = mapSearchPaths(m_cfg.mapsDir,
                                             m_dataLocationMgr.get());

    // Build the effective search path. When an active package is set
    // (!= "default"), look in <root>/<package>/ FIRST across all roots,
    // then fall back to the flat roots (the synthetic "default" package)
    // so a package that lacks the current zone still renders from the
    // shared maps root. Numbered-layer loading then operates within
    // whichever directory provided the base file (locateMap order).
    QStringList dirs;
    const bool usePackage =
        !m_mapPackage.isEmpty() && m_mapPackage != QStringLiteral("default");
    if (usePackage) {
        for (const QString& root : roots)
            dirs.append(root + QLatin1Char('/') + m_mapPackage);
    }
    dirs.append(roots);

    // Mirrors showeq/src/map.cpp:370-423 — locate the base .map/.txt then
    // any numbered layer files (_1, _2, ...). import=true for layer files so
    // they accumulate into the same MapData rather than replacing it.
    const QFileInfo baseMap = locateMap(dirs, shortZoneName + ".map");
    const QFileInfo baseTxt = locateMap(dirs, shortZoneName + ".txt");

    QString extension;
    QStringList files;
    if (baseMap.exists()) {
        extension = ".map";
        files.append(baseMap.absoluteFilePath());
    } else if (baseTxt.exists()) {
        extension = ".txt";
        files.append(baseTxt.absoluteFilePath());
    } else {
        qInfo("no map file found for zone '%s' (searched: %s)",
              qUtf8Printable(shortZoneName),
              qUtf8Printable(dirs.join(", ")));
        return;
    }

    for (int i = 1; i < 10; ++i) {
        const QFileInfo layerFile =
            locateMap(dirs, shortZoneName + "_" + QString::number(i) + extension);
        if (layerFile.exists()) {
            files.append(layerFile.absoluteFilePath());
        }
    }

    bool import = false;
    for (const QString& f : files) {
        if (extension == ".map") {
            m_mapData->loadMap(f, import);
        } else {
            m_mapData->loadSOEMap(f, import);
        }
        import = true;
    }
    qInfo("loaded map for zone '%s' (%lld layer(s) from %s)",
          qUtf8Printable(shortZoneName), (long long)files.size(),
          qUtf8Printable(QFileInfo(files.first()).absolutePath()));
}

// Count base zone files (.map/.txt) in `dir`, ignoring numbered layer
// files <zone>_N.{map,txt} — those are layers of an existing base map,
// not distinct zones. A zone with both a .map and a .txt counts once.
static uint32_t countZoneFiles(const QString& dir)
{
    QDir d(dir);
    const QStringList filters{QStringLiteral("*.map"), QStringLiteral("*.txt")};
    QSet<QString> bases;
    for (const QString& name : d.entryList(filters, QDir::Files)) {
        QString base = QFileInfo(name).completeBaseName();
        const int us = base.lastIndexOf(QLatin1Char('_'));
        if (us > 0 && us + 1 < base.size()) {
            bool numeric = false;
            base.mid(us + 1).toInt(&numeric);
            if (numeric)
                continue; // <zone>_N layer file
        }
        bases.insert(base);
    }
    return static_cast<uint32_t>(bases.size());
}

std::vector<MapPackageInfo>
DaemonApp::mapPackagesIn(const QStringList& roots) const
{
    std::vector<MapPackageInfo> out;

    // Synthetic "default" package == the flat root(s). Sum base zone files
    // sitting directly in the search roots.
    uint32_t defaultZones = 0;
    for (const QString& root : roots)
        defaultZones += countZoneFiles(root);
    out.push_back({QStringLiteral("default"), QStringLiteral("default"),
                   defaultZones});

    // Each immediate subdir holding at least one .map/.txt is a package.
    // First root wins on duplicate package names across roots.
    QSet<QString> seen;
    for (const QString& root : roots) {
        QDir d(root);
        for (const QString& sub :
             d.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (sub == QStringLiteral("default"))
                continue; // reserved id
            if (seen.contains(sub))
                continue;
            const uint32_t zones =
                countZoneFiles(root + QLatin1Char('/') + sub);
            if (zones == 0)
                continue;
            seen.insert(sub);
            out.push_back({sub, sub, zones});
        }
    }
    return out;
}

std::vector<MapPackageInfo> DaemonApp::mapPackages() const
{
    return mapPackagesIn(mapSearchPaths(m_cfg.mapsDir,
                                        m_dataLocationMgr.get()));
}

QString DaemonApp::setMapPackage(const QString& id)
{
    // Fall back to "default" when the requested package is unknown.
    QString resolved = QStringLiteral("default");
    if (!id.isEmpty() && id != QStringLiteral("default")) {
        for (const auto& p : mapPackages()) {
            if (p.id == id) { resolved = id; break; }
        }
    }
    m_mapPackage = resolved;

    // Persist (XMLPreferences, [Maps] Package). Mirrors how Network/Device
    // is read/written elsewhere via pSEQPrefs. XMLPreferences batches
    // modifications in memory, so flush with save() — same pattern as
    // PrefsBroker::apply — otherwise the choice is lost on restart (the
    // daemon hot-reloads via _exit(75), bypassing any aboutToQuit flush).
    if (pSEQPrefs) {
        pSEQPrefs->setPrefString("Package", "Maps", resolved);
        pSEQPrefs->save();
    }

    // Re-resolve the current zone's map within the (new) active package and
    // broadcast a fresh MapPackagesUpdate + ZoneChanged so every client
    // re-renders. No-op gracefully if no zone is known yet.
    const QString zone = m_zoneMgr ? m_zoneMgr->shortZoneName() : QString();
    if (!zone.isEmpty())
        loadZoneMap(zone);

    if (m_ws) {
        seq::v1::Envelope upd;
        seq::encode::fillMapPackages(upd.mutable_map_packages(), mapPackages(),
                                     m_mapPackage);
        m_ws->broadcast(upd);

        if (!zone.isEmpty()) {
            seq::v1::Envelope zc;
            auto* z = zc.mutable_zone_changed();
            z->set_zone_short(zone.toStdString());
            if (m_zoneMgr)
                z->set_zone_long(m_zoneMgr->longZoneName().toStdString());
            if (m_mapData)
                seq::encode::fillMapGeometry(z->mutable_geometry(), *m_mapData);
            m_ws->broadcast(zc);
        }
    }
    return resolved;
}

void DaemonApp::exportHandoffState(const QString& configDir) const
{
    if (m_packet)
        m_packet->exportHandoffState(configDir);

    // Save zone/spawn/player state so the new binary can restore them
    // before the web client reconnects and requests a snapshot. The
    // ".hstate_" prefix keeps these files distinct from any normal
    // save/restore files the user might have configured.
    if (m_zoneMgr && m_spawnShell && m_player) {
        showeq_params->saveRestoreBaseFilename = configDir + "/.hstate_";
        m_zoneMgr->saveZoneState();
        m_spawnShell->saveSpawns();
        m_player->savePlayerState();
    }

    // SpawnMonitor normally flushes via aboutToQuit, which _exit(75)
    // bypasses. Flush it explicitly so the new daemon loads current data.
    if (m_spawnMonitor)
        m_spawnMonitor->saveSpawnPoints();
}

bool DaemonApp::importHandoffState(const QString& configDir)
{
    if (!m_packet || !m_packet->importHandoffState(configDir))
        return false;

    if (m_zoneMgr && m_spawnShell && m_player) {
        showeq_params->saveRestoreBaseFilename = configDir + "/.hstate_";
        // Zone must be restored first — restoreSpawns checks the zone name.
        m_zoneMgr->restoreZoneState();
        m_spawnShell->restoreSpawns();
        m_player->restorePlayerState();
        // Clean up; these files are only valid for one handoff.
        QFile::remove(configDir + "/.hstate_Zone.dat");
        QFile::remove(configDir + "/.hstate_Spawns.dat");
        QFile::remove(configDir + "/.hstate_Player.dat");
    }

    // Reload map geometry and spawn-point list for the restored zone.
    // These normally fire via zoneBegin/zoneChanged signals which are
    // not emitted during a handoff restore.
    const QString zone = m_zoneMgr ? m_zoneMgr->shortZoneName() : QString();
    if (!zone.isEmpty() && zone != "unknown") {
        loadZoneMap(zone);
        // SpawnMonitor self-wired to zoneChanged in its ctor but never
        // received one. Call the slot directly: it sets m_zoneName and
        // calls loadSpawnPoints() without going through the signal (which
        // would clear SpawnShell state we just restored).
        if (m_spawnMonitor)
            m_spawnMonitor->zoneChanged(zone);
    }

    return true;
}
