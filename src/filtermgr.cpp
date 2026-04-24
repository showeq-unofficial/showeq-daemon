/*
 *  filtermgr.cpp
 *  Copyright 2001-2007, 2019 by the respective ShowEQ Developers
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
#include "filtermgr.h"
#include "filter.h"
#include "datalocationmgr.h"
#include "diagnosticmessages.h"

#include <cerrno>

#include <QRegExp>
#include <QString>
#include <QFileInfo>

//
// ZBTEMP: predefined filters and filter mask will be migrated out
// so that ShowEQ code can register the file based filters and there mask
// at runtime ala the runtime Filter stuff
//
//----------------------------------------------------------------------
// FilterMgr

FilterMgr::FilterMgr(const DataLocationMgr* dataLocMgr,
		     const QString filterFile, bool spawnfilter_case)
  : QObject(NULL),
    m_dataLocMgr(dataLocMgr),
    m_caseSensitive(spawnfilter_case)
{
  setObjectName("filtermgr");
  // Initialize filters

  // allocate general filter types object
  m_types = new FilterTypes;

  // Initialize filter types (start with legacy filter types)
  uint8_t type;
  uint32_t mask;

  #define X(a, b, c) m_types->registerType(#a, type, mask);
  FILTER_TYPE_TABLE
  #undef X

  // create global filters object
  m_filters = new Filters(*m_types);

  // load the global filters
  loadFilters(filterFile);

  // create the zone filters object
  m_zoneFilters = new Filters(*m_types);

  // load the zone filters
  loadZone("unknown");

  // allocate runtime filter types object
  m_runtimeTypes = new FilterTypes;

  // create runtime filters object
  m_runtimeFilters = new Filters(*m_runtimeTypes);
}

FilterMgr::~FilterMgr()
{
  if (m_filters)
    delete m_filters;
  if (m_zoneFilters)
    delete m_zoneFilters;
  if (m_types)
    delete m_types;
  if (m_runtimeFilters)
    delete m_runtimeFilters;
  if (m_runtimeTypes)
    delete m_runtimeTypes;
}

void FilterMgr::setCaseSensitive(bool caseSensitive)
{
  m_caseSensitive = caseSensitive;

  m_filters->setCaseSensitive(m_caseSensitive);
  m_zoneFilters->setCaseSensitive(m_caseSensitive);
  m_runtimeFilters->setCaseSensitive(m_caseSensitive);
}

uint32_t FilterMgr::filterMask(const QString& filterString, uint8_t level) const
{
  uint32_t mask = 0;

  mask = m_filters->filterMask(filterString, level);
  mask |= m_zoneFilters->filterMask(filterString, level);

  return mask;
}

QString FilterMgr::filterString(uint32_t mask) const
{
  return m_types->names(mask);
}

QString FilterMgr::filterName(uint8_t filter) const
{
  return m_types->name(filter);
}

void FilterMgr::loadFilters(void)
{
  QFileInfo fileInfo(m_filterFile);

  fileInfo = 
    m_dataLocMgr->findExistingFile("filters", fileInfo.fileName(), false);

  m_filterFile = fileInfo.absoluteFilePath();

  seqInfo("Loading Filters from '%s'", m_filterFile.toLatin1().data());

  m_filters->load(m_filterFile);

  emit filtersChanged();
}

void FilterMgr::loadFilters(const QString& fileName)
{
  QFileInfo fileInfo = 
    m_dataLocMgr->findExistingFile("filters", fileName, false);

  m_filterFile = fileInfo.absoluteFilePath();

  seqInfo("Loading Filters from '%s'", m_filterFile.toLatin1().data());
  
  m_filters->load(m_filterFile);

  emit filtersChanged();
}


void FilterMgr::saveFilters(void)
{
  QFileInfo fileInfo(m_filterFile);
  
  fileInfo = m_dataLocMgr->findWriteFile("filters", fileInfo.fileName(), true);

  m_filterFile = fileInfo.absoluteFilePath();

  seqInfo("Saving filters to %s", m_filterFile.toLatin1().data());

  m_filters->save(m_filterFile);
}

void FilterMgr::listFilters(void)
{
  m_filters->list();
}

bool FilterMgr::addFilter(uint8_t filter, const QString& filterString)
{
  // make sure it's actually a filter
  if (filter >= SIZEOF_FILTERS)
    return false;

  // add the filter
  bool ok = m_filters->addFilter(filter, filterString);

  // signal that the filters have changed
  emit filtersChanged();

  return ok;
}

void FilterMgr::remFilter(uint8_t filter, const QString& filterString)
{
  // validate that it's a valid filter
  if (filter >= SIZEOF_FILTERS)
    return;

  // remove a filter
  m_filters->remFilter(filter, filterString);

  // notify that the filters have changed
  emit filtersChanged();
}

bool FilterMgr::addZoneFilter(uint8_t filter, const QString& filterString)
{
  // make sure it's actually a filter
  if (filter >= SIZEOF_FILTERS)
    return false;

  // add the filter
  bool ok = m_zoneFilters->addFilter(filter, filterString);

  // signal that the filters have changed
  emit filtersChanged();

  return ok;
}

void FilterMgr::remZoneFilter(uint8_t filter, const QString& filterString)
{
  // validate that it's a valid filter
  if (filter >= SIZEOF_FILTERS)
    return;

  // remove a filter
  m_zoneFilters->remFilter(filter, filterString);

  // notify that the filters have changed
  emit filtersChanged();
}

void FilterMgr::loadZone(const QString& shortZoneName)
{
  QString fileName = shortZoneName + ".xml";

  QFileInfo fileInfo = 
    m_dataLocMgr->findExistingFile("filters", fileName, false);

  m_zoneFilterFile = fileInfo.absoluteFilePath();

  seqInfo("Loading Zone Filter File: %s", m_zoneFilterFile.toLatin1().data());

  m_zoneFilters->load(m_zoneFilterFile);

  emit filtersChanged();
}

void FilterMgr::loadZoneFilters(void)
{
  QFileInfo fileInfo(m_zoneFilterFile);
  
  fileInfo = m_dataLocMgr->findExistingFile("filters", fileInfo.fileName(),
					    false);

  m_zoneFilterFile = fileInfo.absoluteFilePath();

  seqInfo("Loading Zone Filter File: %s", m_zoneFilterFile.toLatin1().data());

  m_zoneFilters->load(m_zoneFilterFile);
  
  emit filtersChanged();
}


void FilterMgr::listZoneFilters(void)
{
  m_zoneFilters->list();
}


void FilterMgr::saveZoneFilters(void)
{
  QFileInfo fileInfo(m_zoneFilterFile);
  
  fileInfo = m_dataLocMgr->findWriteFile("filters", fileInfo.fileName(), true);

  m_zoneFilterFile = fileInfo.absoluteFilePath();

  seqInfo("Saving filters to %s", m_zoneFilterFile.toLatin1().data());

  if (! m_zoneFilters->save(m_zoneFilterFile))
  {
    seqWarn("Failed saving filters.");
  }
}

bool FilterMgr::registerRuntimeFilter(const QString& name, 
				      uint8_t& type,
				      uint32_t& mask)
{
  return m_runtimeTypes->registerType(name, type, mask);
}

void FilterMgr::unregisterRuntimeFilter(uint8_t type)
{
  // first, clear any filter associated with the type
  m_runtimeFilters->clearType(type);

  // Then unregister the type
  m_runtimeTypes->unregisterType(type);
}

uint32_t FilterMgr::runtimeFilterMask(const QString& filterString,
				       uint8_t level) const
{
  return m_runtimeFilters->filterMask(filterString, level);
}

QString FilterMgr::runtimeFilterString(uint32_t filterMask) const
{
  return m_runtimeTypes->names(filterMask);
}

bool FilterMgr::runtimeFilterAddFilter(uint8_t type, const QString& filter)
{
  return m_runtimeFilters->addFilter(type, filter);
}

void FilterMgr::runtimeFilterRemFilter(uint8_t type, const QString& filter)
{
  return m_runtimeFilters->remFilter(type, filter);
}

void FilterMgr::runtimeFilterCommit(uint8_t type)
{
  // notify that the runtime filters have changed
  emit runtimeFiltersChanged(type);
}

#ifndef QMAKEBUILD
#include "filtermgr.moc"
#endif

