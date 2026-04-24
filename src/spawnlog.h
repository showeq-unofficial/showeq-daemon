/*
 *  spawnlog.h
 *  Copyright 2000-2005, 2018-2019 by the respective ShowEQ Developers
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
 */

#ifndef _SPAWNLOG_H_
#define _SPAWNLOG_H_

#include <QObject>
#include "logger.h"
#include "everquest.h"

//----------------------------------------------------------------------
// forward declarations
class QDateTime;
class QTime;
class DateTimeMgr;
class Item;

//----------------------------------------------------------------------
// SpawnLog
class SpawnLog: public SEQLogger 
{
   Q_OBJECT

public:
    SpawnLog(DateTimeMgr* dateTimeMgr, const QString& filename);
    SpawnLog(DateTimeMgr* dateTimeMgr, FILE *fp);

public slots:
    void logNewZone(const QString& zone);
    void logZoneSpawns(const uint8_t* zspawns, size_t len);
    void logNewSpawn(const uint8_t* spawn);
    void logKilledSpawn(const Item* item, const Item* kitem, uint16_t kid);
    void logDeleteSpawn(const Item *spawn);
    void logNewSpawn(const Item *spawn);

protected:
    void logSpawnInfo(const char *type, const char *name, int id, int level, 
                      int x, int y, int z, 
                      const char *killer, int kid, int guildid);
    void logSpawnInfo(const char *type, const char *name, int id, int level, 
                      int x, int y, int z, 
		      const QDateTime& eqDate, const QTime& time, 
                      const char *killer, int kid, int guildid);
    int version;
    QString zoneShortName;
    DateTimeMgr* m_dateTimeMgr;
};

#endif // _SPAWNLOG_H_
