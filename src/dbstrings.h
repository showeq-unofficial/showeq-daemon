#ifndef _DBSTRINGS_H_
#define _DBSTRINGS_H_

#include <cstdint>

#include <QHash>
#include <QString>

// Loader for dbstr_us.txt — modern EQ's dynamic-content text table
// (faction names, spell descriptions, /time output, EverQuest splash
// strings, etc.). Format (one entry per line, '^'-delimited):
//
//   <id1>^<id2>^<text>^<flags>^
//
// e.g.
//   2781^0^It is the deep dark of the night.  You can't see enough stars to estimate the time.^0^
//   1672^45^Friends of Zordak Ragefire^0^
//
// id1 is the primary key; id2 selects category/sub-meaning. The wire
// formattedMessage carries id1 (in messageFormat) but the id2 is
// implicit in the EQ client's context — there's no robust way to
// recover it from the wire alone. So we only index id1s that have a
// SINGLE entry in dbstr; multi-entry id1s (1672, 0, etc.) are
// skipped and fall through to the existing "Unknown: <id>: <args>"
// fallback rather than risk picking the wrong meaning.
class DbStrings
{
 public:
  bool load(const QString& path);
  bool isLoaded() const { return m_loaded; }
  int  size() const { return m_uniqueText.size(); }

  // Returns the text for `id1` if and only if dbstr has exactly one
  // (id1, *) entry for it; otherwise returns an empty QString.
  QString uniqueText(uint32_t id1) const;

 private:
  QHash<uint32_t, QString> m_uniqueText;
  bool m_loaded = false;
};

#endif // _DBSTRINGS_H_
