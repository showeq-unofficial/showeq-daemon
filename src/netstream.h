/*
 *  netstream.h
 *  Copyright 2004 Zaphod (dohpaz@users.sourceforge.net).
 *  Copyright 2005-2008, 2016, 2019 by the respective ShowEQ Developers
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

#ifndef _NETSTREAM_H_
#define _NETSTREAM_H_

#include <cstdint>
#include <QString>

class NetStream
{
 public:
  NetStream(const uint8_t* data, size_t length);
  ~NetStream();

  const uint8_t* data() { return m_data; }
  size_t length() { return m_length; }
  void reset();
  bool end() { return (m_pos >= m_lastPos); }
  const uint8_t* pos() { return m_pos; }

  uint8_t readUInt8();
  int8_t readInt8();
  uint16_t readUInt16();
  int16_t readInt16();
  uint32_t readUInt32();
  int32_t readInt32();
  QString readText();		// read null-terminated string
  QString readLPText();		// read length-prefixed string
  uint16_t readUInt16NC();
  uint32_t readUInt32NC();
  void skipBytes(size_t byteCount);

 protected:
  const uint8_t* m_data;
  size_t m_length;
  const uint8_t* m_lastPos;
  const uint8_t* m_pos;
};

/**
 * A network stream that manages data by the bit. This is useful for
 * unpacking non-byte-aligned data.
 */
class BitStream
{
public:
    BitStream(const uint8_t* data, size_t length);
    ~BitStream();

    const uint8_t* data() { return m_data; }
    size_t length() { return m_totalBits >> 3; }
    void reset();
    bool end() { return (m_currentBit >= m_totalBits); }

    bool readBit();
    uint32_t readUInt(size_t bitCount);
    int32_t readInt(size_t bitCount);

protected:
    const uint8_t* m_data;
    size_t m_totalBits;
    size_t m_currentBit;
};

#endif // _NETSTREAM_H_


