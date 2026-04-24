/*
 *  messages.h
 *  Copyright 2002-2003 Zaphod (dohpaz@users.sourceforge.net)
 *  Copyright 2009, 2019 by the respective ShowEQ Developers
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

#ifndef _MESSAGES_H_
#define _MESSAGES_H_

#include "message.h"
#include "messagefilter.h"

#include <cstdint>

#include <QObject>
#include <QString>
#include <QList>

//----------------------------------------------------------------------
// forward declarations
class DateTimeMgr;

//----------------------------------------------------------------------
// MessageList
typedef QList<MessageEntry> MessageList;

//----------------------------------------------------------------------
// Messages
class Messages : public QObject
{
  Q_OBJECT

 public:
  Messages(DateTimeMgr* dateTimeMgr, MessageFilters* messageFilters,
	   QObject* parent = 0, const char* name = 0);
  ~Messages();

  static Messages* messages() { return s_messages; }
  const MessageList messageList() const;

 public slots:
  void addMessage(MessageType type, const QString& text, 
		  uint32_t color = ME_InvalidColor);
  void clear(void);

 protected slots:
  void removedFilter(uint32_t mask, uint8_t filter);
  void addedFilter(uint32_t mask, uint8_t filterid, const MessageFilter& filter);
   
 signals:
  void newMessage(const MessageEntry& message);
  void cleared(void);

 protected:
  DateTimeMgr* m_dateTimeMgr;
  MessageFilters* m_messageFilters;
  MessageList m_messages;

  static Messages* s_messages;
};

inline const MessageList Messages::messageList() const
{
  return m_messages;
}

#endif // _MESSAGES_H_
