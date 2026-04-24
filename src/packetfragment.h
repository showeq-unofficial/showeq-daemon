/*
 *  packetfragment.h
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

#ifndef _PACKETFRAGMENT_H_
#define _PACKETFRAGMENT_H_

#include <sys/types.h>

#include "packetcommon.h"

class EQProtocolPacket;

//----------------------------------------------------------------------
// EQPacketFragmentSequence
class EQPacketFragmentSequence
{
 public:
  EQPacketFragmentSequence();
  EQPacketFragmentSequence(EQStreamID streamid);
  ~EQPacketFragmentSequence();
  void reset();
  void addFragment(EQProtocolPacket& packet);
  bool isComplete();

  uint8_t* data();
  size_t size();

 protected:
  EQStreamID m_streamid;
  uint8_t *m_data;
  uint32_t m_totalLength;
  size_t m_dataSize;
  uint32_t m_dataAllocSize;
};

inline bool EQPacketFragmentSequence::isComplete()
{
  return m_dataSize != 0 && m_totalLength == m_dataSize;
}

inline uint8_t* EQPacketFragmentSequence::data()
{
  return m_data;
}

inline size_t EQPacketFragmentSequence::size()
{
  return m_dataSize;
}

#endif // _PACKETFRAGMENT_H_
