/*
 * v34info.h -- ITU-T V.34: the INFO0 and INFO1a message codecs.
 *
 * Declares the `extern "C"` exports of `VPcmV34Main.cpp` that assemble or
 * take apart a phase-2 INFO message.  The rest of that translation unit's C
 * interface is in v34pcmif.h; the two headers are one TU split by role.
 * See src/pump/v34/v34info.c.
 */

#ifndef DSPLIB_V34INFO_H
#define DSPLIB_V34INFO_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The INFO message buffer every function here takes.
 *
 * THIRTEEN SHORTS IS THE MINIMUM A CALLER MUST SUPPLY.  Nothing in the
 * object states a length; this is the highest index any of these functions
 * touches (`V34SetINFO0dBits` writes index 12) and there is no bounds check
 * anywhere.  A buffer sized from the ten values the debug prints show would
 * be written past.
 */
#define V34_INFO_MSG_SHORTS	13

/*
 * The layout of the record `V34GiveProbeResults` copies from: 25 of them,
 * 44 bytes apart, each holding a double 32 bytes in.  All three numbers are
 * the object's own loop constants.
 */
#define V34_PROBE_OFFSET	0x20
#define V34_PROBE_STRIDE	0x2c

/*
 * Copy the probe results the C++ side measured into the V.34 object, if
 * either PCM receiver is running.  Always returns 0.
 */
int V34GiveProbeResults(void *obj, const void *src);

/*
 * Assemble an outbound INFO0.  `bits` is the message being built and must be
 * at least V34_INFO_MSG_SHORTS long.
 */
void V34SetINFO0aBits(void *obj, short *bits);
void V34SetINFO0dBits(void *obj, short *bits);

/*
 * Take apart a received INFO0.  Unpacks it into the object's 41-entry bit
 * vector and settles whether a short phase 2 is on; does nothing at all
 * without a V.90 receiver running.
 */
void V34GiveINFO0dBits(void *obj, const short *bits);

/*
 * Take apart a received INFO1a.  Returns 1 if the Uinfo code was 6 and 0
 * otherwise -- including on the paths that decode nothing.
 */
int V34GiveINFO1aBits(void *obj, const short *bits);

/*
 * Assemble an outbound INFO1a, INFO1c or INFO1d -- which one depends on the
 * two receiver flags, the role flag and the session's layout selector.  Also
 * moves `v90_receiver` on to 2, or back to 0 if the modem has no Uinfo to
 * report.
 *
 * ALWAYS RETURNS 0, and no caller in the object looks at it; the type is
 * `int` because both epilogues clear `%eax` explicitly.  Defined in
 * `src/pump/v34/v34info1a.cpp` rather than beside the rest of this header's
 * functions, because it calls a C++ member and a C translation unit cannot
 * name one.
 */
int V34SetINFO1aBits(void *obj, short *bits);

/*
 * Build the first short of one of V.92's six Modem-on-Hold messages, chosen
 * by the object's `moh_message`.  A selector above 5 writes nothing at all.
 */
void VPcmV34SetMohMessageBits(void *obj, short *bits);

/*
 * The other direction: decode the first short of an arriving MOH message
 * into `moh_recvd`, and its payload nibble into whichever field that message
 * carries one in.  Only index 0 is read.  A message matching nothing is
 * forced to MHnack, with three lines of diagnostic saying so.
 */
void VPcmV34InterpretMohMessageBits(void *obj, const short *bits);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34INFO_H */
