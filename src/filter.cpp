/*
 *  filter.cpp - regex filter module
 *  Copyright 2001-2005, 2012, 2019 by the respective ShowEQ Developers
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

/* Implementation of filter class */
#include "filter.h"
#include "diagnosticmessages.h"
#include "everquest.h"

#include <sys/types.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <climits>

#include <QFile>
#include <QTextStream>
#include <QXmlStreamReader>

#define MAXLEN   5000

#if (QT_VERSION >= QT_VERSION_CHECK(5,14,0))
#define ENDL Qt::endl
#else
#define ENDL endl
#endif

//#define DEBUG_FILTER

#define X(a, b) #b,
const QString FilterStringFieldName[FSF_Max] = {
    FILTERSTRINGFIELD_TABLE
};
#undef X

#define X(a, b) #b,
const QString FilterStringInfoFieldName[FSIF_Max] = {
    FILTERSTRINGINFOFIELD_TABLE
};
#undef X


//----------------------------------------------------------------------
//  LoadXmlContentHandler declaration
class LoadXmlContentHandler : public QObject
{
public:
  LoadXmlContentHandler(Filters& filters, const FilterTypes& types);
  virtual ~LoadXmlContentHandler();
  
  // QXmlContentHandler overrides
  bool startDocument();
  bool startElement( const QString&, const QString&, const QString& , 
		     const QXmlStreamAttributes& );
  bool characters(const QString& ch);
  bool endElement( const QString&, const QString&, const QString& );
  bool endDocument();
  
protected:
  Filters& m_filters;
  const FilterTypes& m_types;
  
  uint8_t m_currentType;
  
  QString m_currentFilterPattern;
  uint8_t m_currentMinLevel;
  uint8_t m_currentMaxLevel;
  bool m_inRegex;
};

//----------------------------------------------------------------------
// FilterItem
/* FilterItem Class - allows easy creation / deletion of regex types */
FilterItem::FilterItem(const QString& filterPattern, bool caseSensitive)
{
  uint32_t minLevel = 0;
  uint32_t maxLevel = 0;
  
  QString workString = filterPattern;
  
  QString regexString = workString;

  // find the semi-colon that seperates the regex from the level info
  int breakPoint = workString.indexOf(';');

  // if no semi-colon, then it's all a regex
  if (breakPoint == -1)
    regexString = workString;
  else
  {
    // regex is the left most part of the string up to breakPoint characters
    regexString = workString.left(breakPoint);

    // the level string is everything else
    QString levelString = workString.mid(breakPoint + 1);

#ifdef DEBUG_FILTER 
    seqDebug("regexString=%s", (const char*)regexString);
    seqDebug("breakPoint=%d mid()=%s",
	   breakPoint, (const char*)levelString);
#endif

    // see if the level string is a range
    breakPoint = levelString.indexOf('-');

    bool ok;
    int level;
    if (breakPoint == -1)
    {
      // no, level string is just a single level
      level = levelString.toInt(&ok);

      // only use the number if it's ok
      if (ok)
        minLevel = level;

#ifdef DEBUG_FILTER
      seqDebug("filter min level decode levelStr='%s' level=%d ok=%1d minLevel=%d",
	     (const char*)levelString, level, ok, minLevel);
#endif
    }
    else
    {
      // it's a range.  The left most stuff before the hyphen is the minimum
      // level
      level = levelString.left(breakPoint).toInt(&ok);

      // only use the number if it's ok
      if (ok)
        minLevel = level;

#ifdef DEBUG_FILTER
      seqDebug("filter min level decode levelStr='%s' level=%d ok=%1d minLevel=%d",
	     (const char*)levelString.left(breakPoint), level, ok, minLevel);
#endif

      // the rest of the string after the hyphen is the max
      levelString = levelString.mid(breakPoint + 1);

      // if a hyphen was specified, but no max value after it, it means
      // all values above min
      if (levelString.isEmpty())
        maxLevel = INT_MAX;
      else
      {
        // get the max level
        level = levelString.toInt(&ok);

        // only use the number if it's ok
        if (ok)
          maxLevel = level;

#ifdef DEBUG_FILTER
        seqDebug("filter max level decode levelStr='%s' level=%d ok=%1d maxLevel=%d",
	      (const char*)levelString, level, ok, m_maxLevel);
#endif
      }
    }
    
    // if no max level specified, or some dope set it below min, make it 
    // equal the min
    if(maxLevel < minLevel)
      maxLevel = minLevel;
  }

  init(regexString, caseSensitive, minLevel, maxLevel);
}

void FilterItem::init(const QString& regexString, bool caseSensitive, 
        uint8_t minLevel, uint8_t maxLevel)
{
  m_minLevel = minLevel;
  m_maxLevel = maxLevel;

#ifdef DEBUG_FILTER
  seqDebug("regexString=%s minLevel=%d maxLevel=%d", 
	 (const char*)regexString, minLevel, maxLevel);
#endif

  // in theory, we have minLevel and maxLevel, so there should be no level range
  // appended to the filter string.  But there are situation where that can
  // happen, so check for it and remove it if found. Otherwise, if
  // we leave it attached, the filter will never match anything since the
  // level range will be treated as part of the match string.

  QString filterString;
  QString workString = regexString;

  // find the semi-colon that seperates the regex from the level info
  int breakPoint = workString.indexOf(';');

  // if no semi-colon, then it's all a regex
  if (breakPoint == -1)
    filterString = regexString;
  else
    // regex is the left most part of the string up to breakPoint characters
    filterString = workString.left(breakPoint);

  filterString = regexString;

#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
  if (!caseSensitive)
      m_regexp.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
#else
  m_regexp.setPatternSyntax(QRegExp::RegExp);
  m_regexp.setCaseSensitivity(caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
#endif

  // For the pattern, save off the original. This is what will be saved
  // during save operations. But the actual regexp we filter with will
  // mark the # in spawn names as optional to aid in filter writing.
  m_regexpOriginalPattern = filterString;

  QString fixedFilterPattern = filterString;
  fixedFilterPattern.replace("Name:", "Name:#?", Qt::CaseInsensitive);
  m_regexp.setPattern(fixedFilterPattern);

  if (!m_regexp.isValid())
  {
    seqWarn("Filter Error: '%s' - %s",
            m_regexp.pattern().toLatin1().data(),
            m_regexp.errorString().toLatin1().data());
  }
}

FilterItem::FilterItem(const QString& filterPattern, bool caseSensitive, 
		       uint8_t minLevel, uint8_t maxLevel)
{
  init(filterPattern, caseSensitive, minLevel, maxLevel);

  if (!m_regexp.isValid())
  {
    seqWarn("Filter Error: '%s' - %s",
            m_regexp.pattern().toLatin1().data(),
            m_regexp.errorString().toLatin1().data());
  }
}

FilterItem::~FilterItem(void)
{
}

bool FilterItem::save(QString& indent, QTextStream& out)
{
  out << indent << "<oldfilter>";

  if (!m_regexp.pattern().isEmpty())
    out << "<regex>" << m_regexpOriginalPattern << "</regex>";

  if (m_minLevel || m_maxLevel)
  {
    out << "<level";
    if (m_minLevel)
      out << " min=\"" << m_minLevel << "\"";
    if (m_maxLevel)
      out << " max=\"" << m_maxLevel << "\"";
    out << "/>";
  }

  out << "</oldfilter>" << ENDL;

  return true;
}

bool FilterItem::isFiltered(const QString& filterString, uint8_t level) const
{
  // check the main filter string
  if (filterString.indexOf(m_regexp) != -1)
  {
    // is there is a level range component to this filter
    if ((m_minLevel > 0) || (m_maxLevel > 0))
    {
      if (m_maxLevel != m_minLevel)
      {
        if ((level >= m_minLevel) && (level <= m_maxLevel))
          return true; // filter matched
      }
      else
      {
        if (level == m_minLevel)
         return true;
      }
    }
    else 
      return true; // filter matched
  }

  return false;
}

//----------------------------------------------------------------------
// Filter
/* Filter Class - Sets up regex filter */
Filter::Filter(bool caseSensitive)
{
  // set new caseSensitive
  m_caseSensitive = caseSensitive;
} // end Filter()

Filter::~Filter(void)
{
  // Free the list
  m_filterItems.clear();

} // end ~Filter()

void Filter::setCaseSensitive(bool caseSensitive)
{
  m_caseSensitive = caseSensitive;
}

bool Filter::isFiltered(const QString& filterString, uint8_t level)
{
  FilterItem *re;
#ifdef DEBUG_FILTER
  //seqDebug("isFiltered(%s)", string);
#endif /* DEBUG_FILTER */

  // iterate over the filters checking for a match
  FilterListIterator it(m_filterItems);
  while(it.hasNext())
  {
    re = it.next();
    if (!re)
        break;

    if (re->isFiltered(filterString, level))
      return true;
  }

  return false;
}

//
// save
//
// parses the filter file and builds filter list
//
bool Filter::save(QString& indent, QTextStream& out)
{
  FilterItem *re;

  // increase indent
  indent += "    ";

  // construct an iterator over the filter items
  FilterListIterator it(m_filterItems);

  // iterate over the filter items, saving them as we go along.
  while(it.hasNext())
  {
      re = it.next();
      if (!re)
          break;

      re->save(indent, out);
  }

  // decrease indent
  indent.remove(0, 4);

  return true;
}

//
// remFilter
//
// Remove a filter from the list
void
Filter::remFilter(const QString& filterPattern)
{
   FilterItem *re;

   // Find a match in the list and the one previous to it
   //while(re)
   FilterListIterator it(m_filterItems);
   while (it.hasNext())
   {
     re = it.next();
     if (!re)
         break;

     if (re->name() == filterPattern) // if match
     {
       // remove the filter
       delete m_filterItems.takeAt(m_filterItems.indexOf(re));

#ifdef DEBUG_FILTER
       seqDebug("Removed '%s' from List", (const char*)filterPattern);
#endif
       break;
     }
   } //end For
}


//
// addFilter
//
// Add a filter to the list
//
bool 
Filter::addFilter(const QString& filterPattern)
{
  FilterItem* re;

  // no duplicates allowed
  if (findFilter(filterPattern))
    return false;

  re = new FilterItem(filterPattern, m_caseSensitive);

  // append it to the end of the list
  m_filterItems.append(re);

#ifdef DEBUG_FILTER
  seqDebug("Added Filter '%s'", (const char*)filterPattern);
#endif

 return re->valid(); 
} // end addFilter

bool 
Filter::addFilter(const QString& filterPattern, uint8_t minLevel, uint8_t maxLevel)
{
  FilterItem* re;

  // no duplicates allowed
  if (findFilter(filterPattern))
    return false;

  re = new FilterItem(filterPattern, m_caseSensitive, minLevel, maxLevel);

  // append it to the end of the list
  m_filterItems.append(re);

#ifdef DEBUG_FILTER
  seqDebug("Added Filter '%s' (%d, %d)",
	   (const char*)filterPattern, minLevel, maxLevel);
#endif

 return re->valid(); 
} // end addFilter

//
// findFilter
//
// Find a filter in the list
//
FilterItem *
Filter::findFilter(const QString& filterPattern)
{
  FilterItem* re;

  FilterListIterator it(m_filterItems);
  while(it.hasNext())
  {
    re = it.next();
    if (!re)
        break;

    if (re->name() ==  filterPattern)
      return re;
  }

  return NULL;
}

void
Filter::listFilters(void)
{
  FilterItem *re;

#ifdef DEBUG_FILTER
//  seqDebug("Filter::listFilters");
#endif

  FilterListIterator it(m_filterItems);
  while(it.hasNext())
  {
    re = it.next();
    if (!re)
        break;

    if (re->minLevel() || re->maxLevel())
      seqInfo("\t'%s' (%d, %d)",
              re->name().toUtf8().data(), re->minLevel(), re->maxLevel());
    else
      seqInfo("\t'%s'", re->name().toUtf8().data());
  }
}

QString Filter::getFilterString(int index) const
{
    if (index >= m_filterItems.size())
        return QString();

    FilterItem* item = m_filterItems[index];
    QString pattern = item->filterPattern();

    return pattern;
}

QString Filter::getOrigFilterString(int index) const
{
    if (index >= m_filterItems.size())
        return QString();

    FilterItem* item = m_filterItems[index];
    QString pattern = item->origFilterPattern();

    return pattern;
}

int Filter::getMinLevel(int index) const
{
    if (index >= m_filterItems.size())
        return -1;

    FilterItem* item = m_filterItems[index];

    return item->minLevel();
}

int Filter::getMaxLevel(int index) const
{
    if (index >= m_filterItems.size())
        return -1;

    FilterItem* item = m_filterItems[index];

    return item->maxLevel();
};


//----------------------------------------------------------------------
//  Filters
Filters::Filters(const FilterTypes& types)
  : m_types(types)
{
}

Filters::~Filters()
{
}

bool Filters::clear(void)
{
  Filter* filter;
  FilterMap::iterator it;

  // iterate over the filters
  for (it = m_filters.begin(); it != m_filters.end(); it++)
  {
    // get the Filter object
    filter = it->second;

    // delete the filter
    delete filter;
  }

  // empty the container
  m_filters.clear();

  return true;
}

bool Filters::clearType(uint8_t type)
{
  // create the mask
  uint32_t mask = (1 << type);

  // find the filter object
  FilterMap::iterator it = m_filters.find(mask);

  // if filter type not found, just return success
  if (it == m_filters.end())
    return true;

  // get the Filter object
  Filter* filter = it->second;
  
  // erase the filter from the map
  m_filters.erase(it);

  // delete the filter
  delete filter;

  return true;
}

bool Filters::load(const QString& filename)
{
  // clear existing 
  clear();

  // load filters
  m_file = filename;

  if (filename.isEmpty())
      return false;

  // create XML content handler
  LoadXmlContentHandler handler(*this, m_types);

  // create a file object on the file
  QFile xmlFile(filename);
  xmlFile.open(QIODevice::ReadOnly);
  if (!xmlFile.isOpen())
      return false;

  QXmlStreamReader reader(&xmlFile);

  bool status = true;
  while(!reader.atEnd() && status)
  {
      switch(reader.readNext())
      {
          case QXmlStreamReader::NoToken:
              break;
          case QXmlStreamReader::Invalid:
              status = false;
              break;
          case QXmlStreamReader::StartDocument:
              if (!handler.startDocument())
                  status = false;
              break;
          case QXmlStreamReader::EndDocument:
              if (!handler.endDocument())
                  status = false;
              break;
          case QXmlStreamReader::StartElement:
              if (!handler.startElement(QString(), QString(), reader.name().toString(), reader.attributes()))
                  status = false;
              break;
          case QXmlStreamReader::EndElement:
              if (!handler.endElement(QString(), QString(), reader.name().toString()))
                  status = false;
              break;
          case QXmlStreamReader::Characters:
              if (!handler.characters(reader.text().toString()))
                  status = false;
              break;
          case QXmlStreamReader::Comment:
          case QXmlStreamReader::DTD:
          case QXmlStreamReader::EntityReference:
          case QXmlStreamReader::ProcessingInstruction:
              break;
      }
  }

  xmlFile.close();

  return status;
}

bool Filters::save(const QString& filename) const
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
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << ENDL
      << "<!DOCTYPE seqfilters SYSTEM \"seqfilters.dtd\">" << ENDL
      << "<seqfilters>" << ENDL;

  // set initial indent
  QString indent = "    ";

  FilterMap::const_iterator it;

  // iterate over the filters, saving them to the stream
  for (it = m_filters.begin(); it != m_filters.end(); it++)
  {
    // section start tag
    out << indent << "<section name=\"" << m_types.name(it->first)
	<< "\">" << ENDL;

    // persist the filter
    it->second->save(indent, out);

    // section end tag
    out << indent << "</section>" << ENDL;
  }

  // output closing entity
  out << "</seqfilters>" << ENDL;

  return true;
}

bool Filters::save(void) const
{
  return save(m_file);
}

void Filters::list(void) const
{
  FilterMap::const_iterator it;

  seqInfo("Filters from file '%s':",
          m_file.toLatin1().data());
  // iterate over the filters
  for (it = m_filters.begin(); it != m_filters.end(); it++)
  {
    // print the header
    seqInfo("Filter Type '%s':",
            m_types.name(it->first).toLatin1().data());

    // list off the actual filters
    it->second->listFilters();
  }
}

void Filters::setCaseSensitive(bool caseSensitive)
{
  FilterMap::iterator it;

  // iterate over the filters
  for (it = m_filters.begin(); it != m_filters.end(); it++)
  {
    // set the case sensitivity of each one
    it->second->setCaseSensitive(caseSensitive);
  }
}

uint32_t Filters::filterMask(const QString& filterString, uint8_t level) const
{
  uint32_t mask = 0;
  FilterMap::const_iterator it;

  // iterate over the filters
  for (it = m_filters.begin(); it != m_filters.end(); it++)
  {
    // checking each one to see if it matches
    if (it->second->isFiltered(filterString, level))
      mask |= it->first; // if so, include the filters mask in the return mask
  }

  return mask;
}

bool Filters::addFilter(uint8_t type, const QString& filterPattern, 
			uint8_t minLevel, uint8_t maxLevel)
{
  uint32_t mask = (1 << type);

  // only add filters for valid types
  if (!m_types.validMask(mask))
    return false;

  // find the filters
  FilterMap::iterator it = m_filters.find(mask);

  Filter* filter;

  // if the filter doesn't exist, create it
  if (it == m_filters.end())
  {
    // create a new filter
    filter = new Filter(m_caseSensitive);

    // add it to the map
    m_filters[mask] = filter;
  }
  else // use the existing filter
    filter = it->second;

  if (filter)
    return filter->addFilter(filterPattern, minLevel, maxLevel);

  return false;
}

void Filters::remFilter(uint8_t type, const QString& filterPattern)
{
  uint32_t mask = (1 << type);

  // only add filters for valid types
  if (!m_types.validMask(mask))
    return;

  // find the filters
  FilterMap::iterator it = m_filters.find(mask);

  Filter* filter;

  // if filter doesn't exist, then nothing to remove
  if (it == m_filters.end())
    return;

  filter = it->second;

  if (filter)
    filter->remFilter(filterPattern);
}

int Filters::numFilters(uint8_t type) const
{
    uint32_t mask = (1 << type);
    FilterMap::const_iterator it = m_filters.find(mask);

    if (it == m_filters.end()) return 0;

    return it->second->numFilters();
}

QString Filters::getFilterString(uint8_t type, int index) const
{
    uint32_t mask = (1 << type);
    FilterMap::const_iterator it = m_filters.find(mask);

    if (it == m_filters.end()) return QString();

    return it->second->getFilterString(index);
}

QString Filters::getOrigFilterString(uint8_t type, int index) const
{
    uint32_t mask = (1 << type);
    FilterMap::const_iterator it = m_filters.find(mask);

    if (it == m_filters.end()) return QString();

    return it->second->getOrigFilterString(index);
}

int Filters::getMinLevel(uint8_t type, int index) const
{
    uint32_t mask = (1 << type);
    FilterMap::const_iterator it = m_filters.find(mask);

    if (it == m_filters.end()) return -1;

    return it->second->getMinLevel(index);
}

int Filters::getMaxLevel(uint8_t type, int index) const
{
    uint32_t mask = (1 << type);
    FilterMap::const_iterator it = m_filters.find(mask);

    if (it == m_filters.end()) return -1;

    return it->second->getMaxLevel(index);
}




///////////////////////////////////
//  FilterTypes
FilterTypes::FilterTypes()
  : m_allocated(0), 
    m_maxType(0)
{
}

FilterTypes::~FilterTypes()
{
}

bool FilterTypes::registerType(const QString& name, uint8_t& type, uint32_t& mask)
{
  uint32_t tmpMask;
  for (uint8_t i = 0; i <= (sizeof(tmpMask) * 8); i++)
  {
    tmpMask = (1 << i);
    if (!(m_allocated & tmpMask))
    {
      m_allocated |= tmpMask;
      m_filters[tmpMask] = name;
      type = i;
      mask = tmpMask;
      if (type > m_maxType)
	m_maxType = type;

      return true;
    }
  }

  return false;
}

void FilterTypes::unregisterType(uint8_t type)
{
  uint32_t tmpMask = (1 << type);

  if (m_allocated & tmpMask)
  {
    m_allocated &= ~tmpMask;
    m_filters.erase(m_filters.find(tmpMask));

    if (type == m_maxType)
      m_maxType--;
  }
}

uint32_t FilterTypes::mask(const QString& query) const
{
  uint8_t i;
  for (i = 0; i <= maxType(); i++)
  {
    if (name(i) == query)
      return mask(i);
  }

  return 0;
}

uint8_t FilterTypes::type(const QString& query) const
{
  uint8_t i;
  for (i = 0; i <= maxType(); i++)
  {
    if (name(i) == query)
      return i;
  }

  return 255;
}

QString FilterTypes::names(uint32_t mask) const
{
  // start with an empty string
  QString text("");

  FilterTypeMap::const_iterator it;

  // iterate over the filters, adding matches as we go
  for (it = m_filters.begin(); it != m_filters.end(); ++it)
  {
    // as masks are found, add their strings to the text
    if (mask & it->first)
      text += it->second + ":";
  }

  return text;
}

//----------------------------------------------------------------------
//  LoadXmlContentHandler
LoadXmlContentHandler::LoadXmlContentHandler(Filters& filters, 
						      const FilterTypes& types)
  : m_filters(filters),
    m_types(types)
{
}

LoadXmlContentHandler::~LoadXmlContentHandler()
{
}

bool LoadXmlContentHandler::startDocument()
{
  m_currentType = 255;
  m_currentFilterPattern = "";
  m_currentMinLevel = 0;
  m_currentMaxLevel = 0;
  m_inRegex = false;
  return true;
}

bool LoadXmlContentHandler::startElement(const QString&, 
						  const QString&, 
						  const QString& name, 
						  const QXmlStreamAttributes& attr)
{
  if (name == "oldfilter")
  {
    // clear information about the current filter
    m_currentFilterPattern = "";
    m_currentMinLevel = 0;
    m_currentMaxLevel = 0;

    return true;
  }

  if (name == "regex")
  {
    m_inRegex =true;
    return true;
  }

  if (name == "level")
  {
    // if min attribute was found, use it
    if (!attr.value("min").isEmpty())
      m_currentMinLevel = uint8_t(attr.value("min").toString().toUShort());

    // if max attribute was found, use it
    if (!attr.value("max").isEmpty())
      m_currentMaxLevel = uint8_t(attr.value("max").toString().toUShort());

    // done
    return true;
  }

  if (name == "section")
  {
    // section is only valid if a name is specified
    if (attr.value("name").isEmpty())
      return false;

    // get the current type for the name
    m_currentType = m_types.type(attr.value("name").toString());

    return true;
  }

  return true;
}

bool LoadXmlContentHandler::characters(const QString& ch)
{
  if (m_inRegex)
    m_currentFilterPattern = ch;

  return true;
}

bool LoadXmlContentHandler::endElement(const QString&, 
						const QString&, 
						const QString& name)
{
  if (name == "regex")
    m_inRegex = false;

  if (name == "oldfilter")
  {
    if (m_currentType <= m_types.maxType())
    {
      m_filters.addFilter(m_currentType, m_currentFilterPattern,
			  m_currentMinLevel, m_currentMaxLevel);
    }

    return true;
  }

  if (name == "section")
    m_currentType = 255;

  return true;
}

bool LoadXmlContentHandler::endDocument()
{
  return true;
}
