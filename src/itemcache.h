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
    QString m_storePath;
    QTimer* m_flushTimer   = nullptr;
    int     m_learnedCount = 0;
    bool    m_dirty        = false;
};

#endif // SHOWEQ_ITEMCACHE_H
