/*
 *  xmlpreferences.cpp
 *  Copyright 2002-2003,2007 Zaphod (dohpaz@users.sourceforge.net). All Rights Reserved.
 *  Copyright 2002-2007, 2019 by the respective ShowEQ Developers
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


#include "xmlpreferences.h"
#include "xmlconv.h"

#include <cstdlib>

#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QRegExp>
#include <QKeySequence>
#include <QTextStream>


const float seqPrefVersion = 1.0;
const char* seqPrefName = "seqpreferences";
const char* seqPrefSysId = "seqpref.dtd";

XMLPreferences::XMLPreferences(const QString& defaultsFileName, 
			       const QString& inFileName)
  : m_defaultsFilename(defaultsFileName),
    m_filename(inFileName), 
    m_modified(0),
    m_runtimeSections(),
    m_userSections(),
    m_defaultsSections()
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
  m_templateDoc = QString::asprintf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<!DOCTYPE %s SYSTEM \"%s\">\n"
			"<seqpreferences version=\"%1.1f\">\n"
			"<!-- ============================================================= -->"
			"</seqpreferences>\n",
			seqPrefName, seqPrefSysId, seqPrefVersion);
#else
  m_templateDoc.sprintf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<!DOCTYPE %s SYSTEM \"%s\">\n"
			"<seqpreferences version=\"%1.1f\">\n"
			"<!-- ============================================================= -->"
			"</seqpreferences>\n",
			seqPrefName, seqPrefSysId, seqPrefVersion);
#endif


  // load the preferences
  load();
}

XMLPreferences::~XMLPreferences()
{
  for (PrefSectionDict::iterator it = m_userSections.begin(); it != m_userSections.end(); ++it)
  {
      qDeleteAll(**it);
      (*it)->clear();
  }
  qDeleteAll(m_userSections);
  m_userSections.clear();

  for (PrefSectionDict::iterator it = m_defaultsSections.begin(); it != m_defaultsSections.end(); ++it)
  {
      qDeleteAll(**it);
      (*it)->clear();
  }
  qDeleteAll(m_defaultsSections);
  m_defaultsSections.clear();

  for (CommentSectionDict::iterator it = m_commentSections.begin(); it != m_commentSections.end(); ++it)
  {
      qDeleteAll(**it);
      (*it)->clear();
  }
  qDeleteAll(m_commentSections);
  m_commentSections.clear();
}

void XMLPreferences::load()
{
  // load the default preferences
  loadPreferences(m_defaultsFilename, m_defaultsSections);

  // load the user preferences
  loadPreferences(m_filename, m_userSections);
}

void XMLPreferences::save()
{
  // save the user preferences iff they've changed
  if (m_modified & User)
    savePreferences(m_filename, m_userSections);

  // save the user preferences iff they've changed
  if (m_modified & Defaults)
    savePreferences(m_defaultsFilename, m_defaultsSections);
}

void XMLPreferences::revert()
{
  // clear out all default preferecnes
  for (PrefSectionDict::iterator it = m_defaultsSections.begin(); it != m_defaultsSections.end(); ++it)
  {
      qDeleteAll(**it);
      (*it)->clear();
  }
  qDeleteAll(m_defaultsSections);
  m_defaultsSections.clear();

  // clear out all user preferences
  for (PrefSectionDict::iterator it = m_userSections.begin(); it != m_userSections.end(); ++it)
  {
      qDeleteAll(**it);
      (*it)->clear();
  }
  qDeleteAll(m_userSections);
  m_userSections.clear();


  // load the default preferences
  loadPreferences(m_defaultsFilename, m_defaultsSections);

  // load the user preferences back in from the file
  loadPreferences(m_filename, m_userSections);
}

void XMLPreferences::loadPreferences(const QString& filename, 
				     PrefSectionDict& dict)
{
  QDomDocument doc(seqPrefName);
  QFile f(filename);
  if (!f.open(QIODevice::ReadOnly))
  {
    qWarning("Unable to open file: %s!", filename.toLatin1().data());
    return;
  }

  QString errorMsg;
  int errorLine = 0;
  int errorColumn = 0;
  if (!doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
  {
    qWarning("Error processing file: %s!\n\t %s on line %d in column %d!", 
            filename.toLatin1().data(), errorMsg.toLatin1().data(), errorLine, errorColumn);
    f.close();
    return;
  }

  // do more processing here
 QDomElement docElem = doc.documentElement();
 DomConvenience conv(doc);
 QDomNodeList sectionList, propertyList;
 PreferenceDict* sectionDict;
 CommentDict* commentSectionDict;
 QString comment;
 QString* commentVal;
 QDomElement section;
 QDomElement property;
 QString sectionName;
 QString propertyName;
 QDomNode n;
 QDomElement valueElement;
 bool foundValue;

 sectionList = doc.elementsByTagName("section");
 for (uint i = 0; i < sectionList.length(); i++)
 {
   section = sectionList.item(i).toElement();
   if (!section.hasAttribute("name"))
   {
     qWarning("section without name!");
     continue;
   }

   sectionName = section.attribute("name");

   // see if the section exists in the dictionary
   sectionDict = dict.value(sectionName, nullptr);

   // if not, then create it
   if (sectionDict == NULL)
   {
     // create the new preference dictionary
     sectionDict = new PreferenceDict();

     // insert the preference dictionary into the section
     dict.insert(sectionName, sectionDict);
   }

   // see if comment section exists in the dictionary
   commentSectionDict = m_commentSections.value(sectionName, nullptr);

   // if not, then create it
   if (commentSectionDict == NULL)
   {
     // create the new preference dictionary
     commentSectionDict = new CommentDict();

     // insert the preference dictionary into the section
     m_commentSections.insert(sectionName, commentSectionDict);
   }

   propertyList = section.elementsByTagName("property");
   
   for (uint j = 0; j < propertyList.length(); j++)
   {
     property = propertyList.item(j).toElement();
     if (!property.hasAttribute("name"))
     {
       qWarning("property in section '%s' without name! Ignoring!",
               sectionName.toLatin1().data());
       continue;
     }

     propertyName = property.attribute("name");

     foundValue = false;

     QVariant value;
     // iterate over the nodes under the property
     for (n = property.firstChild(); !n.isNull(); n = n.nextSibling())
     {
       if (!n.isElement())
	 continue;

       valueElement = n.toElement();

       if (valueElement.tagName() == "comment")
       {
	 // get comment if any
	 comment = valueElement.text();

	 // if there is a comment, cache it
         if (!comment.isEmpty())
         {
	   commentVal = commentSectionDict->value(propertyName, nullptr);
	   
	   if (commentVal != NULL)
	     *commentVal = comment;
	   else
	     commentSectionDict->insert(propertyName, 
					new QString(comment));
	 }

	 continue;
       }

       if (!conv.elementToVariant(valueElement, value))
       {
           qWarning("property '%s' in section '%s' with bogus value in tag '%s'!"
                   " Ignoring!",
                   propertyName.toLatin1().data(), sectionName.toLatin1().data(),
                   valueElement.tagName().toLatin1().data());

           continue;
       }

       // found the value
       foundValue = true;
       
       // insert value into the section dictionary
       sectionDict->insert(propertyName, new QVariant(value));
       
       break;
     }

#if 0 // ZBTEMP : Support properties without values to get comments?
     if (!foundValue)
     {
       qWarning("property '%s' in section '%s' without value! Ignoring!",
		(const char*)propertyName, (const char*)sectionName);
       continue;
     }
#endif
   }
 }

  // close the file
  f.close();

#if 1 // ZBTEMP
  printf("Loaded preferences file: %s!\n", filename.toLatin1().data());
#endif
}

void XMLPreferences::savePreferences(const QString& filename, 
				     PrefSectionDict& dict)
{
  // open the existing preference file
  QDomDocument doc;
  QFile f(filename);
  bool loaded = false;
  if (f.open(QIODevice::ReadOnly))
  {
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;
    if (doc.setContent(&f, false, &errorMsg, &errorLine, &errorColumn))
      loaded = true;
    else
    {
      qWarning("Error processing file: %s!\n\t %s on line %d in column %d!",
              filename.toLatin1().data(),
              errorMsg.toLatin1().data(), errorLine, errorColumn);

    }

    // close the file
    f.close();
  }

  // if no file was loaded, use the template document
  if (!loaded)
  {
    QString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;
    if (doc.setContent(m_templateDoc, false, &errorMsg, &errorLine, &errorColumn))
      loaded = true;
  }

  // if there was an existing file, rename it
  QFileInfo fileInfo(filename);
  if (fileInfo.exists())
  {
    QDir dir(fileInfo.absolutePath());

    dir.rename(filename, filename + QString(".bak"));
  }

  // do more processing here
  QDomElement docElem = doc.documentElement();
  DomConvenience conv(doc);
  QDomNodeList sectionList, propertyList;
  PreferenceDict* sectionDict;
  QString sectionName;
  QString propertyName;
  QVariant* propertyValue;
  QDomElement e;
  QDomNode n;

  sectionList = docElem.elementsByTagName("section");

  QHashIterator<QString, PreferenceDict*> sdit(dict);
  while (sdit.hasNext())
  {
    sdit.next();

    QDomElement section;
    sectionName = sdit.key();
    sectionDict = sdit.value();

    // iterate over all the sections in the document
    for (uint i = 0; i < sectionList.length(); i++)
    {
      e = sectionList.item(i).toElement();
      if (!e.hasAttribute("name"))
      {
	qWarning("section without name!");
	continue;
      }

      //      printf("found section: %s\n", (const char*)section.attribute("name"));

      // is this the section?
      if (sectionName == e.attribute("name"))
      {
	// yes, done
	section = e;
	break;
      }
    }

    // if no section was found, create a new one
    if (section.isNull())
    {
      // create the section element
      section = doc.createElement("section");

      // set the name attribute of the section element
      section.setAttribute("name", sectionName);

      // append the new section to the document
      docElem.appendChild(section);
    }

    // iterate over all the properties in the section
    QHashIterator<QString, QVariant*> pdit(*sectionDict);
    while (pdit.hasNext())
    {
      pdit.next();

      QDomElement property;
      propertyName = pdit.key();
      propertyValue = pdit.value();

      // get all the property elements in the section
      propertyList = section.elementsByTagName("property");

      // iterate over all the property elements until a match is found
      for (uint j = 0; j < propertyList.length(); j++)
      {
          e = propertyList.item(j).toElement();
          if (!e.hasAttribute("name"))
          {
              qWarning("property in section '%s' without name! Ignoring!",
                      sectionName.toLatin1().data());
              continue;
          }

	// is this the property being searched for?
	if (propertyName == e.attribute("name"))
	{
	  // yes, done
	  property = e;
	  break;
	}
      }

      // if no property was found, create a new one
      if (property.isNull())
      {
	// create the property element
	property = doc.createElement("property");

	// set the name attribute of the property element
	property.setAttribute("name", propertyName);

	// append the new property to the section
	section.appendChild(property);
      }

      QDomElement value;

      // iterate over the children
      for (n = property.firstChild();
	   !n.isNull();
	   n = n.nextSibling())
      {
	if (!n.isElement())
	  continue;

	// don't replace comments
	if (n.toElement().tagName() == "comment")
	  continue;

	// convert it to an element
	value = n.toElement();
	break;
      }

      // if no value element was found, create a new one
      if (value.isNull())
      {
	// create the value element, bogus type, will be filled in later
	value = doc.createElement("bogus");
	
	// append the new value to the property
	property.appendChild(value);
      }

      if (!conv.variantToElement(*propertyValue, value))
      {
          qWarning("Unable to set value element in section '%s' property '%s'!",
                  propertyName.toLatin1().data(), propertyName.toLatin1().data());
      }
    }
  }

  // write the modified DOM to disk
  if (!f.open(QIODevice::WriteOnly))
  {
    qWarning("Unable to open file for writing: %s!",
            filename.toLatin1().data());
  }

  // open a Text Stream on the file
  QTextStream out(&f);

  // make sure stream is UTF8 encoded
  out.setCodec("UTF-8");

  // save the document to the text stream
  QString docText;
  QTextStream docTextStream(&docText, QIODevice::WriteOnly);
  doc.save(docTextStream, 4);

  // put newlines after comments (which unfortunately Qt's DOM doesn't track)
  QRegExp commentEnd("-->");
  docText.replace(commentEnd, "-->\n");

  // write the fixed document out to the file
  out << docText;

  // close the file
  f.close();

  printf("Finished saving preferences to file: %s\n",
          filename.toLatin1().data());
}

QVariant* XMLPreferences::getPref(const QString& inName, 
				  const QString& inSection,
				  Persistence pers)
{
  PreferenceDict* sectionDict;
  QVariant* preference = NULL;

  if (pers & Runtime)
  {
    // see if the section exists in the dictionary
    sectionDict = m_runtimeSections.value(inSection, nullptr);

    // if so, then see if the preference exists
    if (sectionDict != NULL)
    {
      preference = sectionDict->value(inName, nullptr);
      if (preference != NULL)
	return preference;
    }
  }

  if (pers & User)
  {
    // see if the section exists in the dictionary
    sectionDict = m_userSections.value(inSection, nullptr);
    
    // if so, then see if the preference exists
    if (sectionDict != NULL)
    {
      preference = sectionDict->value(inName, nullptr);
      if (preference != NULL)
	return preference;
    }
  }

  if (pers & Defaults)
  {
    // see if the section exists in the defaults dictionary
    sectionDict = m_defaultsSections.value(inSection, nullptr);
    
    // if so, then see if the preferences exists
    if (sectionDict != NULL)
    {
      preference = sectionDict->value(inName, nullptr);
      if (preference != NULL)
	return preference;
    }
  }

  return preference;
}

void XMLPreferences::setPref(const QString& inName, const QString& inSection, 
			     const QVariant& inValue, Persistence pers)
{
  // set the preference in the appropriate section
  if (pers & Runtime)
    setPref(m_runtimeSections, inName, inSection, inValue);
  if (pers & User)
    setPref(m_userSections, inName, inSection, inValue);
  if (pers & Defaults)
    setPref(m_defaultsSections, inName, inSection, inValue);

  m_modified |= pers;
}

void XMLPreferences::setPref(PrefSectionDict& dict,
			     const QString& inName, const QString& inSection, 
			     const QVariant& inValue)
{
  PreferenceDict* sectionDict;
  QVariant* preference;

   // see if the section exists in the dictionary
  sectionDict = dict.value(inSection, nullptr);

   // if not, then create it
  if (sectionDict == NULL)
  {
     // create the new preference dictionary
     sectionDict = new PreferenceDict();

     // insert the preference dictionary into the section
     dict.insert(inSection, sectionDict);
  }

  preference = sectionDict->value(inName, nullptr);

  // if preference exists, change it, otherwise create it
  if (preference != NULL)
    *preference = inValue;
  else
    sectionDict->insert(inName, new QVariant(inValue));
}

QString XMLPreferences::getPrefComment(const QString& inName, const QString& inSection)
{
 CommentDict* commentSectionDict;

 // see if comment section exists in the dictionary
 commentSectionDict = m_commentSections.value(inSection, nullptr);

 if (commentSectionDict == NULL)
   return QString("");

 QString* comment = commentSectionDict->value(inName, nullptr);
 
 if (comment != NULL)
   return *comment;

 return QString("");
}

bool XMLPreferences::isSection(const QString& inSection, 
			       Persistence pers)
{
  PreferenceDict* sectionDict;

  if (pers & Runtime)
  {
    // see if the section exists in the dictionary
    sectionDict = m_runtimeSections.value(inSection, nullptr);

    // if so, then see if the preference exists
    if (sectionDict != NULL)
      return true;
  }

  if (pers & User)
  {
    // see if the section exists in the dictionary
    sectionDict = m_userSections.value(inSection, nullptr);
    
    // if so, then see if the preference exists
    if (sectionDict != NULL)
      return true;
  }

  if (pers & Defaults)
  {
    // see if the section exists in the defaults dictionary
    sectionDict = m_defaultsSections.value(inSection, nullptr);
    
    // if so, then see if the preferences exists
    if (sectionDict != NULL)
      return true;
  }

  return false;
}
			       
bool XMLPreferences::isPreference(const QString& inName, 
				  const QString& inSection,
				  Persistence pers)
{
  // try to retrieve the preference
  QVariant* preference = getPref(inName, inSection, pers);

  return (preference != NULL);
}

// easy/sleezy way to create common get methods
#define getPrefMethod(retType, Type, Def) \
  retType XMLPreferences::getPref##Type(const QString& inName, \
                                        const QString& inSection, \
                                        Def def, \
                                        Persistence pers) \
  { \
    QVariant* preference = getPref(inName, inSection, pers); \
    \
    if (preference) \
      return preference->value< retType >(); \
    \
    return def; \
  } 

// implement common get methods
getPrefMethod(QString, String, const QString&);
getPrefMethod(int, Int, int);
getPrefMethod(uint, UInt, uint);
getPrefMethod(double, Double, double);
getPrefMethod(bool, Bool, bool);
getPrefMethod(QColor, Color, const QColor&);
getPrefMethod(QPen, Pen, const QPen&);
getPrefMethod(QBrush, Brush, const QBrush&);
getPrefMethod(QPoint, Point, const QPoint&);
getPrefMethod(QRect, Rect, const QRect&);
getPrefMethod(QSize, Size, const QSize&);
getPrefMethod(QFont, Font, const QFont&);
getPrefMethod(QSizePolicy, SizePolicy, const QSizePolicy&);
getPrefMethod(QCursor, Cursor, const QCursor&);
getPrefMethod(QStringList, StringList, const QStringList&);

// implement get methods that require special behavior
QKeySequence XMLPreferences::getPrefKey(const QString& inName,
			       const QString& inSection,
			       const QString& def,
			       Persistence pers)
{
  return getPrefKey(inName, inSection, QKeySequence(def), pers);
}

QKeySequence XMLPreferences::getPrefKey(const QString& inName,
			       const QString& inSection,
			       const QKeySequence& def,
			       Persistence pers)
{
  // try to retrieve the preference
  QVariant* preference = getPref(inName, inSection, pers);

  // if preference was retrieved, return it as a string
  if (preference != NULL)
  {

    switch(preference->type())
    {
    case QVariant::KeySequence:
      return preference->value<QKeySequence>();
    case QVariant::String:
      // convert it to a key
      return QKeySequence(preference->value<QKeySequence>());
    case QVariant::Int:
    case QVariant::UInt:
    case QVariant::Double:
      return QKeySequence(preference->value<QKeySequence>());
    default:
      qWarning("XMLPreferences::getPrefKey(%s, %s, %s): preference found,\n"
              "\tbut type %s is not convertable to type key!",
              inName.toLatin1().data(), inSection.toLatin1().data(),
              def.toString().toLatin1().data(), preference->typeName());
      return QKeySequence(def);
    }

  }

  // return the default value
  return QKeySequence(def);
}


int64_t XMLPreferences::getPrefInt64(const QString& inName, 
				     const QString& inSection, 
				     int64_t def, 
				     Persistence pers)
{
  // try to retrieve the preference
  QVariant* preference = getPref(inName, inSection, pers);

  // if preference was retrieved, return it as a string
  if (preference != NULL)
  {
    int64_t value = def;

    switch(preference->type())
    {
    case QMetaType::QString:
      // convert it to a uint64_t (in base 16)
      //TODO ok
      value = strtoll(preference->toString().toLatin1().data(), 0, 16);
      break;
    case QMetaType::UInt:
    case QMetaType::Int:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::ULongLong:
    case QMetaType::LongLong:

      value = preference->toLongLong();
      break;
    case QMetaType::Double:
      value = int64_t(preference->toDouble());
      break;

    default:
      qWarning("XMLPreferences::getPrefInt64(%s, %s, %llu): preference found,\n"
              "\tbut type %s is not convertable to type int64_t!",
              inName.toLatin1().data(), inSection.toLatin1().data(),
              (unsigned long long)def,
              preference->typeName());

              value=def;
              break;
    } //end switch

    // return the key
    return value;
  }

  // return the default value
  return def;
}

uint64_t XMLPreferences::getPrefUInt64(const QString& inName, 
				       const QString& inSection, 
				       uint64_t def, 
				       Persistence pers)
{
  // try to retrieve the preference
  QVariant* preference = getPref(inName, inSection, pers);

  // if preference was retrieved, return it as a string
  if (preference != NULL)
  {
    uint64_t value = def;

    switch(preference->type())
    {
    case QMetaType::QString:
      // convert it to a uint64_t (in base 16)
      // TODO ok
      value = strtoull(preference->toString().toLatin1().data(), 0, 16);
      break;
    case QMetaType::UInt:
    case QMetaType::Int:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::ULongLong:
    case QMetaType::LongLong:

      value = preference->toULongLong();
      break;
    case QMetaType::Double:
      value = uint64_t(preference->toDouble());
      break;

    default:
      qWarning("XMLPreferences::getPrefUInt64(%s, %s, %llu): preference found,\n"
              "\tbut type %s is not convertable to type uint64_t!",
              inName.toLatin1().data(), inSection.toLatin1().data(),
              (unsigned long long)def,
              preference->typeName());

              value=def;
              break;
    } //end switch

    // return the key
    return value;
  }

  // return the default value
  return def;
}


QVariant XMLPreferences::getPrefVariant(const QString& inName, 
					const QString& inSection,
					const QVariant& outDefault,
					Persistence pers)
{
  // try to retrieve the preference
  QVariant* preference = getPref(inName, inSection, pers);

  // if preference was retrieved, return it as a string
  if (preference)
    return *preference;

  // return the default value
  return outDefault;
}

// generic common set preference method
#define setPrefMethod(Type, In) \
  void XMLPreferences::setPref##Type(const QString& inName, \
				     const QString& inSection, \
				     In inValue, \
				     Persistence pers) \
  { \
    setPref(inName, inSection, QVariant(inValue), pers); \
  } 

// create the methods for the types that can be handled the common way
setPrefMethod(String, const QString&);
setPrefMethod(Int, int);
setPrefMethod(UInt, uint);
setPrefMethod(Double, double);
setPrefMethod(Color, const QColor&);
setPrefMethod(Pen, const QPen&);
setPrefMethod(Brush, const QBrush&);
setPrefMethod(Point, const QPoint&);
setPrefMethod(Rect, const QRect&);
setPrefMethod(Size, const QSize&);
setPrefMethod(Font, const QFont&);
setPrefMethod(SizePolicy, const QSizePolicy&);
setPrefMethod(StringList, const QStringList&);

void XMLPreferences::setPrefBool(const QString& inName, 
				 const QString& inSection,
				 bool inValue,
				 Persistence pers)
{
  setPref(inName, inSection, QVariant(inValue), pers);
}

void XMLPreferences::setPrefKey(const QString& inName,
				const QString& inSection,
				int inValue,
				Persistence pers)
{
  setPref(inName, inSection, QVariant::fromValue<QKeySequence>(inValue), pers);
}

void XMLPreferences::setPrefInt64(const QString& inName,
				  const QString& inSection,
				  int64_t inValue,
				  Persistence pers)
{
  QVariant tmp;
  tmp.setValue((qlonglong)inValue);
  setPref(inName, inSection, tmp, pers);
}


void XMLPreferences::setPrefUInt64(const QString& inName,
				   const QString& inSection,
				   uint64_t inValue,
				   Persistence pers)
{
  QVariant tmp;
  tmp.setValue((qulonglong)inValue);
  setPref(inName, inSection, tmp, pers);
}

void XMLPreferences::setPrefVariant(const QString& inName, 
				    const QString& inSection,
				    const QVariant& inValue, 
				    Persistence pers)
{
  setPref(inName, inSection, inValue, pers);
}

#ifndef QMAKEBUILD
#include "xmlpreferences.moc"
#endif
