#ifndef _SPELLMESSAGES_H_
#define _SPELLMESSAGES_H_

#include <cstdint>

#include <QHash>
#include <QString>
#include <QStringList>

// Loader for spells_us_str.txt — the per-spell message text table that
// modern EQ clients consult for spell-flavored chat output.
//
// Format (one entry per line, '^'-delimited):
//   <spellId>^<casterMe>^<casterOther>^<castedMe>^<castedOther>^<spellGone>^
//
// e.g.
//   11^^^You feel the favor of the gods upon you.^ feels the favor of the gods upon them.^You no longer feel blessed.^
//
// The OP_SimpleMessage path on Test packs the spell ID into
// simpleMessageStruct::param0 and the column selector into
// messageFormat (2553 / 2601 / 2686 / 2702). MessageShell::simpleMessage
// dispatches to text(spellId, field).
class SpellMessages
{
 public:
  enum Field {
    CasterMe    = 1, // "You begin casting %1."
    CasterOther = 2, // "%1 begins casting %2."
    CastedMe    = 3, // "You feel ..."
    CastedOther = 4, // " feels ..." (target name prepended by caller)
    SpellGone   = 5, // "You no longer feel ..." / wear-off
  };

  bool load(const QString& path);
  bool isLoaded() const { return m_loaded; }
  int  size() const { return m_byId.size(); }

  // Returns the text for (spellId, field), or an empty QString if either
  // the spell isn't in the table or the column is empty.
  QString text(uint32_t spellId, Field field) const;

 private:
  QHash<uint32_t, QStringList> m_byId;
  bool m_loaded = false;
};

#endif // _SPELLMESSAGES_H_
