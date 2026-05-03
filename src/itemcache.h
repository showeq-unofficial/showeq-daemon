/*
 * itemcache.h
 *
 * Daemon-side cache mapping itemId -> ItemTemplate. Populated
 * incrementally from observed OP_ItemPacket fires (worn items, looted
 * items, vendor purchases, etc.) and persisted as JSON across daemon
 * restarts so that HP/mana/AC totals from worn gear can be computed
 * even for items the user hasn't moved this session.
 *
 *  ShowEQ Distributed under GPL
 *  http://seq.sf.net/
 */

#ifndef SHOWEQ_ITEMCACHE_H
#define SHOWEQ_ITEMCACHE_H

#include <QHash>
#include <QObject>
#include <QString>

#include <cstddef>
#include <cstdint>

#include "itempacket.h"

class QTimer;

class ItemCache : public QObject
{
    Q_OBJECT
public:
    explicit ItemCache(QObject* parent = nullptr);
    ~ItemCache() override;

    // File path on disk for the JSON cache. Loads any existing data
    // immediately. Empty path disables persistence.
    void setStorePath(const QString& path);
    QString storePath() const { return m_storePath; }

    // Periodic flush interval in ms. <= 0 disables. Default 15 minutes.
    void setFlushIntervalMs(int ms);

    bool lookup(uint32_t itemId, ItemTemplate* out) const;
    int  size() const { return m_cache.size(); }
    int  itemsLearnedThisSession() const { return m_learnedCount; }

    // Direct insert; useful for tests and re-entry on already-parsed
    // items. Marks the cache dirty.
    void insert(const ItemTemplate& tpl);

    bool save();

    // Aggregate sums over the player's currently equipped gear (the
    // contents of wornSlots(), looked up against the cache). itemCount
    // is the number of populated worn slots — empty slots don't
    // contribute. Stats are BASE values; aug contributions are not yet
    // folded in.
    struct Totals
    {
        int itemCount  = 0;
        int hp         = 0;
        int mana       = 0;
        int endurance  = 0;
        int ac         = 0;
        int stats[ITEM_STAT_COUNT]   = {0};
        int resists[ITEM_RES_COUNT]  = {0};
        int corruption = 0;
    };
    Totals totals() const;

    // Iterate cached items in itemId order. Used by SessionAdapter to
    // populate Snapshot.items.
    QList<uint32_t> sortedIds() const;

    // Slot index (0-22 worn) -> itemId currently equipped there, as
    // observed via OP_ItemPacket wrapper main_slot=0/sub_slot fields.
    // Empty slots are absent.
    QHash<int, uint32_t> wornSlots() const { return m_wornSlots; }

signals:
    // Fired after a successful insert (new id or overwrite of an
    // existing one). SessionAdapter listens and emits ItemLearned
    // envelopes.
    void itemLearned(uint32_t itemId);

    // Fired when the worn-slot map changes (any equip / un-equip /
    // slot-to-slot move). SessionAdapter listens and emits WornSet +
    // ItemCacheTotals envelopes.
    void wornSlotsChanged();

public slots:
    // connect2 entry point. Parses the raw OP_ItemPacket payload and
    // inserts the resulting ItemTemplate. dir is unused but matches the
    // slot signature expected by EQPacket::connect2.
    void onItemPacket(const uint8_t* data, size_t len, uint8_t dir);

private slots:
    void onFlushTimer();

private:
    bool load();

    QHash<uint32_t, ItemTemplate> m_cache;
    QHash<int, uint32_t>          m_wornSlots;
    QString m_storePath;
    QTimer* m_flushTimer   = nullptr;
    int     m_learnedCount = 0;
    bool    m_dirty        = false;
};

#endif // SHOWEQ_ITEMCACHE_H
