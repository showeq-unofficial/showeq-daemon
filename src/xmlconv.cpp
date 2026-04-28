/*
 *  xmlconv.cpp
 *  Copyright 2002-2003 Zaphod (dohpaz@users.sourceforge.net). All Rights Reserved.
 *  Copyright 2002-2005, 2019 by the respective ShowEQ Developers
 *
 *  Contributed to ShowEQ by Zaphod (dohpaz@users.sourceforge.net)
 *  for use under the terms of the GNU General Public License,
 *  incorporated herein by reference.
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

#define __STDC_LIMIT_MACROS
#include <cstdint>

#include "xmlconv.h"

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QStringList>
#include <QDomElement>

// borrowed with mods from Qt 3.2
static bool ok_in_hex( QChar c)
{
  return c.isDigit() || (c >= 'a' && c < char('a'+6))
                     || (c >= 'A' && c < char('A'+6));
}

DomConvenience::DomConvenience(QDomDocument& doc)
  : m_doc(doc)
{
}

bool DomConvenience::elementToVariant(const QDomElement& e, 
				      QVariant& v)
{
  bool ok = false;

  if (e.tagName() == "string")
  {
    if (e.hasAttribute("value"))
    {
      v = QVariant(e.attribute("value"));
      ok = true;
    }
    else
      qWarning("%s element without value!", e.tagName().toLatin1().data());
  }
  else if (e.tagName() == "int")
  {
    int base = getBase(e);

    if (e.hasAttribute("value"))
      v = QVariant(e.attribute("value").toInt(&ok, base));
    else
      qWarning("%s element without value!", e.tagName().toLatin1().data());
  }
  else if (e.tagName() == "uint")
  {
    int base = getBase(e);

    if (e.hasAttribute("value"))
      v = QVariant(e.attribute("value").toUInt(&ok, base));
    else
      qWarning("%s element without value!", e.tagName().toLatin1().data());
  }
  else if (e.tagName() == "double")
  {
    if (e.hasAttribute("value"))
      v = QVariant(e.attribute("value").toDouble(&ok));
    else
      qWarning("%s element without value!", e.tagName().toLatin1().data());
  }
  else if (e.tagName() == "bool")
  {
    if (e.hasAttribute("value"))
    {
      QString val = e.attribute("value");
      v = QVariant(getBoolFromString(val, ok));

      if (!ok)
          qWarning("%s element with bogus value '%s'!",
                  e.tagName().toLatin1().data(), val.toLatin1().data());
    }
    else
      qWarning("%s element without value!", e.tagName().toLatin1().data());
  }
  else if (e.tagName() == "color")
  {
    SeqColor color = getColor(e);

    ok = color.isValid();

    v = QVariant::fromValue<SeqColor>(color);

    if (!ok)
      qWarning("%s element without value!", e.tagName().toLatin1().data());
  }
  else if (e.tagName() == "point")
  {
    int base = getBase(e);
    bool coordOk;

    int x = 0, y = 0;
    if (e.hasAttribute("x"))
      x = e.attribute("x").toInt(&coordOk, base);

    if (e.hasAttribute("y"))
      y = e.attribute("y").toInt(&coordOk, base);
    
    v = QVariant(QPoint(x, y));
    ok = true;
  }
  else if (e.tagName() == "rect")
  {
    int base = getBase(e);
    bool coordOk;

    int x = 0, y = 0, width = 0, height = 0;
    if (e.hasAttribute("x"))
      x = e.attribute("x").toInt(&coordOk, base);

    if (e.hasAttribute("y"))
      y = e.attribute("y").toInt(&coordOk, base);

    if (e.hasAttribute("width"))
      width = e.attribute("width").toInt(&coordOk, base);

    if (e.hasAttribute("height"))
      height = e.attribute("height").toInt(&coordOk, base);
    
    v = QVariant(QRect(x, y, width, height));
    ok = true;
  }
  else if (e.tagName() == "size")
  {
    int base = getBase(e);
    bool coordOk;

    int width = 0, height = 0;
    if (e.hasAttribute("width"))
      width = e.attribute("width").toInt(&coordOk, base);

    if (e.hasAttribute("height"))
      height = e.attribute("height").toInt(&coordOk, base);
    
    v = QVariant(QSize(width, height));
    ok = true;
  }
  else if (e.tagName() == "stringlist")
  {
    QDomNodeList stringNodeList = e.elementsByTagName("string");
    QStringList stringList;
    QDomElement stringElement;
    
    for (uint i = 0; i < stringNodeList.length(); i++)
    {
      stringElement = stringNodeList.item(i).toElement();
      if (!stringElement.hasAttribute("value"))
      {
          qWarning("%s element in %s without value! Ignoring!",
                  stringElement.tagName().toLatin1().data(),
                  e.tagName().toLatin1().data());
          continue;
      }

      stringList.append(e.attribute("value"));
    }

    v = stringList;

    ok = true;
  }
  else if (e.tagName() == "uint64")
  {
    QString value = e.attribute("value");


    uint64_t tmp = value.toULongLong(&ok, 16);

    if (!ok)
        return false;

    v.setValue(tmp);

  }
  else if (e.tagName() == "list")
  {
    qWarning("Unimplemented tag: %s", e.tagName().toLatin1().data());
  }
  else if (e.tagName() == "map")
  {
    qWarning("Unimplemented tag: %s", e.tagName().toLatin1().data());
  }
  else
  {
    qWarning("Unknown tag: %s", e.tagName().toLatin1().data());
  }

  return ok;
}

bool DomConvenience::variantToElement(const QVariant& v, QDomElement& e)
{
  bool ok = true;

  clearAttributes(e);

  // SeqColor is a custom metatype, so its id isn't a compile-time
  // constant — handle it before the QMetaType switch.
  if (v.userType() == qMetaTypeId<SeqColor>())
  {
    e.setTagName("color");
    SeqColor color = v.value<SeqColor>();
    e.setAttribute("red", color.r);
    e.setAttribute("green", color.g);
    e.setAttribute("blue", color.b);
    return true;
  }

  switch (v.type())
  {
  case QMetaType::QString:
    e.setTagName("string");
    e.setAttribute("value", v.toString().toUtf8().data());
    break;
  case QMetaType::Int:
    e.setTagName("int");
    e.setAttribute("value", v.toInt());
    break;
  case QMetaType::UInt:
    e.setTagName("uint");
    e.setAttribute("value", v.toUInt());
    break;
  case QMetaType::Long:
  case QMetaType::LongLong:
  {
    e.setTagName("int64");
    QString val;
    val = QString::asprintf("%0.16llx", v.toLongLong());
    e.setAttribute("value", val);
    break;
  }
  case QMetaType::ULong:
  case QMetaType::ULongLong:
  {
    e.setTagName("uint64");
    QString val;
    val = QString::asprintf("%0.16llx", v.toULongLong());
    e.setAttribute("value", val);
    break;
  }
  case QMetaType::Double:
    e.setTagName("double");
    e.setAttribute("value", v.toDouble());
    break;
  case QMetaType::Bool:
    e.setTagName("bool");
    e.setAttribute("value", boolString(v.toBool()));
    break;
  case QMetaType::QPoint:
    {
      e.setTagName("point");
      QPoint point = v.toPoint();
      e.setAttribute("x", point.x());
      e.setAttribute("y", point.y());
    }
    break;
  case QMetaType::QRect:
    {
      e.setTagName("rect");
      QRect rect = v.toRect();
      e.setAttribute("x", rect.x());
      e.setAttribute("y", rect.y());
      e.setAttribute("width", rect.width());
      e.setAttribute("height", rect.height());
    }
    break;
  case QMetaType::QSize:
    {
      e.setTagName("size");
      QSize qsize = v.toSize();
      e.setAttribute("width", qsize.width());
      e.setAttribute("height", qsize.height());
    }
    break;
  case QMetaType::QStringList:
    {
      e.setTagName("stringlist");
      uint j;
      
      QDomNode n;
      QDomNodeList stringNodeList = e.elementsByTagName("string");
      QDomElement stringElem;
      QStringList stringList = v.toStringList();
      QStringList::Iterator it = stringList.begin();

      for (j = 0; 
	   ((j < stringNodeList.length()) && (it != stringList.end()));
	   j++)
      {
	// get the current string element
	stringElem = stringNodeList.item(j).toElement();

	// set it to the current string
	variantToElement(QVariant(*it), stringElem);

	// iterate to the next string
	++it;
      }
      
      // more nodes in previous stringlist then current, remove excess nodes
      if (stringNodeList.count() > stringList.count())
      {
	while (j < stringNodeList.count())
	  e.removeChild(stringNodeList.item(j).toElement());
      }
      else if (j <stringList.count())
      {
	while (it != stringList.end())
	{
	  // create a new element
	  stringElem = m_doc.createElement("string");
	
	  // set it to the currentstring
	  variantToElement(QVariant(*it), stringElem);

	  // append it to the current element
	  e.appendChild(stringElem);

	  // iterate to the next string
	  ++it;
	}
      }
    }
    break;

#if 0
  case QVariant::List:
  case QVaraint::Map:
#endif
  default:
    qWarning("Don't know how to persist variant of type: %s (%d)!",
	     v.typeName(), v.type());
    ok = false;
    break;
  }

  return ok;
}

bool DomConvenience::getBoolFromString(const QString& s, bool& ok)
{
  bool val = false;
  if ((s == "true") || (s == "1"))
  {
    val = true;
    ok = true;
  }
  else if ((s == "false") || (s == "0"))
    ok = true;

  return val;
}

int DomConvenience::getBase(const QDomElement& e)
{
  int base = 10; 
  bool baseOk = false;
  if (e.hasAttribute("base")) 
  {
    base  = e.attribute("base").toInt(&baseOk);
    if (!baseOk)
      base = 10;
  }
  
  return base;
}

SeqColor DomConvenience::getColor(const QDomElement& e)
{
  SeqColor color;
  if (e.hasAttribute("name"))
    color = SeqColor(e.attribute("name"));

  // allow specifying of base color by name and then tweaking
  if (e.hasAttribute("red") ||
      e.hasAttribute("green") ||
      e.hasAttribute("blue"))
  {
    int base = getBase(e);
    bool colorOk = false;

    // default to black; if a name was already parsed, start from it
    int r = color.valid ? color.r : 0;
    int g = color.valid ? color.g : 0;
    int b = color.valid ? color.b : 0;

    if (e.hasAttribute("red"))
      r = e.attribute("red").toInt(&colorOk, base);
    if (e.hasAttribute("green"))
      g = e.attribute("green").toInt(&colorOk, base);
    if (e.hasAttribute("blue"))
      b = e.attribute("blue").toInt(&colorOk, base);

    color = SeqColor(uint8_t(r), uint8_t(g), uint8_t(b));
  }

  return color;
}

QString DomConvenience::boolString(bool b)
{
  return b ? QString("true") : QString("false");
}

void DomConvenience::clearAttributes(QDomElement& e)
{
  QDomNamedNodeMap attrMap = e.attributes();
  QDomNode attrNode;

  for (uint i = attrMap.length(); i > 0; --i)
    e.removeAttributeNode(attrMap.item(i - 1).toAttr());
}
