/*
 *  spawnlog.cpp
 *  Copyright 2001-2007, 2018-2019 by the respective ShowEQ Developers
 *  Portions Copyright 2001-2003,2007 Zaphod (dohpaz@users.sourceforge.net).
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

#include "spawn.h"
#include "datetimemgr.h"
#include "everquest.h"
#include "spawnlog.h"

SpawnLog::SpawnLog(DateTimeMgr* dateTimeMgr, const QString& fname)
  : SEQLogger(fname, NULL, "SpawnLog"),
    m_dateTimeMgr(dateTimeMgr)
{
    version = 4;
    zoneShortName = "unknown";
}

SpawnLog::SpawnLog(DateTimeMgr* dateTimeMgr, FILE *fp)
  : SEQLogger(fp, NULL, "SpawnLog"),
    m_dateTimeMgr(dateTimeMgr)
{
    version = 4;
    zoneShortName = "unknown";
}

inline void 
SpawnLog::logSpawnInfo(const char *type, const char *name, int id, int level,
                          int x, int y, int z, 
                          const char *killedBy, int kid, int guildid)
{
  const QDateTime& eqDate = m_dateTimeMgr->updatedDateTime();
  const QTime& time = QTime::currentTime();

  logSpawnInfo(type, name, id, level, x, y, z, 
	       eqDate, time, 
	       killedBy, kid, guildid);
}

void
SpawnLog::logSpawnInfo(const char *type, const char *name, int id, int level,
                          int x, int y, int z, 
			  const QDateTime& eqDate, const QTime& time,
                          const char *killedBy, int kid, int guildid)
{
  if (!open())
      return;

  const QDate& eqDateDate = eqDate.date();
  const QTime& eqDateTime = eqDate.time();

  outputf("%s:%s(%d):%d:%d,%d,%d:%02d.%02d.%02d:%d:%s:%02d.%02d.%02d.%02d.%04d:%s(%d):%d\n",
          type,
          name,
          id,
          level,
          x,
          y,
          z,
          time.hour(), time.minute(), time.second(),
          version,
          zoneShortName.toLatin1().data(),
          eqDateTime.hour(),
          eqDateTime.minute(),
          eqDateDate.month(),
          eqDateDate.day(),
          eqDateDate.year(),
          killedBy,
          kid,
          guildid);

  flush();
}

void 
SpawnLog::logZoneSpawns(const uint8_t* data, size_t len)
{
  const spawnStruct* zspawns = (const spawnStruct*)data;
  int spawndatasize = len / sizeof(spawnStruct);

  const QDateTime& eqDate = m_dateTimeMgr->updatedDateTime();
  const QTime& time = QTime::currentTime();

  for (int i = 0; i < spawndatasize; i++)
  {
    const spawnStruct& spawn = zspawns[i];
    logSpawnInfo("z",spawn.name,spawn.spawnId,spawn.level,
                 (spawn.x >> 3), (spawn.y >> 3), (spawn.z >> 3), 
		 eqDate, time, "", 0, spawn.guildID);
  }
}

void
SpawnLog::logNewSpawn(const uint8_t* data)
{
  const spawnStruct& spawn = *(const spawnStruct*)data;
  logSpawnInfo("+",spawn.name,spawn.spawnId,spawn.level,
	       (spawn.x >> 3), (spawn.y >> 3), (spawn.z >> 3), 
	       "", 0, spawn.guildID);
}

void
SpawnLog::logNewSpawn(const Item *item)
{
  if (item->type() != tSpawn)
    return;

  const Spawn* spawn = (const Spawn*)item;

  logSpawnInfo("+", spawn->name().toLatin1().data() , spawn->id(), spawn->level(),
          spawn->x(), spawn->y(), spawn->z(), "",0, spawn->guildID());
}

void
SpawnLog::logKilledSpawn(const Item *item, const Item* kitem, uint16_t kid)
{
  if (item == NULL)
    return;

  const Spawn* spawn = (const Spawn*)item;
  const Spawn* killer = (const Spawn*)kitem;

  logSpawnInfo("x", spawn->name().toLatin1().data(), spawn->id(), spawn->level(),
          spawn->x(), spawn->y(), spawn->z(),
          killer ? killer->name().toLatin1().data() : "unknown",
          kid, spawn->guildID());
}

void
SpawnLog::logDeleteSpawn(const Item *item)
{
  if (item->type() != tSpawn)
    return;

  const Spawn* spawn = (const Spawn*)item;

  logSpawnInfo("-", spawn->name().toLatin1().data(), spawn->id(), spawn->level(),
          spawn->x(), spawn->y(), spawn->z(), "",0, spawn->guildID());
}

void
SpawnLog::logNewZone(const QString& zonename)
{
  if (!open())
      return;

  outputf("----------\nNEW ZONE: %s\n----------\n", zonename.toLatin1().data());
  outputf(" :name(spawnID):Level:Xpos:Ypos:Zpos:H.m.s:Ver:Zone:eqHour.eqMinute.eqMonth.eqDay.eqYear:killedBy(spawnID)\n");
  flush();
  zoneShortName = zonename;
}

#ifndef QMAKEBUILD
#include "spawnlog.moc"
#endif

