/*
 *  guildshell.h
 *  Copyright 2004 Zaphod (dohpaz@users.sourceforge.net).
 *  Copyright 2005-2006, 2014, 2019 by the respective ShowEQ Developers
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

#ifndef _GUILDSHELL_H_
#define _GUILDSHELL_H_

#include <cstdint>
#include <ctime>

#include <QString>
#include <QObject>
#include <QHash>
#include <QTextStream>

//----------------------------------------------------------------------
// forward declarations
class QTextStream;

class NetStream;
class ZoneMgr;

struct GuildMemberUpdate;

//----------------------------------------------------------------------
// GuildMember
class GuildMember
{
 public:
  GuildMember(NetStream& netStream);
  ~GuildMember();

  void update(const GuildMemberUpdate* gmu);

  const QString& name() const { return m_name; }
  uint8_t level() const { return m_level; }
  uint8_t classVal() const { return m_class; }
  QString classString() const;
  uint32_t guildRank() const { return m_guildRank; }
  const QString& guildRankString() const;
  uint32_t bankRank() const { return m_banker; }
  const QString& bankRankString() const;
  uint32_t altRank() const { return m_alt; }
  const QString& altRankString() const;
  uint32_t memberRank() const { return m_fullmember; }
  const QString& memberRankString() const;
  time_t lastOn() const { return m_lastOn; }
  const QString& publicNote() const { return m_publicNote; }
  uint16_t zoneId() const { return m_zoneId; }
  uint16_t zoneInstance() const { return m_zoneInstance; }

 protected:
  QString m_name;
  uint8_t m_banker; // 0 = no, 1 = banker
  uint8_t m_level;
  uint8_t m_class;
  uint32_t m_guildRank; // 0 = member, 1 = officer, 2 = leader
  time_t m_lastOn;
  uint8_t m_guildTributeOn; // 0 = off, 1 = on
  uint8_t m_guildTrophyOn; // 0 = off, 1 = on
  uint8_t m_guildTributeDonated;
  time_t m_guildTributeLastDonation;
  uint32_t m_alt; // 0 = no, 1 = alt
  uint32_t m_fullmember; // 0 = prospect, 1 = fullmember
  QString m_publicNote;
  uint16_t m_zoneId;
  uint16_t m_zoneInstance;
};

//----------------------------------------------------------------------
// GuildMemberDict
typedef QHash<QString, GuildMember*> GuildMemberDict;
typedef QHashIterator<QString, GuildMember*> GuildMemberDictIterator;

//----------------------------------------------------------------------
// GuildShell
class GuildShell : public QObject
{
  Q_OBJECT
 public:
  GuildShell(ZoneMgr* zoneMgr, QObject* parent = 0, const char* name = 0);
  ~GuildShell();
  const GuildMemberDict& members() { return m_members; }
  size_t maxNameLength() { return m_maxNameLength; }

  void dumpMembers(QTextStream& out);
  
  QString zoneString(uint16_t zoneid) const;

 public slots:
  void guildMemberList(const uint8_t* data, size_t len);
  void guildMemberUpdate(const uint8_t* data, size_t len);

 signals:
  void cleared();
  void loaded();
  void added(const GuildMember* gm);
  void removed(const GuildMember* gm);
  void updated(const GuildMember* gm);

 protected:

  GuildMemberDict m_members;
  size_t m_maxNameLength;
  ZoneMgr* m_zoneMgr;
};

#endif // _GUILDSHELL_H_
