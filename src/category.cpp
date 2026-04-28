/*
 *  category.cpp
 *  Copyright 2001-2007, 2019 by the respective ShowEQ Developers
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

// Author: Zaphod (dohpaz@users.sourceforge.net)
//   Many parts derived from existing ShowEQ/SINS map code

//
// NOTE: Trying to keep this file ShowEQ/Everquest independent to allow it
// to be reused for other Show{} style projects.  Any existing ShowEQ/EQ
// dependencies will be migrated out.
//

#include "category.h"
#include "filter.h"
#include "diagnosticmessages.h"

// ZBTEMP: Temporarily use pSEQPrefs for data
#include "main.h"

#include <cstdio>

// ------------------------------------------------------
// Category
Category::Category(const QString& name, 
		   const QString& filter, 
		   const QString& filterout, 
		   SeqColor color)
{
  m_name = name;
  m_filter = filter;
  if (!filterout.isEmpty())
    m_filterout = filterout;
  m_color = color;

  int cFlags = REG_EXTENDED | REG_ICASE;

  // allocate the filter item
  m_filterItem = new FilterItem(filter, cFlags);
  m_filteredFilter = (filter.indexOf(":Filtered:", 0, Qt::CaseInsensitive) != -1);

  // allocate the filter out item
  if (m_filterout.isEmpty())
    m_filterOutItem = NULL;
  else
    m_filterOutItem = new FilterItem(filterout, cFlags);
}

Category::~Category()
{
  delete m_filterItem;
  delete m_filterOutItem;
}

bool Category::isFiltered(const QString& filterString, int level) const
{
  if ((m_filterItem != NULL) &&
      m_filterItem->isFiltered(filterString, level))
  {
    if ((m_filterOutItem != NULL) && 
	m_filterOutItem->isFiltered(filterString, level))
      return false;
    else
      return true;
  }

  return false;
}

// ------------------------------------------------------
// CategoryMgr
CategoryMgr::CategoryMgr(QObject* parent, const char* name)
  : QObject(parent)
{
  setObjectName(name);
  reloadCategories();
}

CategoryMgr::~CategoryMgr()
{
  qDeleteAll(m_categories);
  m_categories.clear();
}

const CategoryList CategoryMgr::findCategories(const QString& filterString, 
					       int level) const
{
  CategoryList tmpList;
  
  // iterate over all the categories looking for a match
  CategoryListIterator it(m_categories);
  Category* curCategory;
  while (it.hasNext())
  {
      curCategory = it.next();
      if (!curCategory)
          break;

    // if it matches the category add it to the dictionary
    if (curCategory->isFiltered(filterString, level))
      tmpList.append(curCategory);
  }

  return tmpList;
}

const Category* CategoryMgr::addCategory(const QString& name, 
					 const QString& filter, 
					 const QString& filterout, 
					 SeqColor color)
{
  //seqDebug("addCategory() '%s' - '%s':'%s'", name, filter, filterout?filterout:"null");
  
  // ZBTEMP: TODO, need to add check for duplicate category name
  m_changed = true;
  if (!name.isEmpty() && !filter.isEmpty()) 
  {
    Category* newcat = new Category(name, filter, filterout, color);
    
    m_categories.append(newcat);
    
    emit addCategory(newcat);
    
    //seqDebug("Added '%s'-'%s' '%s' %d", newcat->name, newcat->filter, newcat->listitem->text(0).ascii(), newcat->listitem);
     return newcat;
  }

  return NULL;
}

void CategoryMgr::remCategory(const Category* cat)
{
//seqDebug("remCategory()");
  m_changed = true;

  if (cat != NULL) 
  {
    // signal that the category is being deleted
    emit delCategory(cat);

    // remove the category from the list
    // note: indexOf shouldn't modify the input string, but gcc is giving
    // const errors anyway.  So we'll work around it.
    int i = m_categories.indexOf(const_cast<Category*>(cat));
    if (i != -1)
        delete m_categories.takeAt(i);
  }
}

void CategoryMgr::clearCategories(void)
{
  //seqDebug("clearCategories()");
  emit clearedCategories();

  m_categories.clear();
  m_changed = true;
}

void CategoryMgr::reloadCategories(void)
{
  clearCategories();
  m_changed = false;
  
  QString section = "CategoryMgr";
  int i = 0;
  QString prefBaseName;
  QString tempStr;
  for(i = 1; i <= tMaxNumCategories; i++)
  {
    prefBaseName = QString::asprintf("Category%d_", i);
    
    // attempt to pull a button title from the preferences
    tempStr = prefBaseName + "Name";
    if (pSEQPrefs->isPreference(tempStr, section))
    {
      QString name = pSEQPrefs->getPrefString(tempStr, section);
      QString filter =
	pSEQPrefs->getPrefString(prefBaseName + "Filter", section);
      SeqColor color = pSEQPrefs->getPrefColor(prefBaseName + "Color", 
					     section, SeqColor("black"));
      tempStr = prefBaseName + "FilterOut";
      QString filterout;
      if (pSEQPrefs->isPreference(tempStr, section))
	filterout = pSEQPrefs->getPrefString(tempStr, section);
	
      //seqDebug("%d: Got '%s' '%s' '%s'", i, name, filter, color);
      if (!name.isEmpty() && !filter.isEmpty())
      {
        Category* newcat = new Category(name, filter, filterout, color);
	
        m_categories.append(newcat);
      }
    }
  }
  
   // signal that the categories have been loaded
   emit loadedCategories();

   seqInfo("Categories Reloaded");
}

void CategoryMgr::savePrefs(void)
{
  if (!m_changed)
    return;

  int count = 1;
  QString section = "CategoryMgr";
  QString prefBaseName;

  CategoryListIterator it(m_categories);
  Category* curCategory;
  while(it.hasNext())
  {
      curCategory = it.next();
      if (!curCategory)
          break;

    prefBaseName = QString::asprintf("Category%d_", count++);
    pSEQPrefs->setPrefString(prefBaseName + "Name", section, 
			     curCategory->name());
    pSEQPrefs->setPrefString(prefBaseName + "Filter", section,
			     curCategory->filter());
    pSEQPrefs->setPrefString(prefBaseName + "FilterOut", section,
			     curCategory->filterout());
    pSEQPrefs->setPrefColor(prefBaseName + "Color", section,
			    curCategory->color());
  }

  SeqColor black("black");
  while (count <= tMaxNumCategories)
  {
    prefBaseName = QString::asprintf("Category%d_", count++);
    pSEQPrefs->setPrefString(prefBaseName + "Name", section, "");
    pSEQPrefs->setPrefString(prefBaseName + "Filter", section, "");
    pSEQPrefs->setPrefString(prefBaseName + "FilterOut", section, "");
    pSEQPrefs->setPrefColor(prefBaseName + "Color", section, black);
  }
}

#ifndef QMAKEBUILD
#include "category.moc"
#endif

