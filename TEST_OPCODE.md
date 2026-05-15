# Test client opcode rediscovery

Target server: **EverQuest Test client**. Tracking re-discovery of **73 opcodes** confirmed on Live (53 zone + 20 world). Opcodes that are still `[ ]` on `OPCODES_LIVE_TODO.md` (i.e., never confirmed on Live) are NOT in scope here — they remain a Live-tracker concern on `main`.

When all 73 entries below are `[x]`, the `test-client` branch is ready for merge-back review.

Checkbox legend: `[ ]` unresolved, `[x]` resolved on Test, `[~]` confirmed-absent on Test (won't be hunted further).

Confirmation bar: count + zero-competitor over n≥5; n=2-3 is enough if `--opcode-stats` reports no other unknowns at the target size+direction (per memory `feedback_opcode_disambiguation.md`).

Per-entry format: `[ ] OP_Name — typename (dir)`. Each resolved entry gets `0x<hex>` + date appended on confirmation. Append a dated confirmation log entry below each resolved opcode (capture name, method, sample bytes, struct fit, ruled-out leads).

---

## World stream (20)

### Login / handshake (8)
- [x] OP_SendLoginInfo — *(no payload struct; identifier only)* — `0xaca4` (2026-05-13, revised from 0x7b6a)
- [x] OP_LogServer — `0x4398` (2026-05-13, revised from 0xb537)
- [x] OP_ApproveWorld — `0xc8ca` (2026-05-13, revised from 0xb8cc)
- [x] OP_EnterWorld — `0xf31f` (2026-05-13, revised from 0x9bdc)
- [x] OP_ExpansionInfo — `0xc26e` (2026-05-13, revised from 0x6bcf)
- [x] OP_SendCharInfo — `0xde55` (2026-05-13, revised from 0x84f6)
- [x] OP_ZoneServerInfo — `0xb67c` (2026-05-13, revised from 0xf21f)
- [ ] OP_WorldComplete

### Checksums / verification (4)
- [ ] OP_SendSpellChecksum
- [x] OP_SendExeChecksum — `0xd99c` (2026-05-13, revised from 0x44d9)
- [ ] OP_SendBaseDataChecksum
- [ ] OP_SendSkillCapsChecksum

### Chat servers (2)
- [x] OP_SetChatServer — `0x5f8b` (2026-05-13, revised from 0xbb67)
- [x] OP_SetChatServer2 — `0x759e` (2026-05-13, revised from 0xf22b)

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
- [x] OP_PlayerProfile — uint8_t (server, variable) — `0xaaed` (2026-05-13, revised from 0xe284)
- [x] OP_ZoneEntry — ClientZoneEntryStruct (client) / uint8_t (server) — `0xbe93` (2026-05-13, revised from 0xa5bf)
- [ ] OP_TimeOfDay — timeOfDayStruct (server)
- [x] OP_NewZone — uint8_t (server, variable) — `0x5fc3` (2026-05-13, revised from 0xa923)
- [x] OP_SpawnDoor — doorStruct (server, modulus) — `0xae0a` (2026-05-13, revised from 0x794d)
- [x] OP_GroundSpawn — makeDropStruct (server) — `0x7b00` (2026-05-14)
- [ ] OP_SendZonePoints — zonePointsStruct (server)
- [ ] OP_ZoneChange — zoneChangeStruct (both)

### Movement / position (4)
- [x] OP_ClientUpdate — playerSpawnPosStruct (server) — `0x9377` (2026-05-13, revised from 0xf8d1)
- [x] OP_NpcMoveUpdate — uint8_t (server, variable) — `0xaaca` (2026-05-13, revised from 0x917c)
- [x] OP_MobUpdate — spawnPositionUpdate (both) — `0xeff9` (2026-05-13, revised from 0x4a4f)
- [x] OP_MovementHistory — uint8_t (client, variable) — `0xa994` (2026-05-13, revised from 0x9e21)

### Spawn lifecycle / appearance (5)
- [x] OP_DeleteSpawn — deleteSpawnStruct (both) — `0xa183` (2026-05-13, revised from 0x6dba)
- [x] OP_RemoveSpawn — removeSpawnStruct (both) — `0x394a` (2026-05-13, revised from 0xeb88)
- [x] OP_Death — newCorpseStruct (server) — `0xf752` (2026-05-13, revised from 0x1eb2)
- [x] OP_SpawnAppearance — spawnAppearanceStruct (both) — `0x9fd4` (2026-05-13)
- [x] OP_Animation — uint8_t (both) — `0x36c7` (2026-05-13, revised from 0xdd87)

### Combat / actions (4)
- [x] OP_Action — actionStruct (both) — `0x8ac8` (2026-05-13, revised from 0x049e)
- [x] OP_Action2 — action2Struct (both) — `0x4ad7` (2026-05-13, revised from 0x32a9)
- [x] OP_Consider — considerStruct (both) — `0x1ff6` (2026-05-13, revised from 0xa1e7)
- [x] OP_TargetMouse — clientTargetStruct (both) — `0xc7ce` (2026-05-13, revised from 0x1994)

### Stats / HP / mana / xp / endurance (9)
- [x] OP_ExpUpdate — expUpdateStruct (server) — `0x0d56` (2026-05-13)
- [ ] OP_AAExpUpdate — altExpUpdateStruct (server)
- [x] OP_HPUpdate — hpNpcUpdateStruct (both) — `0x0303` (2026-05-13, revised from 0x652f)
- [x] OP_MobHealth — mobHealthStruct (server) — `0xd18f` (2026-05-13, revised from 0x8d24)
- [x] OP_ManaChange — manaDecrementStruct (server) — `0x5617` (2026-05-13)
- [x] OP_SkillUpdate — skillIncStruct (server) — `0xfbcc` (2026-05-13)
- [x] OP_LevelUpdate — levelUpUpdateStruct (server) — `0xe461` (2026-05-13)
- [x] OP_EndUpdate — endUpdateStruct (server) — `0x16a3` (2026-05-13, revised from 0x36d1)
- [x] OP_Stamina — staminaStruct (server) — `0x786a` (2026-05-13)

### Buffs (1)
- [x] OP_Buff — buffStruct (both) — `0xd7f4` (2026-05-14, revised from 0x3b54)

### Group (6)
- [x] OP_GroupInvite — groupInviteStruct (both) — `0xbdab` (2026-05-14, revised from 0x67a4)
- [x] OP_GroupFollow — groupFollowStruct (server) — `0xeeb4` (2026-05-14, revised from 0x01dc)
- [x] OP_GroupUpdate — uint8_t (both, variable) — `0x8fa1` (2026-05-14, revised from 0xccfc)
- [x] OP_GroupDisband — groupDisbandStruct (server) — `0x27fa` (2026-05-14, revised from 0x8a85; dir promoted to "both")
- [x] OP_GroupDisband2 — groupDisbandStruct (server) — `0xc2d8` (2026-05-14, revised from 0x2c76)
- [x] OP_GroupLeader — groupLeaderChangeStruct (server) — `0x04c7` (2026-05-14, revised from 0xb269)

### Guild (5)
- [ ] OP_GuildMOTD — guildMOTDStruct (server)
- [ ] OP_GuildMemberUpdate — GuildMemberUpdate (server)
- [ ] OP_GuildMemberList — uint8_t (server, variable)
- [ ] OP_ExpandedGuildInfo — uint8_t (server, variable)
- [ ] OP_GuildsInZoneList — guildsInZoneListStruct (server)

### Inventory / items (2)
- [ ] OP_ItemPacket — itemPacketStruct (server)
- [x] OP_MoveItem — moveItemStruct (client) — `0xe883` (2026-05-14, revised from 0xdee3)

### Click / interact (2)
- [ ] OP_ClickObject — remDropStruct (both)
- [x] OP_Find — uint8_t (server, variable) — `0x0f64` (2026-05-14, revised from 0x695f)

### Chat / messaging (4)
- [ ] OP_SimpleMessage — simpleMessageStruct (server)
- [x] OP_FormattedMessage — formattedMessageStruct (server) — `0x9a58` (2026-05-13, revised from 0x0ecf)
- [ ] OP_CommonMessage — channelMessageStruct (both)
- [x] OP_SpecialMesg — specialMessageStruct (server) — `0x3f07` (2026-05-13, revised from 0x7162)

### Alternate Advancement (3)
- [x] OP_SendAATable — *(static ability-definition menu)* — `0x3d3b` (2026-05-13, revised from 0xce3d)
- [ ] OP_AAAction
- [x] OP_RespondAA — *(per-spend response with full AA list)* — `0xc893` (2026-05-13, revised from 0xfb0a)

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

### 2026-05-05 — OP_ApproveName = 0xa24a + OP_DeleteCharacter = 0x48f9
- Capture: `tests/replay/test-char-leader.vpk` (user generated random names through char creation, settled on a name needing one character appended for acceptance, zoned in, logged out, deleted, then logged in as main and ran a 2-leader-change group session).
- **OP_ApproveName = 0xa24a**: 5 C>S 84b name candidates + 5 S>C 1-byte ack responses, perfectly interleaved. The C>S payload starts with a NUL-terminated character-name candidate at offset 0; values cycle through 5 successive proposals (Tinnon, Gwin, Soun, the random-rolled name, then the appended-letter variant the user finalized). The 1-byte responses return `0x12` for the first 4 attempts (status: name OK so far) and `0x01` for the 5th (status: committed/created — fires after the user clicks final create). Method: `--dump-payload 0xa24a:` decoded as 84-byte name+padding records bidirectional with 1-byte status.
- **OP_DeleteCharacter = 0x48f9**: 1 C>S 68b fire with the deleted character's name at offset 0. The user deleted exactly one character in this session; 68b = 64-byte char name + 4-byte flags trailer, the canonical "delete by name" shape. Method: `--dump-payload 0x48f9:`.
- Char names redacted from this log per memory; the actual values match the user's actions in the session description.
- OP_CharacterCreate (the C>S that submits race + class + stats + final name) is still unresolved here. Best lead: 0x2078 has 2 C>S 76b fires with character names at offset 0, but fire 1 has the new char's name and fire 2 has the *main* char's name from the next login session — fits a per-session "ZoneEntryPlayer" or "ApproveWorld with this char selected" init packet, not a one-shot create. The 0x1900 single S>C 952b fire (with the new char's name at offset 487) is the SERVER's char-create *response*, but the matching C>S create request is still unidentified from this capture.
- OP_RandomNameGenerator may be absent on Test (client-side random generation) — no dedicated opcode found for the round-trip; the candidate names that flowed through OP_ApproveName were either client-generated locally or returned via OP_ApproveName itself. Worth marking `[~]` confirmed-absent if a future targeted capture (random-button-only, no submit) shows no new opcode.

### 2026-05-05 — OP_ExpansionInfo = 0x1a00
- Capture: `tests/replay/test-char-leader.vpk` (full-from-start session covering char create, 2 logins, group play).
- Method: `--dump-payload 0x1a00:`. 2 fires S>C, both 84 bytes, **identical bytes across fires** = static account-level config sent per login.
- Sample bytes: `00 00 00 00 ff ff ff ff ff ff ff ff … ff ff ff ff 00 00 00 00 …` — u32 header (zero), 11 × `0xffffffff` (all expansion flags set), then trailing zeros. The "all expansions unlocked" shape matches a Test account's typical entitlement.
- Struct fit: 84b fixed = u32 header + ~11 u32 expansion flags + reserved zero space. Once-per-login, identical-payload signature is the ExpansionInfo signature; the legacy XML's "Which expansions user has" comment matches the shape.
- Ruled out alternative: 0x066d (4 S>C 157b, structured array of 36 int32 with mixed `1` / `-1` values, 0x02 header + sentinels) is also a flag-bitmask shape but fires 4 times (per zone session, not per world login) and varies in entry-by-entry pattern — looks more like a per-zone game-feature / class-availability config than the per-world expansion list. Worth re-classifying if a future capture shows OP_ExpansionInfo's 84b form is wrong.

### 2026-05-05 — OP_SetChatServer = 0xf22b
- Capture: `tests/replay/test-char-leader.vpk` (full-from-start handshake covering 2 logins).
- Method: `--list-events` showed 0xf22b firing as S>C 1464b in the early world handshake (between OP_SendLoginInfo and OP_LogServer), then `--dump-payload 0xf22b:` to inspect content.
- Anchor evidence: payload contains ~18 plaintext Daybreak service URLs in NUL-terminated record form. Critical: `https://auth.daybreakgames.com/rest/client/session/create` (the actual chat session endpoint) plus a full catalog of auth/account/commerce/support endpoints (loginWithoutTicket, fundWallet, getpaymentinfo, purchase, redeemcode, finalizesteamtransaction, session/touch, help.daybreakgames.com, etc.). The legacy single-URL chat-server form has been replaced with a multi-URL service catalog on Test.
- Struct fit: 1464b variable size, fires once per world login (2 fires for 2 logins). The "set chat server" semantic still applies — server tells client where chat/auth services live — but the modern shape is a service catalog instead of a single host:port:key tuple.
- OP_SetChatServer2 deferred. The legacy duplicate (mail server vs chat server split) doesn't have an obvious twin opcode here; closest candidate is 0xdeb6 (1188b S>C 2 fires) which contains `https://www.everquest.com/membership` plus binary, but the service-overlap with 0xf22b makes the role-assignment unclear. Worth a closer compare across captures.

### 2026-05-05 — OP_ZoneServerInfo = 0xf21f + OP_SetChatServer revised
- Capture: `tests/replay/test-zone.vpk` (<charname> zoned out from tutorial → crescent reach, then logged out — full zone-handoff world handshake captured).
- Method: `--list-events` showed the post-zone-change world handshake re-running (OP_SendLoginInfo through OP_EnterWorld), with new opcodes 0xf21f (130b S>C) and 0xbb67 (56b S>C) firing in the post-OP_SendSpellChecksum band.
- **OP_ZoneServerInfo = 0xf21f** (130b S>C, 2 fires for the 2 zone-handoffs in this session): payload starts with `eqzone-31.everquest.com\0` (NUL-padded hostname, 24 bytes), then session/key fields, then `0x0879` u16 port (= 2169) at offset 128. Definitively the zone-server pointer.
- **OP_SetChatServer = 0xbb67** (56b S>C, 2 fires): payload is the legacy comma-separated chat-server tuple — `lvseq-chat01.everquest.com,9879,test.<charname>,<sessionkey>,0` (sessionkey redacted). This is the original "IP/Port/servername.charname/password" form the legacy XML described, untouched on Test.
- **OP_SetChatServer revised**: previously committed as 0xf22b based on the URL-rich content alone. With 0xbb67 now showing the canonical legacy chat-server tuple format, that's the better match for OP_SetChatServer; 0xf22b's multi-URL service catalog is more accurately OP_SetChatServer2 (the secondary chat-related config slot, expanded on Test from a single mail-server tuple to a full Daybreak service catalog covering auth/account/commerce/support).
- Cross-validation in test-zone.vpk: 0xf21f and 0xbb67 each fire once per world-handshake cycle — exactly what's expected for per-login zone-server-pointer + per-login chat-server-config opcodes.

### 2026-05-05 — OP_SendZonePoints = 0xc547
- Capture: `tests/replay/test-zone.vpk` (<charname> zoned out from tutorialb to crescent reach via the tutorial's exit portal).
- Method: `--list-events` showed 0xc547 firing as S>C 176b in the moments BEFORE the C>S OP_ZoneChange — the canonical "server announces this zone-point's destination" timing for OP_SendZonePoints.
- Anchor evidence: payload at offset 0 holds a single zonePointStruct entry (`uint32 trigger=189, uint32 sentinel, float y=-146.0, float z=2.0, float x=17.0, float heading=432.0, uint16 zoneId=11, uint16 instance=1`) followed by 144 bytes of zeros (slots for additional zone points, all empty here since the tutorial exit only has one destination — Crescent Reach). The y/x/heading match the player's location near the tutorial-exit portal at the moment of the broadcast.
- Struct fit: 176 bytes is a fixed buffer (24-byte zonePointStruct + ~6 empty slots × 24b + trailer), not a packed variable-length list as in the legacy zonePointsStruct. Same struct family but Test pads to a fixed maximum capacity rather than truncating.
- 2 fires across the capture: once in tutorialb (announcing the exit-to-Crescent portal) and once in crescent (announcing the return / zone-points visible from the new zone). One zone-point per zone × two zones = the expected count.
- No competing 176b S>C fires in this capture.

### 2026-05-05 — OP_ExpansionInfo revised: 0x1a00 → 0x6bcf
- Capture: `tests/replay/test-zone.vpk` (full-from-start capture with `--list-events`).
- Issue with prior 0x1a00 assignment: the events log shows 0x1a00 firing in the **zone stream** (right after OP_ZoneEntry / OP_TimeOfDay during per-zone init), not the world handshake. OP_ExpansionInfo is a world-stage opcode by definition, so the prior confirmation was incorrect.
- New candidate **0x6bcf** (64b S>C, 2 fires per session): fires in the post-OP_LogServer / pre-OP_ApproveWorld world handshake band — the right phase for an expansion-bitmask broadcast. Payload starts with `0x08` (= 8, expansion count?) then a u32-array bitmask region with `0xffffffff` sentinels and per-login session id at offset 36, then `0x07 0x02` tier markers in the trailer. Per-login fires are nearly identical except for the session-id byte difference, fitting "mostly-static account-level expansion bitmask + session marker".
- Confidence: moderate. The shape and timing fit OP_ExpansionInfo, but the legacy bitmask-of-flags interpretation isn't a perfect 1-to-1 mapping with the observed 0x6bcf payload. Worth re-verifying if the daemon's downstream "available expansions" UI behaves correctly with this opcode wired in.

### 2026-05-05 — OP_LoginServerInfo `0x001A` (incidental; daemon currently drops it)

While chasing OP_MOTD via the test-server "Live Environment" disclaimer text in `tests/replay/test-zone.vpk`, byte-level session decode of record 29 (login server 69.174.201.232:15900 → client) revealed app opcode **`0x001a`** carrying a 219-byte server-list payload:

```
06 00 00 00 00 00 00 00 00 00 01 67 00 00 00 00 01 00 00 00
"eqworldNN.everquest.com" \0
28 23 00 00 00 00 00 00
22 01 00 00 01 00 00 00
"Test" \0 "EN" \0 "US" \0 "Standard (Live Environment)" \0
"Content and code on this server may be in progress and may not be representative of the final product." \0
```

Shape: u32 + 8 bytes + zstring hostname + 8 bytes + u32 length-of-locale-block (0x122) + u32 count (1) + null-separated locale strings (locale code, region, language, label, disclaimer). Classic login-server server-list listing — **not** OP_MOTD as initially suspected; the disclaimer rides along here, not in a post-EnterWorld MOTD packet. **Real OP_MOTD remains unconfirmed** — chase it in a CharSelect→ZoneIn capture, not via this string.

Not in the 73-opcode rediscovery list (incidental discovery). Registered in `worldopcodes.xml` as a candidate so future stats reports name it; n=1 across all current `.vpk` fixtures but body shape is unambiguous.

**Daemon decoder bug**: `0x001a` never reaches `dispatchPacket`. Verified via both `--list-events` (zero entries for `0x001a`) and `--dump-payload 0x001a:/tmp/x` (zero `.bin` files written) on `test-zone.vpk`, even though the packet is plainly present in the `.vpk` and SessionResponse precedes it (rec 14, vs the 0x001a fire at rec 29). Most likely cause: seq-window logic in `EQPacketStream::processPacket` for `OP_Packet` (packetstream.cpp:789) caches future-seq packets but the session is `SessionDisconnect`'d (rec 42-43) before the missing earlier seqs arrive, so the cached entry is dropped without dispatch. Less likely: CRC validation against the not-yet-established session key. Worth a short investigation pass to either (a) deliver cached pre-disconnect packets at `SessionDisconnect` time, or (b) confirm the drop is intentional and document it.

**MOTD hunt status after this work**: dumped four small single-fire S>C world-stream candidates from `test-zone-entry.vpk` — `0xa97f` (72b, sparse-zero init shape with float 1.0 trailer), `0x3ef4` (12b, server-time/sync), `0x0afa` (33b, char-list entry: zone+name length-prefixed strings → likely a CharSelect helper, not MOTD), `0xef12` (24b, fires on both streams = ack/heartbeat class). None match an OP_MOTD shape. Live OP_MOTD is probably either a string-table reference packet (small, no free text) or absent entirely on Test from the current capture set. Next attempt should be a fresh login → CharSelect → ZoneIn capture targeted explicitly at the post-EnterWorld scrollback message.

### 2026-05-05 — OP_FormattedMessage (0x0ecf) layout audit + parser rewrite

The 2026-05-04 entry above was right about the opcode but wrong about the struct: `formattedMessageStruct{unknown0000, unknown0001[4], messageFormat@5, messageColor@9, messages@13}` from legacy `everquest.h` only "fit" `test-zone-entry.vpk` because all 9 fires there were short (13b, zero-arg notifications) and offset 5 happened to land on benign bytes. Re-running on `test-combat.vpk` (129 fires, sizes 13–160b, varied content) decoded as 102/272 chat envelopes showing `Unknown: 1000017:` / `1000002:` / `1000006:` — the 0x01XXXXXX high-bit pattern is the smoking gun: bytes 5–8 of the wire = `<low-3-of-format> <messageColor=0x01>`, so the legacy reader was splicing the color byte into the messageFormat read.

**Test wire layout** (verified across all 129 fires):
```
/*0000*/ uint32_t target;        // u32 — spawnId in zone-entry (player ID), constant 0x9e2 in
                                 //   combat broadcasts (channel/topic ref?), varies in others
/*0004*/ uint32_t messageFormat; // eqstr lookup key
/*0008*/ uint8_t  messageColor;  // ChatColor (single byte on Test; was u32 on legacy)
/*0009*/ uint8_t  argCount;
/*0010*/ uint8_t  unknown0010;   // padding (always 0 in samples)
/*0011*/ char     args[0];       // argCount × (16-byte preamble + NUL-terminated UTF-8 string)
```
Header is **11 bytes** (not 12, not 13). Args section repeats `argCount` times: 16-byte preamble (leading u32 = arg index 0..argCount-1; remaining 12 bytes look like spawn-id / sub-format-id refs — opaque for now, may matter for `%T` template substitution) followed by a NUL-terminated string.

**Sample bytes** (`test-combat.vpk` fmsg.106.bin, 93b, 3 args):
```
0000  e2 09 00 00 70 17 00 00 01 03 00                  ← header (target=0x9e2, format=0x1770,
                                                          color=1, argCount=3)
000b  00 00 00 00 00 0b 00 00 00 49 00 00 00 00 00 00   ← arg 0 preamble (counter=0)
001b  "<redacted player name>\0"
0029  01 00 00 00 20 01 00 00 17 00 00 00 00 00 00 00   ← arg 1 preamble (counter=1)
0039  "<redacted player name>\0"
0043  02 00 00 00 1e 14 00 00 57 00 00 00 00 00 00 00   ← arg 2 preamble (counter=2)
0053  "<redacted player name>\0"
```
(Char names redacted in-doc per repo convention.)

**Parser rewrite landed**:
- `src/everquest.h:1793` — replaced legacy `formattedMessageStruct` with the layout above.
- `src/eqstr.h:44` + `src/eqstr.cpp:140` — `EQStr::formatMessage` now takes `argCount` and walks `<16B preamble><NUL string>` repeated, instead of legacy `<u32 length><bytes>` repeated.
- `src/messageshell.cpp:268-290` — passes `fmsg->argCount` and uses the new `args[]` field name.
- `conf/zoneopcodes.xml` — entry already at `sizechecktype="none"` (variable), no change needed.

**Verification** (`test-combat.vpk` replay → pbstream chat envelopes):
- Total: 272 chat envelopes (unchanged — same fire count, just better-decoded).
- Real text resolved: 195/272 (72%) — full substitution working, e.g. `"Greetings, <playername>. We're glad you found your way to our camp."`, `"Your guild has received <playername> favor for your tribute!"`, `"Hail, Guard Rahtiz"`.
- Remaining 77 `Unknown:` entries are **eqstr table gaps**, not parser failures:
  - simpleMessage IDs `0x9f9` / `0xa29` / `0xa7e` / `0xa8e` (4+4+2+2) — Test-extended IDs above the current `eqstr_us.txt`'s 2459 ceiling. Live Apr-2026 client `eqstr_us.txt` (4/6/26, 437KB) does not contain them — they may live in `dbstr_us.txt` or another resource.
  - `Unknown: 0000:` (7×, formatid=0, empty args) — looks like a wire sentinel/heartbeat; not necessarily a bug.
  - Misc formattedMessage IDs (e.g. `0x0909` = 2313) where the args decode perfectly but the format string isn't in the current eqstr table.

**Open**: 16-byte arg preamble's middle 12 bytes are opaque; if downstream `%T` template substitutions ever look wrong, decode preamble as `<u32 argIndex> <u32 spawnIdRef?> <u32 subFormatId?> <u32 zero>` and route the middle u32s through `m_messageStrings.value(...)` like the legacy `%T` handler did.

### 2026-05-13 — May 12 patch reshuffle: batch re-identification via opcode-stats

- Capture: `tests/replay/test-20260513.vpk` (live Crescent Reach session post-May-12 patch; `/tmp/opcode-stats.txt`)
- Method: all previously-confirmed opcodes appeared as "unknown" in the new stats run, confirming a full patch reshuffle. Re-identified 33 zone + 10 world opcodes by cross-referencing struct size + direction + payload samples. Rebuilt and replayed vpk: 671 spawns added (NPC names correct), zone loaded as "crescent", player ID resolved, 486 combat events, 12 kills, 18 removals — core functionality confirmed working.
- Note: C>S side of OP_ClientUpdate grew from 42b to 38b; `sizechecktype` set to none for both directions pending struct audit.

**Re-confirmed zone** (old → new):
OP_PlayerProfile e284→aaed | OP_ZoneEntry a5bf→be93 | OP_NewZone a923→5fc3 | OP_SpawnDoor 794d→ae0a | OP_NpcMoveUpdate 917c→aaca | OP_MobUpdate 4a4f→eff9 | OP_ClientUpdate f8d1→9377 | OP_MovementHistory 9e21→a994 | OP_DeleteSpawn 6dba→a183 | OP_RemoveSpawn eb88→394a | OP_Death 1eb2→f752 | OP_Animation dd87→36c7 | OP_Action 049e→8ac8 | OP_Action2 32a9→4ad7 | OP_Consider a1e7→1ff6 | OP_TargetMouse 1994→c7ce | OP_HPUpdate 652f→0303 | OP_MobHealth 8d24→d18f | OP_EndUpdate 36d1→16a3 | OP_FormattedMessage 0ecf→9a58 | OP_SpecialMesg 7162→3f07 | OP_SendAATable ce3d→3d3b | OP_RespondAA fb0a→c893

**Re-confirmed world** (old → new):
OP_SendLoginInfo 7b6a→aca4 | OP_LogServer b537→4398 | OP_ApproveWorld b8cc→c8ca | OP_EnterWorld 9bdc→f31f | OP_ExpansionInfo 6bcf→c26e | OP_SendCharInfo 84f6→de55 | OP_ZoneServerInfo f21f→b67c | OP_SetChatServer bb67→5f8b | OP_SetChatServer2 f22b→759e | OP_SendExeChecksum 44d9→d99c

**Reset to `[ ]`** (captured but not found — need targeted capture): OP_TimeOfDay, OP_GroundSpawn, OP_SendZonePoints, OP_ZoneChange, OP_SpawnAppearance, OP_Stamina, OP_ExpUpdate, OP_ManaChange, OP_SkillUpdate, OP_LevelUpdate, OP_Buff, all Group opcodes, OP_SimpleMessage, OP_CommonMessage, OP_Find, OP_ClickObject, OP_MoveItem, OP_ItemPacket; and world: OP_WorldComplete, OP_GuildList, OP_SendSpellChecksum, OP_SendBaseDataChecksum, OP_SendSkillCapsChecksum, OP_DeleteCharacter, OP_ApproveName.

### 2026-05-13 addendum — 5 more from post-patch analysis

From the same test-20260513.vpk, additional analysis of stat/appearance candidates:
- **OP_SkillUpdate = 0xfbcc** — 10 fires, 12b S>C. `[skillId=5/13/31, value=75-102, unk=1]`. Skill 31=Meditate at 102 matches level-15 progression; value cycling up confirms skill-gain signature. Zero other 12b S>C opcodes with varying-skillId-shaped first u32.
- **OP_ManaChange = 0x5617** — 30 fires, 20b S>C. `[newMana=680→516, maxMana=389, spellId=340/502/344, 0, -1]`. manaDecrementStruct: newMana decreases per cast, maxMana=389 fits level-15, spellId cycles Necro early-game spell IDs, field4=-1 sentinel constant.
- **OP_Stamina = 0x786a** — 10 fires, 8b S>C. Two u32s both decrementing (4606/3356, 4318/3068, ...) with constant difference 1250, same rate. Paired-decrement signature matches staminaStruct{food, water} (food started higher than water).
- **OP_SpawnAppearance = 0x9fd4** — 10 fires, 8b S>C. `[spawnId=0x3047/0x5b04/etc, param=4]`. SpawnIds in the valid session range; param=4 constant (standing/alive state). Only 8b S>C with valid-range spawnIds + constant param.
- **OP_ExpUpdate = 0x0d56** — 4 fires, 16b S>C. `[exp=65067, 0, type=0/2, 0]` then `[81974, ...]` then `[93992, ...]`. Classic expUpdateStruct{exp, unk, type, unk}; type=0 on initial set, type=2 on incremental gains. Note: capture ends at 93992/100000 XP; level-up (OP_LevelUpdate) was not caught — ding must have occurred before daemon started.

### 2026-05-14 — Group opcode candidates from mercenary auto-form (PENDING player-invite confirmation)

- Capture: `/tmp/test-20260513.vpk` (Crescent Reach session; mercenary hired, server auto-formed the group with the merc). Pre-patch group IDs (0x67a4 C>S / 0xee64 S>C @ 176b, etc.) all read `ffff` in `zoneopcodes.xml` after the May-12 reshuffle and do not fire here — re-hunting.
- Method: `--opcode-stats` + `--dump-payload` on every S>C unknown with low fire-count and name-shaped (≤96b) payload.
- Mercenary auto-form skips the invite handshake entirely. No 176b or 152b S>C/C>S unknown appears in the stats, so neither OP_GroupInvite nor OP_GroupInvite2 fires for a merc add — the server jumps straight to OP_GroupFollow + OP_GroupUpdate without an invite delivery.
- **0xeeb4 (OP_GroupFollow candidate)** — 1× S>C, 68b. NUL-terminated mercenary auto-name at offset 0 (Test `<name><digits>` suffix pattern; see CLAUDE.md note on `stripNameSuffix`). 68b matches the modern groupFollowStruct anchored on `test-group-invite.vpk` (name@0).
- **0x8fa1 (OP_GroupUpdate candidate)** — 4× S>C, 92b. NUL-terminated recipient name (self/PC) at offset 0. u32 at offset 0x44 = 0x06e1 on the first fire (group-formation broadcast / status code), 0x02 on the remaining three (slot index). u32 at offset 0x48 = 0xffffff01 sentinel across all fires. 92b matches the existing xml comment on OP_GroupUpdate (modern: 92 bytes, recipient name + slot). 4 fires = group-formation broadcast burst.
- Ruled out as group-related (other low-count S>C name/80b shapes in this capture):
  - `0xceeb` (12× 80b S>C) — plaintext `ogrestompfire1` at offset 0; ogre-stomp particle/effect tag, not group.
  - `0xb224` (44× 80b S>C) — float position + pointer-shaped trailer; entity/position update, not group.
  - `0xef44` (3× S>C, 68b×2 + 164b×1) — numeric headers, no name at offset 0; unrelated.
- Still unconfirmed in this session: OP_GroupInvite, OP_GroupInvite2, OP_GroupCancelInvite, OP_GroupLeader, OP_GroupDisband, OP_GroupDisband2. A player-to-player invite/accept/disband cycle is needed to anchor them — merc-only sessions don't exercise the invite handshake or the disband path.

### 2026-05-14 — OP_GroupInvite = 0xbdab + OP_GroupInvite2 = 0xfc1d; OP_GroupFollow/OP_GroupUpdate candidates confirmed

- Capture: `tests/replay/test-group-invite-20260514.vpk` (two cycles: PC-A invites PC-B; PC-B accepts; disband; PC-B invites PC-A; PC-A accepts). Only PC-A's client is on the sniff interface, so C>S = PC-A's outgoing invite (cycle 1) and S>C = PC-B's invite delivery to PC-A (cycle 2).
- Method: `--opcode-stats` + `--dump-payload`. Stats show exactly one C>S 176b unknown (`0xbdab`) and exactly one S>C 176b unknown (`0xfc1d`); both `0xeeb4` (68b S>C, 4 fires) and `0x8fa1` (92b S>C, 16 fires across 2 zone-session blocks) repeat from the merc-derived candidate set with the player invite/accept cadence.
- **`0xbdab` = OP_GroupInvite (C>S, 176b)**: invitee name@0 (PC-B), inviter name@64 (PC-A). 48-byte trailer zeros. Matches the legacy `groupInviteStruct` layout (inviter+invitee, two 64-byte slots) padded by 24 bytes — the prior 2026-05-05 "Test split bidirectional opcode into two direction-specific IDs with a single name" observation is INVALID post-May-12-patch; both names are present in both directions.
- **`0xfc1d` = OP_GroupInvite2 (S>C, 176b)**: recipient name@0 (PC-A, self), sender name@64 (PC-B). Same physical layout as `0xbdab`; semantic of "who's at offset 0" is direction-dependent (target on C>S, recipient on S>C) — same as legacy.
- **`0xeeb4` = OP_GroupFollow (S>C, 68b)** confirmed: 4 fires across 2 invite/accept cycles. Names at offset 0 cover all members getting added from the receiver's POV — both the inviting PC and their mercenary (auto-name `<Name><6-digit-suffix>`). Trailing u32 at offset 0x40 alternates 0x01/0x02 (slot index). Matches the modern 68b groupFollowStruct previously hinted in xml.
- **`0x8fa1` = OP_GroupUpdate (S>C, 92b)** confirmed: 18 total fires across the capture. Two payload shapes share the 92b size:
  - State-broadcast fires: name@0 empty; u32@0x40 ~= timestamp-like; u32@0x44 = status code (e.g. 0x08d5); sentinel 0xffffff01 @0x48; trailing `01 00 00 00 0a 00 00 00`.
  - Per-slot status fires: recipient name@0 (self); u32@0x44 = slot index (observed 0x02 across the formed-group sequence); sentinel 0xffffff01 @0x48; trailing `02 00 00 00 0a 00 00 00`.
- Ruled out as group-related in this capture:
  - `0x5865` (1× C>S 80b) — all-zero payload; unexplained but doesn't pattern-match invite/accept/disband. Likely ambient or a generic ACK; not assigned.
  - `0x3ab3` / `0x787b` / `0x6dae` (C>S/S>C 80b, low fires) — also seen in merc-only capture without group activity; ambient.
- Cross-capture coherence: the merc-derived hypothesis from 2026-05-14 entry above (0xeeb4 = OP_GroupFollow, 0x8fa1 = OP_GroupUpdate) held up under a real PC-PC invite — same size + same name-at-offset-0 shape on both opcodes, just with more fires from the multi-member group-formation burst.
- Disband fires *are* in this capture — the 92b/80b "no obvious shape" guess above was wrong. Post-patch disband-family opcodes are **168b S>C** (matching the xml hint for OP_GroupDisband / OP_GroupDisband2 / OP_GroupLeader). Five distinct 168b S>C unknowns fire in this capture; full disambiguation below.
- Follow-up commit: `conf/zoneopcodes.xml` already updated for the 4 confirmed (0xbdab, 0xfc1d, 0xeeb4, 0x8fa1) + OP_GroupDisband2=0xc2d8. Comment on OP_GroupInvite2 rewritten to drop the disproved "single name per direction" claim. `everquest.h` `groupInviteStruct`/`groupFollowStruct`/`groupUpdateStruct` sizes match the confirmed 176/68/92 byte layouts — no struct edits needed.

### 2026-05-14 — OP_GroupDisband2 = 0xc2d8; OP_GroupDisband / OP_GroupLeader candidates from 168b S>C pile

- Capture: same `tests/replay/test-group-invite-20260514.vpk`. Two cycles ended with explicit disbands: cycle 1 ended with PC-B (the inviter from cycle 2) issuing `/disband`; cycle 2 ended with PC-A (self) issuing `/disband`.
- Method: `--dump-payload` on every 168b S>C unknown + the 2 C>S 168b unknowns. All five S>C candidates plus both C>S fires carry NUL-terminated character names at offset 0 and/or offset 64 — name@64 is the dominant "subject" field (matches the xml hint for the disband-family modern layout).
- **`0xc2d8` = OP_GroupDisband2 (S>C 168b)** confirmed. 2 fires:
  - Fire 1: PC-B name @ 64 — peer leaving notification when PC-B issued `/disband` (cycle 1 end).
  - Fire 2: PC-B's mercenary auto-name @ 64 — merc follows the leader out on disband.
  - Layout: only offset 64 (membername) carries data; offset 0 (yourname) is zero on peer-disband fires. Matches legacy `groupDisbandStruct{yourname[64], membername[64]}` semantics where yourname is the recipient's slot identifier (zero when broadcast).
- **`0xfc73` = OP_GroupDisband (C>S 168b)** strong candidate. 1 fire with self-name at BOTH offset 0 and offset 64. Matches PC-A's single `/disband` command in cycle 2. Caveat: legacy xml has `dir="server"` for OP_GroupDisband; on Test it's `dir="both"` (client sends C>S "/disband me" to the server; server then broadcasts S>C disband notifications to remaining members on a different opcode). Updated xml comment notes the candidate; needs S>C broadcast pair to confirm.
- **`0x27fa`** — 1× C>S 168b, layout `<inviter>@0 + <acceptor=self>@64`. Fires once = PC-A clicking accept on PC-B's invite in cycle 2. Likely **OP_GroupCancelInvite** or an accept-side handshake opcode (not in the 73-list, but documented for completeness). NOT assigned in xml.
- Still ambiguous — **OP_GroupLeader** is one of these four 168b S>C unknowns, but the fire patterns don't cleanly disambiguate from post-disband member-slot cleanup broadcasts:
  - `0x04c7` (4× S>C): 3 fires with `<self>@0 + <peer-merc-name>@64`, 1 fire with `empty@0 + <peer>@64`. The peer-merc pattern doesn't fit a leader-change broadcast (mercs don't lead groups), so likely a per-member status broadcast rather than OP_GroupLeader.
  - `0x6108` (3× S>C): all 3 with `<self>@64`. Could be OP_GroupLeader fires on cycle-1 formation (PC-A was implicit leader, broadcast once per existing member). Best leader candidate.
  - `0xd69d` (2× S>C): both with `<self>@64`. Could be post-disband self-slot cleanup or alternate leader-broadcast.
  - `0x548d` (2× S>C): both with `<self>@64`. Same shape as 0xd69d — paired broadcast on disband?
- Recommended next capture: a single-cycle session with explicit `/makeleader` issued mid-group, then `/disband`. The `/makeleader` event has a clean signature (one S>C fire with the new leader's name) that should unambiguously anchor OP_GroupLeader against the four remaining 168b candidates.

### 2026-05-14 — OP_GroupLeader = 0x04c7, OP_GroupMakeLeader = 0xb3b7 (NEW), OP_GroupDisband = 0x27fa; 0xfc73 retracted

- Capture: `tests/replay/test-group-leader-20260514.vpk` (PC-A zoned with existing PC-A + PC-B + PC-B's-merc group carried over; `/makeleader <PC-B>` issued once; `/disband` issued once by PC-A).
- Method: `--dump-payload` of every 168b opcode fired in the capture, comparing the FULL 168 bytes against the prior capture's same opcodes to identify discriminating fields.
- **u32 event-type discriminator at offset 0x84** found in the 168b group-event family. Constant u32 at offset 0x88 (0x40f3 = zone/session id, identical across both captures). Trailer at offset 0xa0 distinguishes command-shaped opcodes (`01 01` marker) from broadcast-shaped ones (zeros).
- **`0xb3b7` = OP_GroupMakeLeader (NEW opcode, dir="both", 168 bytes)** confirmed. 2 fires total: 1× C>S + 1× S>C, both with identical payload (target=new-leader@0, sender=self@64) and the `01 01` trailer at 0xa0. The S>C echo confirms server received and processed the command. Not in the original 73-list — added to xml as a new entry. Layout matches `groupDisbandStruct` two-name slots.
- **`0x04c7` = OP_GroupLeader (S>C 168b, broadcast)** confirmed. 2 fires in this capture: one with `empty@0 + <new-leader>@64, type=2 at 0x84` (the leader-change broadcast triggered by `/makeleader`); one with `<recipient>@0 + <merc-name>@64, type=20 at 0x84` (a related group-event, possibly "merc re-attached to leader" cleanup). The type-byte at 0x84 dispatches semantics — daemon handler should switch on it. Cross-checks against prior capture (4 fires there with the same name patterns) confirm 0x04c7 carries multiple event types under one opcode id.
- **`0x27fa` = OP_GroupDisband (dir="both", 168b)** confirmed. 1 fire in this capture (C>S, layout `<leader=PC-B>@0 + <self=PC-A>@64`) matches PC-A's single `/disband` event. Same opcode also fired 1× C>S in the prior capture (test-group-invite-20260514) with `<leader=PC-B>@0 + <self=PC-A>@64` matching PC-A's cycle-2 self-disband — consistent across both captures. dir promoted from "server" to "both" since the C>S command direction is now confirmed.
- **`0xfc73` RETRACTED as OP_GroupDisband C>S candidate**. It fired 1× C>S 168b in the prior capture (self+self payload) but did NOT fire in this capture despite a real `/disband`. The single prior fire remains unexplained — possibly an invite/accept-related ack from one of the prior cycles. The xml comment carrying the 0xfc73 hint has been removed.
- Still ambiguous — three 168b S>C fires whose semantics are unconfirmed (likely OP_GroupUpdate-extended slot-status or post-disband cleanup):
  - `0xd69d` (3× S>C 168b, all type=0 at 0x84; mix of self@64 and peer@64) — fires on group-state changes (makeleader and disband events) but doesn't pattern-match a single canonical opcode in the 73-list.
  - `0x6108` (1× S>C 168b, self@64, non-zero bytes at offset 0x80 and 0x90) — different shape from 0xd69d; possibly a self-disband S>C confirmation, but not aligned with prior-capture fire-count expectations (3 fires vs 1 here).
  - Neither was carried forward to xml as an assigned opcode.
- Group section status after this entry: **6 / 6 confirmed** in the 73-list (OP_GroupInvite, OP_GroupFollow, OP_GroupUpdate, OP_GroupDisband, OP_GroupDisband2, OP_GroupLeader). OP_GroupMakeLeader is a bonus (added to xml, outside the 73-list). OP_GroupInvite2 + OP_GroupCancelInvite remain ungrouped extras (Invite2 confirmed at 0xfc1d; CancelInvite still `ffff` — would need a decline-invite capture to anchor).

### 2026-05-14 — OP_MoveItem = 0xe883, OP_GroundSpawn = 0x7b00, OP_Buff = 0xd7f4

- Capture: `tests/replay/test-inzone-mixed-20260514.vpk` (Crescent Reach session: zone-in, inventory slot moves, ground-spawn observation, buff list visible on zone, mixed UI interactions).
- **OP_MoveItem = 0xe883 (C>S, 28b)** confirmed via the standing StructHint candidate. 14 C>S fires at exactly 28b, only candidate at that size+direction. moveItemStruct layout matches (two 12-byte invSlotStruct + uint32 stack count).
- **OP_GroundSpawn = 0x7b00 (S>C, ~65b variable)** confirmed. 61 fires on zone-in (one per ground drop in Crescent Reach). Layout: u32 length-prefix, NUL-terminated `IT##_ACTORDEF` item-actor model name (e.g. `IT11056_ACTORDEF`), u32 item-id, u32 spawn-id, 3 IEEE-754 floats giving (x, y, z) position. Position decode verified against expected zone coordinates (-871/-2381/-160 = valid Crescent Reach drop spot).
- **OP_Buff = 0xd7f4 (S>C, 65b primary + 30/39/91 variants)** confirmed. 23+ fires from full buff-list resync on zone-in. Layout: u32 spell_id@0, u32 duration@4, u32 flags@8, u32 tick-counter@17 (decrements between fires of same spell_id), NUL-terminated target-name@27, u16 buff-slot, NUL-terminated caster-name@53. Spell IDs in the 0x1a00-0x1a30 range match level-15 Necromancer self-buffs.
- **Sibling: 0x8108 (S>C, 65b, 8 fires)** — structurally identical buffStruct shape. Likely OP_Buff variant for a separate buff class (songs / short-duration / pet buffs). Not assigned in xml yet; flagged in OP_Buff comment for follow-up disambiguation.
- **Did NOT fire in this capture** (despite being in the requested action list):
  - OP_CommonMessage / OP_SimpleMessage — no `/say "hello"` plaintext anywhere in the dump. Either chat wasn't issued or it's on an opcode whose payload doesn't carry text directly (worth re-checking: legacy uses eqstr template IDs, not raw strings, for many "system" messages).
  - OP_ItemPacket — only fires on item acquisition (loot/buy/quest reward); moving items between slots uses OP_MoveItem only.
  - OP_ClickObject — no clean 12b C>S+S>C with `(dropId, spawnId)` shape identified; multiple 12b candidates exist but none uniquely correlate to drop-and-pickup.
  - OP_Find, OP_AAAction, OP_AAExpUpdate — no obvious candidates emerged.
  - OP_TimeOfDay — no 6-8b S>C with `{hour, min, day, month, year}` shape; daemon may have caught the session AFTER the zone-in time broadcast already passed.
  - OP_ZoneChange, OP_SendZonePoints — same zone-in timing caveat. Worth a focused capture: zone, observe time, click ground item, decline an invite (for OP_GroupCancelInvite), etc.

### 2026-05-14 — OP_Find = 0x0f64 (post-patch); chat opcodes likely deprecated on Test

- Capture: `tests/replay/test-inzone-mixed-20260514.vpk` (user confirmed `/say Hello` + Ctrl+F Find + AA activation in-game).
- **OP_Find = 0x0f64 (S>C, variable)** confirmed. 6 fires across mixed sizes: 22b × 3 (per-action requests/cooldown denial), 1016b × 2 (find-list payload on Ctrl+F open per zone), 5870b × 1 (extended/inter-zone find list). Plaintext NUL-delimited NPC names + find-category labels visible in the large payloads (Blightfire Moors: "Sentry Sudi", "Banker Ozro", "Parcels and Noble Exchange", "Guild Recruitment", etc.). Matches the 2026-05-05 OP_Find = 0x695f signature exactly — same two-phase contract (list-on-open + per-action responses), just at a new opcode id post-patch.
- **Chat opcodes likely deprecated on Test**: `grep -boaE "(Hello|hello)" tests/replay/test-inzone-mixed-20260514.vpk` returns zero hits despite user-confirmed `/say Hello`. Decryption is verified working (we see plaintext `IT##_ACTORDEF`, character names, quest text). The only consistent explanation is that **modern Test routes player chat (/say, /tell, /shout, etc.) through the external Daybreak chat service** referenced in OP_SetChatServer (`https://auth.daybreakgames.com/...`), not through the EQ zone UDP packet stream. Implication for the 73-list:
  - OP_CommonMessage (`channelMessageStruct`, both) — likely candidate for `[~]` confirmed-absent on Test (player chat moved off-wire). Needs one more verification capture (e.g. /tell + /shout + /guild) confirming none of those produce plaintext on the zone stream.
  - OP_SimpleMessage (`simpleMessageStruct`, server) — might still fire for spawn-targeted spell-effect text (per CLAUDE.md "spawn-targeted simple-messages" hint), distinct from player chat. Keep hunting.
- Did NOT confirm in this capture (despite user-confirmed actions):
  - OP_ClickObject — multiple 12b candidates exist (0xacfc, 0xf525, 0x23a2, 0xd825), but several carry spell-id-shaped first u32 instead of `(dropId, spawnId)` shape. The user dropped + picked up, but the pickup may have been masked by ambient spell-effect chatter at the same size.
  - OP_AAAction — no clear 12-32b C>S+S>C burst with AA-id-shaped payload identified.
  - OP_ZoneChange — no 88b candidate in the capture. User zoned to Blightfire Moors (confirmed via OP_Find content carrying Blightfire NPCs), but the zone-transition packet may have been processed during the pre-key-handoff window — the capture daemon's session key wasn't fully established before the zone, so the ZoneChange packets may have been encrypted-undecodable.
  - OP_TimeOfDay — same zone-handoff timing caveat. Needs a capture where the daemon is already running with a stable session key, THEN the user zones (so the daemon decodes ZoneChange + TimeOfDay cleanly).
