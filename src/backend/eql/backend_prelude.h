/*
 * backend_prelude.h — eql (EverQuest Legends) backend
 *
 * Pulled into src/packetinfo.cpp right after "everquest.h" (resolved via the
 * selected backend dir on the include path). For eql this brings the Legends
 * wire structs into scope so the size-registry rows in s_backend_structs.h can
 * take their sizeof(). Legends shares the SOE stream layer with Live but the
 * application structs are fully remapped — kept isolated in everquest_legends.h.
 */
#ifndef SEQ_BACKEND_PRELUDE_H
#define SEQ_BACKEND_PRELUDE_H
#include "everquest_legends.h"
#endif // SEQ_BACKEND_PRELUDE_H
