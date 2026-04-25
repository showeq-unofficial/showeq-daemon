#pragma once

// Pure translation functions between in-memory showeq-c types (Item, Spawn)
// and seq.v1 protobuf messages. Side-effect free so sessionadapter.cpp can
// stay focused on wire I/O, and so the future Rust decoder (Phase 4) has a
// clear spec to match byte-for-byte.

#include "seq/v1/events.pb.h"

class GroupMgr;
class Item;
class Player;
class Spawn;
class SpellItem;
class MapData;

namespace seq::encode {

// Fills `out` from `in`. Populates pos, type, ids, name, levels, HP — all
// fields the Phase 1 web client needs to draw and label a moving dot.
void fillSpawn(seq::v1::Spawn* out, const Item& in);

// Fills a Pos proto from a Spawn's current position + velocity + heading.
void fillPos(seq::v1::Pos* out, const Spawn& in);

// Flattens every layer of `map` into a MapGeometry (lines + named
// locations + bounding rect). Safe to call on an unloaded MapData — the
// output will be empty geometry with zeroed bounds, which callers are
// expected to treat as "no map available for this zone".
void fillMapGeometry(seq::v1::MapGeometry* out, const MapData& map);

// Fills `out` from current Player state (HP/mana/stamina/exp/level/stats).
// Always fills every field; fields are 0 before the player profile loads,
// which the client renders as "—".
void fillPlayerStats(seq::v1::PlayerStats* out, const Player& p);

// Fills `out` with all 6 group slots from `g`. Slots without a member
// produce an empty GroupMember (in_zone=false, name="").
void fillGroupUpdate(seq::v1::GroupUpdate* out, GroupMgr& g);

// Fills `out` with one Buff message from a SpellShell SpellItem.
void fillBuff(seq::v1::Buff* out, const SpellItem& spell);

} // namespace seq::encode
