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

/**
 * The size of the INFO message buffer every function here takes, in shorts.
 *
 * Nothing in the object states a length explicitly; 13 is the highest index
 * any of these functions touches (`V34SetINFO0dBits` writes index 12), and
 * there is no bounds check anywhere, so a caller must supply at least this
 * many shorts.
 */
#define V34_INFO_MSG_SHORTS	13

/** Byte offset of the first entry in the probe-results record `V34GiveProbeResults` copies from. */
#define V34_PROBE_OFFSET	0x20
/** Byte stride between entries in that record; each entry holds one double. */
#define V34_PROBE_STRIDE	0x2c

/**
 * @brief Copy the probe results the C++ side measured into the V.34 object.
 *
 * A no-op unless either PCM receiver is running.
 *
 * @param obj  The V.34 modem object.
 * @param src  The probe-results record (see #V34_PROBE_OFFSET, #V34_PROBE_STRIDE).
 * @return Always 0.
 */
int V34GiveProbeResults(void *obj, const void *src);

/**
 * @brief Assemble an outbound INFO0-a message.
 * @param obj   The V.34 modem object.
 * @param bits  The message being built; must be at least
 *              #V34_INFO_MSG_SHORTS long.
 */
void V34SetINFO0aBits(void *obj, short *bits);

/**
 * @brief Assemble an outbound INFO0-d message.
 * @param obj   The V.34 modem object.
 * @param bits  The message being built; must be at least
 *              #V34_INFO_MSG_SHORTS long.
 */
void V34SetINFO0dBits(void *obj, short *bits);

/**
 * @brief Take apart a received INFO0-d message.
 *
 * Unpacks it into the object's 41-entry bit vector and settles whether a
 * short Phase 2 is in use. Does nothing without a V.90 receiver running.
 *
 * @param obj   The V.34 modem object.
 * @param bits  The received message, at least #V34_INFO_MSG_SHORTS long.
 */
void V34GiveINFO0dBits(void *obj, const short *bits);

/**
 * @brief Take apart a received INFO1a message.
 * @param obj   The V.34 modem object.
 * @param bits  The received message, at least #V34_INFO_MSG_SHORTS long.
 * @return 1 if the Uinfo code was 6, 0 otherwise -- including on the paths
 *         that decode nothing.
 */
int V34GiveINFO1aBits(void *obj, const short *bits);

/**
 * @brief Take apart a received INFO1d message.
 *
 * Settles whether a PCM upstream is in play -- the same session flag
 * V34GiveINFO1aBits() writes -- and, if one is and the configuration bars
 * it, retrains the modem back to V.90.
 *
 * Note the return value is not that session flag read back: the retraining
 * path clears the flag on its way out, so the two disagree on exactly the
 * cases that would otherwise make this redundant with V34GiveINFO1aBits()'s
 * return value.
 *
 * Defined in `src/pump/v34/v34pcmmain.cpp` rather than beside its three
 * siblings: although the function itself is a plain, unmangled `extern "C"`
 * name, it calls `VPcmV34InitiateRetrain`, a C++ member that only lives in
 * that translation unit.
 *
 * @param obj   The V.34 modem object.
 * @param bits  The received message, at least #V34_INFO_MSG_SHORTS long.
 * @return 1 only if it retrained, 0 otherwise.
 */
int V34GiveINFO1dBits(void *obj, const short *bits);

/**
 * @brief Assemble an outbound INFO1a, INFO1c or INFO1d message.
 *
 * Which of the three gets built depends on the two receiver flags, the
 * role flag and the session's layout selector. Also advances `v90_receiver`
 * to 2, or back to 0 if the modem has no Uinfo to report.
 *
 * Defined in `src/pump/v34/v34info1a.cpp` rather than beside the rest of
 * this header's functions, because it calls a C++ member and a C
 * translation unit cannot name one.
 *
 * @param obj   The V.34 modem object.
 * @param bits  The message being built; must be at least
 *              #V34_INFO_MSG_SHORTS long.
 * @return Always 0; no caller in the object examines it.
 */
int V34SetINFO1aBits(void *obj, short *bits);

/**
 * @brief Build the first short of an outbound V.92 Modem-on-Hold message.
 *
 * Which of the six MOH messages gets built is chosen by the object's
 * `moh_message`; a selector above 5 writes nothing.
 *
 * @param obj   The V.34 modem object.
 * @param bits  The message being built.
 */
void VPcmV34SetMohMessageBits(void *obj, short *bits);

/**
 * @brief Decode the first short of an arriving V.92 Modem-on-Hold message.
 *
 * Only index 0 of @p bits is read. Records the message kind in
 * `moh_recvd` and its payload nibble in whichever field that message
 * carries one in. A message matching nothing is treated as MHnack, with
 * a diagnostic saying so.
 *
 * @param obj   The V.34 modem object.
 * @param bits  The received message.
 */
void VPcmV34InterpretMohMessageBits(void *obj, const short *bits);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34INFO_H */
