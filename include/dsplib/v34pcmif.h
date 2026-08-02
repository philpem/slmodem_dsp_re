/*
 * v34pcmif.h -- the V.90/K56Flex side's hooks into V.34.  See v34pcmif.c.
 */

#ifndef DSPLIB_V34PCMIF_H
#define DSPLIB_V34PCMIF_H

#ifdef __cplusplus
extern "C" {
#endif

/* Record the timing offset the V.34 receiver has settled on. */
void VPcmV34LogTimingOffset(void *obj, short offset);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34PCMIF_H */
