# Test client opcode rediscovery

Target server: **EverQuest Test client**. Tracking re-discovery of **73 opcodes** confirmed on Live (53 zone + 20 world). Opcodes that are still `[ ]` on `OPCODES_LIVE_TODO.md` (i.e., never confirmed on Live) are NOT in scope here — they remain a Live-tracker concern on `main`.

When all 73 entries below are `[x]`, the `test-client` branch is ready for merge-back review.

Checkbox legend: `[ ]` unresolved, `[x]` resolved on Test, `[~]` confirmed-absent on Test (won't be hunted further).

Confirmation bar: count + zero-competitor over n≥5; n=2-3 is enough if `--opcode-stats` reports no other unknowns at the target size+direction (per memory `feedback_opcode_disambiguation.md`).

Per-entry format: `[ ] OP_Name — typename (dir)`. Each resolved entry gets `0x<hex>` + date appended on confirmation. Append a dated confirmation log entry below each resolved opcode (capture name, method, sample bytes, struct fit, ruled-out leads).

---

## World stream (20)

### Login / handshake (8)
- [x] OP_SendLoginInfo — *(no payload struct; identifier only)* — `0x7b6a` (2026-05-04)
- [x] OP_LogServer — `0xb537` (2026-05-04)
- [x] OP_ApproveWorld — `0xb8cc` (2026-05-04)
- [x] OP_EnterWorld — `0x9bdc` (2026-05-04)
- [ ] OP_ExpansionInfo
- [x] OP_SendCharInfo — `0x84f6` (2026-05-04)
- [ ] OP_ZoneServerInfo
- [x] OP_WorldComplete — `0xfc46` (2026-05-04)

### Checksums / verification (4)
- [x] OP_SendSpellChecksum — `0x2de1` (2026-05-04)
- [x] OP_SendExeChecksum — `0x44d9` (2026-05-04)
- [x] OP_SendBaseDataChecksum — `0x289c` (2026-05-04)
- [x] OP_SendSkillCapsChecksum — `0xa958` (2026-05-04)

### Chat servers (2)
- [ ] OP_SetChatServer
- [ ] OP_SetChatServer2

### World content (2)
- [x] OP_GuildList — worldGuildListStruct (server) — `0x022f` (2026-05-04)
- [ ] OP_MOTD — worldMOTDStruct (server)

### Character management (4)
- [ ] OP_DeleteCharacter
- [ ] OP_CharacterCreate
- [ ] OP_ApproveName
- [ ] OP_RandomNameGenerator

---

## Zone stream (53)

### Zone bootstrap (8)
- [x] OP_PlayerProfile — uint8_t (server, variable) — `0xe284` (2026-05-04)
- [x] OP_ZoneEntry — ClientZoneEntryStruct (client) / uint8_t (server) — `0xa5bf` (2026-05-04)
- [x] OP_TimeOfDay — timeOfDayStruct (server) — `0x7e22` (2026-05-04)
- [x] OP_NewZone — uint8_t (server, variable) — `0xa923` (2026-05-04)
- [x] OP_SpawnDoor — doorStruct (server, modulus) — `0x794d` (2026-05-04)
- [x] OP_GroundSpawn — makeDropStruct (server) — `0x33ec` (2026-05-04)
- [ ] OP_SendZonePoints — zonePointsStruct (server)
- [x] OP_ZoneChange — zoneChangeStruct (both) — `0x9148` (2026-05-04)

### Movement / position (4)
- [x] OP_ClientUpdate — playerSpawnPosStruct (server) — `0xf8d1` (2026-05-04, sizechecktype relaxed to none)
- [x] OP_NpcMoveUpdate — uint8_t (server, variable) — `0x917c` (2026-05-04)
- [x] OP_MobUpdate — spawnPositionUpdate (both) — `0x4a4f` (2026-05-04)
- [x] OP_MovementHistory — uint8_t (client, variable) — `0x9e21` (2026-05-04)

### Spawn lifecycle / appearance (5)
- [x] OP_DeleteSpawn — deleteSpawnStruct (both) — `0x6dba` (2026-05-04)
- [x] OP_RemoveSpawn — removeSpawnStruct (both) — `0xeb88` (2026-05-04)
- [x] OP_Death — newCorpseStruct (server) — `0x1eb2` (2026-05-04)
- [x] OP_SpawnAppearance — spawnAppearanceStruct (both) — `0x9826` (2026-05-04)
- [x] OP_Animation — uint8_t (both) — `0xdd87` (2026-05-05)

### Combat / actions (4)
- [x] OP_Action — actionStruct (both) — `0x049e` (2026-05-04)
- [x] OP_Action2 — action2Struct (both) — `0x32a9` (2026-05-04)
- [x] OP_Consider — considerStruct (both) — `0xa1e7` (2026-05-04)
- [x] OP_TargetMouse — clientTargetStruct (both) — `0x1994` (2026-05-04)

### Stats / HP / mana / xp / endurance (9)
- [x] OP_ExpUpdate — expUpdateStruct (server) — `0xcf53` (2026-05-04)
- [ ] OP_AAExpUpdate — altExpUpdateStruct (server)
- [x] OP_HPUpdate — hpNpcUpdateStruct (both) — `0x652f` (2026-05-04)
- [x] OP_MobHealth — mobHealthStruct (server) — `0x8d24` (2026-05-04)
- [x] OP_ManaChange — manaDecrementStruct (server) — `0x08fb` (2026-05-05)
- [x] OP_SkillUpdate — skillIncStruct (server) — `0x6a60` (2026-05-04)
- [x] OP_LevelUpdate — levelUpUpdateStruct (server) — `0x9426` (2026-05-04)
- [x] OP_EndUpdate — endUpdateStruct (server) — `0x36d1` (2026-05-04)
- [x] OP_Stamina — staminaStruct (server) — `0x60e7` (2026-05-04)

### Buffs (1)
- [x] OP_Buff — buffStruct (both) — `0x3b54` (2026-05-04)

### Group (6)
- [x] OP_GroupInvite — groupInviteStruct (both) — `0x67a4` (2026-05-05)
- [x] OP_GroupFollow — groupFollowStruct (server) — `0x01dc` (2026-05-05)
- [x] OP_GroupUpdate — uint8_t (both, variable) — `0xccfc` (2026-05-05)
- [x] OP_GroupDisband — groupDisbandStruct (server) — `0x8a85` (2026-05-05)
- [x] OP_GroupDisband2 — groupDisbandStruct (server) — `0x2c76` (2026-05-05)
- [x] OP_GroupLeader — groupLeaderChangeStruct (server) — `0xb269` (2026-05-05)

### Guild (5)
- [ ] OP_GuildMOTD — guildMOTDStruct (server)
- [ ] OP_GuildMemberUpdate — GuildMemberUpdate (server)
- [ ] OP_GuildMemberList — uint8_t (server, variable)
- [ ] OP_ExpandedGuildInfo — uint8_t (server, variable)
- [ ] OP_GuildsInZoneList — guildsInZoneListStruct (server)

### Inventory / items (2)
- [x] OP_ItemPacket — itemPacketStruct (server) — `0xe8bc` (2026-05-04)
- [x] OP_MoveItem — moveItemStruct (client) — `0xdee3` (2026-05-04)

### Click / interact (2)
- [x] OP_ClickObject — remDropStruct (both) — `0xab5e` (2026-05-04)
- [x] OP_Find — uint8_t (server, variable) — `0x695f` (2026-05-05)

### Chat / messaging (4)
- [x] OP_SimpleMessage — simpleMessageStruct (server) — `0x098d` (2026-05-05)
- [x] OP_FormattedMessage — formattedMessageStruct (server) — `0x0ecf` (2026-05-04)
- [x] OP_CommonMessage — channelMessageStruct (both) — `0x498b` (2026-05-04)
- [x] OP_SpecialMesg — specialMessageStruct (server) — `0x7162` (2026-05-04)

### Alternate Advancement (3)
- [x] OP_SendAATable — *(static ability-definition menu)* — `0xce3d` (2026-05-04)
- [ ] OP_AAAction
- [x] OP_RespondAA — *(per-spend response with full AA list)* — `0xfb0a` (2026-05-04)

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

### 2026-05-04 — OP_LevelUpdate = 0x9426
- Capture: `tests/replay/test-zone-entry.vpk`. The lvl 1→2 ding event is the anchor.
- Method: `--dump-payload 0x9426:`. 1 fire, 16 bytes S>C.
- Sample bytes: `02 00 00 00  01 00 00 00  d4 6f 00 00  00 00 00 00`.
- Struct fit: levelUpUpdateStruct{level=2, levelOld=1, exp=0x6fd4=28628, unknown=0} — exact match. The new/old level pair confirms the ding.
- Ruled out: no competitor at S>C 16b with this structure. ExpUpdate (also 16b) shares the size band but type-field semantics differ (see 0xcf53 below).

### 2026-05-04 — OP_ExpUpdate = 0xcf53
- Capture: `tests/replay/test-zone-entry.vpk`.
- Method: `--dump-payload 0xcf53:`. 5 fires, all 16 bytes S>C.
- Sample bytes (fire 5): `d4 6f 00 00  00 00 00 00  02 00 00 00  00 00 00 00`.
- Struct fit: expUpdateStruct{exp, unknown, type, unknown}. exp=0x6fd4=28628 matches the post-level-up exp seen in OP_LevelUpdate above. type field cycles 0/2 across fires (0=set, 2=update per the struct comment) — matches the observed mix of initial-set and incremental-update fires.
- Ruled out: levelUpUpdateStruct ruled out because the type field would not show the 0/2 cycle and the level/oldLevel pair would not be 0/0 in 4 of 5 fires.

### 2026-05-04 — OP_SkillUpdate = 0x6a60
- Capture: `tests/replay/test-zone-entry.vpk`. Anchor: the Meditation skill-up to value 11.
- Method: `--dump-payload 0x6a60:`. 1 fire, 12 bytes S>C.
- Sample bytes: `1f 00 00 00  0b 00 00 00  01 00 00 00`.
- Struct fit: skillIncStruct{skillId=31, value=11, unknown=1}. `src/skills.h:55` lists slot 31 as "Meditate". Value=11 matches the in-game skill rank reached.
- Ruled out: altExpUpdateStruct (12b) — at level 1 the player has 0 AA, so AA-xp updates wouldn't fire here. simpleMessageStruct (12b) — would not have a small skill-id-shaped first u32. remDropStruct (12b) — needs a clickObject event, none observed.

### 2026-05-04 — OP_HPUpdate = 0x652f
- Capture: `tests/replay/test-zone-entry.vpk`. Anchor: HP movement during the mob fight.
- Method: `--dump-payload 0x652f:`. 18 fires, all 18 bytes S>C.
- Sample bytes (fires 1–3): `63 2d 1d 00 00 00  00 00 00 00 1d 00 00 00  00 00 00 00`, then `63 2d 16 00 ...` and `63 2d 18 00 ...`.
- Struct fit: hpNpcUpdateStruct{spawnId=0x2d63=11619, curHP, unknown, maxHP=29, unknown}. curHP varies 22-29 across fires (combat damage + regen ramp); maxHP stays 29 (lvl 1 player). spawnId 11619 is the local PC, consistent with the same prefix appearing in OP_EndUpdate / OP_FormattedMessage dumps.
- Ruled out: 0x917c was the higher-count (99x) competitor at S>C 18b; rejected on byte layout — its u16 prefix doesn't move like a curHP value, and its size is variable (15/17/18) which fits OP_NpcMoveUpdate (uint8_t variable) better than the fixed-18b hpNpcUpdateStruct.

### 2026-05-04 — OP_EndUpdate = 0x36d1
- Capture: `tests/replay/test-zone-entry.vpk`. Anchor: endurance regen ramp from 0 toward max.
- Method: `--dump-payload 0x36d1:`. 28 fires, all 10 bytes S>C.
- Sample bytes (fires 1–3): `63 2d  00 00 00 00  1c 00 00 00`, then `63 2d  01 00 00 00  1c 00 00 00`, then `63 2d  02 00 00 00  1c 00 00 00`.
- Struct fit: endUpdateStruct{spawn_id=0x2d63=11619, cur=0/1/2/…, max=0x1c=28} — exact match. Same spawn_id as OP_HPUpdate (the local PC). cur ramps from 0 each tick; max=28 fits a lvl 1 endurance pool.
- Ruled out: 0xf96e (10b S>C, 10 fires) was the only competitor; rejected on count gap (28 vs 10) and need to actually see the endurance ramp pattern.

### 2026-05-04 — OP_ApproveWorld + OP_EnterWorld + OP_WorldComplete trio
- Capture: `tests/replay/test-login-2.vpk`. The earlier `c7e1b6b` commit incorrectly attributed these opcodes' invisible dump output to a `decodedWorldPacket` signal-arity issue — the real cause was that `/tmp/oc` had been removed during cleanup, so `--dump-payload` writes failed with a silent "No such file or directory" qWarning. The dumper's signal connections are fine.
- **OP_ApproveWorld = 0xb8cc**: 2x S>C 16b, identical bytes both fires: `04 00 00 00  00 00 00 00  00 00 00 00  03 00 00 00`. Static fixed-size handshake response — server approves the login with constant codes (4, 0, 0, 3).
- **OP_EnterWorld = 0x9bdc**: 2x C>S 8b, identical bytes both fires: `41 74 cf 35  00 00 00 00`. The u32 0x35cf7441 looks like a character-id/handle that the user picks at char-select; matches across logins because the user re-entered with the same character (<charname>).
- **OP_WorldComplete = 0xfc46**: 2x C>S 0b, empty payload — fires once per login session, fits "client tells world server it's done with the world stage" (per legacy `worldopcodes.xml` comment "Client telling world server it is done. World replies by disconnecting"). Distinguished from other 0b C>S opcodes (0x9e64/0x72b1/0xee58, each 1 fire) by the 2-per-session count.

### 2026-05-04 — OP_SendCharInfo = 0x84f6
- Capture: `tests/replay/test-login-2.vpk` (2x S>C 784b in this small login) and `tests/replay/test-zone-entry.vpk` (1x S>C 27190b — the user's full account with extra chars).
- Method: `--dump-payload 0x84f6:`. Payload structure: u32 header (`50 04 00 00`), per-character record array. Each record is variable-length: `u32 char-id <NUL-terminated charname> u32 server-id u32 flags…`. Multiple character names in plaintext confirm this is the char-select roster.
- Struct fit: matches the canonical SendCharInfo layout — variable list of character entries with name strings.
- Ruled out: only multi-char-name-bearing variable-size S>C in the world stream; 0xa925 (1704b 2x) didn't dump (decodedWorldPacket signal-arity mismatch in OpcodePayloadDumper that I can chase as a separate fix).

### 2026-05-04 — OP_SendLoginInfo = 0x7b6a
- Capture: `tests/replay/test-login-logout.vpk` (login + camp).
- Method: `--list-events` shows `0x7b6a` is the FIRST C>S packet of every login burst (3 fires across login + camp-back). `--dump-payload 0x7b6a:` confirms.
- Sample bytes: 9-digit ASCII account number + NUL + 10-char ASCII session key + NUL, followed by 444 bytes of padding zeros (464b total). Concrete values redacted as account secrets.
- Struct fit: matches the canonical SendLoginInfo layout — account-id string + session-key string in a fixed 464-byte envelope.
- Ruled out: no other C>S 464b opcode in the world stream.

### 2026-05-04 — OP_GuildList = 0x022f
- Capture: `tests/replay/test-login-logout.vpk`.
- Method: `--dump-payload 0x022f:`. 1 fire, 341132 bytes S>C.
- Sample bytes: 16-byte header followed by NUL-terminated guild-name strings — visible plaintext: "Storied Merchant's Loot Rucksack", "Year of Darkpaw", "A large ba…" (truncated). Hundreds of guild names follow.
- Struct fit: matches `worldGuildListStruct` (variable list of guild entries). XML's `sizechecktype="none"` matches the variable-payload nature.
- Ruled out: only 341KB-class packet in the world stream; size and ASCII content unambiguous.

### 2026-05-04 — OP_LogServer = 0xb537
- Capture: `tests/replay/test-login-logout.vpk`.
- Method: `--dump-payload 0xb537:`. 2 fires, 34765 bytes S>C each.
- Sample bytes: header `01 07 00 00 00` + ASCII timezone string "PST8PDT8PDT" + structured server config table (zone metadata + counts).
- Struct fit: a "log server" / world-config blob carrying server-side timezone + server metadata. The TZ string is the unmistakable anchor.
- Ruled out: no other 34KB-class S>C packet in the world stream. Note: 34KB is much larger than legacy expectations for OP_LogServer — Test may have folded zone-list / server-config data into this packet.

### 2026-05-04 — OP_Send{Spell,BaseData,SkillCaps,Exe}Checksum quartet
- Capture: `tests/replay/test-login-logout.vpk`.
- Method: `--list-events` for fire-ordering + `--dump-payload` for size/shape, disambiguated against legacy `../showeq/conf/worldopcodes.xml` declaration order. Neither legacy showeq nor the daemon has a code handler for these opcodes (registered for naming only), so the legacy XML declaration order — Spell → Exe → BaseData → SkillCaps — is the authoritative ordering hint.
- Observed in capture: three opcodes fire as C>S 2056b in this fire order during each login handshake — `0x2de1` first, then `0x289c`, then `0xa958`. A fourth opcode `0x44d9` fires C>S 64b in the same handshake.
- Disambiguation:
  - **OP_SendSpellChecksum = 0x2de1** — 1st in legacy XML and 1st observed in fire order. Legacy comment "Contains a snippet of spell data" confirms it leads the verification handshake.
  - **OP_SendBaseDataChecksum = 0x289c** — 2nd 2056b packet (legacy lists Exe between Spell and BaseData, but Exe is the 64b packet, so among the three 2056b packets BaseData takes the 2nd slot).
  - **OP_SendSkillCapsChecksum = 0xa958** — 3rd in legacy declaration order ("Third client verification packet") and 3rd observed.
  - **OP_SendExeChecksum = 0x44d9** — the only checksum-shaped C>S packet at a non-2056b size (64b). Payload starts with `04 00 00 00` followed by 60 bytes of binary hash data, fitting an exe-only checksum (smaller fixed binary digest vs. the larger spell/base-data/skill-cap tables).
- Sample bytes (each 2056b packet has a unique 8-byte signature followed by an array of little-endian u32 hash entries):
  - 0x2de1: `fa a5 d2 74  78 fc e6 00  78 a7 1f 00  8a 6b 14 00  …`
  - 0x289c: `3b 6e 98 4b  46 26 13 00  c0 b2 02 00  5c e5 00 00  …`
  - 0xa958: `ae 5d 2b f4  8b 38 01 00  e9 0e 00 00  89 1d 00 00  …`
- Confidence: high for the **set** mapping (4 names → 4 IDs, byte sizes constrain Exe→64b uniquely). Medium-high for the spell/base/skillcaps individual mapping within the 2056b trio: relies on the legacy declaration-order convention being preserved on Test, which is a reasonable but unverified assumption. Re-verify after the next capture by checking a session log for the same fire order.
- **Re-verified 2026-05-04 against `tests/replay/test-login-2.vpk`**: same fire order (`0x2de1 → 0x289c → 0xa958` for the 2056b trio, `0x44d9` paired with SkillCaps in the next ms) reproduced in both login bursts of the new capture. The legacy-declaration-order disambiguation holds.

### 2026-05-04 — OP_SpecialMesg = 0x7162
- Capture: `tests/replay/test-combat.vpk`. 93 fires, S>C, sizes 57-235b (variable per message).
- Method: `--dump-payload 0x7162:`. Payload header (~12b flags/color/spawn-id), then NUL-terminated speaker name, then NUL-terminated message text.
- Sample bytes (fire 1): `01 01 00 33 01 00 00 e2 09 00 00 41 72 69 61 73 00 …` followed by message string `"We found the other slaves!  Not bad, my friend, not bad.  No matter what happens in the mines, you should always be able to find your way back here.  If you seek allies in other instances of these mines, I can send you there if you [wish]."` Speaker = `"Arias"` — the tutorial NPC.
- Struct fit: `specialMessageStruct` with embedded sayer + variable text matches the modern Live wire format (sayer-name lets clients tag the message with the speaker; OP_FormattedMessage doesn't carry a sayer).
- Ruled out: 0x0ecf (already confirmed as OP_FormattedMessage at 13b fixed S>C, no sayer name).

### 2026-05-04 — OP_ItemPacket = 0xe8bc
- Capture: `tests/replay/test-combat.vpk`. 55 fires, all S>C, sizes 949-1018b (variable per item).
- Method: `--dump-payload 0xe8bc:`. Each payload starts with a u32 type/count prefix then a NUL-terminated item-serial ASCII string (`"vTeS004000090000"`), structured stat fields, then NUL-terminated item name + description.
- Sample bytes (fire 1, head): `6b 00 00 00 "vTeS004000090000\0" 00 01 00 00 00 05 00 00 00 00 00 ff ff ff ff …`. Plaintext strings: `"Gloomingdeep Kobold Cloth Sleeves"` + `"Worn by the kobolds in Gloomingdeep Mines"`.
- Struct fit: `itemPacketStruct` per zoneopcodes.xml. 55 fires fits an inventory burst (55+ equipped/inventory items).
- Ruled out: 0x2b01 (48x S>C ~700-1000b similar size) carries quest descriptions ("Join the revolution!", `<c "#F0F000">…`) — that's OP_TaskDescription, out of 73-list scope.

### 2026-05-04 — OP_ClickObject = 0xab5e
- Capture: `tests/replay/test-combat.vpk`. 7 fires (3 C>S + 4 S>C), all 12b — direction "both" matches XML.
- Method: `--dump-payload 0xab5e:`. Sample fires: `5e 00 00 00  e2 09 00 00  00 00 00 00` and `69 00 00 00  e2 09 00 00  00 00 00 00`.
- Struct fit: 12 = sizeof(remDropStruct). Layout matches: u16 dropId (0x5e=94, 0x69=105 — different ground spawns clicked) at offset 0, u16 spawnId (0x09e2 = <charname>, the clicker) at offset 4, trailing 4 zero bytes.
- Ruled out: other 12b candidates (0x14dc heartbeat, 0x098d per-mob updates, etc.) lack the dropId+spawnId pair pattern.

### 2026-05-04 — OP_MoveItem = 0xdee3
- Capture: `tests/replay/test-combat.vpk`. 40 fires (33 C>S + 7 S>C ack), all 28b — the daemon's candidate-match section already flagged this opcode as a strong moveItemStruct fit (`size@28=2 dir-match~2 dirs(C>S=2)` from the test-zone-entry stats).
- Method: `--dump-payload 0xdee3:`. Sample (fire 1, 28b): `05 00 00 00  00 00 ff ff ff ff  00 00 00 00 00 00  23 00 ff ff ff ff  8f 56 00 00 00 00` — from-slot=5, sentinel, slot-id 0x23=35, sentinel, item-cookie 0x568f.
- Struct fit: 28 = sizeof(moveItemStruct). C>S-dominant with S>C acks fits "client requests slot move, server confirms".
- Ruled out: 0xa17e (481x S>C 28b but identical bytes per fire) is a static config blob; 0x202b / 0xb570 (1-2 C>S 28b) are too rare.

### 2026-05-04 — OP_TimeOfDay = 0x7e22
- Capture: `tests/replay/test-zone-entry.vpk`. 2 fires, both 8b S>C.
- Method: `--dump-payload 0x7e22:`. Anchor: bytes decode cleanly as the documented timeOfDayStruct.
- Sample bytes:
  - fire 1: `14 22 01 04 bf 0c 00 00` → hour=20, min=34, day=1, month=4, year=0x0cbf=3263 (Norrathian)
  - fire 2: `15 23 01 04 bf 0c 00 00` → hour=21, min=35, day=1, month=4, year=3263 (1 in-game hour later)
- Struct fit: 8 = sizeof(timeOfDayStruct). Year 3263 is in the right Norrathian range.
- Ruled out: other 8b S>C 2-fire candidates (0x1f7f all-zeros; 0x5548 paired with 0x9826 SpawnAppearance shape) don't decode as time fields.

### 2026-05-04 — OP_Stamina = 0x60e7
- Capture: `tests/replay/test-combat.vpk`. 54 fires, all 8b S>C.
- Method: `--dump-payload 0x60e7:`. Each fire is two identical u32 values that decrement in lockstep across fires (`0x1094 0x1094`, `0x1074 0x1074`, `0x1054 0x1054`, …) — `staminaStruct{food, water}` with food == water and both ticking down at -32 per fire.
- Struct fit: 8 = sizeof(staminaStruct). Decrement rate consistent with combat-session length.
- Ruled out: 0x9826 / 0x5548 (8b S>C with `<spawn_id, code=4>` shape) don't have the paired-decrement pattern.

### 2026-05-04 — OP_SpawnAppearance = 0x9826
- Capture: `tests/replay/test-combat.vpk`. 44 fires (43 S>C + 1 C>S — direction "both" matches XML).
- Method: `--dump-payload 0x9826:`. Per-fire payload is `<spawn_id_u32 varying> 04 00 00 00`, i.e. a spawn-id at offset 0 with appearance value 4 in the parameter slot — fits `spawnAppearanceStruct{u16 spawnId; u16 type; u32 parameter}` reading `spawnId+0` as a u32 spawn-id (test-combat happens to keep the type byte zero, parameter=4 across all fires for whatever stand/alive code is being announced).
- Struct fit: 8 = sizeof(spawnAppearanceStruct). High mixed-direction count fits per-spawn appearance broadcasts during combat.
- Ruled out: 0x5548 (43x mixed 8b) is a near-twin candidate with the same payload shape — likely OP_FaceChange or OP_Animation-class (both also 8b appearance-style structs); confirming which would need a /sit /stand /face change capture sequence to differentiate. 0x60e7 is OP_Stamina (paired-decrementing pattern, distinct shape).

### 2026-05-04 — OP_ClientUpdate = 0xf8d1
- Capture: `tests/replay/test-combat.vpk`. 4078 fires: 3212 C>S 42b + 866 S>C 24b. Highest-volume bidirectional opcode in the capture.
- Method: `--dump-payload 0xf8d1:`. C>S samples (selected from across the run): `36 01  e2 09 00 00  00 00 12 c3 00 00  6c 31 00 00 00 00 00  64 72 69 [varying]  88 41 00 00 00 40 00 00  00 00 00 00 f7 7f 00 00 00 00`. Layout breakdown:
  - bytes 0-1: per-fire counter (sequence number, increments)
  - bytes 2-3: spawn-id `0x09e2` = <charname> (constant)
  - bytes 4-7: zeros / spawn-id-2 placeholder
  - bytes 8-15: bit-packed position fields (varying per fire — heading, pos)
  - bytes 16-21: zeros + ASCII fragment `64 72 69` ("dri") that's constant across all 3212 fires followed by a varying byte (looks like a fixed 4-byte field, possibly an autorun-state code or feature-flag block)
  - bytes 22-29: position floats (varying)
  - bytes 30-37: zeros
  - bytes 38-39: `f7 7f` Test-infra sentinel (we've seen `f6 7f` / `f7 7f` recur)
  - bytes 40-41: zeros
- The 24b S>C side (e.g. fire 10): `b6 09 00 00  4c e0 1e 00  00 00 00 00  dd fe 2f 02  00 e0 f6 fc  00 40 e8 fe` — `b6 09` is a non-player spawn id, then bit-packed pos. Server broadcasts other spawns' positions in this shorter format.
- Struct fit: legacy `playerSpawnPosStruct` is 28b; Test grew the C>S side to 42b (more state fields) and uses 24b S>C (slightly compressed). XML's `sizechecktype` was relaxed to `"none"` so the daemon stops dropping size-mismatched packets at routing time. Downstream parser still reads only the legacy fields — a follow-up to extend the struct definition is in scope.
- Ruled out: 0xa17e (481x S>C 28b but identical bytes per fire) was static config, not position broadcast; 0x9e21 was OP_MovementHistory (variable C>S, accumulates floats). 0xf8d1 is the high-frequency player-tick stream.

### 2026-05-04 — OP_Action = 0x049e
- Capture: `tests/replay/test-combat.vpk`. 105 fires, sizes 64:71 and 88:34.
- Method: `--dump-payload 0x049e:`. Mostly 64b matching `sizeof(actionStruct)`; 88b extended variant carries spell metadata.
- Sample bytes (fire 1, 64b): `e2 09  e2 09  16 80 00 00  04 00 00 00  …  00 00 80 3f  00 e0 b5 43  e7 02 00 00  10 00 00 00  19 00 ff ff ff ff` — target_id=src_id=0x09e2 (self-buff); spell flags 0x8016; float 1.0 (duration multiplier); spell-id range value 0x2e7=743 (early-game range).
- Struct fit: 64 = sizeof(actionStruct). Direction "both" per XML; combat-driven 105 fires fits the player's spell/melee actions during the fight.
- Ruled out: 0x3f37 / 0xf921 (1-3 fires each at 64b) — too low count for primary action stream; likely OP_Action2-like aux fires or non-combat 64b structs.

### 2026-05-04 — OP_Death = 0x1eb2
- Capture: `tests/replay/test-combat.vpk`. 54 fires, all 40b S>C.
- Method: `--dump-payload 0x1eb2:`. Each fire shows two u32 spawn-ids at offsets 0 and 4 (`8f 06`/`b6 09`, `b9 09`/`88 06`, …) — victim + killer pair — followed by combat metadata + sentinel.
- Struct fit: 40 = sizeof(newCorpseStruct). 54 deaths over a combat session matches the user fighting a chain of mobs.
- Ruled out: only 40b S>C in the capture with non-trivial fire count.

### 2026-05-04 — OP_Buff = 0x3b54
- Capture: `tests/replay/test-combat.vpk`. 7 fires, all 168b S>C.
- Method: `--dump-payload 0x3b54:`. Payload starts with spawn-id `e2 09 00 00`, then a sequence of `ff ff ff ff <float>` 16-byte rows (buff slot data with effect floats `-858.21, 2.085, …`), then `e5 da 1c 00 01 00 00 00` (timestamp/buff-id), then `16 80 00 00` (same spell-flag value seen in OP_Action), then `00 00 80 3f` (1.0 duration mod).
- Struct fit: 168 = sizeof(buffStruct). 7 fires = buffs applying during combat.
- Ruled out: 0x4396 / 0x7324 / 0xb269 (1 fire each at 168b) — too rare; likely related-but-distinct buff sub-opcodes or one-shot system events.

### 2026-05-04 — OP_Consider = 0xa1e7
- Capture: `tests/replay/test-combat.vpk`. 34 fires (17 S>C + 17 C>S — perfect request/response pair count).
- Method: `--dump-payload 0xa1e7:`. 28b each fire. Decoded against considerStruct layout:
  - Fire 2: `playerId=0x09e2, targetId=0x2e57, faction=5, level=6, curHp=0, maxHp=0, pvpcon=0, targetType=6(NPC), spareData=1000` — clean fit.
  - Fire 4: same player, targetId=0x3124, level=7, NPC.
- Struct fit: 28 = sizeof(considerStruct). The level field jumping per /con + matching faction byte + NPC type confirm.
- Ruled out: 0xa17e (481x S>C 28b but identical bytes per fire) is a static config blob, not Consider; 0xdee3 / 0x7957 at 28b have wrong direction balance.

### 2026-05-04 — OP_TargetMouse = 0x1994
- Capture: `tests/replay/test-combat.vpk`. 85 fires, all C>S 4b.
- Method: `--dump-payload 0x1994:`. Fires alternate `00 00 00 00` (clear target) and a 4-byte spawn-id (e.g. `57 2e 00 00` = 0x2e57 = the same target spawn-id seen in OP_Consider fire 2).
- Struct fit: 4 = sizeof(clientTargetStruct). Direction "both" per XML; this capture only shows the C>S "client sets target" half.
- Ruled out: 0x7159 (100x S>C 4b but all-zero bytes) is a heartbeat, not target broadcast; 0x6854 / 0x4c14 (smaller counts at 4b) are ambient.

### 2026-05-04 — OP_ZoneChange = 0x9148
- Capture: `tests/replay/test-zone-entry.vpk`. 2 fires (one C>S + one S>C, matching the XML's "both" direction).
- Method: `--dump-payload 0x9148:`. Both fires 100 bytes — exactly `sizeof(zoneChangeStruct)`.
- Sample bytes: starts with `"<charname>\0"` at offset 0, then a Windows TZ-resource string fragment (`"t.z.r.e.s...d.l.l.,.-.2.1.1."` — Test's wire leaks UTF-16 from the client's locale resource), then sentinels and IEEE-754 floats at offsets 0x4c-0x58 (`-61.0, -160.0, 17.0` — destination position).
- Struct fit: 100 = sizeof(zoneChangeStruct). The leading player name + destination floats + zone metadata are unmistakable.
- Ruled out: only 100b S>C/C>S opcode in the capture; size and content unambiguous.

### 2026-05-04 — OP_RespondAA = 0xfb0a
- Capture: `tests/replay/test-zone-entry.vpk`. 53 fires.
- Method: `--dump-payload 0xfb0a:`. Variable S>C, sizes 1000-17007 bytes.
- Sample bytes: payload header (16 ASCII zeros + sentinels) followed by AA grant text: `"Frenzy of Conquest 1 Benefit"`, `"Veng/Drum/ImpDam/BA Benefit I"`, `"Improved Damage 01"`, etc. — the per-spend AA grant flavor matching OPCODES_LIVE_TODO.md's "OP_RespondAA — per-spend response carrying the player's full AA list".
- Struct fit: `uint8_t` variable; payload is the player's full purchased-AA roster.
- Ruled out: 0xce3d (175x same shape) is the *static menu* of all definitions; 0xfb0a fires far less and the strings are grant-flavored ("of Conquest", "Benefit I/II/III") rather than ability-definition flavored.

### 2026-05-04 — OP_MovementHistory = 0x9e21
- Capture: `tests/replay/test-zone-entry.vpk`. 132 fires.
- Method: `--dump-payload 0x9e21:`. Variable C>S, sizes 18 / 35 / 52 / 273 / 426 / 2500 bytes.
- Sample bytes (fire 1, 18b): `00 00 90 41  00 00 1f 43  00 00 d0 c1  03 2d ff 00 00 00` — three IEEE-754 floats `18.0, 159.0, -26.0` followed by spawn-id-like trailer. Subsequent larger fires show repeated 12-byte (xyz) blocks accumulating a movement path.
- Struct fit: `uint8_t` (client, variable) per zoneopcodes.xml. Variable size + ASCII-recognizable position floats are the OP_MovementHistory signature (the legacy `0xdc5d` Live ID was confirmed via the same pattern in 2026-05-01).
- Ruled out: 0xf8d1 (234x C>S fixed 42b with player spawn-id + sequence number) was the closest competitor at first glance, but its fixed size + sequence-counter layout doesn't fit MovementHistory's variable accumulating-path semantics. 0xf8d1 is a different per-tick client broadcast (status / autorun heartbeat), out of 73-list scope.

### 2026-05-04 — OP_DeleteSpawn = 0x6dba
- Capture: `tests/replay/test-zone-entry.vpk`. 15 fires.
- Method: `--dump-payload 0x6dba:`. All fires S>C, all 4 bytes.
- Sample bytes: `63 2d 02 0a`, `67 2d 5c 0a`, `5d 2d 08 0a` — varying spawn-id prefixes (0x2d63, 0x2d67, 0x2d5d) per fire = different mobs being despawned as the player moves through tutoriala/tutorialb.
- Struct fit: `deleteSpawnStruct` is 4 bytes (u32 spawnId historically; Test appears to use u16 spawnId + 2 trailer bytes). Direction "both" per XML; 15 fires S>C-only here = the server informing the client of mob despawns.
- Ruled out: removeSpawnStruct=5b (already taken by OP_RemoveSpawn=0xeb88); itemPacketStruct=4b (would carry item data, not spawn-id-like values); clientTargetStruct=4b (C>S target-set, wrong direction).

### 2026-05-04 — OP_NpcMoveUpdate = 0x917c
- Capture: `tests/replay/test-zone-entry.vpk`. 108 fires across the tutoriala/tutorialb session.
- Method: `--dump-payload 0x917c:`. Variable size 15/17/18 bytes S>C.
- Sample bytes: each fire starts with a 4-byte spawn-id (LE u32) followed by bit-packed motion deltas. Spawn-ids vary across fires (`0x5d2d`, `0x6a06`, `0xeb05`, …) — i.e. it's tracking many NPCs, not one. Tail length varies with how many delta-fields are populated.
- Struct fit: matches `uint8_t` (server, variable) per zoneopcodes.xml. Variable size + per-NPC spawn-id prefix is the OP_NpcMoveUpdate signature.
- Ruled out: 0x652f at S>C 18b (also 18-byte fixed) was rejected as the HPUpdate winner — its bytes match `hpNpcUpdateStruct` exactly. 0x917c's 99-fires-at-18b were the misleading lure; once size variability and the per-NPC prefix were obvious, NpcMoveUpdate fit.

### 2026-05-04 — OP_SendAATable = 0xce3d
- Capture: `tests/replay/test-zone-entry.vpk`. 175 fires.
- Method: `--dump-payload 0xce3d:`. Variable huge S>C sizes (1006-16904 bytes).
- Sample bytes: payload header followed by ASCII ability descriptions in plaintext: `"Benefit: Mana Regeneration 01"`, `"Benefit: Vengeance 10"`, `"Benefit: Improved Damage 01"` (one per fire). The repeated `"Benefit: …"` string per packet is the unmistakable AA-menu signature; OP_SendAATable carries the static ability definitions, one ability per packet, ~175 abilities → 175 fires.
- Struct fit: `uint8_t` (variable) per zoneopcodes.xml.
- Ruled out: 0xfb0a (53x S>C similar huge size) ASCII anchors are also AA-related ("Frenzy of Conquest 1 Benefit") but those are *purchased* AA grants, fitting OP_RespondAA's per-spend semantics rather than the static menu.

### 2026-05-04 — OP_NewZone = 0xa923
- Capture: `tests/replay/test-zone-entry.vpk`. 2 fires (one per zone-in: `tutoriala` then `tutorialb`).
- Method: `--dump-payload 0xa923:`. 338 bytes S>C each fire.
- Sample bytes: starts with `74 75 74 6f 72 69 61 6c 61 00` = `"tutoriala\0"` then `"The Mines of Gloomingdeep\0"` then a duplicated short-name and zone metadata.
- Struct fit: payload contains zone short name + long name + metadata — definitively newZoneStruct on Test (layout differs from Live `newZoneStruct` where shortName is at offset 64 after `name[64]`; on Test the leading character-name field is gone and shortName is at offset 0).
- Verified via daemon log: replay now reports `loaded map for zone 'tutoriala'` and `loaded map for zone 'tutorialb'` as expected — the daemon's ZoneMgr handler triggers on the resolved opcode and reads the short-name correctly because Test happens to put it where the existing parser already looks (post-`name[64]` skip lands inside the unknown trailing bytes — coincidence or layout change). Worth a closer parser audit if any zone-related downstream behavior is off.
- Ruled out: no other 338b S>C 1-2 fire opcode in the capture; the ASCII zone names are unambiguous.

### 2026-05-04 — OP_PlayerProfile = 0xe284
- Capture: `tests/replay/test-zone-entry.vpk`. 2 fires (zone-in tutoriala + zone-in tutorialb), sizes 23391b and 23331b respectively (variable).
- Method: `--dump-payload 0xe284:`. Searched all candidate huge-S>C zone payloads for the player name; only 0xe284 contained `"<charname>\0"` (at offset 20427).
- Anchor evidence:
  - At offset 20 (0x14): u32 = 6 = race (Erudite, plausible for a Necromancer).
  - At offset 24 (0x18): u32 = **11 = Necromancer class** ✓
  - At offset 28 (0x1c): u8 = 1 in fire 1, u8 = 2 in fire 2 = **level field tracks the user's lvl 1 → 2 ding**.
  - Same `0x69f94e9c` PC-specific u32 we saw in the spawnStruct trailing-block also appears inside the profile payload (cross-opcode self-consistency).
- Struct fit: charProfileStruct on Test, layout shifted from Live (player name moved to ~offset 20427 instead of the legacy offset 4). The daemon's `fillProfileStruct` is called with `checkLen=false` so no warnings will fire even if parser-vs-wire layout has drifted; if downstream "char information is not showing up" persists after this XML fix, the parser itself needs an audit (start with the offsets I listed above).
- Ruled out: 0x007d (133KB, 1 fire) was the largest 1-fire S>C zone packet but contains no ASCII strings — likely a different big bulk packet (spell book / item cache / similar, out of 73-list scope).

### Pending: legacy-comment ambiguity
- Both `OP_SendExeChecksum` and `OP_SendBaseDataChecksum` carry the comment "Second client verification packet" in legacy `worldopcodes.xml` — likely a copy-paste from when those names were first added. It does NOT change the disambiguation above (Exe is uniquely 64b, BaseData is uniquely the middle 2056b in fire order), but worth flagging for future reviewers.

### 2026-05-04 — OP_FormattedMessage = 0x0ecf
- Capture: `tests/replay/test-zone-entry.vpk`.
- Method: `--dump-payload 0x0ecf:`. 9 fires, all 13 bytes S>C.
- Sample bytes (fires 1–3): `63 2d 00 00 70  17 00 00 00  01 00 00 00  00`, etc. (only the unknown0001[4] byte at offset 4 varies; messageFormat and messageColor are stable across fires.)
- Struct fit: formattedMessageStruct{unknown0000=0x63, unknown0001[4], messageFormat=0x17=23, messageColor=1, messages=∅}. 13b = the struct's fixed prefix; the variable `messages` tail is empty in these fires (zero-length notification messages). Format 23 + ChatColor 1 are consistent across all 3 inspected fires.
- Ruled out: simpleMessageStruct (12b) has the wrong size; specialMessageStruct (23b) has the wrong size; channelMessageStruct (~2188b) has the wrong size.

### 2026-05-05 — OP_ManaChange = 0x08fb
- Capture: `tests/replay/test-combat.vpk`. 25 fires, all 20 bytes S>C.
- Method: `--dump-payload 0x08fb:`. Decoded all 25 fires as `<i5` (5 little-endian int32s).
- Sample fires (selected):
  - fire 1:  newMana=61  field2=41  spellId=32790 (0x8016, zone-in seed sentinel)  off12=0  off16=-1
  - fire 2:  newMana=51  field2=41  spellId=288   off12=0  off16=-1
  - fire 9:  newMana=39  field2=41  spellId=341   off12=0  off16=-1
  - fire 14: newMana=56  field2=62  spellId=341   off12=0  off16=-1
  - fire 21: newMana=74  field2=63  spellId=346   off12=0  off16=-1
- Struct fit: 20 = sizeof(manaDecrementStruct). newMana walks like a real lvl 1-3 Necromancer mana pool (range 26-75, decrements per cast, regen ramps back up between fires). spellId field cycles through 7 distinct early-Necro spell IDs (288, 338-346, 502 — Lifetap/Cavorting Bones/etc.). Field-2 (legacy "unknown / Looks like endurance") is **max mana** on Test, growing in 3 phases: 41 (fires 1-13) → 62 (fires 14-20) → 63 (fires 21-25), corresponding to in-session level-ups.
- Ruled out competitors at S>C 20b: 0x1d3e (43x, leading -1/0 sentinels with no mana-pool-shaped first u32); 0xb87e (16x, leading u64=0 with small varying counter, not cast-correlated); 0x6805 (14x, mostly all-zero payload, mixed 16/20b sizes — variable, doesn't fit fixed-20b struct).
- Note: legacy struct comment for `manaDecrementStruct.unknown` ("Looks like endurance but not sure why that'd be reported here") is wrong — the field is max mana on Test. Worth a struct-comment refinement in `everquest.h` as a follow-up split commit.

### 2026-05-05 — OP_SimpleMessage = 0x098d
- Capture: `tests/replay/test-combat.vpk`. 12 fires, all 12 bytes S>C.
- Method: `--dump-payload 0x098d:`. Decoded as `<3I` (3 little-endian u32).
- Sample fires:
  - fire 1:  format=2553  color=1  field3=10945
  - fire 2:  format=2553  color=0  field3=208
  - fire 3:  format=2601  color=1  field3=3
  - fire 5:  format=2601  color=1  field3=9
  - fire 9:  format=2686  color=1  field3=10945
  - fire 11: format=2702  color=1  field3=10945
- Struct fit: 12 = sizeof(simpleMessageStruct{messageFormat, messageColor, unknown}). 4 distinct format IDs across 12 fires (2553, 2601, 2686, 2702) all sit in the eqstr message-format range (just above the older eqstr_us.txt's 2459 ceiling — Test's eqstr table has been extended). Color cycles 0/1 = ChatColor enum values. Fires arrive in pairs (color=1 then color=0) per event — the SimpleMessage broadcast pattern where each system event emits a primary-colored line plus a default-colored variant.
- Ruled out competitors at S>C 12b: 0x14dc (100x heartbeat with constant `3b 60 f9 69` session-handle tail and counter); 0xa139 (40x static `(-9191, 0, 0)` heartbeat, identical bytes every fire); 0x92a5 (40x static `(0, 1, 42)` heartbeat, identical bytes every fire); 0xab5e already taken (OP_ClickObject). 0x098d is the only 12b S>C opcode in the capture with a varying format-id-shaped first u32.

### 2026-05-05 — OP_GroupFollow = 0x01dc
- Capture: `tests/replay/test-group-invite.vpk` (2 invite/accept/disband cycles + tutorial NPC re-additions).
- Method: `--dump-payload 0x01dc:`. 5 fires, all 68 bytes S>C.
- Sample bytes: each fire has a NUL-terminated character name at offset 0 (16 bytes), then 8 bytes of session-handle / timestamp, then zeros, then 16 bytes of pointer-shaped trailing data (Test client memory leaks into the trailer, same pattern seen in OP_ZoneChange's UTF-16 locale-resource leak). Names redacted; values match the inviter, invitee, and self at the expected event boundaries.
- Struct fit: 68 = the daemon's pre-existing "modern: 68 bytes, name at offset 0" hint for groupFollowStruct, which the parser already expects. Names are at offset 0, not at the legacy offset 64 — Test layout drops the legacy `unknown0000[64]` placeholder and writes the invitee name straight into offset 0.
- Ruled out: only 68b S>C in the capture.

### 2026-05-05 — OP_GroupInvite = 0x67a4 (and OP_GroupInvite2 = 0xee64)
- Capture: `tests/replay/test-group-invite.vpk`.
- Method: `--dump-payload`. Each opcode fires once at 176 bytes; the user issued one outgoing invite and received one incoming invite delivery in this session.
- 0x67a4 (1 C>S 176b): payload starts with a NUL-terminated target name (the invitee — redacted). The user's outgoing invite carries the *target* name at offset 0.
- 0xee64 (1 S>C 176b): payload starts with a NUL-terminated recipient name (self — redacted). The server's invite delivery carries the *recipient* name at offset 0.
- Struct fit: 176 = modern groupInviteStruct on Test (the legacy 152b layout has been padded by 24 bytes; trailer is zeros). Names are at offset 0, not at the legacy offsets 0/64 for inviter+invitee — Test split the bidirectional opcode into two direction-specific IDs and reduced each to a single name.
- Note: the legacy XML's OP_GroupInvite vs OP_GroupInvite2 distinction was ungrouped-vs-grouped inviter state. On Test the split is by direction (C>S vs S>C). For the 73-list, OP_GroupInvite (0x67a4) is the canonical user-action ID; OP_GroupInvite2 (0xee64) is recorded as the S>C delivery half so the daemon's dir=both routing covers both wire opcodes.
- Ruled out: only 176b opcodes in the capture; both have plaintext char names at offset 0.

### 2026-05-05 — OP_GroupUpdate = 0xccfc
- Capture: `tests/replay/test-group-invite.vpk`. Cumulative across the 2 zone-session stats blocks: 6 fires S>C, all 92 bytes.
- Method: `--dump-payload 0xccfc:`. Layout: name@offset 0 (64 bytes), then 4 bytes session/timestamp, then `<u32 spawn-id> <u32 sentinel=-1> <u32 flag> <u32 small int>` slot-payload. Some fires have an empty name@0 (slot empty) and some have self-name (active slot).
- Struct fit: 92 = the daemon's pre-existing "modern: 92 bytes, recipient name + slot" hint for OP_GroupUpdate. Slot index visible in the trailing u32 pair, name in the leading 64b field.
- Ruled out: only 92b S>C opcode in the capture.

### 2026-05-05 — OP_GroupDisband / OP_GroupDisband2 = 0x8a85 / 0x2c76
- Capture: `tests/replay/test-group-invite.vpk`. The user self-disbanded from both group cycles, so OP_GroupDisband (self-disband notification) should fire twice and OP_GroupDisband2 (peer-disbanded notification) should also fire twice — once per cycle, once for self-confirm and once for the peer-leave broadcast.
- Method: `--dump-payload`. Both opcodes fire 2 times S>C at 168 bytes — the daemon's pre-existing "modern: 168 bytes; membername at offset 64" hint matches.
- 0x8a85 (2 S>C 168b): name@offset 64 = self in BOTH fires → fits the daemon's "membername=self at offset 64" comment for OP_GroupDisband (self-disband self-confirm).
- 0x2c76 (2 S>C 168b): name@offset 64 = peer name (the other group member) in BOTH fires — different peer per cycle → fits "membername=peer at offset 64" for OP_GroupDisband2 (peer-leave broadcast).
- Struct fit: 168 = modern groupDisbandStruct (legacy was 152b; Test added 16 trailer bytes). The split (self vs peer name in the same offset) is what distinguishes the two opcodes here — same struct, different opcode IDs, different fill semantics.
- Ruled out competitors at S>C 168b: 0x4396 (2 fires with BOTH names populated — different shape, likely a different group-related event); 0x7324 / 0xb269 (2 fires each but with denser non-zero content beyond just a name — likely buff-class 168b struct, since OP_Buff was already confirmed at 0x3b54 with the same size).
- OP_GroupLeader (80b S>C) deferred — no leadership change happened in this session (each cycle's inviter was leader by default; no /makeleader was issued), so 0xe005 (2 S>C 80b with float/pointer-shaped payload) is the leading 80b candidate but doesn't carry a name and doesn't pattern-match a leader-change broadcast. Needs a session with explicit /makeleader.

### 2026-05-05 — OP_Find = 0x695f
- Capture: `tests/replay/test-group-invite.vpk` (the user opened the Ctrl+F find window and issued 2 finds with cooldown wait between them).
- Method: `--dump-payload 0x695f:`. 4 fires S>C, sizes 831 / 86 / 86 / 86.
- Anchor evidence: fire 2 (831 bytes) contains an array of plaintext tutorial NPC name + find-category pairs — "Arias / Revolt Leader", "Frizznik / Tradeskills", "Prathun / Grouping and Communication", "Rytan / Spells", "Xenaida / Maps", "McKenzie the Younger / Augmentations", "Scribe Farquard / Achievements", and ~10 more — exactly the modern Ctrl+F find window's category-based finder list for the Mines of Gloomingdeep tutorial zone. The 3 smaller 86-byte fires are find-action responses (cooldown denial + find acknowledgements).
- Struct fit: variable per the legacy XML (`uint8_t (server, variable)`). The 830-byte list-on-open + small per-action fires layout matches the modern find window's two-phase contract (initial list, then per-find responses).
- Ruled out: 0x312a (6 S>C variable 59-179b) — wider size variance, doesn't have NPC-name strings; 0x5b40 (3 S>C 50-51b) — too narrow size band, no plaintext anchors; 0xed85 (5 C>S 40b fixed) — wrong direction (find list is server-pushed). 0x695f is the only S>C variable opcode in the capture with plaintext find-category labels.

### 2026-05-05 — OP_Animation = 0xdd87
- Capture: `tests/replay/test-group-invite.vpk` (user issued /sit /stand /em wave plus jumps; ambient tutorial NPCs idle).
- Method: `--dump-payload 0xdd87:`. 31 fires S>C, all 6 bytes. Cross-referenced against test-combat.vpk: same opcode fires 355 times in a combat session — fits per-melee-swing animation broadcasts on Live.
- Layout: `<u32 spawn-id> <u16 anim-code>`. Spawn-ids cycle through PCs and nearby NPCs. Anim codes have high entropy across fires: 0x10, 0x1d, 0x32, 0x35, 0 — different animations being broadcast (sit/stand/wave/idle/walk variants).
- Struct fit: `uint8_t` (variable per legacy XML); Test uses a fixed 6-byte form. Size grew from legacy's 4-byte form (legacy comment mentioned "4 bytes"). Updated the daemon's XML comment with the modern shape.
- Ruled out competitors at S>C 6b: 0xc57c (5 fires, codes mostly 0 or 100=0x64 — low-entropy percentage broadcast, likely a status pulse); 0x9dbe (3 fires, same low-entropy percentage shape); 0x8271 / 0xa3e0 (2 fires each, similar shape but only 2 samples). Only 0xdd87 has the varying anim-code entropy pattern AND scales 10× from idle session to combat session, which is the OP_Animation signature.
- Note: in this capture only S>C fires are observed; the C>S half of `dir=both` from the user's `/em wave` action wasn't captured — Test may route the user's own animation through OP_SpawnAppearance (stance field) and only broadcast OP_Animation server-side. Worth re-verifying when a more controlled emote-only capture is available.

### 2026-05-05 — OP_GroupLeader = 0xb269
- Capture: `tests/replay/test-char-leader.vpk` (<charname> invited <charname>, made <charname> leader, <charname> made <charname> leader, then <charname> made <charname> main tank).
- Method: `--dump-payload 0xb269:`. 3 fires S>C, all 168 bytes.
- Anchor evidence: name@offset 64 reads as the *new leader* in the right order across fires:
  - fire 1: name@64 = self (initial leader broadcast on group formation, since the user was the inviter)
  - fire 2: name@64 = peer (after the first /makeleader changed leadership to the peer)
  - fire 3: name@64 = self (after the peer's /makeleader handed leadership back)
- Struct fit: 168b matches groupDisbandStruct's modern shape (the daemon hint said 80b but the real Test layout is 168b — same family as OP_GroupDisband / OP_GroupDisband2). Updated XML comment with the corrected size + offset.
- Cross-capture check: in test-group-invite (no explicit /makeleader), 0xb269 fired 2 times — once per group cycle, broadcasting the implicit leader-on-formation. The +1 fire in test-char-leader matches the 2 explicit /makeleader events minus the second cycle absent here.
- Ruled out: 0xe005 (4 S>C 80b) was the 80b candidate suggested by the legacy XML hint, but 3 of its 4 fires are nearly all-zero with a constant `4f 34 8b ff` prefix and IEEE-754 1.0f at offset 4 — looks like a periodic group-state heartbeat, not a name-bearing leader broadcast. 0x6fbb (2 S>C 80b with self-name at offset 0) fires once per zone-in / character switch, also not leader-shaped. 0x933a (1 C>S + 1 S>C 168b with target+source names) is the /maketank round-trip — same struct family, different opcode.
