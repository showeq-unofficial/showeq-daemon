/*
 *  filteredspawnlog.cpp
 *  Copyright 2001-2004, 2007, 2019 by the respective ShowEQ Developers
 *  Portions Copyright 2001-2007 Zaphod (dohpaz@users.sourceforge.net).
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

#include "filteredspawnlog.h"
#include "filtermgr.h"
#include "spawn.h"
#include "datetimemgr.h"

FilteredSpawnLog::FilteredSpawnLog(DateTimeMgr* dateTimeMgr, 
				   FilterMgr* filterMgr,
				   const QString& filename)
  : SEQLogger(filename, NULL, "FilteredSpawnLog"),
    m_dateTimeMgr(dateTimeMgr),
    m_filterMgr(filterMgr),
    m_logFilters(0)
{
}

FilteredSpawnLog::~FilteredSpawnLog()
{
}

void FilteredSpawnLog::setFilters(uint32_t flags)
{
  m_logFilters = flags;
}

void FilteredSpawnLog::addItem(const Item* item)
{
  uint32_t filterFlags = item->filterFlags();

  if (!(filterFlags & m_logFilters))
    return;

  if (filterFlags & (FILTER_FLAG_ALERT | FILTER_FLAG_DANGER | 
		     FILTER_FLAG_CAUTION | FILTER_FLAG_HUNT | 
		     FILTER_FLAG_LOCATE))
    logSpawn(item, "spawned", filterFlags);
}

void FilteredSpawnLog::delItem(const Item* item)
{
  uint32_t filterFlags = item->filterFlags();

  if (!(filterFlags & m_logFilters))
    return;

  if (filterFlags & FILTER_FLAG_ALERT)
    logSpawn(item, "despawned", filterFlags);
}

void FilteredSpawnLog::killSpawn(const Item* item)
{
  uint32_t filterFlags = item->filterFlags();

  if (!(filterFlags & m_logFilters))
    return;

  if (filterFlags & FILTER_FLAG_ALERT)
    logSpawn(item, "killed", filterFlags);
}

void FilteredSpawnLog::logSpawn(const Item* item, const char* action, 
				uint32_t flag)
{
  // make sure the log file is open
  if (!open())
    return;

  // get the current EQ Date&Time
  const QDateTime& eqDate = m_dateTimeMgr->updatedDateTime();

  // log the information
  outputf("%s %s %s LOC %dy, %dx, %dz at %s (%s)\n", 
          m_filterMgr->filterString(flag).toLatin1().data(),
          action,
          item->name().toLatin1().data(),
          item->y(), item->x(), item->z(),
          eqDate.isValid() ? eqDate.toString().toLatin1().data() : "",
          item->spawnTimeStr().toLatin1().data());

  flush();
}

#ifndef QMAKEBUILD
#include "filteredspawnlog.moc"
#endif

