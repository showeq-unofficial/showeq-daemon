/*
 *  spellshell.cpp
 *  Copyright 2001 Crazy Joe Divola (cjd1@users.sourceforge.net)
 *  Portions Copyright 2003-2007 Zaphod (dohpaz@users.sourceforge.net).
 *  Copyright 2001-2007, 2019 by the respective ShowEQ Developers
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

#include "spellshell.h"
#include "util.h"
#include "player.h"
#include "spawnshell.h"
#include "spells.h"
#include "packetcommon.h"

#include "spawn.h"
#include "diagnosticmessages.h"
#include <QList>

//#define DIAG_SPELLSHELL 1 

SpellItem::SpellItem()
  : m_duration(0),
    m_castTime{0, 0},
    m_spellId(0),
    m_casterId(0),
    m_targetId(0),
    m_buffSlot(-1),
    m_isSong(false)
{
    // m_cast (startCastStruct) is a wire struct populated by update();
    // not read until then. m_spellName/m_casterName/m_targetName default
    // to empty QStrings — fine.
}

void SpellItem::updateCastTime()
{
  struct timezone tz;
  gettimeofday(&m_castTime,&tz);
}

QString SpellItem::castTimeStr() const
{
   QString text;
   // using system_spawntime for now...
   if (showeq_params->systime_spawntime)
      text = QString("%1").arg(castTime());
   else {
      /* Friendlier format courtesy of Daisy */
      struct tm *CreationLocalTime = localtime( &(m_castTime.tv_sec) );
      /* tzname should be set by localtime() but this doesn't seem to
         work.  cpphack */
      char buff[256];
      sprintf(buff, "%02d:%02d:%02d", CreationLocalTime->tm_hour,
              CreationLocalTime->tm_min, CreationLocalTime->tm_sec);
      text = QString(buff);
   }
   return text;
}

QString SpellItem::durationStr() const
{
   QString text;
   int d = m_duration;
   if (d < 0)
      d = 0;
   int h = d / 3600;
   d %= 3600;

   text = QString::asprintf("%02d:%02d:%02d", h, d / 60, d % 60);
   return text;
}

void SpellItem::update(uint32_t spellId, const Spell* spell, int duration,
		       uint16_t casterId, const QString& casterName,
		       uint16_t targetId, const QString& targetName)
{
     setSpellId(spellId);

     setDuration(duration);

     if (spell)
     {
       setSpellName(spell->name());
       m_isSong = spell->isSong();

       if (spell->targetType() != 0x06)
	 setTargetId(targetId);
     }
     else
     {
       setSpellName(spell_name(spellId));
       m_isSong = false;
       setTargetId(targetId);
     }

     setCasterId(casterId);

     if (!casterName.isEmpty())
       setCasterName(casterName);
     else
       setCasterName(QString("N/A"));

     if (!targetName.isEmpty())
       setTargetName(targetName);
     else
       setTargetName(QString("N/A"));

     updateCastTime();
}


SpellShell::SpellShell(Player* player, SpawnShell* spawnshell, Spells* spells)
: QObject(NULL),
  m_player(player),
  m_spawnShell(spawnshell),
  m_spells(spells),
  m_lastPlayerSpell(0)
{
   setObjectName("spellshell");
   m_timer = new QTimer(this);
   m_spellList.clear();
   connect(m_timer, SIGNAL(timeout()), SLOT(timeout()));
}

SpellShell::~SpellShell()
{
  clear();
}

SpellItem* SpellShell::findSpell(uint32_t spellId,
				 uint16_t targetId, const QString& targetName)
{
  for(QList<SpellItem*>::Iterator it = m_spellList.begin();
      it != m_spellList.end(); 
      it++) 
  {
    SpellItem *i = *it;
    if (i->spellId() == spellId)
    {
      if ((i->targetId() == targetId) || 
	  ((i->targetId() == 0) && (i->targetName() == targetName)))
	return i;
    }
  }

  return NULL;
}

SpellItem* SpellShell::findSpell(uint32_t spell_id)
{
  for(QList<SpellItem*>::Iterator it = m_spellList.begin();
      it != m_spellList.end();
      it++)
  {
    SpellItem *si = *it;

    if (si->spellId() == spell_id)
      return si;
  }

  return NULL;
}

SpellItem* SpellShell::findSpellBySlot(int slot)
{
  for (SpellItem* si : m_spellList)
    if (si->buffSlot() == slot)
      return si;
  return nullptr;
}

void SpellShell::clear()
{
   emit clearSpells();

   m_lastPlayerSpell = 0;
   for(QList<SpellItem*>::Iterator it = m_spellList.begin();
         it != m_spellList.end(); it++)
      delete (*it);

   m_spellList.clear();
   m_timer->stop();
}

// this is just the public way of doing this
void SpellShell::deleteSpell(const SpellItem* item)
{
  deleteSpell((SpellItem*)item);
}

void SpellShell::deleteSpell(SpellItem *item)
{
   if (item)
   {
      if (m_lastPlayerSpell == item)
          m_lastPlayerSpell = 0;
      m_spellList.removeAt(m_spellList.indexOf(item));
      if (m_spellList.count() == 0)
         m_timer->stop();
      emit delSpell(item);
      delete item;
   }
}

// slots

void SpellShell::selfStartSpellCast(const uint8_t* data)
{
  const startCastStruct *c = (const startCastStruct *)data;
#ifdef DIAG_SPELLSHELL
  seqDebug("selfStartSpellCast - id=%d (slot=%d, inv=%d) on spawnid=%d", 
	   c->spellId, c->slot, c->inventorySlot, c->targetId);
#endif // DIAG_SPELLSHELL

  // get the target 
  const Item* s;
  QString targetName;
  int duration = 0;
  const Spell* spell = m_spells->spell(c->spellId);
  SpellItem *item;
  if (spell)
    duration = spell->calcDuration(m_player->level()) * 6;

  if (!spell || spell->targetType() != 6)
  {
    if (c->targetId && 
	((s = m_spawnShell->findID(tSpawn, c->targetId))))
      targetName = s->name();
    
    item = findSpell(c->spellId, c->targetId, targetName);
  }
  else
  {
    targetName = m_player->name();
    item = findSpell(c->spellId);
  }

  if (item) 
  { // exists
    item->update(c->spellId, spell, duration,
		 m_player->id(), m_player->name(),
		 c->targetId, targetName);
    emit changeSpell(item);
  } 
  else 
  { // new spell
    item = new SpellItem();
    item->update(c->spellId, spell, duration,
		 m_player->id(), m_player->name(),
		 c->targetId, targetName);
    m_spellList.append(item);
    if ((m_spellList.count() > 0) && (!m_timer->isActive()))
      m_timer->start(1000 *
		     pSEQPrefs->getPrefInt("SpellTimer", "SpellList", 6));
    emit addSpell(item);
    m_lastPlayerSpell = item;
  }
}

//slot for loading buffs when main char struct is loaded
void SpellShell::buffLoad(const spellBuff* c)
{
#ifdef DIAG_SPELLSHELL
  seqDebug("Loading buff - id=%d.",c->spellid);
#endif // DIAG_SPELLSHELL

  const Spell* spell = m_spells->spell(c->spellid);
  int duration = c->duration * 6;
  SpellItem *item = findSpell(c->spellid, m_player->id(), m_player->name());
  if (item) 
  { // exists
    item->update(c->spellid, spell, duration, 
		 0, "Buff", m_player->id(), m_player->name());
    emit changeSpell(item);
  } 
  else 
  { // new spell
    item = new SpellItem();
    item->update(c->spellid, spell, duration, 
		 0, "Buff", m_player->id(), m_player->name());
    m_spellList.append(item);
    if ((m_spellList.count() > 0) && (!m_timer->isActive()))
      m_timer->start(1000 *
		     pSEQPrefs->getPrefInt("SpellTimer", "SpellList", 6));
    emit addSpell(item);
  }
}

void SpellShell::buff(const uint8_t* data, size_t size, uint8_t dir)
{
  // Server-side broadcasts only — the client never sends OP_Buff with
  // useful state, and DIR_Client fires are echo/noise.
  if (dir == DIR_Client)
    return;

  // Post-2026-05-22 OP_Buff (opcode 0x18b4) is variable-size; the legacy
  // 168-byte buffStruct is dead. Wire forms cracked from captures
  // (buffquery-fresh2.vpk, 9 fires):
  //
  //   Header (all forms): u32 spawnID, u32 spellID
  //   13b "fade": u16 0x0001, u8 0, u8 slot, u8 0
  //   30b "slot occupied, no metadata" (OP_BuffQuery initial sync):
  //     {u8 0, u8 slot, u8 0, u8 1, u8[3] 0, u32 0xffffffff,
  //      u8[9] 0, u16 0x0001}
  //   34/55/76b "live update": {u8 1, u8 block_count, u8[5] 0} followed
  //     by N blocks of {u32 dur_ticks, u32 init_ticks, u32 pad,
  //     char caster_name[]\0} separated by a 4-byte inter-block sep
  //     and a trailing u16 0x0001 terminator. Block 1's duration is
  //     the buff's remaining time; subsequent blocks are sub-effect
  //     stacks the panel doesn't surface.
  //
  // BuffsPanel only renders the local player's buffs, so any other
  // spawnID (groupmate or random nearby player) is broadcast noise.
  // Filter early instead of letting m_spellList grow into a zone-wide
  // buff dump.
  if (size < 8) return;

  const uint32_t spawnId =
      uint32_t(data[0]) | (uint32_t(data[1]) << 8) |
      (uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 24);
  const uint32_t spellId =
      uint32_t(data[4]) | (uint32_t(data[5]) << 8) |
      (uint32_t(data[6]) << 16) | (uint32_t(data[7]) << 24);

  if (m_player->id() != 0 && spawnId != m_player->id())
    return;

  // 0xffffffff is a legacy no-op marker that occasionally appears as a
  // spellID in 13b forms — drop silently.
  if (spellId == 0xffffffff || spellId == 0)
    return;

  const Spell* spell = m_spells->spell(spellId);
  SpellItem* item = findSpell(spellId, spawnId, QString());

  // 13-byte fade: clear the buff if we're tracking it.
  if (size == 13) {
    if (item) deleteSpell(item);
    return;
  }

  // 30-byte initial-sync form has no on-wire duration — fall back to the
  // spell DB's level-scaled value (matches action() and the legacy buff
  // load path). If the DB doesn't know the spell we can't synthesize a
  // sensible duration; skip rather than emit a 0-second flicker.
  int duration = 0;
  uint8_t slot = 0xff;
  if (size == 30) {
    slot = data[9];
    if (spell) duration = spell->calcDuration(m_player->level()) * 6;
    if (duration <= 0) return;
  } else if (size >= 34) {
    // Block 1 starts at offset 15 (after the 15-byte header). Block 1's
    // duration tick count is the player's remaining time.
    if (size < 15 + 4) return;
    const uint32_t durTicks =
        uint32_t(data[15]) | (uint32_t(data[16]) << 8) |
        (uint32_t(data[17]) << 16) | (uint32_t(data[18]) << 24);
    duration = int(durTicks) * 6;
    if (duration <= 0) return;
  } else {
    // Unrecognized size — defer to a future capture rather than guessing.
#ifdef DIAG_SPELLSHELL
    seqDebug("OP_Buff: unrecognized size %zu for spell %d", size, spellId);
#endif
    return;
  }

#ifdef DIAG_SPELLSHELL
  seqDebug("OP_Buff %zub - spell=%d spawn=%d dur=%ds", size, spellId, spawnId, duration);
#endif

  if (item) {
    item->setDuration(duration);
    if (slot != 0xff) item->setBuffSlot(slot);
    emit changeSpell(item);
  } else {
    item = new SpellItem();
    item->update(spellId, spell, duration,
                 0, QString(), spawnId, QString());
    if (slot != 0xff) item->setBuffSlot(slot);
    m_spellList.append(item);
    if (!m_timer->isActive())
      m_timer->start(1000 *
                     pSEQPrefs->getPrefInt("SpellTimer", "SpellList", 6));
    emit addSpell(item);
  }
}

void SpellShell::action(const uint8_t* data, size_t len, uint8_t)
{
  const actionStruct* a = (const actionStruct*)data;

  if (a->type != 0xe7) // only things to do if action is a spell
    return;

  const Item* s;
  QString targetName;

  if (a->target && 
      ((s = m_spawnShell->findID(tSpawn, a->target))))
    targetName = s->name();

  SpellItem *item = findSpell(a->spell, a->target, targetName);

  if (item || (a->target == m_player->id()))
  {
    int duration = 0;
    const Spell* spell = m_spells->spell(a->spell);
    if (spell)
      duration = spell->calcDuration(a->level) * 6;
    
    QString casterName;
    if (a->source && 
	((s = m_spawnShell->findID(tSpawn, a->source))))
      casterName = s->name();

    if (item)
    {
#ifdef DIAG_SPELLSHELL
      seqDebug("action - found - source=%d (lvl: %d) cast id=%d on target=%d causing %d damage", 
	       a->source, a->level, a->spell, a->target, a->damage);
#endif // DIAG_SPELLSHELL
      
      item->update(a->spell, spell, duration, 
		   a->source, casterName, a->target, targetName);
      emit changeSpell(item);
    }
    else
    {
      // otherwise check for spells cast on us
#ifdef DIAG_SPELLSHELL
      seqDebug("action - new - source=%d (lvl: %d) cast id=%d on target=%d causing %d damage", 
	       a->source, a->level, a->spell, a->target, a->damage);
#endif // DIAG_SPELLSHELL
      
      // only way to get here is if there wasn't an existing spell, so...
      item = new SpellItem();
      item->update(a->spell, spell, duration, 
		   a->source, casterName, a->target, targetName);
      m_spellList.append(item);
      if ((m_spellList.count() > 0) && (!m_timer->isActive()))
	m_timer->start(1000 *
		       pSEQPrefs->getPrefInt("SpellTimer", "SpellList", 6));
      emit addSpell(item);
    }    
  }
}

void SpellShell::simpleMessage(const uint8_t* data, size_t, uint8_t)
{
  // if no spell cast by the player recently, then nothing to do.
  if (!m_lastPlayerSpell)
    return;

  const simpleMessageStruct* smsg = (const simpleMessageStruct*)data;
  switch(smsg->messageFormat)
  {
  case 191: // Your target has no mana to affect
  case 239: // Your target cannot be mesmerized.
  case 240: // Your target cannot be mesmerized (with this spell).
  case 242: // Your target is immune to changes in its attack speed.
  case 243: // Your target is immune to fear spells.
  case 244: // Your target is immune to changes in its run speed.
  case 245: // You are unable to change form here.
  case 248: // Your target is too high of a level for your charm spell.
  case 251: // That spell can not affect this target NPC.
  case 253: // This pet may not be made invisible.
  case 255: // You do not have a pet.
  case 263: // Your spell did not take hold.
  case 264: // Your target has resisted your attempt to mesmerize it.
  case 268: // Your target looks unaffected.
  case 269: // Stick to singing until you learn to play this instrument.
  case 271: // Your spell would not have taken hold on your target.
  case 272: // You are missing some required spell components.
  case 439: // Your spell is interrupted.
  case 3285: // Your target is too powerful to be Castigated in this manner.
  case 9035: // Your target is too high of a level for your fear spell.
  case 9036: // This spell only works in the Planes of Power.
    // delete the last player spell
    deleteSpell(m_lastPlayerSpell);
    m_lastPlayerSpell = 0;
    break;
  default:
    break;
  }
}


void SpellShell::spellMessage(QString &str)
{
   QString spell = str.right(str.length() - 7); // drop 'Spell: '
   bool b = false;
   // Your xxx has worn off.
   // Your target resisted the xxx spell.
   // Your spell fizzles.
   seqInfo("*** spellMessage *** %s", spell.toLatin1().data());
   if (spell.left(25) == QString("Your target resisted the ")) {
      spell = spell.right(spell.length() - 25);
      spell = spell.left(spell.length() - 7);
      seqInfo("RESIST: '%s'", spell.toLatin1().data());
      b = true;
   } else if (spell.right(20) == QString(" spell has worn off.")) {
      spell = spell.right(spell.length() - 5);
      spell = spell.left(spell.length() - 20);
      seqInfo("WORE OFF: '%s'", spell.toLatin1().data());
      b = true;
   }

   if (b) {
      // Can't really tell which spell/target, so just delete the last one
      for(QList<SpellItem*>::Iterator it = m_spellList.begin();
         it != m_spellList.end(); it++) {
         if ((*it)->spellName() == spell) {
            (*it)->setDuration(0);
            break;
         }
      }
   }
}

void SpellShell::zoneChanged(void)
{
  m_lastPlayerSpell = 0;
  SpellItem* spell;
  for(QList<SpellItem*>::Iterator it = m_spellList.begin();
      it != m_spellList.end(); it++) 
  {
    spell = *it;

    // clear all the invalidated spawn ids
    spell->setTargetId(0);
    spell->setCasterId(0);
  }
}

void SpellShell::killSpawn(const Item* deceased)
{
    uint16_t id = deceased->id();

    if (id == m_player->id())
    {
        // We're dead. No more buffs for us.
        clear();
    }
    else
    {
        SpellItem* spell;

        if (m_lastPlayerSpell && (m_lastPlayerSpell->targetId() == id))
        {
            m_lastPlayerSpell = 0;
        }

        QList<SpellItem*>::Iterator it = m_spellList.begin();
        while(it != m_spellList.end())
        {
            spell = *it;
            if (spell->targetId() == id)
            {
                it = m_spellList.erase(it);
                emit delSpell(spell);
                delete spell;
            }
            else
            {
                ++it;
            }
        }

        if (m_spellList.count() == 0)
        {
            m_timer->stop();
        }
    }

}

void SpellShell::timeout()
{
  SpellItem* spell;

  QList<SpellItem*>::Iterator it = m_spellList.begin();
  while (it != m_spellList.end())
  {
    spell = *it;

    int d = spell->duration() -
      pSEQPrefs->getPrefInt("SpellTimer", "SpellList", 6);
    if (d > -6)
    {
      spell->setDuration(d);
      emit changeSpell(spell);
      it++;
    }
    else
    {
      seqInfo("SpellItem '%s' finished.", (*it)->spellName().toLatin1().data());
      if (m_lastPlayerSpell == spell)
          m_lastPlayerSpell = 0;
      it = m_spellList.erase(it);
      emit delSpell(spell);
      delete spell;
    }
   }

  if (m_spellList.count() == 0)
    m_timer->stop();
}
