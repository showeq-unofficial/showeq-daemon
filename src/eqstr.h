/*
 *  eqstr.h
 *  Copyright 2002-2003 Zaphod (dohpaz@users.sourceforge.net)
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
 *
 */

#ifndef _EQSTR_H_
#define _EQSTR_H_

#include <cstdint>

#include <QHash>
#include <QString>

//----------------------------------------------------------------------
// EQStr
class EQStr
{
 public:
  EQStr();
  ~EQStr();

  bool load(const QString& eqstrFile);
  QString find(uint32_t formatid) const;
  QString message(uint32_t formatid) const;
  QString formatMessage(uint32_t formatid, 
			const char* arguments, size_t argslen) const;

 protected:
   QHash<int, QString> m_messageStrings;
   bool m_loaded;
};

#endif // _EQSTR_H_


