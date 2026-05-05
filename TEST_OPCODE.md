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
- [x] OP_ZoneEntry — ClientZoneEntryStruct (client) / uint8_t (server) — `0xa5bf` (2026-05-04)
- [ ] OP_TimeOfDay — timeOfDayStruct (server)
- [ ] OP_NewZone — uint8_t (server, variable)
- [x] OP_SpawnDoor — doorStruct (server, modulus) — `0x794d` (2026-05-04)
- [ ] OP_GroundSpawn — makeDropStruct (server)
- [ ] OP_SendZonePoints — zonePointsStruct (server)
- [ ] OP_ZoneChange — zoneChangeStruct (both)

### Movement / position (4)
- [ ] OP_ClientUpdate — playerSpawnPosStruct (server)
- [ ] OP_NpcMoveUpdate — uint8_t (server, variable)
- [x] OP_MobUpdate — spawnPositionUpdate (both) — `0x4a4f` (2026-05-04)
- [ ] OP_MovementHistory — uint8_t (client, variable)

### Spawn lifecycle / appearance (5)
- [ ] OP_DeleteSpawn — deleteSpawnStruct (both)
- [x] OP_RemoveSpawn — removeSpawnStruct (both) — `0xeb88` (2026-05-04)
- [ ] OP_Death — newCorpseStruct (server)
- [ ] OP_SpawnAppearance — spawnAppearanceStruct (both)
- [ ] OP_Animation — uint8_t (both)

### Combat / actions (4)
- [ ] OP_Action — actionStruct (both)
- [x] OP_Action2 — action2Struct (both) — `0x32a9` (2026-05-04)
- [ ] OP_Consider — considerStruct (both)
- [ ] OP_TargetMouse — clientTargetStruct (both)

### Stats / HP / mana / xp / endurance (9)
- [ ] OP_ExpUpdate — expUpdateStruct (server)
- [ ] OP_AAExpUpdate — altExpUpdateStruct (server)
- [ ] OP_HPUpdate — hpNpcUpdateStruct (both)
- [x] OP_MobHealth — mobHealthStruct (server) — `0x8d24` (2026-05-04)
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

### 2026-05-04 — OP_ZoneEntry = 0xa5bf
- Capture: `tests/replay/test-zone-entry.vpk` (lvl 1 Necromancer tutorial session)
- Method: `--opcode-stats` showed bidirectional pattern matching the XML's two-payload entry: 2 C>S at 92 bytes (= sizeof(ClientZoneEntryStruct)) plus 361 S>C at varied sizes (server-side spawn data, uint8_t variable).
- Sample bytes (C>S 92b): plaintext player name visible at the standard `ClientZoneEntryStruct.name` offset.
- Struct fit: 92 = sizeof(ClientZoneEntryStruct) exactly; client→server direction matches XML.
- Ruled out: nothing else was C>S 92b in the capture.

### 2026-05-04 — OP_SpawnDoor = 0x794d
- Capture: `tests/replay/test-zone-entry.vpk`
- Method: `--opcode-stats`. 8 fires, all S>C, all 136 bytes.
- Struct fit: 136 = sizeof(doorStruct) exactly. Tutorial zone has multiple doors → fires once per door at zone-in.
- Ruled out: only opcode in capture at S>C 136b. No competitor.

### 2026-05-04 — OP_RemoveSpawn = 0xeb88
- Capture: `tests/replay/test-zone-entry.vpk`
- Method: `--opcode-stats`. 50 fires S>C at 5 bytes (+ 1 stray at 4b).
- Struct fit: 5 = sizeof(removeSpawnStruct). High-count S>C 5b matches mob despawn during play.
- Ruled out: no other opcode in capture at S>C 5b. deleteSpawnStruct is 4 bytes (different opcode).

### 2026-05-04 — OP_MobUpdate = 0x4a4f
- Capture: `tests/replay/test-zone-entry.vpk`
- Method: `--opcode-stats`. 80 fires, all S>C, all 14 bytes.
- Struct fit: 14 = sizeof(spawnPositionUpdate). High-count broadcast of mob positions during play.
- Ruled out: no other opcode at S>C 14b. spawnPositionUpdate is the only 14-byte struct in the candidate table.

### 2026-05-04 — OP_Action2 = 0x32a9
- Capture: `tests/replay/test-zone-entry.vpk`
- Method: `--opcode-stats`. 14 fires, all S>C, all 48 bytes.
- Struct fit: 48 = sizeof(action2Struct). Combat actions during the mob-fight portion of capture.
- Ruled out: no other opcode at S>C 48b. action2Struct is the only 48-byte struct.

### 2026-05-04 — OP_MobHealth = 0x8d24
- Capture: `tests/replay/test-zone-entry.vpk`
- Method: `--opcode-stats`. 10 fires, all S>C, all 6 bytes.
- Struct fit: 6 = sizeof(mobHealthStruct). Percent-HP broadcast on the mob during the fight.
- Ruled out: no other opcode at S>C 6b. mobHealthStruct is the only 6-byte struct.
