/*
 * itempacket.h
 *
 * Parser for the variable-length OP_ItemPacket (0x3f3b) serializedItem
 * blob. Extracts itemId, names, base stats, HP/mana/endurance/AC, and
 * resists from the wire format, returning a flat ItemTemplate suitable
 * for protobuf encoding or a daemon-side itemId→stats cache.
 *
 * The wire format gives BASE values; equipped augments contribute their
 * own ItemTemplate (each aug is itself an item served via OP_ItemPacket).
 * Computing total HP/mana from gear is therefore:
 *   sum over worn slots of (item.hp + sum over filled augs of aug.hp)
 *
 * Offsets and field meanings are documented on parsedItemTemplateStruct
 * in everquest.h.
 *
 *  ShowEQ Distributed under GPL
 *  http://seq.sf.net/
 */

#ifndef SHOWEQ_ITEMPACKET_H
#define SHOWEQ_ITEMPACKET_H

#include <QString>
#include <cstddef>
#include <cstdint>

// Indices into ItemTemplate::stats[].
enum ItemStatIndex
{
    ITEM_STAT_STR = 0,
    ITEM_STAT_STA = 1,
    ITEM_STAT_AGI = 2,
    ITEM_STAT_DEX = 3,
    ITEM_STAT_CHA = 4,
    ITEM_STAT_INT = 5,
    ITEM_STAT_WIS = 6,
    ITEM_STAT_COUNT = 7,
};

// Indices into ItemTemplate::resists[].
enum ItemResistIndex
{
    ITEM_RES_COLD    = 0,
    ITEM_RES_DISEASE = 1,
    ITEM_RES_POISON  = 2,
    ITEM_RES_MAGIC   = 3,
    ITEM_RES_FIRE    = 4,
    ITEM_RES_COUNT   = 5,
};

struct ItemTemplate
{
    QString  itemName;
    QString  loreName;
    uint32_t itemId       = 0;
    uint32_t slotBitmask  = 0;
    uint32_t flags        = 0;
    float    weight       = 0.0f;
    int32_t  hp           = 0;
    int32_t  mana         = 0;
    int32_t  endurance    = 0;
    int32_t  ac           = 0;
    int8_t   stats[ITEM_STAT_COUNT]   = {0};
    int8_t   resists[ITEM_RES_COUNT]  = {0};
    int8_t   corruption   = 0;
};

// Parse the full OP_ItemPacket payload (starting at packetType) into
// `out`. Returns true on success, false if the input is too short or
// the name region cannot be located.
bool parseItemPacket(const uint8_t* data, size_t len, ItemTemplate* out);

#endif // SHOWEQ_ITEMPACKET_H
