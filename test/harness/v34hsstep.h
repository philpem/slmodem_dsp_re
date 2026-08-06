/*
 * v34hsstep.h -- drive ONE dispatch case of `v34handshak` and step it once.
 *
 * `v34handshak` is 61,541 bytes, the largest function in the object, and it
 * is a dispatch over three concurrent state machines rather than one body.
 * Reconstructing it as a unit has never been attempted here because the
 * measurement in finding 220 says it cannot be: context growth is cumulative
 * output converging to 1:1, so the wall is a TURN count, and a function this
 * size is hundreds of turns however carefully it is read.
 *
 * The way out is per-dispatch-case testing, and this is the fixture for it.
 * The three state words are plain halfwords in an object the test allocates,
 * so a test can write one, call the function once, and compare -- which turns
 * table 3's 27.6 KB into sixteen independently committable units instead of
 * one that has to work entirely before anything passes.
 *
 * WHAT IT DOES NOT DO.  It does not reconstruct anything.  `v34handshak` has
 * no reconstruction yet, so BOTH sides call the blob today; see `v34hs_step`
 * for the one-line swap that makes side A the reconstruction, and
 * docs/v34handshak.md for what a per-case agent does with it.
 *
 * Blob-against-blob is not a vacuous comparison here, because the two sides
 * are at DIFFERENT ADDRESSES and are brought up by DIFFERENT CODE: side A is
 * initialised by this tree's `v34handshakinit` and side B by the blob's.  An
 * agreeing step therefore says the fixture is deterministic, address
 * independent and fully seeded -- the three things a per-case agent has to be
 * able to assume before its own failures mean anything.
 */

#ifndef DSPLIB_TEST_V34HSSTEP_H
#define DSPLIB_TEST_V34HSSTEP_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ---------------------------------------------------------------------------
 * The three state halfwords (finding 213), and the guards that decide which
 * of the three dispatches sees them.
 *
 * Read off the prologue at 0x628f0 rather than assumed:
 *
 *   62925  movzwl 0x2aa0(%ebx),%edx      ; the transmit sample limit
 *   62933  cmp    %dx,0x221c(%ebx)       ; against the transmit cursor
 *   6293e  jge    629ed                  ; cursor >= limit: once-per-block
 *   62957  movswl 0x3596(%esi),%eax      ; else txstate, table 1
 *
 *   629f1  cmpw   $0x5,(%ebx)            ; ebx = obj+0x264
 *   629f5  jle    62ae3                  ; <= 5: txstate, table 2
 *   62a02  movswl 0x3594(%edx),%eax      ; else the rxstate compare chain
 */
#define V34HS_MICROSTATE	0x3592
#define V34HS_RXSTATE		0x3594
#define V34HS_TXSTATE		0x3596

#define V34HS_TXCURSOR		0x221c	/* short: samples emitted so far   */
#define V34HS_TXLIMIT		0x2aa0	/* short: how many the block wants */
#define V34HS_RXCOUNT		0x0264	/* short: receiver +0x00           */
#define V34HS_FSKGATE		0xa8a0	/* int:  non-zero diverts at 64a87 */
#define V34HS_PROGRESS		0x0004	/* int:  the code the step reports */

/*
 * The one rxstate that reaches the microstate table.  0x64a64 runs V34agc and
 * `fskdemodulate` and only then reads +0x3592, so this is the route to all
 * sixteen of table 3's cases.  Neither of those two functions writes any of
 * the three state words -- swept over the whole of .text, finding 285 -- so a
 * microstate written before the call is still there when the dispatch reads
 * it.
 */
#define V34HS_RX_DPSK		43	/* -> 0x64a64 -> table 3 */
#define V34HS_RX_RECEIVE	4	/* -> 0x653e4            */
#define V34HS_RX_WAIT		35	/* -> 0x6752c            */

enum v34hs_route {
	/*
	 * Table 1, .rodata+0x2da0: txstate INSIDE the per-sample loop.
	 *
	 * DANGEROUS AND DELIBERATELY SO -- see finding 287.  The loop bottom
	 * at 0x629e0 re-tests the cursor against the limit and jumps back to
	 * the dispatch, and the default arm IS that bottom, so a txstate with
	 * no case of its own spins forever.  `v34hs_route` takes the sample
	 * budget explicitly for this reason and `v34hs_step` arms an alarm.
	 */
	V34HS_ROUTE_TXSAMPLE,
	/* Table 2, .rodata+0x2ee8: txstate once per block. */
	V34HS_ROUTE_TXBLOCK,
	/*
	 * The rxstate compare chain, which has no table.  With rxstate
	 * V34HS_RX_DPSK this is the route to table 3, .rodata+0x3000, the
	 * microstate dispatch -- 27.6 KB and sixteen cases, which is #57.
	 */
	V34HS_ROUTE_RXCHAIN
};

/* What one step did, per side. */
struct v34hs_obs {
	unsigned	changed;	/* object bytes the step wrote       */
	unsigned	first;		/* lowest offset written, ~0u if none*/
	unsigned	last;		/* highest offset written            */
	unsigned	hash;		/* FNV-1a over (offset, new byte)    */
	unsigned	lines;		/* diagnostic lines printed          */
	short		mst, rxst, txst;/* the three words afterwards        */
	int		progress;	/* the int at obj+0x04 afterwards    */
};

/*
 * Build both objects and bring them up.  `mode` is `v34handshakinit`'s: 0 is
 * the cold start every case here uses, 1 retrain, 2 rate renegotiation, 4
 * Modem-on-Hold.
 *
 * The objects are filled with VARIED pseudorandom bytes from a fixed LCG and
 * never zeroed, both sides identically (finding 230): zero is the one value
 * that makes a field nothing has written look deliberate.
 */
void v34hs_setup(int mode);

/* Set the guards so the named dispatch is the one that runs. */
void v34hs_route(enum v34hs_route r, short samples);

/* Write the three state halfwords on both sides. */
void v34hs_state(short mst, short rxst, short txst);

/* Call the function once on each side and record what it did. */
void v34hs_step(void);

/*
 * Compare the two sides: the whole object byte for byte with the pointer
 * fields excluded, the two self-pointers by offset-from-own-base, and both
 * transcripts.  This is the check that says the fixture is sound.
 */
void v34hs_compare(const char *what, long tag);

const struct v34hs_obs *v34hs_observed(int side);
const char *v34hs_text(int side);

/* Raw access, for a case that needs a companion field set up. */
void *v34hs_object(int side);
void v34hs_poke_short(unsigned off, short v);
void v34hs_poke_int(unsigned off, int v);
void v34hs_poke_byte(unsigned off, unsigned char v);
short v34hs_peek_short(int side, unsigned off);

/*
 * Turn the diagnostics on for both sides.  `v34handshak` indexes `StateName`
 * unbounded (D42), so every state word must stay in 0..86 while this is on.
 */
void v34hs_debug(int on);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_TEST_V34HSSTEP_H */
