# showeq-daemon

Headless packet-capture and state-tracking daemon for
[ShowEQ](https://github.com/showeq/showeq). Extracted from the legacy
monolithic `showeq-c` Qt application so multiple clients (web, native
Rust/Iced, future tools) can connect to a single capture process over
WebSocket + protobuf.

## What it does

- Captures EverQuest network traffic via libpcap.
- Reassembles the UDP session layer, decodes opcodes, tracks game state
  (spawns, zones, player, group, guild).
- Serves the state to clients on a WebSocket, encoded as `seq.v1` protobuf
  messages.

## What it is not

- Not a GUI. For a UI, run one of the clients (see `showeq-web`, or run the
  legacy `showeq-c` standalone).
- Not a replacement for `showeq-c` yet. Until feature parity is reached,
  `showeq-c` remains the reference implementation and regression oracle.

## Build

Requires: CMake 3.20+, Qt 5.14+ (Core, Network, Xml, WebSockets), libpcap,
Protobuf, zlib, pthreads.

```sh
git submodule update --init --recursive
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

## Run

libpcap requires CAP_NET_RAW or root:

```sh
sudo build/showeq-daemon --device eth0 --listen 127.0.0.1:9090
```

For LAN-reachable mode (trusted LAN only — no auth, no TLS in v1):

```sh
sudo build/showeq-daemon --device eth0 --listen 0.0.0.0:9090
```

## Layout

```
src/              # Daemon sources (extracted from showeq-c + new glue)
  daemonapp.*     # Top-level wiring hub, replaces interface.cpp's role
  wsserver.*      # QWebSocketServer
  sessionadapter.*# Per-client adapter: QObject signals -> protobuf
  protoencoder.*  # Pure translation functions
  ...             # Packet layer + managers, see extraction inventory
proto/            # git submodule -> showeq-proto
conf/             # Opcode XML + preference schema
docs/             # patch-day.md and friends
tests/replay/     # .vpk + .pbstream goldens for regression
cmake/            # CMake helper modules
```

## License

GPL-2.0 (inherited from `showeq-c`). See [LICENSE](LICENSE).

## Patch day

EverQuest patches typically require updates to `src/everquest.h` and
`conf/*opcodes.xml` in *both* this repo and `showeq-c`. See
[docs/patch-day.md](docs/patch-day.md).
