/*
 *  messageshell.cpp
 *  Copyright 2002-2003, 2007 Zaphod (dohpaz@users.sourceforge.net)
 *  Copyright 2005-2009, 2012, 2016, 2019 by the respective ShowEQ Developers
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

#include "messageshell.h"
#include "eqstr.h"
#include "messages.h"
#include "everquest.h"
#include "spells.h"
#include "zonemgr.h"
#include "spawnshell.h"
#include "player.h"
#include "packetcommon.h"
#include "filtermgr.h"
#include "util.h"
#include "netstream.h"

#include <QRegularExpression>

namespace {

// EQ wraps inline item references in chat / system text with the
// 0x12 (DC2) control byte: \x12<binary item header><item name>\x12.
// The header is uppercase-hex digits with a lot of zero padding;
// the name follows. showeq-c rendered these as raw bytes (its
// MessagesWindow doesn't strip them either) — you just rarely see
// a loot line in showeq-c chat. On the wire to a web client the
// binary is plain noise; strip it down to just the readable name.
QString stripEqItemLinks(const QString& in)
{
    if (!in.contains(QChar(0x12))) return in;
    QString out = in;
    // Primary pattern: hex prefix followed by an "Aa"-style start of
    // a real word. Captures the readable tail.
    static const QRegularExpression rx(
        QStringLiteral("\\x12[0-9A-F]+([A-Z][a-z][^\\x12]*)\\x12"));
    out.replace(rx, "\\1");
    // Fallback: any remaining \x12...\x12 pair (link without a clear
    // name boundary). Drop entirely so the wire payload is clean.
    static const QRegularExpression fallback(
        QStringLiteral("\\x12[^\\x12]*\\x12"));
    out.replace(fallback, QString());
    return out;
}

} // namespace

//----------------------------------------------------------------------
// MessageShell
MessageShell::MessageShell(Messages* messages, EQStr* eqStrings,
			   Spells* spells, ZoneMgr* zoneMgr, 
			   SpawnShell* spawnShell, Player* player, 
                           QObject* parent, const char* name)
  : QObject(parent),
    m_messages(messages),
    m_eqStrings(eqStrings),
    m_spells(spells),
    m_zoneMgr(zoneMgr),
    m_spawnShell(spawnShell),
    m_player(player)
{
    setObjectName(name);
}

void MessageShell::channelMessage(const uint8_t* data, size_t len, uint8_t dir)
{
// Added for 8/12/09 patch for serialized message packet ----------------------
   QString qTmp;
   NetStream netStream(data, len);

   channelMessageStruct *cmsg = new channelMessageStruct;
   memset(cmsg, 0, sizeof(channelMessageStruct));

   qTmp = netStream.readText(); // sender

   if(qTmp.length())
      strcpy(cmsg->sender, qTmp.toLatin1().data());

   qTmp = netStream.readText(); // target

   if(qTmp.length())
      strcpy(cmsg->target, qTmp.toLatin1().data());

   netStream.skipBytes(8); // Unknown

   cmsg->language = netStream.readUInt32NC(); // language

   cmsg->chanNum = netStream.readUInt32NC(); // channel

   netStream.readUInt32(); // unknown
   netStream.readUInt8(); // unknown

   cmsg->skillInLanguage = netStream.readUInt32NC(); // skill

   qTmp = netStream.readText(); // message
   if(qTmp.length())
      strcpy(cmsg->message, qTmp.toLatin1().data());

//-----------------------------------------------------------------------------

  // Tells and Group by us happen twice *shrug*. Ignore the client->server one.
  if (dir == DIR_Client &&
      (cmsg->chanNum == MT_Tell || cmsg->chanNum == MT_Group || cmsg->chanNum == MT_Guild ||
      cmsg->chanNum == MT_OOC || cmsg->chanNum == MT_Shout || cmsg->chanNum == MT_Auction ||
      cmsg->chanNum == MT_System || cmsg->chanNum == MT_Raid))
  {
    return;
  }

  // Emit the structured chatMessage signal so the websocket layer can
  // forward it as seq.v1.ChatMessage. Limit to player-to-player channels;
  // MT_System and other server-side noise stay confined to the formatted
  // addMessage() path below.
  switch (cmsg->chanNum) {
  case MT_Guild:
  case MT_Group:
  case MT_Shout:
  case MT_Auction:
  case MT_OOC:
  case MT_Tell:
  case MT_Say:
  case MT_Raid:
    emit chatMessage(static_cast<uint32_t>(cmsg->chanNum),
                     QString::fromLatin1(cmsg->sender),
                     QString::fromLatin1(cmsg->target),
                     stripEqItemLinks(QString::fromLatin1(cmsg->message)));
    break;
  default:
    break;
  }

  QString tempStr;

  bool target = false;
  if (cmsg->chanNum >= MT_Tell)
    target = true;

  if (cmsg->language)
  {
    if ((cmsg->target[0] != 0) && target)
    {
      tempStr = QString::asprintf( "'%s' -> '%s' - %s {%s}",
		       cmsg->sender,
		       cmsg->target,
		       cmsg->message,
		       language_name(cmsg->language).toLatin1().data()
		       );
    }
    else
    {
      tempStr = QString::asprintf( "'%s' - %s {%s}",
		       cmsg->sender,
		       cmsg->message,
		       language_name(cmsg->language).toLatin1().data()
		       );
    }
  }
  else // Don't show common, its obvious
  {
    if ((cmsg->target[0] != 0) && target)
    {
      tempStr = QString::asprintf( "'%s' -> '%s' - %s",
		       cmsg->sender,
		       cmsg->target,
		       cmsg->message
		       );
    }
    else
    {
      tempStr = QString::asprintf( "'%s' - %s",
		       cmsg->sender,
		       cmsg->message
		       );
    }
  }

  m_messages->addMessage((MessageType)cmsg->chanNum, tempStr);

  delete cmsg;
  cmsg = 0;
}

static MessageType chatColor2MessageType(ChatColor chatColor)
{
  MessageType messageType;

  // use the message color to differentiate between certain messages
  switch(chatColor)
  {
  case CC_User_Say:
  case CC_User_EchoSay:
    messageType = MT_Say;
    break;
  case CC_User_Tell:
  case CC_User_EchoTell:
    messageType = MT_Tell;
    break;
  case CC_User_Group:
  case CC_User_EchoGroup:
    messageType = MT_Group;
    break;
  case CC_User_Guild:
  case CC_User_EchoGuild:
    messageType = MT_Guild;
    break;
  case CC_User_OOC:
  case CC_User_EchoOOC:
    messageType = MT_OOC;
    break;
  case CC_User_Auction:
  case CC_User_EchoAuction:
    messageType = MT_Auction;
    break;
  case CC_User_Shout:
  case CC_User_EchoShout:
    messageType = MT_Shout;
    break;
  case CC_User_Emote:
  case CC_User_EchoEmote:
    messageType = MT_Emote;
    break;
  case CC_User_RaidSay:
    messageType = MT_Raid;
    break;
  case CC_User_Spells:
  case CC_User_SpellWornOff:
  case CC_User_OtherSpells:
  case CC_User_SpellFailure:
  case CC_User_SpellCrit:
    messageType = MT_Spell;
    break;
  case CC_User_MoneySplit:
    messageType = MT_Money;
    break;
  case CC_User_Random:
    messageType = MT_Random;
    break;
  default:
    messageType = MT_General;
    break;
  }
  
  return messageType;
}

void MessageShell::formattedMessage(const uint8_t* data, size_t len, uint8_t dir)
{
  // avoid client chatter and do nothing if not viewing channel messages
  if (dir == DIR_Client)
    return;

  const formattedMessageStruct* fmsg = (const formattedMessageStruct*)data;
  QString tempStr;

  size_t messagesLen = len - ((uint8_t*)&fmsg->messages[0] - (uint8_t*)fmsg);
  const MessageType mt = chatColor2MessageType(fmsg->messageColor);
  const QString text = stripEqItemLinks(
      m_eqStrings->formatMessage(fmsg->messageFormat,
                                 fmsg->messages,
                                 messagesLen));
  m_messages->addMessage(mt, text);
  // Forward to the websocket as a system-flavored chatMessage so the web
  // chat panel sees NPC speech, system warnings, exp ticks, etc.
  emit chatMessage(static_cast<uint32_t>(mt), QString(), QString(), text);
}

void MessageShell::simpleMessage(const uint8_t* data, size_t len, uint8_t dir)
{
  // avoid client chatter and do nothing if not viewing channel messages
  if (dir == DIR_Client)
    return;

  const simpleMessageStruct* smsg = (const simpleMessageStruct*)data;
  QString tempStr;

  const MessageType mt = chatColor2MessageType(smsg->messageColor);
  const QString text = stripEqItemLinks(m_eqStrings->message(smsg->messageFormat));
  m_messages->addMessage(mt, text);
  emit chatMessage(static_cast<uint32_t>(mt), QString(), QString(), text);
}

void MessageShell::specialMessage(const uint8_t* data, size_t, uint8_t dir)
{
  // avoid client chatter and do nothing if not viewing channel messages
  if (dir == DIR_Client)
    return;

  const specialMessageStruct* smsg = (const specialMessageStruct*)data;

  const Item* target = NULL;

  if (smsg->target)
    target = m_spawnShell->findID(tSpawn, smsg->target);

  // calculate the message position
  // const char* message = smsg->source + strlen(smsg->source) + 1
  //  + sizeof(smsg->unknown0xxx);
  // NOTE: gcc 8 (and maybe others) over-optimizes the above strlen call on the
  // variable-sized source array (possibly because it isn't the last member
  // of the struct), and as a result, strlen always returns 0 unless compiler
  // optimizations are disabled.  So we work around this by creating a QString
  // and using its size
  const char* message = smsg->source + QString(smsg->source).length() + 1
      + sizeof(smsg->unknown0xxx);

  const MessageType mt = chatColor2MessageType(smsg->messageColor);
  const QString sender = QString::fromLatin1(smsg->source);
  const QString targetName = target ? target->name() : QString();
  const QString text = stripEqItemLinks(QString::fromLatin1(message));

  if (target) {
    m_messages->addMessage(mt,
        QString("Special: '%1' -> '%2' - %3").arg(sender, targetName, text));
  } else {
    m_messages->addMessage(mt,
        QString("Special: '%1' - %2").arg(sender, text));
  }
  // Web chat panel keeps the structured fields (sender + target + text)
  // and renders however it likes; no string concatenation on the wire.
  emit chatMessage(static_cast<uint32_t>(mt), sender, targetName, text);
}

void MessageShell::guildMOTD(const uint8_t* data, size_t, uint8_t dir)
{
  // avoid client chatter and do nothing if not viewing channel messages
  if (dir == DIR_Client)
    return;

  const guildMOTDStruct* gmotd = (const guildMOTDStruct*)data;

  m_messages->addMessage(MT_Guild, 
			 QString("MOTD: %1 - %2")
			 .arg(QString::fromUtf8(gmotd->sender))
			 .arg(QString::fromUtf8(gmotd->message)));
}


void MessageShell::moneyOnCorpse(const uint8_t* data)
{
  const moneyOnCorpseStruct* money = (const moneyOnCorpseStruct*)data;

  QString tempStr;

  if( money->platinum || money->gold || money->silver || money->copper )
  {
    bool bneedComma = false;
    
    tempStr = "You receive ";
    
    if(money->platinum)
    {
      tempStr += QString::number(money->platinum) + " platinum";
      bneedComma = true;
    }
    
    if(money->gold)
    {
      if(bneedComma)
	tempStr += ", ";
      
      tempStr += QString::number(money->gold) + " gold";
      bneedComma = true;
    }
    
    if(money->silver)
    {
      if(bneedComma)
	tempStr += ", ";
      
      tempStr += QString::number(money->silver) + " silver";
      bneedComma = true;
    }
    
    if(money->copper)
      {
	if(bneedComma)
	  tempStr += ", ";
	
	tempStr += QString::number(money->copper) + " copper";
      }
    
    tempStr += " from the corpse";
    
    m_messages->addMessage(MT_Money, tempStr);
  }
}

void MessageShell::moneyUpdate(const uint8_t* data)
{
  //  const moneyUpdateStruct* money = (const moneyUpdateStruct*)data;
  m_messages->addMessage(MT_Money, "Update");
}

void MessageShell::moneyThing(const uint8_t* data)
{
  //  const moneyUpdateStruct* money = (const moneyUpdateStruct*)data;
  m_messages->addMessage(MT_Money, "Thing");
}

void MessageShell::randomRequest(const uint8_t* data)
{
  const randomReqStruct* randr = (const randomReqStruct*)data;
  QString tempStr;

  tempStr = QString::asprintf("Request random number between %d and %d",
		  randr->bottom,
		  randr->top);
  
  m_messages->addMessage(MT_Random, tempStr);
}

void MessageShell::random(const uint8_t* data)
{
  const randomStruct* randr = (const randomStruct*)data;
  QString tempStr;

  tempStr = QString::asprintf("Random number %d rolled between %d and %d by %s",
		  randr->result,
		  randr->bottom,
		  randr->top,
		  randr->name);
  
  m_messages->addMessage(MT_Random, tempStr);
}

void MessageShell::emoteText(const uint8_t* data)
{
  const emoteTextStruct* emotetext = (const emoteTextStruct*)data;
  QString tempStr;

  m_messages->addMessage(MT_Emote, emotetext->text);
}

void MessageShell::inspectData(const uint8_t* data)
{
  const inspectDataStruct *inspt = (const inspectDataStruct *)data;
  QString tempStr;

  for (int inp = 0; inp < 21; inp ++)
  {
    if (strnlen(inspt->itemNames[inp], 64) > 0)
    {
      tempStr = QString::asprintf("He has %s (icn:%i)", inspt->itemNames[inp], inspt->icons[inp]);
      m_messages->addMessage(MT_Inspect, tempStr);
    }
  }

  if (strnlen(inspt->mytext, 200) > 0)
  {
    tempStr = QString::asprintf("His info: %s", inspt->mytext);
    m_messages->addMessage(MT_Inspect, tempStr);
  }
}

void MessageShell::logOut(const uint8_t*, size_t, uint8_t)
{
  m_messages->addMessage(MT_Zone, "LogoutCode: Client logged out of server");
}

void MessageShell::zoneEntryClient(const ClientZoneEntryStruct* zsentry)
{
  m_messages->addMessage(MT_Zone, "EntryCode: Client");
}

void MessageShell::zoneChanged(const zoneChangeStruct* zoneChange, size_t, uint8_t dir)
{
  QString tempStr;

  if (dir == DIR_Client)
  {
    tempStr = "ChangeCode: Client, Zone: ";
    tempStr += m_zoneMgr->zoneNameFromID(zoneChange->zoneId);
  }
  else
  {
    tempStr = "ChangeCode: Server, Zone:";
    tempStr += m_zoneMgr->zoneNameFromID(zoneChange->zoneId);
  }
  
  m_messages->addMessage(MT_Zone, tempStr);
}

void MessageShell::zoneNew(const uint8_t* data, size_t len, uint8_t dir)
{
  NetStream netStream(data, len);
  QString newZoneShortName = netStream.readText ();
  QString newZoneLongName = netStream.readText ();
  QString tempStr;
  tempStr = "NewCode: Zone: ";
  tempStr += newZoneShortName + " (" + newZoneLongName + ")";
  m_messages->addMessage(MT_Zone, tempStr);
}

void MessageShell::zoneBegin(const QString& shortZoneName)
{
  QString tempStr;
  tempStr = QString("Zoning, Please Wait...\t(Zone: '")
    + shortZoneName + "')";
  m_messages->addMessage(MT_Zone, tempStr);
}

void MessageShell::zoneEnd(const QString& shortZoneName, 
			   const QString& longZoneName)
{
  QString tempStr;
  tempStr = QString("Entered: ShortName = '") + shortZoneName +
                    "' LongName = " + longZoneName;

  m_messages->addMessage(MT_Zone, tempStr);
}

void MessageShell::zoneChanged(const QString& shortZoneName)
{
  QString tempStr;
  tempStr = QString("Zoning, Please Wait...\t(Zone: '")
    + shortZoneName + "')";
  m_messages->addMessage(MT_Zone, tempStr);
}


void MessageShell::worldMOTD(const uint8_t* data)
{ 
  const worldMOTDStruct* motd = (const worldMOTDStruct*)data;
  m_messages->addMessage(MT_Motd, QString::fromUtf8(motd->message));
}

void MessageShell::syncDateTime(const QDateTime& dt)
{
  QString dateString = dt.toString(pSEQPrefs->getPrefString("DateTimeFormat", "Interface", "ddd MMM dd,yyyy - hh:mm ap"));

  m_messages->addMessage(MT_Time, dateString);
}

void MessageShell::handleSpell(const uint8_t* data, size_t, uint8_t dir)
{
  const memSpellStruct* mem = (const memSpellStruct*)data;
  QString tempStr;

  bool client = (dir == DIR_Client);

  tempStr = "";
  
  switch (mem->param1)
  {
  case 0:
    {
      if (!client)
	tempStr = "You have finished scribing '";
      break;
    }
    
  case 1:
    {
      if (!client)
	tempStr = "You finish casting '";
      break;
    }
    
  case 2:
    {
      if (!client)
	tempStr = "You have finished memorizing '";
      break;
    }
    
  case 3:
    {
      if (!client)
	tempStr = "You forget '";
      break;
    }

  case 4:
    {
      if (!client)
      break;
    }
    
  default:
    {
      tempStr = QString::asprintf( "Unknown Spell Event ( %s ) - '",
		       client  ?
		     "Client --> Server"   :
		       "Server --> Client"
		       );
      break;
    }
  }
  
  
  if (!tempStr.isEmpty())
  {
    QString spellName;
    const Spell* spell = m_spells->spell(mem->spellId);
    
    if (spell)
      spellName = spell->name();
    else
      spellName = spell_name(mem->spellId);

    if (mem->param1 != 4)
      tempStr = QString::asprintf("%s%s', slot %d.",
              tempStr.toLatin1().data(),
              spellName.toLatin1().data(),
              mem->slotId);

    else 
    {
    // Spell procs send memspell packet for spell at slot 15 which causes duplicate spell cast console messages
    // Commenting out code below and adding a case 4 above with no output removes the duplicate messages 
      /*tempStr.sprintf("%s%s'.", 
		      tempStr.ascii(), 
		      (const char*)spellName);
      */
    }

    m_messages->addMessage(MT_Spell, tempStr);
  }
}

void MessageShell::beginCast(const uint8_t* data)
{
  const beginCastStruct *bcast = (const beginCastStruct *)data;
  QString tempStr;

  tempStr = "";

  if (bcast->spawnId == m_player->id())
    tempStr = "You begin casting '";
  else
  {
    const Item* item = m_spawnShell->findID(tSpawn, bcast->spawnId);
    if (item != NULL)
      tempStr = item->name();
    
    if (tempStr == "" || tempStr.isEmpty())
      tempStr = QString::asprintf("UNKNOWN (ID: %d)", bcast->spawnId);
    
    tempStr += " has begun casting '";
  }
  float casttime = ((float)bcast->param1 / 1000);
  
  QString spellName;
  const Spell* spell = m_spells->spell(bcast->spellId);
  
  if (spell)
    spellName = spell->name();
  else
    spellName = spell_name(bcast->spellId);

  tempStr = QString::asprintf( "%s%s' - Casting time is %g Second%s",
          tempStr.toLatin1().data(),
          spellName.toLatin1().data(), casttime,
          casttime == 1 ? "" : "s");

  m_messages->addMessage(MT_Spell, tempStr);
}

void MessageShell::spellFaded(const uint8_t* data)
{
  const spellFadedStruct *sf = (const spellFadedStruct *)data;
  QString tempStr;

  if (strlen(sf->message) > 0)
  {
      tempStr = QString::asprintf( "Faded: %s", sf->message);

      m_messages->addMessage(MT_Spell, tempStr);
  }
}

void MessageShell::interruptSpellCast(const uint8_t* data)
{
  const badCastStruct *icast = (const badCastStruct *)data;
  const Item* item = m_spawnShell->findID(tSpawn, icast->spawnId);

  QString tempStr;
  if (item != NULL)
    tempStr = QString::asprintf("%s(%d): %s",
            item->name().toLatin1().data(), icast->spawnId, icast->message);
  else
    tempStr = QString::asprintf("spawn(%d): %s",
            icast->spawnId, icast->message);

  m_messages->addMessage(MT_Spell, tempStr);
}

void MessageShell::startCast(const uint8_t* data)
{
  const startCastStruct* cast = (const startCastStruct*)data;
  QString spellName;
  const Spell* spell = m_spells->spell(cast->spellId);
  
  if (spell)
    spellName = spell->name();
  else
    spellName = spell_name(cast->spellId);

  const Item* item = m_spawnShell->findID(tSpawn, cast->targetId);

  QString targetName;

  if (item != NULL)
    targetName = item->name();
  else
    targetName = "";

  QString tempStr;

  tempStr = QString::asprintf("You begin casting %s.  Current Target is %s(%d)",
          spellName.toLatin1().data(), targetName.toLatin1().data(),
          cast->targetId);

  m_messages->addMessage(MT_Spell, tempStr);
}

// 9/30/2008 - no longer used. Group info is sent differently now
void MessageShell::groupUpdate(const uint8_t* data, size_t size, uint8_t dir)
{
  if (size != sizeof(groupUpdateStruct))
  {
    // Ignore groupFullUpdateStruct
    return;
  }
  return;
  const groupUpdateStruct* gmem = (const groupUpdateStruct*)data;
  QString tempStr;

  switch (gmem->action)
  {
    case GUA_Joined :
      tempStr = QString::asprintf ("Update: %s has joined the group.", gmem->membername);
      break;
    case GUA_Left :
      tempStr = QString::asprintf ("Update: %s has left the group.", gmem->membername);
      break;
    case GUA_LastLeft :
      tempStr = QString::asprintf ("Update: The group has been disbanded when %s left.",
         gmem->membername);
      break;
    case GUA_MakeLeader : 
      tempStr = QString::asprintf ("Update: %s is now the leader of the group.", 
         gmem->membername);
      break;
    case GUA_Started :
      tempStr = QString::asprintf ("Update: %s has formed the group.", gmem->membername);
      break;
    default :
       tempStr = QString::asprintf ("Update: Unknown Update action:%d - %s - %s)", 
		   gmem->action, gmem->yourname, gmem->membername);
  }

  m_messages->addMessage(MT_Group, tempStr);
}

void MessageShell::groupInvite(const uint8_t* data, size_t len, uint8_t dir)
{
  const groupInviteStruct* gmem = (const groupInviteStruct*)data;
  QString tempStr;

  if(dir == DIR_Client)
     tempStr = QString::asprintf("Invite: You invite %s to join the group", gmem->invitee);
  else
     tempStr = QString::asprintf("Invite: %s invites %s to join the group", gmem->inviter, gmem->invitee);

  m_messages->addMessage(MT_Group, tempStr);
}

void MessageShell::groupDecline(const uint8_t* data)
{
  const groupDeclineStruct* gmem = (const groupDeclineStruct*)data;
  QString tempStr;
  switch(gmem->reason)
  {
     case 1:
        tempStr = QString::asprintf("Invite: %s declines invite from %s (player is grouped)", 
                        gmem->membername, gmem->yourname);
        break;
     case 3:
        tempStr = QString::asprintf("Invite: %s declines invite from %s", 
                        gmem->membername, gmem->yourname);
        break;
     default:
        tempStr = QString::asprintf("Invite: %s declines invite from %s (unknown reason: %i)", 
                        gmem->membername, gmem->yourname, gmem->reason);
        break;
  }
  m_messages->addMessage(MT_Group, tempStr);
}

void MessageShell::groupFollow(const uint8_t* data)
{
  const groupFollowStruct* gFollow = (const groupFollowStruct*)data;
  QString tempStr;

  if(!strcmp(gFollow->invitee, m_player->name().toLatin1().data()))
     tempStr = "Follow: You have joined the group";
  else
     tempStr = QString::asprintf("Follow: %s has joined the group", gFollow->invitee);
  m_messages->addMessage(MT_Group, tempStr);
}

void MessageShell::groupDisband(const uint8_t* data)
{
  const groupDisbandStruct* gmem = (const groupDisbandStruct*)data;
  QString tempStr;

  tempStr = QString::asprintf ("Disband: %s disbands from the group", gmem->membername);
  m_messages->addMessage(MT_Group, tempStr);
}

void MessageShell::groupLeaderChange(const uint8_t* data)
{
   const groupLeaderChangeStruct *gmem = (const groupLeaderChangeStruct*)data;
   QString tempStr;
   tempStr = QString::asprintf("Update: %s is now the leader of the group", 
                    gmem->membername);
   m_messages->addMessage(MT_Group, tempStr);
}

void MessageShell::player(const charProfileStruct* player)
{
  QString message;

  message = QString::asprintf("Name: '%s' Last: '%s'", 
		  player->name, player->lastName);
  m_messages->addMessage(MT_Player, message);

  message = QString::asprintf("Level: %d", player->profile.level);
  m_messages->addMessage(MT_Player, message);
  
  message = QString::asprintf("PlayerMoney: P=%d G=%d S=%d C=%d",
		 player->profile.platinum, player->profile.gold, 
		 player->profile.silver, player->profile.copper);
  m_messages->addMessage(MT_Player, message);
  
  message = QString::asprintf("BankMoney: P=%d G=%d S=%d C=%d",
		  player->platinum_bank, player->gold_bank, 
		  player->silver_bank, player->copper_bank);
  m_messages->addMessage(MT_Player, message);

  message = QString::asprintf("CursorMoney: P=%d G=%d S=%d C=%d",
		  player->profile.platinum_cursor, player->profile.gold_cursor, 
		  player->profile.silver_cursor, player->profile.copper_cursor);
  m_messages->addMessage(MT_Player, message);

  message = QString::asprintf("SharedMoney: P=%d",
		  player->platinum_shared);
  m_messages->addMessage(MT_Player, message);

  message = QString::asprintf("DoN Crystals: Radiant=%d Ebon=%d",
          player->currentRadCrystals, player->currentEbonCrystals);
  m_messages->addMessage(MT_Player, message);

// charProfileStruct.exp hasn't been found
//   message = "Exp: " + Commanate(player->exp);
//   m_messages->addMessage(MT_Player, message);

  message = "ExpAA: (spent: " + Commanate(player->profile.aa_spent) + 
      ", unspent: " + Commanate(player->profile.aa_unspent) + ")";
  m_messages->addMessage(MT_Player, message);

#if 0 
  // Format for the aa values used to 0-1000 for group, 0-2000 for raid,
  // but now it's different. Just drop it for now. %%%
  message = "GroupLeadAA: " + Commanate(player->expGroupLeadAA) + 
      " (unspent: " + Commanate(player->groupLeadAAUnspent) + ")";
  m_messages->addMessage(MT_Player, message);
  message = "RaidLeadAA: " + Commanate(player->expRaidLeadAA) + 
      " (unspent: " + Commanate(player->raidLeadAAUnspent) + ")";
  m_messages->addMessage(MT_Player, message);
#endif

// 09/03/2008 patch - this is no longer sent in charProfile
//   message.sprintf("Group: %s %s %s %s %s %s", player->groupMembers[0],
//     player->groupMembers[1],
//     player->groupMembers[2],
//     player->groupMembers[3],
//     player->groupMembers[4],
//     player->groupMembers[5]);
//   m_messages->addMessage(MT_Player, message);

  int buffnumber;
  QString spellName;

  for (buffnumber=0;buffnumber<MAX_BUFFS;buffnumber++)
  {
    if (player->profile.buffs[buffnumber].spellid && 
            player->profile.buffs[buffnumber].duration)
    {
      const Spell* spell = m_spells->spell(player->profile.buffs[buffnumber].spellid);
      if(spell)
         spellName = spell->name();
      else
         spellName = spell_name(player->profile.buffs[buffnumber].spellid);

      if(player->profile.buffs[buffnumber].duration == -1)
        message = QString::asprintf("You have buff %s (permanent).", spellName.toLatin1().data());
      else
        message = QString::asprintf("You have buff %s duration left is %d in ticks.",
                spellName.toLatin1().data(), player->profile.buffs[buffnumber].duration);

      m_messages->addMessage(MT_Player, message);
    }
  }

  message = "LDoN Earned Guk Points: " + Commanate(player->ldon_guk_points);
  m_messages->addMessage(MT_Player, message);
  message = "LDoN Earned Mira Points: " + Commanate(player->ldon_mir_points);
  m_messages->addMessage(MT_Player, message);
  message = "LDoN Earned MMC Points: " + Commanate(player->ldon_mmc_points);
  m_messages->addMessage(MT_Player, message);
  message = "LDoN Earned Ruj Points: " + Commanate(player->ldon_ruj_points);
  m_messages->addMessage(MT_Player, message);
  message = "LDoN Earned Tak Points: " + Commanate(player->ldon_tak_points);
  m_messages->addMessage(MT_Player, message);
  message = "LDoN Unspent Points: " + Commanate(player->ldon_avail_points);
  m_messages->addMessage(MT_Player, message);
}

void MessageShell::increaseSkill(const uint8_t* data)
{
  const skillIncStruct* skilli = (const skillIncStruct*)data;
  QString tempStr;
  tempStr = QString::asprintf("Skill: %s has increased (%d)",
          skill_name(skilli->skillId).toLatin1().data(),
          skilli->value);
  m_messages->addMessage(MT_Player, tempStr);
}

void MessageShell::updateLevel(const uint8_t* data)
{
  const levelUpUpdateStruct *levelup = (const levelUpUpdateStruct *)data;
  QString tempStr;
  tempStr = QString::asprintf("NewLevel: %d", levelup->level);
  m_messages->addMessage(MT_Player, tempStr);
}
  
void MessageShell::consent(const uint8_t* data, size_t, uint8_t dir)
{
  const consentResponseStruct* consent = (const consentResponseStruct*)data;

  m_messages->addMessage(MT_General, 
      QString("Consent: %1 %4 permission to drag %2's corpse in %3")
	  		 .arg(QString::fromUtf8(consent->consentee))
			 .arg(QString::fromUtf8(consent->consenter))
             .arg(QString::fromUtf8(consent->corpseZoneName))
             .arg((consent->allow == 1 ? "granted" : "denied")));
}


void MessageShell::consMessage(const uint8_t* data, size_t, uint8_t dir) 
{
  const considerStruct * con = (const considerStruct*)data;
  const Item* item;

  QString lvl("");
  QString hps("");
  QString cn("");
  QString deity;

  QString msg("Your faction standing with ");

  // is it you that you've conned?
  if (con->playerid == con->targetid) 
  {
    deity = m_player->deityName();
    
    // well, this is You
    msg += m_player->name();
  }
  else 
  {
    // find the spawn if it exists
    item = m_spawnShell->findID(tSpawn, con->targetid);
    
    // has the spawn been seen before?
    if (item != NULL)
    {
      Spawn* spawn = (Spawn*)item;

      // yes
      deity = spawn->deityName();

      lvl = QString::number(spawn->level());

      msg += item->name() + " (Lvl: " + lvl + ")";
    } // end if spawn found
    else
      msg += "Spawn:" + QString::number(con->targetid, 16);
  } // else not yourself
  
  switch (con->level) 
  {
  case 0:
  case 5:
  case 20:
    msg += " (even)";
    break;
  case 1:
    msg += " (grey)";
    break;
  case 2:
    msg += " (green)";
    break;
  case 4:
    msg += " (blue)";
    break;
  case 7:
  case 13:
    msg += " (red)";
    break;
  case 6:
  case 15:
    msg += " (yellow)";
    break;
  case 3:
  case 18:
    msg += " (cyan)";
    break;
  default:
    msg += " (unknown: " + QString::number(con->level) + ")";
    break;
  }

  if (!deity.isEmpty())
    msg += QString(" [") + deity + "]";

  msg += QString(" is: ") + print_faction(con->faction) + " (" 
    + QString::number(con->faction) + ")!";

  m_messages->addMessage(MT_Consider, msg);
} // end consMessage()


void MessageShell::setExp(uint32_t totalExp, uint32_t totalTick,
			  uint32_t minExpLevel, uint32_t maxExpLevel, 
			  uint32_t tickExpLevel)
{
    QString tempStr;
    tempStr = QString::asprintf("Exp: Set: %u total, with %u (%u/330) into level with %u left, where 1/330 = %u",
		    totalExp, (totalExp - minExpLevel), totalTick, 
		    (maxExpLevel - totalExp), tickExpLevel);
    m_messages->addMessage(MT_Player, tempStr);
}

void MessageShell::newExp(uint32_t newExp, uint32_t totalExp, 
			  uint32_t totalTick,
			  uint32_t minExpLevel, uint32_t maxExpLevel, 
			  uint32_t tickExpLevel)
{
  QString tempStr;
  uint32_t leftExp = maxExpLevel - totalExp;

  // only can display certain things if new experience is greater then 0,
  // ie. a > 1/330'th experience increment.
  if (newExp)
  {
    // calculate the number of this type of kill needed to level.
    uint32_t needKills = leftExp / newExp;

    tempStr = QString::asprintf("Exp: New: %u, %u (%u/330) into level with %u left [~%u kills]",
		    newExp, (totalExp - minExpLevel), totalTick, 
		    leftExp, needKills);
  }
  else
    tempStr = QString::asprintf("Exp: New: < %u, %u (%u/330) into level with %u left",
		    tickExpLevel, (totalExp - minExpLevel), totalTick, 
		    leftExp);
  
  m_messages->addMessage(MT_Player, tempStr);
}

void MessageShell::setAltExp(uint32_t totalExp,
			     uint32_t maxExp, uint32_t tickExp, 
			     uint32_t aaPoints)
{
  QString tempStr;
  tempStr = QString::asprintf("ExpAA: Set: %u total, with %u aapoints",
		  totalExp, aaPoints);

  m_messages->addMessage(MT_Player, tempStr);
}

void MessageShell::newAltExp(uint32_t newExp, uint32_t totalExp, 
			     uint32_t totalTick, 
			     uint32_t maxExp, uint32_t tickExp, 
			     uint32_t aapoints)
{
  QString tempStr;
  
  // only can display certain things if new experience is greater then 0,
  // ie. a > 1/330'th experience increment.
  if (newExp)
    tempStr = QString::asprintf("ExpAA: %u, %u (%u/330) with %u left",
		    newExp, totalExp, totalTick, maxExp - totalExp);
  else
    tempStr = QString::asprintf("ExpAA: < %u, %u (%u/330) with %u left",
		    tickExp, totalExp, totalTick, maxExp - totalExp);

  m_messages->addMessage(MT_Player, tempStr);
}

void MessageShell::addItem(const Item* item)
{
  uint32_t filterFlags = item->filterFlags();

  if (filterFlags == 0)
    return;

  QString prefix("Spawn");

  // first handle alert
  if (filterFlags & FILTER_FLAG_ALERT)
    filterMessage(prefix, MT_Alert, item);

  if (filterFlags & FILTER_FLAG_DANGER)
    filterMessage(prefix, MT_Danger, item);

  if (filterFlags & FILTER_FLAG_CAUTION)
    filterMessage(prefix, MT_Caution, item);

  if (filterFlags & FILTER_FLAG_HUNT)
    filterMessage(prefix, MT_Hunt, item);

  if (filterFlags & FILTER_FLAG_LOCATE)
    filterMessage(prefix, MT_Locate, item);
}

void MessageShell::delItem(const Item* item)
{
  // if it's an alert log the despawn
  if (item->filterFlags() & FILTER_FLAG_ALERT)
    filterMessage("DeSpawn", MT_Alert, item);
}

void MessageShell::killSpawn(const Item* item)
{
  // if it's an alert log the kill
  if (item->filterFlags() & FILTER_FLAG_ALERT)
    filterMessage("Died", MT_Alert, item);

  // if this is the player spawn, note the place of death
  if (item->id() != m_player->id())
    return;

  QString message;
  
  // use appropriate format depending on coordinate ordering
  if (!showeq_params->retarded_coords)
    message = "Died in zone '%1' at %2,%3,%4";
  else
    message = "Died in zone '%1' at %3,%2,%4";
  
  m_messages->addMessage(MT_Player, 
			 message.arg(m_zoneMgr->shortZoneName())
			 .arg(item->x()).arg(item->y()).arg(item->z()));
}

void MessageShell::filterMessage(const QString& prefix, MessageType type,
				 const Item* item)
{
  QString message;
  QString spawnInfo;

  // try to get a Spawn
  const Spawn* spawn = spawnType(item);

  // extra info if it is a spawn
  if (spawn)
    spawnInfo = QString::asprintf(" LVL %d, HP %d/%d", 
		      spawn->level(), spawn->HP(), spawn->maxHP());

  // use appropriate format depending on coordinate ordering
  if (!showeq_params->retarded_coords)
    message = "%1: %2/%3/%4 at %5,%6,%7%8";
  else
    message = "%1: %2/%3/%4 at %6,%5,%7%8";
  
  m_messages->addMessage(type, message.arg(prefix).arg(item->transformedName())
			 .arg(item->raceString()).arg(item->classString())
			 .arg(item->x()).arg(item->y()).arg(item->z())
			 .arg(spawnInfo));
}

#ifndef QMAKEBUILD
#include "messageshell.moc"
#endif

