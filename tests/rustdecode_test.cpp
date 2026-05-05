// Tier-1 FFI roundtrip test for the Stage A Rust decoder.
//
// Calls seq::rust::decode_mob_update() across the cxx bridge and
// asserts the returned MobUpdateOut matches the same buffer parsed
// by the legacy C bitfield path. Catches FFI-layout regressions
// (e.g. struct repacking, field reordering) independently of the
// tier-2 .vpk replay harness.
//
// What this does NOT catch:
//   - Bit-packing assumptions inside seq-decode (those live in the
//     Rust crate's own cargo tests under seq-decode/tests/)
//   - End-to-end SessionAdapter behavior (covered by tier-2 cmp)

#include <QtTest/QtTest>
#include <cstdint>
#include <cstring>

#include "everquest.h"
#include "seq-bridge-cxx/lib.h"

class RustDecodeTest : public QObject {
    Q_OBJECT

private slots:
    void decode_mob_update_matches_cpp_path();
    void decode_mob_update_negative_coords();
    void decode_mob_update_bad_length_returns_not_ok();
    void decode_delete_spawn_matches_cpp_path();
    void decode_delete_spawn_max_id();
    void decode_delete_spawn_bad_length_returns_not_ok();
    void decode_spawn_field_layout_round_trip();
    void decode_spawn_truncated_returns_not_ok();
    void decode_remove_spawn_round_trip();
    void decode_hp_update_round_trip();
    void decode_mob_health_round_trip();
    void decode_spawn_appearance_round_trip();
    void decode_exp_update_round_trip();
    void decode_level_update_round_trip();
    void decode_skill_update_round_trip();
    void decode_mana_change_round_trip();
    void decode_stamina_round_trip();
    void decode_end_update_round_trip();
    void decode_consider_round_trip();
    void decode_spawn_rename_round_trip();
    void decode_client_target_round_trip();
    void decode_death_round_trip();
};

// Build a 14-byte spawnPositionUpdate buffer the same way the wire
// would deliver one — using the actual C struct, then memcpy out.
// Guarantees the layout under test matches what EQPacket dispatches.
static QByteArray buildPayload(int16_t spawnId, int32_t x, int32_t y, int32_t z,
                               uint16_t heading)
{
    spawnPositionUpdate s{};
    s.spawnId = spawnId;
    s.x = x;
    s.y = y;
    s.z = z;
    s.heading = heading;
    QByteArray buf(sizeof(spawnPositionUpdate), '\0');
    std::memcpy(buf.data(), &s, sizeof(spawnPositionUpdate));
    return buf;
}

void RustDecodeTest::decode_mob_update_matches_cpp_path()
{
    // Mid-range positive values.
    const QByteArray buf = buildPayload(0x1234, 1234, 5678, -42, 0xABC);
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());

    // C path (mirrors SpawnShell::updateSpawns line 1271).
    const spawnPositionUpdate* cpp =
        reinterpret_cast<const spawnPositionUpdate*>(data);

    // Rust path via cxx.
    auto out = seq::rust::decode_mob_update(
        rust::Slice<const uint8_t>{data, sizeof(spawnPositionUpdate)});

    QVERIFY(out.ok);
    QCOMPARE(static_cast<uint16_t>(out.spawn_id),
             static_cast<uint16_t>(cpp->spawnId));
    QCOMPARE(static_cast<int32_t>(out.x), static_cast<int32_t>(cpp->x >> 3));
    QCOMPARE(static_cast<int32_t>(out.y), static_cast<int32_t>(cpp->y >> 3));
    QCOMPARE(static_cast<int32_t>(out.z), static_cast<int32_t>(cpp->z >> 3));
    QCOMPARE(out.heading, static_cast<uint16_t>(cpp->heading));
}

void RustDecodeTest::decode_mob_update_negative_coords()
{
    // Sign-extension is the most likely place to silently disagree.
    // 19-bit signed range: -262144 .. 262143.
    const QByteArray buf = buildPayload(0x0001, -262144, 262143, -1, 0);
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());

    auto out = seq::rust::decode_mob_update(
        rust::Slice<const uint8_t>{data, sizeof(spawnPositionUpdate)});

    QVERIFY(out.ok);
    QCOMPARE(out.x, -32768);   // -262144 >> 3
    QCOMPARE(out.y, 32767);    //  262143 >> 3
    QCOMPARE(out.z, -1);       //  -1     >> 3 = -1 (arithmetic)
}

void RustDecodeTest::decode_mob_update_bad_length_returns_not_ok()
{
    const uint8_t buf[13] = {0};
    auto out = seq::rust::decode_mob_update(
        rust::Slice<const uint8_t>{buf, sizeof(buf)});
    QVERIFY(!out.ok);
}

void RustDecodeTest::decode_delete_spawn_matches_cpp_path()
{
    deleteSpawnStruct s{};
    s.spawnId = 0xCAFEBABE;
    QByteArray buf(sizeof(deleteSpawnStruct), '\0');
    std::memcpy(buf.data(), &s, sizeof(deleteSpawnStruct));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());

    const deleteSpawnStruct* cpp =
        reinterpret_cast<const deleteSpawnStruct*>(data);
    auto out = seq::rust::decode_delete_spawn(
        rust::Slice<const uint8_t>{data, sizeof(deleteSpawnStruct)});

    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, cpp->spawnId);
}

void RustDecodeTest::decode_delete_spawn_max_id()
{
    // u32 max — guards against an accidental sign-extension elsewhere
    // in the FFI shim.
    deleteSpawnStruct s{};
    s.spawnId = 0xFFFFFFFFu;
    QByteArray buf(sizeof(deleteSpawnStruct), '\0');
    std::memcpy(buf.data(), &s, sizeof(deleteSpawnStruct));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());

    auto out = seq::rust::decode_delete_spawn(
        rust::Slice<const uint8_t>{data, sizeof(deleteSpawnStruct)});

    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, 0xFFFFFFFFu);
}

void RustDecodeTest::decode_delete_spawn_bad_length_returns_not_ok()
{
    const uint8_t buf[3] = {0};
    auto out = seq::rust::decode_delete_spawn(
        rust::Slice<const uint8_t>{buf, sizeof(buf)});
    QVERIFY(!out.ok);
}

// Build the smallest plausible NPC spawn payload by hand, decode it
// via Rust, and check that the FFI struct round-trips a few
// representative fields. The payload shape is the same construction
// the cargo tests exercise — this case's job is to confirm that the
// cxx-bridged std::array<*, *> field layouts agree with what Rust
// wrote, since that's the only thing this test catches that
// `cargo test` doesn't.
void RustDecodeTest::decode_spawn_field_layout_round_trip()
{
    QByteArray buf;
    auto u32le = [&](uint32_t v) {
        buf.append(static_cast<char>(v & 0xFF));
        buf.append(static_cast<char>((v >> 8) & 0xFF));
        buf.append(static_cast<char>((v >> 16) & 0xFF));
        buf.append(static_cast<char>((v >> 24) & 0xFF));
    };
    auto u8 = [&](uint8_t v) { buf.append(static_cast<char>(v)); };
    auto pad = [&](size_t n) { buf.append(QByteArray(n, '\0')); };

    buf.append("a goblin\0", 9);
    u32le(0xCAFEBABE);   // spawnId
    u8(40);              // level
    pad(16);
    u8(1);               // NPC
    u32le(0xDEADBEEF);   // miscData
    u8(0);               // otherData (no aura/title/suffix)
    pad(8);
    u8(0);               // charProperties (=0 → bodytype stays 0)
    u8(95);              // curHp
    pad(35);
    u32le(50);           // race (>12, not in special list)
    u8(7);               // holding
    u32le(3);            // deity
    u32le(111);          // guildID
    u32le(222);          // guildServerID
    u32le(5);            // class_
    u8(0);               // skip 1
    u8(11);              // state
    u8(2);               // light
    u8(0);               // skip 1
    buf.append('\0');    // empty lastName
    pad(2);
    u32le(777);          // petOwnerId
    pad(49);             // NPC=1 → skip 49
    pad(20);             // abridged equipment skip 20
    // slots 7 + 8 — 10 u32s.
    for (uint32_t v : {70u, 71u, 72u, 73u, 74u, 80u, 81u, 82u, 83u, 84u}) u32le(v);
    for (uint32_t v : {1u, 2u, 3u, 4u, 5u, 6u}) u32le(v);  // posData
    pad(8);              // unknowns
    u8(1);               // isMercenary
    pad(66);             // tail unknowns

    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_spawn(
        rust::Slice<const uint8_t>{data, static_cast<size_t>(buf.size())});

    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, 0xCAFEBABEu);
    QCOMPARE(out.level, uint8_t(40));
    QCOMPARE(out.npc, uint8_t(1));
    QCOMPARE(out.misc_data, 0xDEADBEEFu);
    QCOMPARE(out.race, 50u);
    QCOMPARE(out.guild_server_id, 222u);
    QCOMPARE(out.class_, 5u);
    QCOMPARE(out.is_mercenary, uint8_t(1));
    QCOMPARE(out.bytes_consumed, static_cast<uint32_t>(buf.size()));
    // Equipment in slot 7 (offset 35..40) populated; earlier slots zero.
    QCOMPARE(out.equip_data[0], 0u);
    QCOMPARE(out.equip_data[35], 70u);
    QCOMPARE(out.equip_data[44], 84u);
    // posData laid out in payload order.
    QCOMPARE(out.pos_data[0], 1u);
    QCOMPARE(out.pos_data[5], 6u);
    // Name preserved up to first NUL.
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(out.name.data()), 8),
             QByteArray("a goblin"));
}

void RustDecodeTest::decode_spawn_truncated_returns_not_ok()
{
    // Just a name + spawnId, then EOF — not enough for the rest of
    // the parser. Rust path must signal `ok=false` so the daemon can
    // fall back to fillSpawnStruct.
    QByteArray buf("name\0\x01\x00\x00\x00", 9);
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_spawn(
        rust::Slice<const uint8_t>{data, static_cast<size_t>(buf.size())});
    QVERIFY(!out.ok);
}

// Stage A+3 — one round-trip case per opcode. Each builds the
// payload by populating the C struct and memcpy'ing, then asserts
// the cxx-bridged Rust output reads the same fields. Bad-length
// behavior is already covered by the cargo unit tests; the FFI-side
// concern these guard against is layout drift in the std::array
// fields and accidental sign-extension or padding mismatches.

void RustDecodeTest::decode_remove_spawn_round_trip()
{
    removeSpawnStruct s{};
    s.spawnId = 42;
    s.removeSpawn = 1;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_remove_spawn(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, 42u);
    QCOMPARE(out.remove_spawn, uint8_t(1));
}

void RustDecodeTest::decode_hp_update_round_trip()
{
    hpNpcUpdateStruct s{};
    s.spawnId = 0xBEEF;
    s.curHP = -1234;
    s.maxHP = 5678;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_hp_update(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, uint16_t(0xBEEF));
    QCOMPARE(out.cur_hp, -1234);
    QCOMPARE(out.max_hp, 5678);
}

void RustDecodeTest::decode_mob_health_round_trip()
{
    mobHealthStruct s{};
    s.spawnId = 199;
    s.hpPercent = 100;  // value is a percentage, not raw HP
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_mob_health(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, uint16_t(199));
    QCOMPARE(out.hp_percent, 100);
}

void RustDecodeTest::decode_spawn_appearance_round_trip()
{
    spawnAppearanceStruct s{};
    s.spawnId = 50;
    s.type = 14;          // anim subcommand
    s.parameter = 110;    // sitting
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_spawn_appearance(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, uint16_t(50));
    QCOMPARE(out.kind, uint16_t(14));
    QCOMPARE(out.parameter, 110u);
}

void RustDecodeTest::decode_exp_update_round_trip()
{
    expUpdateStruct s{};
    s.exp = 97900;        // pre-level sample from confirmation log
    s.type = 2;           // 0=set, 2=update
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_exp_update(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.exp, 97900u);
    QCOMPARE(out.kind, 2u);
}

void RustDecodeTest::decode_level_update_round_trip()
{
    levelUpUpdateStruct s{};
    s.level = 2;
    s.levelOld = 1;
    s.exp = 814;          // post-level sample from confirmation log
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_level_update(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.level, 2u);
    QCOMPARE(out.level_old, 1u);
    QCOMPARE(out.exp, 814u);
}

void RustDecodeTest::decode_skill_update_round_trip()
{
    skillIncStruct s{};
    s.skillId = 30;       // H2H
    s.value = 12;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_skill_update(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.skill_id, 30u);
    QCOMPARE(out.value, 12);
}

// Stage A+4 round-trip cases — same shape as A+3 round-trips: build
// the C struct, memcpy, decode via Rust, compare a representative
// subset of fields. cargo tests cover bad-length and edge cases.

void RustDecodeTest::decode_mana_change_round_trip()
{
    manaDecrementStruct s{};
    s.newMana = -100;
    s.spellId = 42;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_mana_change(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.new_mana, -100);
    QCOMPARE(out.spell_id, 42);
}

void RustDecodeTest::decode_stamina_round_trip()
{
    staminaStruct s{};
    s.food = 127; s.water = 0;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_stamina(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.food, 127u);
    QCOMPARE(out.water, 0u);
}

void RustDecodeTest::decode_end_update_round_trip()
{
    endUpdateStruct s{};
    s.spawn_id = 0x1234;
    s.cur = 94; s.max = 100;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_end_update(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, uint16_t(0x1234));
    QCOMPARE(out.cur, 94u);
    QCOMPARE(out.max, 100u);
}

void RustDecodeTest::decode_consider_round_trip()
{
    considerStruct s{};
    s.playerid = 100; s.targetid = 200; s.faction = -1; s.level = 50;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_consider(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.player_id, 100u);
    QCOMPARE(out.faction, -1);
}

void RustDecodeTest::decode_spawn_rename_round_trip()
{
    spawnRenameStruct s{};
    std::strcpy(s.old_name,       "Foo");
    std::strcpy(s.old_name_again, "Foo");
    std::strcpy(s.new_name,       "Bar");
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_spawn_rename(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(out.old_name.data()), 3),
             QByteArray("Foo"));
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(out.new_name.data()), 3),
             QByteArray("Bar"));
}

void RustDecodeTest::decode_client_target_round_trip()
{
    clientTargetStruct s{};
    s.newTarget = 0xDEADBEEFu;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_client_target(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.new_target, 0xDEADBEEFu);
}

void RustDecodeTest::decode_death_round_trip()
{
    newCorpseStruct s{};
    s.spawnId = 111; s.killerId = 222; s.corpseid = 333; s.type = 1;
    s.spellId = 666; s.zoneId = 77; s.zoneInstance = 8; s.damage = 500;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_death(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, 111u);
    QCOMPARE(out.killer_id, 222u);
    QCOMPARE(out.kind, 1);
    QCOMPARE(out.damage, 500u);
}

// QTEST_GUILESS_MAIN — daemon code is headless (QCoreApplication only),
// no display required.
QTEST_GUILESS_MAIN(RustDecodeTest)
#include "rustdecode_test.moc"
