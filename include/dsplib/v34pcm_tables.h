/*
 * v34pcm_tables.h -- data tables of the V.34 PCM (VPcmV34) module.
 */

#ifndef DSPLIB_V34PCM_TABLES_H
#define DSPLIB_V34PCM_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#define V34_DISCONNECT_THRESH_ENTRIES 8

/**
 * @brief Signal-level disconnect thresholds for VPcmV34, an eight-step ladder
 * roughly 1 dB apart (see finding F270).
 *
 * Indexed 0..7 by a biased, clamped signal level; an out-of-range index
 * (including any negative one, via an unsigned compare) falls back to
 * entry 3. Read by `VPcmV34SetMinimumSigLevel`, `VPcmV34InitiateRetrain`
 * and `VPcmV34Create`, all of them now file-local in one translation unit,
 * `src/pump/v34/v34pcmmain.cpp`, so `V34DisconnectThreshTable` is `static`
 * there and the reference records it LOCAL for the same reason.  A test
 * that names it declares it itself and resolves against the test tier's
 * globalized copy of that object (`tools/testvisible.py`).
 */

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34PCM_TABLES_H */
