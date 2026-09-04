/*
 * v34scram.h -- ITU-T V.34: the scrambler and descrambler pair.
 *
 * Two polynomials each way, and `preinitdigital` installs one of each by
 * address: `role == 0x65` scrambles with GPC and descrambles with GPA,
 * anything else the other way about.  The two ends of a call must use
 * opposite polynomials, which is what makes that field the originate/answer
 * flag.  See src/pump/v34/v34scram.c and finding F177.
 */

#ifndef DSPLIB_V34SCRAM_H
#define DSPLIB_V34SCRAM_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Scramble sixteen bits with the GPC polynomial.
 *
 * Advances the transmit shift register by one 16-bit step. The word mixed
 * in is 0xffff unless the object's scripted-bits test harness (`data_enable`)
 * is active, in which case it is drawn from `tx_data` instead.
 *
 * @param obj    The V.34 modem object (`struct v34_object *`).
 * @param nbits  Caller's remaining bit count; unused except to be decremented.
 * @return `nbits - 16`.
 */
short scrambleGPC(void *obj, short nbits);

/**
 * @brief Scramble sixteen bits with the GPA polynomial.
 *
 * Same role as scrambleGPC(), but GPA's near feedback tap falls inside a
 * single 16-bit step rather than past it, so the feedback has to be folded
 * in four times (five bits at a time) instead of once.
 *
 * @param obj    The V.34 modem object (`struct v34_object *`).
 * @param nbits  Caller's remaining bit count; unused except to be decremented.
 * @return `nbits - 16`.
 */
short scrambleGPA(void *obj, short nbits);

/**
 * @brief Descramble `nbits` fresh bits with the GPC polynomial.
 *
 * Pushes the new bits into the receive shift register. Once the register
 * holds more than 31 bits, one recovered word is produced and appended to
 * the object's capture sink (`rx_data`, capped at 64 entries) if the
 * scripted-bits harness is enabled; otherwise the word is discarded.
 *
 * @param obj    The V.34 modem object (`struct v34_object *`).
 * @param bits   The new bits to push in.
 * @param nbits  How many bits of @p bits are valid.
 * @return Always 0; the recovered word is only observable via the capture sink.
 */
int descrambleGPC(void *obj, unsigned short bits, unsigned short nbits);

/**
 * @brief Descramble `nbits` fresh bits with the GPA polynomial.
 *
 * Same role as descrambleGPC(), for the GPA polynomial's near-tap feedback
 * shape.
 *
 * @param obj    The V.34 modem object (`struct v34_object *`).
 * @param bits   The new bits to push in.
 * @param nbits  How many bits of @p bits are valid.
 * @return Always 0; the recovered word is only observable via the capture sink.
 */
int descrambleGPA(void *obj, unsigned short bits, unsigned short nbits);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34SCRAM_H */
