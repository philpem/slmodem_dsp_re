/*
 * t_v8hs.c -- differential test of the handshake state machine.
 *
 * Two state variables, one per direction, so the sweep is over both: every
 * transmit state that has a body against every receive state that has one,
 * driven for enough blocks that the deadlines and counters actually expire.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v8.h"

extern unsigned int ref_dsplibs_debug_level;

extern int ref_v8handshak(struct v8 *v);
extern int ref_v8_txinit(struct v8 *v);
extern int ref_v8_rxinit(struct v8 *v);
extern void ref_v8_V21_Init(struct v8 *v, short ch, short ans);
extern void ref_v8_ansaminit(struct v8 *v);
extern void ref_initTxSequence(struct v8 *v);
extern int ref_V8Process(struct v8 *v, const short *in, short *out, int n);
extern void ref_v8_phase_rev_init(struct v8_phase_rev *pr);
extern void ref_v8_detectorinit(struct v8 *v, struct v8_detector *d,
				const short *t, short a3, short a4, short a5,
				short a6, short a7);

static const short det_tab[8] = { 15000, -9000, 14000, -8000,
				  -7000, 13000, -6000, 12000 };

static struct v8 obj_a, obj_b;
static struct v8_cm cm_a, cm_b;

static void
fill(void *p, size_t n, unsigned seed)
{
	unsigned char *b = p;
	size_t i;

	for (i = 0; i < n; i++) {
		seed = seed * 1103515245u + 12345u;
		b[i] = (unsigned char)(seed >> 16);
	}
}

/* A pointer into the object, compared as an offset then blanked. */
static void
normalise(size_t off)
{
	uintptr_t pa, pb;
	long da, db;

	memcpy(&pa, (char *)&obj_a + off, sizeof(pa));
	memcpy(&pb, (char *)&obj_b + off, sizeof(pb));
	da = (long)(pa - (uintptr_t)&obj_a);
	db = (long)(pb - (uintptr_t)&obj_b);
	diff_eq_int("pointer at +%ld", db, da, (long)off);
	memcpy((char *)&obj_a + off, &da, sizeof(da));
	memcpy((char *)&obj_b + off, &db, sizeof(db));
}

static void
blank_filters(void)
{
	obj_a.v21.a = obj_b.v21.a = 0;
	obj_a.v21.b = obj_b.v21.b = 0;
	obj_a.v21.c = obj_b.v21.c = 0;
	obj_a.v21.d = obj_b.v21.d = 0;
	obj_a.detector.table = obj_b.detector.table = 0;
	obj_a.cm = obj_b.cm = 0;
}

/*
 * Everything both objects need before the machine will run: a filled object
 * so that nothing untouched can pass by being zero on both sides, then the
 * initialisers that hand out the pointers and filters, then the CM.
 */
static void
setup(unsigned seed, unsigned char b0, unsigned char b1, unsigned char b2)
{
	int i;

	fill(&obj_a, sizeof(obj_a), seed);
	memcpy(&obj_b, &obj_a, sizeof(obj_a));

	memset(&cm_a, 0, sizeof(cm_a));
	cm_a.b0 = b0;
	cm_a.b1 = b1;
	cm_a.b2 = b2;
	cm_a.ext1[0] = 'G';
	memcpy(&cm_b, &cm_a, sizeof(cm_a));

	ref_v8_txinit(&obj_a);
	ref_v8_txinit(&obj_b);
	ref_v8_rxinit(&obj_a);
	ref_v8_rxinit(&obj_b);
	obj_a.fa42 = obj_b.fa42 = 16384;
	ref_v8_V21_Init(&obj_a, 1, 0);
	ref_v8_V21_Init(&obj_b, 1, 0);
	ref_v8_ansaminit(&obj_a);
	ref_v8_ansaminit(&obj_b);
	/*
	 * Both detectors need arming: the phase-reversal one takes its window
	 * size from a field that is fill pattern otherwise, and walks off the
	 * object.
	 */
	ref_v8_phase_rev_init(&obj_a.phase_rev);
	ref_v8_phase_rev_init(&obj_b.phase_rev);
	ref_v8_detectorinit(&obj_a, &obj_a.detector, det_tab,
			    0, 100, 50, 1500, 0);
	ref_v8_detectorinit(&obj_b, &obj_b.detector, det_tab,
			    0, 100, 50, 1500, 0);

	obj_a.cm = &cm_a;
	obj_b.cm = &cm_b;
	obj_a.tx_seq = &obj_a.seq[0];
	obj_b.tx_seq = &obj_b.seq[0];
	obj_a.seq_alt = &obj_a.seq[2];
	obj_b.seq_alt = &obj_b.seq[2];
	obj_a.seq_spare = &obj_a.seq[4];
	obj_b.seq_spare = &obj_b.seq[4];

	/*
	 * Real sequences in the buffers.  Without them v8_getbit sees a
	 * zero-length message set to repeat, resets, and recurses on nothing.
	 */
	ref_initTxSequence(&obj_a);
	initTxSequence(&obj_b);
	for (i = 0; i < 5; i++) {
		obj_a.seq[i] = obj_a.seq[0];
		obj_b.seq[i] = obj_b.seq[0];
	}
}

/* The whole object, with the pointers into it turned into offsets first. */
static void
compare(void)
{
	int i;

	normalise(offsetof(struct v8, tx_seq));
	normalise(offsetof(struct v8, seq_alt));
	normalise(offsetof(struct v8, seq_spare));
	normalise(offsetof(struct v8, tx_sym_a));
	normalise(offsetof(struct v8, tx_sym_b));
	normalise(offsetof(struct v8, tx_ring_base));
	normalise(offsetof(struct v8, tx_ring_half));
	normalise(offsetof(struct v8, rx) + offsetof(struct v8_rx, buf));
	blank_filters();

	for (i = 0; i < (int)sizeof(struct v8); i++)
		diff_eq_int("byte at +%ld", ((unsigned char *)&obj_b)[i],
			    ((unsigned char *)&obj_a)[i], (long)i);
}

/*
 * The handshake's ten diagnostics, one scenario per exit.
 *
 * Three of them are timeouts, and each is announced on the single block where
 * the counter reaches the deadline exactly -- so the scenario has to place it
 * there rather than past it.  The rest are the QCA1 verdicts, which need a
 * message in the spare buffer that will pass or fail the check on purpose.
 */
struct trace_case {
	const char	*name;
	short		f9d4, f9d6, f9d8;
	short		deadline_a, deadline_b;
	int		fe64;
	int		mode;
	int		qca1;		/* load the spare buffer with... */
	short		qca1_w;		/* ...this in words 1 and 4 */
	int		ansam;		/* set up the turn-round instead */
};

static const struct trace_case trace_cases[] = {
	{ "CM timeout",      6, 0x28, 0x29, 40, 60, 40, 0, 0, 0,     0 },
	{ "JM timeout",     23, 0x28, 0x29, 40, 60, 60, 0, 0, 0,     0 },
	{ "CJ timeout",     23, 0x28, 0x29, 40, 60, 60, 1, 0, 0,     0 },
	{ "ANSam timeout",   5, 0x19, 0x19, 40, 60, 40, 0, 0, 0,     0 },
	/*
	 * Once the ANSam wait times out the receive state moves on, so a
	 * second block never re-enters it.  This one starts past the deadline
	 * instead, which is the only way to see that the announcement does
	 * NOT repeat.
	 */
	{ "ANSam timeout, past",  5, 0x19, 0x19, 40, 60, 45, 0, 0, 0, 0 },
	{ "ANSam detected",  5, 0x19, 0x24, 40, 60,  0, 0, 0, 0,     1 },
	/*
	 * The two message words carry more than the shape bit, and the free
	 * bits have to differ or the trace reads the same whichever way round
	 * its arguments go: 0x0cb gives U_QTS the pattern 0101 and the LAPM
	 * bit a 1, and 0x187 gives the ANSpcm index a 3 rather than a 1.
	 */
	{ "QCA1a accepted",  5, 0x28, 0x29, 40, 60,  0, 0, 1, 0x0cb, 0 },
	{ "QCA1d accepted",  5, 0x28, 0x29, 40, 60,  0, 0, 1, 0x187, 0 },
	{ "QCA1 refused",    5, 0x28, 0x29, 40, 60,  0, 0, 1, 0x000, 0 }
};

static void
trace_setup(const struct trace_case *c, unsigned seed)
{
	setup(seed, 0x11, 0x40, 0x04);

	obj_a.f9d4 = obj_b.f9d4 = c->f9d4;
	obj_a.f9d6 = obj_b.f9d6 = c->f9d6;
	obj_a.f9d8 = obj_b.f9d8 = c->f9d8;
	obj_a.deadline_a = obj_b.deadline_a = c->deadline_a;
	obj_a.deadline_b = obj_b.deadline_b = c->deadline_b;
	obj_a.fe64 = obj_b.fe64 = c->fe64;
	obj_a.side = obj_b.side = c->mode;
	obj_a.f110 = obj_b.f110 = 40;
	obj_a.fdc4 = obj_b.fdc4 = 0;
	obj_a.fdd0 = obj_b.fdd0 = 0;
	obj_a.fdb6 = obj_b.fdb6 = 0;
	obj_a.v21_params.f06 = obj_b.v21_params.f06 = 0x20;
	obj_a.v21_params.f16 = obj_b.v21_params.f16 = 0;
	obj_a.v21_params.f24 = obj_b.v21_params.f24 = 5;
	obj_a.v21_params.f26 = obj_b.v21_params.f26 = 5;

	/*
	 * The transmit loop runs only while the queue has room, so the cases
	 * that want the receiver dispatched have to fill it first.
	 */
	if (c->f9d4 == 5) {
		obj_a.f21c = obj_b.f21c = 0x60;
		obj_a.fa3e = obj_b.fa3e = 0x60;
	} else {
		obj_a.f21c = obj_b.f21c = 0;
		obj_a.fa3e = obj_b.fa3e = 0x60;
	}

	if (c->ansam) {
		/*
		 * Past the settling count, with enough energy to have heard
		 * something and a JM to answer with.  The DFT is left switched
		 * off so that the energy reading is the one set here.
		 */
		obj_a.fdb6 = obj_b.fdb6 = 0x960;
		obj_a.fda0 = obj_b.fda0 = 0x200;
		obj_a.fdbe = obj_b.fdbe = 1;
		obj_a.rx.f8a = obj_b.rx.f8a = 0;
	}

	if (c->qca1) {
		short w = c->qca1_w;

		obj_a.f9d8 = obj_b.f9d8 = V8_HS_QCA1;
		obj_a.fdbc = obj_b.fdbc = 5;
		obj_a.seq_spare->word[1] = w;
		obj_a.seq_spare->word[2] = 0x3ff;
		obj_a.seq_spare->word[3] = 0x155;
		obj_a.seq_spare->word[4] = w;
		obj_a.seq_spare->word[5] = 0x3ff;
		*obj_b.seq_spare = *obj_a.seq_spare;
	}
}

static int
t_hs_trace(void)
{
	unsigned n = sizeof(trace_cases) / sizeof(trace_cases[0]);
	unsigned i, lvl;
	long lines = 0;
	int step;

	diff_begin("v8handshak: the trace");

	for (lvl = 1; lvl <= 3; lvl++) {
		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
		dsplib_debug_capture_on = 1;

		for (i = 0; i < n; i++) {
			trace_setup(&trace_cases[i], 7000u + i);

			for (step = 0; step < 2; step++) {
				int ra, rb;

				obj_a.v21_params.f18 =
					obj_b.v21_params.f18 = 10;
				obj_a.v21_params.f1a =
					obj_b.v21_params.f1a = 0x3ff;

				ra = ref_v8handshak(&obj_a);
				rb = v8handshak(&obj_b);
				diff_eq_int("return (case %ld)", rb, ra,
					    (long)(lvl * 16 + i));
			}
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		diff_eq_int("transcript matches (level %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
		if (getenv("DBGDIFF")
		    && strcmp(dsplib_debug_capture_text(0),
			      dsplib_debug_capture_text(1)) != 0) {
			const char *o = dsplib_debug_capture_text(0);
			const char *r = dsplib_debug_capture_text(1);
			int j = 0;

			while (o[j] && o[j] == r[j])
				j++;
			while (j > 0 && o[j - 1] != '\n')
				j--;
			printf("=== level %u: divergence at %d\n", lvl, j);
			printf("--- ours: %.400s\n", o + j);
			printf("--- ref : %.400s\n", r + j);
		}
		diff_eq_int("line counts match (level %ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), (long)lvl);
		if (lvl == 1)
			diff_eq_int("silent below the threshold",
				    (int)dsplib_debug_capture_lines(1), 0, 0);
		else
			lines += dsplib_debug_capture_lines(1);
	}

	diff_eq_int("diagnostics were captured (%ld lines)", lines > 15, 1,
		    lines);

	return diff_end();
}

int
main(void)
{
	/*
	 * The five states that have bodies.  An unknown state is deliberately
	 * NOT swept: the original's default path jumps back to re-read a
	 * counter that only this loop advances, so it spins forever.  That is
	 * faithful, and it means the test would hang rather than fail.
	 */
	static const short tx_states[] = { 5, 6, 23, 43, 45 };
	static const short rx_states[] = { 0x19, 0x20, 0x23, 0x28, 0x63, 0 };
	int rc = 0;
	unsigned t, r;
	int step, i;
	long ran = 0;
	long returns[4];

	memset(returns, 0, sizeof(returns));
	diff_begin("v8handshak");

	for (t = 0; t < sizeof(tx_states) / sizeof(tx_states[0]); t++) {
		for (r = 0; r < sizeof(rx_states) / sizeof(rx_states[0]); r++) {
			setup(4200u + t * 16 + r,
			      (unsigned char)(0x11 * (t + 1)),
			      (unsigned char)(0x22 * (r + 1)),
			      (unsigned char)(t & 1 ? 0x14 : 0x04));

			obj_a.f9d4 = obj_b.f9d4 = tx_states[t];
			obj_a.f9d6 = obj_b.f9d6 = rx_states[r];
			obj_a.f9d8 = obj_b.f9d8 = (short)(r & 1 ? 0x19 : 0x24);
			obj_a.fa3e = obj_b.fa3e = 0x60;
			obj_a.deadline_a = obj_b.deadline_a = 40;
			obj_a.deadline_b = obj_b.deadline_b = 60;
			obj_a.fe64 = obj_b.fe64 = 0;
			obj_a.f110 = obj_b.f110 = 8;
			obj_a.tone.f08 = obj_b.tone.f08 = 6000;
			obj_a.v21_params.f06 = obj_b.v21_params.f06 = 0x20;

			for (step = 0; step < 30; step++) {
				int ra = ref_v8handshak(&obj_a);
				int rb = v8handshak(&obj_b);

				diff_eq_int("return (%ld)", rb, ra,
					    (long)(t * 16 + r));
				if (ra >= 0 && ra < 4)
					returns[ra]++;
				ran++;
			}

			compare();
			diff_eq_int("menu after (%ld)",
				    memcmp(&cm_a, &cm_b, sizeof(cm_a)) == 0, 1,
				    (long)(t * 16 + r));
		}
	}

	diff_eq_int("the machine ran (%ld)", ran > 100, 1, ran);
	diff_eq_int("more than one return value (%ld)",
		    (returns[0] > 0) + (returns[1] > 0) + (returns[2] > 0) >= 2,
		    1, returns[0]);
	rc |= diff_end();

	/*
	 * The matching half, on its own.  It cannot be reached by driving the
	 * machine from a plausible start -- the receive state has to be 0x28
	 * and every sub-state below it wants a different shape of input -- so
	 * the states are constructed instead: bit count and shift register,
	 * the two block counters that gate the framing reset, the buffer
	 * indices, and the four combinations of `mode` and `fa48` that the
	 * end of a message dispatches on.
	 *
	 * `f16` is what decides whether the demodulator does anything: it
	 * takes four samples a block and only looks at them when it has
	 * twelve, so at 0 or 4 the seeded bit count survives the call and at 8
	 * the demodulator overwrites it.  Both are wanted.
	 */
	diff_begin("v8handshak matching");
	{
		static const short subs[] = {
			V8_HS_DRAIN, V8_HS_COLLECT, V8_HS_HUNT, V8_HS_CJ,
			V8_HS_QCA1, V8_HS_TAKEN_RX, V8_HS_TAKEN_TX
		};
		/* Both preambles, both markers, and a CJ octet. */
		static const short ftab[] = { 0xc0f, 0xd55, 0x00f, 0x155,
					      0x3ff, 0x001, 0x000, 0x2aa };
		unsigned si, c;
		int saw_armed = 0, saw_msg_done = 0, saw_qca1_ok = 0;
		int saw_qca1_bad = 0, saw_finished = 0, saw_swap = 0;

		for (si = 0; si < sizeof(subs) / sizeof(subs[0]); si++) {
			for (c = 0; c < 256; c++) {
				short sub = subs[si];
				short f1a = ftab[(c >> 4) & 7];
				short f18 = (c & 1) ? 10 : 9;
				short f16 = (c % 11) == 0 ? 8 : 0;
				short fdbc = (short)(1 + (c % 5) * 3);
				short fdb6 = (short)(c % 4);
				short fdb8 = ((c / 4) % 4) == 0
					     ? 0 : (short)(6 + (c / 4) % 4);
				short f24 = (c % 7) == 0 ? 1 : 5;
				short f26 = (c % 3) ? (short)(f24 - 2) : f24;
				short before8;

				setup(9000u + si * 256 + c, 0x11, 0x40,
				      (unsigned char)((c & 2) ? 0x14 : 0x04));

				/*
				 * Half the QCA1 cases get a message that will
				 * pass the check, and half the collecting
				 * cases a buffer that will accept the marker,
				 * because neither happens by chance.
				 */
				if ((c & 128) && sub == V8_HS_QCA1) {
					short w = (c & 2) ? 0x181 : 0x081;

					f1a = 0x3ff;
					f18 = 10;
					f16 = 0;
					fdbc = 5;
					obj_a.seq_spare->word[1] = w;
					obj_a.seq_spare->word[2] = 0x3ff;
					obj_a.seq_spare->word[3] = 0x155;
					obj_a.seq_spare->word[4] = w;
					*obj_b.seq_spare = *obj_a.seq_spare;
				} else if ((c & 128) && sub == V8_HS_COLLECT) {
					f1a = 0x00f;
					f18 = 10;
					f16 = 0;
					obj_a.seq_alt->wordidx = fdb6;
					obj_b.seq_alt->wordidx = fdb6;
				}

				obj_a.f9d4 = obj_b.f9d4 = 5;
				obj_a.f9d6 = obj_b.f9d6 = 0x28;
				obj_a.f9d8 = obj_b.f9d8 = sub;
				/* Queue already full: skip the transmit loop. */
				obj_a.f21c = obj_b.f21c = 0x60;
				obj_a.fa3e = obj_b.fa3e = 0x60;
				/*
				 * Enough symbols for all three passes: the
				 * AGC takes a block of four off the count
				 * every time it runs, and below six the
				 * receiver is not dispatched at all.
				 */
				obj_a.f110 = obj_b.f110 = 40;
				obj_a.side = obj_b.side = (int)((c >> 2) & 1);
				obj_a.op_mode = obj_b.op_mode = (int)((c >> 3) & 1);
				obj_a.fdc4 = obj_b.fdc4 = 0;
				obj_a.fdd0 = obj_b.fdd0 = 0;
				obj_a.fdb6 = obj_b.fdb6 = fdb6;
				obj_a.fdb8 = obj_b.fdb8 = fdb8;
				obj_a.fdbc = obj_b.fdbc = fdbc;
				obj_a.deadline_a = obj_b.deadline_a = 40;
				obj_a.deadline_b = obj_b.deadline_b = 60;
				obj_a.fe64 = obj_b.fe64 = 0;
				obj_a.v21_params.f06 =
					obj_b.v21_params.f06 = 0x20;
				obj_a.v21_params.f16 =
					obj_b.v21_params.f16 = f16;
				obj_a.v21_params.f24 =
					obj_b.v21_params.f24 = f24;
				obj_a.v21_params.f26 =
					obj_b.v21_params.f26 = f26;

				for (step = 0; step < 3; step++) {
					int ra, rb;

					/*
					 * Re-seed the character each pass:
					 * a match consumes it, and CJ needs
					 * two octets before it fires.
					 */
					obj_a.v21_params.f18 =
						obj_b.v21_params.f18 = f18;
					obj_a.v21_params.f1a =
						obj_b.v21_params.f1a = f1a;

					before8 = obj_a.f9d8;
					ra = ref_v8handshak(&obj_a);
					rb = v8handshak(&obj_b);
					diff_eq_int("return (%ld)", rb, ra,
						    (long)(si * 256 + c));

					if (ra == 2)
						saw_finished = 1;
					if (before8 == V8_HS_HUNT
					    && obj_a.f9d8 != V8_HS_HUNT)
						saw_armed = 1;
					if (before8 == V8_HS_QCA1
					    && obj_a.fdc4 != 0)
						saw_qca1_ok = 1;
					if (before8 == V8_HS_QCA1
					    && obj_a.f9d8 == V8_HS_HUNT)
						saw_qca1_bad = 1;
					if (before8 == V8_HS_COLLECT
					    && obj_a.f9d8 != V8_HS_COLLECT)
						saw_msg_done = 1;
					if (before8 == V8_HS_DRAIN
					    && obj_a.tx_seq == &obj_a.seq[1])
						saw_swap = 1;
				}

				compare();
			}
		}

		/*
		 * Coverage, not correctness: a run in which none of these
		 * fired would compare two objects that never went anywhere.
		 */
		diff_eq_int("a preamble armed a buffer (%ld)", saw_armed, 1, 0);
		diff_eq_int("a message was taken (%ld)", saw_msg_done, 1, 0);
		diff_eq_int("a QCA1 message passed (%ld)", saw_qca1_ok, 1, 0);
		diff_eq_int("a QCA1 message failed (%ld)", saw_qca1_bad, 1, 0);
		diff_eq_int("the drain swapped buffers (%ld)", saw_swap, 1, 0);
		diff_eq_int("the handshake finished (%ld)", saw_finished, 1, 0);
	}
	rc |= diff_end();

	/* And the per-buffer loop on top of it. */
	diff_begin("V8Process");
	{
		static short air[160], out_a[160], out_b[160];
		long statuses[20];
		int st2, n2;

		memset(statuses, 0, sizeof(statuses));
		for (t = 0; t < sizeof(tx_states) / sizeof(tx_states[0]); t++) {
			for (r = 0; r < sizeof(rx_states) / sizeof(rx_states[0]);
			     r++) {
				setup(7700u + t * 8 + r, 0, 0x40, 0x04);
				obj_a.f9d4 = obj_b.f9d4 = tx_states[t];
				obj_a.f9d6 = obj_b.f9d6 = rx_states[r];
				obj_a.f9d8 = obj_b.f9d8 =
					(short)(r & 1 ? 0x19 : 0x24);
				obj_a.fa3e = obj_b.fa3e = 0x60;
				obj_a.side = obj_b.side = (int)(t & 1);
				obj_a.deadline_a = obj_b.deadline_a = 40;
				obj_a.deadline_b = obj_b.deadline_b = 60;
				obj_a.fe64 = obj_b.fe64 = 0;
				obj_a.tone.f08 = obj_b.tone.f08 = 6000;
				obj_a.v21_params.f06 = obj_b.v21_params.f06 =
					0x20;

				for (i = 0; i < 160; i++)
					air[i] = (short)((i * 811) % 9001
							 - 4500);

				for (step = 0; step < 6; step++) {
					int sa, sb;

					memset(out_a, 0x5a, sizeof(out_a));
					memset(out_b, 0x5a, sizeof(out_b));
					sa = ref_V8Process(&obj_a, air, out_a,
							   160);
					sb = V8Process(&obj_b, air, out_b,
						       160);
					diff_eq_int("status (%ld)", sb, sa,
						    (long)(t * 8 + r));
					for (n2 = 0; n2 < 160; n2++)
						diff_eq_int("sample %ld",
							    out_b[n2],
							    out_a[n2], n2);
					if (sa >= 0 && sa < 20)
						statuses[sa]++;
				}

				compare();
			}
		}
		st2 = 0;
		for (i = 0; i < 20; i++)
			if (statuses[i])
				st2++;
		diff_eq_int("several statuses were reported (%ld)", st2 >= 2,
			    1, st2);
	}
	rc |= diff_end();

	rc |= t_hs_trace();
	return rc;
}
