/*
 * t_class1states.c -- differential test of three of the nineteen Class 1 fax
 * state handlers, plus `_idle_state_init`.
 *
 * ALL THREE HANDLERS ARE FILE-LOCAL IN THE OBJECT, so they are reached
 * through the `ref_` aliases F221's two-pass objcopy makes for a promoted
 * local -- the same route `t_v34getbit` takes.  Nothing calls them by name in
 * the blob: `fax_class1_create` stores their addresses into
 * `class1_state_functions`, which is a relocation against the SECTION symbol
 * and appears under no name at all.
 *
 * THE FIXTURE PLANTS EVERY FIELD USED AS A SUBSCRIPT (F8587/D955).  Here the
 * only subscript is `tx[i]`, bounded by `*rx_count` in `_idle_state` -- so
 * `*rx_count` is forced into 0..TX_MAX on every call and the transmit array
 * is TX_MAX + GUARD long, with the guard compared. A random `*rx_count` would
 * walk both sides identically off the end and they would agree.
 *
 * `_recieve_silence_state` calls `FPM_rms` over `*rx_count` samples of `rx`,
 * so the receive array is the same size and is filled identically on both
 * sides; the two sides get their OWN arrays so an aliasing accident cannot
 * make one side see the other's writes.
 *
 * COVERAGE IS ASSERTED FROM THE RUN, not from the case table (F134): the
 * counters below are incremented by what the REFERENCE actually did -- the
 * status it left, the state it installed, the value it wrote into `word8` --
 * so a case list that stopped reaching an arm shows up as a failed
 * denominator rather than as a quiet pass.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/debug.h"
#include "dsplib/fpm.h"

extern unsigned int ref_dsplibs_debug_level;

extern int ref__idle_state_init(void *ctx);
extern int ref__idle_state(void *ctx, const short *rx, short *tx,
			   int w3, int w4, int *rx_count, int *tx_count,
			   int w7, int *w8);
extern int ref__send_silence_state(void *ctx, const short *rx, short *tx,
				   int w3, int w4, int *rx_count,
				   int *tx_count, int w7, int *w8);
extern int ref__recieve_silence_state(void *ctx, const short *rx, short *tx,
				      int w3, int w4, int *rx_count,
				      int *tx_count, int w7, int *w8);

/*
 * FILE-LOCAL in the object, so class1.c defines them `static` and class1.h
 * no longer declares them.  Their addresses are taken (class1.c installs
 * each into `class1_state_functions`), so the ordinary calling convention is
 * unchanged; the test tier links a globalized copy (tools/testvisible.py).
 */
extern int _idle_state(struct fax_class1 *ctx, const short *rx, short *tx,
		       int word3, int word4, int *rx_count, int *tx_count,
		       int word7, int *word8);
extern int _send_silence_state(struct fax_class1 *ctx, const short *rx,
			       short *tx, int word3, int word4,
			       int *rx_count, int *tx_count, int word7,
			       int *word8);
extern int _recieve_silence_state(struct fax_class1 *ctx, const short *rx,
				  short *tx, int word3, int word4,
				  int *rx_count, int *tx_count, int word7,
				  int *word8);

#define TX_MAX	256
#define GUARD	16

static unsigned long seed = 20260831UL;

static unsigned long
rnd(void)
{
	seed = seed * 1103515245UL + 12345UL;
	return (seed >> 8) & 0xffffffUL;
}

static void
fill(void *p, unsigned n)
{
	unsigned char *b = (unsigned char *)p;
	unsigned i;

	for (i = 0; i < n; i++)
		b[i] = (unsigned char)(rnd() & 0xff);
}

static struct fax_class1 ctx_a, ctx_b;
static short rx_a[TX_MAX], rx_b[TX_MAX];
static short tx_a[TX_MAX + GUARD], tx_b[TX_MAX + GUARD];
static int rxc_a, rxc_b, txc_a, txc_b, w8_a, w8_b;

/*
 * Build both sides identically.  `ctx` is randomised first so that no field
 * is quietly zero, then every field these handlers READ is planted:
 * `countdown`, `silence_blocks`, `status` and `state`.
 */
static void
plant(int rx_count, int countdown, unsigned int blocks, int w8)
{
	fill(&ctx_a, (unsigned)sizeof(ctx_a));
	memcpy(&ctx_b, &ctx_a, sizeof(ctx_a));
	ctx_a.countdown = ctx_b.countdown = countdown;
	ctx_a.silence_blocks = ctx_b.silence_blocks = blocks;
	ctx_a.status = ctx_b.status = FAX_CLASS1_ERROR;
	ctx_a.state = ctx_b.state = CLASS1_RECIEVE_SILENCE_STATE;

	fill(rx_a, (unsigned)sizeof(rx_a));
	memcpy(rx_b, rx_a, sizeof(rx_a));
	memset(tx_a, 0x5a, sizeof(tx_a));
	memcpy(tx_b, tx_a, sizeof(tx_a));

	rxc_a = rxc_b = rx_count;
	txc_a = txc_b = (int)0x7e7e7e7e;
	w8_a = w8_b = w8;
}

static void
compare(const char *what, long tag)
{
	char buf[96];

	snprintf(buf, sizeof(buf), "%s: ctx (%%ld)", what);
	diff_eq_int(buf, memcmp(&ctx_a, &ctx_b, sizeof(ctx_a)), 0, tag);
	snprintf(buf, sizeof(buf), "%s: tx block and its guard (%%ld)", what);
	diff_eq_int(buf, memcmp(tx_a, tx_b, sizeof(tx_a)), 0, tag);
	snprintf(buf, sizeof(buf), "%s: tx_count (%%ld)", what);
	diff_eq_int(buf, (long)txc_b, (long)txc_a, tag);
	snprintf(buf, sizeof(buf), "%s: rx_count (%%ld)", what);
	diff_eq_int(buf, (long)rxc_b, (long)rxc_a, tag);
	snprintf(buf, sizeof(buf), "%s: word8 (%%ld)", what);
	diff_eq_int(buf, (long)w8_b, (long)w8_a, tag);
	snprintf(buf, sizeof(buf), "%s: rx untouched (%%ld)", what);
	diff_eq_int(buf, memcmp(rx_a, rx_b, sizeof(rx_a)), 0, tag);
}

static int
run_idle(void)
{
	unsigned i;
	int short_block = 0, full_block = 0;

	diff_begin("_idle_state and _idle_state_init");

	for (i = 0; i < 8; i++)
		diff_eq_int("_idle_state_init (%ld)", _idle_state_init(&ctx_b),
			    ref__idle_state_init(&ctx_a), (long)i);

	for (i = 0; i < 40; i++) {
		static const int counts[10] = {
			0, 1, 2, 3, 159, 160, 161, 255, -1, -160
		};
		int n = counts[i % 10];
		int ra, rb;

		plant(n, 7, 0, 0);
		ra = ref__idle_state(&ctx_a, rx_a, tx_a, 0x11111111,
				     0x22222222, &rxc_a, &txc_a, 0x33333333,
				     &w8_a);
		rb = _idle_state(&ctx_b, rx_b, tx_b, 0x11111111, 0x22222222,
				 &rxc_b, &txc_b, 0x33333333, &w8_b);
		diff_eq_int("_idle_state return (%ld)", rb, ra, (long)n);
		compare("_idle_state", (long)n);

		if (txc_a == CLASS1_BLOCK_SAMPLES)
			full_block++;
		else
			short_block++;
	}
	diff_eq_int("_idle_state: the *rx_count arm ran (%ld)",
		    short_block > 0, 1, (long)short_block);
	diff_eq_int("_idle_state: the 160 fallback ran (%ld)", full_block > 0,
		    1, (long)full_block);
	return diff_end();
}

static int
run_send_silence(void)
{
	unsigned i;
	int fired = 0, not_fired = 0;

	diff_begin("_send_silence_state");
	for (i = 0; i < 24; i++) {
		static const int downs[8] = { 1, 2, 3, 7, 0, -1, -3, 100 };
		int cd = downs[i % 8];
		int ra, rb;

		plant(160, cd, 0, 0);
		ra = ref__send_silence_state(&ctx_a, rx_a, tx_a, 0x44444444,
					     0x55555555, &rxc_a, &txc_a,
					     0x66666666, &w8_a);
		rb = _send_silence_state(&ctx_b, rx_b, tx_b, 0x44444444,
					 0x55555555, &rxc_b, &txc_b,
					 0x66666666, &w8_b);
		diff_eq_int("_send_silence_state return (%ld)", rb, ra,
			    (long)cd);
		compare("_send_silence_state", (long)cd);

		if (ctx_a.status == FAX_CLASS1_OK_NO_CARRIER)
			fired++;
		else
			not_fired++;
	}
	diff_eq_int("_send_silence_state: the countdown expired (%ld)",
		    fired > 0, 1, (long)fired);
	diff_eq_int("_send_silence_state: and did not, elsewhere (%ld)",
		    not_fired > 0, 1, (long)not_fired);
	return diff_end();
}

/*
 * Three arms, and each is asserted to have been taken by the REFERENCE:
 * abort, energy-above-threshold, and the counting arm (which itself has two
 * exits).  The energy is steered by what is put in `rx`, so the fixture makes
 * a loud block and a silent one rather than hoping a random fill lands on
 * both sides of 100.
 */
static int
run_recv_silence(void)
{
	unsigned i;
	int aborts = 0, loud = 0, counting = 0, expired = 0;
	unsigned level;

	diff_begin("_recieve_silence_state");
	for (level = 0; level < 4; level++) {
		for (i = 0; i < 40; i++) {
			int cd = (int)(i % 5) + 1;
			unsigned int blocks = (unsigned int)(i % 7);
			int w8 = (i % 4 == 3) ? (int)(i + 1) : 0;
			int n = (i % 3 == 0) ? 160 : (int)(i % 200) + 1;
			int ra, rb;
			long tag = (long)(level * 1000 + i);
			unsigned j;

			plant(n, cd, blocks, w8);
			/* a loud block on even i, a silent one on odd */
			for (j = 0; j < TX_MAX; j++) {
				short v = (short)((i & 1) ? 0
						 : (short)(3000 - (int)j));
				rx_a[j] = v;
				rx_b[j] = v;
			}
			dsplibs_debug_level = level;
			ref_dsplibs_debug_level = level;

			ra = ref__recieve_silence_state(&ctx_a, rx_a, tx_a,
							0x77777777, (int)0x88888888,
							&rxc_a, &txc_a,
							(int)0x99999999, &w8_a);
			rb = _recieve_silence_state(&ctx_b, rx_b, tx_b,
						    0x77777777, (int)0x88888888,
						    &rxc_b, &txc_b,
						    (int)0x99999999, &w8_b);
			diff_eq_int("_recieve_silence_state return (%ld)", rb,
				    ra, tag);
			compare("_recieve_silence_state", tag);

			if (w8 != 0)
				aborts++;
			else if (ctx_a.energy > CLASS1_SILENCE_THRESHOLD)
				loud++;
			else {
				counting++;
				if (ctx_a.state == CLASS1_IDLE_STATE)
					expired++;
			}
		}
	}
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("_recieve_silence_state: the abort arm ran (%ld)",
		    aborts > 0, 1, (long)aborts);
	diff_eq_int("_recieve_silence_state: energy above threshold (%ld)",
		    loud > 0, 1, (long)loud);
	diff_eq_int("_recieve_silence_state: the counting arm ran (%ld)",
		    counting > 0, 1, (long)counting);
	diff_eq_int("_recieve_silence_state: the count reached countdown (%ld)",
		    expired > 0, 1, (long)expired);
	return diff_end();
}

/*
 * THE THRESHOLD IS `> 100`, AND ONLY AN ENERGY OF EXACTLY 100 SEPARATES THAT
 * FROM `>= 100`.  The random and shaped blocks above never land on it, so the
 * injection ritual reported `>` -> `>=` NOT CAUGHT -- a real hole, not an
 * equivalent mutant, because the two spellings send an exactly-100 block down
 * different arms.  See F8940.
 *
 * The amplitude is FOUND rather than computed: `FPM_rms` is already
 * reconstructed and tested, so it is used to search for a constant block
 * whose RMS is the threshold, and the case is then run differentially like
 * any other.  What the search produced is checked against the REFERENCE's own
 * `energy` afterwards, so a search that found the wrong block fails the
 * denominator instead of quietly testing nothing.
 */
static int
run_recv_silence_exact(void)
{
	int v, found = -1;
	unsigned i;
	long on_threshold = 0;

	diff_begin("_recieve_silence_state on an energy of exactly 100");

	for (v = 1; v < 20000 && found < 0; v++) {
		for (i = 0; i < 160; i++)
			rx_b[i] = (short)v;
		if (FPM_rms(rx_b, 160) == CLASS1_SILENCE_THRESHOLD)
			found = v;
	}
	diff_eq_int("a constant block of RMS 100 was found (%ld)", found > 0, 1,
		    (long)found);
	if (found <= 0)
		return diff_end();

	for (i = 0; i < 8; i++) {
		int cd = (int)(i % 4) + 1;
		int ra, rb;

		plant(160, cd, (unsigned int)(i % 3), 0);
		for (v = 0; v < 160; v++) {
			rx_a[v] = (short)found;
			rx_b[v] = (short)found;
		}
		ra = ref__recieve_silence_state(&ctx_a, rx_a, tx_a, 0, 0,
						&rxc_a, &txc_a, 0, &w8_a);
		rb = _recieve_silence_state(&ctx_b, rx_b, tx_b, 0, 0, &rxc_b,
					    &txc_b, 0, &w8_b);
		diff_eq_int("exact-threshold return (%ld)", rb, ra, (long)i);
		compare("exact-threshold", (long)i);
		if (ctx_a.energy == CLASS1_SILENCE_THRESHOLD)
			on_threshold++;
	}
	diff_eq_int("the reference measured exactly 100 (%ld)",
		    on_threshold, 8, on_threshold);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_idle();
	rc |= run_send_silence();
	rc |= run_recv_silence();
	rc |= run_recv_silence_exact();
	return rc;
}
