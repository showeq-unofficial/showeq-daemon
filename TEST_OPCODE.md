# Test client opcode rediscovery

Target server: **EverQuest Test client**. Tracking re-discovery of **73 opcodes** confirmed on Live (53 zone + 20 world). Opcodes that are still `[ ]` on `OPCODES_LIVE_TODO.md` (i.e., never confirmed on Live) are NOT in scope here — they remain a Live-tracker concern on `main`.

When all 73 entries below are `[x]`, the `test-client` branch is ready for merge-back review.

Checkbox legend: `[ ]` unresolved, `[x]` resolved on Test, `[~]` confirmed-absent on Test (won't be hunted further).

Confirmation bar: count + zero-competitor over n≥5; n=2-3 is enough if `--opcode-stats` reports no other unknowns at the target size+direction (per memory `feedback_opcode_disambiguation.md`).

Per-entry format: `[ ] OP_Name — typename (dir)`. Each resolved entry gets `0x<hex>` + date appended on confirmation. Append a dated confirmation log entry below each resolved opcode (capture name, method, sample bytes, struct fit, ruled-out leads).

---

## World stream (20)

### Login / handshake (8)
- [ ] OP_SendLoginInfo — *(no payload struct; identifier only)*
- [ ] OP_LogServer
- [ ] OP_ApproveWorld
- [ ] OP_EnterWorld
- [ ] OP_ExpansionInfo
- [ ] OP_SendCharInfo
- [ ] OP_ZoneServerInfo
- [ ] OP_WorldComplete

### Checksums / verification (4)
- [ ] OP_SendSpellChecksum
- [ ] OP_SendExeChecksum
- [ ] OP_SendBaseDataChecksum
- [ ] OP_SendSkillCapsChecksum

### Chat servers (2)
- [ ] OP_SetChatServer
- [ ] OP_SetChatServer2

### World content (2)
- [ ] OP_GuildList — worldGuildListStruct (server)
- [ ] OP_MOTD — worldMOTDStruct (server)

### Character management (4)
- [ ] OP_DeleteCharacter
- [ ] OP_CharacterCreate
- [ ] OP_ApproveName
- [ ] OP_RandomNameGenerator

---

## Zone stream (53)

### Zone bootstrap (8)
- [ ] OP_PlayerProfile — uint8_t (server, variable)
- [ ] OP_ZoneEntry — ClientZoneEntryStruct (client) / uint8_t (server)
- [ ] OP_TimeOfDay — timeOfDayStruct (server)
- [ ] OP_NewZone — uint8_t (server, variable)
- [ ] OP_SpawnDoor — doorStruct (server, modulus)
- [ ] OP_GroundSpawn — makeDropStruct (server)
- [ ] OP_SendZonePoints — zonePointsStruct (server)
- [ ] OP_ZoneChange — zoneChangeStruct (both)

### Movement / position (4)
- [ ] OP_ClientUpdate — playerSpawnPosStruct (server)
- [ ] OP_NpcMoveUpdate — uint8_t (server, variable)
- [ ] OP_MobUpdate — spawnPositionUpdate (both)
- [ ] OP_MovementHistory — uint8_t (client, variable)

### Spawn lifecycle / appearance (5)
- [ ] OP_DeleteSpawn — deleteSpawnStruct (both)
- [ ] OP_RemoveSpawn — removeSpawnStruct (both)
- [ ] OP_Death — newCorpseStruct (server)
- [ ] OP_SpawnAppearance — spawnAppearanceStruct (both)
- [ ] OP_Animation — uint8_t (both)

### Combat / actions (4)
- [ ] OP_Action — actionStruct (both)
- [ ] OP_Action2 — action2Struct (both)
- [ ] OP_Consider — considerStruct (both)
- [ ] OP_TargetMouse — clientTargetStruct (both)

### Stats / HP / mana / xp / endurance (9)
- [ ] OP_ExpUpdate — expUpdateStruct (server)
- [ ] OP_AAExpUpdate — altExpUpdateStruct (server)
- [ ] OP_HPUpdate — hpNpcUpdateStruct (both)
- [ ] OP_MobHealth — mobHealthStruct (server)
- [ ] OP_ManaChange — manaDecrementStruct (server)
- [ ] OP_SkillUpdate — skillIncStruct (server)
- [ ] OP_LevelUpdate — levelUpUpdateStruct (server)
- [ ] OP_EndUpdate — endUpdateStruct (server)
- [ ] OP_Stamina — staminaStruct (server) *(hunger/thirst, not endurance)*

### Buffs (1)
- [ ] OP_Buff — buffStruct (both)

### Group (6)
- [ ] OP_GroupInvite — groupInviteStruct (both)
- [ ] OP_GroupFollow — groupFollowStruct (server)
- [ ] OP_GroupUpdate — uint8_t (both, variable)
- [ ] OP_GroupDisband — groupDisbandStruct (server)
- [ ] OP_GroupDisband2 — groupDisbandStruct (server)
- [ ] OP_GroupLeader — groupLeaderChangeStruct (server)

### Guild (5)
- [ ] OP_GuildMOTD — guildMOTDStruct (server)
- [ ] OP_GuildMemberUpdate — GuildMemberUpdate (server)
- [ ] OP_GuildMemberList — uint8_t (server, variable)
- [ ] OP_ExpandedGuildInfo — uint8_t (server, variable)
- [ ] OP_GuildsInZoneList — guildsInZoneListStruct (server)

### Inventory / items (2)
- [ ] OP_ItemPacket — itemPacketStruct (server)
- [ ] OP_MoveItem — moveItemStruct (client)

### Click / interact (2)
- [ ] OP_ClickObject — remDropStruct (both)
- [ ] OP_Find — uint8_t (server, variable)

### Chat / messaging (4)
- [ ] OP_SimpleMessage — simpleMessageStruct (server)
- [ ] OP_FormattedMessage — formattedMessageStruct (server)
- [ ] OP_CommonMessage — channelMessageStruct (both)
- [ ] OP_SpecialMesg — specialMessageStruct (server)

### Alternate Advancement (3)
- [ ] OP_SendAATable — *(static ability-definition menu)*
- [ ] OP_AAAction
- [ ] OP_RespondAA — *(per-spend response with full AA list)*

---

## Confirmation log

Append a dated entry per resolved opcode. Format mirrors `OPCODES_LIVE_TODO.md`:

```
### YYYY-MM-DD — OP_Name = 0xHHHH
- Capture: tests/replay/test-<scenario>.vpk
- Method: --opcode-stats / --dump-payload / --list-events evidence
- Sample bytes: <hex preview>
- Struct fit: typename, sizeof, payload size matches n times
- Ruled out: <other candidates and why>
```

(No entries yet.)
