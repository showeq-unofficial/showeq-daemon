#!/usr/bin/env python3
"""Record a tier-2 replay fixture: runs setcap then the daemon with
--record-vpk + --opcode-stats + --no-listen, writing to tests/replay/.

Usage:  scripts/capture.py <name>          # e.g. aa_progress, inventory-worn

Ctrl-C to stop the recording — the daemon finalizes the .vpk and the
opcode-stats report on shutdown.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

DAEMON_DIR = Path(__file__).resolve().parent.parent
DAEMON_BIN = DAEMON_DIR / "build" / "showeq-daemon"
BUILD_DIR = DAEMON_DIR / "build"
CONF_DIR = DAEMON_DIR / "conf"
REPLAY_DIR = DAEMON_DIR / "tests" / "replay"
DEVICE = "sniff0"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("name", help="capture name (e.g. aa_progress, inventory-worn)")
    parser.add_argument("--device", default=DEVICE, help=f"capture device (default: {DEVICE})")
    parser.add_argument("--no-stats", action="store_true",
                        help="skip --opcode-stats output")
    parser.add_argument("--force", "-f", action="store_true",
                        help="overwrite existing fixture without prompting")
    args = parser.parse_args()

    if not DAEMON_BIN.is_file():
        print(f"error: daemon not built at {DAEMON_BIN}", file=sys.stderr)
        print(f"run: (cd {DAEMON_DIR} && cmake --build build -j)", file=sys.stderr)
        return 2

    vpk = REPLAY_DIR / f"{args.name}.vpk"
    stats = REPLAY_DIR / f"{args.name}.opcodestats.txt"

    if vpk.exists() and not args.force:
        resp = input(f"{vpk.relative_to(DAEMON_DIR)} exists — overwrite? [y/N] ")
        if resp.strip().lower() not in ("y", "yes"):
            print("aborted")
            return 1

    print(f"=> setcap on {DAEMON_BIN.relative_to(DAEMON_DIR)}")
    setcap = subprocess.run(
        ["cmake", "--build", "build", "--target", "setcap"],
        cwd=DAEMON_DIR,
    )
    if setcap.returncode != 0:
        print("error: setcap target failed", file=sys.stderr)
        return setcap.returncode

    cmd = [
        str(DAEMON_BIN),
        "--device", args.device,
        "--config-dir", str(CONF_DIR),
        "--record-vpk", str(vpk),
        "--no-listen",
    ]
    if not args.no_stats:
        cmd.extend(["--opcode-stats", str(stats)])

    print()
    print(f"=> recording to {vpk.relative_to(DAEMON_DIR)}")
    if not args.no_stats:
        print(f"=> opcode stats to {stats.relative_to(DAEMON_DIR)}")
    print(f"=> device: {args.device}    Ctrl-C to stop")
    print()

    try:
        return subprocess.call(cmd, cwd=DAEMON_DIR)
    except KeyboardInterrupt:
        # daemon handles SIGINT itself and finalizes outputs
        return 0


if __name__ == "__main__":
    sys.exit(main())
