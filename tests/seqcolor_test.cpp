/*
 *  seqcolor_test.cpp
 *  Tier-1 unit test for the SeqColor POD parser/formatter.
 */

#include <QtTest/QtTest>

#include "seqcolor.h"

class SeqColorTest : public QObject
{
  Q_OBJECT

private slots:
  void defaultIsInvalid();
  void rgbCtorIsValid();
  void parseHex();
  void parseHexBadLength();
  void parseHexBadDigits();
  void parseNamedExact();
  void parseNamedCaseInsensitive();
  void parseNamedDarkVariants();
  void parseUnknownNameIsInvalid();
  void parseEmptyIsInvalid();
  void nameFormatLowercase();
  void nameRoundTrip();
  void equality();
};

void SeqColorTest::defaultIsInvalid()
{
  SeqColor c;
  QVERIFY(!c.isValid());
  QCOMPARE(c.r, uint8_t(0));
  QCOMPARE(c.g, uint8_t(0));
  QCOMPARE(c.b, uint8_t(0));
}

void SeqColorTest::rgbCtorIsValid()
{
  SeqColor c(10, 20, 30);
  QVERIFY(c.isValid());
  QCOMPARE(c.r, uint8_t(10));
  QCOMPARE(c.g, uint8_t(20));
  QCOMPARE(c.b, uint8_t(30));
}

void SeqColorTest::parseHex()
{
  SeqColor c(QStringLiteral("#ff8000"));
  QVERIFY(c.isValid());
  QCOMPARE(c.r, uint8_t(0xff));
  QCOMPARE(c.g, uint8_t(0x80));
  QCOMPARE(c.b, uint8_t(0x00));
}

void SeqColorTest::parseHexBadLength()
{
  SeqColor c(QStringLiteral("#abc"));
  QVERIFY(!c.isValid());
}

void SeqColorTest::parseHexBadDigits()
{
  SeqColor c(QStringLiteral("#zzzzzz"));
  QVERIFY(!c.isValid());
}

void SeqColorTest::parseNamedExact()
{
  SeqColor black(QStringLiteral("black"));
  QVERIFY(black.isValid());
  QCOMPARE(black.r, uint8_t(0));
  QCOMPARE(black.g, uint8_t(0));
  QCOMPARE(black.b, uint8_t(0));

  SeqColor white(QStringLiteral("white"));
  QCOMPARE(white.r, uint8_t(255));
  QCOMPARE(white.g, uint8_t(255));
  QCOMPARE(white.b, uint8_t(255));

  SeqColor red(QStringLiteral("red"));
  QCOMPARE(red.r, uint8_t(255));
  QCOMPARE(red.g, uint8_t(0));
  QCOMPARE(red.b, uint8_t(0));
}

void SeqColorTest::parseNamedCaseInsensitive()
{
  SeqColor a(QStringLiteral("Black"));
  SeqColor b(QStringLiteral("BLACK"));
  SeqColor c(QStringLiteral("black"));
  QVERIFY(a.isValid());
  QVERIFY(b.isValid());
  QVERIFY(c.isValid());
  QCOMPARE(a, b);
  QCOMPARE(a, c);
}

void SeqColorTest::parseNamedDarkVariants()
{
  // SVG/CSS3 darkgreen, not Qt::darkGreen — same convention as
  // QColor::setNamedColor in Qt5.
  SeqColor dg(QStringLiteral("darkGreen"));
  QVERIFY(dg.isValid());
  QCOMPARE(dg.r, uint8_t(0));
  QCOMPARE(dg.g, uint8_t(100));
  QCOMPARE(dg.b, uint8_t(0));

  SeqColor db(QStringLiteral("darkBlue"));
  QCOMPARE(db.r, uint8_t(0));
  QCOMPARE(db.g, uint8_t(0));
  QCOMPARE(db.b, uint8_t(139));

  SeqColor dm(QStringLiteral("DarkMagenta"));
  QCOMPARE(dm.r, uint8_t(139));
  QCOMPARE(dm.g, uint8_t(0));
  QCOMPARE(dm.b, uint8_t(139));
}

void SeqColorTest::parseUnknownNameIsInvalid()
{
  SeqColor c(QStringLiteral("notacolor"));
  QVERIFY(!c.isValid());
}

void SeqColorTest::parseEmptyIsInvalid()
{
  QString empty;
  SeqColor c(empty);
  QVERIFY(!c.isValid());
}

void SeqColorTest::nameFormatLowercase()
{
  SeqColor c(0xab, 0xcd, 0xef);
  QCOMPARE(c.name(), QStringLiteral("#abcdef"));
}

void SeqColorTest::nameRoundTrip()
{
  SeqColor a(0x12, 0x34, 0x56);
  SeqColor b(a.name());
  QVERIFY(b.isValid());
  QCOMPARE(a, b);
}

void SeqColorTest::equality()
{
  SeqColor a(1, 2, 3);
  SeqColor b(1, 2, 3);
  SeqColor c(4, 5, 6);
  SeqColor invalid;
  QVERIFY(a == b);
  QVERIFY(a != c);
  QVERIFY(a != invalid);
}

QTEST_APPLESS_MAIN(SeqColorTest)
#include "seqcolor_test.moc"
