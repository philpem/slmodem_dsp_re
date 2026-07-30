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

unsigned char linear2alaw(int pcm_val);
int alaw2linear(unsigned char a_val);

unsigned char linear2ulaw(int pcm_val);
int ulaw2linear(unsigned char u_val);

unsigned char alaw2ulaw(unsigned char a_val);
unsigned char ulaw2alaw(unsigned char u_val);

#endif /* DSPLIB_PCM_H */
