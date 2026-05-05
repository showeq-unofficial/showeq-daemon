# Test client opcode rediscovery runbook

Procedural runbook for re-discovering EverQuest opcodes after switching to the Test client. Reuses the existing capture/replay/discovery tooling — no daemon changes are needed on this branch.

Companion docs: `docs/patch-day.md` (general patch-day procedure), `OPCODES_LIVE_TODO.md` (Live tracker on `main`), `TEST_OPCODE.md` (Test tracker, this branch only). The `/opcode-hunt` skill (`.claude/skills/opcode-hunt/`) walks the same procedure interactively.

## 0. Branch & build

```sh
git -C /home/rschultz/src/showeq/showeq-daemon checkout test-client
cmake --build /home/rschultz/src/showeq/showeq-daemon/build --target setcap
```

`setcap` re-grants `cap_net_raw` on the rebuilt binary so live capture works without sudo (per the passwordless sudoers rule).

## 1. Capture device

Test client and Live client share `sniff0`. **Run only one client at a time** so each `.vpk` reflects single-source traffic. When the user is on Test for capture: close Live first.

## 2. Per-scenario capture

For each in-game scenario you want to dissect (e.g., `test-zone-entry`, `test-combat-melee`, `test-aa-buy`, `test-bank-deposit`, `test-group-form`):

```sh
cd /home/rschultz/src/showeq/showeq-daemon
scripts/capture.py test-<scenario>
```

`scripts/capture.py` runs `cmake --build build --target setcap` then the daemon with:

```
--device sniff0 --config-dir conf
--record-vpk tests/replay/test-<scenario>.vpk
--opcode-stats tests/replay/test-<scenario>.opcodestats.txt
--no-listen
```

Ctrl-C finalizes both files. (Per memory `feedback_pair_vpk_with_opcode_stats.md`, always pair `.vpk` with `.opcodestats.txt` so you can re-run stats against the same capture after adding `StructHint` rows.)

## 3. Triage `--opcode-stats` output

Open `tests/replay/test-<scenario>.opcodestats.txt`. Two sections matter:

1. **Per-opcode tally** — every observed ID with its count, payload sizes, and direction. Anything not in `conf/zoneopcodes.xml` shows up here as unresolved (`OP_Unknown_<hex>`).
2. **Candidate matching** (from `src/opcodestats.cpp:207-272`) — for each `StructHint` row (known struct + expected size + direction), ranked candidates among the unresolved opcodes.

Strong candidate signal: an unresolved ID firing N times at exactly the expected size in the expected direction, with **zero competing unresolved IDs** at that size+direction. Per memory `feedback_opcode_disambiguation.md`: n=2-3 is enough when there's no competitor; n≥5 otherwise.

## 4. Confirm and update

For each opcode that clears the bar:

1. **(Optional) Verify with a fresh replay**:
   ```sh
   ./build/showeq-daemon --replay tests/replay/test-<scenario>.vpk \
       --config-dir conf --no-listen \
       --dump-payload <hex>:/tmp/<name>
   ```
   Inspect raw bytes against the struct definition in `src/everquest.h`. Field offsets, ASCII names, sentinel values should line up.

2. **(Optional) Time-correlate**:
   ```sh
   ./build/showeq-daemon --replay tests/replay/test-<scenario>.vpk \
       --config-dir conf --no-listen \
       --list-events tests/replay/test-<scenario>.events.txt
   ```
   Useful when an in-game event happened at a known wallclock during the capture (e.g., the moment you cast a spell) — find the unresolved ID firing in that ms window.

3. **Update `conf/zoneopcodes.xml` (or `conf/worldopcodes.xml`)**:
   - Find the `<opcode id="ffff" name="OP_Foo">` line.
   - Change `id="ffff"` → `id="<hex>"`.
   - Add `updated="MM/DD/YY"` attribute back.

4. **Tick `TEST_OPCODE.md`**:
   - `[ ]` → `[x]` for the opcode.
   - Append a dated confirmation log entry under the `## Confirmation log` section (capture name, method, sample bytes, struct fit, ruled-out leads).

5. **Commit (per memory `feedback_split_opcode_struct_commits.md`)**: opcode XML changes go in their own commit, isolated from any struct deltas, so the same commit can later cherry-pick into legacy `showeq/` post merge-back.

   Example commit: `test-client: confirm OP_ExpUpdate = 0x<hex>` — one opcode per commit when practical, batched only when several were confirmed from the same capture.

## 5. Order of attack

Each step unblocks the next:

1. **World stream first** — login → character-select → enter-world. Until world opcodes decode, the daemon never reaches a usable zone session for opcode-stats capture. Start with `OP_SendLoginInfo`, `OP_SendCharInfo`, `OP_EnterWorld`, `OP_ZoneServerInfo`. Note: many world opcodes are payload-less identifiers — confirmation just requires seeing the ID at the expected handshake step, no struct-size match needed.
2. **Zone bootstrap** — on the first successful zone-in: `OP_ZoneEntry`, `OP_PlayerProfile`, `OP_NewZone`, `OP_TimeOfDay`, `OP_SendZonePoints`, `OP_SpawnDoor`, `OP_GroundSpawn`, `OP_ZoneChange`. These fire deterministically on every zone-in.
3. **Movement & spawn lifecycle** — `OP_ClientUpdate`, `OP_MobUpdate`, `OP_NpcMoveUpdate`, `OP_DeleteSpawn`, `OP_RemoveSpawn`, `OP_SpawnAppearance`. Easy to capture (high frequency).
4. **Stats / HP / xp / mana / endurance / buffs** — combat scenario captures these. `OP_HPUpdate`, `OP_MobHealth`, `OP_ManaChange`, `OP_ExpUpdate`, `OP_LevelUpdate`, `OP_SkillUpdate`, `OP_EndUpdate`, `OP_Buff`.
5. **Combat / actions** — `OP_Action`, `OP_Action2`, `OP_Consider`, `OP_TargetMouse`.
6. **Group / guild / inventory / chat / AA** — scenario-by-scenario, in whatever order matches the user's actual play.

## 6. When to extend `StructHint` / add new structs

If `--opcode-stats` candidate matching can't rank an opcode (because the struct it should map to has no `StructHint` row), add a row to `src/opcodestats.cpp`:

```cpp
{"OP_Foo", "fooStruct", sizeof(fooStruct), DIR_Server},
```

Per `CLAUDE.md` rule: any new struct from `everquest.h` also needs `AddStruct(<name>);` in `s_everquest.h`. Without it, `connect2 SZC_Match` silently drops every packet of that opcode with `OP_X (...) doesn't match: sizeof():0` in stderr — and you'll wonder why your newly-confirmed opcode isn't decoding.

## 7. Replay regression check (optional, for fixtures kept)

`tests/replay/check.sh` will fail on this branch against any pre-existing Live `.vpk` fixtures because they decode against the now-`ffff`'d XML and produce empty pbstreams. Two options:

- **Skip Live fixtures on this branch**: add their basenames to the `SKIP_PATTERNS` array in `tests/replay/check.sh`.
- **Replace with Test fixtures**: as you confirm opcodes, generate fresh Test goldens (`--record-golden`) and let those become the new tier-2 baseline.

Either way, the pre-push hook on this branch is expected to flag old Live fixtures until they're skipped or replaced.

## 8. Merge-back when Test → Live

When the Test client patch promotes to Live:

1. Re-capture against Live to confirm all `test-client` IDs survived the promotion (last-minute tweaks happen).
2. Migrate confirmed entries from `TEST_OPCODE.md` into `OPCODES_LIVE_TODO.md` (or just delete `TEST_OPCODE.md` and let the merge bring confirmed XML IDs into the Live tracker).
3. Per-repo merge:
   ```sh
   git -C /home/rschultz/src/showeq/showeq-daemon checkout main
   git merge test-client
   ```
   Resolve XML conflicts in favor of `test-client` IDs. Drop `TEST_OPCODE.md` and this runbook in the merge commit (or keep them as historical artifacts — caller's choice).
4. Mirror confirmed XML deltas into legacy `showeq/` per memory `feedback_split_opcode_struct_commits.md` (Live-era systems mirror; Quarm does not).
