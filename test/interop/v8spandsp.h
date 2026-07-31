/*
 * v8spandsp.h -- SpanDSP's end of the interop calls.
 *
 * Split from v8neg.h so that the peer, which has to build 32-bit against the
 * blob, never sees a SpanDSP header.
 */

#ifndef DSPLIB_INTEROP_V8SPANDSP_H
#define DSPLIB_INTEROP_V8SPANDSP_H

#include "spandsp.h"

/* The wide menu, as SpanDSP names the modulations. */
#define V8NEG_OURS	(V8_MOD_V21 | V8_MOD_V23 | V8_MOD_V32 | V8_MOD_V34)

/* And the narrow one, for the calls that have to prove an intersection. */
#define V8NEG_NARROW	(V8_MOD_V21 | V8_MOD_V32)

/* Fill in what SpanDSP's end offers. */
void v8neg_spandsp_parms(v8_parms_t *parms, uint32_t modulations);

/* SpanDSP's status codes, for the log. */
const char *v8neg_status_name(int st);

#endif /* DSPLIB_INTEROP_V8SPANDSP_H */
