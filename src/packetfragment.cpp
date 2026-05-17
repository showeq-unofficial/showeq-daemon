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
         reset();
         return;
      }
      // Cap declared total against the protocol-wide sanity limit (mirrors
      // packetstream.cpp's maxPacketSize). Under multibox, interleaved
      // fragments from two clients can present a junk total here.
      if (m_totalLength > 131072)
      {
         seqWarn("Oversized packet fragment requested buffer of size %u on stream %d OpCode %04x seq %04x; dropping",
           m_totalLength, m_streamid, *(uint16_t*)&packet.payload()[4], packet.arqSeq());
         reset();
         return;
      }
      if (m_totalLength > m_dataAllocSize)
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
        // Test wire occasionally under-reports the first-fragment total
        // length by a few hundred bytes; legitimate bulk-spawn packets on
        // stream 3 trip this. The original code seqFatal'd here, then was
        // softened to drop the in-progress assembly — both wrong, the
        // continuation fragment data is real. Grow the buffer to fit,
        // capped at the same protocol-wide sanity ceiling enforced above.
        if (needed > 131072)
        {
          seqWarn("EQPacketFragmentSequence::addFragment(): runaway continuation past sanity limit, dropping: seq %04x stream %d opcode %04x, alloc %u filled to %u, tried to add %u",
            packet.arqSeq(), m_streamid, *(uint16_t*)(m_data),
            m_dataAllocSize, (unsigned)m_dataSize, packet.payloadLength());
          reset();
          return;
        }
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
