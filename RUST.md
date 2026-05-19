# Rust decoder

The daemon's wire-format decoder is Rust, supplied by the sibling
`showeq-decoder-rs` checkout via `seq-bridge` (cxx). Rust is a hard
build dependency; there is no C++ fallback.

## Toolchain + checkout

```sh
# One-time toolchain install:
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
. "$HOME/.cargo/env"

# Sibling-checkout layout that CMake expects:
#   parent-dir/
#     showeq-daemon/
#     showeq-decoder-rs/
git -C .. clone git@github.com:showeq-unofficial/showeq-decoder-rs.git

cmake -B build
cmake --build build -j
```

## Patch day

When `everquest.h` struct fields change, regenerate the Rust bindings:

```sh
(cd ../showeq-decoder-rs && python3 tools/gen_eqstructs.py src/everquest.h)
```

The pre-push hook enforces this; the build will not link if bindings
are stale.

## Architecture

- `seq-decode` — per-opcode parsers, depend on `seq-eqstructs` for
  struct layouts mirrored from `everquest.h`.
- `seq-bridge` — cxx staticlib exposing decoded views to C++.
- `seq-eqstructs` — committed Rust bindings generated from
  `everquest.h` via `tools/gen_eqstructs.py`.

The daemon's per-opcode handlers call the bridge directly and consume
the decoded view without translating into a C struct.
