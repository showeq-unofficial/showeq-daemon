#!/usr/bin/env python3
"""Fast, in-process inspector for daemon-recorded .pbstream goldens.

The on-disk format is a **4-byte little-endian length prefix** followed by the
serialized seq.v1.Envelope payload, repeating to EOF. See src/filesink.cpp.

Unlike the old implementation this does NOT shell out to `protoc --decode` once
per envelope (unusably slow on multi-million-envelope goldens). It parses the
length-delimited stream in-process with a self-contained protobuf wire decoder
driven by a schema parsed from proto/seq/v1/*.proto, so a 359MB golden tallies
in a few seconds instead of spawning millions of subprocesses.

Usage:
  scripts/decode_pbstream.py <path>              # fast tally of payload kinds
  scripts/decode_pbstream.py <path> --kind X     # concise per-envelope summary
                                                   of kind X (--full for text)
  scripts/decode_pbstream.py <path> --kind X --full   # full protoc-like text
  scripts/decode_pbstream.py <path> --grep S     # envelopes whose text contains S
  scripts/decode_pbstream.py <path> --buffs      # own-buff spell-id sets summary
  scripts/decode_pbstream.py <path> --spawns     # distinct spawns from spawn_added
  scripts/decode_pbstream.py <path> --spawn-id N # envelopes referencing spawn id N
  scripts/decode_pbstream.py <path> --limit N    # cap dumps / rows (0 = unlimited)

Behavioral summaries (--buffs / --spawns) are the fast path a developer reaches
for to answer "did feature X produce the right events" without temporary seqInfo
logging in the C++ daemon.

Decoding engine: a zero-dependency pure-Python protobuf decoder (always used for
the tally, summaries, grep and concise output). When the `google.protobuf`
runtime happens to be importable, generated bindings are bootstrapped into
scripts/.pbgen/ (gitignored) and used to render canonical protoc text for
--full; otherwise --full uses the built-in renderer. Pass --no-codegen to force
the built-in renderer.
"""

import argparse
import struct
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
PROTO_INC = REPO / "proto"
PROTO_DIR = REPO / "proto" / "seq" / "v1"
CACHE_DIR = REPO / "scripts" / ".pbgen"

SCALAR_TYPES = {
    "double", "float", "int32", "int64", "uint32", "uint64", "sint32",
    "sint64", "fixed32", "fixed64", "sfixed32", "sfixed64", "bool",
    "string", "bytes",
}
VARINT_SCALARS = {"int32", "int64", "uint32", "uint64", "sint32", "sint64", "bool"}
FIXED32_SCALARS = {"fixed32", "sfixed32", "float"}
FIXED64_SCALARS = {"fixed64", "sfixed64", "double"}


# ---------------------------------------------------------------------------
# Schema (parsed from the .proto text — no protobuf runtime needed)
# ---------------------------------------------------------------------------

class Field:
    __slots__ = ("name", "ptype", "number", "repeated", "packed")

    def __init__(self, name, ptype, number, repeated, packed):
        self.name = name
        self.ptype = ptype
        self.number = number
        self.repeated = repeated
        self.packed = packed


class Schema:
    def __init__(self):
        self.messages = {}       # name -> [Field] in declaration order
        self.fields_by_num = {}  # name -> {number: Field}
        self.enums = {}          # name -> {number: value_name}

    def finalize(self):
        for name, fields in self.messages.items():
            self.fields_by_num[name] = {f.number: f for f in fields}


def _tokenize(text):
    # Strip block then line comments, then split into a flat token stream with
    # structural punctuation as standalone tokens.
    out = []
    i, n = 0, len(text)
    while i < n:
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        else:
            out.append(text[i])
            i += 1
    cleaned = "".join(out)
    for sym in "{}[];=,<>":
        cleaned = cleaned.replace(sym, f" {sym} ")
    return cleaned.split()


def parse_proto(paths):
    schema = Schema()
    tokens = []
    for p in paths:
        tokens.extend(_tokenize(p.read_text()))

    pos = 0
    n = len(tokens)

    def skip_to_semicolon(i):
        while i < n and tokens[i] != ";":
            i += 1
        return i + 1

    def parse_enum(i):
        # tokens[i] is the enum name
        name = tokens[i]
        i += 1
        assert tokens[i] == "{"
        i += 1
        values = {}
        while i < n and tokens[i] != "}":
            t = tokens[i]
            if t in ("option", "reserved"):
                i = skip_to_semicolon(i + 1)
                continue
            # value_name = number ;
            vname = t
            i += 1
            assert tokens[i] == "="
            i += 1
            vnum = int(tokens[i], 0)
            i = skip_to_semicolon(i + 1)
            values[vnum] = vname
        schema.enums[name] = values
        return i + 1  # past '}'

    def parse_message(i, qualifier=""):
        name = tokens[i]
        full = qualifier + name
        i += 1
        assert tokens[i] == "{", f"expected {{ after message {full}"
        i += 1
        fields = []

        def parse_field_list(i, stop):
            while i < n and tokens[i] != stop:
                t = tokens[i]
                if t == "reserved" or t == "option":
                    i = skip_to_semicolon(i + 1)
                    continue
                if t == "message":
                    i = parse_message(i + 1, full + ".")
                    continue
                if t == "enum":
                    i = parse_enum(i + 1)
                    continue
                if t == "oneof":
                    # oneof NAME { <fields> } — members belong to this message.
                    i += 2  # skip 'oneof' and its name -> now at '{'
                    assert tokens[i] == "{"
                    i = parse_field_list(i + 1, "}")
                    i += 1  # past '}'
                    continue
                if t == "map":
                    # map < k , v > name = N ; (not used in these protos)
                    i = skip_to_semicolon(i + 1)
                    continue
                # regular field: [repeated|optional|required] Type name = N [opts];
                repeated = False
                if t in ("repeated", "optional", "required"):
                    repeated = (t == "repeated")
                    i += 1
                ptype = tokens[i]
                i += 1
                fname = tokens[i]
                i += 1
                assert tokens[i] == "=", f"expected = in field {full}.{fname}"
                i += 1
                fnum = int(tokens[i], 0)
                i += 1
                packed = False
                if i < n and tokens[i] == "[":
                    depth = 0
                    while i < n:
                        if tokens[i] == "[":
                            depth += 1
                        elif tokens[i] == "]":
                            depth -= 1
                            if depth == 0:
                                i += 1
                                break
                        elif tokens[i] == "packed":
                            packed = True
                        i += 1
                assert tokens[i] == ";"
                i += 1
                fields.append(Field(fname, ptype, fnum, repeated, packed))
            return i

        i = parse_field_list(i, "}")
        schema.messages[full] = fields
        return i + 1  # past '}'

    while pos < n:
        t = tokens[pos]
        if t in ("syntax", "package", "import", "option"):
            pos = skip_to_semicolon(pos + 1)
        elif t == "message":
            pos = parse_message(pos + 1)
        elif t == "enum":
            pos = parse_enum(pos + 1)
        else:
            pos += 1

    schema.finalize()
    return schema


# ---------------------------------------------------------------------------
# Wire decode (pure Python)
# ---------------------------------------------------------------------------

def read_varint(data, pos):
    result = 0
    shift = 0
    while True:
        b = data[pos]
        pos += 1
        result |= (b & 0x7F) << shift
        if not (b & 0x80):
            return result, pos
        shift += 7


def _zigzag(n):
    return (n >> 1) ^ -(n & 1)


def _varint_value(ptype, val):
    if ptype == "bool":
        return bool(val)
    if ptype in ("sint32", "sint64"):
        return _zigzag(val)
    if ptype == "int32":
        v = val & 0xFFFFFFFF
        return v - 0x100000000 if v >= 0x80000000 else v
    if ptype == "int64":
        return val - 0x10000000000000000 if val >= 0x8000000000000000 else val
    return val  # uint32/uint64/enum


def _fixed32_value(ptype, raw):
    if ptype == "float":
        return struct.unpack("<f", raw)[0]
    if ptype == "sfixed32":
        return struct.unpack("<i", raw)[0]
    return struct.unpack("<I", raw)[0]


def _fixed64_value(ptype, raw):
    if ptype == "double":
        return struct.unpack("<d", raw)[0]
    if ptype == "sfixed64":
        return struct.unpack("<q", raw)[0]
    return struct.unpack("<Q", raw)[0]


def _decode_packed(data, start, end, ptype):
    out = []
    pos = start
    if ptype in FIXED32_SCALARS:
        while pos < end:
            out.append(_fixed32_value(ptype, data[pos:pos + 4]))
            pos += 4
    elif ptype in FIXED64_SCALARS:
        while pos < end:
            out.append(_fixed64_value(ptype, data[pos:pos + 8]))
            pos += 8
    else:
        while pos < end:
            v, pos = read_varint(data, pos)
            out.append(_varint_value(ptype, v))
    return out


def decode_message(data, start, end, msg_type, schema):
    """Generic recursive decode -> dict keyed by field name (repeated -> list).

    Unknown fields are stored under "#<num>" so nothing is silently dropped.
    """
    fbn = schema.fields_by_num[msg_type]
    out = {}
    pos = start
    while pos < end:
        tag, pos = read_varint(data, pos)
        fnum = tag >> 3
        wt = tag & 7
        fd = fbn.get(fnum)
        if wt == 0:
            val, pos = read_varint(data, pos)
            pyval = _varint_value(fd.ptype, val) if fd else val
        elif wt == 5:
            raw = data[pos:pos + 4]
            pos += 4
            pyval = _fixed32_value(fd.ptype, raw) if fd else struct.unpack("<I", raw)[0]
        elif wt == 1:
            raw = data[pos:pos + 8]
            pos += 8
            pyval = _fixed64_value(fd.ptype, raw) if fd else struct.unpack("<Q", raw)[0]
        elif wt == 2:
            ln, pos = read_varint(data, pos)
            seg_s, seg_e = pos, pos + ln
            pos = seg_e
            if fd is None:
                pyval = data[seg_s:seg_e]
            elif fd.ptype in schema.messages:
                pyval = decode_message(data, seg_s, seg_e, fd.ptype, schema)
            elif fd.ptype == "string":
                pyval = data[seg_s:seg_e].decode("utf-8", "replace")
            elif fd.ptype == "bytes":
                pyval = bytes(data[seg_s:seg_e])
            elif fd.repeated and fd.ptype not in ("string", "bytes"):
                # packed repeated scalar
                out.setdefault(fd.name, []).extend(
                    _decode_packed(data, seg_s, seg_e, fd.ptype))
                continue
            else:
                pyval = bytes(data[seg_s:seg_e])
        else:
            break  # groups / unknown wiretype — stop
        key = fd.name if fd else f"#{fnum}"
        if fd and fd.repeated:
            out.setdefault(key, []).append(pyval)
        else:
            out[key] = pyval
    return out


# ---------------------------------------------------------------------------
# Record iteration + fast kind scan
# ---------------------------------------------------------------------------

def iter_records(data):
    """Yield (index, start, end) for each length-delimited envelope."""
    i = 0
    n = len(data)
    idx = 0
    while i + 4 <= n:
        size = data[i] | (data[i + 1] << 8) | (data[i + 2] << 16) | (data[i + 3] << 24)
        i += 4
        if i + size > n:
            break  # truncated tail (e.g. recording killed mid-write) — drop it
        yield idx, i, i + size
        i += size
        idx += 1


def envelope_kind_num(data, start, end):
    """Return the oneof payload field number (schema-derived, see PAYLOAD_NUMS),
    or None. Cheap: only scans top-level tags until the payload field is reached."""
    pos = start
    while pos < end:
        tag, pos = read_varint(data, pos)
        fnum = tag >> 3
        wt = tag & 7
        if fnum in PAYLOAD_NUMS:
            return fnum
        if wt == 0:
            _, pos = read_varint(data, pos)
        elif wt == 2:
            ln, pos = read_varint(data, pos)
            pos += ln
        elif wt == 5:
            pos += 4
        elif wt == 1:
            pos += 8
        else:
            return None
    return None


# ---------------------------------------------------------------------------
# Text rendering (protoc-like)
# ---------------------------------------------------------------------------

def _escape(s):
    out = []
    for ch in s:
        o = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\t":
            out.append("\\t")
        elif ch == "\r":
            out.append("\\r")
        elif 32 <= o < 127:
            out.append(ch)
        else:
            out.append("\\%03o" % o)
    return "".join(out)


def render_lines(schema, msg_type, d, indent=0):
    pad = "  " * indent
    lines = []
    for fd in schema.messages[msg_type]:
        if fd.name not in d:
            continue
        v = d[fd.name]
        items = v if fd.repeated else [v]
        for it in items:
            if fd.ptype in schema.messages:
                lines.append(f"{pad}{fd.name} {{")
                lines += render_lines(schema, fd.ptype, it, indent + 1)
                lines.append(f"{pad}}}")
            elif fd.ptype in schema.enums:
                lines.append(f"{pad}{fd.name}: {schema.enums[fd.ptype].get(it, it)}")
            elif fd.ptype == "string":
                lines.append(f'{pad}{fd.name}: "{_escape(it)}"')
            elif fd.ptype == "bool":
                lines.append(f"{pad}{fd.name}: {'true' if it else 'false'}")
            elif fd.ptype == "bytes":
                lines.append(f'{pad}{fd.name}: "{_escape(it.decode("latin1"))}"')
            else:
                lines.append(f"{pad}{fd.name}: {it}")
    for k, v in d.items():
        if k.startswith("#"):
            items = v if isinstance(v, list) else [v]
            for it in items:
                lines.append(f"{pad}{k}: {it!r}")
    return lines


def render_envelope_text(schema, data, start, end, pb_mod):
    if pb_mod is not None:
        try:
            env = pb_mod.Envelope.FromString(bytes(data[start:end]))
            from google.protobuf import text_format
            return text_format.MessageToString(env)
        except Exception:
            pass
    d = decode_message(data, start, end, "Envelope", schema)
    return "\n".join(render_lines(schema, "Envelope", d))


# ---------------------------------------------------------------------------
# Optional generated-bindings bootstrap (canonical --full text only)
# ---------------------------------------------------------------------------

def maybe_load_pb(proto_srcs):
    try:
        import google.protobuf  # noqa: F401
    except Exception:
        return None
    marker = CACHE_DIR / "seq" / "v1" / "events_pb2.py"
    newest = max(p.stat().st_mtime for p in proto_srcs)
    if not marker.exists() or marker.stat().st_mtime < newest:
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        cmd = ["protoc", "--python_out", str(CACHE_DIR), "-I", str(PROTO_INC)]
        cmd += [str(p) for p in proto_srcs]
        r = subprocess.run(cmd, capture_output=True)
        if r.returncode != 0:
            return None
    try:
        import importlib
        if str(CACHE_DIR) not in sys.path:
            sys.path.insert(0, str(CACHE_DIR))
        return importlib.import_module("seq.v1.events_pb2")
    except Exception:
        return None


# ---------------------------------------------------------------------------
# Summaries
# ---------------------------------------------------------------------------

def cmd_tally(data, schema, num_to_name):
    counts = {}
    first_rec = {}
    total = 0
    for idx, s, e in iter_records(data):
        total += 1
        fnum = envelope_kind_num(data, s, e)
        k = num_to_name.get(fnum, "<unknown>") if fnum else "<none>"
        counts[k] = counts.get(k, 0) + 1
        if k not in first_rec:
            first_rec[k] = (idx, s, e)

    print(f"# {total} envelopes from {DATA_PATH}")
    for k in sorted(counts, key=lambda x: -counts[x]):
        print(f"  {counts[k]:8d}  {k}")

    print("\n# first sample per kind (head 12 lines)")
    for k in sorted(counts, key=lambda x: -counts[x]):
        if k not in first_rec:
            continue
        idx, s, e = first_rec[k]
        d = decode_message(data, s, e, "Envelope", schema)
        head = "\n".join(render_lines(schema, "Envelope", d)[:12])
        print(f"\n--- {k} (envelope {idx}) ---")
        print(head)


def cmd_kind(data, schema, num_to_name, name_to_num, args):
    target = args.kind
    if target not in name_to_num:
        sys.exit(f"unknown kind '{target}'. known: {', '.join(sorted(name_to_num))}")
    want = name_to_num[target]
    limit = args.limit
    shown = 0
    matched = 0
    for idx, s, e in iter_records(data):
        if envelope_kind_num(data, s, e) != want:
            continue
        matched += 1
        if limit and shown >= limit:
            continue
        shown += 1
        if args.full:
            print(f"--- envelope {idx} ---")
            print(render_envelope_text(schema, data, s, e, PB_MOD))
        else:
            d = decode_message(data, s, e, "Envelope", schema)
            print(f"[{idx}] {summarize(schema, target, d.get(target, {}))}")
    print(f"# {matched} '{target}' envelopes"
          + (f" ({shown} shown; --limit 0 for all)" if limit and matched > shown else ""))


def cmd_grep(data, schema, args):
    # Rendering every envelope is O(file); with --limit we STOP at the first N
    # matches instead of scanning the whole stream (best on a huge golden).
    needle = args.grep
    limit = args.limit
    shown = 0
    scanned = 0
    for idx, s, e in iter_records(data):
        if limit and shown >= limit:
            break
        scanned += 1
        text = render_envelope_text(schema, data, s, e, PB_MOD)
        if needle not in text:
            continue
        shown += 1
        print(f"--- envelope {idx} ---")
        print(text)
    if limit and shown >= limit:
        print(f"# stopped at --limit {limit} ({shown} shown, {scanned} scanned) "
              f"— use --limit 0 to scan the whole stream")
    else:
        print(f"# {shown} envelopes contain {needle!r} (full scan)")


def _payload_bytes(data, start, end, want_num):
    """Return the raw bytes of the envelope's oneof payload sub-message."""
    pos = start
    while pos < end:
        tag, pos = read_varint(data, pos)
        fnum = tag >> 3
        wt = tag & 7
        if fnum == want_num and wt == 2:
            ln, pos = read_varint(data, pos)
            return data[pos:pos + ln]
        if wt == 0:
            _, pos = read_varint(data, pos)
        elif wt == 2:
            ln, pos = read_varint(data, pos)
            pos += ln
        elif wt == 5:
            pos += 4
        elif wt == 1:
            pos += 8
        else:
            break
    return None


def cmd_buffs(data, schema, name_to_num, args):
    """Behavioral summary of BuffsUpdate events, grouped by owner (Buff.target_id).

    A BuffsUpdate carries the buffs on the player AND on nearby PCs/groupmates,
    so we bucket per target_id. The player's own self-id(s) are cross-referenced
    from Snapshot.player_id (EQL re-adopts a fresh self-id each zone), so the
    player's owner buckets are flagged (PLAYER). Envelopes are deduped on their
    raw payload bytes before decode (buffs are resent verbatim on every tick),
    which collapses tens of thousands of identical repeats into a handful.
    """
    want = name_to_num["buffs"]
    snap = name_to_num.get("snapshot")

    # Single pass: collect player self-ids from snapshots + raw-dedup buffs.
    player_ids = set()
    payload_counts = {}
    order = []
    total = 0
    for idx, s, e in iter_records(data):
        fnum = envelope_kind_num(data, s, e)
        if fnum == snap:
            d = decode_message(data, s, e, "Envelope", schema)
            pid = d.get("snapshot", {}).get("player_id", 0)
            if pid:
                player_ids.add(pid)
        elif fnum == want:
            total += 1
            pb = _payload_bytes(data, s, e, want)
            if pb is None:
                continue
            if pb not in payload_counts:
                payload_counts[pb] = 0
                order.append(pb)
            payload_counts[pb] += 1

    # Decode each distinct payload once; aggregate per owner.
    owners = {}   # tid -> {name, updates, sets:{sig:count}, sample:{sig:[buff]}}
    for pb in order:
        cnt = payload_counts[pb]
        upd = decode_message(pb, 0, len(pb), "BuffsUpdate", schema)
        by_tid = {}
        for b in upd.get("buffs", []) or []:
            by_tid.setdefault(b.get("target_id", 0), []).append(b)
        for tid, blist in by_tid.items():
            o = owners.setdefault(tid, {"name": "", "updates": 0, "sets": {}, "sample": {}})
            o["updates"] += cnt
            nm = next((b.get("target_name", "") for b in blist if b.get("target_name")), "")
            if nm:
                o["name"] = nm
            sig = tuple(sorted(b.get("spell_id", 0) for b in blist))
            o["sets"][sig] = o["sets"].get(sig, 0) + cnt
            o["sample"].setdefault(sig, blist)

    print(f"# buffs summary — {total} BuffsUpdate envelopes "
          f"({len(order)} distinct payloads) from {DATA_PATH}")
    print(f"# player self-id(s) from snapshots: "
          + (", ".join(str(p) for p in sorted(player_ids)) or "none"))
    print(f"# {len(owners)} owner(s) (grouped by Buff.target_id):")

    # Players first, then by update volume.
    ordered = sorted(owners.items(),
                     key=lambda kv: (kv[0] not in player_ids, -kv[1]["updates"]))
    for tid, o in ordered:
        flag = "  (PLAYER)" if tid in player_ids else ""
        union = sorted({sid for sig in o["sets"] for sid in sig})
        print(f"\nowner {tid}  {o['name']!r}{flag} — {o['updates']} updates, "
              f"{len(o['sets'])} distinct set(s)")
        print(f"  union spell ids ({len(union)}): "
              + ", ".join(str(x) for x in union))
        sets_sorted = sorted(o["sets"].items(), key=lambda kv: -kv[1])
        if args.limit:
            sets_sorted = sets_sorted[:args.limit]
        for sig, c in sets_sorted:
            print(f"  [x{c}] spells({len(sig)}): " + ", ".join(str(x) for x in sig))
            for b in o["sample"][sig]:
                print(f"      {b.get('spell_id', 0):>6}  dur={b.get('duration_s', 0):>7}"
                      f"  {'good' if b.get('beneficial') else 'detr'}"
                      f"  {b.get('spell_name', '')}")


def cmd_spawns(data, schema, name_to_num, args):
    want = name_to_num["spawn_added"]
    type_names = schema.enums.get("SpawnType", {})
    by_id = {}
    order = []
    for idx, s, e in iter_records(data):
        if envelope_kind_num(data, s, e) != want:
            continue
        d = decode_message(data, s, e, "Envelope", schema)
        sp = d.get("spawn_added", {}).get("spawn", {})
        sid = sp.get("id", 0)
        if sid not in by_id:
            order.append(sid)
        by_id[sid] = (sp.get("name", ""), sp.get("level", 0),
                      type_names.get(sp.get("type", 0), sp.get("type", 0)))
    print(f"# spawns summary — {len(by_id)} distinct spawn ids "
          f"across spawn_added from {DATA_PATH}")
    rows = order if not args.limit else order[:args.limit]
    print(f"  {'id':>8}  {'level':>5}  {'type':<10}  name")
    for sid in rows:
        nm, lvl, ty = by_id[sid]
        print(f"  {sid:>8}  {lvl:>5}  {str(ty):<10}  {nm}")
    if args.limit and len(order) > args.limit:
        print(f"  ... {len(order) - args.limit} more (--limit 0 for all)")


def _envelope_ids(kind, d):
    """Spawn ids referenced by an envelope's payload, keyed by role."""
    p = d.get(kind, {})
    ids = set()
    if kind == "spawn_added":
        ids.add(p.get("spawn", {}).get("id", 0))
    elif kind in ("spawn_updated", "spawn_removed", "considered", "targeted"):
        ids.add(p.get("id", p.get("spawn_id", 0)))
    elif kind == "spawn_killed":
        ids.add(p.get("deceased_id", 0))
        ids.add(p.get("killer_id", 0))
    elif kind == "combat":
        ids.add(p.get("source_id", 0))
        ids.add(p.get("target_id", 0))
    elif kind in ("buffs", "spawn_effects"):
        key = "buffs" if kind == "buffs" else "effects"
        for b in p.get(key, []) or []:
            ids.add(b.get("target_id", 0))
            ids.add(b.get("caster_id", 0))
    elif kind == "snapshot":
        ids.add(p.get("player_id", 0))
        for sp in p.get("spawns", []) or []:
            ids.add(sp.get("id", 0))
    elif kind == "inspect_answer":
        ids.add(p.get("spawn_id", 0))
    ids.discard(0)
    return ids


ID_BEARING_KINDS = {
    "spawn_added", "spawn_updated", "spawn_removed", "spawn_killed", "combat",
    "buffs", "spawn_effects", "snapshot", "inspect_answer", "considered",
    "targeted",
}


def cmd_spawn_id(data, schema, num_to_name, args):
    # Only id-bearing kinds are decoded; with --limit we stop at the first N.
    target = args.spawn_id
    limit = args.limit
    shown = 0
    scanned = 0
    for idx, s, e in iter_records(data):
        if limit and shown >= limit:
            break
        fnum = envelope_kind_num(data, s, e)
        kind = num_to_name.get(fnum) if fnum else None
        if kind not in ID_BEARING_KINDS:
            continue
        scanned += 1
        d = decode_message(data, s, e, "Envelope", schema)
        if target not in _envelope_ids(kind, d):
            continue
        shown += 1
        if args.full:
            print(f"--- envelope {idx} ({kind}) ---")
            print(render_envelope_text(schema, data, s, e, PB_MOD))
        else:
            print(f"[{idx}] {kind}: {summarize(schema, kind, d.get(kind, {}))}")
    if limit and shown >= limit:
        print(f"# stopped at --limit {limit} ({shown} shown, {scanned} decoded) "
              f"— use --limit 0 to scan all")
    else:
        print(f"# {shown} envelopes reference spawn id {target} (full scan)")


def summarize(schema, kind, p):
    """One-line concise summary of a payload dict."""
    if kind == "spawn_added":
        sp = p.get("spawn", {})
        return f"spawn_added id={sp.get('id', 0)} name={sp.get('name', '')!r} lvl={sp.get('level', 0)}"
    if kind == "spawn_updated":
        keys = [k for k in ("pos", "hp_cur", "level", "animation", "name", "filter_flags") if k in p]
        return f"spawn_updated id={p.get('id', 0)} changed={keys}"
    if kind == "spawn_removed":
        return f"spawn_removed id={p.get('id', 0)}"
    if kind == "spawn_killed":
        return f"spawn_killed deceased={p.get('deceased_id', 0)} killer={p.get('killer_id', 0)}"
    if kind == "buffs":
        b = p.get("buffs", []) or []
        return f"buffs n={len(b)} spells={[x.get('spell_id', 0) for x in b]}"
    if kind == "chat":
        return (f"chat ch={p.get('channel', 0)} from={p.get('from', '')!r} "
                f"text={p.get('text', '')!r}")
    if kind == "combat":
        return (f"combat src={p.get('source_id', 0)} tgt={p.get('target_id', 0)} "
                f"dmg={p.get('damage', 0)} spell={p.get('spell_id', 0)}")
    if kind == "player_stats":
        return (f"player_stats lvl={p.get('level', 0)} hp={p.get('hp_cur', 0)}/{p.get('hp_max', 0)} "
                f"mana={p.get('mana_cur', 0)}/{p.get('mana_max', 0)}")
    if kind == "snapshot":
        return (f"snapshot zone={p.get('zone_short', '')!r} player={p.get('player_id', 0)} "
                f"spawns={len(p.get('spawns', []) or [])}")
    if kind in ("considered", "targeted"):
        return f"{kind} spawn_id={p.get('spawn_id', 0)}"
    if kind == "exp":
        return f"exp mob={p.get('mob_name', '')!r} gained={p.get('xp_gained', 0)}"
    # generic: the payload message's top-level scalar fields
    ptype = next((f.ptype for f in schema.messages["Envelope"] if f.name == kind), None)
    parts = []
    if ptype:
        for fd in schema.messages.get(ptype, []):
            if fd.name in p and fd.ptype in SCALAR_TYPES:
                parts.append(f"{fd.name}={p[fd.name]}")
    return f"{kind} " + " ".join(parts[:6])


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

DATA_PATH = None
PB_MOD = None
PAYLOAD_NUMS = set()  # payload field numbers, set from the schema in main()


def main():
    global DATA_PATH, PB_MOD
    ap = argparse.ArgumentParser(description="Fast in-process .pbstream inspector")
    ap.add_argument("path", type=Path)
    ap.add_argument("--kind", help="Only envelopes of this payload kind")
    ap.add_argument("--grep", help="Only envelopes whose decoded text contains this substring")
    ap.add_argument("--buffs", action="store_true", help="Own-buff spell-id sets summary")
    ap.add_argument("--spawns", action="store_true", help="Distinct spawns from spawn_added")
    ap.add_argument("--spawn-id", type=int, help="Envelopes referencing this spawn id")
    ap.add_argument("--full", action="store_true",
                    help="With --kind / --spawn-id: full protoc-like text (default: concise)")
    ap.add_argument("--limit", type=int, default=0,
                    help="Cap on dumps / rows (0 = unlimited)")
    ap.add_argument("--no-codegen", action="store_true",
                    help="Never bootstrap generated bindings; use the built-in renderer")
    args = ap.parse_args()

    if not args.path.exists():
        sys.exit(f"no such file: {args.path}")
    proto_srcs = sorted(PROTO_DIR.glob("*.proto"))
    if not proto_srcs:
        sys.exit(f"no .proto sources under {PROTO_DIR}")

    DATA_PATH = args.path
    schema = parse_proto(proto_srcs)
    if "Envelope" not in schema.messages:
        sys.exit("Envelope message not found in schema")

    # Payload kinds = the message-typed Envelope fields (the `oneof payload`
    # members); scalars like `seq` are not kinds. Derived from the schema so new
    # proto messages work with no edit here.
    global PAYLOAD_NUMS
    env_fields = schema.messages["Envelope"]
    num_to_name = {f.number: f.name for f in env_fields if f.ptype not in SCALAR_TYPES}
    name_to_num = {v: k for k, v in num_to_name.items()}
    PAYLOAD_NUMS = set(num_to_name)

    if not args.no_codegen and (args.full or args.grep):
        PB_MOD = maybe_load_pb(proto_srcs)

    data = args.path.read_bytes()

    if args.buffs:
        cmd_buffs(data, schema, name_to_num, args)
    elif args.spawns:
        cmd_spawns(data, schema, name_to_num, args)
    elif args.spawn_id is not None:
        cmd_spawn_id(data, schema, num_to_name, args)
    elif args.kind:
        cmd_kind(data, schema, num_to_name, name_to_num, args)
    elif args.grep:
        cmd_grep(data, schema, args)
    else:
        cmd_tally(data, schema, num_to_name)


if __name__ == "__main__":
    main()
