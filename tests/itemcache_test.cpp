/*
 *  itemcache_test.cpp
 *  Tier-1 unit test for ItemCache (insert/lookup, JSON round-trip,
 *  dirty-flag semantics, and zero-suppressing JSON serialization).
 */

#include <QtTest/QtTest>

#include <QFile>
#include <QTemporaryDir>

#include "itemcache.h"

class ItemCacheTest : public QObject
{
  Q_OBJECT

private slots:
  void emptyByDefault();
  void insertAndLookup();
  void insertIgnoresZeroId();
  void duplicateInsertOverwritesAndCountsOnce();
  void roundTripsThroughJson();
  void preservesAllStatFieldsThroughJson();
  void omitsZeroFieldsFromJson();
  void loadFromMissingFileSucceedsEmpty();

private:
  static ItemTemplate sampleArmor();
  static ItemTemplate sampleRing();
};

ItemTemplate ItemCacheTest::sampleArmor()
{
    ItemTemplate t;
    t.itemId       = 111843;
    t.itemName     = QStringLiteral("Outstanding Necklace of Distant Echoes");
    t.loreName     = t.itemName;
    t.slotBitmask  = 0x20;
    t.flags        = 0x01000001;
    t.weight       = 0.8f;
    t.hp           = 122;
    t.mana         = 124;
    t.endurance    = 90;
    t.ac           = 16;
    t.stats[ITEM_STAT_STR] = 10;
    t.stats[ITEM_STAT_STA] = 10;
    t.stats[ITEM_STAT_AGI] = 10;
    t.stats[ITEM_STAT_DEX] = 10;
    t.stats[ITEM_STAT_CHA] = 10;
    t.stats[ITEM_STAT_INT] = 10;
    t.stats[ITEM_STAT_WIS] = 10;
    t.resists[ITEM_RES_COLD]    = 7;
    t.resists[ITEM_RES_DISEASE] = 7;
    t.resists[ITEM_RES_POISON]  = 7;
    t.resists[ITEM_RES_MAGIC]   = 8;
    t.resists[ITEM_RES_FIRE]    = 7;
    t.corruption = 1;
    return t;
}

ItemTemplate ItemCacheTest::sampleRing()
{
    ItemTemplate t;
    t.itemId      = 14679;
    t.itemName    = QStringLiteral("Platinum Fire Wedding Ring");
    t.loreName    = t.itemName;
    t.slotBitmask = 0x18000;
    t.weight      = 0.1f;
    t.hp          = 55;
    t.ac          = 5;
    return t;
}

void ItemCacheTest::emptyByDefault()
{
    ItemCache c;
    QCOMPARE(c.size(), 0);
    QCOMPARE(c.itemsLearnedThisSession(), 0);
    ItemTemplate dst;
    QVERIFY(!c.lookup(1234, &dst));
}

void ItemCacheTest::insertAndLookup()
{
    ItemCache c;
    c.insert(sampleArmor());
    QCOMPARE(c.size(), 1);
    QCOMPARE(c.itemsLearnedThisSession(), 1);

    ItemTemplate dst;
    QVERIFY(c.lookup(111843, &dst));
    QCOMPARE(dst.itemName, QStringLiteral("Outstanding Necklace of Distant Echoes"));
    QCOMPARE(dst.hp, 122);
    QCOMPARE(dst.mana, 124);

    QVERIFY(!c.lookup(99999, &dst));
}

void ItemCacheTest::insertIgnoresZeroId()
{
    ItemCache c;
    ItemTemplate t = sampleRing();
    t.itemId = 0;
    c.insert(t);
    QCOMPARE(c.size(), 0);
    QCOMPARE(c.itemsLearnedThisSession(), 0);
}

void ItemCacheTest::duplicateInsertOverwritesAndCountsOnce()
{
    ItemCache c;
    c.insert(sampleArmor());
    QCOMPARE(c.itemsLearnedThisSession(), 1);

    // Re-insert same id with mutated fields: should overwrite without
    // bumping the learned counter.
    ItemTemplate t = sampleArmor();
    t.hp = 999;
    c.insert(t);
    QCOMPARE(c.size(), 1);
    QCOMPARE(c.itemsLearnedThisSession(), 1);

    ItemTemplate dst;
    QVERIFY(c.lookup(111843, &dst));
    QCOMPARE(dst.hp, 999);
}

void ItemCacheTest::roundTripsThroughJson()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QLatin1String("/itemcache.json");

    {
        ItemCache c;
        c.setStorePath(path);
        c.insert(sampleArmor());
        c.insert(sampleRing());
        QVERIFY(c.save());
    }

    ItemCache c2;
    c2.setStorePath(path);
    QCOMPARE(c2.size(), 2);

    ItemTemplate ring;
    QVERIFY(c2.lookup(14679, &ring));
    QCOMPARE(ring.itemName, QStringLiteral("Platinum Fire Wedding Ring"));
    QCOMPARE(ring.hp, 55);
    QCOMPARE(ring.ac, 5);
    QCOMPARE(ring.slotBitmask, uint32_t(0x18000));
    QCOMPARE(ring.weight, 0.1f);

    ItemTemplate neck;
    QVERIFY(c2.lookup(111843, &neck));
    QCOMPARE(neck.hp, 122);
    QCOMPARE(neck.mana, 124);
    QCOMPARE(neck.stats[ITEM_STAT_STR], int8_t(10));
    QCOMPARE(neck.resists[ITEM_RES_MAGIC], int8_t(8));
    QCOMPARE(neck.corruption, int8_t(1));
}

void ItemCacheTest::preservesAllStatFieldsThroughJson()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QLatin1String("/itemcache.json");

    ItemTemplate src = sampleArmor();
    {
        ItemCache c;
        c.setStorePath(path);
        c.insert(src);
        QVERIFY(c.save());
    }

    ItemCache c2;
    c2.setStorePath(path);
    ItemTemplate got;
    QVERIFY(c2.lookup(src.itemId, &got));
    for (int i = 0; i < ITEM_STAT_COUNT; i++) {
        QCOMPARE(got.stats[i], src.stats[i]);
    }
    for (int i = 0; i < ITEM_RES_COUNT; i++) {
        QCOMPARE(got.resists[i], src.resists[i]);
    }
    QCOMPARE(got.corruption, src.corruption);
    QCOMPARE(got.slotBitmask, src.slotBitmask);
    QCOMPARE(got.flags, src.flags);
}

void ItemCacheTest::omitsZeroFieldsFromJson()
{
    // A weapon with no HP/mana/AC/stats/resists should serialize compactly.
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QLatin1String("/itemcache.json");

    ItemTemplate weapon;
    weapon.itemId      = 9999;
    weapon.itemName    = QStringLiteral("Plain Weapon");
    weapon.loreName    = weapon.itemName;
    weapon.slotBitmask = 0x6000;
    weapon.weight      = 3.0f;

    {
        ItemCache c;
        c.setStorePath(path);
        c.insert(weapon);
        QVERIFY(c.save());
    }

    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray json = f.readAll();
    QVERIFY(!json.contains("\"hp\""));
    QVERIFY(!json.contains("\"mana\""));
    QVERIFY(!json.contains("\"ac\""));
    QVERIFY(!json.contains("\"stats\""));
    QVERIFY(!json.contains("\"resists\""));
    QVERIFY(json.contains("\"name\": \"Plain Weapon\""));
    QVERIFY(json.contains("\"slotMask\""));
}

void ItemCacheTest::loadFromMissingFileSucceedsEmpty()
{
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.path() + QLatin1String("/does-not-exist.json");

    ItemCache c;
    c.setStorePath(path);
    QCOMPARE(c.size(), 0);
}

QTEST_GUILESS_MAIN(ItemCacheTest)
#include "itemcache_test.moc"
