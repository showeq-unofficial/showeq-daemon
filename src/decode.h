/*
 *  decode.h
 *  Copyright 2001-2003, 2019 by the respective ShowEQ Developers
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

#ifndef EQDECODE_H
#define EQDECODE_H

#ifdef __FreeBSD__
#include <sys/types.h>
#else
#include <cstdint>
#endif

#define FLAG_COMP         0x1000 // Compressed packet
#define FLAG_COMBINED     0x2000 // Combined packet
#define FLAG_CRYPTO       0x4000 // Encrypted packet
#define FLAG_IMPLICIT     0x8000 // Packet with implicit length
#define FLAG_DECODE       ( FLAG_COMP | FLAG_COMBINED | FLAG_IMPLICIT | FLAG_CRYPTO )

#endif	// EQDECODE_H
