/*
 *  fixpt.h
 *  Copyright 2001 Zaphod (dohpaz@users.sourceforge.net). All Rights Reserved.
 *  Copyright 2004, 2019 by the respective ShowEQ Developers
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

// Designed around common fixed point math practices

#ifndef _FIXPT_H_
#define _FIXPT_H_

typedef uint8_t qType;

// convert to and from floating point
template <class _T, class _F> inline
_T fixPtToFixed(_F d, qType q) 
{ 
  return _T(d * double(1 << q)); 
}

template <class _T, class _F> inline
_F fixPtToFloat(_T a, qType q) 
{ 
  return (double(a) / double(1 << q)); 
}

// convert a from format q1 to format q2
template <class _T> inline
_T fixPtConv(_T a, qType q1, qType q2) 
{
  return (q2 > q1) ? (a << (q2 - q1)) : (a >> (q1 - q2));
}

// Basic operations performed on two numbers a and b of fixed point q format
// and returning the answer in q format
template <class _T> inline
_T fixPtAdd(_T a, _T b) 
{ 
  return (a + b); 
}

template <class _T> inline
_T fixPtSub(_T a, _T b) 
{ 
  return (a - b); 
}

template <class _T> inline
_T fixPtMul(_T a, _T b, qType q) 
{ 
  return ((a * b) >> q); 
}

template <class _T> inline
_T fixPtDiv(_T a, _T b, qType q) 
{ 
  return (( 1 << q) / b); 
}

// Basic operations performed on two numbers where a is of fixed point format q
// and b is a generic integer returning result in fixed point q format
template <class _T, class _I> inline
_T fixPtAddI(_T a, qType q, _I b) 
{ 
  return (a + (b << q)); 
}

template <class _T, class _I> inline
_T fixPtSubI(_T a, qType q, _I b) 
{ 
  return (a - (b << q)); 
}

template <class _T, class _I> inline
_T fixPtMulI(_T a, qType q, _I b) 
{ 
  return (a * b); 
}

template <class _T, class _I> inline
_T fixPtDivI(_T a, qType q, _I b) 
{ 
  return (a / b); 
}
    
// Basic operations performed on two numbers where a is of fixed point q format
// and b is a generic integer returning result in integer format
template <class _T, class _I> inline
_I fixPtAddII(_T a, qType q, _I b) 
{ 
  return ((a >> q) + b); 
}

template <class _T, class _I> inline
_I fixPtSubII(_T a, qType q, _I b) 
{ 
  return ((a >> q) - b); 
}

template <class _T, class _I> inline
_I fixPtMulII(_T a, qType q, _I b) 
{ 
  return ((a * b) >> q); 
}

template <class _T, class _I> inline
_I fixPtDivII(_T a, qType q, _I b) 
{ 
  return ((a / b) >> q); 
}
    
// General operations between a in q1 format and b in q2 format returning
// result in q3 format
template <class _T> inline
_T fixPtAddG(_T a, qType q1, _T b, qType q2, qType q3)
{ 
  return (fixPtConv(a, q1, q3) + fixPtConv(b, q2, q3)); 
}

template <class _T> inline
_T fixPtSubG(_T a, qType q1, _T b, qType q2, qType q3)
{ 
  return (fixPtConv(a, q1, q3) - fixPtConv(b, q2, q3)); 
}

template <class _T> inline
_T fixPtMulG(_T a, qType q1, _T b, qType q2, qType q3)
{ 
  return fixPtConv((a * b), (q1 + q2), q3); 
}

template <class _T> inline
_T fixPtDivG(_T a, qType q1, _T b, qType q2, qType q3)
{ 
  return (fixPtConv(a, q1, (q2 + q3)) / b); 
}

#endif // _FIXPT_H_

