/*
 * wire_eql.cpp — EverQuest Legends backend wiring TU.
 *
 * Definition of DaemonApp::wireBoxPipeline() for -DSEQ_TARGET=eql. Structurally
 * this is wire_live.cpp with five opcodes re-pointed at the backend-owned
 * EqlDispatch adapter (OP_PlayerProfile / OP_ClientUpdate / OP_NewZone /
 * OP_ZoneEntry / OP_MobUpdate) and the Live S>C OP_ClientUpdate spawn-position
 * wire dropped (that opcode/struct differs on Legends and isn't mapped yet).
 * Every OTHER wire is carried over from Live unchanged: it only fires if a
 * Legends opcode id collides with a Live-named opcode present in conf/eql, and
 * the size checks keep mismatched payloads from dispatching. Install order is
 * golden-sensitive — do not reorder.
 *
 * Handlers register via EQPacketStream::on() as typed PacketHandlers (seqBind),
 * not Qt string SLOTs. EqlDispatch is a plain object owned by a shared_ptr the
 * wired closures capture — no QObject, no moc, no ODR pull into live/test.
 */

#include "daemonapp.h"

#include <memory>

#include "combatrouter.h"
#include "datetimemgr.h"
#include "eqldispatch.h"        // backend/eql (same-dir include)
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
// DaemonApp::wireBoxPipeline (eql variant of wire_live.cpp)
void DaemonApp::wireBoxPipeline(EQPacketStream* worldC2S, EQPacketStream* worldS2C,
                                EQPacketStream* zoneC2S, EQPacketStream* zoneS2C,
                                const ManagerSet& ms, bool wireGlobalSinks)
{
    // Register one opcode→handler onto the stream(s) selected by (streamPair,
    // dir). The call sequence below is the exact concatenation of the old
    // wireZoneMgr() then wireSpawnShell(), and is golden-sensitive (shared
    // opcode+payload on one stream dispatches in install order) — do not
    // reorder. The handler is copied into each selected stream's dispatcher.
    auto wire = [&](const QString& op, EQStreamPairs sp, uint8_t dir,
                    const char* payload, EQSizeCheckType szt,
                    const PacketHandler& handler) {
        if (sp & SP_World) {
            if ((dir & DIR_Client) && worldC2S)
                worldC2S->on(op, payload, szt, handler);
            if ((dir & DIR_Server) && worldS2C)
                worldS2C->on(op, payload, szt, handler);
        }
        if (sp & SP_Zone) {
            if ((dir & DIR_Client) && zoneC2S)
                zoneC2S->on(op, payload, szt, handler);
            if ((dir & DIR_Server) && zoneS2C)
                zoneS2C->on(op, payload, szt, handler);
        }
    };

    // Backend-owned adapter holding the Legends handlers (never on the core
    // managers — see eqldispatch.h). Owned by a shared_ptr the wired closures
    // capture, so it lives as long as this box's stream dispatchers reference
    // it — no QObject parent needed.
    auto eql = std::make_shared<EqlDispatch>(ms.zoneMgr, ms.spawnShell, ms.player);

    // --- ZoneMgr: zone transitions + player profile.
    // (EQ Legends has no separate c2s OP_ZoneEntry: the 4606 c2s 92B is a
    // spawn-list request, acknowledged under OP_ZoneEntry below and NOT wired to
    // ZoneMgr::zoneEntryClient — on eql that would emit zoneBegin() and clear the
    // spawn list. Zone identity comes from OP_PlayerProfile + OP_NewZone.)
    // EQ Legends OP_PlayerProfile (0x5207): header-only identity decode.
    wire("OP_PlayerProfile", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         seqBind(eql, &EqlDispatch::profile));
    wire("OP_ZoneChange", SP_Zone, DIR_Client | DIR_Server,
         "zoneChangeStruct", SZC_Match,
         seqBind(ms.zoneMgr, &ZoneMgr::zoneChange));
    // EQ Legends OP_NewZone (0x1dbf) S>C: the authoritative current zone, carried
    // as packed short + long name text (parsed in EqlDispatch). Fires once per
    // zone-in, AFTER the profile + bulk spawn list — EqlDispatch::newZone drives
    // ZoneMgr::setZoneByName -> zoneResolved (map/filter/web, no spawn-clear /
    // player-reset). The old 0x4bc8 "OP_NewZone" carried only the BIND zone id
    // (identical across zones) and is not OP_NewZone. See OPCODES_LEGENDS.md.
    wire("OP_NewZone", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         seqBind(eql, &EqlDispatch::newZone));
    wire("OP_SendZonePoints", SP_Zone, DIR_Server,
         "zonePointsStruct", SZC_None,
         seqBind(ms.zoneMgr, &ZoneMgr::zonePoints));
    wire("OP_DzSwitchInfo", SP_Zone, DIR_Server,
         "dzSwitchInfo", SZC_None,
         seqBind(ms.zoneMgr, &ZoneMgr::dynamicZonePoints));
    wire("OP_DzInfo", SP_Zone, DIR_Server,
         "dzInfo", SZC_None,
         seqBind(ms.zoneMgr, &ZoneMgr::dynamicZoneInfo));

    // Cross-manager: profile feeds Player too (after GroupMgr, which is
    // connected in buildManagerSet() — preserves slot fire order).
    connect(ms.zoneMgr, SIGNAL(playerProfile(const charProfileStruct*)),
            ms.player,  SLOT(player(const charProfileStruct*)));

    // EQ Legends OP_ClientUpdate (0x7171): C>S self-position (42B float).
    // Wired DIR_Client only — the 28B S>C other-player variant isn't decoded yet.
    // Size-gated on playerSelfPosStruct (sizeof==42); decode is Rust
    // (decode_player_self_pos). The eql self-pos layout differs from Live's but
    // the size matches, so this is a size-gate only, not a struct cast.
    wire("OP_ClientUpdate", SP_Zone, DIR_Client,
         "playerSelfPosStruct", SZC_Match,
         seqBind(eql, &EqlDispatch::playerUpdateSelf));

    // OP_TimeOfDay / OP_ZoneServerInfo feed daemon-GLOBAL sinks. Only the
    // active box wires them — otherwise every box's (now unmuted) world/zone
    // stream would re-fire them, duplicating ZoneServer / EqTimeSync envelopes.
    //
    // (EQ Legends) OP_ItemPacket (0x74b0) is NOT wired: the Legends bulk-item
    // format differs from Live's itemPacketStruct, so feeding it to the Live
    // itemCache would mis-parse. It's identified in conf/eql/opcodes.toml (for
    // recon labeling) but its decoder is future work — see OPCODES_LEGENDS.md.
    if (wireGlobalSinks) {
        wire("OP_TimeOfDay", SP_Zone, DIR_Server,
             "timeOfDayStruct", SZC_Match,
             seqBind(m_dateTimeMgr, &DateTimeMgr::timeOfDay));
        wire("OP_ZoneServerInfo", SP_World, DIR_Server,
             "zoneServerInfoStruct", SZC_Match,
             seqBind(m_zoneServerMgr, &ZoneServerMgr::zoneServerInfo));
    }

    // --- SpawnShell: spawn lifecycle + positions.
    // eql ground-item defs are makeDropStruct/none (variable name field);
    // szt matches conf/eql/opcodes.toml so the handler binds directly.
    wire("OP_GroundSpawn", SP_Zone, DIR_Server,
         "makeDropStruct", SZC_None,
         seqBind(ms.spawnShell, &SpawnShell::newGroundItem));
    wire("OP_ClickObject", SP_Zone, DIR_Server,
         "remDropStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::removeGroundItem));
    // OP_SpawnDoor (0x71ca): DECODER DEFERRED. eql door rows are 132B (1452=11*132)
    // vs Live doorStruct 136B, so the modulus check on the 136B struct rejects the
    // bulk array. Named in conf/eql/opcodes.toml (uint8_t/none) for logs; wire it
    // once a 132B doorStruct size override (or an eql door decoder) exists.
    // EQ Legends OP_ZoneEntry (0x4606): one spawn per payload (name + block);
    // EqlDispatch parses the variable-length name and the fixed spawn block.
    // Stock-SEQ name: the s2c OP_ZoneEntry has been the per-spawn payload since
    // 2008 (OP_ZoneSpawns is the dead bulk-array op). Replaces the leftover Live
    // SpawnShell::zoneEntry wire — that path calls m_player->update() on the PC
    // record, which eql routes through EqlDispatch::spawn/upsertSpawn instead.
    wire("OP_ZoneEntry", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         seqBind(eql, &EqlDispatch::spawn));
    // EQ Legends OP_MobUpdate (0x67e0): per-mob position update (14B),
    // byte-identical to Live spawnPositionUpdate — size-gate on it directly;
    // decode via the shared Rust decode_mob_update.
    wire("OP_MobUpdate", SP_Zone, DIR_Server,
         "spawnPositionUpdate", SZC_Match,
         seqBind(eql, &EqlDispatch::mobUpdate));
    // EQ Legends OP_TargetMouse (0x1bfe): C>S target select. The Legends payload
    // is byte-identical to Live's clientTargetStruct ({u32 spawn_id}, 0 = clear),
    // so it needs NO Legends glue — wire straight to the neutral core handler,
    // exactly as wire_live.cpp does. clientTarget emits targetSpawn -> Targeted
    // envelope -> web (untarget = spawn_id 0). Confirmed 2026-07-07; see
    // OPCODES_LEGENDS.md.
    wire("OP_TargetMouse", SP_Zone, DIR_Client,
         "clientTargetStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::clientTarget));
    // OP_WearChange (0x5c62) is NAMED in conf/eql/opcodes.toml but deliberately
    // UNWIRED (l-patch addendum 3: stock's WearChange handler shows nothing for
    // equip changes). The two Live SpawnUpdateStruct/updateSpawnInfo bindings (kept
    // in wire_live.cpp) were removed here: eql's WearChange is a 32B {spawnId,
    // wearSlot, material} packet and sizeof(SpawnUpdateStruct)==32, so binding it
    // would SZC_Match and mis-decode wear bytes as a spawn update (spawn-state
    // corruption). Re-derive an eql-specific decoder if live equip tracking is wanted.
    // OP_SpawnAppearance2 type=0x2c = TLP mob-lock / FTE flag.
    wire("OP_SpawnAppearance2", SP_Zone, DIR_Server,
         "spawnAppearance2Struct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::updateSpawnLock));
    // OP_HPUpdate is eql's multiplexed stat-sync channel (u32 id + u8 flags +
    // per-stat payload), not Live's fixed hpNpcUpdateStruct — decode via
    // EqlDispatch, which passes the real packet length. Feeds spawn HP cur/max
    // (wide numeric or narrow percent) and the player's mana (wide form).
    wire("OP_HPUpdate", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         seqBind(eql, &EqlDispatch::statSync));
    wire("OP_MobHealth", SP_Zone, DIR_Server,
         "mobHealthStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::updateMobHealth));

    // Player vitals — same opcodes also feed Player (filtered by self).
    wire("OP_ManaChange", SP_Zone, DIR_Server,
         "manaDecrementStruct", SZC_Match,
         seqBind(ms.player, &Player::manaChange));
    wire("OP_Stamina", SP_Zone, DIR_Server,
         "staminaStruct", SZC_Match,
         seqBind(ms.player, &Player::updateStamina));
    wire("OP_EndUpdate", SP_Zone, DIR_Server,
         "endUpdateStruct", SZC_Match,
         seqBind(ms.player, &Player::updateEndurance));
    // OP_ExpUpdate (0x6801, 16B expUpdateStruct): the regular exp bar. The ids
    // were cross-wired with OP_AAExpUpdate (0x42d1); corrected per the community
    // l-patch. exp@0 is 0-100000 permille — the same scale the daemon already
    // uses (reset()/loadProfile set m_minExp=0/m_maxExp=100000/m_tickExp=1), so
    // updateExp consumes it directly with no conversion. Routed through
    // EqlDispatch (not Player::updateExp direct) because eql has no discrete
    // level-up packet — expUpdate derives the ding from the exp wrap, then
    // forwards to Player::updateExp. See OP_LevelUpdate below + OPCODES_LEGENDS.md.
    wire("OP_ExpUpdate", SP_Zone, DIR_Server,
         "expUpdateStruct", SZC_Match,
         seqBind(eql, &EqlDispatch::expUpdate));
    // OP_LevelUpdate stays mapped to `ffff` in conf/eql/opcodes.toml — eql has no
    // discrete level packet (exhaustively confirmed 2026-07-10, OPCODES_LEGENDS.md),
    // so this never fires; kept wired in case a future patch introduces one.
    wire("OP_LevelUpdate", SP_Zone, DIR_Server,
         "levelUpUpdateStruct", SZC_Match,
         seqBind(ms.player, &Player::updateLevel));
    wire("OP_SkillUpdate", SP_Zone, DIR_Server,
         "skillIncStruct", SZC_Match,
         seqBind(ms.player, &Player::increaseSkill));
    // (OP_WearChange intentionally unwired on eql — see the note above OP_SpawnAppearance2.)
    wire("OP_DeleteSpawn", SP_Zone, DIR_Server | DIR_Client,
         "deleteSpawnStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::deleteSpawn));
    wire("OP_SpawnRename", SP_Zone, DIR_Server,
         "spawnRenameStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::renameSpawn));
    wire("OP_Illusion", SP_Zone, DIR_Server | DIR_Client,
         "spawnIllusionStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::illusionSpawn));
    wire("OP_SpawnAppearance", SP_Zone, DIR_Server | DIR_Client,
         "spawnAppearanceStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::updateSpawnAppearance));
    wire("OP_Death", SP_Zone, DIR_Server,
         "newCorpseStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::killSpawn));
    wire("OP_Shroud", SP_Zone, DIR_Server,
         "spawnShroudSelf", SZC_None,
         seqBind(ms.spawnShell, &SpawnShell::shroudSpawn));
    wire("OP_RemoveSpawn", SP_Zone, DIR_Server | DIR_Client,
         "removeSpawnStruct", SZC_None,
         seqBind(ms.spawnShell, &SpawnShell::removeSpawn));
    // OP_Consider (0x4212, 24B both dirs): eql's considerStruct is its OWN 24B
    // struct (seq-backend-eql owns it; Live's is 32B), size-gated via the backend
    // size table (struct_size_overrides) — so SZC_Match validates the real 24B
    // with the real name, no uint8_t placeholder. parse_consider -> shared
    // Consider -> neutral SpawnShell::consMessage (spawnConsidered -> Considered
    // envelope -> web select-on-consider), the same path Live uses.
    wire("OP_Consider", SP_Zone, DIR_Server | DIR_Client,
         "considerStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::consMessage));
    wire("OP_TargetMouse", SP_Zone, DIR_Server | DIR_Client,
         "clientTargetStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::clientTarget));
    wire("OP_NpcMoveUpdate", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         seqBind(ms.spawnShell, &SpawnShell::npcMoveUpdate));
    // (EQ Legends) The S>C OP_ClientUpdate spawn-position broadcast is a
    // different opcode/struct than Live's playerSpawnPosStruct; not yet mapped,
    // so no wire here (Live wired SpawnShell::playerUpdate at this spot).
    wire("OP_CorpseLocResponse", SP_Zone, DIR_Server,
         "corpseLocStruct", SZC_Match,
         seqBind(ms.spawnShell, &SpawnShell::corpseLoc));

    // --- MessageShell: chat / system / NPC text.
    wire("OP_CommonMessage", SP_Zone, DIR_Client | DIR_Server,
         "channelMessageStruct", SZC_None,
         seqBind(ms.messageShell, &MessageShell::channelMessage));
    wire("OP_FormattedMessage", SP_Zone, DIR_Server,
         "formattedMessageStruct", SZC_None,
         seqBind(ms.messageShell, &MessageShell::formattedMessage));
    wire("OP_SimpleMessage", SP_Zone, DIR_Server,
         "simpleMessageStruct", SZC_Match,
         seqBind(ms.messageShell, &MessageShell::simpleMessage));
    wire("OP_SpecialMesg", SP_Zone, DIR_Server,
         "specialMessageStruct", SZC_None,
         seqBind(ms.messageShell, &MessageShell::specialMessage));
    wire("OP_InspectAnswer", SP_Zone, DIR_Server,
         "inspectDataStruct", SZC_Match,
         seqBind(ms.messageShell, &MessageShell::inspectData));

    // --- GroupMgr.
    wire("OP_GroupUpdate", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         seqBind(ms.groupMgr, &GroupMgr::groupUpdate));
    wire("OP_GroupMemberList", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         seqBind(ms.groupMgr, &GroupMgr::groupMemberList));
    wire("OP_GroupDisband", SP_Zone, DIR_Server,
         "groupDisbandStruct", SZC_None,
         seqBind(ms.groupMgr, &GroupMgr::removeGroupMember));
    wire("OP_GroupDisband2", SP_Zone, DIR_Server,
         "groupDisbandStruct", SZC_None,
         seqBind(ms.groupMgr, &GroupMgr::removeGroupMember));

    // --- SpellShell. (OP_SimpleMessage here is a SECOND receiver after
    // MessageShell above — order preserved.)
    wire("OP_CastSpell", SP_Zone, DIR_Server | DIR_Client,
         "startCastStruct", SZC_Match,
         seqBind(ms.spellShell, &SpellShell::selfStartSpellCast));
    wire("OP_Buff", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         seqBind(ms.spellShell, &SpellShell::buff));
    // OP_BuffList (0x77ae): authoritative per-spawn active-buff list with real
    // remaining durations — preloads the player's buffs at zone-in and keeps the
    // spell-timer window accurate. Player-only (mob lists dropped).
    wire("OP_BuffList", SP_Zone, DIR_Server,
         "uint8_t", SZC_None,
         seqBind(ms.spellShell, &SpellShell::buffList));
    wire("OP_Action", SP_Zone, DIR_Server | DIR_Client,
         "actionStruct", SZC_Match,
         seqBind(ms.spellShell, &SpellShell::action));
    wire("OP_Action", SP_Zone, DIR_Server | DIR_Client,
         "actionAltStruct", SZC_Match,
         seqBind(ms.spellShell, &SpellShell::action));
    wire("OP_SimpleMessage", SP_Zone, DIR_Server,
         "simpleMessageStruct", SZC_Match,
         seqBind(ms.spellShell, &SpellShell::simpleMessage));

    // --- CombatRouter.
    wire("OP_Action2", SP_Zone, DIR_Client | DIR_Server,
         "action2Struct", SZC_Match,
         seqBind(ms.combatRouter, &CombatRouter::action2));
}
