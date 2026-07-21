#ifndef MANAGERSET_H
#define MANAGERSET_H

#include <QString>

// ManagerSet bundles the per-box state managers — the ones that hold
// game state for a single decoded EQ session ("box"). Multibox runs one
// ManagerSet per box so every box decodes continuously into its own
// state and switching the active box (which one SessionAdapter streams
// to the web client) is a non-destructive rebind, not a clear+resnapshot.
//
// The managers NOT listed here stay daemon-global and are shared across
// every box: ItemCache, GuildMgr, FilterMgr, Spells, EQStr,
// Messages/MessageFilters, CategoryMgr, PrefsBroker, DateTimeMgr,
// DataLocationMgr, ZoneServerMgr, MapData. They are either stateless,
// config, or server-uniform.
//
// This is a plain pointer bundle — it does NOT own the managers. The
// managers are QObjects parented to DaemonApp; DaemonApp owns the
// ManagerSet bundles (keyed by box_id) and constructs/wires them via
// buildManagerSet() + wireBoxPipeline().

class ZoneMgr;
class Player;
class SpawnShell;
class SpellShell;
class GroupMgr;
class GuildShell;
class MessageShell;
class CombatRouter;
class SpawnMonitor;

struct ManagerSet {
    ZoneMgr*      zoneMgr      = nullptr;
    Player*       player       = nullptr;
    SpawnShell*   spawnShell   = nullptr;
    SpellShell*   spellShell   = nullptr;
    GroupMgr*     groupMgr     = nullptr;
    // The roster of THIS character's guild — per-box, unlike the daemon-global
    // GuildMgr, which only maps guild ids to names server-wide.
    GuildShell*   guildShell   = nullptr;
    MessageShell* messageShell = nullptr;
    CombatRouter* combatRouter = nullptr;
    SpawnMonitor* spawnMonitor = nullptr;
};

// Resolves the active box's ManagerSet for SessionAdapter. DaemonApp
// implements this; WsServer passes it through to each SessionAdapter so
// the adapter can (re)bind to the active box's managers on subscribe and
// on active-box switch without WsServer/SessionAdapter knowing how boxes
// are constructed or owned.
class ManagerSetProvider {
public:
    virtual ~ManagerSetProvider() = default;
    // The ManagerSet for box_id, or the active box's set if box_id is
    // empty. nullptr if no such box / no active box yet.
    virtual const ManagerSet* managersForBox(const QString& boxId) const = 0;
};

#endif // MANAGERSET_H
