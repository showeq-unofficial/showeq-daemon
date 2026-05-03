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
- [ ] OP_RespondAA
- [ ] OP_SendAAStats
- [ ] OP_SendAATable — likely carries purchased AA ranks (see 2026-05-02 entry)

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

**aa_array on this opcode is auto-granted only.** Every entry has `value=0`. Auto-granted ids observed across captures: `1371-1377` (sequential vet rewards), `4665`, `4700`, `9000` on the lvl 1 capture; the lvl 60 capture adds `1000`, `15073`, `8000`. **Purchased AA ranks** (Origin, Veteran's Enhancement, etc.) — even at 1/1 — do **not** appear here. They almost certainly come over **OP_SendAATable**, still unresolved.

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
