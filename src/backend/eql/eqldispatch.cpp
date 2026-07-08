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

// Bring `value` within half a wrap-period of `reference` by adding/removing
// whole periods — undoes a modular (16-bit) coordinate wrap using positional
// continuity. See EqlDispatch::mobUpdate.
inline int unwrapToward(int value, int reference, int period)
{
    const int half = period / 2;
    while (value - reference > half) value -= period;
    while (reference - value > half) value += period;
    return value;
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
    // OP_PlayerProfile S>C: identity header only (race/class/level).
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_player_profile(
        rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    // Resolve the CURRENT zone FIRST (classic id @36211; 0x4bc8 is the BIND
    // zone, not current). A zone change fires ZoneMgr::zoneChanged ->
    // Player::zoneChanged() -> reset(), which wipes the player's identity — so
    // the zone MUST be applied before setIdentity or the identity is lost
    // (symptom: player shows default race1/class1/level1). Matches Live, which
    // establishes identity after the zone reset.
    if (out.zone_id != 0)
        m_zoneMgr->setZoneById(out.zone_id);
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
    // OP_NewZone S>C (post-2026-07-07): a numeric classic zone id, no name text.
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_new_zone(rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    // Resolve the zone id -> short/long name via zones.h.
    m_zoneMgr->setZoneById((uint16_t)out.zone_id);
}

void EqlDispatch::spawn(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_ZoneSpawns S>C: null-terminated name + fixed 326-byte NPC block.
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_spawn(rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;
    m_spawnShell->upsertSpawn((uint16_t)out.spawn_id, latin1(out.name),
                              out.x, out.y, out.z,
                              out.level, out.cur_hp, out.max_hp);
}

void EqlDispatch::mobUpdate(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_MobUpdate S>C, 14B (parser validates len).
    if (dir != DIR_Server)
        return;
    auto out = seq::rust::decode_mob_update(rust::Slice<const uint8_t>{data, len});
    if (!out.ok)
        return;

    // OP_MobUpdate encodes Y as signed 16-bit fixed-point (raw/8), so any
    // coordinate past ±4095 wraps by 65536/8 = 8192 game units — a far-north
    // mob snaps ~8192 units south. OP_NpcMoveUpdate carries the same position
    // at 19-bit precision (no wrap) and fires for every roaming mob, so once
    // it has anchored the spawn's real position we phase-unwrap each MobUpdate
    // Y toward that known value to undo the wrap (Z: raw/64 → 1024 period, same
    // treatment). X is unscaled i16 here and effectively never wraps in-zone.
    int uy = int(out.y);
    int uz = int(out.z);
    int16_t cx, cy, cz;
    if (m_spawnShell->spawnPos(out.spawn_id, cx, cy, cz)) {
        uy = unwrapToward(uy, cy, 8192);
        uz = unwrapToward(uz, cz, 1024);
    }
    m_spawnShell->moveSpawn(out.spawn_id,
                            int16_t(out.x), int16_t(uy), int16_t(uz));
}
