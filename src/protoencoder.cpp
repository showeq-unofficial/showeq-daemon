#include "protoencoder.h"

#include <QPoint>

#include "mapcore.h"
#include "player.h"
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

    // Convert the legacy heading representation to degrees 0..359.
    // - Player has a precomputed `headingDegrees()` from the full 12-bit
    //   wire value; the truncated int8_t in m_heading would lose the
    //   high four bits and alias compass directions onto each other.
    // - Other spawns store an 8-bit heading; convert the same way the
    //   legacy code does for headingDegrees but with a 256-step scale.
    int headingDegrees;
    if (const auto* p = dynamic_cast<const Player*>(&in)) {
        headingDegrees = p->headingDegrees();
    } else {
        const uint8_t raw8 = static_cast<uint8_t>(in.heading());
        headingDegrees = 360 - ((raw8 * 360) >> 8);
    }
    out->set_heading(headingDegrees);
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
        out->set_filter_flags(sp->filterFlags());
        fillPos(out->mutable_pos(), *sp);
    } else {
        // Drops, doors: no Spawn state, just position on the Item base.
        auto* pos = out->mutable_pos();
        pos->set_x(it.x());
        pos->set_y(it.y());
        pos->set_z(it.z());
    }
}

void fillMapGeometry(seq::v1::MapGeometry* out, const MapData& map)
{
    out->set_min_x(map.minX());
    out->set_min_y(map.minY());
    out->set_max_x(map.maxX());
    out->set_max_y(map.maxY());

    // MapData is const-correct, but mapLayer() is non-const because layers
    // are lazily created during editing. We only read here, so cast away.
    auto& mdata = const_cast<MapData&>(map);
    const uint8_t layerCount = mdata.numLayers();
    for (uint8_t i = 0; i < layerCount; ++i) {
        MapLayer* layer = mdata.mapLayer(i);
        if (!layer) continue;

        // 2D lines — L-lines carry an optional single shared z.
        for (const MapLineL* line : layer->lLines()) {
            if (!line || line->isEmpty()) continue;
            auto* out_line = out->add_lines();
            out_line->set_color(line->colorName().toStdString());
            out_line->set_layer(i);
            const int n = line->size();
            for (int p = 0; p < n; ++p) {
                const QPoint pt = line->at(p);
                out_line->add_x(pt.x());
                out_line->add_y(pt.y());
            }
            if (line->heightSet()) {
                out_line->add_z(line->z());
            }
        }

        // 3D lines — M-lines carry per-point z.
        for (const MapLineM* line : layer->mLines()) {
            if (!line || line->isEmpty()) continue;
            auto* out_line = out->add_lines();
            out_line->set_color(line->colorName().toStdString());
            out_line->set_layer(i);
            const int n = line->size();
            for (int p = 0; p < n; ++p) {
                const MapPoint& pt = line->point(p);
                out_line->add_x(pt.x());
                out_line->add_y(pt.y());
                out_line->add_z(pt.z());
            }
        }

        for (const MapLocation* loc : layer->locations()) {
            if (!loc) continue;
            auto* out_loc = out->add_locations();
            out_loc->set_name(loc->name().toStdString());
            out_loc->set_color(loc->colorName().toStdString());
            out_loc->set_x(loc->x());
            out_loc->set_y(loc->y());
            out_loc->set_z(loc->z());
            out_loc->set_z_valid(loc->heightSet());
            out_loc->set_layer(i);
        }
    }
}

} // namespace seq::encode
