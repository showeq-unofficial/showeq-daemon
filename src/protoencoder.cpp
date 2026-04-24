#include "protoencoder.h"

#include "spawn.h"

namespace seq::encode {

static seq::v1::SpawnType typeFromItem(const Item& it)
{
    switch (it.type()) {
    case tPlayer: return seq::v1::PC;
    case tSpawn:  break;
    case tDrop:   return seq::v1::DROP;
    case tDoors:  return seq::v1::DOOR;
    default:      return seq::v1::SPAWN_UNSPECIFIED;
    }
    if (const auto* sp = dynamic_cast<const Spawn*>(&it)) {
        if (sp->isCorpse()) {
            return sp->NPC() ? seq::v1::CORPSE_NPC : seq::v1::CORPSE_PC;
        }
    }
    return seq::v1::NPC;
}

void fillPos(seq::v1::Pos* out, const Spawn& in)
{
    out->set_x(in.x());
    out->set_y(in.y());
    out->set_z(in.z());
    out->set_vx(in.deltaX());
    out->set_vy(in.deltaY());
    out->set_vz(in.deltaZ());
    out->set_heading(in.heading());
    out->set_delta_heading(in.deltaHeading());
    out->set_animation(in.animation());
}

void fillSpawn(seq::v1::Spawn* out, const Item& it)
{
    out->set_id(it.id());
    out->set_name(it.name().toStdString());
    out->set_type(typeFromItem(it));

    if (const auto* sp = dynamic_cast<const Spawn*>(&it)) {
        // lastName() is absent in Phase 1 headers; omit for now.
        out->set_level(sp->level());
        out->set_race(sp->race());
        out->set_class_(sp->classVal());
        out->set_deity(sp->deity());
        out->set_hp_cur(sp->HP());
        out->set_hp_max(sp->maxHP());
        out->set_guild_id(sp->guildID());
        out->set_guild_server_id(sp->guildServerID());
        out->set_is_gm(sp->gm() != 0);
        fillPos(out->mutable_pos(), *sp);
    } else {
        // Drops, doors: no Spawn state, just position on the Item base.
        auto* pos = out->mutable_pos();
        pos->set_x(it.x());
        pos->set_y(it.y());
        pos->set_z(it.z());
    }
}

} // namespace seq::encode
