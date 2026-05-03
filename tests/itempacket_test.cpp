/*
 *  itempacket_test.cpp
 *  Tier-1 unit tests for parseItemPacket().
 *
 *  Live-captured OP_ItemPacket dumps contain personal character data
 *  and are not committed; instead we hand-construct synthetic payloads
 *  matching the documented layout. Round-trip wire offsets were
 *  verified against five real items (Crescent Gi, Platinum Fire
 *  Wedding Ring, and three "of Distant Echoes" pieces).
 */

#include <QtTest/QtTest>

#include <vector>

#include "itempacket.h"

namespace {

// Build a synthetic OP_ItemPacket payload with the given fields. Mirrors
// the wire format documented on parsedItemTemplateStruct in everquest.h.
std::vector<uint8_t> buildSyntheticItem(
    const QString& name, const QString& lore,
    uint32_t itemId, uint32_t weightX10, uint32_t slotMask,
    int32_t hp, int32_t mana, int32_t endurance, int32_t ac,
    int8_t strMod, int8_t agiMod,
    int8_t mrMod, int8_t frMod)
{
    std::vector<uint8_t> buf;
    buf.reserve(512);

    // packetType (uint32 LE) — observed values: 0x76, 0x78. The exact
    // value isn't needed for parsing; the parser scans past the header.
    buf.insert(buf.end(), {0x78, 0x00, 0x00, 0x00});

    // Pre-name binary header: 16-byte ASCII instance id + null + a
    // mix of zeros and 0xff bytes ending right before the name. We
    // reproduce just enough structure for the parser's `ff ff ff ff
    // 00 .. <UPPERCASE>` heuristic to fire at the right place.
    static const char kInstanceId[] = "TEST00B80003TST0";
    buf.insert(buf.end(), kInstanceId, kInstanceId + 16);
    buf.push_back(0); // null terminator of instance id

    // Pad with zeros up to the typical name offset (0x77). Real
    // payloads have stack/slot/charges/etc. interleaved here, but
    // the parser doesn't need them to be valid.
    while (buf.size() < 0x70) buf.push_back(0);
    // ff ff ff ff 00 00 00 00 marker right before the name
    for (int i = 0; i < 4; i++) buf.push_back(0xff);
    for (int i = 0; i < 4; i++) buf.push_back(0x00);
    Q_ASSERT(buf.size() == 0x78);
    // The parser expects an UPPERCASE ASCII byte preceded by a NUL. The
    // last byte we pushed above is 0x00, so the next byte starts the
    // name. We place the name at offset 0x78 instead of the on-the-wire
    // 0x77 — close enough for the parser, which scans forward.

    // Item name + null
    QByteArray nameBytes = name.toLatin1();
    buf.insert(buf.end(), nameBytes.begin(), nameBytes.end());
    buf.push_back(0);

    // Lore name + null
    QByteArray loreBytes = lore.toLatin1();
    buf.insert(buf.end(), loreBytes.begin(), loreBytes.end());
    buf.push_back(0);

    // parsedItemTemplateStruct (63 bytes documented prefix)
    auto pushU32 = [&](uint32_t v) {
        buf.push_back(uint8_t(v));
        buf.push_back(uint8_t(v >> 8));
        buf.push_back(uint8_t(v >> 16));
        buf.push_back(uint8_t(v >> 24));
    };
    auto pushI32 = [&](int32_t v) { pushU32(uint32_t(v)); };

    pushU32(63);               // +0  format_const
    pushU32(0);                // +4  reserved04
    pushU32(itemId);           // +8  itemId
    pushU32(weightX10);        // +12 weight_x10
    pushU32(0);                // +16 flags (size enum etc.)
    pushU32(slotMask);         // +20 slot_bitmask
    pushU32(0);                // +24 unk024
    pushU32(0);                // +28 unk028
    buf.push_back(0); buf.push_back(0); // +32 pad032 (2 bytes)
    // +34..+38: resists [CR, DR, PR, MR, FR]
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(uint8_t(mrMod));
    buf.push_back(uint8_t(frMod));
    buf.push_back(0);          // +39 corruption
    // +40..+46: stats [STR, STA, AGI, DEX, CHA, INT, WIS]
    buf.push_back(uint8_t(strMod));
    buf.push_back(0);
    buf.push_back(uint8_t(agiMod));
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    buf.push_back(0);
    pushI32(hp);               // +47
    pushI32(mana);             // +51
    pushI32(endurance);        // +55
    pushI32(ac);               // +59

    return buf;
}

} // namespace

class ItemPacketTest : public QObject
{
  Q_OBJECT

private slots:
  void parsesArmorWithFullStatBlock();
  void parsesRingWithJustHp();
  void parsesWeaponWithNoArmorStats();
  void rejectsNullData();
  void rejectsTruncatedData();
  void rejectsBufferWithoutName();
};

void ItemPacketTest::parsesArmorWithFullStatBlock()
{
    auto buf = buildSyntheticItem(
        QStringLiteral("Test Plate Chestpiece"),
        QStringLiteral("Test Plate Chestpiece"),
        /* itemId */ 1234,
        /* weightX10 */ 50,
        /* slotMask */ 0x20000,  // Chest
        /* hp */ 100, /* mana */ 50, /* endurance */ 30, /* ac */ 25,
        /* str */ 5, /* agi */ 3, /* mr */ 4, /* fr */ 2);

    ItemTemplate t;
    QVERIFY(parseItemPacket(buf.data(), buf.size(), &t));
    QCOMPARE(t.itemId, uint32_t(1234));
    QCOMPARE(t.itemName, QStringLiteral("Test Plate Chestpiece"));
    QCOMPARE(t.loreName, QStringLiteral("Test Plate Chestpiece"));
    QCOMPARE(t.weight, 5.0f);
    QCOMPARE(t.slotBitmask, uint32_t(0x20000));
    QCOMPARE(t.hp, 100);
    QCOMPARE(t.mana, 50);
    QCOMPARE(t.endurance, 30);
    QCOMPARE(t.ac, 25);
    QCOMPARE(t.stats[ITEM_STAT_STR], int8_t(5));
    QCOMPARE(t.stats[ITEM_STAT_AGI], int8_t(3));
    QCOMPARE(t.stats[ITEM_STAT_STA], int8_t(0));
    QCOMPARE(t.resists[ITEM_RES_MAGIC], int8_t(4));
    QCOMPARE(t.resists[ITEM_RES_FIRE], int8_t(2));
    QCOMPARE(t.resists[ITEM_RES_COLD], int8_t(0));
    QCOMPARE(t.corruption, int8_t(0));
}

void ItemPacketTest::parsesRingWithJustHp()
{
    auto buf = buildSyntheticItem(
        QStringLiteral("Plain HP Ring"),
        QStringLiteral("Plain HP Ring"),
        /* itemId */ 5678,
        /* weightX10 */ 1,
        /* slotMask */ 0x18000,  // Ring1|Ring2
        /* hp */ 55, /* mana */ 0, /* endurance */ 0, /* ac */ 5,
        /* str */ 0, /* agi */ 0, /* mr */ 0, /* fr */ 0);

    ItemTemplate t;
    QVERIFY(parseItemPacket(buf.data(), buf.size(), &t));
    QCOMPARE(t.hp, 55);
    QCOMPARE(t.mana, 0);
    QCOMPARE(t.ac, 5);
    QCOMPARE(t.slotBitmask, uint32_t(0x18000));
    QCOMPARE(t.weight, 0.1f);
}

void ItemPacketTest::parsesWeaponWithNoArmorStats()
{
    auto buf = buildSyntheticItem(
        QStringLiteral("Dummy Weapon"),
        QStringLiteral("Dummy Weapon"),
        /* itemId */ 9999,
        /* weightX10 */ 30,
        /* slotMask */ 0x6000,  // Primary|Secondary
        /* hp */ 0, /* mana */ 0, /* endurance */ 0, /* ac */ 0,
        /* str */ 0, /* agi */ 0, /* mr */ 0, /* fr */ 0);

    ItemTemplate t;
    QVERIFY(parseItemPacket(buf.data(), buf.size(), &t));
    QCOMPARE(t.hp, 0);
    QCOMPARE(t.ac, 0);
    QCOMPARE(t.slotBitmask, uint32_t(0x6000));
}

void ItemPacketTest::rejectsNullData()
{
    ItemTemplate t;
    QVERIFY(!parseItemPacket(nullptr, 1024, &t));
    QVERIFY(!parseItemPacket(reinterpret_cast<const uint8_t*>(""), 100, nullptr));
}

void ItemPacketTest::rejectsTruncatedData()
{
    auto buf = buildSyntheticItem(
        QStringLiteral("Truncated Item"),
        QStringLiteral("Truncated Item"),
        1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0);

    ItemTemplate t;
    // Drop off everything past the name — no parsedItemTemplateStruct.
    QVERIFY(!parseItemPacket(buf.data(), 0x80, &t));
}

void ItemPacketTest::rejectsBufferWithoutName()
{
    // All-zero buffer has no uppercase letter to anchor on.
    std::vector<uint8_t> buf(512, 0);
    ItemTemplate t;
    QVERIFY(!parseItemPacket(buf.data(), buf.size(), &t));
}

QTEST_APPLESS_MAIN(ItemPacketTest)
#include "itempacket_test.moc"
