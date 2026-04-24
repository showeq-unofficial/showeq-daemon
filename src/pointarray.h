/*
 *  pointarray.h
 *  Copyright 2004-2005, 2019 by the respective ShowEQ Developers
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

// Original Author: Zaphod (dohpaz@users.sourceforge.net)
//   interfaces modeled after QPoint and QPointArray interface
//   but for 3D data of arbitraty type T.

//
// NOTE: Trying to keep this file ShowEQ/Everquest independent to allow it
// to be reused for other Show{} style projects. 
//

#ifndef __POINTARRAY_H_
#define __POINTARRAY_H_
#ifdef __FreeBSD__

#include "point.h"

#include <sys/types.h>
#else
#include <cstdint>
#endif
#include <QVector>
#include <QPoint>
#include <QPolygon>

// Point3DArray
template <class _T>
class Point3DArray : public QVector<Point3D<_T> >
{
 public:
  Point3DArray() {};
  Point3DArray(int size) : QVector<Point3D<_T> > (size) {}
  Point3DArray(const Point3DArray<_T>& array) : QVector<Point3D<_T> > (array) {}
  Point3DArray(uint32_t nPoints, const _T* points);
  ~Point3DArray() {};

  Point3DArray<_T>& operator=(const Point3DArray<_T>& array)
    { return (Point3DArray<_T>&)assign(array); }
  Point3DArray<_T> copy() const
    { Point3DArray<_T> tmp; return *((Point3DArray<_T>*)&tmp.duplicate(*this)); }

  QRect boundingRect() const;

  void point(uint32_t i, _T* x, _T* y, _T* z) const;
  const Point3D<_T>& point( uint32_t i) const;
  void setPoint(uint32_t i, _T x, _T y, _T z);
  void setPoint(uint32_t i, const Point3D<_T>& p);
  bool setPoints(uint32_t nPoints, const _T* points);
  bool setPoints(uint32_t nPoints, _T firstx, _T firsty, _T firstz, ...);
  bool putPoints(uint32_t index, uint32_t nPoints, const _T* points);
  bool putPoints(uint32_t index, uint32_t nPoints, _T firstx, _T firsty, _T firstz, ...);
  QPolygon getQPointArray();
};

template <class _T> inline 
Point3DArray<_T>::Point3DArray(uint32_t nPoints, const _T* points)
{
  setPoints(nPoints, points);
}

template <class _T> inline
QRect Point3DArray<_T>::boundingRect() const
{
  if (QVector<Point3D<_T> >::isEmpty())
    return QRect(0, 0, 0, 0);

  const Point3D<_T>* d = QVector<Point3D<_T> >::constData();
  _T minX, maxX, minY, maxY;

  minX = maxX = d->x();
  minY = maxY = d->y();
  
  uint32_t i;
  for (++d, i = 1;
       i < QVector<Point3D<_T> >::size();
       i++, d++)
  {
    if (d->x() < minX)
      minX = d->x();
    else if (d->x() > maxX)
      maxX = d->x();
    if (d->y() < minY)
      minY = d->y();
    else if (d->y() > maxY)
      maxY = d->y();
  }

  return QRect(QPoint(minX, minY), QPoint(maxX, maxY));
}

template <class _T> inline
void Point3DArray<_T>::point(uint32_t index, _T* x, _T* y, _T* z) const
{
  Point3D<_T> p = QVector<Point3D<_T> >::at(index);
  *x = p.x();
  *y = p. y();
  *z = p. z();
}

template <class _T> inline 
const Point3D<_T>& Point3DArray<_T>::point(uint32_t index) const
{
  return QVector<Point3D<_T> >::at(index);
}

template <class _T> inline
void Point3DArray<_T>::setPoint(uint32_t index, _T x, _T y, _T z)
{
  QVector<Point3D<_T> >::operator[](index) = Point3D<_T>(x, y, z);
}

template <class _T> inline 
void Point3DArray<_T>::setPoint(uint32_t index, const Point3D<_T>& p)
{
  QVector<Point3D<_T> >::operator[](index) = p;
}

template <class _T> inline
bool Point3DArray<_T>::setPoints(uint32_t nPoints, const _T* points)
{
  if (!QVector<Point3D<_T> >::resize(nPoints))
    return false;

  for (uint32_t i = 0; 
       nPoints;
       nPoints--, i++, points += 3)
    setPoint(i, *points, *(points + 1), *(points + 2));
	 
  return true;
}

template <class _T> inline
bool Point3DArray<_T>::setPoints(uint32_t nPoints, 
				 _T firstx, _T firsty, _T firstz, ...)
{
  if (!QVector<Point3D<_T> >::resize(nPoints))
    return false;

  setPoint( 0, firstx, firsty, firstz);

  va_list ap;
  va_start(ap, firstz);

  _T x, y, z;

  uint32_t i;
  for (i = 1, --nPoints;
       nPoints;
       nPoints--, i++)
  {
    x = va_arg(ap, _T);
    y = va_arg(ap, _T);
    z = va_arg(ap, _T);
    
    setPoint(i, x, y, z);
  }

  va_end(ap);

  return true;
}

template <class _T> inline
bool Point3DArray<_T>::putPoints(uint32_t index, uint32_t nPoints, const _T* points)
{
  if ((index + nPoints) > QVector<Point3D<_T> >::size())
    if (!QVector<Point3D<_T> >::resize(index + nPoints))
      return false;

  for (uint32_t i = index; 
       nPoints;
       nPoints--, i++, points += 3)
    setPoint(i, *points, *(points + 1), *(points + 2));
  
  return true;
}

template <class _T> inline
bool Point3DArray<_T>::putPoints(uint32_t index, uint32_t nPoints, 
			    _T firstx, _T firsty, _T firstz, ...)
{
  if ((index + nPoints) > QVector<Point3D<_T> >::size())
    if (!QVector<Point3D<_T> >::resize(index + nPoints))
      return false;

  setPoint( 0, firstx, firsty, firstz);

  va_list ap;
  va_start(ap, firstz);

  _T x, y, z;

  uint32_t i;
  for (i = index + 1, --nPoints;
       nPoints;
       nPoints--, i++)
  {
    x = va_arg(ap, _T);
    y = va_arg(ap, _T);
    z = va_arg(ap, _T);
    
    setPoint(i, x, y, z);
  }

  va_end(ap);

  return true;
}

template <class _T> inline
QPolygon Point3DArray<_T>::getQPointArray()
{
  // create a temporary QPointArray of the same size as this array
  QPolygon tmp(QVector<Point3D<_T> >::size());

  // copy each Point3D<_T> as a QPoint into the temporary QPointArray
  for (uint32_t i = 0; i < QVector<Point3D<_T> >::size(); i++)
    tmp.setPoint(i, point(i).qpoint());

  // return the temporary QPointArray
  return tmp;
}

#endif // __POINTARRAY_H_
