// Tier-1 FFI roundtrip test for the Stage A Rust decoder.
//
// Calls seq::rust::decode_mob_update() across the cxx bridge and
// asserts the returned MobUpdate matches the same buffer parsed
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
    void decode_click_object_round_trip();
    void decode_illusion_round_trip();
    void decode_buff_round_trip();
    void decode_action2_round_trip();
    void decode_zone_point_round_trip();
    void decode_simple_message_round_trip();
    void decode_formatted_message_round_trip();
    void decode_special_message_round_trip();
    void decode_channel_message_round_trip();
    void decode_new_zone_round_trip();
    void decode_player_profile_round_trip();
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

// Stage A+5
void RustDecodeTest::decode_click_object_round_trip()
{
    remDropStruct s{};
    s.dropId = 0xCAFE; s.spawnId = 0xBEEF;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_click_object(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.drop_id, uint16_t(0xCAFE));
    QCOMPARE(out.spawn_id, uint16_t(0xBEEF));
}

void RustDecodeTest::decode_illusion_round_trip()
{
    spawnIllusionStruct s{};
    s.spawnId = 100;
    std::strcpy(s.name, "Goblin");
    s.race = 75; s.gender = 1; s.texture = 5; s.helm = 2; s.face = 42;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_illusion(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, 100u);
    QCOMPARE(QByteArray(reinterpret_cast<const char*>(out.name.data()), 6),
             QByteArray("Goblin"));
    QCOMPARE(out.race, 75u);
    QCOMPARE(out.gender, uint8_t(1));
    QCOMPARE(out.face, 42u);
}

void RustDecodeTest::decode_buff_round_trip()
{
    buffStruct s{};
    s.spawnid = 123; s.spellid = 5024; s.duration = 3600;
    s.level = 60; s.spellslot = 3; s.changetype = 2;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_buff(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.spawn_id, 123u);
    QCOMPARE(out.spell_id, 5024u);
    QCOMPARE(out.duration, 3600u);
    QCOMPARE(out.level, uint8_t(60));
    QCOMPARE(out.spell_slot, 3u);
    QCOMPARE(out.change_type, 2u);
}

void RustDecodeTest::decode_action2_round_trip()
{
    action2Struct s{};
    s.target = 100; s.source = 200; s.damage = 42; s.spell = -1; s.type = 7;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_action2(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.target, uint16_t(100));
    QCOMPARE(out.source, uint16_t(200));
    QCOMPARE(out.damage, 42);
    QCOMPARE(out.spell, -1);
    QCOMPARE(out.kind, uint8_t(7));
}

void RustDecodeTest::decode_zone_point_round_trip()
{
    zonePointStruct s{};
    s.zoneTrigger  = 7;
    s.y            = 1.5f;
    s.x            = 2.5f;
    s.z            = 3.5f;
    s.heading      = 90.0f;
    s.zoneId       = 57;
    s.zoneInstance = 3;
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_zone_point(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.zone_trigger, 7u);
    QCOMPARE(out.y, 1.5f);
    QCOMPARE(out.x, 2.5f);
    QCOMPARE(out.z, 3.5f);
    QCOMPARE(out.heading, 90.0f);
    QCOMPARE(out.zone_id, uint16_t(57));
    QCOMPARE(out.zone_instance, uint16_t(3));
}

void RustDecodeTest::decode_simple_message_round_trip()
{
    simpleMessageStruct s{};
    s.messageFormat = 12345;
    s.messageColor  = static_cast<ChatColor>(0x12);
    QByteArray buf(sizeof(s), '\0');
    std::memcpy(buf.data(), &s, sizeof(s));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_simple_message(
        rust::Slice<const uint8_t>{data, sizeof(s)});
    QVERIFY(out.ok);
    QCOMPARE(out.message_format, 12345u);
    QCOMPARE(out.message_color, 0x12u);
}

void RustDecodeTest::decode_formatted_message_round_trip()
{
    // Just the header — variable-length messages blob is handled
    // daemon-side via offsetof(messages).
    QByteArray buf(offsetof(formattedMessageStruct, messages), '\0');
    formattedMessageStruct hdr{};
    hdr.messageFormat = 999;
    hdr.messageColor  = static_cast<ChatColor>(0x1a);
    std::memcpy(buf.data(), &hdr, buf.size());
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_formatted_message(
        rust::Slice<const uint8_t>{data, static_cast<size_t>(buf.size())});
    QVERIFY(out.ok);
    QCOMPARE(out.message_format, 999u);
    QCOMPARE(out.message_color, 0x1au);
}

void RustDecodeTest::decode_special_message_round_trip()
{
    // Manual layout matching specialMessageStruct: unknown[3], color,
    // target, padding, source\0, unknown0xxx[3], message\0.
    QByteArray buf;
    buf.resize(11, '\0');
    uint32_t color = 0x05;
    uint16_t target = 42;
    std::memcpy(buf.data() + 3, &color, 4);
    std::memcpy(buf.data() + 7, &target, 2);
    buf.append("Soandso");
    buf.append(char(0));
    buf.append(12, char(0)); // unknown0xxx[3]
    buf.append("hello world");
    buf.append(char(0));
    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_special_message(
        rust::Slice<const uint8_t>{data, static_cast<size_t>(buf.size())});
    QVERIFY(out.ok);
    QCOMPARE(out.message_color, 0x05u);
    QCOMPARE(out.target, uint16_t(42));
    QCOMPARE(QString::fromStdString(std::string(out.source)),
             QStringLiteral("Soandso"));
    QCOMPARE(QString::fromStdString(std::string(out.message)),
             QStringLiteral("hello world"));
}

void RustDecodeTest::decode_channel_message_round_trip()
{
    // NetStream layout: sender\0, target\0, 8 skip, language u32,
    // chanNum u32, 4 skip, 1 skip, skill u32, message\0.
    QByteArray buf;
    buf.append("Alice");        buf.append(char(0));
    buf.append("Bob");           buf.append(char(0));
    buf.append(8, char(0));
    const uint32_t language = 0, chan = 14 /*MT_Tell*/, skill = 100;
    buf.append(reinterpret_cast<const char*>(&language), 4);
    buf.append(reinterpret_cast<const char*>(&chan), 4);
    buf.append(4, char(0));
    buf.append(1, char(0));
    buf.append(reinterpret_cast<const char*>(&skill), 4);
    buf.append("private msg");   buf.append(char(0));

    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_channel_message(
        rust::Slice<const uint8_t>{data, static_cast<size_t>(buf.size())});
    QVERIFY(out.ok);
    QCOMPARE(QString::fromStdString(std::string(out.sender)),
             QStringLiteral("Alice"));
    QCOMPARE(QString::fromStdString(std::string(out.target)),
             QStringLiteral("Bob"));
    QCOMPARE(out.chan_num, uint32_t(14));
    QCOMPARE(out.skill_in_language, uint32_t(100));
    QCOMPARE(QString::fromStdString(std::string(out.message)),
             QStringLiteral("private msg"));
}

void RustDecodeTest::decode_new_zone_round_trip()
{
    // NetStream layout: short\0, long\0, 2 skip, zonefile\0, 90 skip,
    // expMult f32, 28 skip, safeY/X/Z f32.
    QByteArray buf;
    buf.append("ecommons");          buf.append(char(0));
    buf.append("East Commonlands");  buf.append(char(0));
    buf.append(2, char(0));
    buf.append("ecommons");          buf.append(char(0));
    buf.append(90, char(0));
    const float expMult = 1.0f;
    buf.append(reinterpret_cast<const char*>(&expMult), 4);
    buf.append(28, char(0));
    const float sy = -100.0f, sx = 200.0f, sz = -50.0f;
    buf.append(reinterpret_cast<const char*>(&sy), 4);
    buf.append(reinterpret_cast<const char*>(&sx), 4);
    buf.append(reinterpret_cast<const char*>(&sz), 4);

    const uint8_t* data = reinterpret_cast<const uint8_t*>(buf.constData());
    auto out = seq::rust::decode_new_zone(
        rust::Slice<const uint8_t>{data, static_cast<size_t>(buf.size())});
    QVERIFY(out.ok);
    QCOMPARE(QString::fromStdString(std::string(out.short_name)),
             QStringLiteral("ecommons"));
    QCOMPARE(QString::fromStdString(std::string(out.long_name)),
             QStringLiteral("East Commonlands"));
    QCOMPARE(out.zone_exp_multiplier, 1.0f);
    QCOMPARE(out.safe_y, -100.0f);
    QCOMPARE(out.safe_x, 200.0f);
    QCOMPARE(out.safe_z, -50.0f);
}

// Helpers for the player_profile round-trip — push primitives onto a
// growing byte buffer in EQ wire byte-orders.
static void appU32LE(QByteArray& b, uint32_t v) {
    char tmp[4]; std::memcpy(tmp, &v, 4); b.append(tmp, 4);
}
static void appU16LE(QByteArray& b, uint16_t v) {
    char tmp[2]; std::memcpy(tmp, &v, 2); b.append(tmp, 2);
}
static void appU16BE(QByteArray& b, uint16_t v) {
    b.append(char((v >> 8) & 0xff));
    b.append(char(v & 0xff));
}
static void appF32(QByteArray& b, float v) {
    char tmp[4]; std::memcpy(tmp, &v, 4); b.append(tmp, 4);
}

void RustDecodeTest::decode_player_profile_round_trip()
{
    QByteArray b;
    appU32LE(b, 0xCAFEBABE);              // checksum
    b.append(16, char(0));                // skip 16
    b.append(char(1));                    // gender
    appU32LE(b, 7);                       // race
    appU32LE(b, 3);                       // class_
    b.append(char(65));                   // level
    b.append(char(65));                   // level1
    // bind count = 1, one BindStruct
    appU32LE(b, 1);
    appU32LE(b, 222);                     // zoneId
    appF32(b, 1.0f);                      // x
    appF32(b, 2.0f);                      // y
    appF32(b, 3.0f);                      // z
    appF32(b, 4.0f);                      // heading
    appU32LE(b, 11);                      // deity
    appU32LE(b, 0);                       // intoxication
    appU32LE(b, 0);                       // refresh count
    appU32LE(b, 0);                       // equip count
    appU32LE(b, 0);                       // sc0
    appU32LE(b, 0);                       // sc1
    appU32LE(b, 0);                       // sc2
    b.append(51, char(0));                // 51b skip
    appU32LE(b, 0);                       // points
    appU32LE(b, 2000);                    // MANA
    appU32LE(b, 4000);                    // curHp
    for (uint32_t v : {120u, 121u, 122u, 123u, 124u, 125u, 126u})
        appU32LE(b, v);                   // STR..WIS
    b.append(28, char(0));                // 28b skip
    appU32LE(b, 1);                       // aa_count
    appU32LE(b, 800);                     // AA
    appU32LE(b, 1);                       // value
    appU32LE(b, 0);                       // unknown008
    appU32LE(b, 0);                       // skills count (skipped on parser side)
    appU32LE(b, 0);                       // sc3
    appU32LE(b, 0);                       // discipline count
    appU32LE(b, 0);                       // sc4
    b.append(4, char(0));                 // 4b skip
    appU32LE(b, 0);                       // recast count
    appU32LE(b, 0);                       // sc5
    appU32LE(b, 0);                       // spellbook count
    appU32LE(b, 0);                       // mem spells count
    appU32LE(b, 0);                       // refresh2 count
    b.append(char(0));                    // 1b skip
    appU32LE(b, 1);                       // buff count
    {
        QByteArray buff(110, char(0));
        int32_t duration = 1200;
        int32_t spellid  = 5042;
        std::memcpy(buff.data() + 12, &duration, 4);
        std::memcpy(buff.data() + 21, &spellid, 4);
        b.append(buff);
    }
    // money on player
    for (uint32_t v : {50u, 60u, 70u, 80u}) appU32LE(b, v);
    // money on cursor
    for (uint32_t v : {5u, 6u, 7u, 8u}) appU32LE(b, v);
    b.append(20, char(0));                // 20b skip
    appU32LE(b, 333);                     // aa_spent
    b.append(4, char(0));                 // 4b skip
    appU32LE(b, 444);                     // aa_assigned
    b.append(20, char(0));                // 20b skip
    appU32LE(b, 555);                     // aa_unspent
    b.append(2, char(0));                 // 2b skip
    appU32LE(b, 0);                       // bandolier count
    b.append(80, char(0));                // 80b skip
    appU32LE(b, 666);                     // endurance
    b.append(58, char(0));                // 58b skip
    appU32LE(b, 22846);                   // expAA
    b.append(8, char(0));                 // 8b skip
    // name: u32 len=8, then 8 bytes
    appU32LE(b, 8);
    b.append("Hero\0\0\0\0", 8);
    // lastName: u32 len=8, then 8 bytes
    appU32LE(b, 8);
    b.append("Stark\0\0\0", 8);
    appU32LE(b, 1000);                    // birthdayTime
    appU32LE(b, 2000);                    // accountCreateDate
    appU32LE(b, 3000);                    // lastSaveTime
    appU32LE(b, 4000);                    // timePlayedMin
    b.append(4, char(0));                 // 4b skip
    appU32LE(b, 0xFFu);                   // expansions
    b.append(4, char(0));                 // 4b skip
    appU32LE(b, 1);                       // lang count
    b.append(char(100));
    appU16LE(b, 99);                      // zoneId
    appU16LE(b, 0);                       // zoneInstance
    appF32(b, -77.0f);                    // y
    appF32(b, 88.5f);                     // x
    appF32(b, 10.0f);                     // z
    appF32(b, 45.0f);                     // heading
    appU16BE(b, 100);                     // standState
    appU16BE(b, 0);                       // anon
    appU32LE(b, 7777);                    // guildID
    appU32LE(b, 2);                       // guildServerID
    b.append(2, char(0));                 // 2b skip
    for (uint32_t v : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u})
        appU32LE(b, v);                   // inventory + bank + shared
    appU32LE(b, 0);                       // sc6
    b.append(8, char(0));                 // 8b skip
    appU32LE(b, 0);                       // careerTribute
    b.append(4, char(0));                 // 4b skip
    appU32LE(b, 0);                       // currentTribute
    b.append(6, char(0));                 // 6b skip
    appU32LE(b, 0);                       // tribute count
    appU32LE(b, 0);                       // sc7
    b.append(137, char(0));               // 137b skip
    appU32LE(b, 11);                      // currentRadCrystals
    appU32LE(b, 12);                      // careerRadCrystals
    appU32LE(b, 13);                      // currentEbonCrystals
    appU32LE(b, 14);                      // careerEbonCrystals
    b.append(91, char(0));                // 91b skip
    b.append(char(1));                    // autosplit
    b.append(57, char(0));                // 57b skip
    for (uint32_t v : {31u, 32u, 33u, 34u, 35u, 36u})
        appU32LE(b, v);                   // LDoN points

    auto out = seq::rust::decode_player_profile(rust::Slice<const uint8_t>{
        reinterpret_cast<const uint8_t*>(b.constData()),
        static_cast<size_t>(b.size())});
    QVERIFY(out.ok);
    QCOMPARE(out.checksum, uint32_t(0xCAFEBABE));
    QCOMPARE(out.gender, uint8_t(1));
    QCOMPARE(out.race, uint32_t(7));
    QCOMPARE(out.class_, uint32_t(3));
    QCOMPARE(out.level, uint8_t(65));
    QCOMPARE(out.bind0_zone_id, uint32_t(222));
    QCOMPARE(out.deity, uint32_t(11));
    QCOMPARE(out.mana, uint32_t(2000));
    QCOMPARE(out.cur_hp, uint32_t(4000));
    QCOMPARE(out.str_, uint32_t(120));
    QCOMPARE(out.wis, uint32_t(126));
    QCOMPARE(out.aa_ids.size(), size_t(1));
    QCOMPARE(out.aa_ids[0], uint32_t(800));
    QCOMPARE(out.aa_values[0], uint32_t(1));
    QCOMPARE(out.buff_spell_ids.size(), size_t(1));
    QCOMPARE(out.buff_spell_ids[0], int32_t(5042));
    QCOMPARE(out.buff_durations[0], int32_t(1200));
    QCOMPARE(out.platinum, uint32_t(50));
    QCOMPARE(out.aa_spent, uint32_t(333));
    QCOMPARE(out.aa_unspent, uint32_t(555));
    QCOMPARE(out.endurance, uint32_t(666));
    QCOMPARE(out.exp_aa, uint32_t(22846));
    QCOMPARE(QString::fromStdString(std::string(out.name)),
             QStringLiteral("Hero"));
    QCOMPARE(QString::fromStdString(std::string(out.last_name)),
             QStringLiteral("Stark"));
    QCOMPARE(out.expansions, uint32_t(0xFF));
    QCOMPARE(out.languages.size(), size_t(1));
    QCOMPARE(out.languages[0], uint8_t(100));
    QCOMPARE(out.zone_id, uint16_t(99));
    QCOMPARE(out.x, 88.5f);
    QCOMPARE(out.y, -77.0f);
    QCOMPARE(out.heading, 45.0f);
    QCOMPARE(out.stand_state, uint16_t(100));
    QCOMPARE(out.guild_id, uint32_t(7777));
    QCOMPARE(out.platinum_inventory, uint32_t(1));
    QCOMPARE(out.platinum_bank, uint32_t(5));
    QCOMPARE(out.platinum_shared, uint32_t(9));
    QCOMPARE(out.current_rad_crystals, uint32_t(11));
    QCOMPARE(out.career_ebon_crystals, uint32_t(14));
    QCOMPARE(out.autosplit, uint8_t(1));
    QCOMPARE(out.ldon_guk_points, uint32_t(31));
    QCOMPARE(out.ldon_avail_points, uint32_t(36));
    QCOMPARE(int(out.bytes_consumed), b.size());
}

// QTEST_GUILESS_MAIN — daemon code is headless (QCoreApplication only),
// no display required.
QTEST_GUILESS_MAIN(RustDecodeTest)
#include "rustdecode_test.moc"
