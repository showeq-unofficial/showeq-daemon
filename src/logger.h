/*
 *  logger.h
 *  Copyright 2002-2003, 2019 by the respective ShowEQ Developers
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

#ifndef SEQLOGGER_H
#define SEQLOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>

#ifdef __FreeBSD__ 
// since they are incapable of following standards
#include <sys/types.h>
#else
#include <cstdint>
#endif
class SEQLogger : public QObject
{
   Q_OBJECT

 public:
   SEQLogger(const QString& fname, 
	     QObject* parent=0, const char* name="SEQLogger");
   SEQLogger(FILE *fp, QObject* parent=0, const char* name="SEQLogger");
   bool open(void);
   bool isOpen(void);
   int outputf(const char *fmt, ...);
   int output(const void *data, int length);
   void flush();
   void outputData(uint32_t len,
		   const uint8_t* data);
   
 protected:
   FILE* m_fp;
   QFile m_file;
   QTextStream m_out;
   QString m_filename;
   bool m_errOpen;
};

inline bool SEQLogger::isOpen() 
{
  return (m_fp != 0);
}

#endif
