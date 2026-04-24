/*
 *  dbcommon.h
 *  Copyright 2001 Zaphod (dohpaz@users.sourceforge.net). All Rights Reserved.
 *  Copyright 2003, 2019 by the respective ShowEQ Developers
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

//
// NOTE: Trying to keep this file ShowEQ/Everquest independent to allow it
// to be reused for other Show{} style projects.  Any existing ShowEQ/EQ
// dependencies will be migrated out.
//

#ifndef DBCOMMON_H
#define DBCOMMON_H

#ifdef __FreeBSD__
#include <sys/types.h>
#else
#include <cstdint>
#endif

class Datum
{
 public:
  // default constructor
  Datum()
  {
    data = NULL;
    size = 0;
  }

  // convenience constructor
  Datum(void* data_, uint32_t size_)
  {
    data = data_;
    size = size_;
  }

  // public data members
  void* data;
  uint32_t size;
};

#endif // DBCOMMON_H
