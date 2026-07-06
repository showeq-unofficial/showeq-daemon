/*
 * eqldispatch.cpp — EverQuest Legends packet dispatch adapter.
 *
 * The Legends decode bodies were ported from the legends-client branch. eql owns
 * no wire-struct types: the Legends wire is read here by offset + per-axis scale
 * (via the rd_* helpers below) and the state mutation / signal emission is
 * delegated to the core managers' target-NEUTRAL primitives. So the frontend
 * (SpawnShell / Player / ZoneMgr) keeps only Live's struct set; nothing named
 * `legends*` exists anywhere. Field offsets/scales are /loc-confirmed — see
 * OPCODES_LEGENDS.md for the per-field evidence. Layout shuffles per patch;
 * re-derive from captures, don't memorize.
 */
#include "eqldispatch.h"

#include <cmath>
#include <cstring>

#include <QString>

#include "packetcommon.h"   // DIR_Client / DIR_Server
#include "player.h"
#include "spawnshell.h"
#include "zonemgr.h"

// Little-endian scalar reads at a byte offset. memcpy (not a cast) because the
// Legends offsets are unaligned (e.g. 231, 241) and #pragma-pack casts there are
// UB; the compiler lowers these to a single load.
namespace {
inline uint16_t rd_u16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }
inline uint32_t rd_u32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
inline int16_t  rd_i16(const uint8_t* p) { int16_t  v; std::memcpy(&v, p, 2); return v; }
inline float    rd_f32(const uint8_t* p) { float    v; std::memcpy(&v, p, 4); return v; }
}

EqlDispatch::EqlDispatch(ZoneMgr* zoneMgr, SpawnShell* spawnShell, Player* player)
    : m_zoneMgr(zoneMgr)
    , m_spawnShell(spawnShell)
    , m_player(player)
{
}

void EqlDispatch::profile(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_PlayerProfile (0x5207) S>C: ~38KB profile; only the identity header is
    // decoded. Classic EQ ids: race u32 @21 (6=Dark Elf), class u32 @25
    // (5=Shadowknight), level u8 @33.
    if (dir != DIR_Server || len < 34)
        return;

    const uint16_t race  = (uint16_t)rd_u32(data + 21);
    const uint8_t  class1 = (uint8_t)rd_u32(data + 25);
    const uint8_t  level = data[33];
    m_player->setIdentity(race, class1, level);
}

void EqlDispatch::playerUpdateSelf(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_ClientUpdate (0x0b03) C>S, 42B: position + deltas as IEEE floats (unlike
    // Live's bit-packed struct). Offsets /loc-confirmed: spawnId u16 @2;
    // x f32 @22 (E/W), z f32 @34 (height), y f32 @38 (N/S); deltaX f32 @30,
    // deltaY f32 @10, deltaZ f32 @14; heading = 11-bit field (0=North) in the
    // u32 @26: (packed>>10)&0x7FF.
    if (len != 42)   // fixed size (was SZC_Match); wire is now uint8_t/SZC_None
        return;

    const uint16_t spawnId = rd_u16(data + 2);

    // Wired DIR_Client only, but keep the self-spawn guard from the branch.
    if (dir == DIR_Client && m_player->id() == 0)
        m_player->setPlayerID(spawnId);
    if (spawnId != m_player->id())
        return;   // controlling another spawn (Eye of Zomm / boat) — not mapped

    const float x  = rd_f32(data + 22);
    const float z  = rd_f32(data + 34);
    const float y  = rd_f32(data + 38);
    const float dX = rd_f32(data + 30);
    const float dY = rd_f32(data + 10);
    const float dZ = rd_f32(data + 14);
    const uint16_t heading = (uint16_t)((rd_u32(data + 26) >> 10) & 0x7FF);

    // newSpeed uses the float deltas before the int16 cast so sub-unit velocity
    // survives.
    const float speed = std::hypot(std::hypot(dX * 80.0f, dY * 80.0f),
                                   dZ * 80.0f) / 119.46664f;

    m_player->applySelfPosition(int16_t(x), int16_t(y), int16_t(z),
                                int16_t(dX), int16_t(dY), int16_t(dZ),
                                heading, speed);
}

void EqlDispatch::newZone(const uint8_t* data, size_t len, uint8_t dir)
{
    if (dir != DIR_Server || len < 2)
        return;

    // payload: null-terminated short name, then long name, then zonefile.
    auto readStr = [&](size_t start) -> QString {
        if (start >= len) return QString();
        size_t e = start;
        while (e < len && data[e] != 0) e++;
        return QString::fromLatin1((const char*)data + start, int(e - start));
    };

    const QString shortName = readStr(0);
    if (shortName.isEmpty())
        return;
    const QString longName = readStr(shortName.length() + 1);

    m_zoneMgr->setZoneByName(shortName, longName);
}

void EqlDispatch::spawn(const uint8_t* data, size_t len, uint8_t dir)
{
    if (dir != DIR_Server)
        return;

    // null-terminated name at the front, then a fixed 326-byte NPC block.
    size_t nameLen = 0;
    while (nameLen < len && data[nameLen] != 0)
        nameLen++;
    if (nameLen == 0 || nameLen >= len)          // empty name or no terminator
        return;

    const uint8_t* s = data + nameLen + 1;
    const size_t blockLen = len - nameLen - 1;
    if (blockLen != 326)                          // scaffold: NPC form only
        return;

    // Block offsets /loc-confirmed: spawnId u32 @0, level u8 @4, curHpPct u8 @44,
    // maxHpPct u8 @45; position int16 mixed-scale — X/Z at 1/8 unit (x8 @231,
    // z8 @227), Y unscaled (y @241).
    const uint16_t id = (uint16_t)rd_u32(s + 0);
    const int16_t  x  = (int16_t)(rd_i16(s + 231) / 8);
    const int16_t  y  = rd_i16(s + 241);
    const int16_t  z  = (int16_t)(rd_i16(s + 227) / 8);
    const uint8_t  level    = s[4];
    const uint8_t  curHpPct = s[44];
    const uint8_t  maxHpPct = s[45];

    m_spawnShell->upsertSpawn(id, QString::fromLatin1((const char*)data, int(nameLen)),
                              x, y, z, level, curHpPct, maxHpPct);
}

void EqlDispatch::mobUpdate(const uint8_t* data, size_t len, uint8_t dir)
{
    // OP_MobUpdate (0x061b) S>C, 14B. Offsets: spawnId u32 @0; position int16
    // per-axis fixed-point — X unscaled (@10), Y*8 (@4), Z*64 (@6).
    if (dir != DIR_Server || len != 14)   // fixed size (was SZC_Match)
        return;

    m_spawnShell->moveSpawn((uint16_t)rd_u32(data + 0),
                            rd_i16(data + 10),
                            (int16_t)(rd_i16(data + 4) / 8),
                            (int16_t)(rd_i16(data + 6) / 64));
}
