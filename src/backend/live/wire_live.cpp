/*
 * wire_live.cpp — live/test backend wiring TU.
 *
 * Definition of DaemonApp::wireBoxPipeline() for the live/test targets. The
 * declaration stays in daemonapp.h and this is still a DaemonApp member, so
 * member access (m_itemCache / m_dateTimeMgr / m_zoneServerMgr) is unchanged —
 * this is a pure relocation of the body from daemonapp.cpp, byte-for-byte.
 *
 * CMake compiles exactly one wiring TU per target: this one for live/test,
 * src/backend/eql/wire_eql.cpp for eql. The install-order of the wire() calls
 * below is golden-sensitive (shared opcode+payload on one stream dispatches in
 * install order) — do not reorder.
 */

#include "daemonapp.h"

#include "combatrouter.h"
#include "datetimemgr.h"
#include "everquest.h"
#include "group.h"
#include "itemcache.h"
#include "messageshell.h"
#include "packet.h"        // EQStreamPairs, SP_World / SP_Zone
#include "packetcommon.h"
#include "packetinfo.h"
#include "packetstream.h"
#include "player.h"
#include "spawnshell.h"
#include "spellshell.h"
#include "zonemgr.h"
#include "zoneservermgr.h"

//----------------------------------------------------------------------
// DaemonApp::wireBoxPipeline (moved verbatim from daemonapp.cpp)
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
    wire("OP_InspectAnswer", SP_Zone, DIR_Server,
         "inspectDataStruct", SZC_Match,
         ms.messageShell, SLOT(inspectData(const uint8_t*)));

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
