#!/usr/bin/env python3
#
#  ucs_chat_decode.py -- decode EQ Legends UCS (cross-zone/global chat) from a
#  capture, offline. The daemon does NOT decode UCS itself; this is a
#  standalone reference/verification port of the sibling legacy client's
#  EQPacket::decodeUCSPacket + MessageShell::ucsChatMessage (../showeq/src/
#  packet.cpp, ../showeq/src/messageshell.cpp), handy for reading UCS chat
#  out of the daemon's tests/replay/eql/*.vpk fixtures.
#
#  UCS chat rides a separate SOE "chat" session on UDP 9877, obfuscated with
#  SOE's keyless self-synchronizing XOR (plain[i] = cipher[i] ^ cipher[i-4]).
#  This tool extracts the server->client 9877 payloads from a .vpk or .pcap,
#  runs the same XOR + SPAM-anchored parse the client uses, and prints each
#  line under its (first-char-repaired) channel name -- verified byte-for-byte
#  against the legacy binary's output (417/417 lines on the chat fixture).
#
#  Usage:  ucs_chat_decode.py [--stats] [--raw-channel] [--port N] FILE [FILE...]
#
#  This program is free software under the GNU GPL, same terms as ShowEQ.

import argparse
import struct
import sys

UCS_PORT_DEFAULT = 9877

PCAP_MAGIC_LE = 0xA1B2C3D4      # us-resolution, little-endian host
PCAP_MAGIC_BE = 0xD4C3B2A1
PCAP_MAGIC_NS_LE = 0x4D3CB2A1   # ns-resolution variants
PCAP_MAGIC_NS_BE = 0xA1B23C4D


# ---------------------------------------------------------------------------
# Capture readers -> yield raw link-layer frames
# ---------------------------------------------------------------------------
def read_vpk(data):
    """Legacy ShowEQ .vpk (VPacket). Record = packet_struct header + frame.

    packet_struct (x86-64, USEVERSION on): int size; pad; time_t; long version;
    long ms; long sequence; char buffer[]  => 40-byte header, frame is a full
    ethernet frame. `size` (LE int32 at record start) = header + frame length.
    Header size is auto-detected (40 with USEVERSION, else 32).
    """
    hdrsize = None
    for cand in (40, 32):
        o, ok = 0, 0
        while o + cand <= len(data) and ok < 3:
            sz = struct.unpack_from("<i", data, o)[0]
            if sz < cand or o + sz > len(data):
                break
            frame = data[o + cand:o + sz]
            if len(frame) >= 14 and frame[12:14] == b"\x08\x00":
                ok += 1
            o += sz
        if ok >= 3:
            hdrsize = cand
            break
    if hdrsize is None:
        raise ValueError("not a recognizable ethernet/IPv4 .vpk")
    off = 0
    while off + hdrsize <= len(data):
        sz = struct.unpack_from("<i", data, off)[0]
        if sz < hdrsize or off + sz > len(data):
            break
        yield 1, data[off + hdrsize:off + sz]      # linktype 1 = EN10MB
        off += sz


def read_pcap(data):
    """Classic pcap. 24-byte global header + per-record (16-byte hdr + frame)."""
    magic = struct.unpack_from("<I", data, 0)[0]
    if magic in (PCAP_MAGIC_LE, PCAP_MAGIC_NS_LE):
        end = "<"
    elif magic in (PCAP_MAGIC_BE, PCAP_MAGIC_NS_BE):
        end = ">"
    else:
        raise ValueError("not a classic pcap (bad magic)")
    linktype = struct.unpack_from(end + "I", data, 20)[0]
    off = 24
    while off + 16 <= len(data):
        _ts, _tus, caplen, _origlen = struct.unpack_from(end + "IIII", data, off)
        off += 16
        if off + caplen > len(data):
            break
        yield linktype, data[off:off + caplen]
        off += caplen


def read_capture(path):
    data = open(path, "rb").read()
    magic = struct.unpack_from("<I", data, 0)[0] if len(data) >= 4 else 0
    if magic in (PCAP_MAGIC_LE, PCAP_MAGIC_BE, PCAP_MAGIC_NS_LE, PCAP_MAGIC_NS_BE):
        yield from read_pcap(data)
    else:
        yield from read_vpk(data)


# linktype -> bytes to skip to reach the IPv4 header (None = not decodable here)
_L2_SKIP = {1: 14, 101: 0, 12: 0, 113: 16}   # EN10MB, RAW, RAW(bsd), LINUX_SLL


def udp_of(linktype, frame):
    """Return (src_port, dst_port, udp_payload) for an IPv4/UDP frame, else None."""
    skip = _L2_SKIP.get(linktype)
    if skip is None:
        return None
    if linktype == 1 and (len(frame) < 14 or frame[12:14] != b"\x08\x00"):
        return None
    ip = frame[skip:]
    if len(ip) < 20 or (ip[0] >> 4) != 4:
        return None
    ihl = (ip[0] & 0x0F) * 4
    if ip[9] != 17:                            # 17 = UDP
        return None
    udp = ip[ihl:]
    if len(udp) < 8:
        return None
    sport, dport, ulen = struct.unpack_from(">HHH", udp, 0)
    payload = udp[8:ulen] if 8 <= ulen <= len(udp) else udp[8:]
    return sport, dport, payload


# ---------------------------------------------------------------------------
# The decode -- faithful port of the ShowEQ C++
# ---------------------------------------------------------------------------
def xor_decode(buf):
    """decodeUCSPacket: plain[i] = cipher[i] ^ cipher[i-4] for i>=4 (absolute)."""
    if len(buf) < 12:
        return None
    out = bytearray(buf)
    for i in range(4, len(buf)):
        out[i] = buf[i] ^ buf[i - 4]
    return bytes(out)


def _mostly_printable(bs):
    if not bs:
        return False
    p = sum(1 for c in bs if 32 <= c < 127)
    return p * 100 >= len(bs) * 85


def _field_before(data, end):
    start = end
    while start > 0 and data[start - 1] != 0:
        start -= 1
    return start, end


class UcsDecoder:
    """Mirrors MessageShell's UCS state: channel names learned across the session."""

    def __init__(self, resolve=True):
        self.channels = []          # s_ucsChannels
        self.resolve = resolve

    def _learn_name(self, name):
        if len(name) >= 2 and name not in self.channels:
            self.channels.append(name)

    def _run(self, text, i):
        j = i
        n = ""
        while j < len(text) and (text[j].isalnum() or text[j] == "_"):
            n += text[j]
            j += 1
        return n, j

    def learn_channels(self, text):
        # "Channels: 1=General2(400), 2=myraid(1), ..." -> <name> before a '('
        i = 0
        while True:
            i = text.find("=", i)
            if i < 0:
                break
            name, j = self._run(text, i + 1)
            if j < len(text) and text[j] == "(":
                self._learn_name(name)
            i = j if j > i else i + 1
        # "channel <name>" / "joined <name>" notices
        for kw in ("channel ", "joined "):
            p = text.find(kw)
            if p < 0:
                continue
            name, _ = self._run(text, p + len(kw))
            self._learn_name(name)

    def resolve_channel(self, tail):
        if not tail:
            return ""
        if not self.resolve:
            return tail
        for name in self.channels:
            if tail.endswith(name) or (len(name) >= 2 and tail.endswith(name[1:])):
                return name
        g = tail.find("eneral")             # General, General1, General2, ...
        if g >= 0:
            return "General" + tail[g + 6:]
        return tail

    def chat_records(self, data):
        """Yield (channel, sender, message, spam) from a decoded UCS buffer."""
        n = len(data)
        if n < 8:
            return
        self.learn_channels(data.decode("latin1", "replace"))
        scan = 0
        while scan + 5 <= n:
            p = data.find(b"SPAM:", scan)
            if p < 0:
                break
            scan = p + 5
            if p < 2 or data[p - 1] != 0:            # "SPAM:" must follow a NUL
                continue
            msg_start, msg_end = _field_before(data, p - 1)
            if msg_start >= msg_end or msg_start < 2:
                continue
            ss_start, ss_end = _field_before(data, msg_start - 1)
            if ss_start >= ss_end:
                continue
            message = data[msg_start:msg_end]
            server_sender = data[ss_start:ss_end].decode("latin1", "replace")
            dot = server_sender.rfind(".")
            sender = server_sender[dot + 1:] if dot >= 0 else server_sender
            channel = ""
            if ss_start >= 2:
                ch_start, ch_end = _field_before(data, ss_start - 1)
                field = data[ch_start:ch_end].decode("latin1", "replace")
                k = len(field)
                while k > 0 and 32 <= ord(field[k - 1]) < 127:
                    k -= 1
                channel = self.resolve_channel(field[k:])
            if (not _mostly_printable(sender.encode("latin1", "replace"))
                    or not _mostly_printable(message) or len(sender) > 32):
                continue
            q = p + 5
            score, have = 0, False
            while q < n and 48 <= data[q] <= 57:
                score = score * 10 + (data[q] - 48)
                have = True
                q += 1
            spam = have and score > 0
            yield channel, sender, message.decode("latin1", "replace"), spam


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(
        description="Decode EQ Legends UCS cross-zone/global chat from a "
                    ".vpk or .pcap capture (offline).")
    ap.add_argument("files", nargs="+", help=".vpk or .pcap capture file(s)")
    ap.add_argument("--port", type=int, default=UCS_PORT_DEFAULT,
                    help="UCS UDP port (default %d)" % UCS_PORT_DEFAULT)
    ap.add_argument("--stats", action="store_true",
                    help="print only per-file packet/record counts")
    ap.add_argument("--raw-channel", action="store_true",
                    help="show the on-wire channel tail (masked first char) "
                         "instead of repairing it from /list names")
    args = ap.parse_args()

    grand_pkts = grand_recs = 0
    for path in args.files:
        dec = UcsDecoder(resolve=not args.raw_channel)
        try:
            frames = list(read_capture(path))
        except (ValueError, OSError) as e:
            print("%s: %s" % (path, e), file=sys.stderr)
            continue
        pkts = recs = 0
        lines = []
        for linktype, frame in frames:
            u = udp_of(linktype, frame)
            if not u:
                continue
            sport, _dport, payload = u
            if sport != args.port:            # server->client (src = UCS server)
                continue
            pkts += 1
            plain = xor_decode(payload)
            if plain is None:
                continue
            for channel, sender, message, spam in dec.chat_records(plain):
                recs += 1
                tag = "(SPAM) " if spam else ""
                ch = "#%s: " % channel if channel else ""
                lines.append("Chat: %s%s'%s' - %s" % (ch, tag, sender, message))
        grand_pkts += pkts
        grand_recs += recs
        print("%s: %d server->client UCS packets, %d chat records"
              % (path, pkts, recs))
        if not args.stats:
            for ln in lines:
                print(ln)
    if len(args.files) > 1:
        print("TOTAL: %d packets, %d chat records" % (grand_pkts, grand_recs),
              file=sys.stderr)
    return 0 if grand_recs else 1


if __name__ == "__main__":
    sys.exit(main())
