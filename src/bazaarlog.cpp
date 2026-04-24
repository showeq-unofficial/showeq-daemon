/*
 *  bazaarlog.cpp
 *  Copyright 2003-2007, 2009, 2019 by the respective ShowEQ Developers
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

#include <QDateTime>
#include <cctype>

#include "bazaarlog.h"
#include "spawnshell.h"
#include "diagnosticmessages.h"

#pragma message("Once our minimum supported Qt version is greater than 5.14, this check can be removed and ENDL replaced with Qt::endl")
#if (QT_VERSION >= QT_VERSION_CHECK(5,14,0))
#define ENDL Qt::endl
#else
#define ENDL endl
#endif

BazaarLog::BazaarLog(EQPacket& packet, const QString& fname,
		     QObject* parent, SpawnShell& shell, const char* name)
  : SEQLogger(fname, parent, name),
    m_packet(packet),
    m_shell(shell)
{
}

BazaarLog::~BazaarLog()
{
}

void BazaarLog::bazaarSearch(const uint8_t* data, size_t len, uint8_t dir)
{
  if (!open()) return;

  if (len == 0 || data == 0 || len < sizeof(bazaarSearchResponseStruct))
    {
      seqWarn("Short / empty bazaar data passed to BazaarLog::bazaarSearch");
      return;
    }

  QString tmp;
  const struct bazaarSearchResponseStruct* r = (const bazaarSearchResponseStruct*) data;
  const size_t bsize = sizeof(bazaarSearchResponseStruct);
  const size_t msize = sizeof(r[0].mark);
  for(int i = 0;i*bsize+msize < len && r[i].mark == 7;i++)
    {
      const struct bazaarSearchResponseStruct& resp = r[i];

      // First copy and remove count from item name
      char name[256] = { 0 };
      // assert(255>sizeof(resp.item_name));
      strncpy(name,resp.item_name,qMin(static_cast<unsigned long>(sizeof(resp.item_name)), 255UL));
      char *p;
      if ((p = rindex(name,'(')) != NULL && isdigit(*(p+1)))
	*p=0;
      Item *merchant = m_shell.spawns().value(resp.player_id, nullptr);
      const char *merchant_name = "unknown";
      if (merchant)
	merchant_name = merchant->name().toLatin1().data();
      QString csv;
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
      csv = QString::asprintf("1^%d^%d^%d^%s^%s",
		  int(time(NULL)),resp.price,resp.count,
		  merchant_name, name);
      m_out << csv << ENDL;
#else
      csv.sprintf("1^%d^%d^%d^%s^%s",
		  int(time(NULL)),resp.price,resp.count,
		  merchant_name, name);
      m_out << csv << ENDL;
#endif
    }

  flush();
}

#ifndef QMAKEBUILD
#include "bazaarlog.moc"
#endif


