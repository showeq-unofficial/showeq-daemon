/*
 *  group.h
 *  Copyright 2001-2005, 2008, 2019 by the respective ShowEQ Developers
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

#ifndef _GROUP_H_
#define _GROUP_H_

#include <cstdint>

#include <QObject>
#include <QString>
#include <QTextStream>

#include "everquest.h"


//----------------------------------------------------------------------
// forward declarations
class Player;
class SpawnShell;
class Item;
class Spawn;

class GroupMgr: public QObject
{
  Q_OBJECT
 public:
  GroupMgr(SpawnShell* spawnShell, 
	   Player* player,  
	   QObject* parent = 0, const char* name = 0);
  virtual ~GroupMgr();
 
  const Spawn* memberByID( uint16_t id );
  const Spawn* memberByName( const QString& name );
  const Spawn* memberBySlot( uint16_t slot );

  size_t groupSize() { return m_memberCount; }
  size_t groupMemberCount() { return m_memberCount; }
  size_t groupMembersInZoneCount() { return m_membersInZoneCount; }
  float groupBonus();
  
  unsigned long totalLevels();
  
 public slots:
  void player(const charProfileStruct* player); 
  void groupUpdate(const uint8_t* data, size_t size);
  void addGroupMember(const uint8_t* data);
  void removeGroupMember(const uint8_t* data);
  void addItem(const Item* item);
  void delItem(const Item* item);
  void killSpawn(const Item* item);

  // dump debug info
  void dumpInfo(QTextStream& out);

 signals:
  void added(const QString& name, const Spawn* mem);
  void removed(const QString& name, const Spawn* mem);
  void cleared();
  
 protected:
  SpawnShell* m_spawnShell;
  Player* m_player;
  struct GroupMember
  {
    QString m_name;
    const Spawn* m_spawn;
  }* m_members[MAX_GROUP_MEMBERS];
  size_t m_memberCount;
  size_t m_membersInZoneCount;
};

#endif // _GROUP_H_
