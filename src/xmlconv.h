/*
 *  xmlconv.h
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

#ifndef XMLCONV_H
#define XMLCONV_H

#include <QVariant>
#include <QDomDocument>

class DomConvenience
{
 public:
  DomConvenience(QDomDocument& doc);
  bool elementToVariant(const QDomElement& elem, QVariant& var);
  bool variantToElement(const QVariant& var, QDomElement& elem);

 protected:
  bool getBoolFromString(const QString& s, bool& ok);
  int getBase(const QDomElement& e);
  QColor getColor(const QDomElement& e);
  QString boolString(bool b);
  void clearAttributes(QDomElement& e);

 private:
  QDomDocument& m_doc;
};

#endif // XMLCONV_H
