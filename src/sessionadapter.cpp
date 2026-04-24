#include "sessionadapter.h"

#include <QByteArray>
#include <QDateTime>
#include <QLoggingCategory>
#include <QSet>
#include <QWebSocket>

#include "group.h"
#include "mapcore.h"
#include "messageshell.h"
#include "player.h"
#include "protoencoder.h"
#include "spawn.h"
#include "spawnshell.h"
#include "zonemgr.h"

#include "seq/v1/client.pb.h"

SessionAdapter::SessionAdapter(QWebSocket*   sock,
                               SpawnShell*   spawnShell,
                               ZoneMgr*      zoneMgr,
                               Player*       player,
                               MapData*      mapData,
                               MessageShell* messageShell,
                               GroupMgr*     groupMgr,
                               QObject*      parent)
    : QObject(parent)
    , m_sock(sock)
    , m_spawnShell(spawnShell)
    , m_zoneMgr(zoneMgr)
    , m_player(player)
    , m_mapData(mapData)
    , m_messageShell(messageShell)
    , m_groupMgr(groupMgr)
{
    connect(sock, &QWebSocket::textMessageReceived,
            this, &SessionAdapter::onTextMessage);
    connect(sock, &QWebSocket::binaryMessageReceived,
            this, &SessionAdapter::onBinaryMessage);
}

SessionAdapter::~SessionAdapter() = default;

void SessionAdapter::onTextMessage(const QString& text)
{
    qInfo("ws text frame ignored: %s", qUtf8Printable(text));
}

void SessionAdapter::onBinaryMessage(const QByteArray& bytes)
{
    seq::v1::ClientEnvelope env;
    if (!env.ParseFromArray(bytes.constData(), bytes.size())) {
        qWarning("malformed ClientEnvelope (%lld bytes)",
                 static_cast<long long>(bytes.size()));
        return;
    }
    if (env.has_subscribe()) {
        if (m_subscribed) {
            qInfo("duplicate Subscribe ignored");
            return;
        }
        m_subscribed = true;
        startStreaming();
    }
}

// Phase 1 ignores the topic set and always starts a full spawn/zone/player
// stream. Finer-grained subscription lands when Chat / Combat / Exp / Group
// messages are populated in later phases.
void SessionAdapter::startStreaming()
{
    if (!m_spawnShell) {
        qWarning("Subscribe received before state managers were wired");
        return;
    }

    // STEP 1: connect signals — handlers push into m_buffered until the
    //         snapshot is drained. This MUST happen before any iteration
    //         of SpawnShell state so mid-iteration changes are captured.
    connect(m_spawnShell, &SpawnShell::addItem,
            this,         &SessionAdapter::onAddItem);
    connect(m_spawnShell, &SpawnShell::delItem,
            this,         &SessionAdapter::onDelItem);
    connect(m_spawnShell, &SpawnShell::changeItem,
            this,         &SessionAdapter::onChangeItem);
    // Player emits its own changeItem on per-tick position/heading updates
    // (player.cpp:932 — tSpawnChangedPosition); SpawnShell forwards a few
    // bigger transitions but not the per-tick movement. Connect both so
    // the player marker tracks live.
    if (m_player) {
        connect(m_player, &Player::changeItem,
                this,     &SessionAdapter::onChangeItem);
    }
    // `killSpawn`, `zoneBegin`, `zoneChanged` each have same-name slots on
    // their source classes (SpawnShell::killSpawn(const uint8_t*), etc.),
    // which confuses the compile-time &Class::member form. Fall back to
    // the string-based SIGNAL()/SLOT() connect for these three.
    connect(m_spawnShell,
            SIGNAL(killSpawn(const Item*, const Item*, uint16_t)),
            this,
            SLOT(onKillSpawn(const Item*, const Item*, uint16_t)));
    if (m_zoneMgr) {
        connect(m_zoneMgr, SIGNAL(zoneBegin(const QString&)),
                this,      SLOT(onZoneBegin(const QString&)));
        connect(m_zoneMgr, SIGNAL(zoneChanged(const QString&)),
                this,      SLOT(onZoneChanged(const QString&)));
    }

    // Player stat signals — every one of these gets coalesced into a
    // single PlayerStats envelope via onPlayerStatsChanged. Slot has no
    // arguments; Qt5 quietly drops the signals' extra args.
    if (m_player) {
        connect(m_player, SIGNAL(hpChanged(int16_t, int16_t)),
                this,     SLOT(onPlayerStatsChanged()));
        connect(m_player, SIGNAL(manaChanged(uint32_t, uint32_t)),
                this,     SLOT(onPlayerStatsChanged()));
        connect(m_player, SIGNAL(statChanged(int, int, int)),
                this,     SLOT(onPlayerStatsChanged()));
        connect(m_player, SIGNAL(stamChanged(int, int, int, int)),
                this,     SLOT(onPlayerStatsChanged()));
        connect(m_player, SIGNAL(levelChanged(uint8_t)),
                this,     SLOT(onPlayerStatsChanged()));
        connect(m_player, SIGNAL(expChangedInt(int, int, int)),
                this,     SLOT(onPlayerStatsChanged()));
        connect(m_player, SIGNAL(expAltChangedInt(int, int, int)),
                this,     SLOT(onPlayerStatsChanged()));
        connect(m_player, SIGNAL(newPlayer()),
                this,     SLOT(onPlayerStatsChanged()));
        // The player's spawn ID changes once at zone-in (from 0 -> real
        // id). The client tracks `playerId` from the Snapshot's
        // player_id field, so we resend a fresh snapshot here — that
        // drops the old player entry and re-establishes player_id
        // pointing at the new one. Connection happens after SpawnShell's
        // playerChangedID slot (registered in SpawnShell ctor), so by
        // the time we run the re-insert into m_players is already done.
        connect(m_player, SIGNAL(changedID(uint16_t, uint16_t)),
                this,     SLOT(onPlayerIdChanged()));
    }

    if (m_messageShell) {
        connect(m_messageShell,
                SIGNAL(chatMessage(uint32_t, const QString&,
                                   const QString&, const QString&)),
                this,
                SLOT(onChatMessage(uint32_t, const QString&,
                                   const QString&, const QString&)));
    }

    if (m_groupMgr) {
        connect(m_groupMgr, SIGNAL(added(const QString&, const Spawn*)),
                this,       SLOT(onGroupChanged()));
        connect(m_groupMgr, SIGNAL(removed(const QString&, const Spawn*)),
                this,       SLOT(onGroupChanged()));
        connect(m_groupMgr, SIGNAL(cleared()),
                this,       SLOT(onGroupChanged()));
    }

    // STEP 2: iterate current state into a Snapshot and ship it.
    sendSnapshot();

    // Initial PlayerStats so the client has something to render before
    // the next Player signal fires.
    if (m_player) {
        sendPlayerStats();
    }
    // Initial GroupUpdate (all 6 slots, may all be empty).
    if (m_groupMgr) {
        sendGroupUpdate();
    }

    // STEP 3: drain anything that arrived during the iteration.
    for (auto& env : m_buffered) {
        emitEnvelope(std::move(env));
    }
    m_buffered.clear();

    // STEP 4: from here on, handlers emit directly to the socket.
    m_liveTailing = true;
}

void SessionAdapter::sendSnapshot()
{
    seq::v1::Envelope env;
    auto* snap = env.mutable_snapshot();
    if (m_zoneMgr) {
        snap->set_zone_short(m_zoneMgr->shortZoneName().toStdString());
        snap->set_zone_long(m_zoneMgr->longZoneName().toStdString());
    }
    if (m_player) {
        snap->set_player_id(m_player->getPlayerID());
    }
    if (m_mapData && m_mapData->numLayers() > 0) {
        seq::encode::fillMapGeometry(snap->mutable_geometry(), *m_mapData);
    }

    // SpawnShell maintains separate ItemMaps for spawns, drops, doors, and
    // players; the Phase 1 client renders all of them the same way (as
    // labeled dots), so we collapse them into a single repeated Spawn list.
    QSet<uint16_t> seenIds;
    auto append = [&](const ItemMap& map) {
        ItemConstIterator it(map);
        while (it.hasNext()) {
            it.next();
            if (const Item* item = it.value()) {
                seq::encode::fillSpawn(snap->add_spawns(), *item);
                seenIds.insert(item->id());
            }
        }
    };
    append(m_spawnShell->spawns());
    append(m_spawnShell->drops());
    append(m_spawnShell->doors());
    append(m_spawnShell->getConstMap(tPlayer));

    // Belt + suspenders: if the player object never made it into
    // SpawnShell's m_players map (which can happen during early init
    // before zonePlayer fires), include it directly so the client gets
    // a marker for `player_id`.
    if (m_player && m_player->getPlayerID() != 0 &&
        !seenIds.contains(m_player->getPlayerID())) {
        seq::encode::fillSpawn(snap->add_spawns(), *m_player);
    }

    emitEnvelope(std::move(env));
}

void SessionAdapter::emitEnvelope(seq::v1::Envelope&& env)
{
    env.set_seq(++m_seq);
    env.set_server_ts_ms(static_cast<uint64_t>(
        QDateTime::currentMSecsSinceEpoch()));

    QByteArray buf;
    buf.resize(static_cast<int>(env.ByteSizeLong()));
    env.SerializeToArray(buf.data(), buf.size());
    m_sock->sendBinaryMessage(buf);
}

void SessionAdapter::sendOrBuffer(seq::v1::Envelope&& env)
{
    if (m_liveTailing) {
        emitEnvelope(std::move(env));
    } else {
        m_buffered.append(std::move(env));
    }
}

void SessionAdapter::onAddItem(const Item* item)
{
    if (!item) return;
    seq::v1::Envelope env;
    seq::encode::fillSpawn(env.mutable_spawn_added()->mutable_spawn(), *item);
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onDelItem(const Item* item)
{
    if (!item) return;
    seq::v1::Envelope env;
    env.mutable_spawn_removed()->set_id(item->id());
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onChangeItem(const Item* item, uint32_t changeType)
{
    if (!item) return;

    // tSpawnChangedALL is the legacy "full re-render" signal — used by
    // SpawnShell::changePlayerID, killSpawn-corpse-replace,
    // updateSpawnInfo, etc. The spawn may not currently exist in the
    // client (e.g. changePlayerID emits delItem(oldId) then
    // changeItem(player, ALL) at the new id). Send SpawnAdded so the
    // client overwrites/creates the entry; SpawnUpdated would be dropped
    // because there's no existing entry at the new id.
    if ((changeType & tSpawnChangedALL) == tSpawnChangedALL) {
        seq::v1::Envelope env;
        seq::encode::fillSpawn(env.mutable_spawn_added()->mutable_spawn(),
                               *item);
        sendOrBuffer(std::move(env));
        return;
    }

    seq::v1::Envelope env;
    auto* upd = env.mutable_spawn_updated();
    upd->set_id(item->id());

    if (const auto* sp = dynamic_cast<const Spawn*>(item)) {
        if (changeType & tSpawnChangedPosition) {
            seq::encode::fillPos(upd->mutable_pos(), *sp);
        }
        if (changeType & tSpawnChangedHP) {
            upd->set_hp_cur(sp->HP());
        }
        if (changeType & tSpawnChangedName) {
            upd->set_name(sp->name().toStdString());
        }
        if (changeType & (tSpawnChangedFilter | tSpawnChangedRuntimeFilter)) {
            upd->set_filter_flags(sp->filterFlags());
        }
    }
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onKillSpawn(const Item* deceased, const Item* killer,
                                 uint16_t killerId)
{
    seq::v1::Envelope env;
    auto* k = env.mutable_spawn_killed();
    if (deceased) k->set_deceased_id(deceased->id());
    k->set_killer_id(killer ? killer->id() : killerId);
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onZoneBegin(const QString& shortName)
{
    seq::v1::Envelope env;
    auto* zc = env.mutable_zone_changed();
    zc->set_zone_short(shortName.toStdString());
    if (m_zoneMgr) {
        zc->set_zone_long(m_zoneMgr->longZoneName().toStdString());
    }
    // DaemonApp::loadZoneMap is wired to the same zoneChanged signal, so by
    // the time this slot runs the MapData reflects the new zone. (Qt invokes
    // direct-connected slots in connection order; loadZoneMap was connected
    // before SessionAdapter.)
    if (m_mapData && m_mapData->numLayers() > 0) {
        seq::encode::fillMapGeometry(zc->mutable_geometry(), *m_mapData);
    }
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onZoneChanged(const QString& shortName)
{
    onZoneBegin(shortName);
}

void SessionAdapter::onPlayerStatsChanged()
{
    if (!m_player) return;
    sendPlayerStats();
}

void SessionAdapter::onPlayerIdChanged()
{
    if (!m_subscribed) return;
    sendSnapshot();
    sendPlayerStats();
}

void SessionAdapter::onChatMessage(uint32_t channel, const QString& from,
                                   const QString& target, const QString& text)
{
    seq::v1::Envelope env;
    auto* chat = env.mutable_chat();
    chat->set_channel(channel);
    chat->set_from(from.toStdString());
    chat->set_target(target.toStdString());
    chat->set_text(text.toStdString());
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onGroupChanged()
{
    if (!m_groupMgr) return;
    sendGroupUpdate();
}

void SessionAdapter::sendGroupUpdate()
{
    if (!m_groupMgr) return;
    seq::v1::Envelope env;
    seq::encode::fillGroupUpdate(env.mutable_group(), *m_groupMgr);
    sendOrBuffer(std::move(env));
}

void SessionAdapter::sendPlayerStats()
{
    seq::v1::Envelope env;
    seq::encode::fillPlayerStats(env.mutable_player_stats(), *m_player);
    sendOrBuffer(std::move(env));
}
