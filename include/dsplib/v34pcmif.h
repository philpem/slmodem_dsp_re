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

/*
 * Where the V.34 object keeps the two arrays the PCM side fills in and reads
 * back.  Both return interior pointers and neither copies anything.
 */
double *V34XF_GetProbeResultsPtr(void *obj);
int *V34XF_GetInfo0BitsPtr(void *obj);

/*
 * The measured round-trip delay plus 480 samples.  Truncated to 16 bits, so
 * a large stored delay comes back negative -- see the note in v34pcmif.c.
 */
short V34XF_GetRTD(void *obj);

/*
 * Phase-3 arrivals.  Each advances the object's `v90_receiver` counter;
 * TRN2d ratchets it rather than setting it.  `constel` and `silence_scr` are
 * carried bits of the message, not sizes.
 */
void V34XF_IndicateJdReceived(void *obj, unsigned char constel,
			      unsigned char silence_scr);
void V34XF_IndicateDilReceived(void *obj, unsigned char constel);
void V34XF_IndicateTrn2dReceived(void *obj);
void V34XF_IndicateK56FlexRateDetermined(void *obj);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34PCMIF_H */
