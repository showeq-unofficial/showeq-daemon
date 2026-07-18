# Character-Registry Refactor — Increment 4 execution plan

Goal: make **Character** the authoritative durable entity that owns exactly one
current **Session** (a `Box` = tuple + streams + zone-bind + ManagerSet),
repointed on zone-in. Retire `Box.merged_into`, `currentBoxFor`, and the
group-aware `evictStale`. Routing MUST stay tuple-keyed (the name binds late, at
OP_EnterWorld/OP_PlayerProfile) — only the *durable-entity choice* inverts.

## Settled decisions (truebox rationale)
- **Q1 — primary/global-streams fork:** retire it on the pin path. A Character
  owns its session's streams uniformly; the primary special-case only ever
  mattered for same-host multibox, which EQ Legends (truebox) can't do.
- **Q2 — attach handoff race:** already solved by the committed `enterWorld`
  reset (clear old spawns + drop self-id at the new session's OP_EnterWorld,
  before the bulk). The new-session attach reuses that seam.

## Already in place (Inc 1-3, shipped to main @ current tip)
- `SessionState { Pending, Attached, Superseded, Reaped }` stamped on each Box.
- `Character { name, id, session, aliasCount }` + `characters()` +
  `currentSessionFor(name)` — VIEWS computed over the box storage.
- `SessionAdapter` follows via `currentSessionFor` on `changed()` (resolve-by-name).
- `sendBoxList` picker reads `characters()`; no longer touches `merged_into`.

## Blast radius (consumers to migrate)
- `boxregistry.{h,cpp}` — storage + `promoteByName`/`observe`/`evictStale` +
  `currentBoxFor`/`currentSessionFor`/`findById`/`lookupByName`.
- `packet.cpp` — routing lookups (`lookupByWorld`/`lookupBoundZone`/
  `lookupByExpectedZone`) stay tuple-keyed; verify they never skip a session.
- `daemonapp.cpp` — `managersForBox` (currentBoxFor), the reload-if-active hook
  (`currentBoxFor(activeBoxId)`), per-box ManagerSet create/teardown.
- `sessionadapter.cpp` — follow (`currentSessionFor`) + picker; `activeBoxId`.
- `namepromoter.cpp` — sets `merged_into` today; will attach a session to a
  Character instead.

## Sub-steps (each builds + `boxregistry`/`namepromoter` unit tests + eql
## check.sh green + live builds; regenerate goldens on intended behavior change)

1. **Authoritative Character store.** Add `std::vector<Character>` (or map keyed
   by name-hash id) owned by BoxRegistry. Maintain it in `promoteByName`
   (upsert the Character, point `session` at the new Box, prior session →
   superseded) and on box removal. Re-point `characters()`/`currentSessionFor()`
   to read from it (O(1)). Keep `merged_into` dual-maintained so routing +
   `currentBoxFor` + `evictStale` are untouched. Verify: identical behavior,
   tests green. (Additive; the safety net is the 19 unit cases.)

2. **Route `currentBoxFor` through the store.** `currentBoxFor(box_id)` →
   resolve the box's Character → return `Character.session`, instead of scanning
   `merged_into` by `first_seen_ms`. Removes the merge-scan. Verify.

3. **Active *character*, not active box.** Replace `m_activeBoxId` /
   `activeBoxChanged` with an active-Character id + `activeCharacterChanged`.
   Update daemonapp + sessionadapter. Keep the SessionAdapter follow semantics.
   Verify (incl. SetActiveBox client path).

4. **Simplify `evictStale`.** Per-Character session lifecycle: reap a
   Character's superseded sessions once idle; reap the Character (its live
   session) only when it's gone idle past TTL. Retire the group-of-boxes walk.
   Verify (unit-test with injected timestamps).

5. **Delete `merged_into`.** `namepromoter` attaches the session to its
   Character via the store; `findById`/`lookupByName` no longer skip merged.
   Remove `merged_into`, `is_merged`, `distinctCount` (dead), and the
   currentBoxFor merge-scan comments. Final verify + regenerate any goldens the
   simplified lifecycle legitimately shifts (esp. the multibox live goldens,
   currently golden-uncovered).

## MODEL-B PIVOT (2026-07-18, supersedes the model-A endgame in steps 4-5)

Decision (user): the refactor keeps a per-box ManagerSet + a web REBIND on every
zone-in (model A, shipped Inc 3). The `--single-session` experiment showed the
better model: the active character's decode context PERSISTS across its zones and
the web never rebinds (model B). Pivot the endgame to model B.

**Target — one persistent ManagerSet PER CHARACTER** (not one global set; that
would merge Live multibox's distinct characters). The web binds to a character's
set ONCE and never rebinds on that character's zone-ins.

**The crux — the name binds late.** A new zone box starts decoding its zone
BEFORE it promotes (name at OP_EnterWorld/profile). So the mechanism is
*attach-on-promote*:
- Box created (anonymous) → decodes into a temp ManagerSet (as today).
- Box promotes → resolve its Character:
  - NEW character → the temp set BECOMES the character's persistent set.
  - EXISTING character (a re-zone) → **ATTACH**: repoint the box's stream
    handlers to the character's EXISTING persistent set, refresh it (the
    `enterWorld` clear + re-adopt = the Q2 handoff), and discard the temp set.
    The web stays bound to the same set → no rebind.

**Retire, once model B lands:** `followActiveCharacter`'s rebind (the set now
persists — no rebind needed), the per-zone new-set churn (where the golden-balloon
+ rare eviction crash lived), and `--single-session` (now the default).

**Increments:**
- **B1 (truebox win, low-risk):** route non-primary boxes into the ACTIVE
  character's persistent set by default (evolve the `--single-session` path);
  keep model A (per-box) as an explicit multibox opt-out. Delivers EQL seamless
  zoning, reuses working code. (For truebox = one character, "active character's
  set" is always the right set, so no attach-on-promote needed yet.)
- **B2 (multibox-correct):** attach-on-promote — a box that promotes to an
  EXISTING character repoints its streams onto that character's set. Retire the
  follow rebind + the per-box set build; delete `--single-session`. Regenerate
  the (currently uncovered) multibox live goldens.

Steps 4 (evictStale simplify) + 5 (delete merged_into) still apply as cleanup and
fold in after B1/B2 (evict is needed in both models).

## DONE (2026-07-18) — B1, B2, steps 4-5 all landed

- **B1** (`1dc2b99`): non-primary boxes route into the ONE persistent
  `m_activeManagers`; model-A per-box sets + `--multibox` removed (EQL truebox).
- **B2** (`ff1a730`): `followActiveCharacter` rebind retired — the set persists,
  so a zone-in needs no re-bind. `m_pinnedCharacter` / `disconnectPerBox` / the
  ctor follow wiring all gone. eql goldens byte-identical (proves it was inert).
- **Steps 4-5** (this commit): `merged_into` / `is_merged` DELETED. A character's
  sessions already share `box_id` (the name hash), so the `m_characters` store
  (keyed by that id) owns identity outright — `merged_into` was redundant.
  `evictStale` collapsed to "protect primary + the active character's live
  session; reap every other idle box." This intentionally reaps a stale
  old-zone anchor session that the old group-walk kept alive purely to anchor
  `merged_into` — the only deliberate behavior change (multibox-only; unit-tested,
  golden-uncovered). `distinctCount` kept (reimplemented over distinct `box_id`)
  — it's used by `dumpString`, not dead as the step-5 sketch assumed.
- Verified: `boxregistry`/`namepromoter` units green, full suite 10/10, eql
  goldens 6/0 stable ×3, smoke replay clean.
- Still open: record the multibox live goldens once Live is re-captured, so the
  reap-stale-anchor behavior change gets golden coverage (currently unit-only).

## Verification gates per sub-step
- `ctest -R "boxregistry|namepromoter"` → 19/19.
- `SEQ_CHECK_TARGET=eql check.sh` → green (chat golden is large but stable;
  skip-list it only if it flaps under load).
- `build-live` compiles; `SEQ_CHECK_TARGET=live SEQ_BUILD_DIR=build-live
  check.sh` — regenerate goldens the intended behavior change shifts, and
  finally record the multibox live goldens (multibox-*.vpk) so the path is
  covered.
- Open item carried in: `mage-wear-clickoff` live golden changed deterministically
  with 0 follow rebinds (one fewer `buffs` envelope) — decide stale-golden vs
  real before regenerating.
