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
    // new name. An empty name (offset drifted) leaves box-naming to own-spawn
    // adoption (SpawnShell::playerChangedID), the prior source.
    QString name = latin1(out.name);
    if (!name.isEmpty())
        m_player->setPlayerName(name);
    m_player->setIdentity((uint16_t)out.race, (uint8_t)out.class_, out.level);
}

void EqlDispatch::playerUpdateSelf(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_ClientUpdate C>S, 42B: IEEE-float position + deltas (parser validates len).
    auto out = seq::rust::decode_player_self_pos(
        rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;

    // Wired DIR_Client only, but keep the self-spawn guard from the branch.
    if (dir == DIR_Client && m_player->id() == 0)
        m_player->setPlayerID(out.spawn_id);
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
    // OP_ZoneSpawns S>C: null-terminated name + variable NPC block, decoded by
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
