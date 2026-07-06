/*
 * everquest_legends.h
 *
 * EverQuest Legends wire-format structs.
 *
 * Legends shares the SOE low-level stream protocol with Live EQ (SessionRequest
 * "Everquest" magic, zlib, Combined/Ack/Oversized framing — the daemon deframes
 * it unchanged), but the APPLICATION opcode table and struct layouts are fully
 * remapped. Keep Legends-specific structs here, out of everquest.h, so the two
 * wire formats don't intermingle.
 *
 * Opcode map + per-field evidence: OPCODES_LEGENDS.md (legends-client branch).
 *
 * This branch is Legends-only: no Live/Legends runtime toggle, mirroring the
 * test-client branch's Test-only convention.
 */

#ifndef EVERQUEST_LEGENDS_H
#define EVERQUEST_LEGENDS_H

#include <cstdint>

#pragma pack(1)

/*
** Player Client Update — OP_ClientUpdate (0x0b03), C>S, 42 bytes.
**
** The player's own position/heading, sent by the client. Unlike Live's
** bit-packed playerSelfPosStruct, Legends carries position and deltas as IEEE
** floats. Axis names + heading zero-point confirmed against /loc ground truth
** (see OPCODES_LEGENDS.md): x=east-west, y=north-south, z=height; heading is an
** 11-bit field (0-2047 = full turn, 0 = North) packed into `packed`.
**
** Field names (spawnId/x/y/z/deltaX/deltaY/deltaZ) match Player::playerUpdateSelf
** so the handler slots in with minimal change.
*/
struct legendsPlayerSelfPos
{
/*0000*/ uint16_t sequence;        // monotonic per-update counter
/*0002*/ uint16_t spawnId;         // player's entity/char id
/*0004*/ uint16_t unknown0004;     // 0
/*0006*/ uint8_t  unknown0006[4];  // near-constant (0xa3ed at +2); role TBD
/*0010*/ float    deltaY;          // north-south velocity
/*0014*/ float    deltaZ;          // vertical velocity
/*0018*/ uint16_t unknown0018;     // 11-bit angle (0-2047), NOT heading; role TBD
/*0020*/ uint16_t unknown0020;     // 0
/*0022*/ float    x;               // east-west position   [CONFIRMED /loc]
/*0026*/ uint32_t packed;          // heading = (packed>>10)&0x7FF (11-bit, 0=N);
                                   //   remaining bits: animation / deltaHeading (TBD)
/*0030*/ float    deltaX;          // east-west velocity
/*0034*/ float    z;               // height position      [CONFIRMED /loc]
/*0038*/ float    y;               // north-south position [CONFIRMED /loc]
/*0042*/
};

/*
** Zone Spawn — OP_ZoneSpawns (0x7475), S>C. One spawn per payload: a
** null-terminated ASCII name followed by this fixed 326-byte block (NPC form;
** players use a 470-byte variant not yet decoded). Position is int16 with a
** MIXED scale — Y unscaled, X/Z at 1/8 unit — confirmed against two stationary
** NPCs /loc'd point-blank (see OPCODES_LEGENDS.md). The layout shuffles per
** patch; re-derive from captures, don't memorize. Odd offsets rely on pack(1).
*/
struct legendsSpawnStruct
{
/*0000*/ uint32_t spawnId;          // entity id (low 16 bits used)
/*0004*/ uint8_t  level;            // mob level
/*0005*/ uint8_t  unknown0005[21];
/*0026*/ uint16_t race;             // race / 3D model
/*0028*/ uint8_t  unknown0028[12];
/*0040*/ uint8_t  bodyType;         // undead=3, humanoid=1, animal=21
/*0041*/ uint8_t  unknown0041[3];
/*0044*/ uint8_t  curHpPct;         // current HP percent (100 = full)
/*0045*/ uint8_t  maxHpPct;         // always 100
/*0046*/ uint8_t  unknown0046[181];
/*0227*/ int16_t  z8;               // Z * 8  (divide by 8 for world units)
/*0229*/ uint8_t  unknown0229[2];
/*0231*/ int16_t  x8;               // X * 8
/*0233*/ uint8_t  unknown0233[8];
/*0241*/ int16_t  y;                // Y (unscaled world units)
/*0243*/ uint8_t  unknown0243[83];
/*0326*/
};

/*
** Mob Position Update — OP_MobUpdate (0x061b), S>C, 14 bytes. Sent frequently
** as an NPC moves. Position is int16 fixed-point with a DIFFERENT per-axis scale
** than legendsSpawnStruct / legendsPlayerSelfPos: Y*8, Z*64, X unscaled. This is
** genuine fixed-point, not a mis-read offset — the stored int16 IS coord*scale
** (e.g. 7413 == 926.625 * 8, a clean 1/8-unit value with no plain int16 == 927
** anywhere in the packet). Re-derive scales per opcode.
*/
struct legendsMobUpdateStruct
{
/*0000*/ uint32_t spawnId;         // entity id (low 16 bits used)
/*0004*/ int16_t  y8;              // Y * 8
/*0006*/ int16_t  z64;             // Z * 64
/*0008*/ uint8_t  unknown0008[2];  // heading / delta (TBD)
/*0010*/ int16_t  x;               // X (unscaled)
/*0012*/ uint8_t  unknown0012[2];  // heading / delta (TBD)
/*0014*/
};

/*
** Character Profile header — OP_PlayerProfile (0x5207), S>C, ~38 KB, sent once
** at zone-in. Only the header (race/class/level) is decoded here; the full
** struct also embeds a bind-point array (@39), inventory, spells, and the char
** name (deep, ~@35551). Field ids reuse classic EQ values (race 6 = Dark Elf,
** class 5 = Shadowknight, class 10 = Shaman). Legends' multi-class 2nd slot is
** a u32 at +147 (not in this header). The CURRENT zone is NOT read from here
** (the @39 zoneId is the BIND zone) — use OP_NewZone (0x5ab6) for the map.
*/
struct legendsCharProfileHdr
{
/*0000*/ uint8_t  unknown0000[21];
/*0021*/ uint32_t race;          // 6 = Dark Elf
/*0025*/ uint32_t class1;        // primary class (5 = Shadowknight)
/*0029*/ uint32_t unknown0029;
/*0033*/ uint8_t  level;         // character level
/*0034*/
};

#pragma pack()

// Heading (0-2047, 0 = North) extracted from the packed word.
static inline uint16_t legendsHeading(const legendsPlayerSelfPos* p)
{
    return static_cast<uint16_t>((p->packed >> 10) & 0x7FF);
}

#endif // EVERQUEST_LEGENDS_H
