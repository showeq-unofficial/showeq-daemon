/*
 * eqldispatch.h — EverQuest Legends packet dispatch adapter.
 *
 * Compiled ONLY for -DSEQ_TARGET=eql. Holds the Legends-specific handlers so
 * they never touch the core manager classes (ZoneMgr / SpawnShell / Player).
 *
 * This is a plain (non-QObject) class: handlers are registered as typed
 * PacketHandlers via EQPacketStream::on() (see wire_eql.cpp), so they no longer
 * need to be Qt slots. That removes the old moc/ODR constraint that forced this
 * adapter to exist as a QObject at all — a slot on a core manager would have
 * been ODR-used by its always-compiled moc and dragged the Legends code into
 * the live/test link. With typed dispatch the handlers are just callables; this
 * adapter groups them and is owned via a shared_ptr captured by the wired
 * closures, so its lifetime tracks the box's stream dispatchers.
 *
 * Each method decodes the Legends wire bytes via the Rust `seq::rust::decode_*`
 * parsers (there is no `everquest_legends.h`; eql owns no C++ wire structs) and
 * drives the core managers through their target-NEUTRAL public primitives (Player::
 * setIdentity/applySelfPosition, ZoneMgr::setZoneByName, SpawnShell::
 * upsertSpawn/moveSpawn). No Legends type ever enters core.
 */
#ifndef SEQ_BACKEND_EQL_EQLDISPATCH_H
#define SEQ_BACKEND_EQL_EQLDISPATCH_H

#include <cstddef>
#include <cstdint>

class ZoneMgr;
class SpawnShell;
class Player;

class EqlDispatch
{
public:
    EqlDispatch(ZoneMgr* zoneMgr, SpawnShell* spawnShell, Player* player);

    // OP_PlayerProfile (0x62f0) S>C: identity header (race/class/level).
    void profile(const uint8_t* data, size_t len, uint8_t dir);
    // OP_ClientUpdate (0x7171) C>S: this player's float position + heading.
    void playerUpdateSelf(const uint8_t* data, size_t len, uint8_t dir);
    // OP_NewZone (0x1dbf) S>C: packed null-terminated short/long zone names.
    void newZone(const uint8_t* data, size_t len, uint8_t dir);
    // OP_ZoneEntry (0x4606) S>C: null-terminated name + fixed spawn block
    // (stock-SEQ name for the per-spawn zone stream; not the dead OP_ZoneSpawns).
    void spawn(const uint8_t* data, size_t len, uint8_t dir);
    // OP_MobUpdate (0x061b) S>C: per-mob position update.
    void mobUpdate(const uint8_t* data, size_t len, uint8_t dir);
    // OP_HPUpdate (0x2735) S>C: multiplexed stat channel; 6B subtype-0x02 is the
    // NPC HP-bar feed (percent).
    void hpUpdate(const uint8_t* data, size_t len, uint8_t dir);

private:
    ZoneMgr*    m_zoneMgr;
    SpawnShell* m_spawnShell;
    Player*     m_player;
};

#endif // SEQ_BACKEND_EQL_EQLDISPATCH_H
