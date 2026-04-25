#include "combatrouter.h"

#include "everquest.h"
#include "spawn.h"
#include "spawnshell.h"
#include "spells.h"

CombatRouter::CombatRouter(SpawnShell* spawnShell, Spells* spells,
                           QObject* parent)
    : QObject(parent)
    , m_spawnShell(spawnShell)
    , m_spells(spells)
{
}

static QString lookupSpawnName(SpawnShell* shell, uint16_t id)
{
    if (!shell || id == 0) return QString();
    if (const Item* it = shell->findID(tSpawn, id))   return it->name();
    if (const Item* it = shell->findID(tPlayer, id))  return it->name();
    return QString();
}

void CombatRouter::action2(const uint8_t* data, size_t len, uint8_t /*dir*/)
{
    if (!data || len < sizeof(action2Struct)) return;
    const action2Struct* a = reinterpret_cast<const action2Struct*>(data);

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

#ifndef QMAKEBUILD
#include "combatrouter.moc"
#endif
