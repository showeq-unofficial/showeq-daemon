#include "combatrouter.h"

#include "everquest.h"
#include "spawn.h"
#include "spawnshell.h"
#include "spells.h"

#ifdef SEQ_USE_RUST
#include "seq-bridge-cxx/lib.h"
#include <cstring>
#endif

CombatRouter::CombatRouter(SpawnShell* spawnShell, Spells* spells,
                           QObject* parent)
    : QObject(parent)
    , m_spawnShell(spawnShell)
    , m_spells(spells)
{
}

// Returns the human-readable form of a spawn's name. For NPCs that's
// the cleanedName (drops the trailing instance digits and unescapes
// underscores into spaces — "a_pyre_beetle25" → "a pyre beetle"); for
// PCs it's the name verbatim. We deliberately skip the article-move
// done by transformedName() ("a pyre beetle" → "pyre beetle, a") since
// combat log lines read more naturally with the article in front.
static QString lookupSpawnName(SpawnShell* shell, uint16_t id)
{
    if (!shell || id == 0) return QString();
    const Item* it = shell->findID(tSpawn, id);
    if (!it) it = shell->findID(tPlayer, id);
    if (!it) return QString();
    if (const Spawn* sp = dynamic_cast<const Spawn*>(it)) {
        return sp->cleanedName();
    }
    return it->name();
}

void CombatRouter::action2(const uint8_t* data, size_t len, uint8_t /*dir*/)
{
    if (!data || len < sizeof(action2Struct)) return;
    [[maybe_unused]] action2Struct tmp;
    const action2Struct* a = nullptr;
#ifdef SEQ_USE_RUST
    if (m_useRustAction2) {
        auto out = seq::rust::decode_action2(
            rust::Slice<const uint8_t>{data, sizeof(action2Struct)});
        if (out.ok) {
            std::memset(&tmp, 0, sizeof(tmp));
            tmp.target = out.target;
            tmp.source = out.source;
            tmp.damage = out.damage;
            tmp.spell  = out.spell;
            tmp.type   = out.kind;
            a = &tmp;
        }
    }
#endif
    if (!a) a = reinterpret_cast<const action2Struct*>(data);

    QString sourceName = lookupSpawnName(m_spawnShell, a->source);
    QString targetName = lookupSpawnName(m_spawnShell, a->target);

    QString spellName;
    if (a->spell > 0 && m_spells) {
        if (const Spell* sp = m_spells->spell(static_cast<uint16_t>(a->spell))) {
            spellName = sp->name();
        }
    }

    emit combatEvent(a->source, sourceName,
                     a->target, targetName,
                     static_cast<uint32_t>(a->type),
                     a->damage,
                     static_cast<uint32_t>(a->spell), spellName);
}
