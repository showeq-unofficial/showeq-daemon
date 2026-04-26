# Local replay fixtures

This directory holds `.vpk` packet captures and their paired
`.pbstream` envelope goldens used by the tier-2 regression workflow
(see `../README.md`).

**Captures are NOT committed to git.** A `.vpk` is a recording of the
plaintext EQ session traffic the daemon saw — that includes your
character name, zone, group/guild membership, and any tells / chat
that flew across the wire during recording. The `.pbstream` golden
derived from the `.vpk` carries the same information in a different
encoding. Both files are listed in `.gitignore` so they stay on the
machine that recorded them. Keep your own pair locally and re-run
the replay+cmp check before each daemon-side change you want
confidence on.

If a fixture ever needs to be shared (e.g. to reproduce a bug across
machines), encrypt it out-of-band and **never push it to a public
remote**.

---

## Capturing a fresh fixture

### 1. Record the raw packet stream

```sh
cd showeq-daemon
sudo ./build/showeq-daemon \
    --device eth0 \
    --config-dir conf \
    --record-vpk tests/replay/<scenario>.vpk
```

`<scenario>` should be a short hyphenated description of what the
recording covers, e.g. `walk-and-zone`, `con-and-kill`,
`group-form-and-disband`. Replace `eth0` with whatever interface
carries your EQ traffic.

In-game, do the actions you want represented in the regression
sample — keep it short (30–60 s is plenty for most scenarios). When
done, `Ctrl-C` the daemon. The `.vpk` should be on the order of
hundreds of KB to a few MB.

### 2. Generate the deterministic envelope golden

```sh
./build/showeq-daemon \
    --replay tests/replay/<scenario>.vpk \
    --config-dir conf \
    --record-golden tests/replay/<scenario>.pbstream
```

This runs the daemon in headless playback mode against the recording.
It exits cleanly at EOF and leaves a `<scenario>.pbstream` next to
the `.vpk`. The two files are now your local golden pair.

### 3. Verify a code change hasn't broken decode

After a daemon-side change (or before merging an EQ patch-day update
to `everquest.h` / opcode XMLs), run the check script:

```sh
tests/replay/check.sh                    # all fixtures
tests/replay/check.sh walk-and-zone      # one (substring match)
```

The script iterates every `*.vpk` in this directory, regenerates a
fresh `.pbstream` to a temp dir, and `cmp`s against the matching
committed-locally golden. Output is one line per fixture (`PASS`,
`FAIL`, or `SKIP` if a `.vpk` has no paired `.pbstream`); exit code
is 0 only if every fixture passes, so it's safe to drop into a git
pre-push hook.

On `FAIL`, the script copies the divergent `.pbstream` to
`<scenario>.check.pbstream` next to the golden so you can inspect
the diff after the run. A reasonable starting point:

```sh
diff <(xxd tests/replay/<scenario>.pbstream) \
     <(xxd tests/replay/<scenario>.check.pbstream) | head -40
```

If the change was intended, regenerate the golden (step 2) and
delete the `.check.pbstream` artifact.

---

## Why fixtures stay local

The tier-2 harness was built with public/CI use in mind, but the
fixtures themselves are personal data. CI runs only the tier-1 unit
tests; tier-2 is a developer-local check you run before pushing
changes that touch the decoder, opcode wiring, encoder, or
`SessionAdapter` logic. If you regularly forget, consider a local
git pre-push hook that runs the cmp.
