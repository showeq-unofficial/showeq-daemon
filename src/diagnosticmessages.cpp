/*
 *  diagnosticmessages.cpp
 *  Copyright 2003-2004 Zaphod (dohpaz@users.sourceforge.net)
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

#include "diagnosticmessages.h"
#include "message.h"
#include "messages.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include <QLoggingCategory>
#include <QString>

//----------------------------------------------------------------------
// constants
static const int SEQ_BUFFER_LENGTH = 8196;       // internal buffer length

//----------------------------------------------------------------------
// internal utility function
static int seqMessage(MessageType type, const char* format, va_list ap)
{
  char buff[SEQ_BUFFER_LENGTH];
  int ret = vsnprintf(buff, sizeof(buff), format, ap);

  // Always route through Qt's logging system so the message handler
  // installed in main.cpp gives the line a timestamp + level tag. Pre-
  // 2026-04-26 the Messages-fallback path printed to bare stderr, which
  // produced timestamp-less lines mid-startup before the Messages
  // singleton was constructed.
  switch (type) {
    case MT_Debug:   qDebug("%s",   buff); break;
    case MT_Warning: qWarning("%s", buff); break;
    default:         qInfo("%s",    buff); break;
  }

  // Also feed the in-memory Messages list when it exists. Today the
  // daemon doesn't expose this list to clients (Phase 3 chat went via
  // MessageShell::chatMessage, not via Messages::newMessage), but the
  // wiring stays in place for future use.
  if (Messages* messages = Messages::messages()) {
    messages->addMessage(type, buff);
  }

  return ret;
}

//----------------------------------------------------------------------
// implementations
int seqDebug(const char* format, ...)
{
  va_list ap;
  int ret;
  va_start(ap, format);
  ret = seqMessage(MT_Debug, format, ap);
  va_end(ap);
  return ret;
}

int seqInfo(const char* format, ...)
{
  va_list ap;
  va_start(ap, format);
  int ret = seqMessage(MT_Info, format, ap);
  va_end(ap);
  return ret;
}

int seqWarn(const char* format, ...)
{
  va_list ap;
  va_start(ap, format);
  int ret = seqMessage(MT_Warning, format, ap);
  va_end(ap);
  return ret;
}

void seqFatal(const char* format, ...)
{
  va_list ap;
  va_start(ap, format);
  seqMessage(MT_Warning, format, ap);
  va_end(ap);
  exit (-1);
}


