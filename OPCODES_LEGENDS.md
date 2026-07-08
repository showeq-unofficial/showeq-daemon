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
Re-mapped from `tests/replay/eqlegends-login-zone-20260707.vpk` (zone = **Nektulos
Forest**; server/world name is "rivervale"; `.vpk`
so `--list-events` has real capture-time) with in-game ground truth: player `/loc`
`2246.50,-954.77,-4.97`; con targets Dragoon_J`len(25225,L50,amiably),
Sergeant_C`Orm(11626,L50,amiably), Vol_T`Vke(12220,**L60,warmly**); NPC locs
C`Orm `2324.94,-990.11,-4.92` + Vol `2337.45,-802.35,-5.86`.

| Opcode | pre-patch | **new id** | status |
|--------|-----------|-----------|--------|
| OP_TargetMouse   | 0x1bfe | **0x2867** | ✅ confirmed 19/19 (value-match; 0=untarget). Layout unchanged (`{u32 spawn_id}`) |
| OP_Consider      | *(new)* | **0x4212** | ✅ NEW. 24B `{u32 self=27090, u32 target, u32 faction, u32 =7}`; C>S req has faction=0, S>C reply fills faction (**2=warmly, 4=amiably**; level comes from spawn). Companions: `0x5b5e` target-HP reveal `{u32 target, u32 cur_hp,…}`, `0x0e54` `{0,target}` |
| OP_ClientUpdate  | 0x0b03 | **0x7171** | ✅ 42B C>S self (+ 28B S>C variant). **FULLY DECODED** (2026-07-08): spawnId u16@2; pos x@10/y@18/z@30 (f32); velocity deltaX f32@6 / deltaY f32@26; **heading u16@14, 11-bit (0–2047, North≈0)** — Sense-Heading-confirmed. deltaZ candidate @22 |
| OP_ZoneSpawns    | 0x7475 | **0x4606** | ✅ id (var 343–352B NPC / 486B rich). level@block+4 OK. **POS CHANGED**: 330B NPC block `X=i16@239/8, Y=@243/8, Z=@235/8`; 486B block `@395/@399/@391`. hp/body offsets TBD |
| OP_MobUpdate     | 0x061b | **0x67e0** | ✅ id (14B, ids 416/416 match spawns). pos offsets TBD |
| OP_ItemPacket    | 0x74b0 | **0x6805** | ✅ id (bulk items; names + `Benefit:`/`Trophy:`) |
| OP_ZoneServerInfo| —      | **0x35d4** | ✅ world 130B, zone-server host `…everquestlegends.com` |
| OP_ChatServer    | —      | **0x4de7** | world 67B, chat connect `host,9877,rivervale.<char>,token` |
| OP_PlayerProfile | 0x5207 | **0x62f0** | ✅ id (~40KB, embeds char name + inventory). name-stub = `0x46df` (656B). race/class/level offsets TBD |
| OP_NewZone       | 0x5ab6 | **numeric id (opcode TBD)** | swept all 177 opcodes: **no zone-name text on the wire** — post-patch the zone is a numeric id (old EQL reused classic ids, `nektulos=25`). `0x1e94` (world 1932B) carries the *server* name "rivervale", NOT the zone. Find the S>C op carrying zoneid 25 at zone-in (or profile's current-zone field); map id→shortname client-side |

**RE-WIRED 2026-07-07** (decoder-rs + daemon): all ids updated in `conf/eql/opcodes.toml`;
the `seq-backend-eql` parsers re-derived — ClientUpdate pos `x@10/y@18/z@30` (f32);
ZoneSpawns pos anchored to the block END (`z@(len-95)/8, x@(len-91)/8, y@(len-87)/8`,
handles both 330B/486B blocks), id@0/level@4/hp@44,45; MobUpdate `x@4/8, y@10, z@6/64`;
new `parse_legends_consider` (24B) → shared `Consider` → the neutral
`SpawnShell::consMessage` (passed the real payload len, not `sizeof(considerStruct)`) →
`spawnConsidered`→`Considered`→web. Verified on the capture: opcodes resolve by name,
positions `/loc`-correct, con fires for all 3 con'd mobs (Considered ids 12220/11626/25225),
target 38 envelopes; **live 9/9 unregressed**.

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
- **`net opcode 0000` framing drops eat `0x2735` messages** (they fire heavily during
  combat) — only 2 Sense Headings (both West) survived this capture out of a "bunch".
  So the earlier "is net-0000 eating real data?" question = **yes, it drops messages**.
- This channel is the foundation for wiring EQL formatted / combat / system message
  **text** into the daemon+web (via eqstr/dbstr) — not yet done.

**ClientUpdate heading/deltas — DONE (2026-07-08).** heading = `u16@14`, 11-bit
(0–2047 = full circle, North≈0), velocity deltaX `f32@6` / deltaY `f32@26`. Confirmed by
a Sense Heading capture (`captures/eqlegends-heading-20260708.pcap`, Dagnor's Cauldron):
turning through N/NE/E/SE/S/SW/W/NW, `u16@14` stepped 2043/1814/1542/1246/1036/782/492/203
(~256 = 45° apart, value falls as compass rises). Note: the Sense Heading *text* is
**client-side** (not on the wire — 0 in the capture), so the in-game log is the direction
ground truth; the packet field is what carries facing. deltas re-derived from running
segments (correlation of f32@6/@26 with Δx/Δy). **This was the last EQL decode TODO.**

## Confirmed (PRE-PATCH — ids dead as of 2026-07-07, kept for method/evidence)

### 2026-07-05 — OP_ClientUpdate = `0x0b03`

Capture: `captures/eqlegends-charcreate-login-20260702-112219.pcap`.
Method: `--dump-payload 0x0b03:…` (1160 fires) + differential decode.

- **OP_ClientUpdate = `0x0b03`** (C>S, 42 bytes, n=1160). Client self-position
  report. Sole 42-byte C>S opcode in the capture — zero competing unknowns at
  that size+direction.

**Confirmed 42-byte layout (LE)** — axes pinned by a `/loc` ground-truth clip
(`captures/eqlegends-locclip-20260705-153823.pcap`): three `/loc` readings
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

Captures: `captures/eqlegends-fight-20260705-180923.pcap` (18 fires) +
`captures/eqlegends-locclip-20260705-153823.pcap` (11 fires, Nektulos — different
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
