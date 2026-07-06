/*
 * s_backend_structs.h — eql (EverQuest Legends) backend
 *
 * Spliced into the EQPacketTypeDB ctor in src/packetinfo.cpp right after
 * "s_everquest.h", where the AddStruct(typeName) macro is in scope. Registers
 * the Legends wire structs so on()'s SZC_Match (OP_ClientUpdate,
 * OP_MobUpdate) and the SZC_None overlays (OP_PlayerProfile) resolve a valid
 * size — an unregistered type makes on() silently drop every packet.
 *
 * No include guard — this is #included inside a function body, once.
 */
AddStruct(legendsPlayerSelfPos);
AddStruct(legendsSpawnStruct);
AddStruct(legendsMobUpdateStruct);
AddStruct(legendsCharProfileHdr);
