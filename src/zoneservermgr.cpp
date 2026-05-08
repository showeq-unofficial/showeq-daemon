#include "zoneservermgr.h"
#include "everquest.h"

#include <cstring>

ZoneServerMgr::ZoneServerMgr(QObject* parent)
    : QObject(parent)
{
}

void ZoneServerMgr::zoneServerInfo(const uint8_t* data)
{
    if (!data) return;
    const auto* info = reinterpret_cast<const zoneServerInfoStruct*>(data);

    // host[128] is NUL-padded; defensive strnlen so a non-terminated
    // payload doesn't read past the struct.
    const size_t hostLen = ::strnlen(info->host, sizeof(info->host));
    m_host = QString::fromLatin1(info->host, static_cast<int>(hostLen));
    m_port = info->port;
    m_hasInfo = true;
    emit zoneServerChanged(m_host, m_port);
}
