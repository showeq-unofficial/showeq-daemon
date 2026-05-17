#include "zoneserverobserver.h"

#include "boxregistry.h"
#include "diagnosticmessages.h"
#include "everquest.h"
#include "packetcommon.h"
#include "packetstream.h"

namespace {
constexpr uint16_t kOpZoneServerInfo = 0x55e6;
constexpr size_t   kZsiLen           = sizeof(zoneServerInfoStruct); // 130
}

ZoneServerObserver::ZoneServerObserver(Box* box, EQPacketStream* world_s2c,
                                       QObject* parent)
    : QObject(parent), m_box(box)
{
    Q_ASSERT(box);
    Q_ASSERT(world_s2c);
    connect(world_s2c,
            SIGNAL(decodedPacket(const uint8_t*, size_t, uint8_t,
                                 uint16_t, const EQPacketOPCode*)),
            this,
            SLOT(onDecodedPacket(const uint8_t*, size_t, uint8_t,
                                 uint16_t, const EQPacketOPCode*)));
}

void ZoneServerObserver::onDecodedPacket(const uint8_t* data, size_t len,
                                         uint8_t dir, uint16_t opcode,
                                         const EQPacketOPCode* /*entry*/)
{
    if (opcode != kOpZoneServerInfo) return;
    if (dir != DIR_Server) return;
    if (len != kZsiLen) return;

    const auto* zsi = reinterpret_cast<const zoneServerInfoStruct*>(data);
    m_box->expected_zone_server_port = htons(zsi->port);
    // Hostname-to-IP resolution is async + costly; defer. dispatchPacket
    // matches on (client_ip, server_port) — for distinct ports per zone
    // it's enough. Two boxes zoning to the same server port simultaneously
    // would collide; deferred to a later stage if it crops up in practice.
    seqInfo("ZoneServerObserver: box %s expects zone server port %u",
            qUtf8Printable(m_box->box_id),
            ntohs(m_box->expected_zone_server_port));
}
