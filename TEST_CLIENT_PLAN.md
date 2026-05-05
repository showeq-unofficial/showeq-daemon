# `test-client` branch — Test-server opcode rediscovery plan

## Context

EverQuest's Test client typically rotates the entire opcode table on major patches. The current `conf/zoneopcodes.xml` and `conf/worldopcodes.xml` reflect Live values, so a daemon built off `main` cannot decode Test-client traffic until the opcode IDs are re-discovered against fresh Test captures.

This is a **temporary divergence** (Test → Live convergence on patch-day), so a branch is the right shape — unlike Quarm, which is a permanent EQMac-codebase fork in its own repo. When Test goes Live, this branch's confirmed IDs become the new Live IDs and merge back to `main`.

The discovery tooling already exists end-to-end (`--record-vpk`, `--opcode-stats` with `StructHint` ranking, `--list-events`, `--dump-payload`, `scripts/capture.py`); this branch needs only a fresh **tracker**, **XML reset**, and **runbook** — no new daemon code.

## Deliverables (all on `test-client` branch only)

1. Branch `test-client` (from `main`).
2. `conf/zoneopcodes.xml` + `conf/worldopcodes.xml` with all active hex IDs reset to `ffff` (names, struct typenames, dirs, sizechecktype, comments preserved). Commented-out historical entries are left untouched.
3. `TEST_OPCODE.md` — work-of-record tracker scoped to **only the 73 opcodes currently confirmed on Live** (53 zone + 20 world). Concrete finite re-discovery target. Branch is "ready for merge-back review" when all 73 are `[x]`.
4. `docs/test-discovery.md` — procedural runbook (capture → opcode-stats → confirm → update XML/tracker).
5. `CLAUDE.md` line flagging that `TEST_OPCODE.md` replaces `OPCODES_LIVE_TODO.md` as work-of-record on this branch.

## Counts (verified pre-reset)

After stripping XML comments (legacy `<!-- OLD ... -->` blocks contain orphan opcode entries that shouldn't be counted):

| File | Active total | Resolved | Reset to `ffff` |
|------|-------------:|---------:|----------------:|
| `conf/zoneopcodes.xml` | 213 | 53 | all 213 |
| `conf/worldopcodes.xml` | 20 | 20 | all 20 |

The 53 + 20 = **73** resolved opcodes are the seed for `TEST_OPCODE.md`. The 160 zone entries that were already `ffff` (unresolved on Live) are not in scope here — they remain a Live-tracker concern on `OPCODES_LIVE_TODO.md`.

## Discovery workflow (summary)

Full procedural detail in `docs/test-discovery.md`. High-level loop:

1. `scripts/capture.py test-<scenario>` — records `.vpk` + `.opcodestats.txt` from a live Test session on `sniff0`.
2. Triage `.opcodestats.txt`: candidate-matching section (from `src/opcodestats.cpp:207-272`) ranks unresolved IDs against the `StructHint` table by size + direction.
3. Confirmation bar: count + zero-competitor over n≥5 (n=2-3 acceptable when no other unknowns at the target size+dir).
4. Optional: `--dump-payload <hex>:/tmp/<name>` to inspect raw bytes; `--list-events` to time-correlate with in-game events.
5. Edit `conf/<zone|world>opcodes.xml`: `id="ffff"` → `id="<hex>"` + restore `updated=`.
6. Tick `TEST_OPCODE.md`, append dated confirmation log entry.
7. Commit (one opcode per commit when practical, per `feedback_split_opcode_struct_commits.md`).

Order of attack: **world stream first** (login is a prerequisite for zone capture) → zone bootstrap (`OP_ZoneEntry`, `OP_PlayerProfile`, `OP_NewZone`, `OP_TimeOfDay`, `OP_SendZonePoints`) → movement & spawn lifecycle → stats/HP/xp/buffs → combat → group/guild/inventory/chat/AA.

## Reactive struct work (NOT done upfront)

If a Test capture reveals that a struct's wire layout diverged from Live (size mismatch in `--opcode-stats`, garbled `--dump-payload` output), the discovery loop pivots:

1. Add the new struct to `src/everquest.h`.
2. Register it in `src/s_everquest.h` via `AddStruct(<name>);` — without this, `connect2 SZC_Match` silently drops every packet of that opcode (`OP_X (...) doesn't match: sizeof():0` in stderr).
3. Add a `StructHint` row in `src/opcodestats.cpp` so `--opcode-stats` candidate ranking can match it.
4. Commit struct deltas separately from opcode-XML deltas (per `feedback_split_opcode_struct_commits.md`).

## Replay regression check on this branch

`tests/replay/check.sh` will fail against pre-existing Live `.vpk` fixtures because they decode against the now-`ffff`'d XML and produce empty pbstreams. Two options:

- **Skip Live fixtures**: add their basenames to `SKIP_PATTERNS` in `tests/replay/check.sh`.
- **Replace with Test fixtures**: as opcodes are confirmed, generate fresh Test goldens (`--record-golden`).

The pre-push hook is expected to flag old Live fixtures until skipped or replaced — accept this until Test fixtures stand in.

## Merge-back when Test → Live

When the Test patch promotes to Live:

1. Re-capture against Live to confirm `test-client` IDs survived the promotion.
2. Migrate `TEST_OPCODE.md` confirmation log entries into `OPCODES_LIVE_TODO.md` (or drop `TEST_OPCODE.md` and let the merge carry the XML IDs).
3. `git -C showeq-daemon checkout main && git merge test-client` — resolve XML conflicts in favor of `test-client` IDs. Drop `TEST_OPCODE.md`, `TEST_CLIENT_PLAN.md`, `docs/test-discovery.md`, and the test-client `CLAUDE.md` line in the merge commit (or keep them as historical artifacts).
4. Mirror confirmed XML deltas into legacy `showeq/` (Live-era systems mirror; Quarm does not).

## What's intentionally NOT in scope

- No new daemon CLI flags or runtime server-flavor switching. Branch-local `conf/` is sufficient.
- No CMake conditionals or build-time variants. Branch is the variant axis.
- No proto schema changes (per `feedback_proto_schema_unified.md`).
- No upfront struct edits in `everquest.h` / `s_everquest.h`. Struct changes are reactive only.
- No legacy `showeq/` mirroring during discovery. Mirroring happens on merge-back to `main`.

## Why a branch (not a separate repo like Quarm)

| | Quarm fork | `test-client` branch |
|---|---|---|
| Codebase | Permanent EQMac divergence | Temporary EQ-Live divergence |
| Wire format | Different protocol family | Same protocol, rotated IDs |
| Convergence | Never | On every Test→Live promotion |
| Upstream relation | `upstream` remote, sibling repo | Branch off `main`, merges back |
| Cost of separation | Justified — permanent | Overhead — branch is enough |

A separate `showeq-daemon-test` repo would force perpetual cherry-picking and double-bookkeeping for what is fundamentally a transient state of `main`.
