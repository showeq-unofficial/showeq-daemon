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

// QTEST_GUILESS_MAIN — daemon code is headless (QCoreApplication only),
// no display required.
QTEST_GUILESS_MAIN(RustDecodeTest)
#include "rustdecode_test.moc"
