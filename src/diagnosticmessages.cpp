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
  Messages* messages = Messages::messages();

  // if the message object exists, use it, otherwise dump to stderr
  if (messages)
    messages->addMessage(type, buff);
  else 
    fprintf(stderr, "%s\n", buff);

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


