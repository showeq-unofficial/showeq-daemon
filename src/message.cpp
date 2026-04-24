/*
 *  message.cpp
 *  Copyright 2003 Zaphod (dohpaz@users.sourceforge.net)
 *  Copyright 2019 by the respective ShowEQ Developers
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

#include "message.h"

QString MessageEntry::s_messageTypeStrings[MT_Max+1] = 
{
  "Guild",
  "",
  "Group",
  "Shout",
  "Auction",
  "OOC",
  "",
  "Tell",
  "Say",
  "",
  "",
  "GMSay",
  "",
  "",
  "GMTell",
  "Raid",
  "Debug",
  "Info",
  "Warning",
  "General",
  "MOTD",
  "System",
  "Money",
  "Random",
  "Emote",
  "Time",
  "Spell",
  "Zone",
  "Inspect",
  "Player",
  "Consider",
  "Alert",
  "Danger",
  "Caution",
  "Hunt",
  "Locate",
};

