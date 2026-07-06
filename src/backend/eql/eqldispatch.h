/*
 * eqldispatch.h — EverQuest Legends packet dispatch adapter.
 *
 * Compiled ONLY for -DSEQ_TARGET=eql. Holds the Legends-specific handler slots
 * so they never touch the core manager classes (ZoneMgr / SpawnShell / Player):
 * a slot declared on a core manager would be ODR-used by its always-compiled
 * moc, forcing the Legends code to link into the live/test builds too. By
 * living in a backend-only QObject, EqlDispatch's slots + moc metadata exist
 * only in the eql binary.
 *
 * Each slot casts the Legends wire struct (everquest_legends.h) and drives the
 * core managers through their target-NEUTRAL public primitives (Player::
 * setIdentity/applySelfPosition, ZoneMgr::setZoneByName, SpawnShell::
 * upsertSpawn/moveSpawn) — those primitives own the private-state writes and
 * signal emissions the managers won't expose. No Legends type ever enters core.
 *
 * Wired by wire_eql.cpp via the normal EQPacketStream::connect2 string path,
 * so packetstream needs no changes.
 */
#ifndef SEQ_BACKEND_EQL_EQLDISPATCH_H
#define SEQ_BACKEND_EQL_EQLDISPATCH_H

#include <QObject>
#include <cstddef>
#include <cstdint>

class ZoneMgr;
class SpawnShell;
class Player;

class EqlDispatch : public QObject
{
    Q_OBJECT

public:
    EqlDispatch(ZoneMgr* zoneMgr, SpawnShell* spawnShell, Player* player,
                QObject* parent = nullptr);

public slots:
    // OP_PlayerProfile (0x5207) S>C: identity header (race/class/level).
    void legendsProfile(const uint8_t* data, size_t len, uint8_t dir);
    // OP_ClientUpdate (0x0b03) C>S: this player's float position + heading.
    void playerUpdateSelf(const uint8_t* data, size_t len, uint8_t dir);
    // OP_NewZone (0x5ab6) S>C: null-terminated short/long zone names.
    void legendsNewZone(const uint8_t* data, size_t len, uint8_t dir);
    // OP_ZoneSpawns (0x7475) S>C: null-terminated name + fixed spawn block.
    void legendsSpawn(const uint8_t* data, size_t len, uint8_t dir);
    // OP_MobUpdate (0x061b) S>C: per-mob position update.
    void legendsMobUpdate(const uint8_t* data, size_t len, uint8_t dir);

private:
    ZoneMgr*    m_zoneMgr;
    SpawnShell* m_spawnShell;
    Player*     m_player;
};

#endif // SEQ_BACKEND_EQL_EQLDISPATCH_H
