#!/usr/bin/env python3
"""Decode a daemon-recorded .pbstream into seq.v1.Envelope summaries.

The on-disk format is **4-byte little-endian length prefix** followed by the
serialized Envelope payload, repeating to EOF. See src/filesink.cpp.

Usage:
  scripts/decode_pbstream.py <path>            # tally + first few of each kind
  scripts/decode_pbstream.py <path> --kind X   # full text of every X envelope
                                                 (X = snapshot, zone_changed,
                                                  player_stats, spawn_added,
                                                  spawn_updated, ...)
  scripts/decode_pbstream.py <path> --grep S   # full text of envelopes whose
                                                 decoded form contains S

Exits non-zero if protoc isn't on PATH or the proto sources are missing.
"""

import argparse
import struct
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROTO_INC = REPO / "proto"
PROTO_SRC = REPO / "proto" / "seq" / "v1" / "events.proto"

KNOWN_KINDS = (
    "snapshot zone_changed spawn_added spawn_updated spawn_removed spawn_killed "
    "player_stats chat exp group buffs combat categories filter_rules prefs "
    "pref_changed considered targeted spawn_point_added spawn_point_updated "
    "spawn_point_removed spawn_points_cleared devices_list item_learned "
    "item_totals worn_set"
).split()


def read_envelopes(path: Path):
    data = path.read_bytes()
    i = 0
    while i + 4 <= len(data):
        size = struct.unpack_from("<I", data, i)[0]
        i += 4
        yield data[i : i + size]
        i += size


def decode(env_bytes: bytes) -> str:
    r = subprocess.run(
        ["protoc", "--decode=seq.v1.Envelope", "-I", str(PROTO_INC), str(PROTO_SRC)],
        input=env_bytes,
        capture_output=True,
        timeout=5,
    )
    return r.stdout.decode("utf-8", errors="replace")


def kind_of(decoded: str) -> str | None:
    for k in KNOWN_KINDS:
        # decoded oneof appears as 'KIND {' at the top level (after a leading
        # 'seq:' / 'server_ts_ms:' field). Match either embedded or as the
        # first field.
        if f"\n{k} {{" in decoded or decoded.startswith(f"{k} {{"):
            return k
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("path", type=Path)
    ap.add_argument("--kind", help="Print full text of every envelope of this kind")
    ap.add_argument("--grep", help="Print full text of every envelope whose decoded form contains this substring")
    ap.add_argument("--limit", type=int, default=0, help="Cap on full-text dumps (0 = unlimited)")
    args = ap.parse_args()

    if not args.path.exists():
        sys.exit(f"no such file: {args.path}")
    if not PROTO_SRC.exists():
        sys.exit(f"events.proto not found at {PROTO_SRC}")

    counts: dict[str, int] = {}
    samples: dict[str, list[tuple[int, str]]] = {}
    full_dumps: list[tuple[int, str]] = []

    for idx, env_bytes in enumerate(read_envelopes(args.path)):
        decoded = decode(env_bytes)
        k = kind_of(decoded) or "<unknown>"
        counts[k] = counts.get(k, 0) + 1
        if args.kind and k == args.kind:
            full_dumps.append((idx, decoded))
        elif args.grep and args.grep in decoded:
            full_dumps.append((idx, decoded))
        else:
            samples.setdefault(k, []).append((idx, decoded))

    print(f"# {sum(counts.values())} envelopes from {args.path}")
    for k in sorted(counts, key=lambda x: -counts[x]):
        print(f"  {counts[k]:5d}  {k}")

    if full_dumps:
        print()
        cap = args.limit if args.limit else len(full_dumps)
        for idx, decoded in full_dumps[:cap]:
            print(f"--- envelope {idx} ---")
            print(decoded)
        if cap < len(full_dumps):
            print(f"... {len(full_dumps) - cap} more (rerun with --limit 0 for all)")
        return

    # default: a snippet of each kind
    print("\n# first sample per kind (head 12 lines)")
    for k in sorted(counts, key=lambda x: -counts[x]):
        if k not in samples or not samples[k]:
            continue
        idx, decoded = samples[k][0]
        head = "\n".join(decoded.split("\n")[:12])
        print(f"\n--- {k} (envelope {idx}) ---")
        print(head)


if __name__ == "__main__":
    main()
