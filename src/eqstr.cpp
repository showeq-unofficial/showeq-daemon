/*
 *  eqstr.cpp
 *  Copyright 2002-2003 Zaphod (dohpaz@users.sourceforge.net)
 *  Copyright 2003-2004, 2016, 2019 by the respective ShowEQ Developers
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


#include "eqstr.h"
#include "diagnosticmessages.h"
#include "packetcommon.h"

#include <cstdio>

#include <QFile>
#include <QStringList>
#include <QVector>
#include <QString>

#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
#include <QRegularExpression>
#else
#include <QRegExp>
#endif

EQStr::EQStr()
  : m_messageStrings(),
    m_loaded(false)
{
}

EQStr::~EQStr()
{
  m_messageStrings.clear();
}

bool EQStr::load(const QString& fileName)
{
  // clear out any existing contents
  m_messageStrings.clear();

  // create a QFile on the file
  QFile formatFile(fileName);

  // open the file read only
  if (!formatFile.open(QIODevice::ReadOnly))
  {
    seqWarn("EQStr: Failed to open '%s'", fileName.toLatin1().data());
    return false;
  }

  // allocate a QByteArray large enough to hold the entire file
  QByteArray textData(formatFile.size() + 1, '\0');

  // read in the entire file
  formatFile.read(textData.data(), textData.size());
  
  // construct a regex to deal with either style line termination
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
  QRegularExpression lineTerm("[\r\n]{1,2}");
#else
  QRegExp lineTerm("[\r\n]{1,2}");
#endif

  // split the data into lines at the line termination
#if (QT_VERSION >= QT_VERSION_CHECK(5,14,0))
  QStringList lines = QString::fromUtf8(textData).split(lineTerm, Qt::SkipEmptyParts);
#else
  QStringList lines = QString::fromUtf8(textData).split(lineTerm, QString::SkipEmptyParts);
#endif

  // start iterating over the lines
  QStringList::Iterator it = lines.begin();
  
  // first is the magic id string
  QString magicString = (*it++);
  int spc;
  uint32_t formatId;
  QString formatString;
  uint32_t maxFormatId = 0;
  
  // next skip over the count, etc...
  it++;
  
  // now iterate over the format lines
  for (; it != lines.end(); ++it)
  {
    // find the beginning space
    spc = (*it).indexOf(' ');

    // convert the beginnign of the string to a ULong
    formatId = (*it).left(spc).toULong();
    
    if (formatId > maxFormatId) 
      maxFormatId = formatId;
    
    // insert the format string into the dictionary.
    m_messageStrings.insert(formatId, QString((*it).mid(spc+1)));    
  }

  // note that strings are loaded
  m_loaded = true;

  seqInfo("Loaded %d message strings from '%s' maxFormat=%d",
          m_messageStrings.count(), fileName.toLatin1().data(),
          maxFormatId);

  return true;
}

QString EQStr::find(uint32_t formatid) const
{
  // attempt to find the message string
  QString res = m_messageStrings.value(formatid, QString());

  return res;
}

QString EQStr::message(uint32_t formatid) const
{
  // attempt to find the message string
  QString res = m_messageStrings.value(formatid, QString());

  // if the message string was found, return it
  if (!res.isEmpty())
    return res;

  // otherwise return a fabricated string
  return QString("Unknown: ") + QString::number(formatid, 16);
}

QString EQStr::formatMessage(uint32_t formatid, 
			     const char* arguments, size_t argsLen) const
{
  QString formatStringRes = m_messageStrings.value(formatid, QString());

  QString tempStr;

    if (formatStringRes.isEmpty())
    {
	uint32_t arg_len;
	unsigned char *cp;
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
	tempStr = QString::asprintf( "Unknown: %04x:", formatid);
#else
	tempStr.sprintf( "Unknown: %04x:", formatid);
#endif
	cp = (unsigned char *) arguments;
	while (cp < ((unsigned char *) &arguments[argsLen] - sizeof(uint32_t)*sizeof(unsigned char))) {
	    arg_len = (cp[0] << 0) | (cp[1] << 8) | (cp[2] << 16) | (cp[3] << 24);
	    cp += 4;
	    if (arg_len == 0 || arg_len > argsLen)
		break;
	    tempStr += " ";
	    tempStr += QString::fromUtf8((const char *) cp, arg_len);
	    cp += arg_len;
	}
	return tempStr;
    }
    else
    {
	QVector<QString> argList;
	argList.reserve(5); // reserve space for 5 elements to handle most common sizes

	//Adjusted to handle prepended string length 05/28/2019
	size_t totalArgsLen = 0;
	const char* curArg;
        uint32_t curSize = 0;
	while (totalArgsLen < argsLen)
	{
	    curArg = arguments + totalArgsLen;
            curSize = eqtohuint32((const uint8_t*) curArg);
            curArg += 4;

            if (curSize > 0) {
	        // insert argument into the argument list
	        argList.push_back(QString::fromUtf8(curArg, curSize));
            }

	    totalArgsLen += curSize + 4;
	}

	bool ok;
	int curPos;
	int substArg;
	int substArgValue;
	QString substFormatStringRes;
	QString substFormatString;

	////////////////////////////
	// replace template (%T) arguments in formatted string
	QString formatString = formatStringRes;
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
    QRegularExpression rxt("%T(\\d{1,3})");
    // find first template substitution
    auto match = rxt.match(formatString, 0);
    curPos = match.capturedStart(1);
#else
    QRegExp rxt("%T(\\d{1,3})");
    // find first template substitution
    curPos = rxt.indexIn(formatString, 0);
#endif

    while (curPos != -1)
    {
        substFormatStringRes = QString();

#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
        substArg = match.captured(1).toInt(&ok);
#else
        substArg = rxt.cap(1).toInt(&ok);
#endif


        if (ok && (substArg <= argList.size()))
        {
            substArgValue = argList[substArg-1].toInt(&ok);

            if (ok)
                substFormatStringRes = m_messageStrings.value(substArgValue, QString());
        }

        // replace template argument with subst string
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
        if (!substFormatStringRes.isEmpty())
        {
            formatString.replace(curPos-2, match.capturedLength(1)+2, substFormatStringRes);
        }
        else
        {
            formatString.replace(curPos-2, match.capturedLength(1)+2, "");
            curPos = match.capturedEnd(1); // if no replacement string, skip over
        }

        // find next substitution
        match = rxt.match(formatString, curPos);
        curPos = match.capturedStart(1);
#else
        if (!substFormatStringRes.isEmpty())
        {
            formatString.replace(curPos, rxt.matchedLength(), substFormatStringRes);
        }
        else
        {
            formatString.replace(curPos, rxt.matchedLength(), "");
            curPos += rxt.matchedLength(); // if no replacement string, skip over
        }

        // find next substitution
        curPos = rxt.indexIn(formatString, curPos);
#endif
    }

	////////////////////////////
	// now replace substitution arguments in formatted string
	// NOTE: not using QString::arg() because not all arguments are always used
	//       and it will do screwy stuff in this situation
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
    QRegularExpression rx("%(\\d{1,3})");
    match = rx.match(formatString, 0);
    curPos = match.capturedStart(1);
#else
    QRegExp rx("%(\\d{1,3})");
    // find first template substitution
    curPos = rx.indexIn(formatString, 0);
#endif

    while (curPos != -1)
    {
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
        substArg = match.captured(1).toInt(&ok);

        // replace substitution argument with argument from list
        if (ok && (substArg <= argList.size()))
        {
            QString sub = argList[substArg-1];
            // some messages contains spell names with additional delimited fields
            if (sub.contains('^'))
            {
                sub = sub.mid(sub.lastIndexOf('^')+1);
                // they also contain an oddball apostrophe
                if (sub.startsWith("'"))
                    sub.replace(0, 1, "");
            }
            formatString.replace(curPos-1, match.capturedLength(1)+1, sub);
        }
        else
        {
            //no argument for this replacement, so replace with empty string
            formatString.replace(curPos-1, match.capturedLength(1)+1, "");
            curPos = match.capturedEnd(1); // if no such argument, skip over
        }

        // find next substitution
        match = rx.match(formatString, curPos);
        curPos = match.capturedStart(1);
#else
        substArg = rx.cap(1).toInt(&ok);

        // replace substitution argument with argument from list
        if (ok && (substArg <= argList.size()))
        {
            QString sub = argList[substArg-1];
            // some messages contains spell names with additional delimited fields
            if (sub.contains('^'))
            {
                sub = sub.mid(sub.lastIndexOf('^')+1);
                // they also contain an oddball apostrophe
                if (sub.startsWith("'"))
                    sub.replace(0, 1, "");
            }
            formatString.replace(curPos, rx.matchedLength(), sub);
        }
        else
        {
            //no argument for this replacement, so replace with empty string
            formatString.replace(curPos, rx.matchedLength(), "");
            curPos += rx.matchedLength(); // if no such argument, skip over
        }

        // find next substitution
        curPos = rx.indexIn(formatString, curPos);
#endif
    }

    return formatString;
    }

}
