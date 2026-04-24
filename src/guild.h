/*
 *  guild.h
 *  Copyright 2001 Fee (fee@users.sourceforge.net). All Rights Reserved.
 *  Copyright 2002-2003, 2009, 2019 by the respective ShowEQ Developers
 *
 *  Contributed to ShowEQ by fee (fee@users.sourceforge.net)
 *  for use under the terms of the GNU General Public License,
 *  incorporated herein by reference.
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

#ifndef _GUILD_H_
#define _GUILD_H_

#include <QObject>
#include <QString>
#include <vector>
#include <map>

#include "everquest.h"

//------------------------------
// GuildMgr
class GuildMgr : public QObject
{
  Q_OBJECT

 public:

  GuildMgr(QString, QObject* parent = 0, const char* name = 0);

  ~GuildMgr();

  QString guildIdToName(uint16_t, uint16_t);

 public slots:
  void newGuildInZone(const uint8_t* data, size_t len);
  void guildsInZoneList(const uint8_t * data, size_t len);
  void readGuildList();
  void guildList2text(QString);
  void listGuildInfo();
  void writeGuildList();

 signals:
  void guildTagUpdated(uint32_t);

 private:
  std::map<uint32_t, QString> m_guildList;

  QString guildsFileName;

};

#endif // _GUILD_H_
