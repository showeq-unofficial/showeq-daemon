/*
 *  packetinfo.cpp
 *  Copyright 2003-2004,2007 Zaphod (dohpaz@users.sourceforge.net).
 *  Copyright 2005-2007, 2019 by the respective ShowEQ Developers
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

#include <cstdio>

#include <QObject>
#include <QMetaObject>
#include <QFile>
#include <QTextStream>
#include <QByteArray>
#include <QXmlStreamReader>

#include <map>

#include "packetinfo.h"
#include "packetcommon.h"
#include "everquest.h"
#include "diagnosticmessages.h"


//----------------------------------------------------------------------
// Macros

// this define is used to diagnose problems with packet dispatch
// #define  PACKET_DISPATCH_DIAG 1


//----------------------------------------------------------------------
// EQPacketTypeDB
EQPacketTypeDB::EQPacketTypeDB()
  : m_typeSizeDict()
{
  // define the convenience macro used in the generated file
#define AddStruct(typeName) addStruct(#typeName, sizeof(typeName))

  // include the generated file
#include "s_everquest.h"

  // undefine the convenience macro
#undef AddStruct

  // these we add manually to handle strings and octet streams
  addStruct("char", sizeof(char));
  addStruct("uint8_t", sizeof(uint8_t));
  addStruct("none", 0);
  addStruct("unknown", 0);
}

EQPacketTypeDB::~EQPacketTypeDB()
{
}

size_t EQPacketTypeDB::size(const char* typeName) const
{
  // attempt to find the item in the type size dictionary
  size_t size = m_typeSizeDict.value(typeName, 0);

  return size;
}

bool EQPacketTypeDB::valid(const char* typeName) const
{
  // attempt to find the item in the type size dictionary
  size_t size = m_typeSizeDict.value(typeName, 0);

  return (size != 0);
}

void EQPacketTypeDB::list(void) const
{
  seqInfo("EQPacketTypeDB contains %d types (in %d buckets)",
	  m_typeSizeDict.count(), m_typeSizeDict.size());

  QHashIterator<QByteArray, size_t> it(m_typeSizeDict);

  while (it.hasNext())
  {
    it.next();
    seqInfo("\t%s = %d", it.key().data(), it.value());
  }
}

void EQPacketTypeDB::addStruct(const char* typeName, size_t size)
{
  m_typeSizeDict.insert(typeName, size);
}

//----------------------------------------------------------------------
// EQPacketDispatch
EQPacketDispatch::EQPacketDispatch(QObject* parent, const char* name)
  : QObject(parent)
{
    setObjectName(name);
}

EQPacketDispatch::~EQPacketDispatch()
{
}

void EQPacketDispatch::activate(const uint8_t* data, size_t len, uint8_t dir)
{
  emit signal(data, len, dir);
}

bool EQPacketDispatch::connect(const QObject* receiver, const char* member)
{
#ifdef PACKET_DISPATCH_DIAG
  seqDebug("Connecting '%s:%s' to '%s:%s' objects %s.",
	  className(), name(), receiver->className(), receiver->name(),
	  (const char*)member);
#endif

  return QObject::connect((QObject*)this, 
			  SIGNAL(signal(const uint8_t*, size_t, uint8_t)),
			  receiver, member);
}

bool EQPacketDispatch::disconnect(const QObject* receiver, const char* member)
{
  return QObject::disconnect((QObject*)this,
			     SIGNAL(signal(const uint8_t*, size_t, uint8_t)),
			     receiver, member);
}

//----------------------------------------------------------------------
// EQPacketPayload
EQPacketPayload::EQPacketPayload()
  : m_typeSize(0),
    m_sizeCheckType(SZC_None),
    m_dir(0x00)
{
}

EQPacketPayload::~EQPacketPayload()
{
}

bool EQPacketPayload::setType(const EQPacketTypeDB& db, 
			      const char* typeName)
{
  // first, check that it is a valid type
  if (!db.valid(typeName))
    return false;

  // valid type, ok, use it
  m_typeName = typeName;

  // get the types size
  m_typeSize = db.size(typeName);

  return true;
}

bool EQPacketPayload::match(const uint8_t* data, size_t size, 
			    uint8_t dir) const
{
  switch(m_sizeCheckType)
  {
  case SZC_None:
    return ((m_dir & dir) != 0);
  case SZC_Match:
    return (((m_dir & dir) != 0) &&
	    (m_typeSize == size));
  case SZC_Modulus:
    return (((m_dir & dir) != 0) &&
	    ((size % m_typeSize) == 0));
  default:
    break;
  }

  return false;
}


//----------------------------------------------------------------------
// EQPacketOPCode
EQPacketOPCode::EQPacketOPCode()
  : m_opcode(0),
    m_implicitLen(0)
{
}

EQPacketOPCode::EQPacketOPCode(uint16_t opcode, const QString& name)
  : m_opcode(opcode),
    m_implicitLen(0),
    m_name(name)
{
}

EQPacketOPCode::EQPacketOPCode(const EQPacketOPCode& opcode)
  : m_opcode(opcode.m_opcode),
    m_implicitLen(opcode.m_implicitLen),
    m_name(opcode.m_name),
    m_updated(opcode.m_updated)
{
}

EQPacketOPCode::~EQPacketOPCode()
{
    qDeleteAll(*this);
    clear();
}

EQPacketPayload* EQPacketOPCode::find(const uint8_t* data, size_t size, uint8_t dir) const
{
  EQPacketPayload* payload;

  // iterate over the payloads until a matching one is found
  EQPayloadListIterator it(*this);
  while (it.hasNext())
  {
    payload = it.next();
    if (!payload)
        break;
    // if a match is found, return it.
    if (payload->match(data, size, dir))
      return payload;
  }

  // no matches, return 0
  return 0;
}


//----------------------------------------------------------------------
// EQPacketOPCodeDB
EQPacketOPCodeDB::EQPacketOPCodeDB()
  : m_opcodes()
{
}

EQPacketOPCodeDB::~EQPacketOPCodeDB()
{
    while(!m_opcodesByName.isEmpty()) {
        remove(m_opcodesByName.begin().key());
    }
}

bool EQPacketOPCodeDB::load(const EQPacketTypeDB& typeDB,
			    const QString& filename)
{
  QFile xmlFile(filename);
  if (!xmlFile.open(QIODevice::ReadOnly))
    return false;

  QXmlStreamReader reader(&xmlFile);

  // State formerly carried across SAX callbacks; now locals over the
  // pull loop. m_inComment is gone — `<comment>` text is read directly
  // when StartElement/EndElement bracket it.
  EQPacketOPCode* currentOPCode = nullptr;
  EQPacketPayload* currentPayload = nullptr;

  while (!reader.atEnd())
  {
    reader.readNext();

    if (reader.isStartElement())
    {
      // QXmlStreamReader::name() returns QStringRef on Qt5 and
      // QStringView on Qt6 — use auto so this builds against both.
      const auto name = reader.name();
      const QXmlStreamAttributes attr = reader.attributes();

      if (name == QLatin1String("opcode"))
      {
        if (!attr.hasAttribute(QLatin1String("id")))
        {
          seqWarn("EQPacketOPCodeDB::load: opcode element without id!");
          return false;
        }

        bool ok = false;
        const QString idStr = attr.value(QLatin1String("id")).toString();
        const uint16_t opcode = idStr.toUShort(&ok, 16);
        if (!ok)
        {
          seqWarn("EQPacketOPCodeDB::load: opcode '%s' failed to convert to uint16_t (result: %#04x)",
                  idStr.toLatin1().data(), opcode);
          return false;
        }

        if (!attr.hasAttribute(QLatin1String("name")))
        {
          seqWarn("EQPacketOPCodeDB::load: opcode %#04x missing name parameter!",
                  opcode);
          return false;
        }

        currentOPCode = add(opcode, attr.value(QLatin1String("name")).toString());
        if (!currentOPCode)
        {
          seqWarn("Failed to add opcode %04x", opcode);
          return false;
        }

        if (attr.hasAttribute(QLatin1String("updated")))
          currentOPCode->setUpdated(attr.value(QLatin1String("updated")).toString());

        if (attr.hasAttribute(QLatin1String("implicitlen")))
          currentOPCode->setImplicitLen(
            attr.value(QLatin1String("implicitlen")).toUShort());

        continue;
      }

      if (name == QLatin1String("comment") && currentOPCode)
      {
        // readElementText consumes through the matching </comment>,
        // accumulating any character data — same effect as the SAX
        // handler's m_currentComment buffer.
        currentOPCode->addComment(reader.readElementText());
        continue;
      }

      if (name == QLatin1String("payload") && currentOPCode)
      {
        currentPayload = new EQPacketPayload();
        currentOPCode->append(currentPayload);

        if (attr.hasAttribute(QLatin1String("dir")))
        {
          const auto dir = attr.value(QLatin1String("dir"));
          if (dir == QLatin1String("both"))
            currentPayload->setDir(DIR_Client | DIR_Server);
          else if (dir == QLatin1String("server"))
            currentPayload->setDir(DIR_Server);
          else if (dir == QLatin1String("client"))
            currentPayload->setDir(DIR_Client);
        }

        if (attr.hasAttribute(QLatin1String("typename")))
        {
          const QString typeName = attr.value(QLatin1String("typename")).toString();
          if (!typeName.isEmpty())
          {
            if (!currentPayload->setType(typeDB, typeName.toLatin1().data()))
              seqWarn("Unknown payload typename '%s' for opcode '%04x'",
                      typeName.toLatin1().data(),
                      currentOPCode->opcode());
          }
        }

        if (attr.hasAttribute(QLatin1String("sizechecktype")))
        {
          const auto szt = attr.value(QLatin1String("sizechecktype"));
          if (szt.isEmpty() || szt == QLatin1String("none"))
            currentPayload->setSizeCheckType(SZC_None);
          else if (szt == QLatin1String("match"))
            currentPayload->setSizeCheckType(SZC_Match);
          else if (szt == QLatin1String("modulus"))
            currentPayload->setSizeCheckType(SZC_Modulus);
        }

        continue;
      }
    }
    else if (reader.isEndElement())
    {
      const auto name = reader.name();
      if (name == QLatin1String("opcode"))
        currentOPCode = nullptr;
      else if (name == QLatin1String("payload"))
        currentPayload = nullptr;
    }
  }

  return !reader.hasError();
}

bool EQPacketOPCodeDB::save(const QString& filename)
{
  // create QFile object
  QFile file(filename);

  // open the file for write only
  if (!file.open(QIODevice::WriteOnly))
    return false;

  // create a QTextStream object on the QFile object
  QTextStream out(&file);

  // set the output encoding to be UTF8
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
  out.setEncoding(QStringConverter::Utf8);
#else
  out.setCodec("UTF-8");
#endif

  // set the number output to be left justified decimal
  out.setIntegerBase(10);
  out.setFieldAlignment(QTextStream::AlignLeft);

  // print document header
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << Qt::endl
      << "<!DOCTYPE seqopcodes SYSTEM \"seqopcodes.dtd\">" << Qt::endl
      << "<seqopcodes>" << Qt::endl;

  // set initial indent
  QString indent = "    ";

  EQPacketOPCode* currentOPCode;
  EQPacketPayload* currentPayload;

  typedef std::map<long, EQPacketOPCode*> OrderedMap;
  OrderedMap orderedOPCodes;

  // iterate over all the opcodes, inserting them into the ordered map
  QHashIterator<int, EQPacketOPCode*> it(m_opcodes);
  while (it.hasNext())
  {
    it.next();
    if (!it.value())
        break;
    currentOPCode = it.value();
    // insert into the ordered opcode map
    orderedOPCodes.insert(OrderedMap::value_type(currentOPCode->opcode(), 
						 currentOPCode));

  }

  OrderedMap::iterator oit;
  QString opcodeString;
  static const char* dirStrs[] = { "client", "server", "both", };
  static const char* sztStrs[] = { "none", "match", "modulus", };

  // iterate over the ordered opcode map
  for (oit = orderedOPCodes.begin(); oit != orderedOPCodes.end(); ++oit)
  {
    currentOPCode = oit->second;

    // output the current opcode
    opcodeString = QString::asprintf("%04x", currentOPCode->opcode());
    out << indent << "<opcode id=\"" << opcodeString << "\" name=\""
	<< currentOPCode->name() << "\"";
    if (currentOPCode->implicitLen())
      out << " implicitlen=\"" << currentOPCode->implicitLen() << "\"";
    if (!currentOPCode->updated().isEmpty())
      out << " updated=\"" << currentOPCode->updated() << "\"";
    out << ">" << Qt::endl;

    // increase the indent
    indent += "    ";

    // output the comments
    QStringList comments = currentOPCode->comments();
    for (QStringList::Iterator cit = comments.begin(); 
	 cit != comments.end(); ++cit)
      out << indent << "<comment>" << *cit << "</comment>" << Qt::endl;

    QByteArray dirStr;
    QByteArray sztStr;

    // iterate over the payloads
    QListIterator<EQPacketPayload*> pit(*currentOPCode);
    while (pit.hasNext())
    {
      currentPayload = pit.next();
      if (!currentPayload)
          break;

      // output the payload
      out << indent << "<payload dir=\"" << dirStrs[currentPayload->dir()-1]
	  << "\" typename=\"" << currentPayload->typeName() 
	  << "\" sizechecktype=\""
	  << sztStrs[currentPayload->sizeCheckType()]
	  << "\"/>" << Qt::endl;
    }

    // decrease the indent
    indent.remove(0, 4);

    // close the opcode entity
    out << indent << "</opcode>" << Qt::endl;
  }

  // output closing entity
  out << "</seqopcodes>" << Qt::endl;

  return true;
}

EQPacketOPCode* EQPacketOPCodeDB::add(uint16_t opcode, const QString& name)
{
  // Create the new opcode object
  EQPacketOPCode* newOPCode = new EQPacketOPCode(opcode, name);

  // insert the opcode into the opcode table
  m_opcodes.insert(opcode, newOPCode);

  // insert the object into the opcode by name table
  m_opcodesByName.insert(name, newOPCode);

  // return the opcode object
  return newOPCode;
}

void EQPacketOPCodeDB::list(void) const
{
  seqInfo("EQPacketOPCodeDB contains %d opcodes (in %d buckets)",
	  m_opcodes.count(), m_opcodes.size());

  EQPacketOPCode* current;
  EQPacketPayload* currentPayload;

  // iterate over all the opcodes
  QHashIterator<int, EQPacketOPCode*> it(m_opcodes);
  while (it.hasNext())
  {
    it.next();
    if (!it.value())
        break;
    current = it.value();
    fprintf(stderr, "\tkey=%04x opcode=%04x",
	    it.key(), current->opcode());
    if (!current->name().isNull())
      fprintf(stderr, " name='%s'", current->name().toLatin1().data());

    if (current->implicitLen())
      fprintf(stderr, " implicitlen='%d'", current->implicitLen());

    if (!current->updated().isNull())
      fprintf(stderr, " updated='%s'", current->updated().toLatin1().data());

    fputc('\n', stderr);

    QStringList comments = current->comments();

    fprintf(stderr, "\t\t%lld comment(s)\n", (long long)comments.count());

    for (QStringList::Iterator cit = comments.begin();
            cit != comments.end(); ++cit)
      fprintf(stderr, "\t\t\t'%s'\n", (*cit).toLatin1().data());

    fprintf(stderr, "\t\t%lld payload(s)\n", (long long)current->count());

    QListIterator<EQPacketPayload*> pit(*current);
    while (pit.hasNext())
    {
      currentPayload = pit.next();
      if (!currentPayload)
          break;

      seqInfo("\t\t\tdir=%d typename=%s size=%d sizechecktype=%d",
	      currentPayload->dir(), currentPayload->typeName().toLatin1().data(),
	      currentPayload->typeSize(), currentPayload->sizeCheckType());
    }
  }
}

bool EQPacketOPCodeDB::remove(uint16_t opcode)
{
  // remove the opcode object from the opcodes table
  EQPacketOPCode* opcodeObj = m_opcodes.take(opcode);

  if (opcodeObj)
  {
    // remove it from the opcodes by name table
    m_opcodesByName.remove(opcodeObj->name());

    // delete the opcode object
    delete opcodeObj;

    return true;
  }

  return false;
}

bool EQPacketOPCodeDB::remove(const QString& opcodeName)
{
  // remove the opcode object from the opcodes table
  EQPacketOPCode* opcode = m_opcodesByName.take(opcodeName);

  // if found, remove it from the opcodes table
  if (opcode)
  {
      m_opcodes.remove(opcode->opcode());
      delete opcode;

      return true;
  }

  return false;
}

bool EQPacketOPCodeDB::move(uint16_t oldOPCode, uint16_t newOPCode)
{
  // attempt to take an existing opcode object out of the table
  EQPacketOPCode* opcode = m_opcodes.take(oldOPCode);

  // if failed to find an existing opcode object, return failure
  if (!opcode)
    return false;

  // set the new opcode value within the object
  opcode->setOPCode(newOPCode);

  // reinsert the object into the table under the new opcode id
  m_opcodes.insert(newOPCode, opcode);

  return true;
}


bool EQPacketOPCodeDB::move(const QString& oldOPCodeName,
			    const QString& newOPCodeName)
{
  // attempt to take an existing opcode object out of the table
  EQPacketOPCode* opcode = m_opcodesByName.take(oldOPCodeName);

  // if failed to find an existing opcode object, return failure
  if (!opcode)
    return false;

  // set the new opcode value within the object
  opcode->setName(newOPCodeName);

  // reinsert the object into the table under the new opcode id
  m_opcodesByName.insert(newOPCodeName, opcode);

  return true;
}

