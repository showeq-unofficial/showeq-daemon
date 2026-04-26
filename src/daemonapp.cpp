#include "daemonapp.h"

#include <QCoreApplication>
#include <QDir>
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
#include "filesink.h"
#include "filtermgr.h"
#include "group.h"
#include "guild.h"
#include "mapcore.h"
#include "messagefilter.h"
#include "messages.h"
#include "messageshell.h"
#include "opcodestats.h"
#include "packet.h"
#include "packetcommon.h"
#include "packetinfo.h"
#include "player.h"
#include "prefsbroker.h"
#include "sessionadapter.h"
#include "spawnshell.h"
#include "spells.h"
#include "spellshell.h"
#include "wsserver.h"
#include "xmlpreferences.h"
#include "zonemgr.h"

#include "seq/v1/client.pb.h"

namespace seq { void initGlobals(const QString& def, const QString& user); }

// Forward decls for helpers defined further down in this file.
static QString legacyShoweqHome();

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
    if (!startServer()) {
        return false;
    }

    // DataLocationMgr resolves file paths against ~/.showeq-daemon (user)
    // and PKGDATADIR (install prefix). When --config-dir is passed, we copy
    // PKGDATADIR by pointing the user dir at it instead, so the bundled
    // opcode XMLs are picked up without an install step.
    if (!m_cfg.configDir.isEmpty()) {
        m_dataLocationMgr = std::make_unique<DataLocationMgr>(m_cfg.configDir);
        qInfo("config dir: %s", qUtf8Printable(m_cfg.configDir));
    } else {
        m_dataLocationMgr = std::make_unique<DataLocationMgr>(".showeq-daemon");
        m_dataLocationMgr->setupUserDirectory();
    }

    const QFileInfo defPref =
        m_dataLocationMgr->findExistingFile(".", "seqdef.xml", true, false);
    const QFileInfo userPref =
        m_dataLocationMgr->findWriteFile(".", "showeq-daemon.xml", true, true);
    seq::initGlobals(defPref.absoluteFilePath(), userPref.absoluteFilePath());

    // Cross-cutting helpers the extracted managers expect to find on the
    // QObject tree. Spells is optional — if spells_us.txt is missing the
    // daemon still runs, we just can't render spell names or compute
    // calc-from-level durations. Search cascade:
    //   1. DataLocationMgr (user dir / --config-dir / pkg dir)
    //   2. /usr/local/share/showeq/ (showeq-c install location)
    //   3. ~/.showeq/ (legacy showeq-c user dir)
    m_dateTimeMgr = new DateTimeMgr(this, "datetimemgr");
    QFileInfo spellsFile =
        m_dataLocationMgr->findExistingFile(".", "spells_us.txt");
    if (!spellsFile.exists()) {
        for (const QString& path : {
                 QStringLiteral("/usr/local/share/showeq/spells_us.txt"),
                 legacyShoweqHome() + "/.showeq/spells_us.txt",
             }) {
            QFileInfo fi(path);
            if (fi.exists()) { spellsFile = fi; break; }
        }
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
        for (const QString& path : {
                 QStringLiteral("/usr/local/share/showeq/eqstr_us.txt"),
                 legacyShoweqHome() + "/.showeq/eqstr_us.txt",
             }) {
            QFileInfo fi(path);
            if (fi.exists()) { eqstrFile = fi; break; }
        }
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

    // EQPacket's ctor calls pcap_create/pcap_activate, which exit(1)s when
    // there's no device available. Skip capture setup entirely when the
    // user passed neither --device nor --replay; the daemon then serves
    // clients with an empty state — useful for smoke tests and local dev.
    if (!m_cfg.device.isEmpty() || !m_cfg.replay.isEmpty()) {
        if (!startCapture()) {
            return false;
        }
    }

    m_zoneMgr = new ZoneMgr(this, "zonemgr");

    const QFileInfo guildFile =
        m_dataLocationMgr->findWriteFile("tmp", "guilds2.dat");
    m_guildMgr = new GuildMgr(guildFile.absoluteFilePath(), this, "guildmgr");

    m_player = new Player(this, m_zoneMgr, m_guildMgr);

    m_filterMgr = new FilterMgr(
        m_dataLocationMgr.get(),
        /*filterFile*/ "global.xml",
        /*caseSensitive*/ false);
    const QString shortZoneName = m_zoneMgr->shortZoneName();
    if (!shortZoneName.isEmpty()) {
        m_filterMgr->loadZone(shortZoneName);
    }

    m_spawnShell = new SpawnShell(*m_filterMgr, m_zoneMgr, m_player, m_guildMgr);

    // GroupMgr tracks group members. Wiring matches showeq-c/src/
    // interface.cpp:593-615 — needs the player profile signal, three
    // group opcode handlers, and the spawn lifecycle slots.
    m_groupMgr = new GroupMgr(m_spawnShell, m_player, this, "groupMgr");
    connect(m_zoneMgr,    SIGNAL(playerProfile(const charProfileStruct*)),
            m_groupMgr,   SLOT(player(const charProfileStruct*)));
    connect(m_spawnShell, SIGNAL(addItem(const Item*)),
            m_groupMgr,   SLOT(addItem(const Item*)));
    connect(m_spawnShell, SIGNAL(delItem(const Item*)),
            m_groupMgr,   SLOT(delItem(const Item*)));
    connect(m_spawnShell, SIGNAL(killSpawn(const Item*, const Item*, uint16_t)),
            m_groupMgr,   SLOT(killSpawn(const Item*)));

    // MessageShell parses chat / system / NPC text packets and emits
    // structured signals (Phase 3 only consumes chatMessage; the rest of
    // its slots stay idle until later phases wire their opcodes). Needs
    // MessageFilters + Messages even though we don't query them today.
    m_messageFilters = new MessageFilters(this, "messageFilters");
    m_messages = new Messages(m_dateTimeMgr, m_messageFilters,
                              this, "messages");
    m_messageShell = new MessageShell(m_messages, m_eqStrings, m_spells,
                                      m_zoneMgr, m_spawnShell, m_player,
                                      this, "messageShell");

    // SpellShell tracks active buffs / outgoing casts. Wires player
    // signals + clear-on-zone, mirroring showeq-c interface.cpp:967-988.
    m_spellShell = new SpellShell(m_player, m_spawnShell, m_spells);
    m_spellShell->setParent(this);
    connect(m_player, SIGNAL(newPlayer()),
            m_spellShell, SLOT(clear()));
    connect(m_player, SIGNAL(buffLoad(const spellBuff*)),
            m_spellShell, SLOT(buffLoad(const spellBuff*)));
    connect(m_zoneMgr, SIGNAL(zoneChanged(const QString&)),
            m_spellShell, SLOT(zoneChanged()));
    connect(m_spawnShell, SIGNAL(killSpawn(const Item*, const Item*, uint16_t)),
            m_spellShell, SLOT(killSpawn(const Item*)));

    // CombatRouter parses OP_Action2 into structured combat events for
    // the websocket layer.
    m_combatRouter = new CombatRouter(m_spawnShell, m_spells, this);

    // CategoryMgr loads user-defined Category groupings from the
    // pSEQPrefs XML preferences (section "CategoryMgr"). seqdef.xml ships
    // with a default set so the list is never empty.
    m_categoryMgr = new CategoryMgr(this, "categoryMgr");

    // PrefsBroker is the curated XMLPreferences <-> wire bridge. Constructed
    // after pSEQPrefs is initialized but before any client can connect, so
    // the very first PrefsSnapshot reflects the on-disk state.
    m_prefsBroker = new PrefsBroker(this);

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

    // Let the WebSocket server hand these to each SessionAdapter it spawns.
    m_ws->setState(m_spawnShell, m_zoneMgr, m_player, m_mapData.get(),
                   m_messageShell, m_groupMgr, m_spellShell,
                   m_combatRouter, m_categoryMgr, m_filterMgr,
                   m_prefsBroker);

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
                                             m_filterMgr, m_prefsBroker, this);
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

        wireZoneMgr();
        wireSpawnShell();

        // In golden-replay mode (--replay + --record-golden) we want the
        // process to exit cleanly at EOF so the test harness can compare
        // .pbstream files. A short delay after EOF lets any final
        // direct-connected slots finish writing into the FileSink.
        if (!m_cfg.replay.isEmpty() && !m_cfg.recordGolden.isEmpty()) {
            connect(m_packet, &EQPacket::playbackFinished, this, [] {
                QTimer::singleShot(50, &QCoreApplication::quit);
            });
        }

        m_packet->start(10);
        qInfo("capture pipeline running");
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
    m_packet = new EQPacket(
        worldOpcodes.absoluteFilePath(),
        zoneOpcodes.absoluteFilePath(),
        /*arqSeqGiveUp*/ 512,
        /*device*/ hasReplay ? QString() : m_cfg.device,
        /*ip*/ AUTOMATIC_CLIENT_IP,
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

void DaemonApp::wireZoneMgr()
{
    // Mirrors showeq-c/src/interface.cpp:568-588 — the minimum set of
    // zone-packet slots needed for zone transitions and player profile.
    m_packet->connect2("OP_ZoneEntry", SP_Zone, DIR_Client,
                       "ClientZoneEntryStruct", SZC_Match,
                       m_zoneMgr,
                       SLOT(zoneEntryClient(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_PlayerProfile", SP_Zone, DIR_Server,
                       "charProfileStruct", SZC_Match,
                       m_zoneMgr,
                       SLOT(zonePlayer(const uint8_t*, size_t)));
    m_packet->connect2("OP_ZoneChange", SP_Zone, DIR_Client|DIR_Server,
                       "zoneChangeStruct", SZC_Match,
                       m_zoneMgr,
                       SLOT(zoneChange(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_NewZone", SP_Zone, DIR_Server,
                       "newZoneStruct", SZC_Match,
                       m_zoneMgr,
                       SLOT(zoneNew(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_SendZonePoints", SP_Zone, DIR_Server,
                       "zonePointsStruct", SZC_None,
                       m_zoneMgr,
                       SLOT(zonePoints(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_DzSwitchInfo", SP_Zone, DIR_Server,
                       "dzSwitchInfo", SZC_None,
                       m_zoneMgr,
                       SLOT(dynamicZonePoints(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_DzInfo", SP_Zone, DIR_Server,
                       "dzInfo", SZC_None,
                       m_zoneMgr,
                       SLOT(dynamicZoneInfo(const uint8_t*, size_t, uint8_t)));

    connect(m_zoneMgr, SIGNAL(playerProfile(const charProfileStruct*)),
            m_player,  SLOT(player(const charProfileStruct*)));

    // Player's own per-tick movement. Mirrors showeq-c/src/interface.cpp:1000.
    // OP_ClientUpdate is overloaded — DIR_Server uses playerSpawnPosStruct
    // (other players' updates, wired in wireSpawnShell), DIR_Client uses
    // playerSelfPosStruct (this user's movement). Both feed the Player
    // object's own changeItem signal.
    m_packet->connect2("OP_ClientUpdate", SP_Zone, DIR_Server|DIR_Client,
                       "playerSelfPosStruct", SZC_Match,
                       m_player,
                       SLOT(playerUpdateSelf(const uint8_t*, size_t, uint8_t)));
}

void DaemonApp::wireSpawnShell()
{
    // Mirrors showeq-c/src/interface.cpp:880-948 — the spawn-opcode wiring.
    m_packet->connect2("OP_GroundSpawn", SP_Zone, DIR_Server,
                       "makeDropStruct", SZC_Modulus,
                       m_spawnShell,
                       SLOT(newGroundItem(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_ClickObject", SP_Zone, DIR_Server,
                       "remDropStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(removeGroundItem(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_SpawnDoor", SP_Zone, DIR_Server,
                       "doorStruct", SZC_Modulus,
                       m_spawnShell,
                       SLOT(newDoorSpawns(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_ZoneEntry", SP_Zone, DIR_Server,
                       "uint8_t", SZC_None,
                       m_spawnShell,
                       SLOT(zoneEntry(const uint8_t*, size_t)));
    m_packet->connect2("OP_MobUpdate", SP_Zone, DIR_Server|DIR_Client,
                       "spawnPositionUpdate", SZC_Match,
                       m_spawnShell,
                       SLOT(updateSpawns(const uint8_t*)));
    m_packet->connect2("OP_WearChange", SP_Zone, DIR_Server|DIR_Client,
                       "SpawnUpdateStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(updateSpawnInfo(const uint8_t*)));
    m_packet->connect2("OP_HPUpdate", SP_Zone, DIR_Server|DIR_Client,
                       "hpNpcUpdateStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(updateNpcHP(const uint8_t*)));

    // Player-vital wirings. Each handler filters by spawnId == self
    // so the same opcodes route through both SpawnShell (any-spawn
    // updates) and Player (only when packet is for the local PC).
    // Mirrors showeq-c/src/interface.cpp:909-1018. OP_Stamina is
    // wired even though its opcode is still id="ffff" — once it's
    // resolved the slot fires automatically without code changes.
    m_packet->connect2("OP_HPUpdate", SP_Zone, DIR_Server|DIR_Client,
                       "hpNpcUpdateStruct", SZC_Match,
                       m_player,
                       SLOT(updateNpcHP(const uint8_t*)));
    m_packet->connect2("OP_ManaChange", SP_Zone, DIR_Server,
                       "manaDecrementStruct", SZC_Match,
                       m_player,
                       SLOT(manaChange(const uint8_t*)));
    m_packet->connect2("OP_Stamina", SP_Zone, DIR_Server,
                       "staminaStruct", SZC_Match,
                       m_player,
                       SLOT(updateStamina(const uint8_t*)));
    m_packet->connect2("OP_WearChange", SP_Zone, DIR_Server|DIR_Client,
                       "SpawnUpdateStruct", SZC_Match,
                       m_player,
                       SLOT(updateSpawnInfo(const uint8_t*)));
    m_packet->connect2("OP_DeleteSpawn", SP_Zone, DIR_Server|DIR_Client,
                       "deleteSpawnStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(deleteSpawn(const uint8_t*)));
    m_packet->connect2("OP_SpawnRename", SP_Zone, DIR_Server,
                       "spawnRenameStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(renameSpawn(const uint8_t*)));
    m_packet->connect2("OP_Illusion", SP_Zone, DIR_Server|DIR_Client,
                       "spawnIllusionStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(illusionSpawn(const uint8_t*)));
    m_packet->connect2("OP_SpawnAppearance", SP_Zone, DIR_Server|DIR_Client,
                       "spawnAppearanceStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(updateSpawnAppearance(const uint8_t*)));
    m_packet->connect2("OP_Death", SP_Zone, DIR_Server,
                       "newCorpseStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(killSpawn(const uint8_t*)));
    m_packet->connect2("OP_Shroud", SP_Zone, DIR_Server,
                       "spawnShroudSelf", SZC_None,
                       m_spawnShell,
                       SLOT(shroudSpawn(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_RemoveSpawn", SP_Zone, DIR_Server|DIR_Client,
                       "removeSpawnStruct", SZC_None,
                       m_spawnShell,
                       SLOT(removeSpawn(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_Consider", SP_Zone, DIR_Server|DIR_Client,
                       "considerStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(consMessage(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_TargetMouse", SP_Zone, DIR_Server|DIR_Client,
                       "clientTargetStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(clientTarget(const uint8_t*)));
    m_packet->connect2("OP_NpcMoveUpdate", SP_Zone, DIR_Server,
                       "uint8_t", SZC_None,
                       m_spawnShell,
                       SLOT(npcMoveUpdate(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_ClientUpdate", SP_Zone, DIR_Server,
                       "playerSpawnPosStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(playerUpdate(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_CorpseLocResponse", SP_Zone, DIR_Server,
                       "corpseLocStruct", SZC_Match,
                       m_spawnShell,
                       SLOT(corpseLoc(const uint8_t*)));

    // Chat. OP_CommonMessage carries the player-to-player channels
    // (/say /tell /guild /group /raid /shout /auction /ooc) parsed by
    // MessageShell::channelMessage; the matching legacy
    // showeq-c/src/interface.cpp:679 wires it under the same name. The
    // daemon was previously asking for "OP_ChannelMessage", which
    // isn't in conf/zoneopcodes.xml, so the slot never fired and chat
    // was silently dropped.
    m_packet->connect2("OP_CommonMessage", SP_Zone, DIR_Client|DIR_Server,
                       "channelMessageStruct", SZC_None,
                       m_messageShell,
                       SLOT(channelMessage(const uint8_t*, size_t, uint8_t)));
    // System / NPC / spell text. Each of these resolves through
    // chatColor2MessageType(messageColor) into an MT_* channel id
    // (MT_General, MT_Spell, MT_Money, MT_Random, MT_Emote, ...) so the
    // web chat panel can categorize and filter without daemon-side
    // policy. DIR_Server only — the client echoes nothing useful here.
    m_packet->connect2("OP_FormattedMessage", SP_Zone, DIR_Server,
                       "formattedMessageStruct", SZC_None,
                       m_messageShell,
                       SLOT(formattedMessage(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_SimpleMessage", SP_Zone, DIR_Server,
                       "simpleMessageStruct", SZC_Match,
                       m_messageShell,
                       SLOT(simpleMessage(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_SpecialMesg", SP_Zone, DIR_Server,
                       "specialMessageStruct", SZC_None,
                       m_messageShell,
                       SLOT(specialMessage(const uint8_t*, size_t, uint8_t)));

    // Group opcodes — mirrors showeq-c/src/interface.cpp:595-606.
    m_packet->connect2("OP_GroupUpdate", SP_Zone, DIR_Server,
                       "uint8_t", SZC_None,
                       m_groupMgr,
                       SLOT(groupUpdate(const uint8_t*, size_t)));
    m_packet->connect2("OP_GroupFollow", SP_Zone, DIR_Server,
                       "groupFollowStruct", SZC_Match,
                       m_groupMgr,
                       SLOT(addGroupMember(const uint8_t*)));
    m_packet->connect2("OP_GroupDisband", SP_Zone, DIR_Server,
                       "groupDisbandStruct", SZC_Match,
                       m_groupMgr,
                       SLOT(removeGroupMember(const uint8_t*)));
    m_packet->connect2("OP_GroupDisband2", SP_Zone, DIR_Server,
                       "groupDisbandStruct", SZC_Match,
                       m_groupMgr,
                       SLOT(removeGroupMember(const uint8_t*)));

    // SpellShell — mirrors showeq-c/src/interface.cpp:973-988.
    m_packet->connect2("OP_CastSpell", SP_Zone, DIR_Server|DIR_Client,
                       "startCastStruct", SZC_Match,
                       m_spellShell,
                       SLOT(selfStartSpellCast(const uint8_t*)));
    m_packet->connect2("OP_Buff", SP_Zone, DIR_Server|DIR_Client,
                       "buffStruct", SZC_Match,
                       m_spellShell,
                       SLOT(buff(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_Action", SP_Zone, DIR_Server|DIR_Client,
                       "actionStruct", SZC_Match,
                       m_spellShell,
                       SLOT(action(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_Action", SP_Zone, DIR_Server|DIR_Client,
                       "actionAltStruct", SZC_Match,
                       m_spellShell,
                       SLOT(action(const uint8_t*, size_t, uint8_t)));
    m_packet->connect2("OP_SimpleMessage", SP_Zone, DIR_Server,
                       "simpleMessageStruct", SZC_Match,
                       m_spellShell,
                       SLOT(simpleMessage(const uint8_t*, size_t, uint8_t)));

    // Combat events. action2Struct carries damage data (showeq-c handles
    // this in interface.cpp around line 4950). OP_Action2 is server-side.
    m_packet->connect2("OP_Action2", SP_Zone, DIR_Client|DIR_Server,
                       "action2Struct", SZC_Match,
                       m_combatRouter,
                       SLOT(action2(const uint8_t*, size_t, uint8_t)));
}

static QString legacyShoweqHome()
{
    // Under sudo, $HOME (and QDir::homePath) point at /root, not the
    // invoking user's home — so the legacy ~/.showeq/maps directory the
    // user actually populated isn't found. SUDO_USER is set by sudo to the
    // original username, which we use to reconstruct the right path.
    const QByteArray sudoUser = qgetenv("SUDO_USER");
    if (!sudoUser.isEmpty() && qgetenv("USER") == "root") {
        return QStringLiteral("/home/") + QString::fromLocal8Bit(sudoUser);
    }
    return QDir::homePath();
}

static QStringList mapSearchPaths(const QString& override,
                                  const DataLocationMgr* dlm)
{
    // Override wins; otherwise default cascade: legacy showeq-c location,
    // then whatever DataLocationMgr considers the user + pkg "maps" dir.
    QStringList paths;
    if (!override.isEmpty()) {
        paths.append(override);
        return paths;
    }
    paths.append(legacyShoweqHome() + "/.showeq/maps");
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

    const QStringList dirs = mapSearchPaths(m_cfg.mapsDir,
                                            m_dataLocationMgr.get());

    // Mirrors showeq-c/src/map.cpp:370-423 — locate the base .map/.txt then
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
    qInfo("loaded map for zone '%s' (%d layer(s) from %s)",
          qUtf8Printable(shortZoneName), files.size(),
          qUtf8Printable(QFileInfo(files.first()).absolutePath()));
}
