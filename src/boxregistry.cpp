#include "boxregistry.h"

#include "diagnosticmessages.h"

#include <QHostAddress>

QString Box::summary() const
{
    const QString ip = QHostAddress(ntohl(client_ip)).toString();
    const QString name = display_name.isEmpty() ? QStringLiteral("(unknown)")
                                                : display_name;
    return QStringLiteral("box %1  ip=%2  ports=%3->%4  pkts=%5  name=%6")
        .arg(box_id)
        .arg(ip)
        .arg(ntohs(client_world_port))
        .arg(ntohs(server_world_port))
        .arg(packet_count)
        .arg(name);
}

Box* BoxRegistry::observe(in_addr_t client_ip,
                          in_port_t client_world_port,
                          in_port_t server_world_port,
                          qint64    now_ms)
{
    for (auto& b : m_boxes) {
        if (b.client_ip == client_ip &&
            b.client_world_port == client_world_port &&
            b.server_world_port == server_world_port) {
            b.last_seen_ms = now_ms;
            ++b.packet_count;
            return &b;
        }
    }

    // First sighting — build a placeholder box_id from the 5-tuple.
    // Stage 3 will overwrite with a hash of the character name once
    // OP_PlayerProfile arrives.
    Box b;
    b.client_ip = client_ip;
    b.client_world_port = client_world_port;
    b.server_world_port = server_world_port;
    b.first_seen_ms = now_ms;
    b.last_seen_ms = now_ms;
    b.packet_count = 1;
    b.box_id = QStringLiteral("p-%1-%2-%3")
        .arg(client_ip, 8, 16, QChar('0'))
        .arg(ntohs(client_world_port), 4, 16, QChar('0'))
        .arg(ntohs(server_world_port), 4, 16, QChar('0'));

    m_boxes.push_back(std::move(b));

    Box* added = &m_boxes.back();
    seqInfo("BoxRegistry: new box %s  ip=%s  client_port=%u  server_port=%u",
            qUtf8Printable(added->box_id),
            qUtf8Printable(QHostAddress(ntohl(added->client_ip)).toString()),
            ntohs(added->client_world_port),
            ntohs(added->server_world_port));
    return added;
}

Box* BoxRegistry::primary()
{
    return m_boxes.empty() ? nullptr : &m_boxes.front();
}

QString BoxRegistry::dumpString() const
{
    if (m_boxes.empty())
        return QStringLiteral("BoxRegistry: empty");

    QString out = QStringLiteral("BoxRegistry: %1 box(es)\n").arg(m_boxes.size());
    for (const auto& b : m_boxes) {
        out += QStringLiteral("  ") + b.summary() + QChar('\n');
    }
    return out;
}
