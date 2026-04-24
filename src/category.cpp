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

#include <QColorDialog>
#include <QHBoxLayout>
#include <QBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

// ------------------------------------------------------
// Category
Category::Category(const QString& name, 
		   const QString& filter, 
		   const QString& filterout, 
		   QColor color)
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
// CategoryDlg
CategoryDlg::CategoryDlg(QWidget *parent, QString name)
 : QDialog(parent, Qt::Dialog)
{
   setObjectName(name);
   setModal(true);
   setWindowTitle("Add Category");

   QFont labelFont;
   labelFont.setBold(true);

   QBoxLayout* topLayout = new QVBoxLayout(this);
   QBoxLayout* row4Layout = new QHBoxLayout();
   topLayout->addLayout(row4Layout);
   QBoxLayout* row3Layout = new QHBoxLayout();
   topLayout->addLayout(row3Layout);
   QBoxLayout* row2Layout = new QHBoxLayout();
   topLayout->addLayout(row2Layout);
   QBoxLayout* row1Layout = new QHBoxLayout();
   topLayout->addLayout(row1Layout);
   QBoxLayout* row0Layout = new QHBoxLayout();
   topLayout->addLayout(row0Layout);

   QLabel *colorLabel = new QLabel ("Color", this);
   colorLabel->setFont(labelFont);
   colorLabel->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
   row1Layout->addWidget(colorLabel, 0, Qt::AlignLeft);

   m_Color = new QPushButton(this);
   m_Color->setObjectName("color");
   m_Color->setText("...");
   m_Color->setFont(labelFont);
   connect(m_Color, SIGNAL(clicked()),
	   this, SLOT(select_color()));
   row1Layout->addWidget(m_Color);

   QLabel *nameLabel = new QLabel ("Name", this);
   nameLabel->setFont(labelFont);
   nameLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
   row4Layout->addWidget(nameLabel);

   m_Name = new QLineEdit(this);
   m_Name->setObjectName("Name");
   m_Name->setFont(labelFont);
   row4Layout->addWidget(m_Name);

   QLabel *filterLabel = new QLabel ("Filter", this);
   filterLabel->setFont(labelFont);
   filterLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
   row3Layout->addWidget(filterLabel);

   m_Filter  = new QLineEdit(this);
   m_Filter->setObjectName("Filter");
   m_Filter->setFont(labelFont);
   row3Layout->addWidget(m_Filter);

   QLabel *filteroutLabel = new QLabel ("FilterOut", this);
   filteroutLabel->setFont(labelFont);
   filteroutLabel->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
   row2Layout->addWidget(filteroutLabel);

   m_FilterOut  = new QLineEdit(this);
   m_FilterOut->setObjectName("FilterOut");
   m_FilterOut->setFont(labelFont);
   row2Layout->addWidget(m_FilterOut);

   QPushButton *ok = new QPushButton("OK", this);
   row0Layout->addWidget(ok, 0, Qt::AlignLeft);

   QPushButton *cancel = new QPushButton("Cancel", this);
   row0Layout->addWidget(cancel, 0, Qt::AlignRight);

   // Hook on pressing the buttons
   connect(ok, SIGNAL(clicked()), SLOT(accept()));
   connect(cancel, SIGNAL(clicked()), SLOT(reject()));
}

CategoryDlg::~CategoryDlg()
{
}

void CategoryDlg::select_color(void)
{
  QColor newColor =
    QColorDialog::getColor(palette().color(backgroundRole()), this);

  if (newColor.isValid())
    m_Color->setPalette(QPalette(QColor(newColor)));
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
					 QColor color)
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

void CategoryMgr::addCategory(QWidget* parent)
{
  // not editing an existing, adding a new
  editCategories(NULL, parent);
}

void CategoryMgr::editCategories(const Category* cat, QWidget* parent)
{
  // Create the filter dialog
  CategoryDlg* dlg = new CategoryDlg(parent, "CategoryDlg");

  // editing an existing category?
  if (cat != NULL)
  {
    // yes, use it's info for the defaults
    dlg->m_Name->setText(cat->name());
    dlg->m_Filter->setText(cat->filter());
    dlg->m_FilterOut->setText(cat->filterout());
    dlg->m_Color->setPalette(QPalette(QColor(cat->color())));
  }
  else
  {
    dlg->m_Name->setText("");
    dlg->m_Filter->setText(".");
    dlg->m_FilterOut->setText("");
    dlg->m_Color->setPalette(QPalette(QColor("black")));
  }

  // execute the dialog
  int res = dlg->exec();

  // if the dialog wasn't accepted, don't add/change a category
  if (res != QDialog::Accepted)
    return;

  // remove the old category
  if (cat != NULL)
    remCategory(cat);

  // Add Category
  QString name = dlg->m_Name->text();
  QString filter = dlg->m_Filter->text();

  //seqDebug("Got name: '%s', filter '%s', filterout '%s', color '%s'",
  //  name?name:"", color?color:"", filter?filter:"", filterout?filterout:""); 

  if (!name.isEmpty() && !filter.isEmpty())
    addCategory(name,
            filter,
            dlg->m_FilterOut->text(),
            dlg->m_Color->palette().color(dlg->m_Color->backgroundRole()));

  delete dlg;
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
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
    prefBaseName = QString::asprintf("Category%d_", i);
#else
    prefBaseName.sprintf("Category%d_", i);
#endif
    
    // attempt to pull a button title from the preferences
    tempStr = prefBaseName + "Name";
    if (pSEQPrefs->isPreference(tempStr, section))
    {
      QString name = pSEQPrefs->getPrefString(tempStr, section);
      QString filter =
	pSEQPrefs->getPrefString(prefBaseName + "Filter", section);
      QColor color = pSEQPrefs->getPrefColor(prefBaseName + "Color", 
					     section, QColor("black"));
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

#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
    prefBaseName = QString::asprintf("Category%d_", count++);
#else
    prefBaseName.sprintf("Category%d_", count++);
#endif
    pSEQPrefs->setPrefString(prefBaseName + "Name", section, 
			     curCategory->name());
    pSEQPrefs->setPrefString(prefBaseName + "Filter", section,
			     curCategory->filter());
    pSEQPrefs->setPrefString(prefBaseName + "FilterOut", section,
			     curCategory->filterout());
    pSEQPrefs->setPrefColor(prefBaseName + "Color", section,
			    curCategory->color());
  }

  QColor black("black");
  while (count <= tMaxNumCategories)
  {
#if (QT_VERSION >= QT_VERSION_CHECK(5,5,0))
    prefBaseName = QString::asprintf("Category%d_", count++);
#else
    prefBaseName.sprintf("Category%d_", count++);
#endif
    pSEQPrefs->setPrefString(prefBaseName + "Name", section, "");
    pSEQPrefs->setPrefString(prefBaseName + "Filter", section, "");
    pSEQPrefs->setPrefString(prefBaseName + "FilterOut", section, "");
    pSEQPrefs->setPrefColor(prefBaseName + "Color", section, black);
  }
}

#ifndef QMAKEBUILD
#include "category.moc"
#endif

