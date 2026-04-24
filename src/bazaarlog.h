/*
 *  bazaarlog.h
 *  Copyright 2003-2004, 2018-2019 by the respective ShowEQ Developers
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

#ifndef _BAZAARLOG_H_
#define _BAZAARLOG_H_

#include <QObject>
#include "logger.h"

class EQPacket;
class SpawnShell;
 
//----------------------------------------------------------------------
// BazaarLog is an SEQLogger subclass that handles bazaar packets
class BazaarLog : public SEQLogger
{
  Q_OBJECT

 public:
  BazaarLog(EQPacket &packet, const QString& fname,
	    QObject* parent, SpawnShell& shell, const char* name = 0);
  virtual ~BazaarLog();

 public slots:
  void bazaarSearch(const uint8_t*, size_t, uint8_t);

 protected:
  EQPacket& m_packet;
  SpawnShell& m_shell;
};

#endif // _BAZAARLOG_H_
