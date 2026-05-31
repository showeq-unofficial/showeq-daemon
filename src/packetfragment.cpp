/*
 *  packetfragment.cpp
 *  Copyright 2000-2005, 2019 by the respective ShowEQ Developers
 *  Portions Copyright 2001-2003 Zaphod (dohpaz@users.sourceforge.net).
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

/* Implementation of EQPacketFragmentSequence class */

#include "packetfragment.h"
#include "packetformat.h"
#include "diagnosticmessages.h"

#include <cstring>

//----------------------------------------------------------------------
// Constants

// Hard ceiling on a reassembled fragment sequence. Legitimate bulk
// packets (e.g. dense zone-spawn dumps) reach ~180 KB; anything past a
// few MB is almost always a fragment decoded with the wrong session key
// (foreign-session bytes mis-routed onto this stream in multibox). Past
// this we abandon the sequence rather than allocate unbounded memory.
static constexpr uint32_t kMaxFragmentBuffer = 4u * 1024u * 1024u; // 4 MB

//----------------------------------------------------------------------
// Macros

// diagnose fragmentation problems
//#define PACKET_PROCESS_FRAG_DIAG

//----------------------------------------------------------------------
// EQPacketFragmentSequence class methods

////////////////////////////////////////////////////
// Constructor
EQPacketFragmentSequence::EQPacketFragmentSequence()
  : m_streamid(unknown_stream),
    m_data(0),
    m_totalLength(0),
    m_dataSize(0),
    m_dataAllocSize(0)
{
}

////////////////////////////////////////////////////
// Constructor
EQPacketFragmentSequence::EQPacketFragmentSequence(EQStreamID streamid)
  : m_streamid(streamid),
    m_data(0),
    m_totalLength(0),
    m_dataSize(0),
    m_dataAllocSize(0)
{
}

////////////////////////////////////////////////////
// Destructor
EQPacketFragmentSequence::~EQPacketFragmentSequence()
{
  if (m_data)
    delete [] m_data;
}

////////////////////////////////////////////////////
// Reset the fragment sequence
void EQPacketFragmentSequence::reset()
{
#ifdef PACKET_PROCESS_FRAG_DIAG
   qDebug ("EQPacketFragmentSequence::reset() stream %d (complete fragment? %s)",
     m_streamid, (isComplete() ? "yes" : "no"));
#endif
  m_dataSize = 0;
  m_totalLength = 0;
}

////////////////////////////////////////////////////
// Add a fragment to the sequence
void EQPacketFragmentSequence::addFragment(EQProtocolPacket& packet)
{
#ifdef PACKET_PROCESS_FRAG_DIAG
   qDebug ("EQPacketFragmentSequence::addFragment() stream %d seq %04x", 
     m_streamid, packet.arqSeq());
#endif
   
   // If dataSize isn't filled in, this is first fragment. Need to alloc.
   if (m_dataSize == 0)
   {
      // Buffer length is on the wire first.
      m_totalLength = eqntohuint32(packet.payload());

      if (m_totalLength == 0)
      {
         seqWarn("Oversized packet fragment requested buffer of size 0 on stream %d OpCode %04x seq %04x",
           m_streamid, *(uint16_t*)&packet.payload()[4], packet.arqSeq());
      }
      else if (m_totalLength > kMaxFragmentBuffer)
      {
         // Implausibly large declared total — almost always a fragment
         // decoded with the wrong session key (foreign-session bytes
         // mis-routed onto this stream). Abandon the sequence rather than
         // allocate a huge buffer or abort the daemon.
         seqWarn("EQPacketFragmentSequence: rejecting oversized first "
           "fragment (declared %u > cap %u) on stream %d OpCode %04x seq %04x "
           "— likely wrong-session data; dropping sequence.",
           m_totalLength, kMaxFragmentBuffer, m_streamid,
           *(uint16_t*)&packet.payload()[4], packet.arqSeq());
         reset();
         return;
      }
      else if (m_totalLength > m_dataAllocSize)
      {
        // Buffer isn't big enough. Enlargen it.
        if (m_data)
        {
          delete[] m_data;
        }
#ifdef PACKET_PROCESS_FRAG_DIAG
        seqDebug("EQPacketFragmentSequence::addFragment(): Allocating %d bytes for seq %04x, stream %d, OpCode 0x%04x",
          m_totalLength, packet.arqSeq(), m_streamid, 
          *(uint16_t*)&packet.payload()[4]);
#endif
        m_dataAllocSize = m_totalLength;
        m_data = new uint8_t[m_dataAllocSize];
      }
      
      // Now put in this fragment. Payload starts after alloc size.
#ifdef PACKET_PROCESS_FRAG_DIAG
      seqDebug("EQPacketFragmentSequence::addFragment(): Putting initial %d byte fragment into buffer for seq %04x, stream %d, OpCode 0x%04x",
             packet.payloadLength()-4, packet.arqSeq(), m_streamid, 
             *(uint16_t*)&packet.payload()[4]);
#endif
      memcpy(m_data, &packet.payload()[4], packet.payloadLength()-4);
      m_dataSize = packet.payloadLength() - 4;
   }
   else
   {
      // Add this fragment to the buffer. Payload starts immediately.
#ifdef PACKET_PROCESS_FRAG_DIAG
      seqDebug("EQPacketFragmentSequence::addFragment(): Putting %d byte fragment into buffer for seq %04x, stream %d, OpCode 0x%04x",
             packet.payloadLength(), packet.arqSeq(), m_streamid, 
             *(uint16_t*)(m_data));
#endif
      
      const size_t needed = m_dataSize + packet.payloadLength();
      if (needed > m_dataAllocSize)
      {
        // The first fragment under-reported the total. This happens on
        // legitimate large bulk packets (a few hundred bytes short on
        // stream 3) AND on wrong-session data. GROW the buffer to fit —
        // preserving every byte, so no continuation is silently dropped
        // (the cascade trap from the reverted soft-drop attempt). Past the
        // hard cap it's almost certainly foreign-session garbage, so we
        // abandon the sequence instead of growing without bound or
        // aborting the daemon.
        if (needed > kMaxFragmentBuffer)
        {
          seqWarn("EQPacketFragmentSequence: fragment sequence on stream %d "
            "seq %04x exceeds cap (%zu > %u) — abandoning (likely "
            "wrong-session data).",
            m_streamid, packet.arqSeq(), needed, kMaxFragmentBuffer);
          reset();
          return;
        }
        seqWarn("EQPacketFragmentSequence: growing fragment buffer %u -> %zu "
          "on stream %d seq %04x (first-fragment total under-reported).",
          m_dataAllocSize, needed, m_streamid, packet.arqSeq());
        uint8_t* grown = new uint8_t[needed];
        memcpy(grown, m_data, m_dataSize);
        delete[] m_data;
        m_data = grown;
        m_dataAllocSize = needed;
      }

      memcpy(m_data + m_dataSize, packet.payload(), packet.payloadLength());
      m_dataSize += packet.payloadLength();
   }
}
