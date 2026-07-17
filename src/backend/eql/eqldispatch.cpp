/*
 * eqldispatch.cpp — EverQuest Legends packet dispatch adapter.
 *
 * eql owns no wire-struct types: the Legends wire is decoded in Rust
 * (showeq-decoder-rs, `backend-eql` feature → `seq-backend-eql`) via the same
 * uniform `seq::rust::decode_*` surface Live uses. This adapter reads the decoded
 * fields and drives the core managers' target-NEUTRAL primitives (setIdentity /
 * applySelfPosition / setZoneByName / upsertSpawn / moveSpawn), so the frontend
 * (SpawnShell / Player / ZoneMgr) keeps only Live's struct set and nothing named
 * `legends*` exists in core. The per-field offsets/scales live in the Rust
 * parsers (the `seq-backend-eql` crate); see OPCODES_LEGENDS.md for the evidence.
 * Layout shuffles per patch — re-derive the Rust parser from captures.
 */
#include "eqldispatch.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include <QString>

#include "seq-bridge-cxx/lib.h"

#include "diagnosticmessages.h"
#include "packetcommon.h"   // DIR_Client / DIR_Server
#include "player.h"
#include "spawnshell.h"
#include "zonemgr.h"

namespace {
inline QString latin1(const rust::String& s)
{
    return QString::fromLatin1(s.data(), int(s.size()));
}

// Resolve an EQ Legends STANCE ability id to its display name. The id is a
// stable client enum (eqgame.exe GetAbilityName switch); the OPCODE id is
// patch-volatile and lives in the toml. Unknown ids fall through to the caller.
QString stanceName(uint32_t id)
{
    switch (id) {
    case 117: return QStringLiteral("Offense");
    case 118: return QStringLiteral("Defense");
    case 119: return QStringLiteral("Evasive");
    case 120: return QStringLiteral("Balanced");
    case 121: return QStringLiteral("Mage Hunter");
    case 122: return QStringLiteral("Striker");
    case 123: return QStringLiteral("Berserker");
    case 124: return QStringLiteral("Ranged");
    case 135: return QStringLiteral("Channeler");
    default:  return QString();
    }
}

// Resolve an EQ Legends INVOCATION ability id to its display name (same stable
// eqgame.exe GetAbilityName enum). Unknown ids fall through to the caller.
QString invocationName(uint32_t id)
{
    switch (id) {
    case 125: return QStringLiteral("Recover");
    case 126: return QStringLiteral("Empower");
    case 127: return QStringLiteral("Inversion");
    case 128: return QStringLiteral("Spell Blade");
    case 129: return QStringLiteral("Over Channel");
    case 130: return QStringLiteral("Inviolable");
    case 131: return QStringLiteral("Divine");
    case 132: return QStringLiteral("Chained");
    case 133: return QStringLiteral("Arcane Mastery");
    case 134: return QStringLiteral("Unyielding");
    default:  return QString();
    }
}
}

EqlDispatch::EqlDispatch(ZoneMgr* zoneMgr, SpawnShell* spawnShell, Player* player)
    : m_zoneMgr(zoneMgr)
    , m_spawnShell(spawnShell)
    , m_player(player)
{
}

void EqlDispatch::profile(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_PlayerProfile S>C: identity header (race/class/level) + the character
    // NAME, located by the Rust parser's absolute anchor-scan (the u32 64 + name
    // + u32 32 + surname block signature — robust vs the old fixed deep offset,
    // empty only when the block can't be found). The parser also recovers the
    // surname + current zone/position/guild, but those stay unused here: the
    // current zone comes from OP_NewZone (newZone() below) and position from
    // OP_ClientUpdate, both authoritative. Nothing on the eql zone path fires a
    // reset, so what we set here survives the later OP_NewZone.
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_player_profile(
        rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    // Name first: setPlayerName only stores it (+ signals the box picker); the
    // setIdentity() below then emits changeItem(tSpawnChangedALL) carrying the
    // new name. An empty name (anchor block not found) leaves box-naming to
    // own-spawn adoption (SpawnShell::playerChangedID), the prior source.
    QString name = latin1(out.name);
    if (!name.isEmpty())
        m_player->setPlayerName(name);
    m_player->setIdentity((uint16_t)out.race, (uint8_t)out.class_, out.level);
    m_player->setClassMask(out.class_mask);   // EQL multiclass (bit N = class N)
}

void EqlDispatch::expUpdate(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_ExpUpdate (0x6801, 16B expUpdateStruct): the regular exp bar. eql sends
    // NO discrete level-up packet — confirmed exhaustively from the multi-ding
    // `eqlegends-levelup.vpk` capture (2026-07-10): across all 228 zone opcodes,
    // no opcode fires only at the dings, and no per-kill opcode / SpawnAppearance
    // / profile field carries the level increment (full evidence in
    // OPCODES_LEGENDS.md). The client itself infers a ding from the exp bar:
    // regular exp climbs monotonically within a level and only ever resets (wraps
    // to a low value) at a level-up. eql has NO death XP penalty (per-server
    // design, per user), so a DECREASE is unambiguously a ding — no need to
    // distinguish it from a death dip. Seed the level from the profile
    // (setIdentity) and bump it by one on each wrap.
    if (dir != DIR_Server)
        return;

    auto out = seq::rust::decode_exp_update(
        rust::Slice<const uint8_t>{data, len});
    if (out.ok)
    {
        if (m_lastExp >= 0 && out.exp < (uint32_t)m_lastExp)
            m_player->applyLevel((uint8_t)(m_player->level() + 1));
        m_lastExp = (int64_t)out.exp;
    }

    // Preserve the existing exp-bar behavior (exp log + web exp bar).
    m_player->updateExp(data);
}

void EqlDispatch::playerUpdateSelf(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_ClientUpdate (0x5188) C>S 38B self-position. Re-cracked 2026-07-14 vs a
    // /loc ground-truth capture: IEEE floats gameX@14 / gameY@26 / gameZ@10 +
    // the 13-bit heading@18 bit-8 (re-cracked 2026-07-15 vs a stationary 360-spin;
    // the earlier "low 12b" read was wrong). Unlike the pre-07/14 42B form it carries NO spawnId,
    // so this can no longer adopt/re-adopt the self-id — that comes from
    // SpawnShell::zoneEntry name-match (fires on real zone-ins / EnterWorld
    // re-entry). Consequence: eql's in-zone death respawn (which sends no self
    // OP_ZoneEntry) has no self-id recovery source post-patch — a known gap (the
    // death()/enterWorld() severs below still run; m_awaitingRespawnFromId is now
    // dormant). A C>S packet is by definition the local player, so once we hold a
    // self-id we apply straight to m_player. See OPCODES_LEGENDS.md.
    if (dir != DIR_Client)
        return;
    auto out = seq::rust::decode_player_self_pos(
        rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    if (m_player->id() == 0)
        return;   // self-id not adopted yet (awaiting zoneEntry name-match)

    // Deltas aren't located in the 38B form yet (parser surfaces 0), so the speed
    // indicator reads 0; position + the 13-bit heading are authoritative on every
    // packet (turning included — see player_self_pos.rs).
    m_player->applySelfPosition(int16_t(out.x), int16_t(out.y), int16_t(out.z),
                                0, 0, 0, out.heading, 0.0f);
}

void EqlDispatch::death(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_Death (0x66cb) S>C, 40B newCorpseStruct (victim = spawn_id, killer_id).
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_death(rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;

    if (m_player && out.spawn_id == (uint32_t)m_player->id())
    {
        // The LOCAL player died. On EQL a death respawns you IN-ZONE with a
        // BRAND-NEW self-spawn id: there is no SessionDisconnect (unlike a zone
        // change, which resets the self-id to 0 and self-heals) and no
        // player-reinit OP_ZoneEntry (the classic charProfile re-entry). The
        // respawned PC is simply re-broadcast as an ordinary OP_ZoneEntry NewSpawn
        // and the C>S self-pos stream switches to the new id (confirmed on
        // eqlegends-corpsepin: self-id 12636 -> 12913, no id-0 dip). The Live
        // death chain (killSpawn -> corpse-in-place, then respawnFromHover +
        // OP_ZoneEntry re-init) never completes here, so routing the player's own
        // death to killSpawn pins the PC record to the death spot as a renamed
        // corpse forever — the marker never follows the respawn (the reported
        // bug). Instead sever the player from its id so the next self-pos
        // re-adopts the respawn id (playerUpdateSelf's id()==0 path) and
        // playerChangedID re-adopts the respawn spawn's name; stash the dead id so
        // a trailing self-pos for the old body can't re-pin us. The server's own
        // OP_RemoveSpawn/OP_DeleteSpawn for the old id clears the stale body, and
        // EQL broadcasts no lingering player-corpse spawn, so we fabricate none.
        m_awaitingRespawnFromId = out.spawn_id;
        m_player->setID(0);
        return;
    }

    // A mob (or any non-player spawn) died: unchanged shared Live corpse path
    // (marks the spawn a corpse, sets last-kill on our killing blow, emits
    // SpawnKilled). It re-decodes the same bytes — cheap, and keeps one owner of
    // the corpse logic.
    m_spawnShell->killSpawn(data);
}

void EqlDispatch::enterWorld(const uint8_t*, size_t, uint8_t dir)
{
    // OP_EnterWorld (0x26bf) C>S, 72B: the client entering the world with its
    // character. It fires at login and AGAIN on every in-place session re-entry
    // — a private instance, or any zone that reuses the world socket. Because the
    // world socket is reused, BoxRegistry keeps the SAME box, so no active-box
    // roll re-snapshots the client, and zoneResolved is deliberately non-clearing
    // (the first zone-in's name arrives after its bulk). So on a re-entry the box
    // still holds the OLD zone's spawns and our old self-id, and the instance can
    // share the old zone's short name (making its zone_changed a client no-op).
    //
    // Reset here — BEFORE the instance's OP_ZoneEntry bulk repopulates — and drop
    // the self-id so playerUpdateSelf / own-spawn adoption re-adopts the instance's
    // NEW one; stash the old id so a trailing old-session self-pos can't re-pin us
    // (mirrors death()). That re-adoption calls Player::setPlayerID, whose
    // changedID signal already drives SessionAdapter::onPlayerIdChanged -> a fresh
    // Snapshot, so the web re-primes with the instance's spawns + new self-id with
    // no extra plumbing here. Gate on an established session so the initial login
    // (whose OP_EnterWorld precedes any zone) does not reset.
    if (dir != DIR_Client)
        return;
    if (!m_sessionEstablished || !m_player)
        return;
    seqInfo("EQL: session re-entry (OP_EnterWorld) — resetting box, self-id %d -> 0",
            m_player->id());
    const uint32_t oldId = (uint32_t)m_player->id();
    m_spawnShell->clear();
    m_player->setID(0);
    m_awaitingRespawnFromId = oldId;
}

void EqlDispatch::playerUpdateOther(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_ClientUpdate (0x5188) S>C, 24B: the position broadcast for spawns OTHER
    // than the local player (players + some NPCs). Same size as Live's 24B
    // playerSpawnPosStruct — eql packs each coord in the LOW 19 bits of a u32
    // (×8 fixed-point), decoded by this backend's own parse_player_spawn_pos.
    // The parser surfaces raw 19-bit values; apply >> 3 here for the 1/8-unit →
    // integer conversion (as Live's SpawnShell::playerUpdate does). Position
    // only → the neutral SpawnShell::moveSpawn, the same path OP_MobUpdate uses;
    // moveSpawn takes no heading, so the decoded heading is unused for now.
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_player_spawn_pos(
        rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    // The player's own position is authoritative from OP_ClientUpdate C>S
    // (playerUpdateSelf); never let the broadcast move the PC record.
    if (m_player && out.spawn_id == (uint16_t)m_player->id())
        return;
    m_spawnShell->moveSpawn(out.spawn_id,
                            int16_t(out.x >> 3), int16_t(out.y >> 3),
                            int16_t(out.z >> 3));
}

void EqlDispatch::newZone(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_NewZone (0x1dbf) S>C, once per zone-in: the authoritative current zone,
    // carried as packed null-terminated short + long name text. Drives the map /
    // filter / web via ZoneMgr::setZoneByName -> zoneResolved (which does NOT
    // clear spawns or reset the player — see zonemgr.cpp). This fires AFTER the
    // bulk spawn list + the profile, so it must not reset the box.
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_new_zone(rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    m_zoneMgr->setZoneByName(latin1(out.short_name), latin1(out.long_name));

    // A zone has now resolved: any LATER OP_EnterWorld is a genuine re-entry
    // (see enterWorld()).
    m_sessionEstablished = true;
}

bool EqlDispatch::consumeSelfSpawn(const QString& name, uint16_t id)
{
    // eql sends the local player's OWN ZoneEntry TWICE per zone — a live copy that
    // moves and a static phantom the client hides — each under a fresh per-zone id
    // (the phantom's is a few higher). Neither should become a spawns[] entry: the
    // self is owned by Player, whose identity comes from OP_PlayerProfile and whose
    // position/heading come from the C>S self-report. We adopt the FIRST self-named
    // record of a zone (== the live copy: it's first on the wire in every capture,
    // /loc-confirmed to be the one that moves) and swallow the rest.
    if (id == 0 || name.isEmpty() || name != m_player->realName())
        return false;   // not us — let it through to the spawn list

    // Adopt when we have no self-id yet (fresh login / post-death sever) or the id
    // jumped zones (a stale id is hundreds/thousands off — the live+phantom pair of
    // one zone stays within a few ids). Re-homing runs playerChangedID, which drops
    // any stray copy from the list and re-snapshots the client. A later record
    // within kSameBatch of the current self-id is the phantom twin — just swallow it.
    constexpr int kSameBatch = 16;
    if (m_player->id() == 0 ||
        std::abs(static_cast<int>(id) - static_cast<int>(m_player->id())) > kSameBatch)
        m_player->setPlayerID(id);
    else if (id != m_player->id())
        // The near-id twin. eql keys the self's MOVEMENT to the adopted id but its
        // PROFILE/BUFF data (OP_BuffList, profile) to this second id — remember it
        // so that character data routes to the player instead of a dropped spawn.
        m_player->setAltId(id);
    return true;
}

void EqlDispatch::spawn(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_ZoneEntry S>C: null-terminated name + variable NPC block, decoded by
    // the full front walk in parse_zone_spawn (name, lastName, race/class/deity/
    // guild, real npc flag, decoded position, HP, h2048 heading). title/suffix
    // are decoded but have no daemon/proto home yet; the heading is the starting
    // facing — OP_MobUpdate / OP_NpcMoveUpdate refine it on movement.
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_spawn(rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    const QString name = latin1(out.name);
    // Keep the local player's own ZoneEntry (and its phantom twin) out of the
    // spawn list; adopt/re-home the self-id from it instead.
    if (consumeSelfSpawn(name, (uint16_t)out.spawn_id))
        return;
    m_spawnShell->upsertSpawn((uint16_t)out.spawn_id, name, latin1(out.last_name),
                              out.x, out.y, out.z, out.heading,
                              out.level, out.cur_hp, out.max_hp,
                              (uint16_t)out.race, (uint8_t)out.class_, (uint16_t)out.deity,
                              (uint16_t)out.guild_id, out.npc, out.class_mask);
}

void EqlDispatch::loadoutSwap(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_LoadoutSwap (0x7477) S>C: a player switched loadouts (the Legends
    // multiclass class/level change). EQL sends no OP_PlayerProfile on a swap,
    // so this is the only source for the new identity. The self variant carries
    // a serialized inventory tail; the server also broadcasts a short ~490B
    // variant for ANY nearby player's swap. Both share the header + ZoneEntry-
    // format record the Rust decoder reads (see loadout_swap.rs).
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_loadout_swap(rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    if (m_player && out.spawn_id == (uint32_t)m_player->id())
    {
        // Our own swap: refresh identity. Race is unchanged on a swap but the
        // record carries it correctly, so the existing setIdentity primitive is
        // safe (it drives class + level to the frontend via tSpawnChangedALL).
        m_player->setIdentity((uint16_t)out.race, (uint8_t)out.class_, out.level);
        return;
    }
    // A nearby player's swap: update the tracked spawn's level + class in place
    // (no position/HP — the spawn list / con display is the consumer), so it
    // doesn't read stale until that spawn's next regular OP_ZoneEntry.
    m_spawnShell->updateSpawnIdentity((uint16_t)out.spawn_id, out.level, (uint8_t)out.class_);
}

void EqlDispatch::stance(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_Stance (0x0fab) S>C echo (authoritative): the player's active stance
    // changed. 4B {u32 abilityId}; resolve the id to a display name and store it
    // on Player -> PlayerStats.stance. Unknown id → "#<id>" so it stays visible.
    if (dir != DIR_Server || !m_player)
        return;
    auto out = seq::rust::decode_activate_ability(
        rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    QString name = stanceName(out.ability_id);
    if (name.isEmpty())
        name = QStringLiteral("#%1").arg(out.ability_id);
    m_player->setStance(name);
}

void EqlDispatch::invocation(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_Invocation (0x3b12) S>C echo (authoritative): the player's active
    // invocation changed. Same 4B {u32 abilityId} as OP_Stance; resolve via the
    // invocation table and store on Player -> PlayerStats.invocation.
    if (dir != DIR_Server || !m_player)
        return;
    auto out = seq::rust::decode_activate_ability(
        rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    QString name = invocationName(out.ability_id);
    if (name.isEmpty())
        name = QStringLiteral("#%1").arg(out.ability_id);
    m_player->setInvocation(name);
}

void EqlDispatch::mobUpdate(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_MobUpdate S>C, 14B (parser validates len).
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_mob_update(rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    // The Legends wire is byte-identical to Live's spawnPositionUpdate (packed
    // 19-bit fixed-point coords), so the shared Live parser decodes the full
    // coordinate range directly — no 16-bit wrap, no phase-unwrap needed.
    m_spawnShell->moveSpawn(out.spawn_id,
                            int16_t(out.x), int16_t(out.y), int16_t(out.z));
}

void EqlDispatch::statSync(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_HPUpdate (0x2735) S>C multiplexed stat-sync channel. The Rust parser
    // (parse_stat_sync) decodes u32 spawnId + u8 flags + per-stat payload —
    // wide {i64 cur,max} pairs or narrow u8 percents — for HP/mana/endurance,
    // with a structural size canary. Passing the REAL length matters: the size
    // varies per packet (6/7/21/37/53B, optional +4 tail).
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_stat_sync(rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;

    // HP. The Legends server sends the player their own real cur/max via the
    // wide form and other spawns a narrow percent (max=100). The daemon surfaces
    // the player's vitals through the Player object (→ player_stats), NOT through
    // m_spawns — the player is never a m_spawns entry here (unlike legacy SEQ,
    // where the f-patch routes to m_spawns.value(spawnId)). So split by id: the
    // player's HP → Player::setHealth; every other spawn's HP → updateSpawnHP.
    // Guard max>0 so a wide zero-max packet can't blank an HP bar.
    if (out.has_hp && out.hp_max > 0)
    {
        if (m_player && out.spawn_id == (uint32_t)m_player->id())
            m_player->setHealth((uint32_t)out.hp_cur, (uint32_t)out.hp_max);
        else
            m_spawnShell->updateSpawnHP((uint16_t)out.spawn_id,
                                        (int32_t)out.hp_cur, (int32_t)out.hp_max);
    }

    // Mana → the player only, and only from the wide (real cur/max) form; the
    // narrow percent form has no useful max. Plays the role of Live's absent
    // OP_ManaChange on the Legends wire.
    if (out.has_mana && out.wide && m_player &&
        out.spawn_id == (uint32_t)m_player->id())
        m_player->setMana((uint32_t)out.mana_cur, (uint32_t)out.mana_max);

    // Endurance → the player only, wide form. Legends drives endurance through
    // this channel (the standalone OP_EndUpdate opcode id is unknown/ffff, so it
    // never fires), and it moves constantly as skills/abilities consume it —
    // surface it as the stock End bar (player_stats.endurance_cur/max).
    if (out.has_end && out.wide && m_player &&
        out.spawn_id == (uint32_t)m_player->id())
        m_player->setEndurance((uint32_t)out.end_cur, (uint32_t)out.end_max);
}
