/*
 *  zonemgr.h
 *  Copyright 2001,2007 Zaphod (dohpaz@users.sourceforge.net). All Rights Reserved.
 *  Copyright 2002-2012, 2012-2019 by the respective ShowEQ Developers
 *  Portions Copyright 2003 Fee (fee@users.sourceforge.net)
 *
 *  Contributed to ShowEQ by Zaphod (dohpaz@users.sourceforge.net)
 *  for use under the terms of the GNU General Public License,
 *  incorporated herein by reference.
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

#include "zonemgr.h"
#include "packet.h"
#include "main.h"
#include "everquest.h"
#include "diagnosticmessages.h"
#include "netstream.h"

#include <QFile>
#include <QDataStream>
#include <QRegularExpression>

//----------------------------------------------------------------------
// constants
static const char magicStr[5] = "zon2"; // magic is the size of uint32_t + a null
static const uint32_t* magic = (uint32_t*)magicStr;
const float defaultZoneExperienceMultiplier = 0.75;

// Sequence of signals on initial entry into eq from character select screen
// EQPacket                              ZoneMgr                       isZoning
// ----------                            -------                       --------
// zoneEntry(ClientZoneEntryStruct)      zoneBegin()                   true
// PlayerProfile(charProfileStruct)      zoneBegin(shortName)          false
// zoneNew(newZoneStruct)                zoneEnd(shortName, longName)  false
//
// Sequence of signals on when zoning from zone A to zone B
// EQPacket                              ZoneMgr                       isZoning
// ----------                            -------                       --------
// zoneChange(zoneChangeStruct, client)                                true
// zoneChange(zoneChangeStruct, server)  zoneChanged(shortName)        true
// zoneEntry(ClientZoneEntryStruct)      zoneBegin()                   false
// PlayerProfile(charProfileStruct)      zoneBegin(shortName)          false
// zoneNew(newZoneStruct)                zoneEnd(shortName, longName)  false
//
ZoneMgr::ZoneMgr(QObject* parent, const char* name)
  : QObject(parent),
    m_zoning(false),
    m_zone_exp_multiplier(defaultZoneExperienceMultiplier),
    m_zonePointCount(0),
    m_zonePoints(0)
{
  setObjectName(name);
  m_shortZoneName = "unknown";
  m_longZoneName = "unknown";
  m_zoning = false;
  m_dzID = 0;

  if (showeq_params->restoreZoneState)
    restoreZoneState();
}

ZoneMgr::~ZoneMgr()
{
  if (m_zonePoints)
    delete [] m_zonePoints;
}

struct ZoneNames
{
  const char* shortName;
  const char* longName;
};

static const ZoneNames zoneNames[] =
{
#include "zones.h"
};

QString ZoneMgr::zoneNameFromID(uint16_t zoneId)
{
   const char* zoneName = NULL;
   if ((zoneId & 0x0fff) < (sizeof(zoneNames) / sizeof(ZoneNames)))
       zoneName = zoneNames[zoneId & 0x0fff].shortName;

   if (zoneName != NULL)
      return zoneName;

   seqDebug("ZoneMgr::zoneNameFromID: zone name not found: zoneId=%d", zoneId);
   QString tmpStr;
   tmpStr = QString::asprintf("unk_zone_%d", zoneId);
   return tmpStr;
}

QString ZoneMgr::zoneLongNameFromID(uint16_t zoneId)
{
   const char* zoneName = NULL;
   if ((zoneId & 0x0fff) < (sizeof(zoneNames) / sizeof(ZoneNames)))
       zoneName = zoneNames[zoneId & 0x0fff].longName;

   if (zoneName != NULL)
      return zoneName;

   seqDebug("ZoneMgr::zoneLongNameFromID: zone name not found: zoneId=%d", zoneId);
   QString tmpStr;
   tmpStr = QString::asprintf("unk_zone_%d", zoneId);
   return tmpStr;
}

const zonePointStruct* ZoneMgr::zonePoint(uint32_t zoneTrigger)
{
  if (!m_zonePoints)
    return 0;

  for (size_t i = 0; i < m_zonePointCount; i++)
    if (m_zonePoints[i].zoneTrigger == zoneTrigger)
      return &m_zonePoints[i];

  return 0;
}

void ZoneMgr::saveZoneState(void)
{
  QFile keyFile(showeq_params->saveRestoreBaseFilename + "Zone.dat");
  if (keyFile.open(QIODevice::WriteOnly))
  {
    QDataStream d(&keyFile);
    // write the magic string
    d << *magic;

    d << m_longZoneName;
    d << m_shortZoneName;
  }
}

void ZoneMgr::restoreZoneState(void)
{
  QString fileName = showeq_params->saveRestoreBaseFilename + "Zone.dat";
  QFile keyFile(fileName);
  if (keyFile.open(QIODevice::ReadOnly))
  {
    QDataStream d(&keyFile);

    // check the magic string
    uint32_t magicTest;
    d >> magicTest;

    if (magicTest != *magic)
    {
      seqWarn("Failure loading %s: Bad magic string!",
              fileName.toLatin1().data());
      return;
    }

    d >> m_longZoneName;
    d >> m_shortZoneName;

    seqInfo("Restored Zone: %s (%s)!",
            m_shortZoneName.toLatin1().data(),
            m_longZoneName.toLatin1().data());
  }
  else
  {
    seqWarn("Failure loading %s: Unable to open!",
            fileName.toLatin1().data());
  }
}

void ZoneMgr::zoneEntryClient(const uint8_t* data, size_t len, uint8_t dir)
{
  const ClientZoneEntryStruct* zsentry = (const ClientZoneEntryStruct*)data;

  m_shortZoneName = "unknown";
  m_longZoneName = "unknown";
  m_zone_exp_multiplier = defaultZoneExperienceMultiplier;
  m_zoning = false;

  emit zoneBegin();
  emit zoneBegin(zsentry, len, dir);

  if (showeq_params->saveZoneState)
    saveZoneState();
}

int32_t ZoneMgr::fillProfileStruct(charProfileStruct *player, const uint8_t *data, size_t len, bool checkLen)
{
  /*
  This reads data from the variable-length charPlayerProfile struct
  */
  NetStream netStream(data, len);
  int32_t retVal;
  QString name;

  player->checksum = netStream.readUInt32NC();

  // Unknown  
  netStream.skipBytes(16);
  
  player->profile.gender = netStream.readUInt8();
  // Note: readUInt32() is big-endian (network order); readUInt32NC() is
  // little-endian (EQ wire format). race + class_ used the wrong reader,
  // so class=2 (cleric) read as 0x02000000 then truncated to uint8_t=0
  // in setClassVal. m_class would be re-set later from spawnStruct via
  // Spawn::update, but calcMaxMana(...,m_class=0,...) inside loadProfile
  // had already returned 0 → m_maxMana stuck at 0 → mana bar showed "—".
  player->profile.race = netStream.readUInt32NC();
  player->profile.class_ = netStream.readUInt32NC();
  player->profile.level = netStream.readUInt8();
  player->profile.level1 = netStream.readUInt8();

  // Really, everything after the level is not critical for operation.  If 
  // needed, skip the rest to get up and running quickly after patch day.

  // Bind points (5 ints)
  int bindCount = netStream.readUInt32NC();
  for (int i = 0; i < bindCount; i++) {
    memcpy(&player->profile.binds[i], netStream.pos(), sizeof(player->profile.binds[i]));
    netStream.skipBytes(sizeof(player->profile.binds[i]));
  }

  player->profile.deity = netStream.readUInt32NC();
  player->profile.intoxication = netStream.readUInt32NC();

  // Spell slot refresh (10 ints)
  int spellRefreshCount = netStream.readUInt32NC();
  for (int i = 0; i < spellRefreshCount; i++) {
    player->profile.spellSlotRefresh[i] = netStream.readUInt32NC();
  }

  // Equipment (22 ints)
  int equipCount = netStream.readUInt32NC();
  for (int i = 0; i < equipCount; i++) {
    memcpy(&player->profile.equipment[i], netStream.pos(), sizeof(player->profile.equipment[i]));
    netStream.skipBytes(sizeof(player->profile.equipment[i]));
  }
   
  // Something (9 ints)
  int sCount = netStream.readUInt32NC();
  for (int i = 0; i < sCount; i++) {	
    netStream.skipBytes(20);
  }

  // Something (9 ints)
  int sCount1 = netStream.readUInt32NC();
  for (int i = 0; i < sCount1; i++) {
    netStream.skipBytes(4);
  }

  // Something (9 ints)
  int sCount2 = netStream.readUInt32NC();
  for (int i = 0; i < sCount2; i++) {
    netStream.skipBytes(4);
  }

  // Looks like face, haircolor, beardcolor, eyes, etc. Skipping over it.
  netStream.skipBytes(51);

  player->profile.points = netStream.readUInt32NC();
  player->profile.MANA = netStream.readUInt32NC();
  player->profile.curHp = netStream.readUInt32NC();
  player->profile.STR = netStream.readUInt32NC();
  player->profile.STA = netStream.readUInt32NC();
  player->profile.CHA = netStream.readUInt32NC();
  player->profile.DEX = netStream.readUInt32NC();
  player->profile.INT = netStream.readUInt32NC();
  player->profile.AGI = netStream.readUInt32NC();
  player->profile.WIS = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(28);

  // AAs (300 ints)
  int aaCount = netStream.readUInt32NC();
  for (int i = 0; i < aaCount; i++) {
    player->profile.aa_array[i].AA = netStream.readUInt32NC();
    player->profile.aa_array[i].value = netStream.readUInt32NC();
    player->profile.aa_array[i].unknown008 = netStream.readUInt32NC();
  }

  // Number of SKills (100 ints)
  int skills = netStream.readUInt32NC();
  for (int i = 0; i < skills; i++) {
    netStream.skipBytes(4);
  }

  // Something (25 ints)
  int sCount3 = netStream.readUInt32NC();
  for (int i = 0; i < sCount3; i++) {
    netStream.skipBytes(4);
  }

  // Disciplines (300 ints)
  int disciplineCount = netStream.readUInt32NC();
  for (int i = 0; i < disciplineCount; i++) {
    player->profile.disciplines[i] = netStream.readUInt32NC();
  }

  // Something (25 ints)
  int sCount4 = netStream.readUInt32NC();
  for (int i = 0; i < sCount4; i++) {
    netStream.skipBytes(4);
  }

  // Unknown
  netStream.skipBytes (4);

  // Recast Timers (25 ints)
  int recastTypes = netStream.readUInt32NC();
  for (int i = 0; i < recastTypes; i++) {
    player->profile.recastTimers[i] = netStream.readUInt32NC();
  }

  // Something (100 ints)
  int sCount5 = netStream.readUInt32NC();
  for (int i = 0; i < sCount5; i++) {
    netStream.skipBytes(4);
  }

  // Spellbook (960 ints)
  int spellBookSlots = netStream.readUInt32NC();
  for (int i = 0; i < spellBookSlots; i++) {
    player->profile.sSpellBook[i] = netStream.readInt32();
  }

  // Mem Spell Slots (18 ints)
  int spellMemSlots = netStream.readUInt32NC();
  for (int i = 0; i < spellMemSlots; i++) {
    player->profile.sMemSpells[i] = netStream.readInt32();
  }

  // Spell Slot Refresh Timers (15 ints)
  int spellSlotRefreshTimer = netStream.readUInt32NC();
  for (int i = 0; i < spellSlotRefreshTimer; i++) {
    player->profile.spellSlotRefresh[i] = netStream.readInt32();
  }

  // Unknown
  netStream.skipBytes(1);

  // Buff Count (42 ints)
  int buffCount = netStream.readUInt32NC();
  for (int i = 0; i < buffCount; i++) {
    memcpy(&player->profile.buffs[i], netStream.pos(), sizeof(player->profile.buffs[i]));
    netStream.skipBytes(sizeof(player->profile.buffs[i]));
  }

  player->profile.platinum = netStream.readUInt32NC();
  player->profile.gold = netStream.readUInt32NC();
  player->profile.silver = netStream.readUInt32NC();
  player->profile.copper = netStream.readUInt32NC();

  player->profile.platinum_cursor = netStream.readUInt32NC();
  player->profile.gold_cursor = netStream.readUInt32NC();
  player->profile.silver_cursor = netStream.readUInt32NC();
  player->profile.copper_cursor = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(20);

  player->profile.aa_spent = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(4);

  player->profile.aa_assigned = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(20);

  player->profile.aa_unspent = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(2);

  //Bandolier (20 ints)
  int bandolierCount = netStream.readUInt32NC();
  for (int i = 0; i < bandolierCount; i++) {
    name = netStream.readText();
    if(name.length()) {
      strncpy(player->profile.bandoliers[i].bandolierName, name.toLatin1().data(), 32);
    }

    // Mainhand
    name = netStream.readText();
    if(name.length()) {
      strncpy(player->profile.bandoliers[i].mainHand.itemName, name.toLatin1().data(), 64);
    }
    player->profile.bandoliers[i].mainHand.itemId = netStream.readUInt32NC();
    player->profile.bandoliers[i].mainHand.icon = netStream.readUInt32NC();

    // Offhand
    name = netStream.readText();
    if(name.length()) {
      strncpy(player->profile.bandoliers[i].offHand.itemName, name.toLatin1().data(), 64);
    }
    player->profile.bandoliers[i].offHand.itemId = netStream.readUInt32NC();
    player->profile.bandoliers[i].offHand.icon = netStream.readUInt32NC();

    // Range
    name = netStream.readText();
    if(name.length()) {
      strncpy(player->profile.bandoliers[i].range.itemName, name.toLatin1().data(), 64);
    }
    player->profile.bandoliers[i].range.itemId = netStream.readUInt32NC();
    player->profile.bandoliers[i].range.icon = netStream.readUInt32NC();

    // Ammo
    name = netStream.readText();
    if(name.length()) {
      strncpy(player->profile.bandoliers[i].ammo.itemName, name.toLatin1().data(), 64);
    }
    player->profile.bandoliers[i].ammo.itemId = netStream.readUInt32NC();
    player->profile.bandoliers[i].ammo.icon = netStream.readUInt32NC();
  }

  // Unknown
  netStream.skipBytes(80);

  player->profile.endurance = netStream.readUInt32NC();

  // Wire layout (live, 2026-05-02): the legacy 70-byte unknown gap
  // straddles the AA exp counter. expAA is a u32 LE on a 0..100000
  // scale (display % = value / 1000), confirmed against four
  // OP_PlayerProfile dumps from tests/replay/aa_progress.vpk:
  // 22846 / 22846 / 22846 / 40318 matching in-game 22.846% → 40.318%.
  netStream.skipBytes(58);
  player->expAA = netStream.readUInt32NC();
  netStream.skipBytes(8);

  // Name
  int firstName = netStream.readUInt32NC();
  memcpy(player->name, netStream.pos(), 64);
  netStream.skipBytes(firstName);

  // Lastname
  int lastName = netStream.readUInt32NC();
  memcpy(player->lastName, netStream.pos(), 32);
  netStream.skipBytes(lastName);

  player->birthdayTime = netStream.readUInt32NC();
  player->accountCreateDate = netStream.readUInt32NC();
  player->lastSaveTime = netStream.readUInt32NC();
  player->timePlayedMin = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(4);

  player->expansions = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(4);

  // MAX_KNOWN_LANGS (32 ints)
  int langCount = netStream.readUInt32NC();
  for (int i = 0; i < langCount; i++) {
    player->languages[i] = netStream.readUInt8();
  }

  player->zoneId = netStream.readUInt16NC();
  player->zoneInstance = netStream.readUInt16NC();

  memcpy(&player->y, netStream.pos(), sizeof(player->y));
  netStream.skipBytes(sizeof(player->y));

  memcpy(&player->x, netStream.pos(), sizeof(player->x));
  netStream.skipBytes(sizeof(player->x));

  memcpy(&player->z, netStream.pos(), sizeof(player->z));
  netStream.skipBytes(sizeof(player->z));

  memcpy(&player->heading, netStream.pos(), sizeof(player->heading));
  netStream.skipBytes(sizeof(player->heading));

  player->standState = netStream.readUInt16();
  player->anon = netStream.readUInt16();
  player->guildID = netStream.readUInt32NC();
  player->guildServerID = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(2);

  player->platinum_inventory = netStream.readUInt32NC();
  player->gold_inventory = netStream.readUInt32NC();
  player->silver_inventory = netStream.readUInt32NC();
  player->copper_inventory = netStream.readUInt32NC();
  player->platinum_bank = netStream.readUInt32NC();
  player->gold_bank = netStream.readUInt32NC();
  player->silver_bank = netStream.readUInt32NC();
  player->copper_bank = netStream.readUInt32NC();
  player->platinum_shared = netStream.readUInt32NC();

  // Something (134 ints)
  int sCount6 = netStream.readUInt32NC();
  for (int i = 0; i < sCount6; i++) {
    netStream.skipBytes(8);
  }

  // Unknown
  netStream.skipBytes(8);

  player->careerTribute = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(4);

  player->currentTribute = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(6);

  // Tributes (5 ints)
  int tributeCount = netStream.readUInt32NC();
  for (int i = 0; i < tributeCount; i++) {
    memcpy(&player->tributes[i], netStream.pos(), sizeof(player->tributes[i]));
    netStream.skipBytes(sizeof(player->tributes[i]));
  }

  // Something (10 ints)
  int sCount7 = netStream.readUInt32NC();
  for (int i = 0; i < sCount7; i++) {
    netStream.skipBytes(8);
  }

  // Unknown
  netStream.skipBytes(137);

  player->currentRadCrystals = netStream.readUInt32NC();
  player->careerRadCrystals = netStream.readUInt32NC();
  player->currentEbonCrystals = netStream.readUInt32NC();
  player->careerEbonCrystals = netStream.readUInt32NC();

  // Unknown
  netStream.skipBytes(91);

  player->autosplit = netStream.readUInt8();

  // Unknown
  netStream.skipBytes(57);


  player->ldon_guk_points = netStream.readUInt32NC();
  player->ldon_mir_points = netStream.readUInt32NC();
  player->ldon_mmc_points = netStream.readUInt32NC();
  player->ldon_ruj_points = netStream.readUInt32NC();
  player->ldon_tak_points = netStream.readUInt32NC();
  player->ldon_avail_points = netStream.readUInt32NC();

  // Below are the structs still not found in the new playerpacket

/*
  int innateSkillCount = netStream.readUInt32NC();
  for (int i = 0; i < innateSkillCount; i++) {
    player->profile.innateSkills[i] = netStream.readUInt32NC();
  }

  player->profile.toxicity = netStream.readUInt32NC();
  player->profile.thirst = netStream.readUInt32NC();
  player->profile.hunger = netStream.readUInt32NC();

  player->pvp = netStream.readUInt8();
  player->gm = netStream.readUInt8();
  player->guildstatus = netStream.readInt8();
  player->exp = netStream.readUInt32NC();

   // Unknown (41)
  int doubleIntCount = netStream.readUInt32NC();
  for (int i = 0; i < doubleIntCount; i++) {
    int something = netStream.readUInt32NC();
    int somethingElse = netStream.readUInt32NC();
  }

  // Unknown (64)
  int byteCount = netStream.readUInt32NC();
  for (int i = 0; i < byteCount; i++) {
    char something = netStream.readUInt8();
  }
*/

  retVal = netStream.pos() - netStream.data();
  if (checkLen && (int32_t)len != retVal)
  {
    seqDebug("SpawnShell::fillProfileStruct - expected length: %d, read: %d for player '%s'", len, retVal, player->name);
  }

  return retVal;
}


void ZoneMgr::zonePlayer(const uint8_t* data, size_t len)
{
  charProfileStruct *player = new charProfileStruct;

  memset(player,0,sizeof(charProfileStruct));

  fillProfileStruct(player,data,len,false); // don't bother checking the length since it's always going to not match up

  // fillProfileStruct's netstream parser handles most variable-length
  // sections correctly, but it explicitly skips the skills array (reads
  // its length prefix, then skipBytes through the values). Overlay
  // skills directly so Player::charProfile sees real values. Confirmed
  // 2026-05-02 across two captures (combat.vpk @ 23875 bytes; login.vpk
  // @ 37088 bytes): wire offset 4618 holds u32=100 (MAX_KNOWN_SKILLS
  // length prefix) and 4622..5021 holds uint32[100] skills.
  //
  // For reference (no overlay needed — the netstream parser populates
  // it correctly): wire 1014 holds u32=300 (MAX_AA prefix), 1018..4617
  // is AA_Array[300] (12 bytes each: aa_id, value, unk). On this opcode
  // every entry's `value` is 0 — only auto-granted / server-tracked AA
  // ids appear here. Purchased AA ranks (Origin, Veteran's Enhancement,
  // etc.) come over OP_SendAATable (still unresolved).
  //
  // Defensive: bail if the wire is too short.
  constexpr size_t kSkillsWireOffset = 4622;
  if (len >= kSkillsWireOffset + sizeof(player->profile.skills)) {
    memcpy(player->profile.skills, data + kSkillsWireOffset,
           sizeof(player->profile.skills));
  }

  m_shortZoneName = zoneNameFromID(player->zoneId);
  m_longZoneName = zoneLongNameFromID(player->zoneId);
  m_zone_exp_multiplier = defaultZoneExperienceMultiplier;
  m_zoning = false;

  emit zoneBegin(m_shortZoneName);
  emit playerProfile(player);

  if (showeq_params->saveZoneState)
    saveZoneState();
}

void ZoneMgr::zoneChange(const uint8_t* data, size_t len, uint8_t dir)
{
  const zoneChangeStruct* zoneChange = (const zoneChangeStruct*)data;
  m_shortZoneName = zoneNameFromID(zoneChange->zoneId);
  m_longZoneName = zoneLongNameFromID(zoneChange->zoneId);
  m_zone_exp_multiplier = defaultZoneExperienceMultiplier;
  m_zoning = true;

  if (dir == DIR_Server)
    emit zoneChanged(m_shortZoneName);
    emit zoneChanged(zoneChange, len, dir);

  if (showeq_params->saveZoneState)
    saveZoneState();
}

void ZoneMgr::zoneNew(const uint8_t* data, size_t len, uint8_t dir)
{
  newZoneStruct *zoneNew = new newZoneStruct;
  memset (zoneNew, 0, sizeof (newZoneStruct));
  NetStream netStream (data, len);

  QString shortName = netStream.readText ();
  if (shortName.length ())
    strcpy (zoneNew->shortName, shortName.toLatin1().data());

  QString longName = netStream.readText ();
  if (longName.length ())
    strcpy (zoneNew->longName, longName.toLatin1().data());

  netStream.skipBytes (2);

  QString zonefile = netStream.readText ();
  if (zonefile.length ())
    strcpy (zoneNew->zonefile, zonefile.toLatin1().data());

  netStream.skipBytes (90);

  union { uint32_t n; float f; } x;
  x.n = netStream.readUInt32NC();
  zoneNew->zone_exp_multiplier = x.f;

  netStream.skipBytes (28);

  x.n = netStream.readUInt32NC();
  zoneNew->safe_y = x.f;
  x.n = netStream.readUInt32NC();
  zoneNew->safe_x = x.f;
  x.n = netStream.readUInt32NC();
  zoneNew->safe_z = x.f;

  m_safePoint.setPoint(lrintf(zoneNew->safe_x), lrintf(zoneNew->safe_y),
		       lrintf(zoneNew->safe_z));
  m_zone_exp_multiplier = zoneNew->zone_exp_multiplier;

  // ZBNOTE: Apparently these come in with the localized names, which means we 
  //         may not wish to use them for zone short names.  
  //         An example of this is: shortZoneName 'ecommons' in German comes 
  //         in as 'OGemeinl'.  OK, now that we have figured out the zone id
  //         issue, we'll only use this short zone name if there isn't one or
  //         it is an unknown zone.
  if (m_shortZoneName.isEmpty() || m_shortZoneName.startsWith("unk"))
  {
    m_shortZoneName = zoneNew->shortName;

    // LDoN likes to append a _262 to the zonename. Get rid of it.
    QRegularExpression rx("_\\d+$");
    m_shortZoneName.replace( rx, "");

    // 2020-01-20 patch seems to have added _progress suffix to certain
    // zone names, presumably for the progression servers. This happens in
    // ToV DZs for sure, but there may be others.
    QRegularExpression rz("_progress$");
    m_shortZoneName.replace(rz, "");

    // some zones are getting a suffix of _int (particularly guild halls)
    // which causes failure to load maps.
    QRegularExpression ry("_int$");
    m_shortZoneName.replace(ry, "");

    //anniversary missions
    QRegularExpression rw("_errand$");
    m_shortZoneName.replace(rw, "");
  }

  m_longZoneName = zoneNew->longName;
  m_zoning = false;

#if 1 // ZBTEMP
  seqDebug("Welcome to lovely downtown '%s' with an experience multiplier of %f",
	 zoneNew->longName, zoneNew->zone_exp_multiplier);
  seqDebug("Safe Point (%f, %f, %f)", 
	 zoneNew->safe_x, zoneNew->safe_y, zoneNew->safe_z);
#endif // ZBTEMP
  
//   seqDebug("zoneNew: m_short(%s) m_long(%s)",
//      (const char*)m_shortZoneName,
//      (const char*)m_longZoneName);
  
  emit zoneEnd(m_shortZoneName, m_longZoneName);

  if (showeq_params->saveZoneState)
    saveZoneState();

  delete zoneNew;
}

void ZoneMgr::zonePoints(const uint8_t* data, size_t len, uint8_t)
{
  const zonePointsStruct* zp = (const zonePointsStruct*)data;
  // note the zone point count
  m_zonePointCount = zp->count;

  // delete the previous zone point set
  if (m_zonePoints)
    delete [] m_zonePoints;
  
  // allocate storage for zone points
  m_zonePoints = new zonePointStruct[m_zonePointCount];

  // copy the zone point information
  memcpy((void*)m_zonePoints, zp->zonePoints, 
	 sizeof(zonePointStruct) * m_zonePointCount);
}

void ZoneMgr::dynamicZonePoints(const uint8_t *data, size_t len, uint8_t)
{
   const dzSwitchInfo *dz = (const dzSwitchInfo*)data;

   if(len == sizeof(dzSwitchInfo))
   {
      m_dzPoint.setPoint(lrintf(dz->x), lrintf(dz->y), lrintf(dz->z));
      m_dzID = dz->zoneID;
      m_dzLongName = zoneLongNameFromID(m_dzID);
      if(dz->type != 1 && dz->type > 2 && dz->type <= 5)
         m_dzType = 0; // green
      else
         m_dzType = 1; // pink
   }
   else if(len == 8)
   {
      // we quit the expedition
      m_dzPoint.setPoint(0, 0, 0);
      m_dzID = 0;
      m_dzLongName = "";
   }
}

void ZoneMgr::dynamicZoneInfo(const uint8_t* data, size_t len, uint8_t)
{
   const dzInfo *dz = (const dzInfo*)data;

   if(!dz->newDZ)
   {
      m_dzPoint.setPoint(0, 0, 0);
      m_dzID = 0;
      m_dzLongName = "";
   }
}
