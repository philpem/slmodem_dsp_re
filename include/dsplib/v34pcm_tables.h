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
 * and `VPcmV34Create`. File-local in the blob; exposed here only because
 * the build globalizes file-local symbols for comparison (see
 * `tools/symmap.py`).
 */
extern const int V34DisconnectThreshTable[V34_DISCONNECT_THRESH_ENTRIES];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34PCM_TABLES_H */
