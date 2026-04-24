/*
 *  filtermgr.h
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

#ifndef FILTERMGR_H
#define FILTERMGR_H

#ifdef __FreeBSD__
#include <sys/types.h>
#else
#include <cstdint>
#endif

#include <map>

#include <QObject>
#include <QDialog>
#include <QString>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>

#include "everquest.h"

//----------------------------------------------------------------------
// forward declarations
class Filter;
class Filters;
class FilterTypes;
class DataLocationMgr;

//
// ZBTEMP: predefined filters and filter mask will be migrated out
// so that ShowEQ code can register the file based filters and there mask
// at runtime ala the runtime Filter stuff
//

//----------------------------------------------------------------------
// Macro defintions

//Label, uppercased label (for backwards compat), id
#define FILTER_TYPE_TABLE \
    X(Hunt, HUNT, 0)        \
    X(Caution, CAUTION, 1)  \
    X(Danger, DANGER, 2)    \
    X(Locate, LOCATE, 3)    \
    X(Alert, ALERT, 4)      \
    X(Filtered, FILTERED, 5)\
    X(Tracer, TRACER, 6)


//Filter Flags
enum FilterTypeDefs {
#define X(a, b, c) b##_FILTER = c,
    FILTER_TYPE_TABLE
    SIZEOF_FILTERS
#undef X
};

// max of 32 flags
enum FilterTypeFlags {
#define X(a, b, c) FILTER_FLAG_##b = (1ULL << c),
    FILTER_TYPE_TABLE
#undef X
};


//----------------------------------------------------------------------
// FilterMgr
class FilterMgr : public QObject
{
  Q_OBJECT

 public:
  FilterMgr(const DataLocationMgr* dataLocMgr, 
	    const QString filterFile, bool spawnfilter_case);
  ~FilterMgr();

  const QString& filterFile(void) { return m_filterFile; }
  const QString& zoneFilterFile(void) { return m_zoneFilterFile; }
  bool caseSensitive(void) { return m_caseSensitive; }
  void setCaseSensitive(bool caseSensitive);

  uint32_t filterMask(const QString& filterString, uint8_t level) const;
  QString filterString(uint32_t mask) const;
  QString filterName(uint8_t filter) const;
  bool addFilter(uint8_t filter, const QString& filterString);
  void remFilter(uint8_t filter, const QString& filterString);
  bool addZoneFilter(uint8_t filter, const QString& filterString);
  void remZoneFilter(uint8_t filter, const QString& filterString);

  bool registerRuntimeFilter(const QString& name, 
			     uint8_t& flag,
			     uint32_t& flagMask);
  void unregisterRuntimeFilter(uint8_t flag);
  uint32_t runtimeFilterMask(const QString& filterString, uint8_t level) const;
  QString runtimeFilterString(uint32_t filterMask) const;
  bool runtimeFilterAddFilter(uint8_t flag, const QString& filter);
  void runtimeFilterRemFilter(uint8_t flag, const QString& filter);
  void runtimeFilterCommit(uint8_t flag);

 public slots:
  void loadFilters(void);
  void loadFilters(const QString& filterFile);
  void saveFilters(void);
  void listFilters(void);
  void loadZone(const QString& zoneShortName);
  void loadZoneFilters(void);
  void listZoneFilters(void);
  void saveZoneFilters(void);

 signals:
  void filtersChanged();
  void runtimeFiltersChanged(uint8_t flag);


 private:
  const DataLocationMgr* m_dataLocMgr;
  FilterTypes* m_types;
  QString m_filterFile;
  Filters* m_filters;
  QString m_zoneFilterFile;
  Filters* m_zoneFilters;

  FilterTypes* m_runtimeTypes;
  Filters* m_runtimeFilters;

  bool m_caseSensitive;
};

#endif // FILTERMGR_H
