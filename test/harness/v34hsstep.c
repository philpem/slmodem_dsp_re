/*
 * v34hsstep.c -- the per-dispatch-case fixture for `v34handshak`.
 *
 * See v34hsstep.h for what this is for and docs/v34handshak.md for how to use
 * it.  This file is the mechanics: two objects, the pointer fields each one
 * needs aimed somewhere real, the four guards that pick a dispatch, and a
 * step that records what the call wrote.
 *
 * NOTHING HERE RECONSTRUCTS ANY PART OF `v34handshak`.  The offsets below are
 * read off its prologue and off `v34handshakinit`'s callees; no case body is
 * modelled, and the fixture has no opinion about what a case should do.
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "harness.h"
#include "v34hsstep.h"

#include "dsplib/v34fsk.h"	/* struct v34_object */
#include "dsplib/v34hshak.h"	/* v34handshakinit */

extern unsigned int dsplibs_debug_level;
extern unsigned int ref_dsplibs_debug_level;

extern void ref_v34handshak(void *obj);
extern void ref_v34handshakinit(void *obj, int mode);
extern void ref_V34InitializeImplementationSpecific(void *obj);
extern void V34InitializeImplementationSpecific(void *obj);

/*
 * THE ONE-LINE SWAP.
 *
 * `v34handshak` has no reconstruction, so side A runs the blob too and the
 * comparison proves the FIXTURE rather than a reconstruction.  The first
 * agent to land a dispatch case defines V34HS_OURS on the command line (or
 * here) and side A becomes the reconstruction, at which point every test
 * written against this fixture becomes an ordinary tier-1 differential test
 * with no other edit anywhere.
 */
#ifdef V34HS_OURS
extern void v34handshak(void *obj);
#define V34HS_CALL_A(o)		v34handshak(o)
#define V34HS_LOG_A		0
#else
#define V34HS_CALL_A(o)		ref_v34handshak(o)
#define V34HS_LOG_A		1
#endif

/* --- the two sides -------------------------------------------------------- */

#define OBJ_SIZE	((unsigned)sizeof(struct v34_object))

static struct v34_object obj_a;
static unsigned char obj_b[sizeof(struct v34_object)];
static unsigned char snap_a[sizeof(struct v34_object)];
static unsigned char snap_b[sizeof(struct v34_object)];

/*
 * The blocks the object points OUT of, per side.
 *
 * `V34SetupModulator` writes through the shaping buffer, and
 * `GetVPcmMinimalTxPowerReduction` -- which the handshake reaches through
 * `settxlevel` -- walks the session at +0x3548 to a PCM receiver and reads
 * the configuration at +0xac3c.  Sizes are t_v34hshak.c's, which derived them
 * from the highest offset each is indexed at.
 */
#define SHAPED_LEN	4096
#define SESS_LEN	0x6200
#define PCM_LEN		0x0520
#define CFG_LEN		0x0080
#define SESS_PCM	0x610c
#define DUMMY_LEN	8192

static short shaped_a[SHAPED_LEN], shaped_b[SHAPED_LEN];
static unsigned char sess_a[SESS_LEN], sess_b[SESS_LEN];
static unsigned char pcm_a[PCM_LEN], pcm_b[PCM_LEN];
static unsigned char cfg_a[CFG_LEN], cfg_b[CFG_LEN];
static short dummy_a[DUMMY_LEN], dummy_b[DUMMY_LEN];

/*
 * Every pointer-sized field this fixture's bring-up leaves holding an
 * address.  Two objects at two addresses hold two different values in each,
 * necessarily and forever, so these are excluded from the byte comparison and
 * checked by what they select instead.
 *
 * The list is t_v34hshak.c's, which derived it from the code rather than from
 * watching a test fail, plus the four this fixture owns.  `saw_hole` asserts
 * every entry was reached, so it cannot quietly go stale.
 */
static const unsigned holes[] = {
	0x0394,		/* receiver +0x130 rx_samples  -- rxinit, interior  */
	0x0418,		/* receiver +0x1b4 carrier     -- setupreceiver     */
	0x0508,		/* receiver +0x2a4 f2a4        -- dpskinit          */
	0x0620, 0x0624,	/* timing +0x114, +0x118      -- mode 0 only       */
	0x1460,		/* modulator +0x10 sine        -- V34SetupModulator */
	0x2074,		/* modulator +0xc24 shaped     -- seeded here       */
	0x20cc,		/* modulator +0xc7c ec_prem                         */
	0x2100,		/* modulator +0xcb0 preemp                          */
	0x3564,		/* detector +0x00 coeff        -- detectorinit      */
	0xaa90,		/* tx power scale              -- setfinalrate      */
	0xaaac,		/* rx power scale                                   */
	0xaab0,		/* rx carrier descriptor                            */
	0x0268, 0x026c,	/* receive sample queue cursors                     */
	0x2220, 0x2224,	/* transmit sample queue cursors                    */
	0x80b8, 0x80bc, 0x80c0, 0x80c4, 0x80c8,	/* echo canceller 0        */
	0x9138, 0x913c, 0x9140, 0x9144, 0x9148,	/* and 1                   */
	0x0a28, 0x0e48,	/* receive shell context                            */
	0x2608, 0x2a28,	/* transmit shell context                           */
	0xaa6c, 0xaa70,	/* the two SELF-pointers -- checked by offset       */
	0x3548, 0xac3c	/* the session and the configuration -- ours        */
};
#define NHOLES	((unsigned)(sizeof(holes) / sizeof(holes[0])))

/*
 * WAS EACH SKIP ENTRY ACTUALLY EXERCISED?  Accumulated across the whole run
 * and asserted once, never per case: a per-case reset would only ever report
 * the last one, which is a check that reads as a check and is not.  An entry
 * that never differs is an entry the two sides agree on, and a hole nothing
 * needs is a hole that has gone stale -- which is the failure this array
 * exists to catch and did not, until it was read as well as written.
 */
static int saw_hole[NHOLES];

static int
in_hole(unsigned off)
{
	unsigned k;

	for (k = 0; k < NHOLES; k++)
		if (off >= holes[k] && off < holes[k] + 4)
			return 1;
	return 0;
}

/* --- pokes and peeks ------------------------------------------------------ */

void *
v34hs_object(int side)
{
	return side ? (void *)obj_b : (void *)&obj_a;
}

static unsigned char *
base(int side)
{
	return side ? obj_b : (unsigned char *)&obj_a;
}

void
v34hs_poke_short(unsigned off, short v)
{
	memcpy(base(0) + off, &v, sizeof(v));
	memcpy(base(1) + off, &v, sizeof(v));
}

void
v34hs_poke_int(unsigned off, int v)
{
	memcpy(base(0) + off, &v, sizeof(v));
	memcpy(base(1) + off, &v, sizeof(v));
}

void
v34hs_poke_byte(unsigned off, unsigned char v)
{
	base(0)[off] = v;
	base(1)[off] = v;
}

short
v34hs_peek_short(int side, unsigned off)
{
	short v;

	memcpy(&v, base(side) + off, sizeof(v));
	return v;
}

static void
poke_ptr(unsigned off, void *pa, void *pb)
{
	memcpy(base(0) + off, &pa, sizeof(pa));
	memcpy(base(1) + off, &pb, sizeof(pb));
}

static void *
peek_ptr(int side, unsigned off)
{
	void *p;

	memcpy(&p, base(side) + off, sizeof(p));
	return p;
}

/* --- construction --------------------------------------------------------- */

/*
 * VARIED BYTES, NEVER ZERO, AND THE SAME ON BOTH SIDES.
 *
 * A constant fill makes a 16-bit field and the two 8-bit fields beside it
 * indistinguishable, and zero additionally makes "nothing wrote this" look
 * like a deliberate value.  The generator is an LCG rather than `rand` so the
 * fill is the same on every host and in every build: a differential test that
 * drifts between runs cannot be bisected.  Finding 230.
 */
static void
fill(unsigned char *p, unsigned n, unsigned seed)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		seed = seed * 1103515245u + 12345u;
		p[i] = (unsigned char)(seed >> 16);
	}
}

void
v34hs_setup(int mode)
{
	unsigned i;

	fill((unsigned char *)&obj_a, OBJ_SIZE, 0x5eed1234u);
	memcpy(obj_b, &obj_a, OBJ_SIZE);

	fill((unsigned char *)shaped_a, sizeof(shaped_a), 0x11117777u);
	memcpy(shaped_b, shaped_a, sizeof(shaped_a));
	fill(sess_a, sizeof(sess_a), 0x22228888u);
	memcpy(sess_b, sess_a, sizeof(sess_a));
	fill(pcm_a, sizeof(pcm_a), 0x33339999u);
	memcpy(pcm_b, pcm_a, sizeof(pcm_a));
	fill(cfg_a, sizeof(cfg_a), 0x4444aaaau);
	memcpy(cfg_b, cfg_a, sizeof(cfg_a));

	for (i = 0; i < DUMMY_LEN; i++)
		dummy_a[i] = dummy_b[i] = (short)(0x4b00 + i);

	/*
	 * The session's one pointer field, and the configuration byte
	 * `GetVPcmMinimalTxPowerReduction` reads as a power reduction.  Left
	 * at the fill they would be a wild pointer and an absurd reduction.
	 */
	memcpy(sess_a + SESS_PCM, &(void *){ pcm_a }, sizeof(void *));
	memcpy(sess_b + SESS_PCM, &(void *){ pcm_b }, sizeof(void *));
	memset(cfg_a + 0x40, 0, 0x20);
	memset(cfg_b + 0x40, 0, 0x20);

	/*
	 * EVERY POINTER FIELD IS AIMED BEFORE ANY CODE RUNS.  The fill puts a
	 * pseudorandom address in each, and the bring-up below dereferences
	 * several of them; a fixture that let one through would fault rather
	 * than fail, and a fault has no offset in it.
	 */
	poke_ptr(0x2074, shaped_a, shaped_b);
	poke_ptr(0x3548, sess_a, sess_b);
	poke_ptr(0xac3c, cfg_a, cfg_b);
	for (i = 0; i < NHOLES; i++)
		if (holes[i] != 0x2074 && holes[i] != 0x3548
		    && holes[i] != 0xac3c)
			poke_ptr(holes[i], dummy_a, dummy_b);

	/*
	 * AND THE THREE STATE WORDS ARE PUT IN RANGE BEFORE ANYTHING TRACES.
	 *
	 * `StateName` is indexed with no bound (D42), and `v34handshakinit`
	 * announces each transition by printing the state it is LEAVING -- so
	 * with the diagnostics on, a pseudorandom halfword at +0x3592 is a
	 * wild `char *` handed to `vsnprintf`, and the fixture faults inside
	 * its own bring-up.  Found exactly that way.
	 */
	v34hs_state(0, 0, 0);

	/*
	 * `v34handshakinit` reaches `txinit`, which cleans both echo
	 * cancellers through five pointers each, and aiming those is
	 * `V34InitializeImplementationSpecific`'s job rather than its own.
	 * That is what a real caller does, not something the fixture invents:
	 * `VPcmV34Create` calls it before the handshake comes up.
	 *
	 * Side A runs OUR copy and side B the blob's, so the bring-up is
	 * itself differential -- which is what makes an agreeing step evidence
	 * rather than a tautology.
	 */
	if (getenv("V34HS_REFINIT")) {
		ref_V34InitializeImplementationSpecific(&obj_a);
		ref_v34handshakinit(&obj_a, mode);
	} else {
		V34InitializeImplementationSpecific(&obj_a);
		v34handshakinit(&obj_a, mode);
	}
	ref_V34InitializeImplementationSpecific(obj_b);
	ref_v34handshakinit(obj_b, mode);

	if (getenv("V34HS_DIAG")) {
		static const unsigned tab[] = { 0x0418, 0x0508, 0x0620,
						0x0624, 0x1460, 0x20cc,
						0x2100, 0x3564, 0xaa90,
						0xaaac, 0xaab0 };
		unsigned k, n;

		for (k = 0; k < sizeof(tab) / sizeof(tab[0]); k++) {
			const unsigned char *pa = peek_ptr(0, tab[k]);
			const unsigned char *pb = peek_ptr(1, tab[k]);
			int first = -1;

			if (pa == (const unsigned char *)dummy_a)
				continue;
			for (n = 0; n < 4096; n++)
				if (pa[n] != pb[n]) { first = (int)n; break; }
			printf("  DIAG table +0x%04x: %s at %d\n", tab[k],
			       first < 0 ? "equal to 4096" : "DIFFERS", first);
		}
	}
}

void
v34hs_route(enum v34hs_route r, short samples)
{
	switch (r) {
	case V34HS_ROUTE_TXSAMPLE:
		/* cursor < limit: the per-sample loop, `samples` passes. */
		v34hs_poke_short(V34HS_TXLIMIT, samples);
		v34hs_poke_short(V34HS_TXCURSOR, 0);
		break;
	case V34HS_ROUTE_TXBLOCK:
		/* cursor >= limit, then receiver +0x00 <= 5. */
		v34hs_poke_short(V34HS_TXLIMIT, 0);
		v34hs_poke_short(V34HS_TXCURSOR, 0);
		v34hs_poke_short(V34HS_RXCOUNT, 5);
		break;
	case V34HS_ROUTE_RXCHAIN:
		/* cursor >= limit, receiver +0x00 > 5, and the gate at
		 * +0xa8a0 clear so rxstate 43 reaches `fskdemodulate` and
		 * the microstate table rather than diverting at 0x64a87. */
		v34hs_poke_short(V34HS_TXLIMIT, 0);
		v34hs_poke_short(V34HS_TXCURSOR, 0);
		v34hs_poke_short(V34HS_RXCOUNT, 6);
		v34hs_poke_int(V34HS_FSKGATE, 0);
		break;
	}
}

void
v34hs_state(short mst, short rxst, short txst)
{
	v34hs_poke_short(V34HS_MICROSTATE, mst);
	v34hs_poke_short(V34HS_RXSTATE, rxst);
	v34hs_poke_short(V34HS_TXSTATE, txst);
}

void
v34hs_debug(int on)
{
	dsplib_debug_capture_on = on;
	dsplibs_debug_level = on ? 2u : 0u;
	ref_dsplibs_debug_level = on ? 2u : 0u;
}

/* --- the step ------------------------------------------------------------- */

static struct v34hs_obs obs[2];
static char text[2][4096];

/*
 * THE LOOP AT 0x629e0 DOES NOT ALWAYS TERMINATE, so the step is guarded.
 *
 * Table 1's default arm is the loop bottom itself: an unhandled txstate
 * re-tests the cursor against the limit, finds it unchanged, and jumps back to
 * the dispatch.  Without this the failure is a test run that never returns,
 * which `make phase` reports as nothing at all.  Finding 287.
 */
static volatile int step_side;
static short step_mst, step_rxst, step_txst;

static void
step_alarm(int sig)
{
	char msg[160];
	int n;

	(void)sig;
	n = snprintf(msg, sizeof(msg),
		     "\nv34hs_step: side %d did not return with "
		     "mst=%d rxstate=%d txstate=%d -- see finding 287\n",
		     step_side, step_mst, step_rxst, step_txst);
	/* Nothing useful to do about a failed write from a signal handler;
	 * the exit status carries the failure either way. */
	if (n > 0)
		(void)!write(2, msg, (size_t)n);
	_exit(3);
}

/*
 * SCRUB THE STACK BEFORE EACH SIDE'S CALL.
 *
 * Without this the two sides disagree in the per-sample transmit route, and
 * WHICH txstates disagree changes when unrelated code in this file changes
 * -- because side A runs first and leaves residue that side B then reads.
 * The blob reads uninitialised stack there, which is D37's shape one level
 * up: the object supplies no default arm for a switch, so a rate it does not
 * recognise runs on whatever the caller happened to leave behind.
 *
 * Handing both sides the same 64 KB of 0x5a makes the step a function of the
 * object again.  It does not make the read defined -- it makes it EQUAL, and
 * finding 289 records that the difference is real and what it costs.
 */
static volatile unsigned scrub_sink;

static void
scrub_stack(void)
{
	unsigned char pad[65536];

	memset(pad, 0x5a, sizeof(pad));
	scrub_sink = (unsigned)pad[0] + pad[sizeof(pad) - 1];
}

static void
observe(int side, const unsigned char *now, const unsigned char *was,
	unsigned lines)
{
	struct v34hs_obs *o = &obs[side];
	unsigned i;

	o->changed = 0;
	o->first = ~0u;
	o->last = 0;
	o->hash = 2166136261u;
	o->lines = lines;

	for (i = 0; i < OBJ_SIZE; i++) {
		/*
		 * THE POINTER FIELDS ARE OUT OF THE SIGNATURE ENTIRELY, value
		 * AND fact-of-change.
		 *
		 * The first version hashed a marker for a changed pointer, on
		 * the reasoning that a step which only re-aimed one would
		 * otherwise signature as having done nothing.  That is
		 * address-dependent and therefore wrong: writing a pointer
		 * over a previous value changes however many BYTES the two
		 * addresses happen to differ in, which is two on one side and
		 * three on the other.  Every microstate case then differed by
		 * exactly one byte -- but only in the instrumented build,
		 * where the addresses move.  `make debugcov` caught it; the
		 * ordinary build never would have.
		 *
		 * What replaces it is `check_self_ptr` over all thirty-five
		 * holes, which compares OFFSETS and is address-independent by
		 * construction.
		 */
		if (now[i] == was[i] || in_hole(i))
			continue;
		o->changed++;
		if (o->first == ~0u)
			o->first = i;
		o->last = i;
		o->hash = (o->hash ^ i) * 16777619u;
		o->hash = (o->hash ^ now[i]) * 16777619u;
	}

	o->mst = v34hs_peek_short(side, V34HS_MICROSTATE);
	o->rxst = v34hs_peek_short(side, V34HS_RXSTATE);
	o->txst = v34hs_peek_short(side, V34HS_TXSTATE);
	memcpy(&o->progress, base(side) + V34HS_PROGRESS, sizeof(o->progress));
}

void
v34hs_step(void)
{
	void (*prev)(int);

	step_mst = v34hs_peek_short(0, V34HS_MICROSTATE);
	step_rxst = v34hs_peek_short(0, V34HS_RXSTATE);
	step_txst = v34hs_peek_short(0, V34HS_TXSTATE);

	memcpy(snap_a, &obj_a, OBJ_SIZE);
	memcpy(snap_b, obj_b, OBJ_SIZE);

	prev = signal(SIGALRM, step_alarm);

	step_side = 0;
	dsplib_debug_capture_reset();
	scrub_stack();
	alarm(5);
	V34HS_CALL_A(&obj_a);
	alarm(0);
	snprintf(text[0], sizeof(text[0]), "%s",
		 dsplib_debug_capture_text(V34HS_LOG_A));
	observe(0, (const unsigned char *)&obj_a, snap_a,
		dsplib_debug_capture_lines(V34HS_LOG_A));

	step_side = 1;
	dsplib_debug_capture_reset();
	scrub_stack();
	alarm(5);
	ref_v34handshak(obj_b);
	alarm(0);
	snprintf(text[1], sizeof(text[1]), "%s", dsplib_debug_capture_text(1));
	observe(1, obj_b, snap_b, dsplib_debug_capture_lines(1));

	signal(SIGALRM, prev);
}

void
v34hs_holes_check(void)
{
	unsigned k;
	char msg[128];

	for (k = 0; k < NHOLES; k++) {
		snprintf(msg, sizeof(msg),
			 "pointer skip +0x%04x was exercised", holes[k]);
		diff_eq_int(msg, saw_hole[k], 1, (long)holes[k]);
	}
	diff_eq_int("pointer skips", NHOLES, V34HS_NHOLES, 0);
}

const struct v34hs_obs *
v34hs_observed(int side)
{
	return &obs[side & 1];
}

const char *
v34hs_text(int side)
{
	return text[side & 1];
}

/* --- the comparison ------------------------------------------------------- */

/*
 * The two self-pointers.  `v34handshakinit` aims +0xaa70 at the record at
 * +0xa97c (and mode 4 also aims +0xaa6c at +0xa94c), so the two sides hold
 * two different addresses and the check that means anything is "the same
 * offset from its own base", not "the same bytes" -- both records are nearly
 * all zero, so a swapped pair would pass a content comparison.
 */
static void
check_self_ptr(const char *what, unsigned off, long tag)
{
	long da = (long)((char *)peek_ptr(0, off) - (char *)&obj_a);
	long db = (long)((char *)peek_ptr(1, off) - (char *)obj_b);
	int ina = da >= 0 && da < (long)OBJ_SIZE;
	int inb = db >= 0 && db < (long)OBJ_SIZE;
	char msg[160];

	/*
	 * A pointer OUT of the object selects a table, and which table it
	 * selects is a content question this comparison cannot answer -- the
	 * two sides legitimately hold two addresses of two identical tables.
	 * What must match is whether it points into the object at all, and
	 * where, since that part is an offset and offsets are comparable.
	 */
	snprintf(msg, sizeof(msg), "%s +0x%04x: points into the object",
		 what, off);
	diff_eq_int(msg, ina, inb, tag);
	if (!ina || !inb)
		return;
	snprintf(msg, sizeof(msg), "%s +0x%04x: offset within it", what, off);
	diff_eq_int(msg, (int)da, (int)db, tag);
}

void
v34hs_compare(const char *what, long tag)
{
	const unsigned char *a = (const unsigned char *)&obj_a;
	unsigned i, k;
	int bad = 0;
	char msg[192];

	for (k = 0; k < NHOLES; k++)
		if (memcmp(a + holes[k], obj_b + holes[k], 4) != 0)
			saw_hole[k] = 1;

	for (i = 0; i < OBJ_SIZE; i++) {
		if (a[i] == obj_b[i] || in_hole(i))
			continue;
		bad++;
		if (getenv("V34HS_DIAG"))
			printf("  DIAG %s +0x%05x: %02x vs %02x (case %ld)\n",
			       what, i, a[i], obj_b[i], tag);
		if (bad <= 8) {
			snprintf(msg, sizeof(msg),
				 "%s: object byte at +0x%x (case %ld)",
				 what, i, tag);
			diff_eq_int(msg, a[i], obj_b[i], (long)i);
		}
	}
	diff_eq_int(what, bad, 0, tag);

	/*
	 * EVERY INTERIOR POINTER, BY OFFSET.  Half the holes hold addresses
	 * INTO the object -- the two sample-queue cursor pairs, the ten echo
	 * canceller pointers, the four shell contexts and the two message
	 * records -- and a skipped pointer is a hole a wrong offset walks
	 * straight through.  t_v34hshak.c checks two of them this way; every
	 * one is checked here, because a cursor one element out changes what
	 * the next step reads without changing a byte this comparison sees.
	 */
	for (k = 0; k < NHOLES; k++)
		check_self_ptr("v34handshak interior pointer", holes[k], tag);

	/* The blocks the object points out of are compared too, or a write
	 * through one of the skipped pointers is invisible. */
	snprintf(msg, sizeof(msg), "%s: shaping buffer", what);
	diff_eq_int(msg, memcmp(shaped_a, shaped_b, sizeof(shaped_a)) == 0,
		    1, tag);
	snprintf(msg, sizeof(msg), "%s: PCM receiver block", what);
	diff_eq_int(msg, memcmp(pcm_a, pcm_b, sizeof(pcm_a)) == 0, 1, tag);
	snprintf(msg, sizeof(msg), "%s: configuration block", what);
	diff_eq_int(msg, memcmp(cfg_a, cfg_b, sizeof(cfg_a)) == 0, 1, tag);
	snprintf(msg, sizeof(msg), "%s: session block", what);
	diff_eq_int(msg,
		    memcmp(sess_a, sess_b, SESS_PCM) == 0
		    && memcmp(sess_a + SESS_PCM + 4, sess_b + SESS_PCM + 4,
			      SESS_LEN - SESS_PCM - 4) == 0, 1, tag);

	/* And what each side observed, including the transcript. */
	snprintf(msg, sizeof(msg), "%s: bytes written", what);
	diff_eq_int(msg, obs[0].changed, obs[1].changed, tag);
	snprintf(msg, sizeof(msg), "%s: step signature", what);
	diff_eq_int(msg, obs[0].hash, obs[1].hash, tag);
	snprintf(msg, sizeof(msg), "%s: diagnostic lines", what);
	diff_eq_int(msg, obs[0].lines, obs[1].lines, tag);
	snprintf(msg, sizeof(msg), "%s: transcript", what);
	diff_eq_int(msg, strcmp(text[0], text[1]) == 0, 1, tag);
}
