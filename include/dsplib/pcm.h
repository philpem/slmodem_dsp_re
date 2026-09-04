/*
 * pcm.h -- ITU-T G.711 companding.
 *
 * A-law and u-law conversion to and from 16-bit linear PCM.  Note that this
 * is the full-16-bit-scale variant used throughout dsplibs, not the 13/14-bit
 * form found in the Sun reference code -- see src/service/pcm.c.
 */

#ifndef DSPLIB_PCM_H
#define DSPLIB_PCM_H

/* Bias added before u-law segmentation (CCITT G.711 "BIAS"), 16-bit scale. */
#define PCM_ULAW_BIAS 0x84

/**
 * @brief Convert a 16-bit linear PCM sample to A-law.
 * @param pcm_val 16-bit linear sample.
 * @return The A-law code.
 */
unsigned char linear2alaw(int pcm_val);

/**
 * @brief Convert an A-law code to 16-bit linear PCM.
 * @param a_val The A-law code.
 * @return The 16-bit linear sample.
 */
int alaw2linear(unsigned char a_val);

/**
 * @brief Convert a 16-bit linear PCM sample to u-law.
 * @param pcm_val 16-bit linear sample.
 * @return The u-law code.
 */
unsigned char linear2ulaw(int pcm_val);

/**
 * @brief Convert a u-law code to 16-bit linear PCM.
 * @param u_val The u-law code.
 * @return The 16-bit linear sample.
 */
int ulaw2linear(unsigned char u_val);

/**
 * @brief Convert an A-law code directly to u-law, via lookup table.
 *
 * Avoids a round trip through linear PCM, which would lose accuracy twice.
 * The table is extracted verbatim from the object (CCITT G.711 Table 2).
 *
 * @param a_val The A-law code.
 * @return The equivalent u-law code.
 */
unsigned char alaw2ulaw(unsigned char a_val);

/**
 * @brief Convert a u-law code directly to A-law, via lookup table.
 *
 * Avoids a round trip through linear PCM. The table is extracted verbatim
 * from the object (CCITT G.711 Table 1).
 *
 * @param u_val The u-law code.
 * @return The equivalent A-law code.
 */
unsigned char ulaw2alaw(unsigned char u_val);

#endif /* DSPLIB_PCM_H */
