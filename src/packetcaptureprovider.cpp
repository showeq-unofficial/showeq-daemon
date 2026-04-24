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

#include "packetcaptureprovider.h"


PacketCaptureProviderThread::PacketCaptureProviderThread() :
        m_pcache_first(NULL),
        m_pcache_last(NULL),
        m_pcache_closed(true)
{
    pthread_mutex_init(&m_pcache_mutex, NULL);
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
        memcpy (buf, pc->data, ret);
        free (pc);
    }

    return ret;
}
