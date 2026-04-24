/*
 *  packetcaptureprovider.h
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

#ifndef _PACKETCAPTUREPROVIDERTHREAD_H_
#define _PACKETCAPTUREPROVIDERTHREAD_H_

#include <cstdint>
#include <QString>

#include <pthread.h>

class PacketCaptureProviderThread
{
    public:
        PacketCaptureProviderThread();
        virtual ~PacketCaptureProviderThread();

        virtual bool offlinePlaybackSupported() = 0;
        virtual void startOffline(const char* filename, int playbackSpeed) = 0;
        virtual void setPlaybackSpeed(int playbackSpeed) = 0;
        virtual int getPlaybackSpeed() = 0;

        virtual void start (const char *device, const char *host, bool realtime, uint8_t address_type) = 0;
        virtual void stop () = 0;

        virtual uint16_t getPacket (unsigned char *buf);

        virtual void setFilter (const char *device, const char *hostname, bool realtime,
                uint8_t address_type, uint16_t zone_server_port, uint16_t client_port) = 0;
        virtual const QString getFilter() = 0;


    protected:

        struct packetCache
        {
            struct packetCache *next;
            ssize_t len;
            unsigned char data[0];
        };
        struct packetCache *m_pcache_first;
        struct packetCache *m_pcache_last;
        bool m_pcache_closed;

        pthread_t m_tid;
        pthread_mutex_t m_pcache_mutex;

};

#endif

