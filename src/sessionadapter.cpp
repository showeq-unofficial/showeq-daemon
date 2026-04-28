#include "sessionadapter.h"

#include <QByteArray>
#include <QDateTime>
#include <QLoggingCategory>
#include <QSet>
#include <algorithm>
#include <vector>

extern "C" { // pcap headers aren't c++-clean
#include <pcap.h>
}

#include "category.h"
#include "combatrouter.h"
#include "envelopesink.h"
#include "filtermgr.h"
#include "group.h"
#include "mapcore.h"
#include "messageshell.h"
#include "player.h"
#include "prefsbroker.h"
#include "protoencoder.h"
#include "spawn.h"
#include "spawnmonitor.h"
#include "spawnshell.h"
#include "spellshell.h"
#include "zonemgr.h"

#include "seq/v1/client.pb.h"

SessionAdapter::SessionAdapter(IEnvelopeSink* sink,
                               SpawnShell*   spawnShell,
                               ZoneMgr*      zoneMgr,
                               Player*       player,
                               MapData*      mapData,
                               MessageShell* messageShell,
                               GroupMgr*     groupMgr,
                               SpellShell*   spellShell,
                               CombatRouter* combatRouter,
                               CategoryMgr*  categoryMgr,
                               FilterMgr*    filterMgr,
                               PrefsBroker*  prefsBroker,
                               SpawnMonitor* spawnMonitor,
                               QObject*      parent)
    : QObject(parent)
    , m_sink(sink)
    , m_spawnShell(spawnShell)
    , m_zoneMgr(zoneMgr)
    , m_player(player)
    , m_mapData(mapData)
    , m_messageShell(messageShell)
    , m_groupMgr(groupMgr)
    , m_spellShell(spellShell)
    , m_combatRouter(combatRouter)
    , m_categoryMgr(categoryMgr)
    , m_filterMgr(filterMgr)
    , m_prefsBroker(prefsBroker)
    , m_spawnMonitor(spawnMonitor)
{
}

SessionAdapter::~SessionAdapter() = default;

void SessionAdapter::handleClientText(const QString& text)
{
    qInfo("ws text frame ignored: %s", qUtf8Printable(text));
}

void SessionAdapter::handleClientBinary(const QByteArray& bytes)
{
    // Leaky-bucket rate limit. 30 sustained msg/s with a 60-msg burst is
    // far above what any reasonable client UI emits (Subscribe is once
    // per connection; filter / pref edits are user-initiated). A runaway
    // client gets quietly dropped and warned once per session.
    constexpr double kTokensPerSec = 30.0;
    constexpr double kBucketCap = 60.0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_bucketLastMs == 0) {
        m_bucketLastMs = now;
        m_bucketTokens = kBucketCap;
    } else {
        m_bucketTokens += (now - m_bucketLastMs) * kTokensPerSec / 1000.0;
        if (m_bucketTokens > kBucketCap) m_bucketTokens = kBucketCap;
        m_bucketLastMs = now;
    }
    if (m_bucketTokens < 1.0) {
        if (!m_rateLimitWarned) {
            qWarning("rate limit: client exceeded %.0f msg/s, dropping",
                     kTokensPerSec);
            m_rateLimitWarned = true;
        }
        return;
    }
    m_bucketTokens -= 1.0;
    m_rateLimitWarned = false;

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
        return;
    }
    if (env.has_add_filter_rule() && m_filterMgr) {
        const auto& add = env.add_filter_rule();
        const QString pattern = QString::fromStdString(add.pattern());
        const uint8_t type = static_cast<uint8_t>(add.filter_type());
        if (add.per_zone()) {
            m_filterMgr->addZoneFilter(type, pattern);
        } else {
            m_filterMgr->addFilter(type, pattern);
        }
        // FilterMgr emits filtersChanged after the mutation, which fans
        // out to every connected SessionAdapter via onFilterRulesChanged.
        return;
    }
    if (env.has_remove_filter_rule() && m_filterMgr) {
        const auto& rem = env.remove_filter_rule();
        const QString pattern = QString::fromStdString(rem.pattern());
        const uint8_t type = static_cast<uint8_t>(rem.filter_type());
        if (rem.per_zone()) {
            m_filterMgr->remZoneFilter(type, pattern);
        } else {
            m_filterMgr->remFilter(type, pattern);
        }
        return;
    }
    if (env.has_edit_filter_rule() && m_filterMgr) {
        const auto& edit = env.edit_filter_rule();
        const QString oldPat = QString::fromStdString(edit.old_pattern());
        const QString newPat = QString::fromStdString(edit.new_pattern());
        const uint8_t type = static_cast<uint8_t>(edit.filter_type());
        if (edit.per_zone()) {
            m_filterMgr->editZoneFilter(type, oldPat, newPat);
        } else {
            m_filterMgr->editFilter(type, oldPat, newPat);
        }
        return;
    }
    if (env.has_save_filters() && m_filterMgr) {
        // Mirrors showeq's "Save Filters" / "Save Zone Filters"
        // menu items. Persistence is explicit on purpose so a UI
        // mutation doesn't immediately overwrite an operator's
        // hand-edited XML. No filtersChanged emit — saving doesn't
        // alter rule state, just persists it.
        if (env.save_filters().per_zone()) {
            m_filterMgr->saveZoneFilters();
        } else {
            m_filterMgr->saveFilters();
        }
        return;
    }
    if (env.has_reload_filters() && m_filterMgr) {
        // Re-read XML from disk for both global filters and the
        // current zone overlay (if any). loadFilters/loadZone each
        // emit filtersChanged, which fans out to SpawnShell (re-eval
        // of every spawn's flag bits) and every SessionAdapter
        // (broadcast a fresh FilterRulesUpdate).
        m_filterMgr->loadFilters();
        if (m_zoneMgr) {
            const QString zone = m_zoneMgr->shortZoneName();
            if (!zone.isEmpty()) m_filterMgr->loadZone(zone);
        }
        return;
    }
    if (env.has_set_pref() && m_prefsBroker) {
        // PrefsBroker validates against the allowlist; rejects (unknown
        // key, mismatched value variant) are dropped silently — no error
        // envelope yet. On success the broker emits prefChanged, which
        // every connected SessionAdapter forwards as PrefChanged.
        m_prefsBroker->apply(env.set_pref().pref());
        return;
    }
    if (env.has_list_devices()) {
        // Session-scoped reply, not broadcast — the picker is per-tab UI.
        // pcap_findalldevs walks /sys/class/net (or platform equivalent);
        // failure leaves the list empty and the client renders "no
        // devices found", which is the same fallback path as a host
        // with no usable interfaces.
        seq::v1::Envelope reply;
        auto* list = reply.mutable_devices_list();
        char errbuf[PCAP_ERRBUF_SIZE] = {};
        pcap_if_t* alldevs = nullptr;
        if (pcap_findalldevs(&alldevs, errbuf) == 0) {
            for (pcap_if_t* d = alldevs; d != nullptr; d = d->next) {
                auto* dev = list->add_devices();
                dev->set_name(d->name ? d->name : "");
                dev->set_description(d->description ? d->description : "");
                dev->set_is_loopback((d->flags & PCAP_IF_LOOPBACK) != 0);
            }
            pcap_freealldevs(alldevs);
        } else {
            qWarning("pcap_findalldevs failed: %s", errbuf);
        }
        sendOrBuffer(std::move(reply));
        return;
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
    connect(m_spawnShell, &SpawnShell::spawnConsidered,
            this,         &SessionAdapter::onSpawnConsidered);
    connect(m_spawnShell, &SpawnShell::targetSpawn,
            this,         &SessionAdapter::onTargetSpawn);
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

    if (m_spellShell) {
        connect(m_spellShell, SIGNAL(addSpell(const SpellItem*)),
                this,         SLOT(onBuffsChanged()));
        connect(m_spellShell, SIGNAL(delSpell(const SpellItem*)),
                this,         SLOT(onBuffsChanged()));
        connect(m_spellShell, SIGNAL(changeSpell(const SpellItem*)),
                this,         SLOT(onBuffsChanged()));
        connect(m_spellShell, SIGNAL(clearSpells()),
                this,         SLOT(onBuffsChanged()));
    }

    if (m_combatRouter) {
        connect(m_combatRouter,
                SIGNAL(combatEvent(uint32_t, const QString&,
                                   uint32_t, const QString&,
                                   uint32_t, int32_t,
                                   uint32_t, const QString&)),
                this,
                SLOT(onCombatEvent(uint32_t, const QString&,
                                   uint32_t, const QString&,
                                   uint32_t, int32_t,
                                   uint32_t, const QString&)));
    }

    if (m_categoryMgr) {
        // CategoryMgr's add/del/cleared/loaded signals all fire on
        // mutation; coalesce into one slot that resends the full list.
        connect(m_categoryMgr, SIGNAL(addCategory(const Category*)),
                this,          SLOT(onCategoriesChanged()));
        connect(m_categoryMgr, SIGNAL(delCategory(const Category*)),
                this,          SLOT(onCategoriesChanged()));
        connect(m_categoryMgr, SIGNAL(clearedCategories()),
                this,          SLOT(onCategoriesChanged()));
        connect(m_categoryMgr, SIGNAL(loadedCategories()),
                this,          SLOT(onCategoriesChanged()));
    }

    if (m_filterMgr) {
        connect(m_filterMgr, SIGNAL(filtersChanged()),
                this,        SLOT(onFilterRulesChanged()));
    }

    if (m_prefsBroker) {
        // Direct connection in the same thread; pref payloads are small
        // and this fans out to every connected SessionAdapter so each
        // emits its own PrefChanged envelope.
        connect(m_prefsBroker, &PrefsBroker::prefChanged,
                this,          &SessionAdapter::onPrefChanged);
    }

    if (m_spawnMonitor) {
        // SpawnMonitor surfaces four signals — newSpawnPoint (promoted
        // from candidate to tracked), spawnPointUpdated (re-pop / kill
        // restart), spawnPointDeleted (explicit user delete), and
        // clearSpawnPoints (zone change / clear-all). The current
        // promoted set is iterated into the Snapshot below.
        connect(m_spawnMonitor, &SpawnMonitor::newSpawnPoint,
                this,           &SessionAdapter::onSpawnPointAdded);
        connect(m_spawnMonitor, &SpawnMonitor::spawnPointUpdated,
                this,           &SessionAdapter::onSpawnPointUpdated);
        connect(m_spawnMonitor, &SpawnMonitor::spawnPointDeleted,
                this,           &SessionAdapter::onSpawnPointDeleted);
        connect(m_spawnMonitor, &SpawnMonitor::clearSpawnPoints,
                this,           &SessionAdapter::onSpawnPointsCleared);
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
    // Initial BuffsUpdate (may be an empty list before login).
    if (m_spellShell) {
        sendBuffsUpdate();
    }
    // Initial CategoriesUpdate (loaded from prefs at CategoryMgr ctor).
    if (m_categoryMgr) {
        sendCategoriesUpdate();
    }
    // Initial FilterRulesUpdate (global + per-zone rules currently
    // loaded; either may be empty if files don't exist).
    if (m_filterMgr) {
        sendFilterRulesUpdate();
    }
    // Initial PrefsSnapshot — every allowlisted pref's current value.
    if (m_prefsBroker) {
        sendPrefsSnapshot();
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
    snap->set_session_id(m_sessionId.toStdString());
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

    // SpawnShell maintains separate ItemMaps (QHash<int, Item*>) for
    // spawns, drops, doors, and players; the Phase 1 client renders all
    // of them the same way (as labeled dots), so we collapse them into
    // a single repeated Spawn list. ItemMap is a QHash; Qt randomizes
    // hash seeds per-process so iteration order is not stable. Collect
    // pointers into a vector and sort by id before encoding so the
    // Snapshot bytes are deterministic across runs (regression-harness
    // requirement; clients key by id and don't depend on order anyway).
    std::vector<const Item*> all;
    QSet<uint16_t> seenIds;
    auto collect = [&](const ItemMap& map) {
        ItemConstIterator it(map);
        while (it.hasNext()) {
            it.next();
            if (const Item* item = it.value()) {
                all.push_back(item);
                seenIds.insert(item->id());
            }
        }
    };
    collect(m_spawnShell->spawns());
    collect(m_spawnShell->drops());
    collect(m_spawnShell->doors());
    collect(m_spawnShell->getConstMap(tPlayer));

    // Belt + suspenders: if the player object never made it into
    // SpawnShell's m_players map (which can happen during early init
    // before zonePlayer fires), include it directly so the client gets
    // a marker for `player_id`.
    if (m_player && m_player->getPlayerID() != 0 &&
        !seenIds.contains(m_player->getPlayerID())) {
        all.push_back(m_player);
    }

    std::sort(all.begin(), all.end(),
              [](const Item* a, const Item* b) {
                  return a->id() < b->id();
              });

    for (const Item* item : all) {
        seq::encode::fillSpawn(snap->add_spawns(), *item,
                               m_categoryMgr, m_filterMgr);
    }

    // Seed any already-promoted SpawnPoints. QHash iteration order is
    // randomized per-process, so sort by key for deterministic Snapshot
    // bytes (regression-harness requirement).
    if (m_spawnMonitor) {
        std::vector<const SpawnPoint*> points;
        const auto& map = m_spawnMonitor->spawnPoints();
        points.reserve(map.size());
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (it.value()) points.push_back(it.value());
        }
        std::sort(points.begin(), points.end(),
                  [](const SpawnPoint* a, const SpawnPoint* b) {
                      return a->key() < b->key();
                  });
        for (const SpawnPoint* sp : points) {
            seq::encode::fillSpawnPoint(snap->add_spawn_points(), *sp,
                                        m_deterministic);
        }
    }

    emitEnvelope(std::move(env));
}

void SessionAdapter::emitEnvelope(seq::v1::Envelope&& env)
{
    env.set_seq(++m_seq);
    env.set_server_ts_ms(static_cast<uint64_t>(
        QDateTime::currentMSecsSinceEpoch()));
    m_sink->send(env);

    // Resume buffer. Cap by time window (30s) and by absolute size
    // (10k entries) so a chatty zone can't OOM the daemon between
    // disconnect + reconnect.
    constexpr int kMaxEntries = 10000;
    constexpr qint64 kWindowMs = 30 * 1000;
    m_ringBuffer.push_back(std::move(env));
    const qint64 cutoff = m_ringBuffer.back().server_ts_ms() - kWindowMs;
    while (!m_ringBuffer.empty() &&
           static_cast<qint64>(m_ringBuffer.front().server_ts_ms()) < cutoff) {
        m_ringBuffer.pop_front();
    }
    while (static_cast<int>(m_ringBuffer.size()) > kMaxEntries) {
        m_ringBuffer.pop_front();
    }
}

void SessionAdapter::replaySince(uint64_t lastSeq)
{
    // Linear scan is fine — the buffer is bounded at 10k. Skip envelopes
    // the client already has, replay the rest in order through the
    // current sink. We do NOT bump m_seq or rewrite server_ts_ms here:
    // the envelope was already finalized when it was first emitted, and
    // a faithful replay preserves the original ordering + timestamps.
    for (const auto& env : m_ringBuffer) {
        if (env.seq() > lastSeq) {
            m_sink->send(env);
        }
    }
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
    seq::encode::fillSpawn(env.mutable_spawn_added()->mutable_spawn(), *item,
                           m_categoryMgr, m_filterMgr);
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
    //
    // Filter-flag changes get the same treatment: filter flags feed into
    // category membership (see fillSpawn), so when a user adds a rule
    // the spawn's category_ids set changes too. SpawnUpdated only carries
    // filter_flags, so the client's category_ids would otherwise stay
    // stale until re-zone.
    const bool filterChanged =
        (changeType & (tSpawnChangedFilter | tSpawnChangedRuntimeFilter)) != 0;
    if ((changeType & tSpawnChangedALL) == tSpawnChangedALL || filterChanged) {
        seq::v1::Envelope env;
        seq::encode::fillSpawn(env.mutable_spawn_added()->mutable_spawn(),
                               *item, m_categoryMgr, m_filterMgr);
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

void SessionAdapter::onBuffsChanged()
{
    if (!m_spellShell) return;
    sendBuffsUpdate();
}

void SessionAdapter::sendBuffsUpdate()
{
    if (!m_spellShell) return;
    seq::v1::Envelope env;
    auto* update = env.mutable_buffs();
    update->set_captured_ms(static_cast<uint64_t>(
        QDateTime::currentMSecsSinceEpoch()));
    for (const SpellItem* s : m_spellShell->spellList()) {
        if (!s) continue;
        seq::encode::fillBuff(update->add_buffs(), *s);
    }
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onCombatEvent(uint32_t sourceId, const QString& sourceName,
                                   uint32_t targetId, const QString& targetName,
                                   uint32_t type, int32_t damage,
                                   uint32_t spellId, const QString& spellName)
{
    seq::v1::Envelope env;
    auto* ev = env.mutable_combat();
    ev->set_source_id(sourceId);
    ev->set_source_name(sourceName.toStdString());
    ev->set_target_id(targetId);
    ev->set_target_name(targetName.toStdString());
    ev->set_type(type);
    ev->set_damage(damage);
    ev->set_spell_id(spellId);
    ev->set_spell_name(spellName.toStdString());
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onCategoriesChanged()
{
    if (!m_categoryMgr) return;
    sendCategoriesUpdate();
}

void SessionAdapter::sendCategoriesUpdate()
{
    if (!m_categoryMgr) return;
    seq::v1::Envelope env;
    seq::encode::fillCategoriesUpdate(env.mutable_categories(), *m_categoryMgr);
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onFilterRulesChanged()
{
    if (!m_filterMgr) return;
    sendFilterRulesUpdate();
}

void SessionAdapter::sendFilterRulesUpdate()
{
    if (!m_filterMgr) return;
    seq::v1::Envelope env;
    seq::encode::fillFilterRulesUpdate(env.mutable_filter_rules(), *m_filterMgr);
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onPrefChanged(const seq::v1::Pref& pref)
{
    seq::v1::Envelope env;
    *env.mutable_pref_changed()->mutable_pref() = pref;
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onSpawnConsidered(const Item* item)
{
    if (!item) return;
    seq::v1::Envelope env;
    env.mutable_considered()->set_spawn_id(item->id());
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onTargetSpawn(uint32_t spawnId)
{
    seq::v1::Envelope env;
    env.mutable_targeted()->set_spawn_id(spawnId);
    sendOrBuffer(std::move(env));
}

void SessionAdapter::sendPrefsSnapshot()
{
    if (!m_prefsBroker) return;
    seq::v1::Envelope env;
    m_prefsBroker->fillSnapshot(env.mutable_prefs());
    sendOrBuffer(std::move(env));
}

void SessionAdapter::sendPlayerStats()
{
    seq::v1::Envelope env;
    seq::encode::fillPlayerStats(env.mutable_player_stats(), *m_player);
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onSpawnPointAdded(const SpawnPoint* sp)
{
    if (!sp) return;
    seq::v1::Envelope env;
    seq::encode::fillSpawnPoint(
        env.mutable_spawn_point_added()->mutable_point(), *sp,
        m_deterministic);
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onSpawnPointUpdated(const SpawnPoint* sp)
{
    if (!sp) return;
    seq::v1::Envelope env;
    seq::encode::fillSpawnPoint(
        env.mutable_spawn_point_updated()->mutable_point(), *sp,
        m_deterministic);
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onSpawnPointDeleted(const SpawnPoint* sp)
{
    if (!sp) return;
    seq::v1::Envelope env;
    env.mutable_spawn_point_removed()->set_key(sp->key().toStdString());
    sendOrBuffer(std::move(env));
}

void SessionAdapter::onSpawnPointsCleared()
{
    seq::v1::Envelope env;
    env.mutable_spawn_points_cleared();
    sendOrBuffer(std::move(env));
}
