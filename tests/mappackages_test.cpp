#include <QtTest/QtTest>

#include <vector>

#include "protoencoder.h"
#include "mappackagehost.h"

using namespace seq::encode;

// Tier-1 unit test for the map-package proto encoder. The discovery +
// active->default fallback *resolution* logic lives in DaemonApp
// (src/daemonapp.cpp), which is part of the daemon executable target rather
// than seq-daemon-core, so it isn't reachable from a tier-1 test without
// CMake restructuring — covered here is the pure encoder hop that IS in the
// core library. See the report / follow-up note for the discovery+fallback
// gap.
class MapPackagesTest : public QObject {
    Q_OBJECT
private slots:
    void emptyWhenNoPackages();
    void copiesIdLabelZoneCountAndActive();
    void defaultActiveId();
};

void MapPackagesTest::emptyWhenNoPackages()
{
    seq::v1::MapPackagesUpdate out;
    fillMapPackages(&out, {}, QStringLiteral("default"));
    QCOMPARE(out.packages_size(), 0);
    QCOMPARE(QString::fromStdString(out.active_id()), QStringLiteral("default"));
}

void MapPackagesTest::copiesIdLabelZoneCountAndActive()
{
    std::vector<MapPackageInfo> pkgs = {
        {QStringLiteral("default"), QStringLiteral("default"), 7},
        {QStringLiteral("brewall"), QStringLiteral("brewall"), 3},
    };
    seq::v1::MapPackagesUpdate out;
    fillMapPackages(&out, pkgs, QStringLiteral("brewall"));

    QCOMPARE(out.packages_size(), 2);
    QCOMPARE(QString::fromStdString(out.packages(0).id()),
             QStringLiteral("default"));
    QCOMPARE(QString::fromStdString(out.packages(0).label()),
             QStringLiteral("default"));
    QCOMPARE(out.packages(0).zone_count(), 7u);
    QCOMPARE(QString::fromStdString(out.packages(1).id()),
             QStringLiteral("brewall"));
    QCOMPARE(out.packages(1).zone_count(), 3u);
    QCOMPARE(QString::fromStdString(out.active_id()),
             QStringLiteral("brewall"));
}

void MapPackagesTest::defaultActiveId()
{
    std::vector<MapPackageInfo> pkgs = {
        {QStringLiteral("default"), QStringLiteral("default"), 0},
    };
    seq::v1::MapPackagesUpdate out;
    fillMapPackages(&out, pkgs, QStringLiteral("default"));
    QCOMPARE(out.packages_size(), 1);
    QCOMPARE(out.packages(0).zone_count(), 0u);
    QCOMPARE(QString::fromStdString(out.active_id()), QStringLiteral("default"));
}

QTEST_MAIN(MapPackagesTest)
#include "mappackages_test.moc"
