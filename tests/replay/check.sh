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
# The daemon is compiled for exactly one backend (-DSEQ_TARGET). Goldens
# only match the backend that produced them, so fixtures live in per-target
# subdirs (live/ eql/ test/) and we validate just the current build's set.
# SEQ_BUILD_DIR lets a dev point at a sibling build dir (build-test/ etc.).
BUILD_DIR="${SEQ_BUILD_DIR:-${DAEMON_DIR}/build}"
DAEMON="${BUILD_DIR}/showeq-daemon"
CONF_DIR="${DAEMON_DIR}/conf"
FILTER="${1:-}"

if [[ ! -x "${DAEMON}" ]]; then
    echo "error: daemon not built — run 'cmake --build ${BUILD_DIR##*/} -j' in ${DAEMON_DIR}" >&2
    exit 2
fi

# Detect the compiled backend from the build's CMakeCache (same parse as
# build.sh; unset == the default 'live'). Override with SEQ_CHECK_TARGET.
detect_target() {
    local cache="${BUILD_DIR}/CMakeCache.txt" t=""
    [[ -f "${cache}" ]] && t="$(sed -n 's/^SEQ_TARGET:[^=]*=//p' "${cache}")"
    echo "${t:-live}"
}
TARGET="${SEQ_CHECK_TARGET:-$(detect_target)}"
GOLDEN_DIR="${REPLAY_DIR}/${TARGET}"
echo "tier-2 target=${TARGET} (from ${BUILD_DIR##*/}/CMakeCache.txt); fixtures in ${GOLDEN_DIR#${REPLAY_DIR}/}/"

shopt -s nullglob
vpks=("${GOLDEN_DIR}"/*.vpk)
if [[ ${#vpks[@]} -eq 0 ]]; then
    echo "no .vpk fixtures in ${GOLDEN_DIR} — nothing to check for target '${TARGET}' (see README.md)" >&2
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

    golden="${GOLDEN_DIR}/${name}.pbstream"
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
        keep="${GOLDEN_DIR}/${name}.check.pbstream"
        cp "${check}" "${keep}"
        bytes="$(cmp "${golden}" "${check}" 2>&1 || true)"
        echo "FAIL ${name}: ${bytes}"
        echo "     golden:  ${golden}"
        echo "     produced: ${keep}"
        fail=$((fail+1))
        failures+=("${name}")
    fi
done

echo
echo "summary: ${pass} pass, ${fail} fail, ${skip} skip"
if [[ ${fail} -gt 0 ]]; then
    echo "failed fixtures: ${failures[*]}"
    exit 1
fi
