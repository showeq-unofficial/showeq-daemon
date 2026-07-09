# EverQuest Legends — opcode table (work-of-record)

New EQ-family target (first seen 2026-07-02). Same SOE **stream** protocol as
Live EQ (SessionRequest carries the "Everquest" magic; zlib; Combined/Ack/
Oversized framing — the daemon deframes it unchanged), but the **application
opcode table is fully remapped** — every app opcode reads `unknown` against the
Live table. This file tracks the Legends-specific opcode + struct mapping.

Decode captures with the raw-pcap replay path:
```
./build/showeq-daemon --replay-pcap <capture>.pcap --config-dir conf --no-listen \
    --opcode-stats out.opcodestats.txt --list-events out.events.txt \
    --dump-payload 0xXXXX:out/prefix
```
Captures live in the meta-repo `captures/` dir (gitignored — plaintext creds/
session data; never commit, use generic placeholders in this doc).

Server topology (Daybreak netblock `69.174.201.x`): login `:15900`, world
`:9000`, zones on dynamic high ports, chat `:9877`.

**Game-design notes (affect struct layout):**
- A character has **3 simultaneous classes** (not one). Expect the char-profile /
  skills / spell-affinity structs to carry 3 class ids + possibly 3 parallel
  class-data blocks — don't assume the single-`class_` layout of Live
  `charProfileStruct`. Watch for triplicated class/level fields.

## ⚠ PATCHED 2026-07-07 — full opcode re-map (IDs shuffled + some layouts changed)

EQL patched 2026-07-07. **Every app opcode ID moved** (stream layer unchanged), and
at least ClientUpdate + ZoneSpawns **struct layouts also changed**. All `0x...` ids in
the pre-patch "Confirmed" sections below are DEAD — kept for method/evidence only.
Re-mapped from a **Nektulos Forest** login+zone capture (server/world name is
"rivervale"; a `.vpk`, so `--list-events` has real capture-time) with in-game ground truth: player `/loc`
`2246.50,-954.77,-4.97`; con targets Dragoon_J`len(25225,L50,amiably),
Sergeant_C`Orm(11626,L50,amiably), Vol_T`Vke(12220,**L60,warmly**); NPC locs
C`Orm `2324.94,-990.11,-4.92` + Vol `2337.45,-802.35,-5.86`.

⚠ **Coordinate gotcha (read before touching positions):** EQ `/loc` prints **(Y, X, Z)**,
not (X, Y, Z). The position offsets in this doc label wire fields by GAME axis
(`gameX`/`gameY`); the `seq-backend-eql` parsers bind `x=gameX, y=gameY` to match the
daemon's neutral Spawn convention (which negates X/Y for screen — `protoencoder`). The
first re-derivation matched decoded values to `/loc` *in order* → bound x/y **backwards**;
everything was self-consistent (cross-checks passed) but X/Y-transposed vs the map. Fixed
`60b2a79`. Live is unaffected — the swap lives in the eql backend, which is the point of
per-backend decoders.

| Opcode | pre-patch | **new id** | status |
|--------|-----------|-----------|--------|
| OP_TargetMouse   | 0x1bfe | **0x2867** | ✅ confirmed 19/19 (value-match; 0=untarget). Layout unchanged (`{u32 spawn_id}`) |
| OP_Consider      | *(new)* | **0x4212** | ✅ NEW. 24B `{u32 self=27090, u32 target, u32 faction, u32 =7}`; C>S req has faction=0, S>C reply fills faction (**2=warmly, 4=amiably**; level comes from spawn). Companions: `0x5b5e` target-HP reveal `{u32 target, u32 cur_hp,…}`, `0x0e54` `{0,target}` |
| OP_ClientUpdate  | 0x0b03 | **0x7171** | ✅ **C>S 42B self FULLY DECODED**; **S>C 28B = other-player positions** (identified 2026-07-08, layout partial — see entry below, unwired). C>S: spawnId u16@2; wire has gameY@10 / gameX@18 / z@30 (f32) → bound **x=gameX@18, y=gameY@10**; velocity deltaX@26 / deltaY@6 (f32); **heading u16@14, 11-bit (0–2047, North≈0)** — Sense-Heading-confirmed. deltaZ candidate @22. (X/Y-swap fixed `60b2a79`) |
| OP_ZoneSpawns    | 0x7475 | **0x4606** | ✅ id (var 343–352B NPC / 486B rich). level@block+4 OK. **POS from block END**: three u32 words z@(len-95), **y=gameY@(len-91), x=gameX@(len-87)**, each a **signed 19-bit fixed-point (×8) coord in the word's low bits** (Live spawnStruct-style packing; upper 13 bits = other subfields). The earlier `/8 i16` read was the same field truncated — wrapped past ±4095. hp/body offsets TBD. **C>S 92B = client zone-entry / spawn-list request** (name@4 + session token + fixed signature block; identified 2026-07-08, see entry below) |
| OP_MobUpdate     | 0x061b | **0x67e0** | ✅ id (14B, ids 416/416 match spawns). **Byte-identical to Live `spawnPositionUpdate`** (2026-07-08): spawnId u16@0 + 2 zero bytes, then packed `y:19 z:19 u3:7 x:19` (fixed-point ×8, u64@4) + `heading:12`@12 — decoded by the shared `decode_mob_update`, full range, no wrap. The old i16-offset read (y@4/8, z@6/64, x@10 "unscaled") was a truncated window of these bitfields. Sparse full-position sync; continuous movement is OP_NpcMoveUpdate |
| OP_NpcMoveUpdate | 917c (dead) | **0x7352** | ✅ **2026-07-08**. The continuous per-NPC movement stream (var 15–21B, top S>C opcode: 734 fires/51 ids vs MobUpdate's 591). **Byte-identical bit format to Live** OP_NpcMoveUpdate → decoded by the shared `decode_npc_move_update`; only the opcode-id mapping was wrong (917c never appears on the wire). Carries pos **+ velocity + heading**. Confirmed: all 51 ids (16b BE spawnId) + decoded x/y/z match the 0x67e0 stream. Fixes "mobs freeze while moving, jump on stop" |
| OP_ItemPacket    | 0x74b0 | **0x6805** | ✅ id (bulk items; names + `Benefit:`/`Trophy:`) |
| OP_ZoneServerInfo| —      | **0x35d4** | ✅ world 130B, zone-server host `…everquestlegends.com` |
| OP_EnterWorld    | 0x0839 | **0x26bf** | ✅ **2026-07-08**. World C>S 72B, **byte-identical to Live**: char name zero-padded @0 + 64B zeros + `0xffffffff` trailer; server reuses the opcode for a 1B S>C ack. Confirmed 2/2 login captures via dump-payload (name = known char). Mapping it lights up NamePromoter on eql (matches by opcode name + 72B len) → boxes are named at the **world handshake**, before zone-in; `--only-session <charname>` relays from that point. Prior 9bdc was a dead pre-patch guess |
| OP_ChatServer    | —      | **0x4de7** | world 67B, chat connect `host,9877,rivervale.<char>,token` |
| OP_PlayerProfile | 0x5207 | **0x62f0** | ✅ id (~40KB, embeds char name + inventory). name-stub = `0x46df` (656B). race/class/level offsets TBD |
| OP_NewZone       | 0x5ab6 | **numeric id (opcode TBD)** | swept all 177 opcodes: **no zone-name text on the wire** — post-patch the zone is a numeric id (old EQL reused classic ids, `nektulos=25`). `0x1e94` (world 1932B) carries the *server* name "rivervale", NOT the zone. Find the S>C op carrying zoneid 25 at zone-in (or profile's current-zone field); map id→shortname client-side |
| OP_Action2       | 0x32a9 | **0x1734** | ✅ **WIRED 2026-07-08**. Combat/damage stream (S>C, 48B, n=481/968). **Byte-identical to Live `action2Struct`**: target u16@0, source u16@2, damage i32@8, spell i32@20 (**-1=melee** for 422/481), type u8@40; @24–39 = knockback floats. Sole 48B S>C op. Lit up the already-wired `CombatRouter::action2` → replay emits **481 CombatEvent envelopes** (real src/tgt/dmg, e.g. 13167→13154 dmg107) |
| OP_Action        | 0x049e | **0x73de** | ✅ **WIRED 2026-07-08**. Spell/special action (S>C). **Live paired send**: 64B `actionStruct` (n=97/117) + 88B `actionAltStruct` (n=22/43); target/source @0/@2, spell u16@4 (real ids 502/445/821…). Sole 64B/88B op → `SpellShell::action` (both payloads), 119 fires |
| OP_DeleteSpawn   | 0x94d4 | **0x59a1** | ✅ **WIRED 2026-07-08**. Spawn removal/death (S>C, 4B `{u32 spawnId}` = `deleteSpawnStruct`, n=9). Time-correlated: **8/9 ids stop receiving MobUpdate at/after the event** (mobs killed in combat). → `SpawnShell::deleteSpawn`; replay emits SpawnRemoved for all 9. ⚠ `0x67a8` is the 4B look-alike but = combat **engage/disengage** (ids keep moving after; id=0 clears) — NOT despawn |
| OP_Death         | 0x1eb2 | **0x66cb** | ✅ **WIRED 2026-07-08**. Death/corpse (S>C, 40B = `newCorpseStruct`, n=8). Decodes field-for-field: victim u32@0 (**all 8 ∈ the DeleteSpawn set**), killer u32@4 (=player 13167 every kill), corpse type@12, killing-blow spell u32@16 (-1=melee), damage u32@24 (9–80). → already-wired `SpawnShell::killSpawn`; replay emits **8 `SpawnKilled`** (0→8). Fires just before the matching DeleteSpawn |
| OP_Animation     | 0x6dba | **0x1293** | ✅ **2026-07-08** (id-only, no handler). Animation broadcast (S>C, 4B, n=354/718). **Live `animationStruct`**: spawnId u16@0, action u8@2 (1–46), speed u8@3 (**=10 across all 354**). Remapped so it resolves in opcode-stats; animation isn't surfaced, so no handler wired |
| *(target HP reveal)* | — | **0x5b5e** | ⚑ 2026-07-08 candidate (unwired). On-target HP reveal (S>C, 13B primary, n=12/22): `{u32 spawn_id, u32 cur_hp, u32 =0x07000001, u8 0}`; cur_hp is **absolute** (=0 for a just-dead spawn). Consider-companion (cf. 0x0e54 `{0,target}`). NOT a continuous %-HP bar stream |

**RE-WIRED 2026-07-07** (decoder-rs + daemon): all ids updated in `conf/eql/opcodes.toml`;
the `seq-backend-eql` parsers re-derived — ClientUpdate pos `x=gameX@18 / y=gameY@10 / z@30`
(f32); ZoneSpawns pos from the block END (`z@(len-95)/8, x=gameX@(len-87)/8, y=gameY@(len-91)/8`,
both 330B/486B blocks), id@0/level@4/hp@44,45; MobUpdate `x=gameX@10, y=gameY@4/8, z@6/64`;
new `parse_legends_consider` (24B) → shared `Consider` → the neutral
`SpawnShell::consMessage` (passed the real payload len, not `sizeof(considerStruct)`) →
`spawnConsidered`→`Considered`→web. Verified on the capture: opcodes resolve by name,
positions `/loc`-correct, con fires for all 3 con'd mobs (Considered ids 12220/11626/25225),
target 38 envelopes; **live 9/9 unregressed**.

**OP_NpcMoveUpdate = `0x7352`** (2026-07-08): the continuous NPC movement stream — the
missing packet behind "mobs freeze while moving and only jump when they stop". It's the
single highest-frequency S>C zone opcode (734 fires / 51 ids in the Upper Guk capture, vs
OP_MobUpdate's 591), variable 15–21B. **Byte-identical bit layout to Live's OP_NpcMoveUpdate**
(MSB-first BitStream: 16b BE spawnId + 16b pad + 6b fieldmask + 19/19/19 y/x/z sign-magnitude
`>>3` + 12b heading + optional pitch/deltaHeading/animation/deltaX/deltaY/deltaZ per mask).
Method: dumped `0x7352`, saw per-id repeats with a 16-bit BE id that byte-swaps to a known
`0x67e0` id; ran the *existing* `decode_npc_move_update` over every payload — **all 51 ids +
decoded x/y/z land inside their 0x67e0 position ranges**, and the packet lengths match the
field-mask bit math exactly (fs=0x1c→18B, 0x02→15B, 0x3d→21B). Fix was a **one-line opcode-id
correction** in `conf/eql/opcodes.toml` (`917c`→`7352`; the old id never appeared on the wire);
the `SpawnShell::npcMoveUpdate` wire in `wire_eql.cpp` and the Rust decoder were already in
place. Carries velocity + heading, so the web gets smooth motion, not just denser jumps.
Replay-verified: 734 events decode, no length warnings.

**SUPERSEDED 2026-07-08 (same day): the "16-bit wrap" was a mis-read of Live's bit-packed
`spawnPositionUpdate` — the phase-unwrap below is REMOVED.** The 14B payload is byte-identical
to Live's struct: `spawnId u16@0` + 2 zero bytes + packed `y:19 z:19 u3:7 x:19` (fixed-point ×8,
u64@4) + `heading:12`@12. The i16@4/8 read returns the LOW 16 of y:19 → gameY **mod 8192**
(the observed wrap); i16@6/64 returns z bits 3–12 → gameZ **mod 1024** (the old Z unwrap
period); i16@10 lands exactly on x bits 3–18 → gameX unscaled, full i16 range (why X "never
wrapped"). Every observed quirk was an artifact of the truncated reads. Proof over 1665
MobUpdates across 3 captures (upperguk / unrest / nektulos): y's bits 16–18 and z's top 6 bits
are **perfect sign-fill** (0 violations — impossible for independent fields), all 8 sub-unit
fraction values occur on every axis, bytes 2–3 ≡ 0, u3 ≡ 0, heading decodes as clean 12-bit.
The prior "no wider field (float/int32 scan = 0 hits)" conclusion missed it because a 19-bit
field straddles byte boundaries — neither scan could see it. OP_ZoneSpawns positions are the
same packing (19-bit LSB of u32 words at len-95/-91/-87). Fix: eql routes `decode_mob_update`
to the shared Live parser and `parse_legends_zone_spawn` decodes 19-bit words; the
`SpawnShell::spawnPos`/zone-bounds unwrap machinery is deleted. Bonus: MobUpdate now yields
**heading** too. Historical record follows:

**OP_MobUpdate Y is a 16-bit WRAP — fixed by phase-unwrap** (2026-07-08): once
OP_NpcMoveUpdate was live, high-Y mobs flickered — correct while moving (NpcMove, 19-bit)
then snapping ~8192 units **south** on each OP_MobUpdate. Cause: OP_MobUpdate encodes Y as
signed `i16` fixed-point (raw/8), so any coordinate past ±4095 wraps by 65536/8 = **8192
game units** (Z likewise: raw/64 → 1024). Confirmed on Qeynos Hills (zone spans ry≈-611..
+5200, wider than the ±4095 an i16/8 field holds): a mob NpcMove-true at y=4724 reads
i16 ≈ −3400 every MobUpdate, and −3400 **+8192** = ~4790. The OP_ZoneSpawns position wraps
identically (23/32 high-Y spawns match tail±8192); **neither carries a wider field** (float
/int32 scan = 0 hits). Since NpcMoveUpdate is unambiguous 19-bit and fires for every roaming
mob, the fix phase-unwraps each MobUpdate coordinate toward the spawn's last known position
(`EqlDispatch::mobUpdate` + new neutral `SpawnShell::spawnPos`; wrap period Y=8192, Z=1024).
Replay-verified against the capture: every wrapped MobUpdate resolves back to its NpcMove
anchor, in BOTH directions (a genuinely-south mob whose MobUpdate wraps north is pulled back
south too). Residual: a high-Y mob shows wrapped until its FIRST NpcMove anchors it (roaming
mobs self-correct in <1s; a truly-never-moving high-Y NPC would stay wrapped — none observed,
the fixed named NPCs are all low-Y). Recon aid added: `--dump-all-sessions` (forward every
box to the recon dumpers, beating the primary-box limitation that hid the high-Y zone).

**PlayerProfile `0x62f0` VERIFIED** (2026-07-07): the identity header survived the patch —
race u32@21 (=6 DarkElf), class1 u32@25 (=5 SHD), level u8@33 (=12), confirmed against a known
L12 SHD/DRU/MNK char. `class@25` is the **primary of 3** (3-class design: SHD/DRU/MNK = 5/6/7);
2nd/3rd class ids sit in a separate block ~@12094 — not surfaced (neutral `setIdentity` carries
one class). Parser unchanged.

**Current zone = OP_PlayerProfile `0x62f0` `u16@36211`** (corrected 2026-07-08). Found by
cross-diffing a Nektulos vs an Upper Guk capture — the **only** field that flips
`25→65` (nektulos→guktop). It's the `{zoneId, x, y, z}` current-location record in the
profile. Wired in `EqlDispatch::profile` → `ZoneMgr::setZoneById` (resolves via `zones.h`)
→ map loads.

⚠ **Correction:** the first attempt wired `0x4bc8@6` and was WRONG — that's the **BIND**
zone (where the char binds, = nektulos), *identical across zones*, so it showed 'nektulos'
while the player was in Upper Guk. `0x4bc8` is now **UNWIRED** (kept identified as the
zone-in/bind marker). Lesson: a single-zone capture can't tell bind from current — needs
≥2 zones. Caveat on the fix: `@36211` is a deep offset in a ~40KB variable-length profile,
so it may shift with big inventory changes — re-derive if the zone resolves wrong.

**Still TODO:** ClientUpdate heading+deltas (left 0 — no facing arrow / speed-between-updates;
need a `/loc`-while-turning capture). That's the last open piece.

### 2026-07-08 — Reverse-direction sweep: OP_ClientUpdate S>C (28B, other players) + OP_ZoneSpawns C>S (92B, zone-entry request)

Read-only sweep of the existing eql fixtures (`fulllogin`, `login-zone`, `chat`, `upperguk`) for
opcodes that fire **both** directions but only had one side wired. Method: replay against the eql
build + `--dump-payload OP:PATH --dump-all-sessions`, split files by dir/size, diff samples.
`dir=1`=C>S, `dir=2`=S>C. No `typename`/struct edits made (struct strategy deferred — see below).

**OP_ClientUpdate `0x7171` — S>C 28B = OTHER-PLAYER position broadcast. Identity CONFIRMED, field layout PARTIAL.**
- Direction split: C>S 42B (self, wired) + **S>C 28B** (1 fire in fulllogin; **230 in login-zone across 24 distinct ids**).
- Confirmed *other players*, not a self-echo: the C>S self id (`0x69d2`) never appears in the S>C
  stream, and **20 of 24 S>C ids are absent from the OP_MobUpdate (NPC) id set** → player-controlled
  spawns. Same split as Live (NPCs → MobUpdate/NpcMoveUpdate; players → ClientUpdate both ways).
- Structure (moving-sample diffs, busiest id n=78): `u16 spawnId@0`, `u16 pad@2 (=0)`, motion/flags
  block @4–11 (`@4=0x1b` marker once moving), state/pose byte `@12` (`0x13` while active), `@13–15=0`,
  then per-axis **`u16 pos` + `i16 vel`** pairs @16–23 (`@16` pos climbs monotonically while walking,
  `@18` vel tracks speed; `@20/@22` the second axis), heading candidate `@24` (small, tracks turning),
  `@26–27=0`. **NOT** MobUpdate's 19-bit MSB packing — its own byte-aligned pos+vel layout.
- **Axis→game-axis binding + scale are UNCONFIRMED and cannot be finished from existing fixtures:**
  self isn't echoed S>C, and the 4 S>C ids that overlap MobUpdate have only 1–2 samples each (ranges
  too wide to disambiguate a field-value scan). **Finish with a purpose capture** — a 2nd known
  character at a known `/loc` walking cardinals — the same method that pinned the 42B C>S self.

**OP_ZoneSpawns `0x4606` — C>S 92B = client zone-entry / spawn-list request. Identity CONFIRMED, framing MOSTLY MAPPED.**
- Fires **exactly once per zone-in**, C>S; the S>C side (spawn blocks) is already wired. 8 samples
  across several zone-ins (same character).
- Framing (offsets/roles only — identity bytes intentionally not transcribed): `u32 @0` = per-session
  token (constant within a login session, differs across sessions); **null-terminated char name @4**;
  `u32 @0x40` varies per zone-in (zone/position-dependent); `@0x44–0x53` = a fixed client/character
  signature block (byte-identical across every session + zone captured); short trailer @0x54–0x5b.
  Carries client **identity**, not spawn data — "I'm here, send the zone" → server replies with the
  S>C spawn stream.

**Also mapped in the sweep (unwired unknowns, for later):**
- `0x1bdc` (24B, bidirectional, S>C-heavy) — **CONFIRMED OP_SpawnAppearance2**, byte-identical to the
  neutral 24B `spawnAppearance2Struct` (`u32 spawnId@0` [hi-16 always 0], `u32 type@4`, `u32 value@8`,
  `u8 pad[12]`). 55 spawn ids; type→value fits SpawnAppearance semantics (type 22→val 0; type 43→val
  0/1/2 flag; type 6→val 100/110/115; type 5→val 7/11/12). A **name-swap** (not a mechanism customer).
- `0x4f7a` (12B S>C ×200, steady in every capture) — periodic self-referential tick: `u32 counter@4`
  (monotonic 0..N), constant self entity-id `@8`, optional other-id `@0`. Low tracker value.
- `0x5c17` (C>S variable + S>C 23B×31) — probable request/response pair (bytes unexamined).

**Struct/decoder architecture — DECIDED (2026-07-08): Rust-sourced sizes ("everything swappable").**
The `SZC_Match` size-gate is a pure `name→size` map (`addStruct`); eql never casts a C++ struct
(Rust decodes), so eql needs only a *number* per opcode. Plan: byte-identical opcodes point their
toml `typename` at the existing **neutral** C++ struct + `match`; eql-unique sizes get a per-backend
size table exported from `seq-bridge`/`seq-backend-eql` and registered via `addStruct(name, size)` —
no C++ eql structs, neutral names, no reshaping, Rust is the reference.

**Landed (byte-identical cleanups, verified regression-free):**
- `OP_MobUpdate` 14B → `typename="spawnPositionUpdate"`, `SZC_Match` (was `uint8_t`/none + in-dispatcher `len==14`).
- `OP_ClientUpdate` C>S 42B → `typename="playerSelfPosStruct"`, `SZC_Match` (size matches even though the eql field layout differs; decode stays Rust).
- `OP_SpawnAppearance2` = **`0x1bdc`** (24B) → reuses the neutral `spawnAppearance2Struct`; adding the id mapping **lit up the already-present-but-dormant `wire_eql.cpp` handler** (`SpawnShell::updateSpawnLock`). No wire/handler change — just the toml id. Now resolves as known; golden byte-identical (handler no-ops on these captures' types; armed for lock-ruleset eql servers).
- Verified: fulllogin replay → MobUpdate 658 / ClientUpdate 706 events (unchanged), 0x1bdc→OP_SpawnAppearance2 known, golden byte-identical run-to-run + across the SpawnAppearance2 change, 0 size warnings.

**Method note (from this pass):** the daemon has a large catalog of neutral structs (`s_everquest.h` size index); many eql "unknown" opcodes are just Live opcodes remapped, so the play is to match eql `(size, dir, behavior)` against an existing neutral struct + reuse the Live opcode name (as with `spawnAppearance2Struct`), NOT to invent structs or deep-crack bytes. `dispatchFor` also has a **last-payload fallback** (a `wire()` typename that doesn't exactly match a toml payload binds to the opcode's LAST payload rather than erroring) — so always exact-match the toml payload; a mis-typed wire silently mis-binds instead of failing loudly.

**Pending (need the Rust size-table mechanism — NOT pure name-swaps):** `OP_Consider` (24B; the C++
`considerStruct` is **32B**, not a match) and `OP_Animation` (4B; **no `animationStruct` exists** in
the daemon header). And the 28B S>C ClientUpdate once its layout is cracked.

### 2026-07-08 — OP_PlayerProfile `0x62f0` internals: multiclass level table (CONFIRMED) + class-data / loadout / storage leads

Capture: `tests/replay/eql/eqlegends-fulllogin-20260708.vpk`. Method: `--dump-payload 0x62f0`
(one 41271-byte profile), structural analysis against player-supplied ground truth — a 3-class
SHD/DRU/MNK char where every *un-played* class is parked at a level-**10** floor and the active
classes level independently. `level u8@33` reads **20** here (the "=12" in the note above was an
earlier/stale capture; the char leveled).

- **Per-class level table — CONFIRMED.** The profile **ends** with a length-prefixed table:
  `u32 count (=16)` then **16× u8**, `byte[i]` = level of class `i+1` (WAR..BER), then a `0x00`
  pad. Here classes 5/6/7 (SHD/DRU/MNK) = **20**, every other class = **10** (the un-played
  floor) — matches ground truth exactly. Tail bytes:
  ```
  ff*28  10 00 00 00  0a 0a 0a 0a 14 14 14 0a 0a 0a 0a 0a 0a 0a 0a 0a  00
         └ count=16 ┘  └ WAR CLR PAL RNG [SHD DRU MNK=20] BRD..BER=10 ┘  pad
  ```
  Locate robustly (absolute offset shifts with profile size): it's the last section —
  `count = u32 @ (len-21)`, `levels = bytes[len-17 : len-1]`; it sits immediately after a run
  of `0xff`. Anchor from EOF / the `0xff` run, not a fixed offset.

- **3 parallel class-data records — STRONG lead.** 3× 20-byte records at `~@40966` (stride 20,
  marker `u32 0x37530004`, constant `0x00020001 0x00110000` header + 2 varying fields) — one per
  active class, i.e. the "3 parallel class-data blocks" the 3-class design predicts (spell
  affinity / per-class state). Not yet decoded field-by-field.

- **Loadout / race+class records — PLAUSIBLE lead.** `{u32 race=6, u32 class=5, …small ids…,
  0xffffffff empty slots}` at `~@41058` and `~@41162` (the `{6,5}` = DarkElf/SHD pair recurs
  there, besides the `@21` header). Shape = {race, primary class, gear-id slot array} → the
  loadout / "be any race+primary class" feature. Count/stride not yet pinned.

- **Magical storage / bank — CANDIDATE.** Item-id slot arrays with `0xffffffff` empties scattered
  through the blob (e.g. a 10-empty-slot run `@35981`; plausible item ids 341/242/252/91/5012/340).
  Char-bound storage vs bag/bank is indistinguishable from a single snapshot — needs a paired diff.

**Next capture (Mode C paired diff) to confirm storage + loadouts:** record ONE `.vpk` that
brackets a single change with two profile fires (OP_PlayerProfile fires per zone-in), then
`--dump-payload 0x62f0` → `pp.1.bin` (before) / `pp.2.bin` (after) and diff the two:
- storage: zone in, deposit/withdraw ONE known item id in magical storage, re-zone.
- loadout: zone in, swap/edit a loadout, re-zone.
The single u32 item-id that appears/vanishes localizes the storage array; the changed
`{race,class,gear}` block localizes loadouts.

### 2026-07-08 — `0x2735` = formatted-message channel (S>C); Sense Heading decoded

`0x2735` is the high-volume S>C **message channel** (1026 fires in the Nektulos
capture, 571 in Upper Guk; variable 5–53 B). It's multiplexed — each message type has
its own small struct keyed by an eqstr/dbstr string-id; render via
`~/.showeq/eql/eqstr_us.txt` (id → template) + `dbstr_us.txt`.

**Sense Heading** message = **6 B** `{u32 direction_string_id, u16 unk}`. The direction
id is an eqstr id `12427–12434` = **N / NE / E / SE / S / SW / W / NW** (clockwise), which
the client renders into `12435 "You think you are heading %1."`. Confirmed: two 6-B
payloads `91 30 00 00 …` = `12433` "West".

Notes / gotchas:
- The channel's leading `u32` is often the **entity id**, not a string-id — watch for
  coincidental eqstr collisions (the player id `13167` matches eqstr `13167 "Current
  mouse speed…"`, a false positive). The real string-id offset varies per message type.
- **`net opcode 0000` — TWO sources, neither is dropped EQ data** (2026-07-08):
  1. In a **raw pcap taken with a broad `host` filter**: mDNS / service-discovery
     (`EQBOX1.local`, `_dosvc._tcp.local`, Bonjour) — LAN multicast on port 5353. Fixed
     by excluding `port 5353` + `net 224.0.0.0/4` from the open capture filter (`1d3dceb`).
  2. On the **live/port-restricted stream** (real EQL traffic): an **ENCRYPTED C→S
     channel** multiplexed on the game socket (fires during combat/in-zone activity, not
     zone-in — 0 in the login-zone vpk, 78 in Upper Guk; live Unrest showed 82–482B
     bursts). Rigorously verified opaque (dumped 40): no plaintext EQ structure at any
     offset (no spawn-ids / names / eqstr), payloads high-entropy and 40/40 unique
     (~5.33 bits/byte on 48B ≈ random), plaintext framing header + a per-packet 16-byte
     field = an **AEAD nonce** signature. Not decodable without the client key, and NOT
     the server game-state we decode (that's the S→C SOE channels, intact). Dropped
     silently with a one-time announce (`44f5a76`).
  Separately, the **`calcCRC16 called for length > 1048576`** spam (~every 10s) was a
  **1-byte packet** (netOp `0x00ff`) underflowing `rawPacketLength()-2` (unsigned) into a
  ~4.29GB length — guarded in `calculateCRC` (`c12bf31`). `0x2735` messages are **not**
  dropped by any of this; the missing Sense Headings were client-side text.
- This channel is the foundation for wiring EQL formatted / combat / system message
  **text** into the daemon+web (via eqstr/dbstr) — not yet done.

**2026-07-08 — 0x2735 is NOT cleanly wireable as a chat channel (needs a dedicated
capture).** Attempted to wire it; deep-decoded 571 (upperguk) + 1026 (login-zone) payloads.
Finding: **the channel is predominantly a per-ENTITY event stream, not text.** Of the 571
combat-capture messages, **388 have a spawn/entity id at `u32@0`** and only ~4 are genuine
system text (2× eqstr 12116 "You groan and feel a bit weaker", 2× 12433 "West"/Sense
Heading). The apparent "173 text" is a **false positive**: the player id `13167` collides
with eqstr 13167 "Current mouse speed multiplier is %1." (169×). Root cause — **the entity-id
and string-id number spaces overlap** (the "OTHER" `u32@0` values 11633/11715/… are
simultaneously live spawn ids AND `dbstr_us.txt` entries; 950/1088 resolve in dbstr), so the
leading u32 cannot be classified entity-vs-string from the packet alone. The subtype byte
`@4` (0x02/0x04/0x0f/0x23/…) does NOT separate them either (genuine text sits in @4=0x04
*mixed with* entity + other). **Wiring it blind would spam the web chat with garbage** (the
169× "mouse speed" line). CONCLUSION: to wire EQL chat/system/combat text safely, capture a
**dedicated session with KNOWN content**. For each channel type a **distinctive but
ordinary-looking** phrase — pick natural words you'll remember, **NOT an obvious test marker
like "SEQTEST"** (the text goes to the live server; keep it innocuous and un-botlike) — e.g.
`/say`, `/ooc`, `/tell <box>`, `/shout`, `/auction` each with a different memorable phrase
you jot down locally to grep for; and trigger known combat/system lines ("You have slain …",
"… hits YOU for N", "You gain experience!", a resisted spell). Then `--dump-payload` every
opcode and **string-grep for those literal phrases** to pin the real chat opcode(s) + format
precisely (player chat carries LITERAL text, unlike 0x2735's string-ids). The on-disk captures contain no typed
chat, so the chat opcodes (OP_CommonMessage / OP_SpecialMesg / OP_FormattedMessage — all
handlers pre-wired, awaiting ids) can't be found in them. 0x2735 itself stays unwired until
its entity-event subtypes are separately decoded (they may overlap with already-decoded
spawn state).

**ClientUpdate heading/deltas — DONE (2026-07-08).** heading = `u16@14`, 11-bit
(0–2047 = full circle, North≈0), velocity deltaX `f32@26` / deltaY `f32@6`. Confirmed by
a Sense Heading capture (Dagnor's Cauldron):
turning through N/NE/E/SE/S/SW/W/NW, `u16@14` stepped 2043/1814/1542/1246/1036/782/492/203
(~256 = 45° apart, value falls as compass rises). Note: the Sense Heading *text* is
**client-side** (not on the wire — 0 in the capture), so the in-game log is the direction
ground truth; the packet field is what carries facing. deltas re-derived from running
segments (correlation of f32@6/@26 with Δx/Δy). **This was the last EQL decode TODO.**

### 2026-07-08 — combat opcodes from existing captures: Action2 / Action / Animation

Mined the two rich post-patch captures already on disk (an Upper Guk combat/dungeon
capture + a full-login capture) with `--dump-payload` over the top
unmapped S>C opcodes, then decoded each against Live structs (the "try Live's wire first"
rule — all three are byte-identical). Method: built the live entity-id universe from the
0x67e0 MobUpdate stream (spawnId u16@0) and cross-checked candidate id fields against it.

- **OP_Action2 = `0x1734`** (S>C, 48B fixed, n=481 upperguk / 968 fulllogin). The
  **melee combat / damage-resolution stream**. Byte-identical to Live `action2Struct`:
  `target u16@0` (315/481 ∈ live ids), `source u16@2` (274/481), `damage i32@8`
  (0–107, small melee values; 0 = miss), `spell i32@20` = **-1 (0xffffffff) for 422/481
  melee swings**, real spell-id for the rest, `type u8@40` (combat-type enum). The @24–39
  "unknown" block holds the modern knockback floats (force/heading/pushUp: e.g. f32 0.025,
  146.5). Sole 48B S>C opcode — the only other 48B fires are two singleton-count noise ops.
- **OP_Action = `0x73de`** (S>C). The classic Live **paired action send**: 64B
  `actionStruct` (n=97/117) immediately followed for some by 88B `actionAltStruct`
  (n=22/43). `target/source u16@0/@2`, `spell u16@4` = real spell-ids (502/445/821/267…).
  Sole 64B/88B opcode. This is the spell/special-action channel (vs Action2's melee).
- **OP_Animation = `0x1293`** (S>C, 4B, n=354/718). Byte-identical to Live `animationStruct`:
  `spawnId u16@0` (70% ∈ live ids; rest = doors/objects/self), `action u8@2` (values 1–46),
  `speed u8@3` = **10 in all 354 packets** (constant animation speed — the Live signature).

**Side finding — `0x5b5e` = on-target HP reveal** (S>C, 13B primary, n=12/22):
`{u32 spawn_id, u32 cur_hp, u32 =0x07000001, u8 0}`. cur_hp is **absolute** (=0 for a
just-killed spawn), id 100% ∈ live set. Fires when a spawn is targeted/considered
(companion to `0x0e54 {0,target}`). This is the *on-select* HP reveal, **not** a
continuous %-HP health-bar broadcast — that stream (Live OP_HPUpdate/OP_MobHealth
equivalent) is still unlocated; the health bar may instead be driven client-side off
Action2 `damage@8`. Ruled out for it: `0x4f7a` (12B, n≈200 in EVERY capture regardless of
activity → fixed-rate heartbeat, no id field at any offset).

**Also confirmed the same day — OP_DeleteSpawn = `0x59a1`** (S>C, 4B `{u32 spawnId}` =
`deleteSpawnStruct`, n=9). Found among the 4B-`{id}` S>C candidates and confirmed by
**time-correlation** against the OP_MobUpdate stream: for 8 of 9 fires the spawn stops
receiving any position update at/after the DeleteSpawn event (the 9th never moved) — i.e.
these are mobs dying over the combat session. The look-alike `0x67a8` (also 4B `{id}`,
n=11) is **NOT** despawn: its ids keep moving afterward and it emits `id=0` clears → it's a
combat **engage/disengage** state broadcast; left unmapped.

**KEY REALIZATION — the EQL hunt is mostly ID-supply, not handler-writing.** `wire_eql.cpp`
already wires the full Live handler set **by name** (`OP_Action2`→`CombatRouter::action2`,
`OP_Action`→`SpellShell::action`, `OP_DeleteSpawn`→`SpawnShell::deleteSpawn`, `OP_HPUpdate`,
`OP_Death`, messages, groups, spells…), each with `SZC_Match` against the Live struct size.
They never fired only because `conf/eql/opcodes.toml` still held **stale Live ids that never
appear on the EQL wire** (the "we didn't reset to ffff" problem — verified harmless: 0/67
stale ids collide with a live EQL opcode, so no misdecodes, just dead handlers). For any
EQL opcode whose payload is byte-identical to its Live struct, **supplying the correct id in
the toml is the entire fix** — the wired handler + Rust decoder light up exactly like
OP_TargetMouse did. That is what happened here.

**Status: WIRED + replay-verified 2026-07-08.** Remapped `OP_Action2 0x1734`,
`OP_Action 0x73de`, `OP_DeleteSpawn 0x59a1`, `OP_Animation 0x1293` in `conf/eql/opcodes.toml`,
rebuilt eql. All four now resolve as `known` with correct sizes (**zero `SZC_Match` drops**),
and a recorded golden over the Upper Guk combat capture emits **481 `CombatEvent`**
envelopes (real source/target/damage; melee `spell_id=0xffffffff`) and **11 `SpawnRemoved`**
(covering all 9 confirmed DeleteSpawn ids). `SpawnKilled=0` because **OP_Death's EQL id is
still unmapped** — mobs despawn but without a corpse/death event; that + a continuous %-HP
health-bar stream are the next combat gaps. damage/`type` sub-field *semantics* (some
negative `damage@8` = misses/absorbs?, large `type` values) want a controlled kill capture
(Mode C) to fully pin, but the identification and combat-log wiring are solid.

### 2026-07-08 — OP_Death = `0x66cb`; HP% has no dedicated opcode; ffff cleanup

**OP_Death = `0x66cb`** (S>C, 40B, n=8) — byte-identical to Live `newCorpseStruct`.
Found as the 40B S>C op with n=8 (= the 8 combat kills), confirmed by decoding:
`victim u32@0` (all 8 ∈ the OP_DeleteSpawn `0x59a1` set), `killer u32@4` (=player 13167
every kill), `corpse type i32@12`, killing-blow `spellId u32@16` (-1=melee),
`damage u32@24` (9–80, sane killing blows). Remapped in `conf/eql/opcodes.toml` → the
already-wired `SpawnShell::killSpawn`; replay now emits **8 `SpawnKilled`** envelopes
(was 0) with exactly those 8 victim ids. OP_Death fires just before its matching
OP_DeleteSpawn (death → corpse → remove).

**HP% (continuous per-mob health) — NO dedicated opcode found (well-supported negative).**
Exhaustively searched for a per-spawn field that drains toward 0 at each victim's death
time: tested 13 candidate opcodes (`0x52bc`/`0x5591`/`0x5b5e`/`0x42b5`/`0x6007`/`0x1bdc`/
`0x22e1`/`0x1f55`/`0x6801`/`0x3ada`/`0x6982`/`0x50a7`/`0x18e0`) **plus every internal field of
OP_Action2** — none carries a monotonic HP drain (the apparent Action2 @38–44 "drains" are
the knockback-float bytes read as int32, pure noise). Conclusion: **EQL derives mob health
client-side** from ZoneSpawns initial HP (`0x4606` hp@44/45 = 100%) + the OP_Action2
`damage@8` stream, with the **on-target absolute-HP reveal `0x5b5e`** refreshing the selected
target. A dedicated continuous-%-HP broadcast (Live OP_MobHealth/OP_HPUpdate) either doesn't
exist here or needs a **targeted capture to isolate** — the current capture is a fast
high-kill-rate multi-mob capture that buries any faint HP signal. To settle it: con/target ONE mob and whittle
it down slowly (few hits, pauses) while logging, then re-run the drain test.

**Bonus lead — `0x5591` ≈ OP_BeginCast** (S>C, 19B, n=62). Not HP (was an HP suspect): its
fields read as `{spellId@0` (values 502/445/821/91… = the same spell-ids seen in Action2/
Death), `casterId u16@4` (41/62 ∈ live spawns), `castTime@6` (0/1500/2000/2500 ms)`} — a
spell-cast bar broadcast. Needs a dedicated spell capture to pin the exact layout before
wiring to `SpellShell` (the handler + `beginCastStruct` are already wired, awaiting the id).

**Table hygiene (2026-07-08):** reset the 63 stale Live opcode ids in `conf/eql/opcodes.toml`
to `ffff` (they never appear on the post-patch EQL wire — 0/63 fire in any capture; EQL
remapped every app opcode). No behavior change; the toml + opcode-stats now honestly show
what's unmapped (206 ffff / 13 confirmed) instead of masking gaps behind stale ids. The
handlers stay wired by name — supplying a real id is all that's ever needed.

### 2026-07-08 — OP_BuffWindow = `0x18e0`; level/exp/skill absent from the capture

A Legends zone-in + combat capture (no player chat present) was searched for the
XP/level/skill opcodes.

- **OP_BuffWindow = `0x18e0`** (S>C, 12B, n=10) — byte-identical to Live `buffWindowSlotStruct`:
  `slot u32@0` = 0..9, `spellid u32@4` = 0xffffffff (empty), `pad u32@8` = 0. Fires 10× at
  zone-in (one per short-buff slot). Remapped in `conf/eql/opcodes.toml` for table honesty,
  but **id-only** — no OP_BuffWindow handler is wired on eql, so it resolves in opcode-stats
  without surfacing anything (like OP_Animation).

- **OP_LevelUpdate / OP_ExpUpdate / OP_SkillUpdate — not present in this capture.**
  Value-based search found **no level-up event** (no `levelUpUpdateStruct`-shaped 24B opcode
  with a field rising to the current level, and no `{level=N, levelOld=N-1}` anywhere) and
  **no skill-up** (no `{skillId<75, rising value}` opcode) — the capture window does not span
  those events. Ruled-out exp false positives: `0x6007` (variable 130–346B list packet — a
  fixed offset lands on different fields per size, faking a sawtooth) and `0x4f7a` (a fixed
  200-element indexed stream `{0, counter 0..199, …}`, not per-kill exp; fires exactly 200×
  per capture regardless of activity). **To crack these:** a capture must **span a level-up
  event** — OP_LevelUpdate is then 1 fire per level gain (value = new level), skill-ups
  accompany it, and the pre-wired `Player::updateLevel`/`updateExp`/`increaseSkill` handlers
  light up on the id remaps.

## Confirmed (PRE-PATCH — ids dead as of 2026-07-07, kept for method/evidence)

### 2026-07-05 — OP_ClientUpdate = `0x0b03`

Capture: a char-create + login capture.
Method: `--dump-payload 0x0b03:…` (1160 fires) + differential decode.

- **OP_ClientUpdate = `0x0b03`** (C>S, 42 bytes, n=1160). Client self-position
  report. Sole 42-byte C>S opcode in the capture — zero competing unknowns at
  that size+direction.

**Confirmed 42-byte layout (LE)** — axes pinned by a `/loc` ground-truth clip
(a `/loc`-clip capture): three `/loc` readings
time-correlated (capture-time `--list-events`) to the float fields; spot 1
matched to within 0.5s exactly (X=−858.5→f@22, Z=41.4→f@34, Y=994.3→f@38).

```c
struct legendsPlayerSelfPos {   // OP_ClientUpdate 0x0b03, C>S, 42 bytes
/*00*/ uint16_t sequence;   // monotonic per-update counter
/*02*/ uint16_t entityId;   // player entity/char id (11653 in the sample zone)
/*04*/ uint16_t unknown04;  // 0
/*06*/ uint8_t  unknown06[4]; // near-constant (0xa3ed @08); role TBD
/*10*/ float    deltaY;     // north-south velocity  (Δ of y@38)
/*14*/ float    deltaZ;     // vertical velocity     (Δ of z@34)
/*18*/ uint16_t unknown18;  // angle 0-2047, NOT heading; role TBD
/*20*/ uint16_t unknown20;  // 0
/*22*/ float    x;          // EAST-WEST position   [CONFIRMED /loc]
/*26*/ uint32_t packed;     // heading = (packed>>10)&0x7FF, 11-bit, 0=North
                            //   [CONFIRMED /loc]; other bits = anim/deltaHeading
/*30*/ float    deltaX;     // east-west velocity    (Δ of x@22)
/*34*/ float    z;          // HEIGHT position       [CONFIRMED /loc]
/*38*/ float    y;          // NORTH-SOUTH position  [CONFIRMED /loc]
};
```

Evidence chain: position/velocity pairing by physics-consistency (Δpos vs vel
field, r=.81–.84); heading field located by motion-direction correlation
(R=0.97); **axis names + heading zero-point (0=North, ~1024=South) confirmed by
`/loc`** — running north drove y@38 up 994→2551 while x@22 held ~−830, and
heading read ≈0/2047 through the run vs 1054 (≈180°/South) at the turnaround.
Heading is **11-bit (0-2047 = full circle)** — the *legacy* EQ width, not
modern Live's 12-bit.

**Still open (minor):**
- off18-19: an 11-bit-range angle field, **not** movement heading — camera/look
  yaw? target heading? animation? (falsified as heading, R=0.08).
- off6-9: near-constant (`0xa3ed` @08) — role TBD.
- off26 low bits: animation + deltaHeading sub-fields not yet split out.
- id@2 persistence across zones (single-zone dumps so far — untested).

**Recon notes / `--replay-pcap` limitations found (2026-07-05):**
- `--dump-payload` follows only the daemon's **primary box** (first world
  session seen), so a multi-zone capture yields one zone's opcode stream. To
  decode a later zone, slice the pcap to its time window
  (`tshark -Y "frame.time_relative>=A && <=B" -w slice.pcap`) and replay that.
- `--replay-pcap` stamps `--list-events` with **replay wall-clock, not capture
  time** (the tcpdump path doesn't propagate `pcap_pkthdr.ts` like the .vpk path
  does). Fine for `--dump-payload`/`--opcode-stats`; breaks Mode-B time-window
  correlation. TODO: plumb `ph->ts` through the packet cache into
  `processPackets` when `PLAYBACK_FORMAT_TCPDUMP`.

**Next capture to finalize:** stand at a known `/loc`, face north (heading 0),
then walk a single cardinal direction a known distance; the axis whose absolute
value matches `/loc` names x/y, and heading=0 at north pins the heading zero.

### 2026-07-05 — OP_ItemPacket = `0x74b0`

Capture: loc clip + char-create (byte-identical 0x74b0 stream — same character,
same inventory). Method: `--dump-payload 0x74b0` + string/struct analysis.

- **OP_ItemPacket = `0x74b0`** (S>C, variable ~1.1 KB … 19 KB, n=174). **Bulk
  item transfer** — the character's full item set, sent on zone-in. (Initial
  "zone spawns" hypothesis was WRONG: no position floats; the payload is items.)

Framing (confirmed):
- `u32 @4` = **item count N** (=1 for the ~1.1 KB payloads, =17 for the ~19 KB
  ones — matches the item serial/name count exactly).
- Followed by N item records, **~1122–1130 bytes each** (length varies with the
  name string). A packet carries 1..~17 items.
- Per item record: 16-char ASCII serial (`"0000…0"`), item **name** (appears
  2×), a `"Trophy: …"` or `"Benefit: …"` descriptor, then stat fields. Item-id
  candidate = first u32 of the record (~56000–69000 range).
- `u32 @0` varies per payload (216/36/34… for single-item; 27/29/21 for
  multi-item) — likely a slot/container context; TBD.

Cross-validated: byte-identical 0x74b0 stream across two independent captures
(same character's inventory) — a strong identity check.

**Open:** the full ~1130-byte item record (id, stats, slots, class restrictions,
benefit/trophy ids, flags) — a large struct, its own decode pass. Expect class
fields to reflect the 3-classes-at-once design. `u32 @0` header semantics.

### 2026-07-05 — spawn opcode (OP_ZoneSpawns/OP_NewSpawn) = `0x7475`

Capture: loc clip (Nektulos). Method: `--dump-payload 0x7475` + string/struct
analysis. Found by scanning candidate S>C opcodes for ASCII names.

- **spawn opcode = `0x7475`** (S>C, variable 92…507 B, n=239). One spawn per
  payload — carries the spawn's **name + id**. NPCs present (`a_skeleton09/07/11`
  → ids 14268/14266/13667; `a_large_piranha05`), 163/239 are `a_`-prefixed
  NPCs; the player's own spawn also appears.

Structure: **null-terminated ASCII name** (variable length) + a fixed block —
**326 B for NPCs**, **470 B for players** (richer: equipment / 3-class etc.).

Decoded NPC block fields (offset within the fixed block):

| off | type | field | evidence |
|-----|------|-------|----------|
| 0   | u32  | **spawn id** | unique per mob; CONFIRMED — the player self-spawn's id here (14239) matches the 0x0b03 self id |
| 4   | u8   | **level** | type-appropriate ranges: bears 3–5, Deathfist legionnaires 7–9, piranhas 10–14, skeletons 4–6, snakes/wolves 1 |
| ~26 | —    | race/model | constant within a creature type, differs across types |
| 40  | u8   | **body type** | undead(skeleton)=3, humanoid(legionnaire)=1, animal=21 |
| 44–45 | u8×2 | **HP% (cur/max)** | 100/100 for all sampled spawns (full health) |
| 227 | i16 | **Z / 8** | height (fixed-point, 1/8 unit) |
| 231 | i16 | **X / 8** | east-west (fixed-point, 1/8 unit) |
| 241 | i16 | **Y** (unscaled) | north-south (integer units) |

**Position CONFIRMED** via two stationary named NPCs `/loc`'d point-blank
(Guard E`tru, Captain N`Farre): decoded X/Y/Z match their `/loc` to <0.5 units
AND reproduce the guard→captain deltas (dX=−2, dY=−36, dZ=−0.75). Validated
across all 163 NPC spawns → coherent zone coords (X [−861,1268], Y [−2180,2158],
Z median −5 = terrain height), 155/163 within zone bounds. **Mixed scaling**
(Y unscaled but X/Z ÷8) is just this build's shuffled layout — re-derive per
patch, don't memorize. `/consider` reaches ~200 u but here the player stood on
top of each NPC (Y matched within 1 u), which is what made the crack clean.

**Ruled out for spawns:** `0x3299` (S>C, 130–350 B, n=62) — numeric only
(repeated ids + 0xff padding), no names/coords.

### 2026-07-05 — OP_MobUpdate = `0x061b`

Capture: loc clip. Method: `--dump-payload` + cross-ref vs 0x7475 spawn
ids/positions + single-mob trajectory.

- **OP_MobUpdate = `0x061b`** (S>C, 14 B fixed, n=649). Per-mob position update.
  585/649 (90%) carry a spawn id matching a known 0x7475 spawn.

Layout (14 B):

| off | type | field |
|-----|------|-------|
| 0   | u32  | spawn id (low 16 bits) |
| 4   | i16  | **Y / 8** |
| 6   | i16  | **Z / 64** |
| 10  | i16  | **X** (unscaled) |
| 8–9, 12–13 | — | heading / delta (TBD) |

CONFIRMED: `a_rotting_citizen01`'s first update decodes to its exact 0x7475
spawn (874, 2158, −8); all 585 updates land in-zone; stationary mobs show
constant position bytes across many updates. **Yet another scale set**
(Y/8, Z/64, X unscaled) — different from 0x7475 (X/8, Y, Z/8) and 0x0b03
(floats). Re-derive per opcode; don't assume a shared convention.

**Ruled out:** `0x4566` (S>C, 12 B, n=200) is a periodic heartbeat/counter —
`[u32 0][u32 incrementing 0,1,2…][u32 const 0x6a4add7e]`, no id or position.

### 2026-07-05 — OP_NewZone = `0x5ab6`, OP_PlayerProfile = `0x5207` (both WIRED)

Capture: fresh Nektulos login. Legends **reuses classic EQ ids** (race/class/zone).

- **OP_NewZone = `0x5ab6`** (S>C, ~343 B). Null-terminated **shortName**
  (`"nektulos"`) + **longName** (`"Nektulos Forest"`) + **zonefile**. The short
  name drives `loadZoneMap` → **the map now loads**. This is the CURRENT zone.
- **OP_PlayerProfile = `0x5207`** (S>C, ~38 KB, once at zone-in). Header
  (`legendsCharProfileHdr`): **race u32@21** (6=DarkElf), **class1 u32@25**
  (5=SK), **level u8@33**. 2nd class (Shaman=10) is u32@**147**. A **bind-point
  array** starts @39 (5× {u32 zoneId=25, float x/y/z}) — that zoneId is the
  BIND zone, **not** current (verified: login pos ≠ bind pos), so the map uses
  OP_NewZone. Char name is deep (~@35551). Embeds inventory (item names/serials).
  Verified: race=6/class=5/level=3 matches ground truth.

**Profile-hunt gotcha:** the char name appears only in a name-stub (`0x4048`)
and the inventory bulk (`0x5207`), never a clean "profile" struct — find the
profile by its **id-cluster header** (race/class/level as u32 at unaligned
offsets), not by grepping the name.

### 2026-07-07 — OP_TargetMouse = `0x1bfe` (Target / UnTarget)

Captures: a combat capture (18 fires) +
a Nektulos `/loc`-clip capture (11 fires — different
spawn set). Method: `--dump-payload 0x1bfe` + **value-match** against the `0x7475`
spawn-id set (no timing needed — pure payload cross-check, so robust to the
`--replay-pcap` capture-time gap).

- **OP_TargetMouse = `0x1bfe`** (C>S, 4 bytes). Player target selection.
  Payload = `{u32 spawn_id}`; **spawn_id = 0 = clear target (untarget)** — Target
  and UnTarget are the *same* opcode.

Confirmed: **18/18** fight payloads + **11/11** locclip payloads resolve to either
0 (untarget) or a live `0x7475` spawn id, zero misses. Fight sample hit 6 distinct
named mobs (`a_fire_beetle10`, `a_skeleton05`, `a_decaying_skeleton01`,
`a_moss_snake03`, …); locclip (different zone) hit `Kirak_Vil00` + `a_spiderling02`.
No competing spawn-id-carrying C>S opcode exists (the other small C>S opcodes are a
toggle / constants — see candidates), so the ID is unambiguous.

**WIRED** (`wire_eql.cpp`): the Legends payload is byte-identical to Live's
`clientTargetStruct`, so `0x1bfe` wires straight to the existing neutral
`SpawnShell::clientTarget` — **no Legends-specific code, no new decoder, proto, or
web**. The whole chain was already backend-neutral: `clientTarget` → `emit
targetSpawn` → `SessionAdapter::onTargetSpawn` → `Targeted` envelope → web
(`App.tsx`, `spawn_id=0` = untarget). Verified via golden replay: 36 `Targeted`
envelopes, real ids = the 6 confirmed named-mob targets + untargets.

## Candidates (unconfirmed)

- `0x71fc` (C>S, variable 18…2483 B, n=553) — char-create / inventory upload?
- `0x0dba` (S>C, ~1.1 KB) — large periodic (another item packet? — compare vs 0x74b0)
- **`0x0d9c` + `0x2d07`** (both S>C, 8 B, n≈63/54) — per-mob `{u32 spawn_id, u32 flag}`
  state broadcasts; every id is a live `0x7475` spawn. flag is **not HP** (constant 4,
  occasionally 32/64, doesn't drain during a fight) — looks like an aggro/stance
  state (a given mob flips 4↔32 over time). Two near-identical opcodes carrying the
  same shape — possibly enter-state vs leave-state.
- **`0x6704`** (C>S, 4 B, n=14) — value toggles `2/0/2/0`: auto-attack on/off (carries
  no spawn id, so not a target).
- **`0x26db`** (C>S+S>C, 21–24 B, n=34) — item opcode: `{u32 item_id, u32, u32 namelen,
  ASCII name+'*'}` (`"Short Sword"` id 9998, `"Bandages"` id 21779). Inventory/item, not combat.
- **`0x42fe`** (C>S, 8 B, n=19) — constant `{225, 13}` every fire; periodic client
  keepalive/heartbeat, not a targeted request.
- **`0x2824`** (S>C, ~18 B, **n=2952**) + **`0x7f8a`** (S>C, variable, n=767) — the two
  highest-volume unknowns in a fight; combat/animation per-tick stream? unhunted.

**Con (`/consider`) — NOT present in the fight captures (2026-07-07).** Ruled out by
exhaustion: the only C>S opcode carrying a *varying* target spawn id is `0x1bfe`
(target); there is no second targeted C>S request, and no S>C reply carrying
faction/level/HP for a con color. The player targeted + fought but never `/con`'d.
**To crack Con, capture a dedicated `/consider` session** — con several *named* mobs
of *known, varied* level/faction, ideally without attacking so the reply isn't buried
in combat spam. Expect the reply to carry `{target spawn_id, level, cur_hp%, faction}`;
cross-check level vs the mob's `0x7475` block +4 and HP% vs block +44/45.
