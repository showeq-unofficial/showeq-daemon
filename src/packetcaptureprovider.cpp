/*
 *  packetcaptureprovider.cpp
 *  Copyright 2000-2024 by the respective ShowEQ Developers
 *
 *  This file is part of ShowEQ.
 *  http://www.sourceforge.net/projects/seq
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <cstdlib>
#include <cstring>

#include "packetcaptureprovider.h"


PacketCaptureProviderThread::PacketCaptureProviderThread() :
        m_pcache_first(NULL),
        m_pcache_last(NULL),
        m_pcache_closed(true),
        m_offline_eof(false),
        m_lastCaptureMs(0)
{
    pthread_mutex_init(&m_pcache_mutex, NULL);
}

void PacketCaptureProviderThread::signalOfflineEof()
{
    pthread_mutex_lock(&m_pcache_mutex);
    m_offline_eof = true;
    pthread_mutex_unlock(&m_pcache_mutex);
}

void PacketCaptureProviderThread::enqueue(const unsigned char* data, uint32_t len,
        int64_t ts_ms)
{
    struct packetCache* pc =
        (struct packetCache*)malloc(sizeof(struct packetCache) + len);
    pc->len = len;
    pc->ts_ms = ts_ms;
    memcpy(pc->data, data, len);
    pc->next = NULL;

    pthread_mutex_lock(&m_pcache_mutex);
    if (!m_pcache_closed)
    {
        if (m_pcache_last)
            m_pcache_last->next = pc;
        m_pcache_last = pc;
        if (!m_pcache_first)
            m_pcache_first = pc;
    }
    else
    {
        free(pc);
    }
    pthread_mutex_unlock(&m_pcache_mutex);
}

bool PacketCaptureProviderThread::offlinePlaybackComplete()
{
    pthread_mutex_lock(&m_pcache_mutex);
    // EOF reached by the reader AND the consumer has drained the queue. The
    // reader sets m_offline_eof only after its last packetCallBack has already
    // enqueued, so an empty queue here means nothing more is coming.
    const bool done = m_offline_eof && (m_pcache_first == NULL);
    pthread_mutex_unlock(&m_pcache_mutex);
    return done;
}

PacketCaptureProviderThread::~PacketCaptureProviderThread()
{
    // Drop the packets we have lying around
    pthread_mutex_lock (&m_pcache_mutex);

    struct packetCache *pc = m_pcache_first;
    struct packetCache* freeMe = NULL;

    while (pc)
    {
        freeMe = pc;
        pc = pc->next;

        free(freeMe);
    }

    m_pcache_first = NULL;
    m_pcache_last = NULL;
    m_pcache_closed = true;

    pthread_mutex_unlock (&m_pcache_mutex);

    pthread_mutex_destroy(&m_pcache_mutex);

}

uint16_t PacketCaptureProviderThread::getPacket (unsigned char *buf)
{
    uint16_t ret;
    struct packetCache *pc = NULL;

    pthread_mutex_lock (&m_pcache_mutex);

    ret = 0;

    pc = m_pcache_first;

    if (pc)
    {
        m_pcache_first = pc->next;

        if (!m_pcache_first)
            m_pcache_last = NULL;
    }

    pthread_mutex_unlock (&m_pcache_mutex);

    if (pc)
    {
        ret = pc->len;
        m_lastCaptureMs = pc->ts_ms;
        memcpy (buf, pc->data, ret);
        free (pc);
    }

    return ret;
}
