/*
 *  messages.cpp
 *  Copyright 2002-2003, 2007 Zaphod (dohpaz@users.sourceforge.net)
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

#include "messages.h"
#include "datetimemgr.h"

//----------------------------------------------------------------------
// initialize statics
Messages* Messages::s_messages = 0;

//----------------------------------------------------------------------
// Messages
Messages::Messages(DateTimeMgr* dateTimeMgr, MessageFilters* messageFilters,
		   QObject* parent, const char* name)
  : QObject(parent),
    m_dateTimeMgr(dateTimeMgr),
    m_messageFilters(messageFilters)
{
  setObjectName(name);
  if (!s_messages)
    s_messages = this;

  connect(m_messageFilters, SIGNAL(removed(uint32_t, uint8_t)),
	  this, SLOT(removedFilter(uint32_t, uint8_t)));
  connect(m_messageFilters, SIGNAL(added(uint32_t, uint8_t, 
					 const MessageFilter&)),
	  this, SLOT(addedFilter(uint32_t, uint8_t, const MessageFilter&)));
}

Messages::~Messages()
{
}

void Messages::addMessage(MessageType type, const QString& text, 
			  uint32_t color)
{
  // filter the message
  uint32_t filterFlags = m_messageFilters->filterMessage(type, text);
  
  // create a message entry
  MessageEntry message(type, QDateTime::currentDateTime(),
		       m_dateTimeMgr->updatedDateTime(),
		       text, color, filterFlags);

  // create the message and append it to the end of the list
  m_messages.append(message);

  // signal that a new message exists
  emit newMessage(message);
}

void Messages::clear(void)
{
  // clear the messages
  m_messages.clear();

  // signal that the messages have been cleared
  emit cleared();
}

void Messages::removedFilter(uint32_t mask, uint8_t filter)
{
  // filter has been removed, remove its mask from all the messages
  MessageList::iterator it;
  for (it = m_messages.begin(); it != m_messages.end(); ++it)
    (*it).setFilterFlags((*it).filterFlags() & ~mask);
}

void Messages::addedFilter(uint32_t mask, uint8_t filterid, 
			   const MessageFilter& filter)
{
  // filter has been added, filter all messages against it
  MessageList::iterator it;
  for (it = m_messages.begin(); it != m_messages.end(); ++it)
    if (filter.isFiltered(*it))
      (*it).setFilterFlags((*it).filterFlags() | mask);
}

#ifndef QMAKEBUILD
#include "messages.moc"
#endif

