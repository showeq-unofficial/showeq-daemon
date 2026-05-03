/*
 * itemcache.cpp - see itemcache.h.
 *
 * JSON schema: a top-level array of item objects, sorted by itemId for
 * stable diffs. Zero-valued numeric fields are omitted to keep the file
 * compact. Unknown JSON keys are tolerated on load so future
 * parsedItemTemplateStruct fields can be added without invalidating
 * existing caches.
 */

#include "itemcache.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>

#include <algorithm>

namespace {

constexpr int kDefaultFlushMs = 15 * 60 * 1000;

QJsonObject toJson(const ItemTemplate& t)
{
    QJsonObject o;
    o.insert(QStringLiteral("id"),       qint64(t.itemId));
    o.insert(QStringLiteral("name"),     t.itemName);
    if (t.loreName != t.itemName) {
        o.insert(QStringLiteral("lore"), t.loreName);
    }
    o.insert(QStringLiteral("slotMask"), qint64(t.slotBitmask));
    if (t.flags) {
        o.insert(QStringLiteral("flags"), qint64(t.flags));
    }
    if (t.weight != 0.0f) {
        o.insert(QStringLiteral("weight"), double(t.weight));
    }
    if (t.hp)        o.insert(QStringLiteral("hp"),        t.hp);
    if (t.mana)      o.insert(QStringLiteral("mana"),      t.mana);
    if (t.endurance) o.insert(QStringLiteral("endurance"), t.endurance);
    if (t.ac)        o.insert(QStringLiteral("ac"),        t.ac);

    bool anyStat = false;
    for (int i = 0; i < ITEM_STAT_COUNT; i++) {
        if (t.stats[i]) { anyStat = true; break; }
    }
    if (anyStat) {
        QJsonArray stats;
        for (int i = 0; i < ITEM_STAT_COUNT; i++) stats.append(t.stats[i]);
        o.insert(QStringLiteral("stats"), stats);
    }

    bool anyResist = false;
    for (int i = 0; i < ITEM_RES_COUNT; i++) {
        if (t.resists[i]) { anyResist = true; break; }
    }
    if (anyResist) {
        QJsonArray resists;
        for (int i = 0; i < ITEM_RES_COUNT; i++) resists.append(t.resists[i]);
        o.insert(QStringLiteral("resists"), resists);
    }

    if (t.corruption) {
        o.insert(QStringLiteral("corruption"), t.corruption);
    }
    return o;
}

ItemTemplate fromJson(const QJsonObject& o)
{
    ItemTemplate t;
    t.itemId      = uint32_t(o.value(QStringLiteral("id")).toVariant().toULongLong());
    t.itemName    = o.value(QStringLiteral("name")).toString();
    t.loreName    = o.value(QStringLiteral("lore")).toString();
    if (t.loreName.isEmpty()) t.loreName = t.itemName;
    t.slotBitmask = uint32_t(o.value(QStringLiteral("slotMask")).toVariant().toULongLong());
    t.flags       = uint32_t(o.value(QStringLiteral("flags")).toVariant().toULongLong());
    t.weight      = float(o.value(QStringLiteral("weight")).toDouble());
    t.hp          = o.value(QStringLiteral("hp")).toInt();
    t.mana        = o.value(QStringLiteral("mana")).toInt();
    t.endurance   = o.value(QStringLiteral("endurance")).toInt();
    t.ac          = o.value(QStringLiteral("ac")).toInt();

    auto stats = o.value(QStringLiteral("stats")).toArray();
    for (int i = 0; i < ITEM_STAT_COUNT && i < stats.size(); i++) {
        t.stats[i] = int8_t(stats.at(i).toInt());
    }
    auto resists = o.value(QStringLiteral("resists")).toArray();
    for (int i = 0; i < ITEM_RES_COUNT && i < resists.size(); i++) {
        t.resists[i] = int8_t(resists.at(i).toInt());
    }
    t.corruption = int8_t(o.value(QStringLiteral("corruption")).toInt());
    return t;
}

} // namespace

ItemCache::ItemCache(QObject* parent)
    : QObject(parent)
    , m_flushTimer(new QTimer(this))
{
    m_flushTimer->setInterval(kDefaultFlushMs);
    m_flushTimer->setSingleShot(false);
    connect(m_flushTimer, &QTimer::timeout, this, &ItemCache::onFlushTimer);
}

ItemCache::~ItemCache()
{
    if (m_dirty && !m_storePath.isEmpty()) {
        save();
    }
}

void ItemCache::setStorePath(const QString& path)
{
    m_storePath = path;
    if (!path.isEmpty()) {
        load();
        if (m_flushTimer->interval() > 0) m_flushTimer->start();
    } else {
        m_flushTimer->stop();
    }
}

void ItemCache::setFlushIntervalMs(int ms)
{
    if (ms <= 0) {
        m_flushTimer->stop();
        m_flushTimer->setInterval(0);
        return;
    }
    m_flushTimer->setInterval(ms);
    if (!m_storePath.isEmpty()) m_flushTimer->start();
}

bool ItemCache::lookup(uint32_t itemId, ItemTemplate* out) const
{
    auto it = m_cache.constFind(itemId);
    if (it == m_cache.constEnd()) return false;
    if (out) *out = it.value();
    return true;
}

void ItemCache::insert(const ItemTemplate& tpl)
{
    if (tpl.itemId == 0) return;
    auto it = m_cache.find(tpl.itemId);
    if (it == m_cache.end()) ++m_learnedCount;
    m_cache.insert(tpl.itemId, tpl);
    m_dirty = true;
}

void ItemCache::onItemPacket(const uint8_t* data, size_t len,
                             uint8_t /*dir*/)
{
    ItemTemplate t;
    if (!parseItemPacket(data, len, &t)) return;
    insert(t);
}

void ItemCache::onFlushTimer()
{
    if (m_dirty && !m_storePath.isEmpty()) save();
}

bool ItemCache::load()
{
    QFile f(m_storePath);
    if (!f.exists()) {
        qInfo("ItemCache: no existing cache at %s (first run)",
              qUtf8Printable(m_storePath));
        return true;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("ItemCache::load: cannot open %s for read",
                 qUtf8Printable(m_storePath));
        return false;
    }
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        qWarning("ItemCache::load: invalid JSON in %s: %s",
                 qUtf8Printable(m_storePath),
                 qUtf8Printable(err.errorString()));
        return false;
    }
    auto arr = doc.array();
    m_cache.reserve(arr.size());
    for (const auto& v : arr) {
        if (!v.isObject()) continue;
        auto t = fromJson(v.toObject());
        if (t.itemId == 0) continue;
        m_cache.insert(t.itemId, t);
    }
    qInfo("ItemCache: loaded %d items from %s",
          int(m_cache.size()), qUtf8Printable(m_storePath));
    m_dirty = false;
    return true;
}

bool ItemCache::save()
{
    if (m_storePath.isEmpty()) return false;

    auto ids = m_cache.keys();
    std::sort(ids.begin(), ids.end());

    QJsonArray arr;
    for (uint32_t id : ids) {
        arr.append(toJson(m_cache.value(id)));
    }
    QJsonDocument doc(arr);

    QSaveFile f(m_storePath);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning("ItemCache::save: cannot open %s for write",
                 qUtf8Printable(m_storePath));
        return false;
    }
    f.write(doc.toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        qWarning("ItemCache::save: commit failed for %s",
                 qUtf8Printable(m_storePath));
        return false;
    }
    qInfo("ItemCache: saved %d items (%d new this session) to %s",
          int(m_cache.size()), m_learnedCount,
          qUtf8Printable(m_storePath));
    m_dirty = false;
    return true;
}
