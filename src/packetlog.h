/*
 *  packetlog.h
 *  Copyright 2000-2005, 2009, 2019 by the respective ShowEQ Developers
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
 *
 */

#ifndef _PACKETLOG_H_
#define _PACKETLOG_H_

#include <QObject>
#include "logger.h"
#include "packet.h"

//----------------------------------------------------------------------
// forward declarations
class EQUDPIPPacketFormat;

//----------------------------------------------------------------------
// PacketLog
class PacketLog : public SEQLogger
{
  Q_OBJECT

 public:
  PacketLog(EQPacket& packet, const QString& fname, 
	    QObject* parent=0, const char* name = 0);
  virtual ~PacketLog();
  QString print_addr(in_addr_t addr);

 public slots:
  void logMessage(const QString& message);
  void logData (const uint8_t* data,
		size_t       len,
		const QString& prefix = QString());
  void logData (const uint8_t* data,
		size_t       len,
		uint8_t        dir,
		uint16_t       opcode,
		const QString& origPrefix = QString()); 
  void logData (const uint8_t* data,
		size_t len,
		uint8_t dir,
		uint16_t opcode, 
		const EQPacketOPCode* opcodeEntry,
		const QString& origPrefix = QString()); 
  void logData(const EQUDPIPPacketFormat& packet);
  void printData(const uint8_t* data, size_t len, uint8_t dir,
		 uint16_t opcode, const QString& origPrefix = QString());

 protected:
  QString m_timeDateFormat;
  EQPacket& m_packet;
  uint8_t m_dir;
};

//----------------------------------------------------------------------
// PacketStreamLog
class PacketStreamLog : public PacketLog
{
  Q_OBJECT

 public:
  PacketStreamLog(EQPacket& packet, const QString& fname, 
		  QObject* parent=0, const char* name = 0);
  bool raw();
  void setRaw(bool val);
  uint8_t getDir() { return m_dir; }
  void setDir(uint8_t direction) { m_dir = direction; }
  
 public slots:
   void rawStreamPacket(const uint8_t* data, size_t len, uint8_t dir, 
			uint16_t opcode);
   void decodedStreamPacket(const uint8_t* data, size_t len, uint8_t dir, 
			    uint16_t opcode, const EQPacketOPCode* opcodeEntry);

 protected:
   bool m_raw;
};

inline bool PacketStreamLog::raw()
{
  return m_raw;
}

inline void PacketStreamLog::setRaw(bool val)
{
  m_raw = val;
}

//----------------------------------------------------------------------
// UnknownPacketLog
class UnknownPacketLog : public PacketLog
{
  Q_OBJECT

 public:
  UnknownPacketLog(EQPacket& packet, const QString& fname, 
		   QObject* parent=0, const char* name = 0);
  bool view();
  void setView(bool val);

 public slots:
   void packet(const uint8_t* data, size_t len, uint8_t dir, 
	       uint16_t opcode, const EQPacketOPCode* opcodeEntry,
	       bool unknown);

 protected:
  bool m_view;
};

inline bool UnknownPacketLog::view()
{
  return m_view;
}

inline void UnknownPacketLog::setView(bool val)
{
  m_view = val;
}

//----------------------------------------------------------------------
// OPCodeMonitorPacketLog
class OPCodeMonitorPacketLog : public PacketLog
{
  Q_OBJECT

 public:
  OPCodeMonitorPacketLog(EQPacket& packet, const QString& fname, 
			 QObject* parent=0, const char* name = 0);
  bool log();
  void setLog(bool val);
  bool view();
  void setView(bool val);
  void init(QString monitoredOPCodes);

 public slots:
  void packet(const uint8_t* data, size_t len, uint8_t dir, 
	      uint16_t opcode, const EQPacketOPCode* opcodeEntry, 
	      bool unknown);

 protected:
#define OPCODE_SLOTS 15
  unsigned int MonitoredOpCodeList[OPCODE_SLOTS][3];
  QString MonitoredOpCodeAliasList[OPCODE_SLOTS];
  bool m_log;
  bool m_view;
};

inline bool OPCodeMonitorPacketLog::log()
{
  return m_log;
}

inline void OPCodeMonitorPacketLog::setLog(bool val)
{
  m_log = val;
}

inline bool OPCodeMonitorPacketLog::view()
{
  return m_view;
}

inline void OPCodeMonitorPacketLog::setView(bool val)
{
  m_view = val;
}

#endif // __PACKETLOG_H_
