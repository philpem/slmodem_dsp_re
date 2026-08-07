/*
 * v34pcm_tables.h -- data tables of the V.34 PCM (VPcmV34) module.
 */

#ifndef DSPLIB_V34PCM_TABLES_H
#define DSPLIB_V34PCM_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * `V34DisconnectThreshTable` is 32 bytes at `.data + 0xc0` and is indexed
 * 0..7 with everything else clamped to 3.  It is `d` -- file-local -- in the
 * blob, and is reachable for comparison only because the Makefile globalizes
 * file-local symbols before renaming them (see tools/symmap.py).
 */
#define V34_DISCONNECT_THRESH_ENTRIES 8

extern const int V34DisconnectThreshTable[V34_DISCONNECT_THRESH_ENTRIES];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34PCM_TABLES_H */
