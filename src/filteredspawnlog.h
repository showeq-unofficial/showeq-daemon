/*
 *  filteredspawnlog.h
 *  Copyright 2000-2003, 2019 by the respective ShowEQ Developers
 *  Portions Copyright 2003 Zaphod (dohpaz@users.sourceforge.net).
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

#ifndef _FILTEREDSPAWNLOG_H_
#define _FILTEREDSPAWNLOG_H_

#include <QObject>
#include "logger.h"

//----------------------------------------------------------------------
// forward declarations
class QDateTime;
class DateTimeMgr;
class FilterMgr;
class Item;

//----------------------------------------------------------------------
// FilteredSpawnLog
class FilteredSpawnLog: public SEQLogger 
{
   Q_OBJECT

public:
   FilteredSpawnLog(DateTimeMgr* dateTimeMgr, FilterMgr* filterMgr,
		    const QString& filename);
   ~FilteredSpawnLog();
   uint32_t filters() { return m_logFilters; }

public slots:
   void setFilters(uint32_t flags);

   void addItem(const Item* item);
   void delItem(const Item* item);
   void killSpawn(const Item* item);

 protected:
   void logSpawn(const Item* item, const char* action, uint32_t flag);
   DateTimeMgr* m_dateTimeMgr;
   FilterMgr* m_filterMgr;
   uint32_t m_logFilters;
};

#endif // _FILTEREDSPAWNLOG_H_

