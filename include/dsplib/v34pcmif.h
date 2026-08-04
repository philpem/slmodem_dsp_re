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
 * Set the transmit scale to the one constant the object ever uses, and say
 * so through `edprintf`.  No parameter: the value is built in.
 */
void VPcmV34SetTxScale(void *obj);

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

/*
 * The three requests the shell makes of a running connection.  Each forks on
 * `status`: 1 and 2 mean a PCM receiver has the line and the request goes to
 * the C++ side down a chain of three pointers instead.
 */

/* Tear the connection down.  Clears the rate request and both bounds. */
void VPcmV34InitiateHangUp(void *obj);

/*
 * Ask for a different rate.  `req` is 0, 2 or 5 for one index down, 3 for one
 * up and anything else for "no particular rate"; a step that would leave
 * [rate_min, rate_max] leaves the request unchanged rather than clamping.
 * On the PCM arm the code is forwarded verbatim and means something else.
 */
void VPcmV34InitiateRateRenegotiation(void *obj, int req);

/*
 * Rebuild the transmitter for a V.90 rate renegotiation -- the only one of
 * the three that does not go through the handshake.  Both parameters are
 * tested against zero only: `rrn_type` selects 15 or 11 for `v90_receiver`
 * and `constel_size` selects 0x89b0 or 0x8990 for `f382`.  The names are the
 * object's own, from the diagnostic this prints.
 */
void VPcmV34SetV90RateReneg(void *obj, short rrn_type,
			    unsigned char constel_size);

/*
 * Two progress reports the handshake makes, and nothing acts on.
 *
 * Each is a debug gate and one string; neither touches the object or reads
 * its argument.  The argument exists all the same: the object overwrites its
 * own first argument slot with the format pointer and tail-jumps into
 * `dsplibs_debug_printf`, which a function with no parameters would have no
 * slot to do.  So the transcript is the whole observable behaviour, and a
 * test that compared only state would pass on an empty body.
 *
 * `v34handshak` calls one of each and nothing else calls either.
 */
void VPcmV34ReportStartOfEchoAdapt(void *obj);
void VPcmV34ReportMiddleOfEchoAdapt(void *obj);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34PCMIF_H */
