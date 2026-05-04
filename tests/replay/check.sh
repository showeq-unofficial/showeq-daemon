#!/usr/bin/env bash
# Tier-2 regression check: replay every local .vpk fixture through the
# daemon, regenerate the envelope golden into a temp file, and cmp
# against the committed-locally .pbstream. Exits non-zero if any
# fixture mismatches — suitable for a git pre-push hook.
#
# Fixtures are personal data (see ../README.md), so this script is
# intentionally a developer-local tool, not wired into ctest/CI.
#
# Usage:
#   tests/replay/check.sh                 # check all fixtures
#   tests/replay/check.sh walk-and-zone   # check one (substring match)

set -euo pipefail

REPLAY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAEMON_DIR="$(cd "${REPLAY_DIR}/../.." && pwd)"
DAEMON="${DAEMON_DIR}/build/showeq-daemon"
CONF_DIR="${DAEMON_DIR}/conf"
FILTER="${1:-}"

if [[ ! -x "${DAEMON}" ]]; then
    echo "error: daemon not built — run 'cmake --build build -j' in ${DAEMON_DIR}" >&2
    exit 2
fi

# rustdecode_test only gets built under -DSEQ_USE_RUST=ON; use it as the
# build-mode probe. Absent => skip the cross-decoder second pass.
if [[ -x "${DAEMON_DIR}/build/tests/rustdecode_test" ]]; then
    HAVE_RUST=1
else
    HAVE_RUST=0
    echo "note: daemon built without -DSEQ_USE_RUST=ON; skipping cross-decoder check"
fi

shopt -s nullglob
vpks=("${REPLAY_DIR}"/*.vpk)
if [[ ${#vpks[@]} -eq 0 ]]; then
    echo "no .vpk fixtures in ${REPLAY_DIR} — see README.md to capture one" >&2
    exit 0
fi

# High-random port avoids collision with a daemon you might have running.
PORT=$(( 50000 + RANDOM % 10000 ))
TMPDIR_RUN="$(mktemp -d)"
trap 'rm -rf "${TMPDIR_RUN}"' EXIT

pass=0
fail=0
skip=0
failures=()

for vpk in "${vpks[@]}"; do
    name="$(basename "${vpk}" .vpk)"
    if [[ -n "${FILTER}" && "${name}" != *"${FILTER}"* ]]; then
        continue
    fi

    # Per-fixture skip list. Drop a fixture's basename here when its
    # daemon-side handling is wallclock-dependent and cmp would flap.
    # Currently: SpellShell::timeout fires every 6s independent of the
    # packet stream (decrements buff durations / removes expired ones),
    # so any fixture that exercises buff lifecycles produces a different
    # byte stream per run.
    if [[ "${name}" == "buffs" ]]; then
        echo "SKIP ${name} (wallclock-dependent SpellShell timer; cmp non-deterministic)"
        skip=$((skip+1))
        continue
    fi

    golden="${REPLAY_DIR}/${name}.pbstream"
    if [[ ! -f "${golden}" ]]; then
        echo "SKIP ${name} (no .pbstream — capture a golden via --record-golden first)"
        skip=$((skip+1))
        continue
    fi

    check="${TMPDIR_RUN}/${name}.pbstream"
    log="${TMPDIR_RUN}/${name}.log"
    PORT=$((PORT+1))

    if ! "${DAEMON}" \
            --replay "${vpk}" \
            --config-dir "${CONF_DIR}" \
            --record-golden "${check}" \
            --listen "127.0.0.1:${PORT}" >"${log}" 2>&1; then
        echo "FAIL ${name} (daemon exited non-zero — see ${log})"
        fail=$((fail+1))
        failures+=("${name}")
        continue
    fi

    if cmp --silent "${golden}" "${check}"; then
        echo "PASS ${name}"
        pass=$((pass+1))
    else
        # Save divergence artifacts so the user can inspect them after
        # the script's tmpdir would otherwise be wiped.
        keep="${REPLAY_DIR}/${name}.check.pbstream"
        cp "${check}" "${keep}"
        bytes="$(cmp "${golden}" "${check}" 2>&1 || true)"
        echo "FAIL ${name}: ${bytes}"
        echo "     golden:  ${golden}"
        echo "     produced: ${keep}"
        fail=$((fail+1))
        failures+=("${name}")
        continue
    fi

    if [[ ${HAVE_RUST} -eq 0 ]]; then
        continue
    fi

    # Second pass: rerun with the Stage A Rust decoder routing
    # OP_MobUpdate through seq-bridge. Output must still byte-match
    # the same golden — that's the cross-decoder guarantee. Add more
    # opcode names here as further stages adopt them.
    rust_check="${TMPDIR_RUN}/${name}.rust.pbstream"
    PORT=$((PORT+1))
    if ! "${DAEMON}" \
            --replay "${vpk}" \
            --config-dir "${CONF_DIR}" \
            --record-golden "${rust_check}" \
            --rust-opcodes OP_MobUpdate,OP_DeleteSpawn \
            --listen "127.0.0.1:${PORT}" >>"${log}" 2>&1; then
        echo "FAIL ${name} [Rust] (daemon exited non-zero — see ${log})"
        fail=$((fail+1))
        failures+=("${name}[Rust]")
        continue
    fi
    if cmp --silent "${golden}" "${rust_check}"; then
        echo "PASS ${name} [Rust]"
        pass=$((pass+1))
    else
        keep="${REPLAY_DIR}/${name}.rust.check.pbstream"
        cp "${rust_check}" "${keep}"
        bytes="$(cmp "${golden}" "${rust_check}" 2>&1 || true)"
        echo "FAIL ${name} [Rust]: ${bytes}"
        echo "     golden:   ${golden}"
        echo "     produced: ${keep}"
        fail=$((fail+1))
        failures+=("${name}[Rust]")
    fi
done

echo
echo "summary: ${pass} pass, ${fail} fail, ${skip} skip"
if [[ ${fail} -gt 0 ]]; then
    echo "failed fixtures: ${failures[*]}"
    exit 1
fi
