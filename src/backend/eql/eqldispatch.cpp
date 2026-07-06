/*
 * eqldispatch.cpp — EverQuest Legends packet dispatch adapter.
 *
 * The Legends decode bodies were ported from the legends-client branch, where
 * they lived directly on the core managers. Here the wire-struct casts +
 * per-axis fixed-point decode stay local, and the state mutation / signal
 * emission is delegated to the managers' neutral primitives. See eqldispatch.h.
 */
#include "eqldispatch.h"

#include <cmath>

#include <QString>

#include "everquest_legends.h"
#include "packetcommon.h"   // DIR_Client / DIR_Server
#include "player.h"
#include "spawnshell.h"
#include "zonemgr.h"

EqlDispatch::EqlDispatch(ZoneMgr* zoneMgr, SpawnShell* spawnShell, Player* player)
    : m_zoneMgr(zoneMgr)
    , m_spawnShell(spawnShell)
    , m_player(player)
{
}

void EqlDispatch::legendsProfile(const uint8_t* data, size_t len, uint8_t dir)
{
    if (dir != DIR_Server || len < sizeof(legendsCharProfileHdr))
        return;

    const legendsCharProfileHdr* p = (const legendsCharProfileHdr*)data;
    m_player->setIdentity((uint16_t)p->race, (uint8_t)p->class1, p->level);
}

void EqlDispatch::playerUpdateSelf(const uint8_t* data, size_t len, uint8_t dir)
{
    if (len < sizeof(legendsPlayerSelfPos))
        return;

    const legendsPlayerSelfPos* p = (const legendsPlayerSelfPos*)data;

    // Wired DIR_Client only, but keep the self-spawn guard from the branch.
    if (dir == DIR_Client && m_player->id() == 0)
        m_player->setPlayerID(p->spawnId);
    if (p->spawnId != m_player->id())
        return;   // controlling another spawn (Eye of Zomm / boat) — not mapped

    // legendsPlayerSelfPos carries position + deltas as floats; heading is an
    // 11-bit field packed into `packed` (legendsHeading()). newSpeed uses the
    // float deltas before the int16 cast so sub-unit velocity survives.
    const uint16_t heading = legendsHeading(p);
    const float speed = std::hypot(std::hypot(p->deltaX * 80.0f, p->deltaY * 80.0f),
                                   p->deltaZ * 80.0f) / 119.46664f;

    m_player->applySelfPosition(int16_t(p->x), int16_t(p->y), int16_t(p->z),
                                int16_t(p->deltaX), int16_t(p->deltaY), int16_t(p->deltaZ),
                                heading, speed);
}

void EqlDispatch::legendsNewZone(const uint8_t* data, size_t len, uint8_t dir)
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

void EqlDispatch::legendsSpawn(const uint8_t* data, size_t len, uint8_t dir)
{
    if (dir != DIR_Server)
        return;

    // null-terminated name at the front, then the fixed block.
    size_t nameLen = 0;
    while (nameLen < len && data[nameLen] != 0)
        nameLen++;
    if (nameLen == 0 || nameLen >= len)          // empty name or no terminator
        return;

    const size_t blockLen = len - nameLen - 1;
    if (blockLen != sizeof(legendsSpawnStruct))  // scaffold: NPC form only
        return;

    const legendsSpawnStruct* s = (const legendsSpawnStruct*)(data + nameLen + 1);
    const uint16_t id = (uint16_t)s->spawnId;

    // position: X/Z are 1/8-unit fixed-point, Y unscaled (everquest_legends.h).
    const int16_t x = (int16_t)(s->x8 / 8);
    const int16_t y = s->y;
    const int16_t z = (int16_t)(s->z8 / 8);

    m_spawnShell->upsertSpawn(id, QString::fromLatin1((const char*)data, int(nameLen)),
                              x, y, z, s->level, s->curHpPct, s->maxHpPct);
}

void EqlDispatch::legendsMobUpdate(const uint8_t* data, size_t len, uint8_t dir)
{
    if (dir != DIR_Server || len < sizeof(legendsMobUpdateStruct))
        return;

    const legendsMobUpdateStruct* m = (const legendsMobUpdateStruct*)data;

    // per-axis fixed point: X unscaled, Y*8, Z*64 (everquest_legends.h).
    m_spawnShell->moveSpawn((uint16_t)m->spawnId,
                            m->x, (int16_t)(m->y8 / 8), (int16_t)(m->z64 / 64));
}
