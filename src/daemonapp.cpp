#include "daemonapp.h"

#include <QFileInfo>
#include <QLoggingCategory>

#include "main.h"

#include "datalocationmgr.h"
#include "datetimemgr.h"
#include "eqstr.h"
#include "filtermgr.h"
#include "guild.h"
#include "packet.h"
#include "packetcommon.h"
#include "packetinfo.h"
#include "player.h"
#include "spawnshell.h"
#include "spells.h"
#include "wsserver.h"
#include "zonemgr.h"

namespace seq { void initGlobals(const QString& def, const QString& user); }

DaemonApp::DaemonApp(Config cfg, QObject* parent)
    : QObject(parent)
    , m_cfg(std::move(cfg))
    , m_ws(std::make_unique<WsServer>(this))
{
}

DaemonApp::~DaemonApp() = default;

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
    // daemon still runs, we just can't render spell names.
    m_dateTimeMgr = new DateTimeMgr(this, "datetimemgr");
    const QFileInfo spellsFile =
        m_dataLocationMgr->findExistingFile(".", "spells_us.txt");
    m_spells    = new Spells(spellsFile.exists()
                              ? spellsFile.absoluteFilePath()
                              : QString());
    m_eqStrings = new EQStr();

    if (!startCapture()) {
        return false;
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

    // Let the WebSocket server hand these to each SessionAdapter it spawns.
    m_ws->setState(m_spawnShell, m_zoneMgr, m_player);

    wireZoneMgr();
    wireSpawnShell();

    // Start the capture pipeline (opcode loading happened in the EQPacket
    // constructor; start() kicks the thread + decoder timer).
    m_packet->start(10);
    qInfo("capture pipeline running");
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
        /*recordPackets*/ false,
        /*playbackPackets*/ hasReplay ? 1 : PLAYBACK_OFF,
        /*playbackSpeed*/ 0,
        this, "packet");
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
}
