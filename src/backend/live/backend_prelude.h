/*
 * backend_prelude.h — live/test backend
 *
 * Pulled into src/packetinfo.cpp right after "everquest.h". Each backend dir
 * ships a file with THIS name; CMake puts the selected backend dir on the
 * include path (${SEQ_BACKEND_DIR}), so the right one resolves with no #ifdef
 * in core. Live/Test define no extra wire structs beyond everquest.h, so this
 * is intentionally empty. The eql backend's copy includes everquest_legends.h.
 */
#ifndef SEQ_BACKEND_PRELUDE_H
#define SEQ_BACKEND_PRELUDE_H
#endif // SEQ_BACKEND_PRELUDE_H
