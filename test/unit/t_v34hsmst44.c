/*
 * t_v34hsmst44.c -- `v34handshak`'s microstate 44, `DET_INFO`.
 *
 * OURS ON SIDE A.  `v34hs_ours(1)` puts this tree's `v34handshak` on side A
 * of test/harness/v34hsstep.c, so every case below is an ordinary tier-1
 * differential comparison of our arm against the blob's over the whole
 * 44,096-byte object, the five blocks it points at, the padding around them
 * and both transcripts.  t_v34hsstep.c goes on proving the FIXTURE, blob
 * against blob; this proves one arm.
 *
 * WHAT 44 IS.  `fskdemodulate` runs earlier on this route and leaves the bits
 * it recovered in `obj->fsk`; 44 is the clock that consumes them, ONE PER
 * CALL.  It steps a CRC-16 register in the record at +0xaa70 while the
 * message is arriving, stores a byte every eighth bit, and when the counter
 * reaches the message length plus the sixteen CRC bits it compares the
 * register against what arrived and either accepts the message or restarts
 * the exchange.
 *
 * WHAT IS COVERED, and what is not:
 *
 *     0x668c0   the bit clock, all five of its exits
 *     0x6c133   the CRC step without the xor
 *     0x6bda0   the restart a failed CRC takes, and its three sub-branches
 *     0x718fc   role == 0x66, the answering side
 *     0x71920   role == 0x65 with a V.90 receiver
 *     0x6e534   the accept path, and all four of its arms: the default at
 *               0x6e552 and the three selected by a message length --
 *               0x6f438 (0x4d bits, INFO1c), 0x6ed17 (0x26, INFO1a) and
 *               0x6ea38 (0x08, Modem-on-Hold)
 *
 * THE TXSTATE IS PART OF THE FIXTURE (finding F288).  Every arm of table 3
 * leaves through the once-per-block transmit dispatch, so a microstate case
 * is a microstate arm AND a transmit arm.  MOH_SILENCE (81) throughout, which
 * is above table 2's window and selects the dispatch's own default at 0x62a40
 * -- except that the restart FORCES TX_DPSK on its way out, which is table
 * 2's arm at 0x644c9.  That arm exists (finding F354 put it there for
 * microstate 79) and this is why the restart can be driven at all.
 *
 * THE FSK RECEIVER IS HELD STILL.  `obj->fsk_inhibit` non-zero makes
 * `fskdemodulate` return before it touches `nbits` or `sr` (DPSK.c), so the
 * two fields this arm reads are the test's to choose rather than whatever the
 * fill's coefficients made of four samples.  One case at the end leaves it
 * clear, so the arm is also driven downstream of the real demodulator.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "v34hsstep.h"
#include "dsplib/v34det.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34hshak.h"
#include "dsplib/v34filt.h"	/* the eight half-sine windows, by name    */

extern void ref_dftenergy(void *bins, short nbins, short scale);

/* The tail's own inputs, pinned so that the arm is the only variable. */
#define T44T_PROGRESS	0x0004
#define T44T_LVL_LIMIT	0x0230
#define T44T_LVL_COUNT	0x0234
#define T44T_TIMER_LO	0x0238
#define T44T_TIMER_HI	0x023c
#define T44T_MODE	0x2218

#define T44T_V90RECV	0x024c	/* int:   `v90_receiver`                   */
#define T44T_K56RECV	0x0250	/* int:   `k56flex_receiver`               */
#define T44T_INHIBIT	0x0402	/* short: `fsk_inhibit`                    */
#define T44T_F356A	0x356a
#define T44T_F358C	0x358c
#define T44T_F35A2	0x35a2	/* short: the message-descriptor length    */
#define T44T_DETCOEF	0x3564	/* the detector's coefficient pointer      */
#define T44T_REC_A9AC	0xa9ac	/* the INFO1a record the 0x4d arm installs */
#define T44T_REC_A9DC	0xa9dc	/* the INFO1a/1c record that arrived       */
#define T44T_TXBAUD	0xaa84
#define T44T_RXBAUD	0xaa96
#define T44T_RXCARRIER	0xaaa8
#define T44T_FAA80	0xaa80
#define T44T_FABE0	0xabe0	/* short: the MOH decoder's timeout code   */
#define T44T_FABF0	0xabf0	/* int:   1 sends the 8-bit arm to the tone*/
#define T44T_FABF4	0xabf4	/* int:   `moh_recvd`, the decoder's output*/
#define T44T_FABF8	0xabf8	/* byte:  raised by the 8-bit arm's other  */

/* The receiver, and the fields the 0x26 arm's two tables write. */
#define T44T_RX		0x0264
#define T44T_RX_FLAGS2	(T44T_RX + 0x122)
#define T44T_RX_F128	(T44T_RX + 0x128)
#define T44T_RX_GAIN	(T44T_RX + 0x136)
#define T44T_RX_STEP	(T44T_RX + 0x13a)
#define T44T_RX_F1AC	(T44T_RX + 0x1ac)
#define T44T_RX_F1AE	(T44T_RX + 0x1ae)
#define T44T_RX_F1B0	(T44T_RX + 0x1b0)
#define T44T_RX_CARRIER	(T44T_RX + 0x1b4)
#define T44T_RX_F1BA	(T44T_RX + 0x1ba)
#define T44T_RX_F1BE	(T44T_RX + 0x1be)
#define T44T_RX_F262	(T44T_RX + 0x262)
#define T44T_F3588	0x3588
#define T44T_F358A	0x358a
#define T44T_F359C	0x359c	/* short: 0x65 originate, 0x66 answer      */
#define T44T_PTR_AA6C	0xaa6c
#define T44T_PTR_AA70	0xaa70
#define T44T_COUNT	0xaa78	/* short: bits taken so far                */
#define T44T_COUNT_SRC	0xaa7c	/* short: reloaded on short phase 2        */
#define T44T_FAA7A	0xaa7a	/* short: cleared on entry, every call     */
#define T44T_NBITS	0xaae0	/* short: `fsk.nbits`                      */
#define T44T_SR		0xaae2	/* short: `fsk.sr`                         */
#define T44T_FABAE	0xabae
#define T44T_FABC2	0xabc2
#define T44T_REMOTE_V92 0xabc8
#define T44T_LOCALSHORT	0xabca
#define T44T_ISSHORT	0xabcc
#define T44T_RX_FLAGS	(0x0264 + 0x122)

/*
 * The two records.  `v34handshakinit` mode 0 aims +0xaa70 at +0xa97c, and the
 * restart installs +0xa94c at +0xaa6c; the first is ASSERTED below rather
 * than assumed, because every offset this file seeds is relative to it.
 */
#define T44T_REC	0xa97c
#define T44T_REC_MOVED	0xa9c0	/* where one case re-aims +0xaa70          */
#define T44T_BLK	0xa94c

#define T44T_R_CRC	0x14
#define T44T_R_F16	0x16
#define T44T_R_NBITS	0x18
#define T44T_R_F1A	0x1a
#define T44T_R_F1C	0x1c
#define T44T_R_F1E	0x1e
#define T44T_R_F20	0x20
#define T44T_R_F22	0x22
#define T44T_R_F24	0x24
#define T44T_R_F28	0x28
#define T44T_R_F2A	0x2a
#define T44T_R_F2C	0x2c

static int dump;

/*
 * Whether the object fill is the default one.  `changed` counts bytes that
 * DIFFER from what the fill left, so a write of the value already there is
 * not counted; that one measurement is asserted at the default fill and
 * everything else at every fill.  The differential comparison itself is never
 * relaxed at any knob.  Finding F359 made the same distinction.
 */
static int default_fill;

/*
 * Open a case: both objects built and brought up, the rxstate chain routed to
 * table 3, the three state words written, the tail's five inputs pinned, the
 * FSK receiver switched off, and every field this arm can write seeded to
 * something OTHER than what the arm will store.
 *
 * That last part is finding F345's warning: a claim about a value written is
 * untestable against a field that already holds it.
 */
static void
begin(short tx)
{
	int i;

	v34hs_setup(0);
	v34hs_route(V34HS_ROUTE_RXCHAIN, 0);
	v34hs_state(V34HS_DET_INFO, V34HS_RX_DPSK, tx);

	v34hs_poke_int(T44T_MODE, 0);
	v34hs_poke_int(T44T_TIMER_LO, 1000);
	v34hs_poke_int(T44T_TIMER_HI, 2000);
	v34hs_poke_int(T44T_LVL_LIMIT, 0x7fffffff);
	v34hs_poke_int(T44T_LVL_COUNT, 0);

	v34hs_poke_short(T44T_INHIBIT, 1);

	/* Non-zero, so the unconditional clear at 0x668c9 is observable. */
	v34hs_poke_short(T44T_FAA7A, 0x5a5a);

	/* The ten byte slots, distinct and none of them a value stored. */
	for (i = 0; i < 10; i++)
		v34hs_poke_short(T44T_REC + 2 * i, (short)(0x0301 + 0x111 * i));

	/* What the restart clears or sets, none of it already right. */
	v34hs_poke_short(T44T_F3588, 0x0a02);	/* bit 2 clear, others set */
	v34hs_poke_short(T44T_F358A, 0x7abc);
	v34hs_poke_short(T44T_LOCALSHORT, 0x4d4d);
	v34hs_poke_short(T44T_ISSHORT, 0x5e5e);
	for (i = 0; i <= 10; i++)
		v34hs_poke_short(T44T_FABAE + 2 * i, (short)(0x2001 + 7 * i));

	/* The record the restart installs and fills in. */
	v34hs_poke_short(T44T_BLK + 0x04, 0x1200);
	v34hs_poke_short(T44T_BLK + T44T_R_CRC, 0x1234);
	v34hs_poke_short(T44T_BLK + T44T_R_F16, 0x2345);
	v34hs_poke_short(T44T_BLK + T44T_R_NBITS, 0x3456);
	v34hs_poke_short(T44T_BLK + T44T_R_F1A, 0x4567);
	v34hs_poke_short(T44T_BLK + T44T_R_F1C, 0x5678);
	v34hs_poke_short(T44T_BLK + T44T_R_F1E, 0x6789);
	v34hs_poke_short(T44T_BLK + T44T_R_F20, 0x789a);
	v34hs_poke_short(T44T_BLK + T44T_R_F22, 0x1a2b);
	v34hs_poke_int(T44T_BLK + T44T_R_F24, 0x2b3c4d);
	v34hs_poke_short(T44T_BLK + T44T_R_F28, 0x3c4d);
	v34hs_poke_short(T44T_BLK + T44T_R_F2A, 0x4d5e);
	v34hs_poke_int(T44T_BLK + T44T_R_F2C, 0x5e6f70);
}

/* One bit's worth of state: what is waiting, and where the clock is. */
static void
bits(short nbits, short sr, short count, short len, short crc)
{
	v34hs_poke_short(T44T_NBITS, nbits);
	v34hs_poke_short(T44T_SR, sr);
	v34hs_poke_short(T44T_COUNT, count);
	v34hs_poke_short(T44T_REC + T44T_R_NBITS, len);
	v34hs_poke_short(T44T_REC + T44T_R_CRC, crc);
}

/*
 * A message of `len` bits whose CRC checks out, one bit short of complete.
 *
 * The counter is at len+15 and the register holds exactly what is about to
 * arrive, so the step takes the last bit, finds the counter at len+16 and
 * compares equal -- which is the accept path, and `len` is then what 0x6e534
 * dispatches on.  Both receivers are off, which is what makes the three
 * decoders below take their short exits and leaves the branch the test's.
 */
static void
accept_at(short len)
{
	bits(5, 0x1234, (short)(len + 0x0f), len, 0x1234);
	v34hs_poke_short(T44T_F359C, 0x0050);
	v34hs_poke_int(T44T_V90RECV, 0);
	v34hs_poke_int(T44T_K56RECV, 0);
}

/* A pointer field of side A's object, for the holes no byte compare sees. */
static const void *
peek_ptr_a(unsigned off)
{
	return *(const void *const *)((const char *)v34hs_object(0) + off);
}

/*
 * THE SESSION'S TWO CAPABILITY POINTERS, AIMED AT A BUFFER PER SIDE.
 *
 * One branch of the 0x26 arm needs `V34GiveINFO1aBits` to answer non-zero,
 * and the only way it can is the V.90 path -- which walks the session to two
 * blocks of capability bytes at +0x612c and +0x1760 and writes four of them.
 * `v34hs_setup` aims the object's thirty-five pointers and the session's one
 * pointer to the PCM receiver; it does NOT build the session's interior, so
 * those two fields hold the fill's pseudorandom address and the branch faults
 * rather than failing.
 *
 * Two buffers, one per side, aimed before the step -- and the four bytes of
 * each pointer put BACK before the comparison, because the two addresses
 * differ and the arena is compared byte for byte with only the session's PCM
 * pointer exempt.  The buffers are then compared to each other directly, so
 * nothing is given up: what the decoder wrote is still checked side against
 * side, and the arena's own claim is untouched.
 */
#define T44T_SESS	0x3548		/* the object's pointer to it       */
#define T44T_SESS_TYPE	0x611c
#define T44T_SESS_LAYOUT 0x6120
#define T44T_SESS_CAPS	0x612c
#define T44T_SESS_UP	0x1760
#define T44T_CFG_V92LITE 0x02

static unsigned char caps_buf[2][64];
static unsigned char caps_saved[2][2][4];

static unsigned char *
session_of(int side)
{
	return *(unsigned char *const *)
		((const char *)v34hs_object(side) + T44T_SESS);
}

static void
aim_session(void)
{
	int side, i;

	for (side = 0; side < 2; side++) {
		unsigned char *sess = session_of(side);
		void *buf = caps_buf[side];

		for (i = 0; i < 64; i++)
			caps_buf[side][i] = (unsigned char)(0x40 + i);
		memcpy(caps_saved[side][0], sess + T44T_SESS_CAPS, 4);
		memcpy(caps_saved[side][1], sess + T44T_SESS_UP, 4);
		memcpy(sess + T44T_SESS_CAPS, &buf, sizeof(buf));
		memcpy(sess + T44T_SESS_UP, &buf, sizeof(buf));
	}
}

static void
restore_session(long tag)
{
	int side;

	for (side = 0; side < 2; side++) {
		unsigned char *sess = session_of(side);

		memcpy(sess + T44T_SESS_CAPS, caps_saved[side][0], 4);
		memcpy(sess + T44T_SESS_UP, caps_saved[side][1], 4);
	}
	diff_eq_int("the session capability bytes, side against side",
		    memcmp(caps_buf[0], caps_buf[1], sizeof(caps_buf[0])), 0,
		    tag);
}

/*
 * A probe bank state which `dftenergy` can really produce.  The integer and
 * double accumulators retain the 1:64 relationship left by a product exactly
 * divisible by 64; the public reducer then derives every `denergy`, rather
 * than the test planting impossible fractional energies in its output field.
 */
static void
prepare_probe_energy(void)
{
	int side, i;

	for (side = 0; side < 2; side++) {
		struct v34_object *obj = (struct v34_object *)v34hs_object(side);

		for (i = 0; i < V34_PROBE_BINS; i++) {
			obj->probe_bins[i].acc_re = i + 1;
			obj->probe_bins[i].acc_im = 0;
			obj->probe_bins[i].sum_re = (double)(64 * (i + 1));
			obj->probe_bins[i].sum_im = 0.0;
			obj->probe_results[i] = 0.0;
		}
	}

	dftenergy(((struct v34_object *)v34hs_object(0))->probe_bins,
		  (short)V34_PROBE_BINS, 2);
	ref_dftenergy(((struct v34_object *)v34hs_object(1))->probe_bins,
		      (short)V34_PROBE_BINS, 2);
}

/*
 * Step, compare, and say what the step DID.
 *
 * The comparison is the differential check; the four `diff_eq_int`s are the
 * anti-vacuity half.  A case that took a different branch from the one it is
 * named for still compares -- both sides took it -- so without them a seed
 * that stopped reaching the branch would leave the test green and testing
 * nothing.
 */
static void
step(const char *what, long tag, unsigned changed, unsigned lines,
     short mst, short txst)
{
	const struct v34hs_obs *o;

	v34hs_step();
	v34hs_compare(what, tag);

	o = v34hs_observed(0);
	if (dump)
		printf("  %-34s changed %3u  lines %u  mst %2d tx %2d\n",
		       what, o->changed, o->lines, o->mst, o->txst);

	/*
	 * No conversion in these: `diff_eq_int` appends the input when the
	 * format has none (finding F220), and a `%s` would be handed a long.
	 */
	if (default_fill)
		diff_eq_int("object bytes the step wrote", o->changed, changed,
			    tag);
	diff_eq_int("diagnostic lines printed", o->lines, lines, tag);
	diff_eq_int("microstate afterwards", o->mst, mst, tag);
	diff_eq_int("txstate afterwards", o->txst, txst, tag);
}

/* What the restart leaves behind, checked once per sub-branch. */
static void
check_restart(long tag, short rec_len, short blk_len)
{
	const char *base = (const char *)v34hs_object(0);

	diff_eq_int("restart: the CRC register is reset",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC), -1, tag);
	diff_eq_int("restart: the message length",
		    v34hs_peek_short(0, T44T_REC + T44T_R_NBITS), rec_len, tag);
	diff_eq_int("restart: +0x358a", v34hs_peek_short(0, T44T_F358A), 1,
		    tag);
	diff_eq_int("restart: bit 2 of +0x3588",
		    v34hs_peek_short(0, T44T_F3588), 0x0a06, tag);
	diff_eq_int("restart: local_short cleared",
		    v34hs_peek_short(0, T44T_LOCALSHORT), 0, tag);
	diff_eq_int("restart: is_short cleared",
		    v34hs_peek_short(0, T44T_ISSHORT), 0, tag);
	diff_eq_int("restart: +0xabc2 cleared",
		    v34hs_peek_short(0, T44T_FABC2), 0, tag);
	/*
	 * TEN SHORTS FROM +0xabae, AND THE FIRST OF THEM IS INDEX ZERO.
	 * 0x6bf1f loads 1 into %eax for the +0x358a store and 0x6bf3e zeroes
	 * it again before the loop, so a reading that starts the loop at one
	 * is exactly the plausible mistake -- and it leaves +0xabae holding
	 * the seed, which this catches.  The far end is +0xabc0.
	 */
	diff_eq_int("restart: +0xabae, the first", v34hs_peek_short(0, 0xabae),
		    0, tag);
	diff_eq_int("restart: +0xabc0, the tenth", v34hs_peek_short(0, 0xabc0),
		    0, tag);

	/* The installed record. */
	diff_eq_int("restart: +0xaa6c aimed at +0xa94c",
		    (int)(*(char *const *)(base + T44T_PTR_AA6C) - base),
		    T44T_BLK, tag);
	diff_eq_int("restart: the installed record's length",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_NBITS), blk_len, tag);
	diff_eq_int("restart: installed +0x14",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_CRC), -1, tag);
	diff_eq_int("restart: installed +0x16",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F16), 1, tag);
	diff_eq_int("restart: installed +0x1a",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F1A), 0, tag);
	diff_eq_int("restart: installed +0x1c",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F1C), 8, tag);
	diff_eq_int("restart: installed +0x1e",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F1E), 0, tag);
	diff_eq_int("restart: installed +0x20",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F20), 1, tag);
	diff_eq_int("restart: installed +0x22",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F22), 0, tag);
	diff_eq_int("restart: installed +0x28",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F28), 0xc, tag);
	diff_eq_int("restart: installed +0x2a",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F2A), 0xc, tag);
	diff_eq_int("restart: installed +0x24 low",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F24), 0xf72, tag);
	diff_eq_int("restart: installed +0x24 high",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F24 + 2), 0, tag);
	diff_eq_int("restart: installed +0x2c low",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F2C), 0xf72, tag);
	diff_eq_int("restart: installed +0x2c high",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F2C + 2), 0, tag);
}

int
main(void)
{
	const char *base;

	dump = getenv("V34HS_DUMP") != NULL;
	default_fill = getenv("V34HS_SEED") == NULL;
	diff_begin("v34handshak: microstate 44, DET_INFO");

	v34hs_ours(1);
	v34hs_debug(1);

	/*
	 * WHERE +0xaa70 POINTS, asserted and not assumed.  Every offset this
	 * file seeds into the record is relative to +0xa97c, so a bring-up
	 * that aimed the field somewhere else would leave every seed landing
	 * in the wrong place and every case below passing for nothing.
	 */
	begin(V34HS_MOH_SILENCE);
	base = (const char *)v34hs_object(0);
	diff_eq_int("+0xaa70 points at the record at +0xa97c",
		    (int)(*(char *const *)(base + T44T_PTR_AA70) - base),
		    T44T_REC, 1);

	/* --- the outermost gate ------------------------------------------ */

	/*
	 * NO BIT WAITING.  `fsk.nbits` zero and the arm is over at 0x668da:
	 * it clears +0xaa7a and leaves through the transmit dispatch without
	 * touching the counter, the register or the byte array.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(0, 0x1234, 100, 200, 0x4321);
	step("no bit waiting", 100, 13, 0, V34HS_DET_INFO, V34HS_MOH_SILENCE);
	diff_eq_int("no bit: the counter did not move",
		    v34hs_peek_short(0, T44T_COUNT), 100, 100);
	diff_eq_int("no bit: the register did not move",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC), 0x4321, 100);
	diff_eq_int("no bit: +0xaa7a cleared",
		    v34hs_peek_short(0, T44T_FAA7A), 0, 100);

	/*
	 * ONE BIT WAITING, AND EXACTLY ONE IS TAKEN.  `nbits` goes 5 -> 4,
	 * not 5 -> 0: the arm consumes one bit per call however many are
	 * queued, which is what makes the counter and `nbits` two different
	 * quantities.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 0, 64, 0x0001);
	step("one bit taken", 101, 16, 0, V34HS_DET_INFO, V34HS_MOH_SILENCE);
	diff_eq_int("one bit: nbits decremented once",
		    v34hs_peek_short(0, T44T_NBITS), 4, 101);
	diff_eq_int("one bit: the counter stepped",
		    v34hs_peek_short(0, T44T_COUNT), 1, 101);

	/*
	 * AND THE GATE IS `== 0`, NOT `<= 0`.  0x668d7 is `test %ax,%ax; je`,
	 * so a negative `nbits` is not "nothing waiting": the arm takes its
	 * bit and steps the count on down.  Nothing sane produces one, which
	 * is exactly why the distinction needs a case of its own.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(-3, 0x1234, 0, 64, 0x0001);
	step("a negative nbits is not empty", 102, 16, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("nbits stepped on from -3",
		    v34hs_peek_short(0, T44T_NBITS), -4, 102);

	/* --- the CRC register: the feedback bit's whole truth table ------- */

	/*
	 * FOUR INDEPENDENT CASES, one per row.  The feedback is the register's
	 * bit 15 XOR the incoming bit, and 0x1021 is xored in only when it is
	 * one, so each row fails on its own for its own reason and none of
	 * them can be got right by getting another wrong.
	 *
	 *   bit15   sr&1   feedback   register after
	 *     0      0        0       0x0001 << 1            = 0x0002
	 *     1      0        1       0x8000 << 1 ^ 0x1021   = 0x1021
	 *     0      1        1       0x0001 << 1 ^ 0x1021   = 0x1023
	 *     1      1        0       0x8001 << 1            = 0x0002
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 0, 64, 0x0001);
	step("CRC: 0,0 -> no xor", 110, 16, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("CRC 0,0", v34hs_peek_short(0, T44T_REC + T44T_R_CRC),
		    0x0002, 110);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 0, 64, (short)0x8000);
	step("CRC: 1,0 -> xor", 111, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("CRC 1,0", v34hs_peek_short(0, T44T_REC + T44T_R_CRC),
		    0x1021, 111);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1235, 0, 64, 0x0001);
	step("CRC: 0,1 -> xor", 112, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("CRC 0,1", v34hs_peek_short(0, T44T_REC + T44T_R_CRC),
		    0x1023, 112);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1235, 0, 64, (short)0x8001);
	step("CRC: 1,1 -> no xor", 113, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("CRC 1,1", v34hs_peek_short(0, T44T_REC + T44T_R_CRC),
		    0x0002, 113);

	/*
	 * THE REGISTER STOPS AT THE MESSAGE LENGTH, and the boundary is `<`.
	 * Two cases one apart: at 63 of 64 the register steps, at 64 of 64 it
	 * does not, because the sixteen bits that follow the message ARE the
	 * CRC and are compared rather than accumulated.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 63, 64, 0x0001);
	step("CRC: one bit short of the length", 114, 18, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("the register stepped at 63 of 64",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC), 0x0002, 114);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 64, 64, 0x0001);
	step("CRC: at the length", 115, 15, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("the register held at 64 of 64",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC), 0x0001, 115);

	/* --- the byte store, and the three exits that skip it ------------- */

	/*
	 * EVERY EIGHTH BIT COMPLETES A BYTE.  The counter reaching 8 stores
	 * the low byte of `sr`, zero extended, into slot 0; a counter of 9
	 * stores nothing.  Two cases, so the mask is pinned rather than the
	 * store.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, 7, 200, 0x0001);
	step("byte 0 stored", 120, 19, 0, V34HS_DET_INFO, V34HS_MOH_SILENCE);
	diff_eq_int("the low byte of sr, widened",
		    v34hs_peek_short(0, T44T_REC + 0), 0x00ab, 120);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, 8, 200, 0x0001);
	step("no byte at nine bits", 121, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("slot 0 untouched at nine bits",
		    v34hs_peek_short(0, T44T_REC + 0), 0x0301, 121);

	/*
	 * AND THE MASK IS THREE BITS AND NOT TWO.  Every other case that
	 * skips the store skips it under `& 3` as well, so a two-bit mask
	 * reads exactly like a three-bit one everywhere above; a counter of 4
	 * is the shape that separates them, because it is a multiple of four
	 * and not of eight.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, 3, 200, 0x0001);
	step("four bits is not a byte", 126, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("nothing stored at four bits",
		    v34hs_peek_short(0, T44T_REC + 0), 0x0301, 126);

	/*
	 * THE TENTH BYTE AND THE ELEVENTH.  Slot 9 is the last: at 80 bits it
	 * is written and at 88 the arm leaves through 0x71857 with nothing
	 * stored.  The bound is `> 9` on the index, which is
	 * (counter >> 3) - 1, so these two also pin the -1.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, 79, 200, 0x0001);
	step("byte 9, the last slot", 122, 19, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("slot 9 written", v34hs_peek_short(0, T44T_REC + 18),
		    0x00ab, 122);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, 87, 200, 0x0001);
	step("one byte too many", 123, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("nothing past slot 9",
		    v34hs_peek_short(0, T44T_REC + 18),
		    (short)(0x0301 + 0x111 * 9), 123);

	/*
	 * A COUNTER THAT IS NOT POSITIVE takes 0x7186a instead, and the test
	 * is `<= 0` and not `== 0`: at -8 the eighth-bit mask is satisfied
	 * -- -8 & 7 is zero -- and the arm still stores nothing.  Two cases,
	 * because one of them alone cannot tell `jle` from `je`.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, -1, 200, 0x0001);
	step("the counter reaches zero", 124, 18, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("the counter is zero",
		    v34hs_peek_short(0, T44T_COUNT), 0, 124);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x12ab, -9, 200, 0x0001);
	step("a negative counter, byte aligned", 125, 17, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("still nothing stored",
		    v34hs_peek_short(0, T44T_REC + 0), 0x0301, 125);

	/* --- the restart, 0x6bda0 ---------------------------------------- */

	/*
	 * THE MESSAGE IS COMPLETE AND ITS CRC IS WRONG.  The threshold is the
	 * message length plus sixteen: with a length of 24 the counter
	 * reaching 40 ends the message, and the register (0x4321) against
	 * what arrived (0x1234) disagrees, so the exchange restarts.
	 *
	 * THE REGISTER MUST DISAGREE IN EVERY CASE HERE.  Agreement is
	 * 0x6e534, which nobody has written and which halts.
	 *
	 * 40 is also a multiple of eight, so the restart is followed by the
	 * byte store -- on the 0xffff the restart itself left in `sr`, which
	 * is how the two halves are shown to be one arm.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0050);
	step("restart, neither side", 130, 82, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(130, 24, 0x11);
	diff_eq_int("restart: sr reset",
		    v34hs_peek_short(0, T44T_SR), -1, 130);
	diff_eq_int("restart: nbits cleared",
		    v34hs_peek_short(0, T44T_NBITS), 0, 130);
	diff_eq_int("restart: byte 4 stored from the reset sr",
		    v34hs_peek_short(0, T44T_REC + 8), 0x00ff, 130);

	/*
	 * ONE SHORT OF THE THRESHOLD, so the restart does NOT run.  This is
	 * what makes the +0x10 a measurement rather than a transcription: at
	 * 39 of 24+16 the arm is an ordinary bit and the state words do not
	 * move.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 38, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0050);
	step("one short of the threshold", 131, 15, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("no restart: +0x358a untouched",
		    v34hs_peek_short(0, T44T_F358A), 0x7abc, 131);

	/*
	 * AND PAST IT, WHICH IS ALSO NO RESTART.  0x6694e is `cmp %ebp,%ebx;
	 * je`, so the threshold is an equality and not "at or beyond": a
	 * counter of 31 against a length of 8 is seven bits past the end of
	 * the message and the arm is still an ordinary bit.  Every case above
	 * reads the same under `>=` as under `==`; this one does not.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 30, 8, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0050);
	step("past the threshold", 137, 15, 0, V34HS_DET_INFO,
	     V34HS_MOH_SILENCE);
	diff_eq_int("past the threshold: +0x358a untouched",
		    v34hs_peek_short(0, T44T_F358A), 0x7abc, 137);

	/*
	 * THE ANSWERING SIDE, `role == 0x66`.  One extra store -- the
	 * record's length becomes 0x1e -- and one extra diagnostic line.
	 *
	 * AND NO V.90 RECEIVER, WHICH IS THE POINT OF THE CASE.  `v34info.c`'s
	 * `V34SetINFO0dBits` writes 30 into index 12 of the SAME record and
	 * `v34handshakinit` mode 0 calls it under the SAME `role == 0x66`
	 * test -- but it returns early when `v90_receiver` is zero, and this
	 * inlined copy at 0x718fc has no such guard: 0x71903's store sits
	 * before the diagnostic check and is reached unconditionally.  The two
	 * are not the same function and this case is what says so.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0066);
	v34hs_poke_int(T44T_V90RECV, 0);
	step("restart, answering side", 132, 83, 4, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(132, 0x1e, 0x11);

	/*
	 * THE ORIGINATING SIDE, `role == 0x65`, and the V.90 receiver
	 * decides ONE FIELD: the installed record's length is 0x1e when there
	 * is one and 0x11 when there is not.  Both are driven, so the
	 * condition is pinned from both sides.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0065);
	v34hs_poke_int(T44T_V90RECV, 3);
	step("restart, originating with V.90", 133, 82, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(133, 24, 0x1e);

	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0065);
	v34hs_poke_int(T44T_V90RECV, 0);
	step("restart, originating without V.90", 134, 82, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(134, 24, 0x11);

	/*
	 * AND `role == 0x66` DOES NOT REACH THE SECOND BRANCH.  The two
	 * tests are 0x66 for the record's length and 0x65 for the installed
	 * one, and a reconstruction that used one value for both would pass
	 * every case above; this is the case that separates them, because it
	 * has a V.90 receiver AND the answering side's value.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0066);
	v34hs_poke_int(T44T_V90RECV, 3);
	step("restart, answering side with V.90", 135, 83, 4, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(135, 0x1e, 0x11);

	/*
	 * A RESTART THAT IS NOT BYTE ALIGNED.  A length of 17 puts the
	 * threshold at 33, so the restart runs and the arm then leaves
	 * through 0x6bd8d with no byte stored.  Same restart, different tail.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 32, 17, 0x4321);
	v34hs_poke_short(T44T_F359C, 0x0050);
	step("restart, not byte aligned", 136, 80, 3, V34HS_DET_SYNC,
	     V34HS_TX_DPSK);
	check_restart(136, 17, 0x11);
	diff_eq_int("no byte stored on a 33-bit restart",
		    v34hs_peek_short(0, T44T_REC + 0), 0x0301, 136);

	/* --- the accept path's default arm, 0x6e552 ----------------------- */

	/*
	 * THE MESSAGE IS COMPLETE AND ITS CRC CHECKS OUT.  Same threshold as
	 * the restart cases -- a length of 24 and a counter reaching 40 --
	 * with the register and the sixteen bits that arrived made EQUAL
	 * instead of different.  0x6e534 then dispatches on the length, and
	 * 24 is none of the three that have bodies of their own, so this is
	 * the default arm at 0x6e552.
	 *
	 * `v90_receiver` is zero throughout except where a case says
	 * otherwise: `V34GiveINFO0dBits` returns before it prints or decides
	 * anything when it is (v34info.c), which leaves `is_short` the test's
	 * to set and the branch it picks the test's to name.
	 *
	 * +0x358a IS NOT 1 HERE, so the byte copy is skipped entirely and the
	 * arm goes straight to 0x6e745.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x1234);
	v34hs_poke_short(T44T_F359C, 0x0050);
	v34hs_poke_int(T44T_V90RECV, 0);
	v34hs_poke_short(T44T_ISSHORT, 0);
	step("accept, no byte copy", 160, 19, 3, V34HS_TX_PHASE1_ANS,
	     V34HS_MOH_SILENCE);
	diff_eq_int("accept: the counter is reset",
		    v34hs_peek_short(0, T44T_COUNT), 0, 160);
	diff_eq_int("accept: sr is reset on the answering side",
		    v34hs_peek_short(0, T44T_SR), -1, 160);
	diff_eq_int("accept: the detector is disarmed",
		    v34hs_peek_short(0, T44T_RX_FLAGS) & 0x200, 0, 160);
	/*
	 * AND THE BYTE CLOCK'S TAIL RUNS ON A COUNTER OF ZERO.  0x6695d
	 * re-reads +0xaa78 rather than using the value 0x6693d stored, so
	 * resetting it at 0x6e750 makes the whole tail a no-op: nothing is
	 * stored, and slot 4 keeps what the fixture put there.  A
	 * reconstruction that carried the stepped count in a local would
	 * write 0x00ff here, and the blob does not.
	 */
	diff_eq_int("accept: a reset counter stores nothing",
		    v34hs_peek_short(0, T44T_REC + 8),
		    (short)(0x0301 + 0x111 * 4), 160);

	/*
	 * SHORT PHASE 2 TAKES A DIFFERENT STATE AND RELOADS THE COUNTER.
	 * 0x6e8ac is reached by falling out of the `is_short` branch and is
	 * skipped by both others, so this is the only case in which +0xaa7c
	 * reaches +0xaa78.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x1234);
	v34hs_poke_short(T44T_F359C, 0x0050);
	v34hs_poke_int(T44T_V90RECV, 0);
	v34hs_poke_short(T44T_ISSHORT, 1);
	v34hs_poke_short(T44T_COUNT_SRC, 40);
	step("accept, short phase 2", 161, 21, 3, V34HS_RX_PHASE1_ANS,
	     V34HS_MOH_SILENCE);
	diff_eq_int("accept: the counter came from +0xaa7c",
		    v34hs_peek_short(0, T44T_COUNT), 40, 161);
	/*
	 * +0xaa7c IS 40 HERE ON PURPOSE.  It is a positive multiple of eight,
	 * so the re-read tail finds a counter that DOES complete a byte, and
	 * slot 4 is written from the reset `sr`.  That is the one case in
	 * which the accept path is shown to rejoin the byte clock at all
	 * rather than merely to reach its first guard and stop.
	 */
	diff_eq_int("accept: the reload feeds the byte clock",
		    v34hs_peek_short(0, T44T_REC + 8), 0x00ff, 161);

	/*
	 * THE ORIGINATING SIDE SKIPS TWO STORES.  `role == 0x65` goes to its
	 * own state and reaches neither 0x6e814's `sr` reset nor 0x6e8ac's
	 * counter reload -- so the byte the clock then stores comes from the
	 * sr the message arrived in, 0x1234, and not from 0xffff.  That one
	 * byte is what separates this branch from the two above.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x1234);
	v34hs_poke_short(T44T_F359C, 0x0065);
	v34hs_poke_int(T44T_V90RECV, 0);
	v34hs_poke_short(T44T_ISSHORT, 1);
	v34hs_poke_short(T44T_COUNT_SRC, 40);
	step("accept, originating side", 162, 17, 3, V34HS_RX_PHASE1_CALL,
	     V34HS_MOH_SILENCE);
	diff_eq_int("accept: sr is NOT reset on the originating side",
		    v34hs_peek_short(0, T44T_SR), 0x1234, 162);
	/*
	 * SAME +0xaa7c AS THE CASE ABOVE, and it does not arrive: 0x6e8ac is
	 * reached only by falling out of the short-phase-2 branch, so the
	 * counter stays at the zero 0x6e750 left and the byte clock stores
	 * nothing.  The two cases differ in `role` alone.
	 */
	diff_eq_int("accept: and the counter is NOT reloaded",
		    v34hs_peek_short(0, T44T_COUNT), 0, 162);
	diff_eq_int("accept: so no byte is stored either",
		    v34hs_peek_short(0, T44T_REC + 8),
		    (short)(0x0301 + 0x111 * 4), 162);

	/*
	 * +0x358a == 1 OPENS THE BYTE COPY.  The bits received are moved from
	 * the record into the object's own array at +0xabae -- the same array
	 * the restart clears -- and +0xabc2 gets how many BYTES that was.
	 *
	 * THE COUNT IT DIVIDES IS THE STEPPED ONE.  0x6e58d reads +0xaa78
	 * after 0x6693d wrote it, so a message of 26 bits is counted at 42 and
	 * not 41 -- five whole bytes and a remainder, so six.  A length of 24
	 * would put it at 40, an exact multiple of eight, and the rounding-up
	 * term could then be dropped without any case noticing.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 41, 26, 0x1234);
	v34hs_poke_short(T44T_F359C, 0x0050);
	v34hs_poke_int(T44T_V90RECV, 0);
	v34hs_poke_short(T44T_F358A, 1);
	v34hs_poke_self_ptr(T44T_PTR_AA6C, T44T_BLK);
	v34hs_poke_short(T44T_BLK + 0x04, 0x1200);
	step("accept, byte copy and out", 163, 36, 1, V34HS_DET_SYNC,
	     V34HS_MOH_SILENCE);
	diff_eq_int("accept: six bytes arrived",
		    v34hs_peek_short(0, T44T_FABC2), 6, 163);
	diff_eq_int("accept: slot 0 copied", v34hs_peek_short(0, 0xabae),
		    0x0301, 163);
	diff_eq_int("accept: slot 5 copied", v34hs_peek_short(0, 0xabb8),
		    (short)(0x0301 + 0x111 * 5), 163);
	diff_eq_int("accept: slot 6 NOT copied", v34hs_peek_short(0, 0xabba),
		    (short)(0x2001 + 7 * 6), 163);
	diff_eq_int("accept: the other record's +0x04 gains bit 7",
		    v34hs_peek_short(0, T44T_BLK + 0x04), 0x1280, 163);
	diff_eq_int("accept: and its +0x22 is cleared",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F22), 0, 163);
	diff_eq_int("accept: the register is reset",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC), -1, 163);
	diff_eq_int("accept: and it did NOT rejoin the byte clock",
		    v34hs_peek_short(0, T44T_REC + 8),
		    (short)(0x0301 + 0x111 * 4), 163);

	/*
	 * BIT 7 OF THE OTHER RECORD'S +0x04 ALREADY SET, so neither the
	 * clear of its +0x22 nor the or happens.  Same copy, same exit.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x1234);
	v34hs_poke_short(T44T_F359C, 0x0050);
	v34hs_poke_int(T44T_V90RECV, 0);
	v34hs_poke_short(T44T_F358A, 1);
	v34hs_poke_self_ptr(T44T_PTR_AA6C, T44T_BLK);
	v34hs_poke_short(T44T_BLK + 0x04, 0x1280);
	step("accept, the flag already set", 164, 31, 1, V34HS_DET_SYNC,
	     V34HS_MOH_SILENCE);
	diff_eq_int("accept: +0x22 survives when the flag was set",
		    v34hs_peek_short(0, T44T_BLK + T44T_R_F22), 0x1a2b, 164);

	/*
	 * AND THE RECORD'S OWN SLOT 2 WITH BIT 7 SET goes on to 0x6e745
	 * instead, so the copy runs AND the handshake moves state.  That is
	 * the third exit of this arm and the one that reaches both halves.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x1234);
	v34hs_poke_short(T44T_F359C, 0x0050);
	v34hs_poke_int(T44T_V90RECV, 0);
	v34hs_poke_short(T44T_ISSHORT, 0);
	v34hs_poke_short(T44T_F358A, 1);
	v34hs_poke_self_ptr(T44T_PTR_AA6C, T44T_BLK);
	v34hs_poke_short(T44T_BLK + 0x04, 0x1280);
	v34hs_poke_short(T44T_REC + 4, 0x05a3);
	step("accept, copy then move on", 165, 30, 3, V34HS_TX_PHASE1_ANS,
	     V34HS_MOH_SILENCE);
	diff_eq_int("accept: five bytes arrived here too",
		    v34hs_peek_short(0, T44T_FABC2), 5, 165);
	diff_eq_int("accept: the copy ran and the counter is still reset",
		    v34hs_peek_short(0, T44T_COUNT), 0, 165);

	/*
	 * WITH A V.90 RECEIVER, so `V34GiveINFO0dBits` really decodes rather
	 * than returning at its first line.  `is_short` is then ITS output
	 * and not the test's, which is the arrangement the object runs in;
	 * the case is here so that the call is driven through rather than
	 * around, and the branch it picks is read back rather than asserted.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 39, 24, 0x1234);
	v34hs_poke_short(T44T_F359C, 0x0050);
	v34hs_poke_int(T44T_V90RECV, 2);
	v34hs_poke_short(T44T_ISSHORT, 0x5e5e);
	v34hs_step();
	v34hs_compare("accept, with the INFO0 decoder", 166);
	diff_eq_int("accept: the decoder wrote is_short",
		    v34hs_peek_short(0, T44T_ISSHORT) == 0x5e5e, 0, 166);
	diff_eq_int("accept: and the state followed it",
		    v34hs_observed(0)->mst,
		    v34hs_peek_short(0, T44T_ISSHORT) != 0
			? V34HS_RX_PHASE1_ANS : V34HS_TX_PHASE1_ANS, 166);

	/*
	 * THE BYTE COPY HAS NO BOUND, AND THIS IS WHAT THAT COSTS.
	 *
	 * `nbytes` is the stepped counter divided by eight and rounded up,
	 * and the loop at 0x6e5c8 runs that many times into an array of TEN
	 * shorts.  A message of 110 bits puts the counter at 126 and `nbytes`
	 * at 16, so six iterations run off the end -- and the object does
	 * them.  Where they land is not padding:
	 *
	 *     index 10  ->  +0xabc2, WHICH IS `nbytes` ITSELF, four
	 *                   instructions after it was stored there
	 *     index 14  ->  +0xabca, `local_short`
	 *     index 15  ->  +0xabcc, `is_short` -- WHICH THE ARM THEN READS
	 *                   at 0x6e824 to choose the next microstate
	 *
	 * and what they copy is the record's own tail: index 10 reads its CRC
	 * register at +0x14, index 12 its length at +0x18, index 15 its +0x1e.
	 * So a 110-bit message ends by choosing its successor state out of a
	 * field of the message record that has nothing to do with the
	 * question, and +0xabc2 does not hold the byte count it was just
	 * given.  Both are asserted below.
	 *
	 * `v90_receiver` is zero, so `V34GiveINFO0dBits` returns before it
	 * writes `is_short` and the value the branch reads is the one the
	 * overrun left.  +0x1e is 3, and 3 is not a length, a count or a
	 * flag -- it is whatever the copy found.
	 *
	 * REPRODUCED AND NOT REPAIRED.  The comparison is byte for byte
	 * against the blob and it passes, which is the whole claim; the loop
	 * is the default arm's and belongs to findings F400-406's commit, so
	 * this case adds the measurement without touching the code.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 125, 110, 0x1234);
	v34hs_poke_short(T44T_F359C, 0x0050);
	v34hs_poke_int(T44T_V90RECV, 0);
	v34hs_poke_short(T44T_F358A, 1);
	v34hs_poke_self_ptr(T44T_PTR_AA6C, T44T_BLK);
	v34hs_poke_short(T44T_REC + 4, 0x1280);		/* bit 7 set: 0x6e745 */
	v34hs_poke_short(T44T_REC + T44T_R_F16, 0x0a0b);
	v34hs_poke_short(T44T_REC + T44T_R_F1A, 0x0c0d);
	v34hs_poke_short(T44T_REC + T44T_R_F1C, 0x0e0f);
	v34hs_poke_short(T44T_REC + T44T_R_F1E, 3);
	v34hs_poke_short(T44T_COUNT_SRC, 0);
	v34hs_poke_short(0xabc4, 0x6161);
	v34hs_poke_short(0xabc6, 0x6262);
	v34hs_poke_short(0xabc8, 0x6363);
	v34hs_step();
	v34hs_compare("accept, the copy runs past ten slots", 185);
	diff_eq_int("overrun: +0xabc2 holds the record's CRC, not the count",
		    v34hs_peek_short(0, T44T_FABC2), 0x1234, 185);
	diff_eq_int("overrun: +0xabc4 <- the record's +0x16",
		    v34hs_peek_short(0, 0xabc4), 0x0a0b, 185);
	diff_eq_int("overrun: +0xabc6 <- the record's LENGTH",
		    v34hs_peek_short(0, 0xabc6), 110, 185);
	diff_eq_int("overrun: +0xabc8 <- the record's +0x1a",
		    v34hs_peek_short(0, 0xabc8), 0x0c0d, 185);
	diff_eq_int("overrun: local_short <- the record's +0x1c",
		    v34hs_peek_short(0, T44T_LOCALSHORT), 0x0e0f, 185);
	diff_eq_int("overrun: is_short <- the record's +0x1e",
		    v34hs_peek_short(0, T44T_ISSHORT), 3, 185);
	diff_eq_int("overrun: and the arm believed it",
		    v34hs_observed(0)->mst, V34HS_RX_PHASE1_ANS, 185);

	/*
	 * ONE BYTE SHORT OF THE OVERRUN, so the same arm with `nbytes` at ten
	 * leaves +0xabc2 holding the count and `is_short` holding its seed --
	 * which is what makes the case above a measurement of the loop bound
	 * rather than of the arm.  A message of 64 bits puts the counter at
	 * 80 and `nbytes` at exactly 10.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(5, 0x1234, 79, 64, 0x1234);
	v34hs_poke_short(T44T_F359C, 0x0050);
	v34hs_poke_int(T44T_V90RECV, 0);
	v34hs_poke_short(T44T_F358A, 1);
	v34hs_poke_self_ptr(T44T_PTR_AA6C, T44T_BLK);
	v34hs_poke_short(T44T_REC + 4, 0x1280);
	v34hs_poke_short(T44T_ISSHORT, 0);
	v34hs_poke_short(T44T_COUNT_SRC, 0);
	v34hs_step();
	v34hs_compare("accept, ten slots exactly", 186);
	diff_eq_int("ten slots: +0xabc2 still holds the count",
		    v34hs_peek_short(0, T44T_FABC2), 10, 186);
	diff_eq_int("ten slots: local_short untouched",
		    v34hs_peek_short(0, T44T_LOCALSHORT), 0x4d4d, 186);
	diff_eq_int("ten slots: is_short untouched, so TX_PHASE1_ANS",
		    v34hs_observed(0)->mst, V34HS_TX_PHASE1_ANS, 186);

	/*
	 * THE THREE LENGTHS THAT HAVE BODIES OF THEIR OWN, driven from ONE
	 * AWAY.  Each of 0x4d, 0x26 and 0x08 now has an arm below, and these
	 * three cases are the other half of each constant: at 0x4e, 0x27 and
	 * 0x09 the dispatch must NOT claim the message, so a constant moved
	 * by one is caught HERE by taking a body where the default belongs
	 * and there by taking the default where a body belongs.  Two cases
	 * per constant and neither can be dropped.
	 *
	 * (This used to rest on the unwritten-path stop aborting -- finding
	 * F358's abort-as-a-catch.  With the bodies written it rests on the
	 * bodies disagreeing with the default instead, which is a byte
	 * difference rather than a dead run.)
	 *
	 * +0x358a is not 1, so the byte copy is out of the way and the case is
	 * about the dispatch alone.
	 */
	{
		static const short near_miss[3] = { 0x4e, 0x27, 0x09 };
		static const unsigned wrote[3] = { 19, 19, 19 };
		int k;

		for (k = 0; k < 3; k++) {
			char what[64];

			snprintf(what, sizeof(what),
				 "accept, length %d", near_miss[k]);
			begin(V34HS_MOH_SILENCE);
			bits(5, 0x1234, (short)(near_miss[k] + 0x0f),
			     near_miss[k], 0x1234);
			v34hs_poke_short(T44T_F359C, 0x0050);
			v34hs_poke_int(T44T_V90RECV, 0);
			v34hs_poke_short(T44T_ISSHORT, 0);
			step(what, 170 + k, wrote[k], 3, V34HS_TX_PHASE1_ANS,
			     V34HS_MOH_SILENCE);
		}
	}

	/* --- 0x08 bits: Modem-on-Hold, 0x6ea38 ---------------------------- */

	/*
	 * ONE BYTE OF MOH MESSAGE.  The arm hands the record to
	 * `VPcmV34InterpretMohMessageBits` and then asks +0xabf0 what that
	 * made of it; 0x57 is MHack with a timeout code of 7, which the
	 * decoder turns into `moh_recvd` 4 and +0xabe0 7 -- so a step that
	 * skipped the call leaves both seeds in place and is caught.
	 *
	 * +0xabf0 IS NOT 1 HERE, which is the exit that goes back to hunting
	 * for the synchronisation pattern.  The arm does not touch +0xaa78,
	 * so the byte clock it returns into finds the counter at 24 and
	 * stores a byte -- from the 0xffff this arm has just put in `sr`.
	 */
	begin(V34HS_MOH_SILENCE);
	accept_at(8);
	v34hs_poke_short(T44T_REC + 0, 0x0057);
	v34hs_poke_int(T44T_FABF0, 0x7777);
	v34hs_poke_int(T44T_FABF4, 0x2222);
	v34hs_poke_short(T44T_FABE0, 0x3333);
	v34hs_poke_short(T44T_FABF8, 0x5a5a);
	step("accept, 8 bits, resynchronise", 190, 27, 3, V34HS_DET_SYNC,
	     V34HS_MOH_SILENCE);
	diff_eq_int("8 bits: the MOH decoder ran",
		    v34hs_peek_short(0, T44T_FABF4), 4, 190);
	diff_eq_int("8 bits: on the record at +0xaa70",
		    v34hs_peek_short(0, T44T_FABE0), 7, 190);
	/*
	 * A BYTE AND NOT A HALFWORD.  0x6ea9b is `movb $0x1,0xabf8`, so the
	 * neighbour at +0xabf9 keeps its seed; a halfword store of 1 would
	 * clear it and this reads both at once.
	 */
	diff_eq_int("8 bits: +0xabf8 raised, one byte wide",
		    (unsigned short)v34hs_peek_short(0, T44T_FABF8), 0x5a01,
		    190);
	diff_eq_int("8 bits: the register is reset",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC), -1, 190);
	diff_eq_int("8 bits: and `sr` with it",
		    v34hs_peek_short(0, T44T_SR), -1, 190);
	diff_eq_int("8 bits: the counter is NOT reset",
		    v34hs_peek_short(0, T44T_COUNT), 24, 190);
	diff_eq_int("8 bits: so the byte clock stored slot 2 from the reset sr",
		    v34hs_peek_short(0, T44T_REC + 4), 0x00ff, 190);

	/*
	 * +0xabf0 == 1 IS THE OTHER EXIT: the tone detector is armed and the
	 * microstate goes to MOH_TONE_DROP.  Nothing resets `sr` on this
	 * path, so the byte the clock then stores is the low byte of the
	 * message's own shift register -- which is what separates the two
	 * exits by a byte as well as by a state.
	 *
	 * ORIGINATING, so the detector takes 2400 Hz's coefficients.  +0x3564
	 * is one of the fixture's thirty-five skipped pointers and holds two
	 * addresses of two copies, so the comparison cannot see it; this
	 * names the table on side A explicitly.
	 */
	begin(V34HS_MOH_SILENCE);
	accept_at(8);
	v34hs_poke_short(T44T_F359C, 0x0065);
	v34hs_poke_int(T44T_FABF0, 1);
	v34hs_poke_short(T44T_F356A, 0x3c3c);
	step("accept, 8 bits, tone, originating", 191, 28, 6,
	     V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	diff_eq_int("8 bits: +0x356a set to 1",
		    v34hs_peek_short(0, T44T_F356A), 1, 191);
	diff_eq_int("8 bits: 2400 Hz's coefficients when this end originated",
		    peek_ptr_a(T44T_DETCOEF) == (const void *)c2400_, 1, 191);
	diff_eq_int("8 bits: `sr` is NOT reset on this exit",
		    v34hs_peek_short(0, T44T_SR), 0x1234, 191);
	diff_eq_int("8 bits: so the byte clock stored the message's own byte",
		    v34hs_peek_short(0, T44T_REC + 4), 0x0034, 191);

	/* ANSWERING, so 1200 Hz's.  The one field that differs is the table. */
	begin(V34HS_MOH_SILENCE);
	accept_at(8);
	v34hs_poke_short(T44T_F359C, 0x0066);
	v34hs_poke_int(T44T_FABF0, 1);
	v34hs_poke_short(T44T_F356A, 0x3c3c);
	step("accept, 8 bits, tone, answering", 192, 28, 6,
	     V34HS_MOH_TONE_DROP, V34HS_MOH_SILENCE);
	diff_eq_int("8 bits: 1200 Hz's coefficients when this end answered",
		    peek_ptr_a(T44T_DETCOEF) == (const void *)c1200_, 1, 192);

	/*
	 * +0xabf0 IS AN INT.  0x6ea80 is `cmpl $0x1`, so 0x10001 is NOT one
	 * and the arm resynchronises; a halfword read would see a 1 and arm
	 * the tone detector instead.  The two exits differ in the microstate,
	 * in +0x356a and in the byte the clock then stores, so one seed
	 * separates a width from a value.
	 */
	begin(V34HS_MOH_SILENCE);
	accept_at(8);
	v34hs_poke_int(T44T_FABF0, 0x10001);
	v34hs_poke_short(T44T_FABF8, 0x5a5a);
	step("accept, 8 bits, +0xabf0 is 0x10001", 194, 27, 6, V34HS_DET_SYNC,
	     V34HS_MOH_SILENCE);
	diff_eq_int("8 bits: 0x10001 is not one",
		    (unsigned short)v34hs_peek_short(0, T44T_FABF8), 0x5a01,
		    194);

	/*
	 * AND ONE GUARD IN THIS ARM CANNOT BE DRIVEN FROM BOTH SIDES, said
	 * here rather than left to look like a gap.  Every microstate
	 * transition in all three arms is reached with +0x3592 still holding
	 * 44 -- that is what selected the arm -- so `hs_setstate`'s
	 * already-there exit is unreachable for DET_SYNC, MOH_TONE_DROP and
	 * INFODONE alike, and so is the rxstate one for RX_DPSK in the first
	 * exit above, which is a no-op on every path a test can drive.  The
	 * transmit state is the exception and case 231 below drives it.
	 */

	/* --- 0x26 bits: INFO1a, 0x6ed17 ----------------------------------- */

	/*
	 * THE DECODER'S ANSWER DECIDES EVERYTHING.  `V34GiveINFO1aBits`
	 * returns the session's UINFO6 word, which it sets to one only when
	 * there is a V.90 receiver AND the upstream baud index decodes to 6;
	 * a non-zero answer ends the phase -- WAIT, INFODONE and nothing else
	 * -- and a zero one configures the whole receiver.
	 *
	 * Index 2 bit 0 set and bit 1 clear, index 3 bit 7 set: 2 + 4 is the
	 * six the decoder is looking for.
	 */
	begin(V34HS_MOH_SILENCE);
	accept_at(0x26);
	v34hs_poke_int(T44T_V90RECV, 2);
	v34hs_poke_short(T44T_REC + 4, 0x0001);
	v34hs_poke_short(T44T_REC + 6, 0x0080);
	aim_session();
	v34hs_step();
	restore_session(195);
	v34hs_compare("accept, 0x26 bits, the phase ends", 195);
	diff_eq_int("0x26 bits: the microstate is INFODONE",
		    v34hs_observed(0)->mst, V34HS_INFODONE, 195);
	diff_eq_int("0x26 bits: and the counter is untouched",
		    v34hs_peek_short(0, T44T_COUNT), 0x36, 195);

	/*
	 * AND THE ZERO ANSWER IS THE LONG PATH.  No V.90 and no K56flex
	 * receiver, so the decoder leaves before it decides anything and the
	 * arm goes on to `setfinalrate`, `rxinit` and the two rate tables.
	 *
	 * THE RATE CODE IS DELIBERATELY ONE `setfinalrate` DOES NOT KNOW.
	 * +0xa9e0 bit 1 set with +0xa9e2 bit 7 clear makes its receive code 1,
	 * which has no arm, so it leaves +0xaa96 and +0xaaa8 exactly as this
	 * test poked them -- which is what lets the sweep below drive all six
	 * baud rates and all eight carriers from the test rather than from a
	 * message this arm would then also have to decode.  Two of the six
	 * baud rates `setfinalrate` cannot produce at all.
	 *
	 * The descriptor's own two halfwords are 7 and 8, so the seven bit
	 * length is 0x61 and its reversal is 0x43 -- asymmetric on purpose,
	 * because a palindrome would let the reversal be dropped, and with
	 * bit 1 of the first halfword set so that the two-bit mask is one bit
	 * wider than a mask that would also pass.  Bit 2 is set as well and
	 * cannot be seen: it lands at bit 7 of the assembled value, which is
	 * outside the seven `bitreverse` reads.
	 */
	begin(V34HS_MOH_SILENCE);
	accept_at(0x26);
	v34hs_poke_short(T44T_REC_A9DC + 0, 7);
	v34hs_poke_short(T44T_REC_A9DC + 2, 8);
	v34hs_poke_short(T44T_REC_A9DC + 4, 2);
	v34hs_poke_short(T44T_REC_A9DC + 6, 0);
	v34hs_poke_short(T44T_RXBAUD, 0x960);
	v34hs_poke_short(T44T_RXCARRIER, 0x640);
	v34hs_poke_short(T44T_F35A2, 0x1e1e);
	v34hs_poke_short(T44T_FAA80, 0x2e2e);
	v34hs_poke_short(T44T_RX_F128, 0x3333);
	v34hs_poke_short(T44T_RX_FLAGS2, 0x0f55);
	v34hs_step();
	v34hs_compare("accept, 0x26 bits, the receiver configured", 196);
	diff_eq_int("0x26 bits: the message-descriptor length, reversed",
		    v34hs_peek_short(0, T44T_F35A2), 0x43, 196);
	diff_eq_int("0x26 bits: the rxstate is RECEIVE",
		    v34hs_observed(0)->rxst, V34HS_RECEIVE, 196);
	diff_eq_int("0x26 bits: +0x128 is 4",
		    v34hs_peek_short(0, T44T_RX_F128), 4, 196);
	diff_eq_int("0x26 bits: the AGC step",
		    v34hs_peek_short(0, T44T_RX_STEP), 0x2000, 196);
	diff_eq_int("0x26 bits: the gain came from +0x262",
		    v34hs_peek_short(0, T44T_RX_GAIN),
		    v34hs_peek_short(0, T44T_RX_F262), 196);
	/*
	 * THE TOP NIBBLE OF THE FLAG WORD IS CLEARED AND THEN DET_PENDING IS
	 * PUT BACK.  Seeded with all four bits set and the low byte at 0x55,
	 * so a mask one bit wide either way shows.
	 */
	diff_eq_int("0x26 bits: the flag word",
		    (unsigned short)v34hs_peek_short(0, T44T_RX_FLAGS2),
		    0x0255, 196);
	diff_eq_int("0x26 bits: the counter is reset",
		    v34hs_peek_short(0, T44T_COUNT), 0, 196);
	diff_eq_int("0x26 bits: +0xaa80 set to 1",
		    v34hs_peek_short(0, T44T_FAA80), 1, 196);

	/*
	 * THE TWO RATE TABLES, every entry and both defaults.
	 *
	 * Six baud rates against four constants each and eight carriers
	 * against a table and a count -- and the counts are half the table
	 * lengths `include/dsplib/v34filt.h` declares, which is eight
	 * agreements this file can assert directly rather than eight
	 * constants taken on trust.  A baud rate or a carrier that is in
	 * neither table leaves its fields alone, and both of those are driven
	 * too.
	 *
	 * +0x1b4 is a skipped pointer -- two addresses of two copies -- so
	 * the table is named on side A rather than compared.
	 */
	{
		static const struct {
			short baud;
			short phase_wrap, phase_inc, symbol_period, phase_frac;
		} rates[] = {
			{ 0x960, 0x3e80, 0x3e80, 0x3e80, 0x1f40 },
			{ 0xab7, 0x3e80, 0x36b0, 0x36b0, 0x1f40 },
			{ 0xaf0, 0x3e82, 0x3594, 0x3594, 0x1f41 },
			{ 0xbb8, 0x3e80, 0x3200, 0x3200, 0x1f40 },
			{ 0xc80, 0x3e80, 0x2ee0, 0x2ee0, 0x1f40 },
			{ 0xd65, 0x3e80, 0x2bc0, 0x2bc0, 0x1f40 },
			{ 0x0961, 0x4141, 0x4242, 0x4343, 0x4444 }
		};
		static const struct {
			short carrier;
			const short *tbl;
			short half_len;
			int taps;
		} carriers[] = {
			{ 0x640, hsine1600,  6, 12 },
			{ 0x690, hsine1680, 40, 80 },
			{ 0x708, hsine1800, 16, 32 },
			{ 0x725, hsine1829, 21, 42 },
			{ 0x74b, hsine1867, 36, 72 },
			{ 0x780, hsine1920,  5, 10 },
			{ 0x7a7, hsine1959, 49, 98 },
			{ 0x7d0, hsine2000, 24, 48 },
			{ 0x641, NULL,	   0x5555, 0 }
		};
		unsigned k;
		char what[80];

		for (k = 0; k < sizeof(rates) / sizeof(rates[0]); k++) {
			begin(V34HS_MOH_SILENCE);
			accept_at(0x26);
			v34hs_poke_short(T44T_REC_A9DC + 0, 7);
			v34hs_poke_short(T44T_REC_A9DC + 2, 8);
			v34hs_poke_short(T44T_REC_A9DC + 4, 2);
			v34hs_poke_short(T44T_REC_A9DC + 6, 0);
			v34hs_poke_short(T44T_RXBAUD, rates[k].baud);
			v34hs_poke_short(T44T_RXCARRIER, 0x641);
			v34hs_poke_short(T44T_RX_F1B0, 0x4141);
			v34hs_poke_short(T44T_RX_F1AE, 0x4242);
			v34hs_poke_short(T44T_RX_F1BE, 0x4343);
			v34hs_poke_short(T44T_RX_F1AC, 0x4444);
			snprintf(what, sizeof(what),
				 "accept, 0x26 bits, baud %d", rates[k].baud);
			v34hs_step();
			v34hs_compare(what, 200 + (long)k);
			diff_eq_int("0x26 bits: +0x1b0",
				    v34hs_peek_short(0, T44T_RX_F1B0),
				    rates[k].phase_wrap, 200 + (long)k);
			diff_eq_int("0x26 bits: +0x1ae",
				    v34hs_peek_short(0, T44T_RX_F1AE),
				    rates[k].phase_inc, 200 + (long)k);
			diff_eq_int("0x26 bits: +0x1be",
				    v34hs_peek_short(0, T44T_RX_F1BE),
				    rates[k].symbol_period, 200 + (long)k);
			diff_eq_int("0x26 bits: +0x1ac",
				    v34hs_peek_short(0, T44T_RX_F1AC),
				    rates[k].phase_frac, 200 + (long)k);
		}

		for (k = 0; k < sizeof(carriers) / sizeof(carriers[0]); k++) {
			begin(V34HS_MOH_SILENCE);
			accept_at(0x26);
			v34hs_poke_short(T44T_REC_A9DC + 0, 7);
			v34hs_poke_short(T44T_REC_A9DC + 2, 8);
			v34hs_poke_short(T44T_REC_A9DC + 4, 2);
			v34hs_poke_short(T44T_REC_A9DC + 6, 0);
			v34hs_poke_short(T44T_RXBAUD, 0x0961);
			v34hs_poke_short(T44T_RXCARRIER, carriers[k].carrier);
			v34hs_poke_short(T44T_RX_F1BA, 0x5555);
			snprintf(what, sizeof(what),
				 "accept, 0x26 bits, carrier %d",
				 carriers[k].carrier);
			v34hs_step();
			v34hs_compare(what, 210 + (long)k);
			diff_eq_int("0x26 bits: the half-sine count",
				    v34hs_peek_short(0, T44T_RX_F1BA),
				    carriers[k].half_len, 210 + (long)k);
			if (carriers[k].tbl != NULL) {
				diff_eq_int("0x26 bits: the half-sine table",
					    peek_ptr_a(T44T_RX_CARRIER)
					    == (const void *)carriers[k].tbl,
					    1, 210 + (long)k);
				diff_eq_int("0x26 bits: and its count is half "
					    "the table's length",
					    2 * (int)carriers[k].half_len,
					    carriers[k].taps, 210 + (long)k);
			}
		}
	}

	/* --- 0x4d bits: INFO1c, 0x6f438 ----------------------------------- */

	/*
	 * THE CALLER'S INFO1c, AND THE INFO1a THAT ANSWERS IT.  The arm
	 * zeroes both trace counters before anything else, sets the transmit
	 * state to TX_DPSK, decodes the same message-descriptor length the
	 * 0x26 arm does, runs `probeselect`, and then builds a record at
	 * +0xa9ac and installs it at +0xaa6c.
	 *
	 * NO V.90 RECEIVER, so `V34GiveINFO1dBits` returns before it decides
	 * anything and the long path runs.
	 *
	 * THE COUNTER IS RESET AT THE TOP, so the byte clock this returns
	 * into finds zero and stores nothing -- finding F406's shape, and the
	 * seed at slot 3 is what shows it.
	 */
	begin(V34HS_MOH_SILENCE);
	accept_at(0x4d);
	v34hs_poke_short(T44T_REC_A9DC + 0, 7);
	v34hs_poke_short(T44T_REC_A9DC + 2, 8);
	v34hs_poke_short(T44T_F35A2, 0x1e1e);
	v34hs_poke_short(T44T_F358C, 0x2f2f);
	v34hs_poke_short(0x2aa2, 0x0123);
	v34hs_poke_short(T44T_REC_A9AC + 0x04, 0x1200);
	v34hs_poke_short(T44T_REC_A9AC + T44T_R_CRC, 0x1234);
	v34hs_poke_short(T44T_REC_A9AC + T44T_R_F16, 0x2345);
	v34hs_poke_short(T44T_REC_A9AC + T44T_R_NBITS, 0x3456);
	v34hs_poke_short(T44T_REC_A9AC + T44T_R_F1A, 0x4567);
	v34hs_poke_short(T44T_REC_A9AC + T44T_R_F1C, 0x5678);
	v34hs_poke_short(T44T_REC_A9AC + T44T_R_F1E, 0x6789);
	v34hs_poke_short(T44T_REC_A9AC + T44T_R_F20, 0x789a);
	v34hs_poke_short(T44T_REC_A9AC + T44T_R_F22, 0x1a2b);
	v34hs_poke_int(T44T_REC_A9AC + T44T_R_F24, 0x2b3c4d);
	v34hs_poke_short(T44T_REC_A9AC + T44T_R_F28, 0x3c4d);
	v34hs_poke_short(T44T_REC_A9AC + T44T_R_F2A, 0x4d5e);
	v34hs_poke_int(T44T_REC_A9AC + T44T_R_F2C, 0x5e6f70);
	v34hs_step();
	v34hs_compare("accept, 0x4d bits, the INFO1a answer", 230);
	base = (const char *)v34hs_object(0);
	diff_eq_int("0x4d bits: the transmit state is TX_DPSK",
		    v34hs_observed(0)->txst, V34HS_TX_DPSK, 230);
	diff_eq_int("0x4d bits: the microstate is INFODONE",
		    v34hs_observed(0)->mst, V34HS_INFODONE, 230);
	diff_eq_int("0x4d bits: both trace counters are zeroed",
		    v34hs_peek_short(0, 0x2aa2) | v34hs_peek_short(0, T44T_COUNT),
		    0, 230);
	diff_eq_int("0x4d bits: so the byte clock stored nothing",
		    v34hs_peek_short(0, T44T_REC + 6),
		    (short)(0x0301 + 0x111 * 3), 230);
	diff_eq_int("0x4d bits: the message-descriptor length, reversed",
		    v34hs_peek_short(0, T44T_F35A2), 0x43, 230);
	diff_eq_int("0x4d bits: +0x358c cleared",
		    v34hs_peek_short(0, T44T_F358C), 0, 230);
	diff_eq_int("0x4d bits: +0xaa6c aimed at +0xa9ac",
		    (int)(*(char *const *)(base + T44T_PTR_AA6C) - base),
		    T44T_REC_A9AC, 230);
	/*
	 * THE RECORD IT BUILT, and its length is 0x26 -- the very length the
	 * arm above answers to.  The two sized arms are the two ends of one
	 * exchange and that is the constant checked twice.
	 */
	diff_eq_int("0x4d bits: the answer is 0x26 bits long",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_NBITS), 0x26,
		    230);
	diff_eq_int("0x4d bits: its register is reset",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_CRC), -1, 230);
	diff_eq_int("0x4d bits: +0x16",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F16), 1, 230);
	diff_eq_int("0x4d bits: +0x1a",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F1A), 0, 230);
	diff_eq_int("0x4d bits: +0x1c",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F1C), 8, 230);
	diff_eq_int("0x4d bits: +0x1e",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F1E), 0, 230);
	diff_eq_int("0x4d bits: +0x20",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F20), 0, 230);
	diff_eq_int("0x4d bits: +0x22",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F22), 0, 230);
	diff_eq_int("0x4d bits: +0x28",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F28), 0x10, 230);
	diff_eq_int("0x4d bits: +0x2a",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F2A), 0x10, 230);
	/*
	 * 0xff72 AND NOT THE RESTART'S 0xf72, at both +0x24 and +0x2c, and
	 * both are full ints: the high halfword is read too, because a
	 * halfword store would leave the seed's 0x2b and 0x5e up there.
	 */
	diff_eq_int("0x4d bits: +0x24 low",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F24), (short)0xff72,
		    230);
	diff_eq_int("0x4d bits: +0x24 high",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F24 + 2), 0, 230);
	diff_eq_int("0x4d bits: +0x2c low",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F2C), (short)0xff72,
		    230);
	diff_eq_int("0x4d bits: +0x2c high",
		    v34hs_peek_short(0, T44T_REC_A9AC + T44T_R_F2C + 2), 0, 230);

	/*
	 * THE V.92-CAPABLE ANSWER PATH, with the incoming record where the
	 * real predecessor leaves it.  Microstate 72 aims +0xaa70 at A9DC
	 * before DET_INFO; the older case above inherited bring-up's A97C
	 * pointer while separately planting the decoded fields in A9DC.  This
	 * sibling makes the clocked record and the decoded record one object.
	 *
	 * The final bit and CRC are hand-staged, so this remains an arm-local
	 * differential witness rather than a claim that the whole INFO1c bit
	 * stream ran through the FSK receiver.  Everything after acceptance is
	 * a supported state: receiver 1 is the entry state, both V.92
	 * capabilities are present, bit 0x20 requests PCM upstream, and the
	 * signed V92Lite configuration byte bars the retrain while preserving
	 * that selection.  The INFO1a builder then advances the receiver to 2.
	 *
	 * This makes the decoder argument observable in state, not just prose:
	 * the correct incoming A9DC record selects PCM and sets answer bit 0x20;
	 * the wrong freshly-zeroed A9AC record does neither.  The probe results
	 * are independently non-zero values derived by `dftenergy`, so omitting
	 * their copy is observable as well.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_self_ptr(T44T_PTR_AA70, T44T_REC_A9DC);
	{
		int i, side;

		for (i = 0; i < 10; i++) {
			v34hs_poke_short(T44T_REC_A9DC + 2 * i, (short)i);
			v34hs_poke_short(T44T_REC_A9AC + 2 * i, 0);
		}
		/* Canonical seven-bit descriptor and the INFO1d PCM request. */
		v34hs_poke_short(T44T_REC_A9DC + 0, 3);
		v34hs_poke_short(T44T_REC_A9DC + 2, 8);
		v34hs_poke_short(T44T_REC_A9DC + 14, 0x20);
		v34hs_poke_short(T44T_REC_A9DC + T44T_R_NBITS, 0x4d);
		v34hs_poke_short(T44T_REC_A9DC + T44T_R_CRC, 0x1234);
		v34hs_poke_short(T44T_NBITS, 5);
		v34hs_poke_short(T44T_SR, 0x1234);
		v34hs_poke_short(T44T_COUNT, 0x4d + 0x0f);
		v34hs_poke_short(T44T_F359C, 0x0066);
		v34hs_poke_int(T44T_V90RECV, 1);
		v34hs_poke_int(T44T_K56RECV, 0);
		v34hs_poke_short(T44T_REMOTE_V92, 1);

		aim_session();
		for (side = 0; side < 2; side++) {
			unsigned char *sess = session_of(side);
			struct v34_object *obj =
				(struct v34_object *)v34hs_object(side);
			int zero = 0;

			caps_buf[side][0x11] = 1;
			memcpy(sess + T44T_SESS_LAYOUT, &zero, sizeof(zero));
			((unsigned char *)obj->pac3c)[T44T_CFG_V92LITE] = 0x80;
		}
	}
	prepare_probe_energy();
	v34hs_step();
	restore_session(232);
	v34hs_compare("accept, 0x4d bits, V.92-capable incoming record", 232);
	diff_eq_int("0x4d V.92: the incoming record stayed selected",
		    (int)(*(char *const *)((const char *)v34hs_object(0)
					       + T44T_PTR_AA70)
			  - (const char *)v34hs_object(0)),
		    T44T_REC_A9DC, 232);
	diff_eq_int("0x4d V.92: the session remains PCM",
		    *(const int *)(session_of(0) + T44T_SESS_TYPE), 1, 232);
	diff_eq_int("0x4d V.92: the answer carries the PCM bit",
		    (unsigned short)v34hs_peek_short(0, T44T_REC_A9AC + 14)
			    & 0x20,
		    0x20, 232);
	diff_eq_int("0x4d V.92: the receiver advances to INFO1d sent",
		    *(const int *)((const char *)v34hs_object(0) + T44T_V90RECV),
		    2, 232);
	diff_eq_int("0x4d V.92: first probe energy copied",
		    ((const struct v34_object *)v34hs_object(0))->probe_results[0]
			    == 4096.0,
		    1, 232);
	diff_eq_int("0x4d V.92: last probe energy copied",
		    ((const struct v34_object *)v34hs_object(0))->probe_results[24]
			    == 2560000.0,
		    1, 232);

	/*
	 * ALREADY IN TX_DPSK, so that transition is a no-op and its
	 * diagnostic is not printed -- the one guard of the three arms that
	 * CAN be driven from both sides, because the microstate is 44 on
	 * every entry and the rxstate 43.
	 *
	 * TX_DPSK is also table 2's arm at 0x644c9, so the tail this case
	 * leaves through is a different one from every case above.
	 */
	begin(V34HS_TX_DPSK);
	accept_at(0x4d);
	v34hs_poke_short(T44T_REC_A9DC + 0, 7);
	v34hs_poke_short(T44T_REC_A9DC + 2, 8);
	v34hs_step();
	v34hs_compare("accept, 0x4d bits, the transmit state already there",
		      231);
	diff_eq_int("0x4d bits: still TX_DPSK", v34hs_observed(0)->txst,
		    V34HS_TX_DPSK, 231);

	/*
	 * Case 232 closes the former two V.90 call gaps locally.  It does not
	 * claim a continuous received INFO1c: that stronger composition is a
	 * separate 95-step sequence through microstates 72, 41 and all 93
	 * message/CRC bits of 44.  Keeping the boundary explicit is important:
	 * a caught local mutation is not evidence that the live connection has
	 * reached this exchange.
	 */

	/*
	 * THE RECORD MOVES AND ONE CALL DOES NOT FOLLOW IT.  Everything in
	 * this arm reaches the record through +0xaa70 -- except 0x6e757, which
	 * is `lea 0xa97c(%ebx)` and hands `V34GiveINFO0dBits` the fixed
	 * address whatever the pointer says.  With the two coincident, as the
	 * bring-up leaves them, that distinction is invisible; this case aims
	 * +0xaa70 somewhere else and gives the decoder a V.90 receiver so that
	 * it really reads and prints its buffer.  The two printouts then name
	 * two different records and the transcript separates them.
	 */
	begin(V34HS_MOH_SILENCE);
	v34hs_poke_self_ptr(T44T_PTR_AA70, T44T_REC_MOVED);
	{
		int k;

		for (k = 0; k < 10; k++)
			v34hs_poke_short(T44T_REC_MOVED + 2 * k,
					 (short)(0x0702 + 0x131 * k));
	}
	v34hs_poke_short(T44T_REC_MOVED + T44T_R_NBITS, 24);
	v34hs_poke_short(T44T_REC_MOVED + T44T_R_CRC, 0x1234);
	v34hs_poke_short(T44T_NBITS, 5);
	v34hs_poke_short(T44T_SR, 0x1234);
	v34hs_poke_short(T44T_COUNT, 39);
	v34hs_poke_short(T44T_F359C, 0x0050);
	v34hs_poke_int(T44T_V90RECV, 2);
	v34hs_step();
	v34hs_compare("accept, the record moved", 180);
	diff_eq_int("accept: the moved record's register was reset",
		    v34hs_peek_short(0, T44T_REC_MOVED + T44T_R_CRC),
		    v34hs_peek_short(1, T44T_REC_MOVED + T44T_R_CRC), 180);

	/* --- downstream of the real demodulator --------------------------- */

	/*
	 * `fsk_inhibit` CLEAR, so `fskdemodulate` really runs and the bits
	 * this arm consumes are the ones it produced.  Everything above holds
	 * the FSK receiver still to make `nbits` and `sr` the test's to
	 * choose; this one case gives that up in exchange for driving the arm
	 * where the object drives it.
	 *
	 * The branch is still pinned: `nbits` is seeded high enough that the
	 * demodulator cannot bring it to zero, and the counter reaching 1 is
	 * neither byte aligned nor at any threshold, so the exit is 0x6bd8d
	 * whatever four samples the queue delivered.  `changed` is not
	 * asserted here -- it counts what the demodulator wrote too.
	 */
	begin(V34HS_MOH_SILENCE);
	bits(200, 0x1234, 0, 500, 0x0001);
	v34hs_poke_short(T44T_INHIBIT, 0);
	v34hs_step();
	v34hs_compare("with the demodulator running", 140);
	diff_eq_int("the demodulator ran and the arm still took one bit",
		    v34hs_peek_short(0, T44T_COUNT), 1, 140);
	diff_eq_int("and stepped the register",
		    v34hs_peek_short(0, T44T_REC + T44T_R_CRC),
		    (v34hs_peek_short(0, T44T_SR) & 1) != 0 ? 0x1023 : 0x0002,
		    140);

	/* --- the transmit state is part of the case ----------------------- */

	/*
	 * MOH_CLEARDOWN as the transmit state.  84 is above table 2's window
	 * like MOH_SILENCE, but the tail compares against it by name and
	 * writes a progress code of its own (finding F358), so this is a
	 * different tail from every case above with the same arm in front of
	 * it.
	 */
	begin(V34HS_MOH_CLEARDOWN);
	bits(5, 0x12ab, 7, 200, 0x0001);
	step("byte 0 stored, MOH_CLEARDOWN", 150, 23, 0, V34HS_DET_INFO,
	     V34HS_MOH_CLEARDOWN);
	diff_eq_int("the tail's own progress code",
		    (int)((const int *)v34hs_object(0))[1], 0x10, 150);

	/*
	 * THE POINTER HOLES.  `v34hs_compare` skips thirty-five pointer
	 * fields and compares each by offset from its own base; this asserts
	 * every one of them was reached, so the skip list cannot go stale
	 * while these cases run.  Finding F290.
	 */
	v34hs_holes_check();

	return diff_end();
}
