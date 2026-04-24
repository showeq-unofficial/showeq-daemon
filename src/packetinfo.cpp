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
#include <QXmlAttributes>
#include <QTextStream>
#include <QByteArray>

#include <map>

#include "packetinfo.h"
#include "packetcommon.h"
#include "everquest.h"
#include "diagnosticmessages.h"

#pragma message("Once our minimum supported Qt version is greater than 5.14, this check can be removed and ENDL replaced with Qt::endl")
#if (QT_VERSION >= QT_VERSION_CHECK(5,14,0))
#define ENDL Qt::endl
#else
#define ENDL endl
#endif

//----------------------------------------------------------------------
// Macros

// this define is used to diagnose problems with packet dispatch
// #define  PACKET_DISPATCH_DIAG 1

//----------------------------------------------------------------------
// OPCodeXmlContentHandler declaration
class OPCodeXmlContentHandler : public QXmlDefaultHandler
{
public:
  OPCodeXmlContentHandler(const EQPacketTypeDB& typeDB, 
			  EQPacketOPCodeDB& opcodeDB);
  virtual ~OPCodeXmlContentHandler();
  
  // QXmlContentHandler overrides
  bool startDocument();
  bool startElement( const QString&, const QString&, const QString& , 
		     const QXmlAttributes& );
  bool characters(const QString& ch);
  bool endElement( const QString&, const QString&, const QString& );
  bool endDocument();
  
protected:
  const EQPacketTypeDB& m_typeDB;
  EQPacketOPCodeDB& m_opcodeDB;

  EQPacketOPCode* m_currentOPCode;
  EQPacketPayload* m_currentPayload;
  
  QString m_currentComment;

  bool m_inComment;
};


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
  // load opcodes

  // create XML content handler
  OPCodeXmlContentHandler handler(typeDB, *this);

  // create a file object on the file
  QFile xmlFile(filename);

  // create an XmlInputSource on the file
  QXmlInputSource source(&xmlFile);
  
  // create an XML parser
  QXmlSimpleReader reader;

  // set the content handler
  reader.setContentHandler(&handler);

  // parse the file
  return reader.parse(source);
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
  out.setCodec("UTF-8");

  // set the number output to be left justified decimal
  out.setIntegerBase(10);
  out.setFieldAlignment(QTextStream::AlignLeft);

  // print document header
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << ENDL
      << "<!DOCTYPE seqopcodes SYSTEM \"seqopcodes.dtd\">" << ENDL
      << "<seqopcodes>" << ENDL;

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
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
    opcodeString = QString::asprintf("%04x", currentOPCode->opcode());
#else
    opcodeString.sprintf("%04x", currentOPCode->opcode());
#endif
    out << indent << "<opcode id=\"" << opcodeString << "\" name=\""
	<< currentOPCode->name() << "\"";
    if (currentOPCode->implicitLen())
      out << " implicitlen=\"" << currentOPCode->implicitLen() << "\"";
    if (!currentOPCode->updated().isEmpty())
      out << " updated=\"" << currentOPCode->updated() << "\"";
    out << ">" << ENDL;

    // increase the indent
    indent += "    ";

    // output the comments
    QStringList comments = currentOPCode->comments();
    for (QStringList::Iterator cit = comments.begin(); 
	 cit != comments.end(); ++cit)
      out << indent << "<comment>" << *cit << "</comment>" << ENDL;

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
	  << "\"/>" << ENDL;
    }

    // decrease the indent
    indent.remove(0, 4);

    // close the opcode entity
    out << indent << "</opcode>" << ENDL;
  }

  // output closing entity
  out << "</seqopcodes>" << ENDL;

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

    fprintf(stderr, "\t\t%d comment(s)\n", comments.count());

    for (QStringList::Iterator cit = comments.begin();
            cit != comments.end(); ++cit)
      fprintf(stderr, "\t\t\t'%s'\n", (*cit).toLatin1().data());

    fprintf(stderr, "\t\t%d payload(s)\n", current->count());

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

//----------------------------------------------------------------------
// OPCodeXmlContentHandler implementation
OPCodeXmlContentHandler::OPCodeXmlContentHandler(const EQPacketTypeDB& typeDB, 
						 EQPacketOPCodeDB& opcodeDB)
  : m_typeDB(typeDB),
    m_opcodeDB(opcodeDB)
{
}

OPCodeXmlContentHandler::~OPCodeXmlContentHandler()
{
}
  
  // QXmlContentHandler overrides
bool OPCodeXmlContentHandler::startDocument()
{
  // not in an opcode yet, so set the current OPCode object to NULL
  m_currentOPCode = NULL;
  m_currentPayload = NULL;
  m_inComment = false;;
  return true;
}

bool OPCodeXmlContentHandler::startElement(const QString&, const QString&, 
					   const QString& name, 
					   const QXmlAttributes& attr)
{
  if (name == "opcode")
  {
    bool ok = false;

    // get the index of the id attribute
    int index = attr.index("id");
    if (index == -1)
    {
      seqWarn("OPCodeXmlContentHandler::startElement(): opcode element without id!");
	      
      return false; // this is an error, something is wrong
    }

    // the id attribute is the opcode value
    uint16_t opcode = attr.value(index).toUShort(&ok, 16);

#if 0 // ZBTEMP
    opcode += 2;
#endif 

    if (!ok)
    {
      seqWarn("OPCodeXmlContentHandler::startElement(): opcode '%s' failed to convert to uint16_t (result: %#04x)",
	      attr.value(index).toLatin1().data(), opcode);

      return false; // this is an error
    }

    // get the index of the name attribute
    index = attr.index("name");
    
    // if name attribute was found, set the opcode objects name
    if (index == -1)
    {
      seqWarn("OPCodeXmlContentHandler::startElement(): opcode %#04x missing name parameter!",
	      opcode);

      return false;
    }

    // add/create the new opcode object
    m_currentOPCode = m_opcodeDB.add(opcode, attr.value(index));

    if (!m_currentOPCode)
    {
      seqWarn("Failed to add opcode %04x", opcode);
      return false;
    }


    // get the index of the updated attribute
    index = attr.index("updated");
    
    // if the updated attribute was found, set the objects updated field
    if (index != -1)
      m_currentOPCode->setUpdated(attr.value(index));

    // get the index of the implicitlen attribute
    index = attr.index("implicitlen");

    // if implicitlen attribute was found, set the objects implicitLen field
    if (index != -1)
      m_currentOPCode->setImplicitLen(attr.value(index).toUShort());

    return true;
  }

  if ((name == "comment") && (m_currentOPCode))
  {
    // clear any current comment
    m_currentComment = "";
    m_inComment = true;

    return true;
  }

  if ((name == "payload") && (m_currentOPCode))
  {
    // create a new payload object and make it the current one
    m_currentPayload = new EQPacketPayload();

    // add the payload object to the opcode
    m_currentOPCode->append(m_currentPayload);

    // check for direction attribute
    int index = attr.index("dir");

    // if an index attribute exists, then use it
    if (index != -1)
    {
      QString value = attr.value(index);

      if (value == "both")
	m_currentPayload->setDir(DIR_Client | DIR_Server);
      else if (value == "server")
	m_currentPayload->setDir(DIR_Server);
      else if (value == "client")
	m_currentPayload->setDir(DIR_Client);
    }

    // get the typename attribute
    index = attr.index("typename");

    // if a typename attribute exist, then set the payload type
    if (index != -1)
    {
      QString value = attr.value(index);
      
      if (!value.isEmpty())
      {
          if (!m_currentPayload->setType(m_typeDB, value.toLatin1().data()))
              seqWarn("Unknown payload typename '%s' for opcode '%04x'",
                      value.toLatin1().data(), m_currentOPCode->opcode());
      }
    }

    // attempt to retrieve the sizechecktype
    index = attr.index("sizechecktype");

    // if a sizechecktype exists, then set the payload size check type
    if (index != -1)
    {
      QString value = attr.value(index);

      if (value.isEmpty() || (value == "none"))
	m_currentPayload->setSizeCheckType(SZC_None);
      else if (value == "match")
	m_currentPayload->setSizeCheckType(SZC_Match);
      else if (value == "modulus")
	m_currentPayload->setSizeCheckType(SZC_Modulus);
    }

    return true;
  }

  return true;
}

bool OPCodeXmlContentHandler::characters(const QString& ch)
{
  // if in a <comment>, add the current characters to it's text
  if (m_inComment)
    m_currentComment += ch;
    
  return true;
}

bool OPCodeXmlContentHandler::endElement(const QString&, const QString&, 
					 const QString& name)
{
  if (name == "opcode")
  {
    // not currently in an opcode, so set the current OPCode object to NULL
    m_currentOPCode = NULL;

    return true;
  }

  if ((name == "comment") && (m_inComment))
  {
    m_inComment = false;
    if (m_currentOPCode)
      m_currentOPCode->addComment(m_currentComment);
  }

  if ((name == "payload") && (m_currentPayload))
    m_currentPayload = NULL;

  return true;
}

bool OPCodeXmlContentHandler::endDocument()
{
  return true;
}

#ifndef QMAKEBUILD
#include "packetinfo.moc"
#endif

