/*
 * v34pcm_tables.c -- data tables of the V.34 PCM (VPcmV34) module.
 *
 * Reconstructed from dsplibs.o.  One so far.
 */

#include "dsplib/v34pcm_tables.h"

/*
 * The minimum-signal-level ladder, eight 32-bit entries at `.data + 0xc0`.
 *
 * Three functions read it, all of them VPcmV34*:
 * `VPcmV34SetMinimumSigLevel`, `VPcmV34InitiateRetrain` and `VPcmV34Create`.
 * The first indexes it with a value that is CLAMPED, not masked:
 *
 *     mov  0x60(%edx),%eax        ; a signed level from the parameter block
 *     add  $0x30,%eax             ; bias it to an index
 *     cmp  $0x7,%eax
 *     jbe  ok
 *     mov  $0x3,%eax              ; out of range -> entry 3, which is 101
 *  ok: mov  0xc0(,%eax,4),%eax
 *
 * `jbe` is unsigned, so the single compare rejects negatives too: an index
 * below zero wraps large and takes the same default.  The default is entry 3
 * and not entry 0, which is why the clamp is worth writing down -- it is a
 * chosen fallback level rather than a saturation.
 *
 * THE VALUES ARE REFERENCE BYTES AND NOT A GENERATOR, and that is a failure
 * rather than a choice; finding F270 records what was tried.
 */
const int V34DisconnectThreshTable[V34_DISCONNECT_THRESH_ENTRIES] = {
	71, 80, 90, 101, 113, 127, 142, 160,
};
