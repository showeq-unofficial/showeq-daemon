/*
 *  datalocationmgr.h
 *  Copyright 2003 Zaphod (dohpaz@users.sourceforge.net).
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

#ifndef _DATALOCATIONMGR_H_
#define _DATALOCATIONMGR_H_

#include <QString>
#include <QDir>
#include <QFileInfo>

class DataLocationMgr
{
 public:
  DataLocationMgr(const QString& homeSubDir);
  ~DataLocationMgr();
  bool setupUserDirectory();
  QFileInfo findExistingFile(const QString& subdir, const QString& filename,
			     bool caseSensitive = false, 
			     bool preferUser = true) const;
  QFileInfo findWriteFile(const QString& subdir, const QString& filename,
			  bool caseSensitive = true,
			  bool preferUser = true) const;

  QDir pkgDataDir(const QString& subdir) const;
  QDir userDataDir(const QString& subdir) const;

 protected:
  QFileInfo findFile(const QString& dir1, const QString& dir2,
		     const QString& subdir, const QString& filename,
		     bool caseSensitive = false) const;
  QFileInfo findFile(const QDir& dir, const QString& filename, 
		     bool caseSensitive = false, bool writable = false) const;
  QFileInfo findWriteFile(const QString& dir1, const QString& dir2,
			  const QString& subdir, const QString& filename,
			  bool caseSensitive = false) const;
  QDir findOrMakeSubDir(const QString& dir, const QString& subdir) const;

  QString m_pkgData;
  QString m_userData;
};

#endif // _DATALOCATIONMGR_H_


