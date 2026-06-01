/*
 *  boxregistry_test.cpp
 *  Tier-1 unit test for BoxRegistry — the multibox identity layer.
 *
 *  Covers the wire-key observation path (observe / lookupByWorld) and the
 *  two-layer identity logic that drives the picker: promoteByName (placeholder
 *  -> name-hash id), re-handshake merge, currentBoxFor (latest box of a
 *  character), the adopt-active-from-stale-placeholder rule, the active-box
 *  switch signal, and the zone-binding lookups (lookupBoundZone +
 *  lookupByExpectedZone recency tiebreak / skip-bound). No network, no
 *  streams — Box fields the daemon's observers populate are set directly.
 */

#include <QtTest/QtTest>

#include <netinet/in.h>

#include "boxregistry.h"

namespace {
// Distinct same-host 5-tuples: one client IP, varying client world ports,
// one world server port (the :9016 case that motivated multibox).
constexpr in_addr_t kClientIp = 0x0a0d0008;  // 10.13.0.8
constexpr in_port_t kSrvPort  = 0x789c;      // htons(9016)-ish; value only

in_port_t cport(uint16_t p) { return htons(p); }
}

class BoxRegistryTest : public QObject
{
  Q_OBJECT

private slots:
  void observeCreatesPrimary();
  void observeSameTupleUpdates();
  void observeDistinctCreatesSecond();
  void lookupByWorldFindsAndMisses();
  void promoteByNameStampsHashId();
  void promoteByNameMergesRelog();
  void currentBoxForReturnsLatest();
  void adoptActiveFromStalePlaceholder();
  void setActiveBoxEmitsChange();
  void lookupBoundZoneMatches();
  void lookupByExpectedZoneRecencyAndSkipBound();
};

void BoxRegistryTest::observeCreatesPrimary()
{
  BoxRegistry reg;
  Box* b = reg.observe(kClientIp, cport(1000), kSrvPort, 1);
  QVERIFY(b != nullptr);
  QVERIFY(b->is_primary);
  QCOMPARE(b->packet_count, quint64(1));
  QCOMPARE(reg.size(), size_t(1));
  QCOMPARE(reg.distinctCount(), size_t(1));
  QVERIFY(b->display_name.isEmpty());           // not promoted yet
  QVERIFY(b->box_id.startsWith(QStringLiteral("p-")));  // placeholder id
  QCOMPARE(reg.activeBoxId(), b->box_id);        // first box is active
}

void BoxRegistryTest::observeSameTupleUpdates()
{
  BoxRegistry reg;
  Box* a = reg.observe(kClientIp, cport(1000), kSrvPort, 1);
  Box* b = reg.observe(kClientIp, cport(1000), kSrvPort, 2);
  QCOMPARE(a, b);                                // same Box, not a new one
  QCOMPARE(b->packet_count, quint64(2));
  QCOMPARE(reg.size(), size_t(1));
}

void BoxRegistryTest::observeDistinctCreatesSecond()
{
  BoxRegistry reg;
  Box* a = reg.observe(kClientIp, cport(1000), kSrvPort, 1);
  Box* b = reg.observe(kClientIp, cport(1001), kSrvPort, 2);
  QVERIFY(a != b);
  QVERIFY(a->is_primary);
  QVERIFY(!b->is_primary);                       // only the first is primary
  QCOMPARE(reg.size(), size_t(2));
  QCOMPARE(reg.distinctCount(), size_t(2));
}

void BoxRegistryTest::lookupByWorldFindsAndMisses()
{
  BoxRegistry reg;
  Box* a = reg.observe(kClientIp, cport(1000), kSrvPort, 1);
  QCOMPARE(reg.lookupByWorld(kClientIp, cport(1000), kSrvPort), a);
  QVERIFY(reg.lookupByWorld(kClientIp, cport(9999), kSrvPort) == nullptr);
}

void BoxRegistryTest::promoteByNameStampsHashId()
{
  BoxRegistry reg;
  Box* b = reg.observe(kClientIp, cport(1000), kSrvPort, 1);
  const QString placeholder = b->box_id;

  Box* parent = reg.promoteByName(b, QStringLiteral("Alpha"));
  QVERIFY(parent == nullptr);                    // first sighting: no merge
  QCOMPARE(b->display_name, QStringLiteral("Alpha"));
  QVERIFY(b->box_id.startsWith(QStringLiteral("b-")));  // stable name hash
  QVERIFY(b->box_id != placeholder);
  QVERIFY(!b->is_merged());
  QCOMPARE(reg.distinctCount(), size_t(1));
  // findById tracks the new id; the old placeholder is gone.
  QCOMPARE(reg.findById(b->box_id), b);
  QVERIFY(reg.findById(placeholder) == nullptr);
}

void BoxRegistryTest::promoteByNameMergesRelog()
{
  BoxRegistry reg;
  Box* first  = reg.observe(kClientIp, cport(1000), kSrvPort, 1);
  Box* second = reg.observe(kClientIp, cport(1001), kSrvPort, 2);
  reg.promoteByName(first,  QStringLiteral("Alpha"));

  // Same character re-handshakes on a new world socket (zone change).
  Box* parent = reg.promoteByName(second, QStringLiteral("Alpha"));
  QCOMPARE(parent, first);                        // merged into the first box
  QVERIFY(second->is_merged());
  QCOMPARE(second->merged_into, first->box_id);
  QCOMPARE(reg.distinctCount(), size_t(1));       // one character, two boxes
  QCOMPARE(reg.size(), size_t(2));
}

void BoxRegistryTest::currentBoxForReturnsLatest()
{
  BoxRegistry reg;
  Box* first  = reg.observe(kClientIp, cport(1000), kSrvPort, 10);
  Box* second = reg.observe(kClientIp, cport(1001), kSrvPort, 20);  // later
  reg.promoteByName(first,  QStringLiteral("Alpha"));
  reg.promoteByName(second, QStringLiteral("Alpha"));

  // The character's live box is the most recently seen one (the new zone
  // session), reachable via any of the character's box_ids.
  QCOMPARE(reg.currentBoxFor(first->box_id), second);
}

void BoxRegistryTest::adoptActiveFromStalePlaceholder()
{
  BoxRegistry reg;
  // First box is a partial/dead session that never resolves a character
  // (daemon started mid-session) — it's active but stays a placeholder.
  Box* stale = reg.observe(kClientIp, cport(1000), kSrvPort, 1);
  QCOMPARE(reg.activeBoxId(), stale->box_id);

  // A real distinct character then decodes on another box. Because the
  // active box is an unpromoted placeholder, the registry adopts the newly
  // decoded character as active (else SessionAdapter streams an empty box).
  Box* real = reg.observe(kClientIp, cport(1001), kSrvPort, 2);
  reg.promoteByName(real, QStringLiteral("Beta"));
  QCOMPARE(reg.activeBoxId(), real->box_id);
}

void BoxRegistryTest::setActiveBoxEmitsChange()
{
  BoxRegistry reg;
  Box* a = reg.observe(kClientIp, cport(1000), kSrvPort, 1);
  Box* b = reg.observe(kClientIp, cport(1001), kSrvPort, 2);
  reg.promoteByName(a, QStringLiteral("Alpha"));
  reg.promoteByName(b, QStringLiteral("Beta"));

  Box* spyOld = nullptr;
  Box* spyNew = nullptr;
  int  changes = 0;
  QObject::connect(&reg, &BoxRegistry::activeBoxChanged,
                   [&](Box* o, Box* n) { spyOld = o; spyNew = n; ++changes; });

  QVERIFY(reg.setActiveBoxId(b->box_id));
  QCOMPARE(reg.activeBoxId(), b->box_id);
  QCOMPARE(changes, 1);
  QCOMPARE(spyOld, a);
  QCOMPARE(spyNew, b);

  // Re-selecting the same box is a no-op (returns true, no new signal).
  QVERIFY(reg.setActiveBoxId(b->box_id));
  QCOMPARE(changes, 1);

  // Unknown id fails.
  QVERIFY(!reg.setActiveBoxId(QStringLiteral("b-nope")));
}

void BoxRegistryTest::lookupBoundZoneMatches()
{
  BoxRegistry reg;
  Box* a = reg.observe(kClientIp, cport(1000), kSrvPort, 1);
  a->zone_client_port       = cport(5555);
  a->zone_server_port_bound = cport(7000);

  QCOMPARE(reg.lookupBoundZone(kClientIp, cport(5555), cport(7000)), a);
  QVERIFY(reg.lookupBoundZone(kClientIp, cport(9999), cport(7000)) == nullptr);
}

void BoxRegistryTest::lookupByExpectedZoneRecencyAndSkipBound()
{
  BoxRegistry reg;
  Box* a = reg.observe(kClientIp, cport(1000), kSrvPort, 1);
  Box* b = reg.observe(kClientIp, cport(1001), kSrvPort, 2);

  // Both await a zone on the same server port (ZoneServerObserver defers IP
  // resolution, so expected_zone_server_ip stays 0 -> port-only match).
  a->expected_zone_server_port = cport(7000);
  a->zone_await_ms             = 100;
  b->expected_zone_server_port = cport(7000);
  b->zone_await_ms             = 200;            // notified more recently

  // The most recently notified box owns the SessionRequest arriving now.
  QCOMPARE(reg.lookupByExpectedZone(kClientIp, 0x01020304, cport(7000)), b);

  // Once b binds a live zone session it's skipped, so a wins.
  b->zone_client_port = cport(6666);
  QCOMPARE(reg.lookupByExpectedZone(kClientIp, 0x01020304, cport(7000)), a);
}

QTEST_GUILESS_MAIN(BoxRegistryTest)
#include "boxregistry_test.moc"
