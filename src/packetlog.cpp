/*
 *  packetlog.cpp
 *  Copyright 2000-2005, 2009, 2019 by the respective ShowEQ Developers
 *  Portions Copyright 2001-2004,2007 Zaphod (dohpaz@users.sourceforge.net).
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

#include <QDateTime>

#include "packetlog.h"
#include "packetformat.h"
#include "packetinfo.h"
#include "decode.h"
#include "diagnosticmessages.h"

#pragma message("Once our minimum supported Qt version is greater than 5.14, this check can be removed and ENDL replaced with Qt::endl")
#if (QT_VERSION >= QT_VERSION_CHECK(5,14,0))
#define ENDL Qt::endl
#else
#define ENDL endl
#endif

//----------------------------------------------------------------------
// PacketLog
PacketLog::PacketLog(EQPacket& packet, const QString& fname, 
		     QObject* parent, const char* name)
  : SEQLogger(fname, parent, name),
    m_packet(packet),
    m_dir(0)
{
  m_timeDateFormat = "MMM dd yyyy hh:mm:ss:zzz";
}

PacketLog::~PacketLog()
{
}

inline QString opCodeToString(uint16_t opCode)
{
  QString tempStr;
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
  tempStr = QString::asprintf("[OPCode: %#.04x]", opCode);
#else
  tempStr.sprintf("[OPCode: %#.04x]", opCode);
#endif

  // Flags are gone? Combined and implicit don't make sense anymore and
  // nothing is compressed or encrypted at this point...
  /*
  if (opCode & FLAG_DECODE)
  {
    tempStr += " ";
    if (opCode & FLAG_COMP)
      tempStr += "[FLAG_COMP]";
    if (opCode & FLAG_COMBINED)
      tempStr += "[FLAG_COMBINED]";
    if (opCode & FLAG_CRYPTO)
      tempStr += "[FLAG_CRYPTO]";
    if (opCode & FLAG_IMPLICIT)
      tempStr += "[FLAG_IMPLICIT]";
  }
  */

  return tempStr;
}

/* Returns string representation of numeric IP address */
QString PacketLog::print_addr (in_addr_t  addr)
{
#ifdef DEBUG_PACKET
   qDebug ("print_addr()");
#endif /* DEBUG_PACKET */
  QString paddr;

  if (addr == m_packet.clientAddr())
    paddr = "client";
  else
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
    paddr = QString::asprintf( "%d.%d.%d.%d",
		   addr & 0x000000ff,
		   (addr & 0x0000ff00) >> 8,
		   (addr & 0x00ff0000) >> 16,
		   (addr & 0xff000000) >> 24);
#else
    paddr.sprintf( "%d.%d.%d.%d",
		   addr & 0x000000ff,
		   (addr & 0x0000ff00) >> 8,
		   (addr & 0x00ff0000) >> 16,
		   (addr & 0xff000000) >> 24);
#endif

   return paddr;
}

/* Makes a note in a log */
void PacketLog::logMessage(const QString& message)
{
  if (!open())
    return;

   outputf ("%s\n", message.toLatin1().data());

   flush();
}

/* Logs packet data in a human-readable format */
void PacketLog::logData(const uint8_t* data,
			size_t       len,
			const QString& prefix)
{
  if (!open())
    return;

  if (!prefix.isEmpty())
    outputf("%s ", prefix.toUtf8().data());

   outputf("[Size: %d] %s\n",
           len,
           QDateTime::currentDateTime().toString(m_timeDateFormat).toUtf8().data());

   // make sure there is a len before attempting to print it
   if (len)
     outputData(len, data);
   else
     outputf("\n");

  flush();
}

/* Logs packet data in a human-readable format */
void PacketLog::logData(const uint8_t* data,
			size_t len,
			uint8_t dir,
			uint16_t opcode,
			const QString& origPrefix)
{
  if (!open())
    return;

  // timestamp
  m_out << QDateTime::currentDateTime().toString(m_timeDateFormat) << " ";

  // append direction and opcode information
  if (!origPrefix.isEmpty())
    m_out << origPrefix << " ";
  
  // data direction and size
  m_out << ((dir == DIR_Server) ? "[Server->Client] " : "[Client->Server] ")
      << "[Size: " << QString::number(len) << "]" << ENDL;

  // output opcode info
  m_out << opCodeToString(opcode) << ENDL;

  flush();

  // make sure there is a len before attempting to output it
  if (len)
    outputData(len, data);
  else
    m_out << ENDL;

  flush();
}

/* Logs packet data in a human-readable format */
void PacketLog::logData(const uint8_t* data,
			size_t len,
			uint8_t dir,
			uint16_t opcode,
			const EQPacketOPCode* opcodeEntry,
			const QString& origPrefix)
{
  if (!open())
    return;

  if (showeq_params->filterZoneDataLog && showeq_params->filterZoneDataLog != dir)
     return;
  
  // timestamp
  m_out << QDateTime::currentDateTime().toString(m_timeDateFormat) << " ";

  // append direction and opcode information
  if (!origPrefix.isEmpty())
    m_out << origPrefix << " ";
  
  // data direction and size
  m_out << ((dir == DIR_Server) ? "[Server->Client] " : "[Client->Server] ")
      << "[Size: " << QString::number(len) << "]" << ENDL;

  // output opcode info
  m_out << opCodeToString(opcode) << ENDL;

  if (opcodeEntry)
  {
    m_out << "[Name: " << opcodeEntry->name() << "][Updated: " 
	  << opcodeEntry->updated() << "]";
    const EQPacketPayload* payload = opcodeEntry->find(data, len, dir);
    if (payload)
    {
      m_out << "[Type: " << payload->typeName() << " (" 
	    << payload->typeSize() << ")";
      switch (payload->sizeCheckType())
      {
      case SZC_Match:
	m_out << " ==]";
	break;
      case SZC_Modulus:
	m_out << " %]";
	break;
      case SZC_None:
	m_out << " nc]";
	break;
      default:
	m_out << " " << payload->sizeCheckType() << "]";
	break;
      }
    }

    m_out  << ENDL;
  }

  flush();

  // make sure there is a len before attempting to output it
  if (len)
    outputData(len, data);
  else
    m_out << ENDL;

  flush();
}

void PacketLog::logData(const EQUDPIPPacketFormat& packet)
{
  if (!open())
    return;

  QString sourceStr, destStr;
  if (packet.getIPv4SourceN() == m_packet.clientAddr())
    sourceStr = "client";
  else
    sourceStr = packet.getIPv4SourceA();

  if (packet.getIPv4DestN() == m_packet.clientAddr())
    destStr = "client";
  else
    destStr = packet.getIPv4DestA();
  
  m_out << QDateTime::currentDateTime().toString(m_timeDateFormat)
      << " [" << sourceStr << ":" << QString::number(packet.getSourcePort())
      << "->" << destStr << ":" << QString::number(packet.getDestPort()) 
      << "] [Size: " << QString::number(packet.getUDPPayloadLength()) << "]"
      << ENDL;

  uint16_t calcedCRC;

  if (! packet.hasCRC() || 
    packet.crc() == (calcedCRC = ::calcCRC16(
      packet.rawPacket(), packet.rawPacketLength()-2, 
        packet.getSessionKey())))
  {
    m_out << "[OPCode: 0x" << QString::number(packet.getNetOpCode(), 16) << "]";

    if (packet.hasArqSeq())
    {
      m_out << " [Seq: " << QString::number(packet.arqSeq(), 16) << "]";
    }

    if (packet.hasFlags())
    {
      m_out << " [Flags: " << QString::number(packet.getFlags(), 16) << "]";
    }

    if (packet.hasCRC())
    {
      m_out << " [CRC ok]" << ENDL;
    }

    m_out << ENDL;
  }
  else
  {
    m_out << "[BAD CRC (" << QString::number(calcedCRC, 16) 
	  << " != " << QString::number(packet.crc(), 16) 
	  << ")! Sessions crossed or unitialized or non-EQ packet! ]" << ENDL;
    m_out << "[SessionKey: " << QString::number(packet.getSessionKey(), 16) <<
      "]" << ENDL;
  }

  flush();

  // make sure there is a len before attempting to output it
  if (packet.payloadLength())
    outputData(packet.getUDPPayloadLength(), 
	      (const uint8_t*)packet.getUDPPayload());
  else
    m_out << ENDL;

  flush();
}

void PacketLog::printData(const uint8_t* data, size_t len, uint8_t dir,
			  uint16_t opcode, const QString& origPrefix)
{
  if (!origPrefix.isEmpty())
    ::printf("%s ", origPrefix.toLatin1().data());
  else
    ::putchar('\n');

  ::printf("%s [Size: %lu]%s\n",
          ((dir == DIR_Server) ? "[Server->Client]" : "[Client->Server]"),
          len, opCodeToString(opcode).toLatin1().data());

  if (len)
  {
    for (size_t a = 0; a < len; a ++)
    {
      if ((data[a] >= 32) && (data[a] <= 126))
	::putchar(data[a]);
      else
	::putchar('.');
    }
    
    ::putchar('\n');
    
    for (size_t a = 0; a < len; a ++)
    {
      if (data[a] < 32)
	::putchar(data[a] + 95);
      else if (data[a] > 126)
	::putchar(data[a] - 95);
      else if (data[a] > 221)
	::putchar(data[a] - 190);
      else
	::putchar(data[a]);
    }
    
    ::putchar('\n');
    
    
    for (size_t a = 0; a < len; a ++)
      ::printf ("%02X", data[a]);

    ::printf("\n\n"); /* Adding an extra line break makes it easier
			 for people trying to decode the OpCodes to
			 tell where the raw data ends and the next
			 message begins...  -Andon */
  }
  else
    ::putchar('\n');
}

//----------------------------------------------------------------------
// PacketStreamLog
PacketStreamLog::PacketStreamLog(EQPacket& packet, const QString& fname, 
				 QObject* parent, const char* name)
  : PacketLog(packet, fname, parent, name),
    m_raw(true)
{
}

void PacketStreamLog::rawStreamPacket(const uint8_t* data, size_t len, 
				 uint8_t dir, uint16_t opcode)
{
  if (m_raw)
    logData(data, len, dir, opcode, "[Raw]");
}

void PacketStreamLog::decodedStreamPacket(const uint8_t* data, size_t len, 
					  uint8_t dir, uint16_t opcode, 
					  const EQPacketOPCode* opcodeEntry)
{
  //  if ((opcode != 0x0028) && (opcode != 0x003f) && (opcode != 0x025e))
    logData(data, len, dir, opcode, opcodeEntry, "[Decoded]");
}

//----------------------------------------------------------------------
// UnknownPacketLog
UnknownPacketLog::UnknownPacketLog(EQPacket& packet, const QString& fname, 
				   QObject* parent, const char* name)
  : PacketLog(packet, fname, parent, name),
    m_view(false)
{
}

void UnknownPacketLog::packet(const uint8_t* data, size_t len, uint8_t dir, 
			      uint16_t opcode, 
			      const EQPacketOPCode* opcodeEntry, bool unknown)
{
  if (unknown)
  {
    logData(data, len, dir, opcode, opcodeEntry);
   
    if (m_view)
      printData(data, len, dir, opcode, "Unknown");
  }
}

//----------------------------------------------------------------------
// OpCodeMonitorPacketLog
OPCodeMonitorPacketLog::OPCodeMonitorPacketLog(EQPacket& packet, 
					       const QString& fname, 
					       QObject* parent, 
					       const char* name)
  : PacketLog(packet, fname, parent, name),
    m_log(false), 
    m_view(false)
{
}

void OPCodeMonitorPacketLog::init(QString monitoredOPCodes)
{
  if (monitoredOPCodes.isEmpty() || monitoredOPCodes == "0") /* DISABLED */
  {
    seqWarn("OpCode monitoring COULD NOT BE ENABLED!");
    seqWarn(">> Please check your showeq.xml file for a list entry under [OpCodeMonitoring]");
    return;
  }

  seqInfo("OpCode monitoring ENABLED...");
  seqInfo("Using list:\t%s", monitoredOPCodes.toLatin1().data());


  QString qsCommaBuffer (""); /* Construct these outside the loop so we don't have to construct
				 and destruct these variables during each iteration... */
  QString qsColonBuffer ("");
  
  int            iCommaPos      = 0;
  int            iColonPos      = 0;
  uint8_t        uiIterationID  = 0;
  
  for (uint8_t uiIndex = 0; (uiIndex < OPCODE_SLOTS) && !monitoredOPCodes.isEmpty(); uiIndex ++)
  {
    /* Initialize the variables with their default values */
    MonitoredOpCodeList      [uiIndex] [0] = 0; /* OpCode number (16-bit HEX) */
    MonitoredOpCodeList      [uiIndex] [1] = 3; /* Direction, DEFAULT: Client <--> Server */
    MonitoredOpCodeList      [uiIndex] [2] = 1; /* Show raw data if packet is marked as known */
    MonitoredOpCodeAliasList [uiIndex]     = "Monitored OpCode"; /* Name used when displaying the raw data */

    iCommaPos = monitoredOPCodes.indexOf(",");

    if (iCommaPos == -1)
      iCommaPos = monitoredOPCodes.length ();

    qsCommaBuffer = monitoredOPCodes.left (iCommaPos);
    monitoredOPCodes.remove (0, iCommaPos + 1);

    uiIterationID = 0;

    uint8_t uiColonCount = qsCommaBuffer.count(":");
    iColonPos = qsCommaBuffer.indexOf(":");

    if (iColonPos == -1)
      qsColonBuffer  = qsCommaBuffer;

    else
      qsColonBuffer = qsCommaBuffer.left (iColonPos);

    while (uiIterationID <= uiColonCount)
    {
      if (uiIterationID == 0)
          MonitoredOpCodeList [uiIndex] [0] = qsColonBuffer.toUInt (NULL, 16);

      else if (uiIterationID == 1)
          MonitoredOpCodeAliasList [uiIndex] = qsColonBuffer;

      else if (uiIterationID == 2)
          MonitoredOpCodeList [uiIndex] [1] = qsColonBuffer.toUInt (NULL, 10);

      else if (uiIterationID == 3)
          MonitoredOpCodeList [uiIndex] [2] = qsColonBuffer.toUInt (NULL, 10);

      qsCommaBuffer.remove (0, iColonPos + 1);

      iColonPos = qsCommaBuffer.indexOf(":");

      if (iColonPos == -1)
          qsColonBuffer = qsCommaBuffer;

      else
          qsColonBuffer = qsCommaBuffer.left (iColonPos);

      uiIterationID ++;
    }

#if 1 // ZBTEMP
    seqDebug("opcode=%04x name='%s' dir=%d known=%d",
            MonitoredOpCodeList [uiIndex] [0],
            MonitoredOpCodeAliasList [uiIndex].toLatin1().data(),
            MonitoredOpCodeList [uiIndex] [1],
            MonitoredOpCodeList [uiIndex] [2]);
#endif
  }
}

void OPCodeMonitorPacketLog::packet(const uint8_t* data, size_t len, 
				    uint8_t dir, uint16_t opcode, 
				    const EQPacketOPCode* opcodeEntry, 
				    bool unknown)
{
  unsigned int uiOpCodeIndex = 0;
  unsigned int uiIndex = 0;
    
  for (; ((uiIndex < OPCODE_SLOTS) && (uiOpCodeIndex == 0)); uiIndex ++)
  {
    if (opcode == MonitoredOpCodeList[ uiIndex ][ 0 ])
    {
      if ((MonitoredOpCodeList[ uiIndex ][ 1 ] == dir) || 
	  (MonitoredOpCodeList[ uiIndex ][ 1 ] == 3))
      {
	if ((!unknown && (MonitoredOpCodeList[ uiIndex ][ 2 ] == 1)) 
	    || unknown)
	  uiOpCodeIndex = uiIndex + 1;
      }
    }
  }

  if (uiOpCodeIndex > 0)
  {
    QString opCodeName = MonitoredOpCodeAliasList[uiOpCodeIndex - 1].toLatin1().data();

    if (m_view)
      printData(data, len, dir, opcode, opCodeName);

    if (m_log)
      logData(data, len, dir, opcode, opcodeEntry,opCodeName);
  }
}

#ifndef QMAKEBUILD
#include "packetlog.moc"
#endif

