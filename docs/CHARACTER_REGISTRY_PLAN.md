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
