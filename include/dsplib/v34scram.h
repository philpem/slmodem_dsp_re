/*
 * v34scram.h -- ITU-T V.34: the scrambler and descrambler pair.
 *
 * Two polynomials each way, and `preinitdigital` installs one of each by
 * address: `f359c == 0x65` scrambles with GPC and descrambles with GPA,
 * anything else the other way about.  The two ends of a call must use
 * opposite polynomials, which is what makes that field the originate/answer
 * flag.  See src/pump/v34/v34scram.c and finding F177.
 */

#ifndef DSPLIB_V34SCRAM_H
#define DSPLIB_V34SCRAM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Advance the transmit register by one 16-bit step.  `nbits` is the caller's
 * count of bits still wanted and comes straight back as `nbits - 16`;
 * nothing else reads it.  The word fed in is 0xffff unless the object's
 * scripted-bits harness is on.
 */
short scrambleGPC(void *obj, short nbits);
short scrambleGPA(void *obj, short nbits);

/*
 * Push `nbits` fresh bits into the receive register.  Nothing is recovered
 * until it holds more than 31, and the recovered word is observable only
 * through the object's capture sink -- both return 0 unconditionally.
 */
int descrambleGPC(void *obj, unsigned short bits, unsigned short nbits);
int descrambleGPA(void *obj, unsigned short bits, unsigned short nbits);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_V34SCRAM_H */
