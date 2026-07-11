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

#include <cmath>

#include <QString>

#include "seq-bridge-cxx/lib.h"

#include "packetcommon.h"   // DIR_Client / DIR_Server
#include "player.h"
#include "spawnshell.h"
#include "zonemgr.h"

namespace {
inline QString latin1(const rust::String& s)
{
    return QString::fromLatin1(s.data(), int(s.size()));
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
    // OP_ClientUpdate C>S, 42B: IEEE-float position + deltas (parser validates len).
    auto out = seq::rust::decode_player_self_pos(
        rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;

    // Adopt the self-spawn id when we don't have one yet: at zone-in (id 0), and
    // after the player's own death severed it — an in-zone respawn brings a
    // BRAND-NEW self-id (see death() below). Never adopt a transient spawn_id==0,
    // and never re-adopt the dead id off a trailing corpse-side update; wait for
    // the genuine respawn id.
    if (dir == DIR_Client && m_player->id() == 0 &&
        out.spawn_id != 0 && out.spawn_id != m_awaitingRespawnFromId)
    {
        m_player->setPlayerID(out.spawn_id);
        m_awaitingRespawnFromId = 0;
    }
    if (out.spawn_id != m_player->id())
        return;   // controlling another spawn (Eye of Zomm / boat) — not mapped

    // newSpeed uses the float deltas before the int16 cast so sub-unit velocity
    // survives.
    const float speed = std::hypot(std::hypot(out.delta_x * 80.0f,
                                              out.delta_y * 80.0f),
                                   out.delta_z * 80.0f) / 119.46664f;

    m_player->applySelfPosition(int16_t(out.x), int16_t(out.y), int16_t(out.z),
                                int16_t(out.delta_x), int16_t(out.delta_y),
                                int16_t(out.delta_z),
                                out.heading, speed);
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

void EqlDispatch::playerUpdateOther(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_ClientUpdate S>C, 28B: the position broadcast for spawns OTHER than the
    // local player (players + some NPCs). Distinct from Live's 24B
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
    m_spawnShell->upsertSpawn((uint16_t)out.spawn_id, latin1(out.name), latin1(out.last_name),
                              out.x, out.y, out.z, out.heading,
                              out.level, out.cur_hp, out.max_hp,
                              (uint16_t)out.race, (uint8_t)out.class_, (uint16_t)out.deity,
                              (uint16_t)out.guild_id, out.npc);
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
