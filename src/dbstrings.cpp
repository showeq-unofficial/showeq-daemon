#include "dbstrings.h"
#include "diagnosticmessages.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

namespace {

// dbstr tooltip-substitution markers used by spell/AA/item descriptions:
//   #1 #2 ...     numeric placeholders (e.g. "causing #1 damage")
//   %z %h %H ...  attribute placeholders (e.g. "for up to %z")
//   *@%N *#9%N    actor / proc self-references
//   @1 @2 ...     range upper bounds (e.g. "between #1 and @1 hp")
//   <BR> <c "...">  inline HTML for the spell-book renderer
// Any of these means we're looking at tooltip text that would render
// as gibberish in chat without the EQ client's display-time
// substitution engine. Filter them out at load time.
bool looksLikeTooltip(const QString& text)
{
    static const QRegularExpression rx(
        QStringLiteral("(#\\d|%[zhHnN]|\\*[@#]|@\\d|<BR>|<c \")"));
    return rx.match(text).hasMatch();
}

}

bool DbStrings::load(const QString& path)
{
  m_uniqueText.clear();
  m_names.clear();
  m_loaded = false;

  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    seqWarn("DbStrings: failed to open '%s'", path.toLatin1().data());
    return false;
  }

  // Only index id2=0 entries — the "primary message" sub-id (rare, ~83
  // entries) that holds /time output, splash strings, and similar
  // free-form chat lines. id2=4/6 contain item/spell tooltip text that
  // would need EQ-client-side substitution to render correctly; id2=22
  // / 23 / 45 / etc. categorize specific content (faction names,
  // descriptions). Fanning out across all id2s without context picks
  // the wrong meaning more often than not.
  QTextStream in(&f);
  while (!in.atEnd()) {
    const QString line = in.readLine();
    if (line.isEmpty() || line.startsWith('#'))
      continue;

    const QStringList fields = line.split('^');
    if (fields.size() < 3)
      continue;

    bool ok = false;
    const uint32_t id1 = fields[0].toUInt(&ok);
    if (!ok)
      continue;

    const QString& text = fields[2];
    if (text.isEmpty())
      continue;

    if (fields[1] == QStringLiteral("1")) {
      // type-1: display names (AA titles, spell names). Kept raw — these are
      // clean single-line strings the caller resolves by exact id (e.g.
      // OP_SendAATable titleSID). Last entry wins on a duplicate id.
      m_names.insert(id1, text);
    } else if (fields[1] == QStringLiteral("0")) {
      if (looksLikeTooltip(text))
        continue;
      m_uniqueText.insert(id1, text);
    }
  }
  m_loaded = !m_uniqueText.isEmpty() || !m_names.isEmpty();
  if (m_loaded)
    seqInfo("Loaded %d id2=0 + %d id2=1 dbstr entries from '%s'",
            m_uniqueText.size(), m_names.size(), path.toLatin1().data());
  return m_loaded;
}

QString DbStrings::uniqueText(uint32_t id1) const
{
  return m_uniqueText.value(id1, QString());
}

QString DbStrings::nameById(uint32_t id) const
{
  return m_names.value(id, QString());
}
