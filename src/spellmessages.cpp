#include "spellmessages.h"
#include "diagnosticmessages.h"

#include <QFile>
#include <QTextStream>

bool SpellMessages::load(const QString& path)
{
  m_byId.clear();
  m_loaded = false;

  QFile f(path);
  if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
    seqWarn("SpellMessages: failed to open '%s'", path.toLatin1().data());
    return false;
  }

  QTextStream in(&f);
  while (!in.atEnd()) {
    const QString line = in.readLine();
    if (line.isEmpty() || line.startsWith('#'))
      continue;

    const QStringList fields = line.split('^');
    if (fields.size() < 2)
      continue;

    bool ok = false;
    const uint32_t id = fields[0].toUInt(&ok);
    if (!ok)
      continue;

    // Keep the 5 message columns. Trailing empty fields after the last
    // "^" are preserved so a caller asking for SpellGone on a spell that
    // has only a wears-off line still resolves correctly.
    QStringList msgs;
    msgs.reserve(5);
    for (int i = 1; i <= 5; ++i)
      msgs << (i < fields.size() ? fields[i] : QString());
    m_byId.insert(id, msgs);
  }

  m_loaded = !m_byId.isEmpty();
  if (m_loaded)
    seqInfo("Loaded %d spell-message entries from '%s'",
            m_byId.size(), path.toLatin1().data());
  return m_loaded;
}

QString SpellMessages::text(uint32_t spellId, Field field) const
{
  const auto it = m_byId.find(spellId);
  if (it == m_byId.end())
    return QString();
  const int idx = static_cast<int>(field) - 1;
  if (idx < 0 || idx >= it->size())
    return QString();
  return it->at(idx);
}
