# Unresolved opcodes (live)

`showeq-daemon/conf/zoneopcodes.xml` has **226 of 275** zone opcodes still set to `id="ffff"`. `worldopcodes.xml` is clean.

Target server: **live EQ** (Tasks/AA/DZ/Tribute/Fellowship/Marketplace/Mercenaries/etc. all in scope).

Confirmation bar: see `feedback_opcode_disambiguation.md` — count + zero-competitor over n>=5; n=2-3 is enough if `--opcode-stats` reports no other unknowns at the target size+direction.

---

## Tier 1 — Core gameplay loop (79)

### Combat (7)
- [ ] OP_AutoAttack
- [ ] OP_AutoAttack2
- [ ] OP_CombatAbility
- [ ] OP_Stun
- [ ] OP_Taunt
- [ ] OP_Sacrifice
- [ ] OP_ApplyPoison

### Spells (5)
- [ ] OP_BeginCast
- [ ] OP_CastSpell
- [ ] OP_MemorizeSpell
- [ ] OP_SwapSpell
- [ ] OP_LoadSpellSet

### Stats / HP / mana / xp (10)
- [x] OP_ExpUpdate — `0x6961` (2026-05-01)
- [x] OP_LevelUpdate — `0x7a97` (2026-05-01)
- [ ] OP_ManaUpdate
- [x] OP_SkillUpdate — `0x6f75` (2026-05-01)
- [ ] OP_LeaderExpUpdate
- [x] OP_MobHealth — `0x2171` (2026-05-01)
- [ ] OP_InitialMobHealth
- [ ] OP_MendHPUpdate
- [ ] OP_IncreaseStats
- [ ] OP_UIUpdate

### Spawn / appearance (8)
- [ ] OP_ZoneSpawns
- [ ] OP_SpawnRename
- [ ] OP_WearChange
- [ ] OP_Illusion
- [ ] OP_Shroud
- [x] OP_Animation — `0x3d3d` (2026-05-01)
- [ ] OP_FaceChange
- [ ] OP_Dye

### Movement / zone (18)
- [ ] OP_Jump
- [ ] OP_SetRunMode
- [x] OP_MovementHistory — `0xdc5d` (2026-05-01)
- [ ] OP_EnvDamage
- [ ] OP_Weather
- [ ] OP_SafePoint
- [ ] OP_Translocate
- [ ] OP_ClickDoor
- [ ] OP_MoveDoor
- [ ] OP_BoardBoat
- [ ] OP_LeaveBoat
- [ ] OP_ReqNewZone
- [ ] OP_RequestZoneChange
- [ ] OP_SaveOnZoneReq
- [ ] OP_ReqZoneObjects
- [ ] OP_ReqClientSpawn
- [ ] OP_SendExpZonein
- [ ] OP_ZoneUnavail

### Skill actions (16)
- [ ] OP_BindWound
- [ ] OP_Mend
- [ ] OP_Forage
- [ ] OP_Hide
- [ ] OP_Sneak
- [ ] OP_Track
- [ ] OP_Begging
- [ ] OP_DisarmTraps
- [ ] OP_SenseTraps
- [ ] OP_SenseHeading
- [ ] OP_FindPersonRequest
- [ ] OP_FindResponse
- [ ] OP_RandomReq
- [ ] OP_RandomReply
- [ ] OP_Consume
- [ ] OP_ReadBook

### Group / raid / pet / target (8)
- [ ] OP_GroupInvite2
- [ ] OP_GroupCancelInvite
- [ ] OP_GroupFollow2
- [ ] OP_GroupMemberList
- [ ] OP_RaidInvite
- [ ] OP_RaidJoin
- [ ] OP_PetCommands
- [ ] OP_TargetCommand

### Buffs / inspect / chat (7)
- [ ] OP_BuffWindow
- [ ] OP_BuffFadeMsg
- [ ] OP_ClickBuffOff
- [ ] OP_InspectRequest
- [ ] OP_InspectAnswer
- [ ] OP_ItemTextFile
- [ ] OP_Emote

---

## Tier 2 — Trade / loot / inventory / economy (41)

### Loot / corpse (8)
- [ ] OP_LootRequest
- [ ] OP_LootItem
- [ ] OP_EndLootRequest
- [ ] OP_LootComplete
- [ ] OP_MoneyOnCorpse
- [ ] OP_CorpseLocResponse
- [ ] OP_ConsiderCorpse
- [ ] OP_SummonCorpse

### Player trade (8)
- [ ] OP_TradeRequest
- [ ] OP_TradeRequestAck
- [ ] OP_TradeDeny
- [ ] OP_FinishTrade
- [ ] OP_CancelTrade
- [ ] OP_TradeAcceptClick
- [ ] OP_TradeCoins
- [ ] OP_TradeCoins2

### Inventory / items (8)
- [x] OP_MoveItem = 0xcb03 (C>S, 28B; resolved 2026-05-02)
- [x] OP_ItemPacket = 0x3f3b (S>C, variable; resolved 2026-05-02)
- [ ] OP_ItemPlayerPacket — appears obsolete on current Live; per-item OP_ItemPacket fires now cover both bulk-on-zone-in and per-move delivery
- [ ] OP_ItemLinkClick
- [ ] OP_ItemLinkResponse
- [ ] OP_ClickInventory
- [ ] OP_ClickInventoryAck
- [ ] OP_OpenObject

### Money (4)
- [ ] OP_GainMoney
- [ ] OP_MoneyUpdate
- [ ] OP_MoveCash
- [ ] OP_Split

### Vendor / Bazaar / Trader (8)
- [ ] OP_ShopRequest
- [ ] OP_ShopItem
- [ ] OP_ShopPlayerBuy
- [ ] OP_ShopPlayerSell
- [ ] OP_ShopEnd
- [ ] OP_BazaarSearchRequest
- [ ] OP_BazaarSearchResponse
- [ ] OP_Trader

### Tradeskill (5)
- [ ] OP_OpenTradeskillContainer
- [ ] OP_TradeskillRecipes
- [ ] OP_TradeSkillCombine
- [ ] OP_TradeSkillCombineOld
- [ ] OP_TradeSkillResult

---

## Tier 3 — Social / guild / GM / petition (52)

### Social (11)
- [ ] OP_FriendsList
- [ ] OP_FriendsOnline
- [ ] OP_WhoAllRequest
- [ ] OP_WhoAllResponse
- [ ] OP_Surname
- [ ] OP_LFGCommand
- [ ] OP_LFPCommand
- [ ] OP_LFGGetMatchesRequest
- [ ] OP_LFGGetMatchesResponse
- [ ] OP_LFPGetMatchesRequest
- [ ] OP_LFPGetMatchesResponse

### Guild (12)
- [ ] OP_GetGuildMOTD
- [ ] OP_GuildsList
- [ ] OP_NewGuildInZone
- [ ] OP_GuildBank
- [ ] OP_GuildDemote
- [ ] OP_GuildLeader
- [ ] OP_GuildPeace
- [ ] OP_GuildWar
- [ ] OP_GuildPublicNote
- [ ] OP_GuildRemove
- [ ] OP_GuildTributeInfo
- [ ] OP_GuildTributeStatus

### GM (21)
- [ ] OP_GMApproval
- [ ] OP_GMBecomeNPC
- [ ] OP_GMDelCorpse
- [ ] OP_GMEmoteZone
- [ ] OP_GMEndTraining
- [ ] OP_GMEndTrainingResponse
- [ ] OP_GMGoto
- [ ] OP_GMHideMe
- [ ] OP_GMInquire
- [ ] OP_GMKick
- [ ] OP_GMKill
- [ ] OP_GMLastName
- [ ] OP_GMSearchCorpse
- [ ] OP_GMServers
- [ ] OP_GMSoulmark
- [ ] OP_GMSummon
- [ ] OP_GMToggle
- [ ] OP_GMTraining
- [ ] OP_GMTrainSkill
- [ ] OP_GMZoneRequest
- [ ] OP_GMZoneRequest2

### Petition (8)
- [ ] OP_Petition
- [ ] OP_DeletePetition
- [ ] OP_PetitionCheckIn
- [ ] OP_PetitionCheckout
- [ ] OP_PetitionQue
- [ ] OP_PetitionResolve
- [ ] OP_PetitionUnCheckout
- [ ] OP_ViewPetition

---

## Tier 4 — Live-era / endgame systems (36)

### Tasks (3)
- [ ] OP_TaskDescription
- [ ] OP_TaskActivity
- [ ] OP_CompletedTasks

### AAs (4)
- [x] OP_AAAction — `0x773e` (2026-05-02)
- [x] OP_RespondAA — `0xa1e1` (2026-05-03) — per-spend response carrying the player's full AA list
- [ ] OP_SendAAStats — likely obsolete on current Live; not observed firing in `aa_point.vpk`
- [x] OP_SendAATable — `0xa30a` (2026-05-03) — static ability-definition menu, NOT purchased ranks

### DZ / Expedition (4)
- [ ] OP_DzInfo
- [ ] OP_DzSwitchInfo
- [ ] OP_DzMembers
- [ ] OP_Lockouts

### Adventure / LDoN (5)
- [ ] OP_Adventure
- [ ] OP_AdventureRequest
- [ ] OP_AdventureMerchantPurchase
- [ ] OP_AdventureMerchantResponse
- [ ] OP_AdventurePointsUpdate

### Mercenaries (1)
- [ ] OP_MercenaryList

### Tribute (2)
- [ ] OP_TributeInfo
- [ ] OP_TributeUpdate

### Fellowship / Campfire (3)
- [ ] OP_Fellowship
- [ ] OP_Campfire
- [ ] OP_SelectCampfire

### Marketplace / Claims / Voice / Polls / Shroud (8)
- [ ] OP_Marketplace
- [ ] OP_MarketplaceSelect
- [ ] OP_Claims
- [ ] OP_VoiceChat
- [ ] OP_PollQuestions
- [ ] OP_PollResponses
- [ ] OP_ShroudProgression
- [ ] OP_ShroudTemplates

### Housing (2)
- [ ] OP_HouseAddress
- [ ] OP_HouseContents

### Misc rewards / titles (4)
- [ ] OP_VeteranRewards
- [ ] OP_Rewards
- [ ] OP_CustomTitles
- [ ] OP_WelcomeScreenTitle

---

## Tier 5 — Protocol / system / bug-report (24)

### Session / system (11)
- [ ] OP_AckPacket
- [ ] OP_DeltaCheck
- [ ] OP_SetDataRate
- [ ] OP_SetServerFilter
- [ ] OP_ClientReady
- [ ] OP_Save
- [ ] OP_Camp
- [ ] OP_Logout
- [ ] OP_RespawnWindow
- [ ] OP_ConfirmDelete
- [ ] OP_ExpansionSetting

### Misc world / consent (6)
- [ ] OP_TGB
- [ ] OP_FTPNags
- [ ] OP_ConsentResponse
- [ ] OP_DenyResponse
- [ ] OP_BecomePK
- [ ] OP_YellForHelp

### Bug / feedback (4)
- [ ] OP_Bug
- [ ] OP_Feedback
- [ ] OP_Report
- [ ] OP_CrashDump

### Unknown (3)
- [ ] OP_Unknown1
- [ ] OP_Unknown2
- [ ] OP_Unknown3

---

## Confirmation log

### 2026-05-01 — first tier-1 sweep (monk solo, ~10 min varied activity)

Capture: `--opcode-stats` over one mob fight + movement + /random ×2 + /findperson + inspect + /emote + door click + sit/stand + jumps + one /mend.

- **OP_Animation = `0x3d3d`** (S>C, 4 bytes, n=78). Bytes match `{u16 spawn_id, u8 anim, u8 speed}` — spawn_id alternates between player and mob across the fight, anim varies, speed=10 constant. Dominates all other 4-byte S>C unknowns (next highest n=2). No daemon handler today; XML id set so future stats reports mark it known.
- **OP_MobHealth = `0x2171`** (S>C, 6 bytes, n=26 in discovery + n=5/9 across two kill verifies). Struct is `{u16 spawnId, int32 hpPercent}` — value is **HP percentage 0-100**, not raw HP. Confirmed 2026-05-01 by kill captures where the first observed value was exactly 100. Wired through `SpawnShell::updateMobHealth` → `setHP(maxHP * pct / 100)` when maxHP is known from a prior OP_HPUpdate / OP_InitialMobHealth.

  **Side finding worth carrying forward**: in the verify capture both targeted mobs (yellow skel + white mob) showed `maxHP=100` from `OP_HPUpdate`. That suggests `hpNpcUpdateStruct` is also a percent on live (its XML comment already says "HP % of a PC or NPC") — `maxHP` is normalized to 100, not raw. Means the daemon's HP gauge for spawns is fundamentally percent-based on live, and any future tier-1 work that wants real raw HP needs an out-of-band source (mob level/race lookup tables).
- **OP_MovementHistory = `0xdc5d`** (C>S, base 18 bytes batched up to 2500, n=407). Per-record 18 bytes with X/Y/Z floats; batched bursts during movement. Only loud C>S unknown; behavioral fit is unmistakable. No handler; XML-only.

Hint table refresh (`src/opcodestats.cpp`): dropped the 9 stale entries (all already-resolved opcodes), added tier-1 hints with sizes from `everquest.h`:
- OP_ExpUpdate / 16 / S>C
- OP_LevelUpdate / 16 / S>C (struct is `levelUpUpdateStruct`)
- OP_SkillUpdate / 12 / S>C
- OP_InspectRequest / 8 / S>C
- OP_InspectAnswer / 1956 / S>C
- OP_EnvDamage / 46 / S>C

### 2026-05-01 — exp + level + skill triple-confirm (monk Gloomingdeep, ~10 min, level 1→2 + 4 kills + 11 skill-ups)

The new tier-1 hint table immediately surfaced all three at the right size+direction:

- **OP_SkillUpdate = `0x6f75`** (S>C, 12 b, n=11 — exact match for the 11 skill-ups). Sample bytes decode cleanly as `skillIncStruct{u32 skillId, i32 value, u8[4] unknown}`: skillId=30 (H2H), value=12 ✓ matches "H2H to 12". Three competing 12-byte S>C unknowns existed (300/30/n hits) but counts didn't match user activity.
- **OP_ExpUpdate = `0x6961`** (S>C, 16 b, n=9 across 4 kills + level transition; ~2 fires per kill = set+update). Sample bytes decode as `expUpdateStruct{u32 exp, u32 unknown, u32 type, u32 unknown}` with **exp on a 0-100000 scale** (not legacy 0-330 or percent-100). Pre-level sample: exp=97900, type=0 then type=2; post-level: exp=814 — matches the OP_LevelUpdate cross-reference exactly.
- **OP_LevelUpdate = `0x7a97`** (S>C, 16 b, n=1 at level transition). Sample bytes: `02 00 00 00 01 00 00 00 2e 03 00 00 00 00 00 00` → `levelUpUpdateStruct{level=2, levelOld=1, exp=814, unknown=0}`. The `exp=814` cross-references the OP_ExpUpdate post-level sample byte-for-byte. This is about as definitive as it gets.

Player.cpp lost the legacy `calc_exp() / 330` machinery (per-class exp tables that don't apply to live's per-level percent format). The init / charProfile / updateLevel paths now hard-code `m_minExp=0, m_maxExp=100000, m_tickExp=1`. Bar UI uses `expCur/expMax` ratio so percent correctly displays without further changes.

Round-2 ideas:
- Isolation captures for the 16-byte S>C field (six candidates) — kill a mob with no other activity to nail OP_ExpUpdate.
- Skill-up fishing on a low skill (BindWound, Sense Heading) for OP_SkillUpdate at 12 bytes S>C.
- Track session on a ranger to chase OP_Track.
- Add struct definitions + handlers for OP_Animation / OP_MobHealth / OP_MovementHistory if we want decoded events in the daemon (separate work — current XML-only state means stats report cleanup, no behavioral decode).

### 2026-05-02 — OP_PlayerProfile wire layout (AA + skills array prefixes)

OP_PlayerProfile (`0xdb56`) was already resolved; this is a layout refinement. The struct in `everquest.h` documents fixed offsets, but the live wire is variable-length with explicit u32 length prefixes before each array. Confirmed against two captures (`combat.vpk` lvl 1 monk @ 23875 bytes; a fresh login capture for a lvl 60 @ 37088 bytes — both via `--dump-payload 0xdb56:path`):

- **wire 1014**: `u32 = 300` (MAX_AA prefix)
- **wire 1018..4617**: `AA_Array[300]` (12 bytes each: `aa_id`, `value`, `unk`)
- **wire 4618**: `u32 = 100` (MAX_KNOWN_SKILLS prefix)
- **wire 4622..5021**: `uint32[100]` skills

Only the skills overlay in `zonemgr.cpp::zonePlayer` is necessary — `fillProfileStruct`'s netstream parser already populates `aa_array` correctly (via length-prefix reads); it just explicitly skips the skills values.

**aa_array on this opcode carries the FULL AA list, including purchased ranks** — early indices populated with auto-grants at `value=0` (e.g. `1371-1377` vet rewards, `4665`, `4700`, `9000`, plus `1000`, `15073`, `8000` at lvl 60), then trailing slots with the player's purchased AAs at `value=rank_count`. Confirmed 2026-05-03 against `aa_point.vpk`: index 13 = `(163, 5)` Mend Pet × 5, index 14 = `(64, 3)` Innate Run Speed × 3 in fire 4 (post-spend). The earlier "auto-grants only" claim came from a capture taken before the user had purchased anything; once `aa_spent > 0`, the trailing slots populate. _(Original note left below for context, struck through.)_

> ~~aa_array on this opcode is auto-granted only. Every entry has value=0. Auto-granted ids observed across captures: 1371-1377 (sequential vet rewards), 4665, 4700, 9000 on the lvl 1 capture; the lvl 60 capture adds 1000, 15073, 8000. Purchased AA ranks (Origin, Veteran's Enhancement, etc.) — even at 1/1 — do not appear here. They almost certainly come over OP_SendAATable, still unresolved.~~

Round-2 ideas (specific to AA work):
- Hunt OP_SendAATable: capture during a fresh login and `--opcode-stats` for repeated S>C unknowns sized like a per-AA record (~16-32 bytes per entry × N).
- Once AA ranks are wired, also nail down `aa_spent` / `aa_unspent` / `aa_assigned` / `expAA` wire offsets in OP_PlayerProfile (post-skills, in the variable-tail region — needs a non-zero capture for disambiguation).

### 2026-05-02 — OP_PlayerProfile expAA wire offset

Capture: `aa_progress.vpk` recorded during a 4-zone session that progressed AA from 22.846% to 40.318%, with `--dump-payload 0xdb56:/tmp/pp_aa` extracting all four OP_PlayerProfile fires (each 37108 bytes, identical to the lvl 60 baseline length).

- **expAA at wire offset 20411** (u32 LE). Values across the four dumps: `22846 / 22846 / 22846 / 40318` — exact match for the in-game progress (display % = `expAA / 1000.0`). Zero-out competing candidates: a Python sweep of every offset ≥5022 for a u32 in [10000, 100000] that is monotonically non-decreasing across the four dumps AND brackets the start/end values returned a single hit.

  The legacy parser had `skipBytes(70)` in `fillProfileStruct` between `endurance` and the firstName length-prefix; expAA sits 58 bytes into that gap. Fix is splitting it into `skip(58) + read expAA + skip(8)`. Mirrored to legacy `showeq/src/zonemgr.cpp`.

  Confirmed scale by replaying the vpk through the patched daemon — all four `player->expAA` reads matched the wire-offset Python analysis byte-for-byte. Tier-2 `.pbstream` goldens regenerated for the `aa_exp_max` 15M→100K change (encoder `protoencoder.cpp`; legacy `player.cpp` emit args also 15M→100K so StatList's integer-truncated % calc stops collapsing to 0).

- **aa_spent / aa_unspent / aa_assigned**: still unresolved on the wire. All four dumps had every nearby u32 either 0 or constant — the user hadn't earned a full AA point during the capture, so there's no signal to disambiguate the field positions. Round-3: re-capture after spending one AA point (`aa_spent` 0→1 plus likely OP_AAAction round-trip + OP_SendAATable refresh).

### 2026-05-02 — OP_PlayerProfile aa_spent / aa_assigned / aa_unspent wire offsets

Capture: `aa_point.vpk` recorded across 4 zone-ins spanning an AA level-up event + spending. Final state: 8 spent (5 Mend Pet + 3 Run Speed), 1 unspent. expAA progression confirms the 88533 → 27 reset pattern that follows a level-up (same wrap behavior as normal exp).

- **aa_spent at wire offset 19487, aa_assigned at 19495, aa_unspent at 19519** (all u32 LE). Progression across the four fires:

  | fire | aa_spent | aa_assigned | aa_unspent | expAA  | story |
  |------|----------|-------------|------------|--------|-------|
  | 1    | 0        | 0           | 8          | 88533  | pre-level-up |
  | 2    | 0        | 0           | 8          | 88533  | same |
  | 3    | 0        | 0           | 9          | 27     | crossed expAA threshold → +1 point |
  | 4    | 8        | 8           | 1          | 27     | spent 5 + 3 = 8 → 1 left |

  The existing parser already lands at the right offsets (verified by replaying with a temporary `qInfo` log of `netStream.pos() - netStream.data()`); prior captures showed all-zero spent/assigned because the user genuinely had `aa_spent=0` then. No code change.

- **aa_spent vs aa_assigned**: identical values (8/8) without a glyph applied — the parser order matches `everquest.h`'s `aa_spent → unknown(4) → aa_assigned → unknown(16, declared as 4×u32) → aa_unspent`. Wire stride between aa_assigned and aa_unspent is 20 bytes, +4 over the struct's 16. Disambiguation requires a glyph capture (`aa_spent` includes glyphs, `aa_assigned` doesn't).

- **`aa_points` proto field semantics**: ~~daemon currently emits `aa_points = aa_spent` (`player.cpp:349`). For a UI showing "AA: N pool to spend", `aa_unspent` is the more useful number — but that's a proto schema change, deferred.~~ Resolved 2026-05-02: added `aa_unspent` field to `seq.v1.PlayerStats`; daemon emits both. UI shows `AA · N spent · M ready`.

### 2026-05-02 — OP_AAExpUpdate (0xbf67) wire fields

Captures: `aa_progress.vpk`, `aa_point.vpk`, `aa_slider50.vpk`. Each 12-byte payload decodes as:

| field | type | semantic |
|-------|------|----------|
| `altexp` | u32 LE | 0..100000 sub-percent progress to next AA point (matches charProfile `expAA` scale; in-game % = `altexp/1000`). Direct; no scaling. |
| `aapoints` | u32 LE | **Unspent** count, NOT spent — decrements as the player allocates points. Legacy code routed this into `m_currentAApts` (the "spent" tracker), corrupting it on every tick. Now goes to `m_currentAAUnspent`. |
| `percent` | u8 | The player's "% of exp to AA" slider. Constant during kills; fires its own OP_AAExpUpdate when the slider moves (`aa_slider50.vpk` shows the walk 100→90→80→70→60→50 across consecutive fires). |

Implication: pre-fix the AA bar was driven by `altexp * (15000000/330)` (legacy x/330 scaling) but the encoder advertised `aa_exp_max=100000`, so post-tick the bar pegged to 100% or showed wildly wrong values. Direct use of `altexp` resolves it.

Round-3 ideas:
- Apply a glyph and re-capture to disambiguate `aa_spent` vs `aa_assigned`.
- Hunt OP_SendAATable now that there are non-zero purchased ranks (Mend Pet × 5, Run Speed × 3) — the table should include records with `value=5` and `value=3` matching those ranks, easy to spot by `--opcode-stats` + `--dump-payload`.

### 2026-05-02 — OP_AAAction = 0x773e

Capture: same `aa_point.vpk`. Method: time-correlation via the new daemon `--list-events` flag (`<unix_ms> <C|S> 0xXXXX <bytes> <stream> <name>` per packet). The naive count-based hunt against `--opcode-stats` failed because the `aaActionStruct` is 16 bytes but no global 4-count C>S unknown matched at that size; the win came from filtering events to the **spend window** (between OP_PlayerProfile fire #3 with `aa_unspent=9, aa_spent=0` and fire #4 with `aa_unspent=1, aa_spent=8`).

In that window exactly one unknown C>S opcode fired exactly four times at 16 bytes:

| fire | u32_le payload | meaning |
|------|----------------|---------|
| 1 | `[3, 58, 0, 0]` | action=3 (buy), ability=58 — Mend Pet, single buy of 5 ranks |
| 2 | `[3, 13, 0, 0]` | ability=13 — Innate Run Speed rank 1 |
| 3 | `[3, 13, 0, 0]` | rank 2 |
| 4 | `[3, 13, 0, 0]` | rank 3 |

Decode matches EQMacEmu's `AA_Action { action, ability, unknown08, exp_value }` 16-byte struct. Resolution landed in both `showeq-daemon/conf/zoneopcodes.xml` and legacy `showeq/conf/zoneopcodes.xml`.

The same opcode also fires 400× S>C at 12 bytes across the full capture — a companion server-side message (legacy XML comment hinted at multi-purpose: "changing percent, buying, activating skills"). Not investigated; the C>S resolution alone covers the user-visible "buy AA" event.

Generalizable lesson: when count-based correlation against `--opcode-stats` totals returns nothing, narrow the window. `--list-events` + an in-game-event-bracketing pair of known opcodes (here, two OP_PlayerProfile fires) often resolves a rare opcode that was lost in the noise of the global tally.

### 2026-05-02 — OP_ItemPacket = 0x3f3b, OP_MoveItem = 0xcb03 (full inventory pipeline)

OP_MoveItem (`0xcb03`, C>S 28B) and OP_ItemPacket (`0x3f3b`, S>C variable ~960B) resolved via `inventory.opcodestats.txt` from a slot-swap session — Whitened Treant Fists ↔ Adamantite Club moves produced exactly the expected counts at unique size+direction combos.

OP_ItemPacket's `serializedItem[0]` blob decoded into `parsedItemTemplateStruct` (everquest.h). Layout pinned by aligning post-name offsets across five reference items (Crescent Gi, Platinum Fire Wedding Ring, Outstanding Necklace/Mask/Band of Distant Echoes) with known stats:

| Offset | Type | Field |
|--------|------|-------|
| +0  | u32 | format constant (always 63) |
| +8  | u32 | itemId |
| +12 | u32 | weight × 10 |
| +20 | u32 | slot bitmask (0x6000 = Pri\|Sec, 0x18000 = Ring1\|Ring2, 0x20 = Neck, 0x20000 = Chest, etc.) |
| +34..+38 | i8 × 5 | resists [CR, DR, PR, MR, FR] |
| +39 | i8 | corruption resist |
| +40..+46 | i8 × 7 | stats [STR, STA, AGI, DEX, CHA, INT, WIS] |
| +47..+50 | i32 LE | HP modifier |
| +51..+54 | i32 LE | mana modifier |
| +55..+58 | i32 LE | endurance modifier |
| +59..+62 | i32 LE | AC modifier |

Wire gives BASE values; equipped augments contribute via separate OP_ItemPacket fires for the aug item. Total stats = base + each filled aug.

Notable: legacy bulk OP_ItemPlayerPacket appears obsolete on current Live. Modern Live sends per-item OP_ItemPacket fires for both zone-in pickup and slot moves.

Daemon side: parser in `src/itempacket.{h,cpp}`, persistent itemId → ItemTemplate cache at `~/.showeq/daemon/itemcache.json` (`src/itemcache.{h,cpp}`), emitted to clients as ItemLearned + ItemCacheTotals envelopes (proto messages 33/34, plus Snapshot fields 8/9). Web client renders an InventoryStatsPanel.

### 2026-05-02 — OP_PlayerProfile worn-slot decode: stalled (research notes)

**Goal:** find the offset(s) in OP_PlayerProfile where worn-slot itemIds live, so the daemon can sum HP/mana/AC across the *currently equipped* set instead of every observed item in the cache.

**Approach tried:** differential capture. Three sequences across `inventory-worn.vpk`, `inventory-worn2.vpk`, `inventory-naked.vpk`:

1. Single-item un-equip + zone (Mask off, Mask on, Necklace off, Necklace on).
2. Multi-item un-equip with manual drag (Mask + Necklace + Band off + on).
3. Full strip (every worn slot) + zone, repeated for cross-validation.

**Findings:**

- Itemid bytes for known worn items appear at three FIXED offsets each (Mask at 36131/36471/36811, Necklace at 36171/36511/36851) and **do not change when the item is un-equipped**. So those offsets are not the worn-slot indicator. They appear to be persistent inventory listings or some account-wide record (a Wedding Ring from a different character on the same account also appears at one of these offsets, supporting "account-wide" interpretation).
- 320–416 bytes consistently change when gear is removed, scattered across PP offsets 20591–36025 in 1–3 byte runs. No clean contiguous worn-slot block.
- Densest cluster: offsets **21300–21399** (45 changing bytes). With gear on, this region holds float-shaped values; with gear off, it's all zeros. Likely a calculated-stats / aug-summary table rather than itemId mapping.
- Money block confirmed at offsets **19435 (and duplicated at 20625)** — uint32 LE platinum/gold/silver/copper; usable as a stable landmark for future PP work.
- Buff timers tick continuously and produce ~10–60 byte noise floor between supposedly-identical baselines, contaminating naive diffs. Strip all buffs before any future differential capture.

**Hypothesis:** modern Live's OP_PlayerProfile does not carry a flat "slot → itemId" table. Equipment data appears split across multiple sub-blocks (calculated stat totals, weight/encumbrance contributions, possibly cached aug-array data) that the client uses to render computed totals. Worn-slot itemId mapping may live in a dedicated opcode that fires on each equip/un-equip rather than in PP.

**Next investigation paths (in priority order):**

1. **Hunt for a dedicated worn-update opcode.** Look for unknown C>S+S>C pairs that fire alongside drag-equip/un-equip but NOT alongside other inventory moves. Use `--list-events` + an in-game-event window bracketed by zoning events. Promising candidates from the inventory-worn captures' opcode-stats: any C>S that fires exactly N times during N slot transitions.
2. **Isolate one stat at a time.** Equip ONLY a +HP item, capture; then ONLY a +mana item, capture; then ONLY a +AC item. Each diff against naked should reveal the byte offset for ONE calculated-stat field. Multi-day work but accurate.
3. **Cross-reference EQ client serialization.** The 21300-21399 region looks like the wire format used for serialized-item-via-stack on the client side. May be possible to reverse-engineer from public-source EQ-emulator references with care for licensing.

For now, the v1 ItemCache + ItemCacheTotals approach (sum across ALL observed items, not worn-only) is shipped and useful as a "potential gear stats" total. Worn-only is a long-tail follow-up.

### 2026-05-03 — OP_ItemPacket wrapper carries the worn-slot (PP detour resolved)

The 2026-05-02 hypothesis was right (PP doesn't carry a flat slot→itemId table) but the suggested next step (hunt a dedicated worn-update opcode) turned out to be unnecessary: **OP_ItemPacket already broadcasts each slot transition.** The daemon's parser was just discarding the wrapper bytes that name the destination slot.

**Method.** Replayed `inventory-worn2.vpk` and `inventory-naked.vpk` with `--dump-payload 0x3f3b:…` and inspected the wrapper bytes preceding the (already-decoded) item-name region. No new captures needed.

**Wrapper layout** (validated across 90+ fires):

| Offset | Type | Field |
|--------|------|-------|
| +0  | u32 | `packetType` — 0x74=item-in-bag, 0x76=bag-itself, 0x78=move-response |
| +4  | char[16] | ASCII instance-id (e.g. `"vIS00B80001H3G00"`) |
| +20 | u8  | NUL terminator |
| +21 | u32 | stack/charges count |
| +25 | u32 | `main_slot` — 0=top-level (worn/inv/cursor), nonzero=parent bag slot |
| +29 | u16 | `sub_slot` — when main=0, this IS the worn/inv slot index |

Slot enum (matches the legacy `slot_bitmask` bit indices on `parsedItemTemplateStruct`):
`0=Charm, 1=Ear_L, 2=Head, 3=Face, 4=Ear_R, 5=Neck, 6=Shoulder, 7=Arms, 8=Back, 9=Wrist_L, 10=Wrist_R, 11=Range, 12=Hands, 13=Primary, 14=Secondary, 15=Finger_L, 16=Finger_R, 17=Chest, 18=Legs, 19=Feet, 20=Waist, 21=PowerSrc, 22=Ammo, 23-30=PersonalInv, 35(0x23)=Cursor`.

**Cross-validation in `inventory-naked.vpk`** (full strip → re-equip × 2). A monk with 21 slots filled produced exactly 21 type-0x78 fires per re-equip, slot indices and items aligning perfectly:

| Slot | Item |
|------|------|
| 0  Charm    | Outstanding Charm of Distant Echoes |
| 1  Ear_L    | Outstanding Summoner Stud of Distant Echoes |
| 2  Head     | Apothic Crown |
| 3  Face     | Outstanding Mask of Distant Echoes |
| 4  Ear_R    | Outstanding Stud of Distant Echoes |
| 5  Neck     | Outstanding Necklace of Distant Echoes |
| 6  Shoulder | Outstanding Shawl of Distant Echoes |
| 7  Arms     | Apothic Sleeves |
| 8  Back     | Outstanding Cloak of Distant Echoes |
| 9  Wrist_L  | Bracelet of Exertion |
| 10 Wrist_R  | Supple Scale Armband |
| 11 Range    | Iksar Hide Manual |
| 12 Hands    | Dusty Bloodstained Gloves |
| 13 Primary  | Orb of Mastery |
| 14 Secondary| Book of Obulus |
| 15 Finger_L | Outstanding Band of Distant Echoes |
| 16 Finger_R | Excellent Band of Distant Echoes |
| 17 Chest    | Robe of the Azure Sky |
| 18 Legs     | Apothic Kilt |
| 19 Feet     | Apothic Boots |
| 20 Waist    | Damask Sash |
| 22 Ammo     | Metamorph Wand: Undead Gingerbread Man |

Strip phase showed all 21 items moving to slot 35 (Cursor). The pattern repeats once more at the second strip→redress cycle, so the result reproduces.

**Ruled-out leads** (worth recording so we don't re-chase them):
- `0x6c39` (14B S>C): count tracked transitions (5/6/etc.) but payload is byte-identical (`0a000000…0`) across every fire — it's a periodic end-of-burst sentinel after bag-snapshot bursts of OP_ItemPackets, not a wear-change.
- `OP_WearChange` as a dedicated player-self update: not visible in these captures. May still exist as a *visual broadcast* for **other** players' gear changes (legacy comment: "or give a pet a weapon (model changes)"), but for the player's own worn-slot bookkeeping the data is in OP_ItemPacket.
- `0x549c / 0x9d44 / 0x37ad / 0x800e / 0xbbf0`: counts loosely correlate with various inventory-burst-tier events but none are worn-slot identifiers.

**Daemon parser change** (sketch — see `src/itempacket.{h,cpp}`):

1. Add to `ItemTemplate`:
   ```cpp
   uint32_t packetType = 0;   // 0x74 / 0x76 / 0x78
   uint32_t mainSlot   = 0;   // 0 = top-level
   uint16_t subSlot    = 0;   // worn/inv slot index when mainSlot==0
   ```
2. In `parseItemPacket`, before the existing `findNameStart` scan, read the fixed-offset wrapper fields:
   ```cpp
   if (len < 31) return false;
   out->packetType = readU32LE(data + 0);
   // bytes [4..20] = 16-byte ASCII instance-id + NUL @ +20
   if (data[20] != 0) return false;                    // sanity check
   // bytes [21..24] = stack/charges (currently unused)
   out->mainSlot = readU32LE(data + 25);
   out->subSlot  = uint16_t(data[29]) | (uint16_t(data[30]) << 8);
   ```
3. In `ItemCache::onItemPacket`, when `mainSlot==0 && subSlot<=22`, also push a `(slot, itemId)` update into a new `WornSlotMap` keyed by slot index. Move-to-cursor (`subSlot==0x23`) clears the slot. The map is the authoritative "currently equipped" set.
4. Rework `ItemCacheTotals` to sum over `WornSlotMap` instead of all cached items. Keep the legacy "every observed item" sum behind a flag if it's still useful as a "potential" view.
5. Proto: add a `WornSet` message (slot → itemId, plus per-slot summed stats), wired through `protoencoder.cpp`. Pull the legacy OP_ItemPlayerPacket todo entry — that opcode is obsolete on Live.

**No new opcode resolution.** OP_ItemPacket was already known/resolved on 2026-05-02; this is a wrapper-decoding refinement on top of it. No XML changes needed in `zoneopcodes.xml`.

### 2026-05-03 — OP_SendAATable = 0xa30a (static AA-definition menu, NOT purchased ranks)

Capture: existing `aa_point.vpk` (the same one that nailed OP_AAAction). Method: `--opcode-stats` triage of fire-count=4 S>C unknowns followed by `--dump-payload 0xa30a:/tmp/aa_table/op_a30a` over a daemon replay.

- **OP_SendAATable = `0xa30a`** (S>C, 15640 B, n=4 — one fire per zone-in, matching OP_PlayerProfile's cadence). All four fires are **bit-identical**, which is what proves it's a static reference table rather than per-character state.

  Layout: `u32 count` (= 314 in this capture) followed by 314 records of a 44-byte base + variable-length prereq trailer.

  Base record (11 u32 LE, internally consistent for the first 121 records walked at fixed 44 B stride):

  | Offset | Type | Field |
  |--------|------|-------|
  | +0  | u32 | `ability_id` (sequential per series; rank-1 of a series uses the series base id) |
  | +4  | u32 | `title_sid` (string id; constant within a series — e.g. 101 for Innate Strength) |
  | +8  | u32 | `hotkey_sid` (mostly equals `ability_id`) |
  | +12 | u32 | `type` (1/4/8/… — likely tab/category code) |
  | +16 | u32 | `cost` (recurring 100/101/102/103/104; varies by tier) |
  | +20 | u32 | `rank` (1..N within the series) |
  | +24 | u32 | `?` (often 1 or 2 — possibly `class_mask` or `tier_index`) |
  | +28 | u32 | `level_req` (e.g. 20→100 in 5-step ranks 1..17, then 101+ for tier-2 ranks) |
  | +32 | u32 | `spell_id` (1016, 1024, 1025, 1026, … — the spell triggered when the rank is purchased) |
  | +36 | u32 | `?` |
  | +40 | u32 | `prereq_count` (0 for many ranks; 1+ for ranks ≥ 2 of multi-rank series) |

  Sample of the first three records (sid=101 = Innate Strength tier-1):

  ```
  rec[0] @    4: id=101  sid=101  hk=101  type=1  cost=100  rank=1   _=1  lvl=20  spell=1016  _=0  prereq_count=0
  rec[1] @   48: id=102  sid=101  hk=102  type=1  cost=100  rank=2   _=1  lvl=25  spell=1016  _=0  prereq_count=0
  rec[2] @   92: id=103  sid=101  hk=103  type=1  cost=100  rank=3   _=1  lvl=30  spell=1016  _=0  prereq_count=0
  ```

  Series transitions cleanly: at rec[17] (id=118, still sid=101) the cost/spell/level_req shift (cost=101, spell=1024, lvl=101) — Innate Strength tier-2. Distinct sids in the table: 101, 201, 301, 401, … (the canonical Innate Str/Sta/Agi/Dex/Cha/Wis/Int chains) plus dozens of class/archetype-specific sids.

  Past rec[121] the records have a per-record **prereq prefix** that prepends an `(skill_id u32, min_points u32)` pair to the base. The original "the trailer is broken" intuition was wrong — the trailing 8 bytes after a base record actually belong to the *next* record as its prefix.

  **Full record schema** (validated against all 314 records in `aa_point.vpk`'s OP_SendAATable):

  ```
  Record:
    + (optional) 8-byte prereq prefix:
        u32 prereq_skill_id
        u32 prereq_min_points
    + 44-byte base:
        u32 ability_id      // unique per rank
        u32 title_sid       // string id; constant within a series
        u32 hotkey_sid      // typically equals ability_id
        u32 type            // tab/category code (1, 2, 3, 4, 8, 10, 12, 14, …)
        u32 cost            // typically 100-104
        u32 rank            // 1..N within the series
        u32 ?               // 1 or 2 (possibly tier_index)
        u32 level_req       // min character level (20, 25, 30, …, 125)
        u32 spell_id        // spell triggered (1016, 1024, 1025, …)
        u32 ?               // typically 0
        u32 has_prereq_flag // 0 or 1; predicts whether the *next* same-series record has a prefix (with edge cases below)
  ```

  Counts in this capture: **122 records with no prefix, 192 records with an 8-byte prefix, 0 with 16-byte (the 16B observation from the rough first walk turned out to be misalignment)**. Edge cases the strict "f10 → has_prefix" rule misses, both at series boundaries:

  - **rec[120]** (id=1000, rank-1 of series 1000): `f10=1` but **no prefix** — first rank of a new prereq-bearing series, nothing to point to yet.
  - **rec[312]** (id=5000, rank-1 of series 5000): `f10=0` but **has prefix** `(skill=103, min=26)` — first rank of an epic series carries an entry-condition prereq even though no later ranks reference it.

  After all 314 records the file ends with a **284-byte trailing summary block** (71 u32s) attached to rec[313] (id=5001 with constant-5001 fields):

  ```
  (9, 1016, 1, 1, 2, 5523, 5524, 1024, 1, 1, 2,
   5535, 5536, 1025, 1, 1, 2, 5537, 5538, 1026, 1, 1,
   2, 5539, 5540, 1028, 1, 1, 2, 5543, 5544, 1030, 1,
   1, 2, 5547, 5548, 1032, 1, 1, 2, 5561, 5562, 5000,
   2, 1, 1, 8311, 2, 1, 8313, 5001, 6, 1, 1,
   8315, 2, 1, 8316, 3, 1, 8317, 4, 1, 8318, 5,
   1, 8319, 6, 1, 8320)
  ```

  The pattern looks like cross-reference rows mapping pairs of late-game ability ids (5523-5548, 5561-5562) to their series ids (1024, 1025, 1026, 1028, 1030, 1032, 5000), then an additional run for ids 8311 and 8313-8320 (likely glyph or veteran-mastery slots). The exact field semantics within each row are a future-work item — the daemon doesn't need them to display the AA window, but a future "show me what AAs exist" panel would.

- **Important correction to the 2026-05-02 round-3 note**: that note speculated `OP_SendAATable` "likely carries purchased AA ranks." The bit-identical 4-fire payload disproves that — `OP_SendAATable` is the static client-side AA-window menu data, not state. Purchased AA ranks remain unresolved on the wire (OP_PlayerProfile gives `aa_spent`/`aa_unspent`/`aa_assigned` totals, OP_AAExpUpdate gives the unspent-pool counter, but the per-ability rank breakdown still has no known opcode).

  Auto-grant evidence: the auto-granted AA ids observed in OP_PlayerProfile's `aa_array` (1371-1377, 4665, 4700, 8000, 9000, 15073) **do not appear** in the `0xa30a` payload — only id `1000` is present. That confirms `0xa30a` is the *purchasable*-AA definitions table, while auto-grants are reported separately via `aa_array`.

**Ruled-out leads** (so we don't re-chase them):

- `0xa1e1` (3628 B, S>C, n=4): looked promising because its payload contains the exact set of auto-grant ids from OP_PlayerProfile (1000, 15073, 1371-1377, 4665, 4700, 8000, 9000). But fire-by-fire diff shows the payload **changes every fire** with a monotonic counter at offsets 0/4/8 (`5,5,0` → `6,6,1` → `7,7,2` → `8,8,3` across the four zone-ins), and only one additional 8-byte field changes between fire 1 and fire 4 (`(0,0)` → `(64,3)` at offset 196..203). Delta isn't structured like AA-rank state (Mend Pet × 5 + Run Speed × 3 = two records, but only one offset moved). Likely a per-zone-in sequence-number or zone-progress packet that happens to carry the auto-grant id list as static reference data; not the AA table.
- `0xb92d` (6249 B, S>C, n=4): zlib-deflated (`78 da` signature) — almost certainly map / static-content blob.
- `0xe49f` (7023 B, S>C, n=4): plaintext char-create dropdowns ("Yes/No/I Don't Care", race names) — character-creation reference data.
- `0xac8b` (494 B, S>C, n=4): item-name reference list (`Broken Key of Sands`, `Pocket Full of Keys`, …). Not AA.
- `0xa12c` (48 B, S>C, n=4): `(5,1,2,3,4,5,5,1,2,3,4,5)` — looks like a 5×2 ordering table (party slots? raid roles?). Too small for OP_SendAATable.
- `0x2e6e` (1603 B, S>C, n=4): structured but starts `(8,0x37,10,1,10,…)` — looks like spawn / NPC reference rather than ability defs.

**Round-N ideas**:
- The 284-byte trailing summary block in OP_SendAATable maps glyph/mastery ability ids onto their parent series. Field semantics within each row are unconfirmed — future work if the daemon ever needs to render the late-game AA tabs.

### 2026-05-03 — OP_RespondAA = 0xa1e1 (per-spend AA snapshot)

Capture: same `aa_point.vpk`. Method: timestamp correlation via `--list-events`.

The 2026-05-03 OP_SendAATable entry called out `0xa1e1` as a suspicious 4-fire S>C opcode that carried the auto-grant id list with a per-fire counter. The hypothesis was "per-zone-in zone snapshot." Wrong: it fires **per OP_AAAction spend event**, not per zone-in. Timestamp evidence:

| event | times relative to PP fire 1 (ms) |
|-------|----------------------------------|
| OP_PlayerProfile (S>C 37108B) — zone-ins | 0, 98, 252, 353 |
| OP_AAAction (C>S 16B) — spend events | 303, 305, 305, 306 |
| `0xa1e1` (S>C 3628B) — 4 fires | 303, 305, 306, 306 |

The four `0xa1e1` fires line up with the four `OP_AAAction` C>S spend events within ~1 ms — server's "spend confirmation" reply.

**Payload layout** (validated against fires 1-4 of `aa_point.vpk`):

| Offset | Type | Field |
|--------|------|-------|
| +0  | u32 | `total_purchased_ranks` (5, 6, 7, 8 across the four spends) |
| +4  | u32 | duplicate of `total_purchased_ranks` |
| +8  | u32 | `last_action_progress` (0, 1, 2, 3 — Run Speed rank as it tickles up) |
| +12 | u32 | constant 5 in this capture |
| +16 | u32[3] | zeros |
| +28 onward | record[] | 12 B each: `(ability_id u32, rank u32, _ u32)` |

Records observed in fire 4 (post-spend snapshot):

```
@ 28: (1000,    0, 0)   auto-grant
@ 40: (15073,   0, 0)   auto-grant
@ 52: (1371,    0, 0)   vet-reward auto-grant
... (1372-1377 same shape)
@136: (4665,    0, 0)   auto-grant
@148: (4700,    0, 0)
@160: (8000,    0, 0)
@172: (9000,    0, 0)
@184: (163,     5, 0)   Mend Pet rank-5 (purchased!)  ← was already (163,5) in fire 1; bulk buy preceded fire 1
@196: (64,      3, 0)   Innate Run Speed rank-3 (purchased!) ← grew (62,1)→(63,2)→(64,3) across fires 2-4
@208 onward: zeros (empty slots up to ~3628 B)
```

Auto-grant entries match the OP_PlayerProfile `aa_array` set exactly. Purchased entries carry the rank count in the `rank` field (vs. `rank=0` for auto-grants). The `ability_id` field is the AA_Ability id (per-rank id from OP_SendAATable's record[0]) of the *highest* rank purchased, not the AA_Skill id used by OP_AAAction (which had ability=58 for Mend Pet, ability=13 for Run Speed).

Cross-validation: at fire 1 (after Mend Pet × 5 bulk buy, before any Run Speed), the (163, 5) Mend Pet entry is already present and the Run Speed slot is `(0, 0)`. Fire 2-4 walk the Run Speed slot from `(62, 1)` → `(63, 2)` → `(64, 3)` as each individual rank is bought.

**Implication for the daemon**: OP_RespondAA is a per-action delta. The full per-ability snapshot also lives in **OP_PlayerProfile's `aa_array`** (see correction in the 2026-05-02 PP entry above) — fires on every zone-in and contains both auto-grants and purchased ranks. So a daemon connecting mid-session WILL see the player's complete AA breakdown via PP, no need to wait for a spend. OP_RespondAA is the convenient real-time delta; OP_PlayerProfile is the authoritative snapshot.

**Negative result on OP_SendAAStats**: methodically checked all n=4 S>C zone-stream opcodes in `aa_point.vpk` for fire-3-vs-fire-4 (pre-spend vs post-spend zone-in) payload diffs. PP's `aa_array` already carries this state on every zone-in, so OP_SendAAStats may genuinely be obsolete — the per-ability rank breakdown doesn't need a separate opcode.

- 0x7932 (804/100 alternating) — zone-area-specific data, not AA state
- 0xb634 (1/5 alternating, 1 byte) — too small
- 0xe44a (24 B all fires) — values like (56320, 900000, 180000, 6, 0, 0) and (3087136512, 900000, …) don't fit AA shape, look like a session/timer field
- All other n=4 zone-stream S>C opcodes are bit-identical across fires (static reference data)

So **OP_SendAAStats does not fire on zone-in in current Live**, at least not as a separate opcode. The 2009 reference may be obsolete; the per-spend OP_RespondAA mechanism appears to subsume it. Marking OP_SendAAStats with that note in the tier list rather than `[x]`.

**Round-N ideas**:
- Capture a fresh login (just-zoned-in, mid-session reconnect) where the player already has spent AAs, to verify nothing else carries the per-ability breakdown. If confirmed, the daemon's "AA tab" view is fundamentally limited to in-session spends unless we also persist the OP_RespondAA snapshot to disk like the item cache.
- Decode the leading 4 u32s of `0xa1e1` more carefully — `last_action_progress` might actually be "rank within most-recent series" and the constant `5` at +12 might be a per-character constant worth understanding.
