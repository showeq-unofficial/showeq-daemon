# Optional: Rust decoder integration

Stage A of the parallel Rust decoder track (`OP_MobUpdate` parsed via
`showeq-decoder-rs`'s `seq-bridge` cxx crate) is opt-in. Default builds
have no Rust toolchain dependency. To enable:

```sh
# One-time toolchain install:
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
. "$HOME/.cargo/env"

# Sibling-checkout layout that CMake expects:
#   parent-dir/
#     showeq-daemon/
#     showeq-decoder-rs/
git -C .. clone git@github.com:showeq-unofficial/showeq-decoder-rs.git

cmake -B build -DSEQ_USE_RUST=ON
cmake --build build -j
build/showeq-daemon --rust-opcodes OP_MobUpdate ...   # route opcode at runtime
```

Without `-DSEQ_USE_RUST=ON`, `--rust-opcodes` is accepted but ignored
with a warning.
