/*
 *  filter.h - regex filter module
 *  Copyright 2001-2005, 2019 by the respective ShowEQ Developers
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

//
// NOTE: Trying to keep this file ShowEQ/Everquest independent to allow it
// to be reused for other Show{} style projects.  Any existing ShowEQ/EQ
// dependencies will be migrated out.
//
#ifndef FILTER_H
#define FILTER_H

#include <cstdint>
#include <sys/types.h>

#include <QString>
#include <QList>
#include <QTextStream>

#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
#include <QRegularExpression>
#else
#include <QRegExp>
#endif

#include <map>


//--------------------------------------------------
// defines and enums

// HumanReadableName, FilterStringFieldName
#define FILTERSTRINGFIELD_TABLE  \
    X(Name, Name)          \
    X(Level, Level)        \
    X(Race, Race)          \
    X(Class, Class)        \
    X(NPC, NPC)            \
    X(X, X)                \
    X(Y, Y)                \
    X(Z, Z)                \
    X(Light, Light)        \
    X(Deity, Deity)        \
    X(RaceTeam, RTeam)     \
    X(DeityTeam, DTeam)    \
    X(Type, Type)          \
    X(LastName, LastName)  \
    X(Guild, Guild)        \
    X(SpawnTime, Spawn)    \
    X(Info, Info)          \
    X(GM, GM)

// HumanReadableName, FilterStringFieldName
#define FILTERSTRINGINFOFIELD_TABLE \
    X(Light, Light) \
    X(Head, H)      \
    X(Chest, C)     \
    X(Arms, A)      \
    X(Waist, W)     \
    X(Gloves, G)    \
    X(Legs, L)      \
    X(Feet, F)      \
    X(Primary, 1)   \
    X(Secondary, 2)


#define X(a, b) FSF_##a,
enum FilterStringField
{
    FILTERSTRINGFIELD_TABLE
    FSF_Max
};
#undef X

#define X(a, b) FSIF_##a,
enum FilterStringInfoField
{
    FILTERSTRINGINFOFIELD_TABLE
    FSIF_Max
};
#undef X

extern const QString FilterStringFieldName[FSF_Max];
extern const QString FilterStringInfoFieldName[FSIF_Max];


// special handling for min/max level, which aren't part of regex filter string
#define FSF_MINLEVEL_NAME "MinLevel"
#define FSF_MINLEVEL_LABEL "Min Level"
#define FSF_MAXLEVEL_NAME "MaxLevel"
#define FSF_MAXLEVEL_LABEL "Max Level"


//--------------------------------------------------
// forward declarations
class FilterItem;
class Filter;
class Filters;
class FilterTypes;

//--------------------------------------------------
// typedefs
typedef QList<FilterItem*> FilterList;
typedef QListIterator<FilterItem*> FilterListIterator;
typedef std::map<uint32_t, QString> FilterTypeMap;
typedef std::map<uint32_t, Filter*> FilterMap;

//--------------------------------------------------
// FilterItem
class FilterItem
{
public:
  FilterItem(const QString& filterPattern, bool caseSensitive);
  FilterItem(const QString& filterPattern, bool caseSensitive, 
	     uint8_t minLevel, uint8_t maxLevel);
  ~FilterItem(void);
   bool save(QString& indent, QTextStream& out);

  bool isFiltered(const QString& filterString, uint8_t level) const;

  QString name() const { return m_regexp.pattern(); }
  QString filterPattern() const { return m_regexp.pattern(); }
  QString origFilterPattern() const { return m_regexpOriginalPattern; }
  uint8_t minLevel() const { return m_minLevel; }
  uint8_t maxLevel() const { return m_maxLevel; }
  bool valid() { return m_regexp.isValid(); }

 protected:
  void init(const QString& filterPattern, bool caseSensitive, uint8_t minLevel,
         uint8_t maxLevel);
  
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
  QRegularExpression m_regexp;
#else
  QRegExp m_regexp;
#endif
  QString m_regexpOriginalPattern;
  uint8_t m_minLevel;
  uint8_t m_maxLevel;
};


//--------------------------------------------------
// Filter
class Filter
{
public:
   Filter(bool caseSensitive = 0);
   ~Filter();

   bool save(QString& indent, QTextStream& out);
   bool isFiltered(const QString& filterString, uint8_t level);
   bool addFilter(const QString& filterPattern);
   bool addFilter(const QString& filterPattern, uint8_t minLevel, uint8_t maxLevel);
   void remFilter(const QString& filterPattern); 
   void listFilters(void);
   void setCaseSensitive(bool caseSensitive);

   int numFilters() const { return m_filterItems.size(); }
   QString getFilterString(int index) const;
   QString getOrigFilterString(int index) const;
   int getMinLevel(int index) const;
   int getMaxLevel(int index) const;

private:
   FilterItem* findFilter(const QString& filterPattern);

   FilterList m_filterItems;
   bool m_caseSensitive;
};

//--------------------------------------------------
// Filters
class Filters
{
 public:
  Filters(const FilterTypes& types);
  ~Filters();
  
  bool clear(void);
  bool clearType(uint8_t type);
  bool load(const QString& filename);
  bool save(const QString& filename) const;
  bool save(void) const;
  void list(void) const;

  bool caseSensitive(void) const { return m_caseSensitive; }
  void setCaseSensitive(bool caseSensitive);
  uint32_t filterMask(const QString& filterString, uint8_t level) const;
  bool addFilter(uint8_t type, const QString& filterString, 
		 uint8_t minLevel = 0, uint8_t maxLevel = 0);
  void remFilter(uint8_t type, const QString& filterString);

  int numFilters(uint8_t type) const;
  QString getFilterString(uint8_t type, int index) const;
  QString getOrigFilterString(uint8_t type, int index)const;
  int getMinLevel(uint8_t type, int index) const;
  int getMaxLevel(uint8_t type, int index) const;

 protected:
  QString m_file;
  FilterMap m_filters;
  const FilterTypes& m_types;
  bool m_caseSensitive;
};

//--------------------------------------------------
// FilterTypes
class FilterTypes
{
 public:
  FilterTypes();
  ~FilterTypes();

  bool registerType(const QString& name, uint8_t& type, uint32_t& mask);
  void unregisterType(uint8_t type);
  uint8_t maxType(void) const;
  uint32_t mask(uint8_t type) const;
  uint32_t mask(const QString& name) const;
  uint8_t type(const QString& name) const;
  QString name(uint8_t type) const;
  QString name(uint32_t mask) const;
  QString names(uint32_t mask) const;
  bool validMask(uint32_t mask) const;

 protected:
  uint32_t m_allocated;
  FilterTypeMap m_filters;
  uint8_t m_maxType;
};

inline uint8_t FilterTypes::maxType(void) const
{
  return m_maxType;
}

inline uint32_t FilterTypes::mask(uint8_t type) const
{
  return (1 << type);
}

inline QString FilterTypes::name(uint8_t type) const
{
  return name(mask(type));
}

inline QString FilterTypes::name(uint32_t mask) const
{
  FilterTypeMap::const_iterator it = m_filters.find(mask);

  if (it != m_filters.end())
    return it->second;

  return QString("Unknown:") + QString::number(mask);
}

inline bool FilterTypes::validMask(uint32_t mask) const 
{
  return ((m_allocated & mask) != 0);
}

#endif // FILTER_H
