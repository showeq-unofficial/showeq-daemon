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

## Confirmed

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

Named in `conf/eql/opcodes.toml` (name only; **no handler yet** — surfacing the
selected target in the web client would need a target primitive + proto field).

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
