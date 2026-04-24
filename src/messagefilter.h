/*
 *  messagefilter.h
 *  Copyright 2003 Zaphod (dohpaz@users.sourceforge.net)
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
 *
 */

#ifndef _MESSAGEFILTER_H_
#define _MESSAGEFILTER_H_

#include "message.h"

#include <cstdint>

#include <QObject>
#include <QString>
#include <QRegExp>

//----------------------------------------------------------------------
// constants
const int maxMessageFilters = 32;

//----------------------------------------------------------------------
// MessageFilter
class MessageFilter
{
 public:
  MessageFilter(const QString& name, 
		uint64_t matchTypes, const QRegExp& regexp);
  MessageFilter(uint64_t matchTypes, const QRegExp& regexp);
  MessageFilter(const MessageFilter& mf);
  ~MessageFilter();

  uint64_t types() const { return m_types; }
  void setTypes(uint64_t types) { m_types = types; }
  const QString& name() const { return m_name; }
  void setName(const QString& name) { m_name = name; }
  const QRegExp& regexp() const { return m_regexp; }
  void setRegExp(const QRegExp& regexp) { m_regexp = regexp; }
  bool valid() const { return m_regexp.isValid(); }

  bool isFiltered(const MessageEntry& message) const;
  bool isFiltered(MessageType type, const QString& message) const;
  bool isFiltered(uint64_t messageTypeMask, const QString& message) const;
  
  bool operator==(const MessageFilter& filter) const;
 protected:
  QString m_name;
  uint64_t m_types;
  QRegExp m_regexp;
};

inline bool MessageFilter::isFiltered(const MessageEntry& message) const
{
  return isFiltered(message.type(), message.text());
}

inline bool MessageFilter::isFiltered(MessageType type,
				      const QString& message) const 
{
  return isFiltered(uint64_t(1) << type, message);
}

inline bool MessageFilter::isFiltered(uint64_t messageTypeMask, 
				      const QString& message) const
{
  return (((m_types & messageTypeMask) != 0) &&
	  (m_regexp.indexIn(message) != -1));
}

inline bool MessageFilter::operator==(const MessageFilter& filter) const
{
  return ((m_name == filter.name()) && 
	  (m_types == filter.types()) && 
	  (m_regexp == filter.regexp()));
}

//----------------------------------------------------------------------
// MessageFilters
class MessageFilters : public QObject
{
  Q_OBJECT
 public:
  MessageFilters(QObject* parent = 0, const char* name = 0);
  ~MessageFilters();
  uint8_t addFilter(const MessageFilter& filter);
  bool remFilter(const MessageFilter& filter);
  bool remFilter(uint8_t filter);
  bool remFilter(const QString& name);
  uint32_t mask(uint8_t filter) { return 1 << filter; }
  const MessageFilter* filter(uint8_t filter) const { return m_filters[filter]; }

  uint32_t filterMessage(MessageEntry& message);
  uint32_t filterMessage(MessageType messageType, const QString& message);
  uint32_t filterMessage(uint64_t messageTypeMask, const QString& message);

 signals:
  void removed(uint32_t mask, uint8_t filter);
  void added(uint32_t mask, uint8_t filterid, const MessageFilter& filter);

 protected:
  MessageFilter* m_filters[maxMessageFilters];
};

inline uint32_t MessageFilters::filterMessage(MessageEntry& message)
{
  return filterMessage(message.type(), message.text());
}

inline uint32_t MessageFilters::filterMessage(MessageType messageType, 
					      const QString& message)
{
  return filterMessage(uint64_t(1) << messageType, message);
}

#endif // _MESSAGEFILTER_H_

