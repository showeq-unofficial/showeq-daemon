/*
 *  seqcolor.h
 *  Copyright 2019, 2026 by the respective ShowEQ Developers
 *
 *  This file is part of ShowEQ.
 *  http://www.sourceforge.net/projects/seq
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef SEQCOLOR_H
#define SEQCOLOR_H

#include <cstdint>

#include <QMetaType>
#include <QString>

// SeqColor is a small RGB POD that replaces QColor in the headless
// daemon. It carries an explicit `valid` flag to mirror QColor's
// default-constructed-is-invalid behavior, and parses both "#RRGGBB"
// and the small set of Qt color names that the legacy showeq prefs
// schema uses ("black", "white", "red", "green", "blue", "yellow",
// "cyan", "magenta", "gray", plus "darkBlue"/"darkCyan"/"darkGreen"/
// "darkMagenta"). Anything else returns invalid.
struct SeqColor
{
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  bool valid = false;

  SeqColor() = default;
  SeqColor(uint8_t r_, uint8_t g_, uint8_t b_)
    : r(r_), g(g_), b(b_), valid(true) {}
  explicit SeqColor(const QString& spec);

  bool isValid() const { return valid; }
  QString name() const;

  bool operator==(const SeqColor& o) const
  { return valid == o.valid && r == o.r && g == o.g && b == o.b; }
  bool operator!=(const SeqColor& o) const { return !(*this == o); }
};

Q_DECLARE_METATYPE(SeqColor)

#endif // SEQCOLOR_H
