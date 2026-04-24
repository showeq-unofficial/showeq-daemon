#pragma once

// Pure translation functions between in-memory showeq-c types (Item, Spawn,
// zoneChangeStruct, ...) and seq.v1 protobuf messages. All functions here
// must be side-effect free -- they exist so sessionadapter.cpp can stay
// focused on wire I/O, and so the Rust decoder in Phase 4 has a clear spec
// to match byte-for-byte.
//
// Populated alongside the extraction of spawnshell.*, zonemgr.*, player.*
// from showeq-c. Empty stub in Phase 0.

namespace seq::encode {
// Free functions will be added as types are extracted:
//   void fillSpawn(seq::v1::Spawn* out, const Item& in);
//   void fillPos(seq::v1::Pos* out, const Spawn& in);
//   ...
}
