/*
 *  group.cpp
 *  Copyright 2001-2008, 2014, 2019 by the respective ShowEQ Developers
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

#include "group.h"
#include "seq-bridge-cxx/lib.h"
#include "player.h"
#include "spawn.h"
#include "spawnshell.h"
#include "everquest.h"
#include "diagnosticmessages.h"
#include "netstream.h"

#include <cstring>

#include <QSet>
#include <QTextStream>

GroupMgr::GroupMgr(SpawnShell* spawnShell,
        Player* player,
        QObject* parent, const char* name)
  : QObject(parent),
    m_spawnShell(spawnShell),
    m_player(player),
    m_memberCount(0),
    m_membersInZoneCount(0)
{
  setObjectName(name);
  for (int i=0; i<MAX_GROUP_PEERS; i++)
  {
    m_members[i] = new GroupMember();
  }

  // clear the array of members
  for (int i = 0; i < MAX_GROUP_PEERS; i++)
    m_members[i]->m_spawn = 0;
}

GroupMgr::~GroupMgr()
{
  for (int i=0; i<MAX_GROUP_PEERS; i++)
  {
    delete m_members[i];
  }
}

void GroupMgr::player(const charProfileStruct* player)
{
  // The original logic cleared the roster here on the assumption that
  // the post-PlayerProfile OP_GroupUpdate would repopulate it from a
  // full-roster blob. Modern EQ's OP_GroupUpdate (0x6890) carries no
  // peer identity, and OP_GroupFollow (0x1bcd) only fires for new
  // joins — neither re-announces existing members on zone-in. So we
  // keep names. SpawnShell will fire delItem on the old zone's spawns
  // (clearing stale Spawn pointers in m_members) and addItem on the
  // new zone's spawns (re-resolving them by name), keeping in_zone
  // counts accurate without any work here.

// 9/3/2008 - Not used. Group data is no longer sent in charProfile.  We still
//            need to reset the data as done above.
#if 0
  // initialize the array of members with information from the player profile
  for (int i = 0; i < MAX_GROUP_PEERS; i++)
  {
     m_members[i]->m_name = player->groupMembers[i];

    if (!m_members[i]->m_name.isEmpty())
      m_memberCount++;

    if (m_members[i]->m_name != player->name)
      m_members[i]->m_spawn = 0;
    else
    {
      m_members[i]->m_spawn = (const Spawn*)m_player;

      m_membersInZoneCount++;
    }

    emit added(m_members[i]->m_name, m_members[i]->m_spawn);
  }
#endif
}

void GroupMgr::groupUpdate(const uint8_t* /*data*/, size_t /*size*/)
{
  // Modern OP_GroupUpdate (opcode 0x6890, fixed 92 bytes) is a per-slot
  // status push: each packet carries the *recipient's* own name at
  // offset 0 + a slot index, but no peer identity. Member identity
  // arrives via OP_GroupFollow (addGroupMember) and departures via
  // OP_GroupDisband / OP_GroupDisband2 (removeGroupMember), so this
  // handler currently noops. A future revision can read the slot index
  // and reconcile m_members ordering if the UI ever needs it.
}

void GroupMgr::groupMemberList(const uint8_t* data, size_t size)
{
   // OP_GroupMemberList (0x312a) wire layout (cracked 2026-05-28 from
   // groupbuff-repro.vpk + group-2variants.vpk):
   //
   //   u32 group_id
   //   u32 member_count          // 1..6
   //   <slot 0 — leader, extended record: 48b solo, ~56b w/ assist,
   //    ~58b with full leader-class field; null-padded name fields and
   //    an i32 level-or-class value follow>
   //   For i in 1..member_count-1:
   //     u32 slot_idx = i
   //     char name[]; null-terminated, padded to 8 or 16 bytes
   //     i32 level
   //     u8[~36] padding
   //
   // The slot-0 (leader) record has variable per-capture width because
   // both name field and an embedded assist-name field are individually
   // null-padded — the exact padding depends on string length. Rather
   // than reverse-engineer every field, we scan forward for printable
   // ASCII strings: each non-empty run between nulls is a member name,
   // capped at member_count and de-duplicated (the leader name appears
   // twice in the slot-0 record, and the player's own name shouldn't be
   // re-added as a peer). This handles every observed packet shape
   // without needing per-size special cases.
   if (size < 8) return;
   const uint32_t memberCount =
       uint32_t(data[4]) | (uint32_t(data[5]) << 8) |
       (uint32_t(data[6]) << 16) | (uint32_t(data[7]) << 24);
   if (memberCount == 0 || memberCount > MAX_GROUP_MEMBERS) return;

   // Walk the payload first into a peer set; do not touch m_members yet
   // — we want a *diff* so subscribers see only the actual change (one
   // `added` on a mid-zone join, one `removed` on a mid-zone leave),
   // not a "remove all → re-add all" churn that would briefly empty the
   // panel on every roster mutation.
   const QString self = m_player ? m_player->name() : QString();
   QSet<QString> incoming;
   incoming.reserve(int(memberCount));
   // EQ names are capitalized and ≥3 characters; this guards against
   // stray printable bytes (e.g. 0x3c = '<' from a small u32 level field
   // adjacent to the name area) being mis-scanned as 1-char names.
   auto isNameStart = [](uint8_t b) { return b >= 'A' && b <= 'Z'; };
   auto isNameByte  = [](uint8_t b) { return (b >= 'A' && b <= 'Z') ||
                                             (b >= 'a' && b <= 'z') ||
                                             (b >= '0' && b <= '9') ||
                                              b == '_'; };

   size_t pos = 8;
   while (pos < size) {
      while (pos < size && !isNameStart(data[pos])) ++pos;
      if (pos >= size) break;
      const size_t start = pos;
      while (pos < size && isNameByte(data[pos])) ++pos;
      if (pos - start < 3) continue;  // too short to be an EQ name
      const QString name = QString::fromLatin1(
          reinterpret_cast<const char*>(data + start),
          int(pos - start));
      if (pos < size) ++pos;  // consume terminator
      if (name == self) continue;  // peer set excludes self
      incoming.insert(name);
   }

   // Remove members no longer present.
   for (int i = 0; i < MAX_GROUP_PEERS; ++i) {
      const QString& n = m_members[i]->m_name;
      if (!n.isEmpty() && !incoming.contains(n)) {
         emit removed(n, m_members[i]->m_spawn);
         if (m_members[i]->m_spawn) m_membersInZoneCount--;
         m_members[i]->m_name.clear();
         m_members[i]->m_spawn = nullptr;
         m_memberCount--;
      }
   }

   // Add members not yet tracked. Existing entries keep their slot index
   // so subscribers that key on slot don't re-order on every mutation.
   QSet<QString> existing;
   for (int i = 0; i < MAX_GROUP_PEERS; ++i) {
      if (!m_members[i]->m_name.isEmpty())
         existing.insert(m_members[i]->m_name);
   }
   for (const QString& name : incoming) {
      if (existing.contains(name)) continue;
      for (int slot = 0; slot < MAX_GROUP_PEERS; ++slot) {
         if (m_members[slot]->m_name.isEmpty()) {
            m_members[slot]->m_name = name;
            m_members[slot]->m_spawn =
                m_spawnShell->findPlayerByDisplayName(name);
            m_memberCount++;
            if (m_members[slot]->m_spawn) m_membersInZoneCount++;
            emit added(name, m_members[slot]->m_spawn);
            break;
         }
      }
   }
}

void GroupMgr::removeGroupMember(const uint8_t* data)
{
   auto out = seq::rust::decode_group_disband(
       rust::Slice<const uint8_t>{data, sizeof(groupDisbandStruct)});
   if (!out.ok) return;
   const QString memberName =
       QString::fromLatin1(out.membername.data(), out.membername.size());

   // If we're disbanding, reset counters and clear member slots
   if(memberName == m_player->name())
   {
      m_memberCount = 0;
      m_membersInZoneCount = 0;

      for(int i = 0; i < MAX_GROUP_PEERS; i++)
      {
         m_members[i]->m_name = "";
         m_members[i]->m_spawn = 0;
      }

      emit cleared();
   }
   else
   {
      for(int i = 0; i < MAX_GROUP_PEERS; i++)
      {
         // is this the member?
         if(m_members[i]->m_name == memberName)
         {
            // yes, announce its removal
            emit removed(m_members[i]->m_name, m_members[i]->m_spawn);

            // decrement member count
            m_memberCount--;

            // if the member is in zone decrement zone count
            m_members[i]->m_spawn = m_spawnShell->findPlayerByDisplayName(m_members[i]->m_name);
            if(m_members[i]->m_spawn)
               m_membersInZoneCount--;

            // clear it
            m_members[i]->m_name = "";
            m_members[i]->m_spawn = 0;
            break;
         }
      }
   }
}

void GroupMgr::addItem(const Item* item)
{
  // only care about Spawn class
  if ((item->type() != tSpawn) && (item->type() != tPlayer))
    return;

  const Spawn* spawn = (const Spawn*)item;

  // only care about players
  if (!spawn->isPlayer())
    return;

  // Compare against both raw name() and transformedName() — see
  // findPlayerByDisplayName for why the OP_GroupFollow payload form
  // isn't guaranteed.
  const QString raw = spawn->name();
  const QString display = spawn->transformedName();
  for (int i = 0; i < MAX_GROUP_PEERS; i++)
  {
    if (m_members[i]->m_name.isEmpty()) continue;
    if (m_members[i]->m_name == raw || m_members[i]->m_name == display)
    {
      m_members[i]->m_spawn = spawn;
      m_membersInZoneCount++;
      break;
    }
  }
}

void GroupMgr::delItem(const Item* item)
{
  if ((item->type() != tSpawn) && (item->type() != tPlayer))
    return;

  const Spawn* spawn = (const Spawn*)item;

  // only care about players
  if (!spawn->isPlayer())
    return;

  // Match against either form (see addItem)
  const QString raw = spawn->name();
  const QString display = spawn->transformedName();
  for (int i = 0; i < MAX_GROUP_PEERS; i++)
  {
    if (m_members[i]->m_name.isEmpty()) continue;
    if (m_members[i]->m_name == raw || m_members[i]->m_name == display)
    {
      m_members[i]->m_spawn = 0;
      m_membersInZoneCount--;
      break;
    }
  }
}

void GroupMgr::killSpawn(const Item* item)
{
  if ((item->type() != tSpawn) && (item->type() != tPlayer))
    return;

  const Spawn* spawn = (const Spawn*)item;

  // only care about players
  if (!spawn->isPlayer())
    return;

  // Match against either form (see addItem)
  const QString raw = spawn->name();
  const QString display = spawn->transformedName();
  for (int i = 0; i < MAX_GROUP_PEERS; i++)
  {
    if (m_members[i]->m_name.isEmpty()) continue;
    if (m_members[i]->m_name == raw || m_members[i]->m_name == display)
    {
      m_members[i]->m_spawn = 0;
      m_membersInZoneCount--;
      break;
    }
  }
}

void GroupMgr::clear()
{
  // Zone change: SpawnShell just freed every Spawn. Drop the now-dangling
  // spawn pointers but keep the roster (names) — group membership survives
  // zoning. Members re-attach via addItem() as their spawns re-enter zone,
  // which re-increments m_membersInZoneCount from this zeroed baseline.
  for (int i = 0; i < MAX_GROUP_PEERS; i++)
    m_members[i]->m_spawn = 0;
  m_membersInZoneCount = 0;
}

void GroupMgr::dumpInfo(QTextStream& out)
{
  // dump general group manager information
  out << "[GroupMgr]" << Qt::endl;
  out << "Members: " << m_memberCount << Qt::endl;
  out << "MembersInZone: " << m_membersInZoneCount << Qt::endl;
  out << "Player: " << m_player->name() << Qt::endl;
  out << "GroupBonus: " << groupBonus() << Qt::endl;
  out << "GroupTotalLevels: " << totalLevels() << Qt::endl;
  out << "GroupAverageLevel: ";
  if (m_membersInZoneCount)
    out << totalLevels()/m_membersInZoneCount << Qt::endl;
  else
    out << totalLevels() << Qt::endl;

  // iterate over the group members
  for (int i = 0; i < MAX_GROUP_PEERS; i++)
  {
    if (m_members[i]->m_name.isEmpty())
      continue;

    out << "Member (" << i << "): " << m_members[i]->m_name;

    if (m_members[i]->m_spawn)
      out << " level " << m_members[i]->m_spawn->level()
	  << " " << m_members[i]->m_spawn->raceString()
	  << " " << m_members[i]->m_spawn->classString();

    out << Qt::endl;
  }  
}

float GroupMgr::groupBonus()
{
  switch (groupSize())
  {
  case 2:	return 1.02;
  case 3:	return 1.06;
  case 4:	return 1.10;
  case 5:	return 1.14;
  case 6:	return 1.20;
  default:	return 1.00;
  }
}

unsigned long GroupMgr::totalLevels()
{
  // if the player isn't in a group, just return their level
  if (m_memberCount == 0) 
    return m_player->level();

  unsigned long total = 0;

  // iterate over the group members
  for (int i = 0; i < MAX_GROUP_PEERS; i++)
  {
    // add up the group member levels
    if (m_members[i]->m_spawn)
      total += m_members[i]->m_spawn->level();
  }

  // shouldn't happen, but just in-case
  if (total == 0)
    total = m_player->level();

  return total;
}

const Spawn* GroupMgr::memberByID(uint16_t id)
{
  // iterate over the members until a matching spawn id is found
  for (int i = 0; i < MAX_GROUP_PEERS; i++)
  {
    // if this member is in zone, and the spawnid matches, return it
    if (m_members[i]->m_spawn && (m_members[i]->m_spawn->id() == id))
      return m_members[i]->m_spawn;
  }

  // not found
  return 0;
}

const Spawn* GroupMgr::memberByName(const QString& name)
{
  // iterate over the members until a matching spawn name is found
  for (int i = 0; i < MAX_GROUP_PEERS; i++)
  {
    // if this member has the name, return its spawn
    if (m_members[i]->m_name == name)
      return m_members[i]->m_spawn;
  }
  
  // not found
  return 0;
}

const Spawn* GroupMgr::memberBySlot(uint16_t slot )
{
  // validate slot value
  if (slot >= MAX_GROUP_PEERS)
    return 0;

  // guard the slot pointer too (cf. memberNameBySlot) — don't deref a
  // null GroupMember before reaching m_spawn
  if (!m_members[slot])
    return 0;

  // return the spawn object associated with the group slot, if any
  return m_members[slot]->m_spawn;
}

QString GroupMgr::memberNameBySlot(uint16_t slot) const
{
  if (slot >= MAX_GROUP_PEERS) return QString();
  if (!m_members[slot]) return QString();
  return m_members[slot]->m_name;
}
