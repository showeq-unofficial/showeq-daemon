/*
 * s_backend_structs.h — live/test backend
 *
 * Spliced into the EQPacketTypeDB ctor in src/packetinfo.cpp right after
 * "s_everquest.h", where the AddStruct(typeName) macro is in scope. This is
 * the per-target size-registry extension: a struct missing from here makes
 * connect2 SZC_Match silently drop every matching packet (sizeof():0).
 *
 * Live/Test add no structs beyond s_everquest.h, so this is empty. The eql
 * backend's copy carries the AddStruct(legends…) rows.
 *
 * No include guard — this is #included inside a function body, once.
 */
