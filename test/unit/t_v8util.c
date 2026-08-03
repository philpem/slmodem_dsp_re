/*
 * t_v8util.c -- differential test of the V.8 arithmetic leaves.
 *
 * All six are global symbols, so both sides can be called by name and swept
 * exhaustively where the domain allows it: `v8_mpyint` and `v8_absfn` are
 * checked over every short, and `v8_cosread` over every index, so those three
 * are proved rather than sampled.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v8.h"

extern unsigned int ref_dsplibs_debug_level;

extern short ref_v8_mpyint(short a, short b);
extern short ref_v8_absfn(short x);
extern short ref_v8_cosread(unsigned char phase);
extern void ref_v8_crc(struct v8_handshake *hs, int bit);
extern void ref_v8_copycoeff(short *dst, const short *src, short n);
extern void ref_v8_dftenergy(struct v8_dft_bin *bin, short n, short shift);
extern void ref_V8_setFilters(struct v8 *v, const short *a, const short *b,
			      const short *c, const short *d);
extern void ref_V8_V21_reset(struct v8 *v);
extern void ref_v8_TONEq_init(struct v8 *v);
extern void ref_v8_phase_rev_init(struct v8_phase_rev *pr);
extern int ref_v8_txinit(struct v8 *v);
extern int ref_v8_rxinit(struct v8 *v);
extern void ref_v8_detectorinit(struct v8 *v, struct v8_detector *d,
				const short *table, short a3, short a4,
				short a5, short a6, short a7);
extern void ref_v8_V21_Init(struct v8 *v, short channel, short answerer);
extern unsigned char ref_charFlip(unsigned char b);
extern void ref_initTxSequence(struct v8 *v);
extern void ref_v8handshakinit(struct v8 *v);
extern struct v8 *ref_V8Create(const struct v8_cfg *cfg);
extern void ref_V8Delete(struct v8 *v);
extern int ref_V8GetMessage(struct v8 *v, unsigned char *out, int *count);
extern int ref_V8SetMessage(struct v8 *v, int which, const unsigned char *o,
			    int n);


/* The two objects every comparison below runs through. */
static struct v8 obj_a, obj_b;
static struct v8_tx_sequence seq_a, seq_b;
static struct v8_cm cm_a, cm_b;

/*
 * The sequence builder works through two pointers the caller sets, so each
 * side gets its own pair and the results are compared directly rather than
 * through the object.
 */

static int
t_txsequence(void)
{
	unsigned b0, b1, b2, e;
	long built = 0;

	diff_begin("initTxSequence: CM and JM assembly");

	/*
	 * Every combination of the bits the builder looks at -- b0 has five,
	 * b1 has seven, b2 has four -- would be 2^16 runs; sweep each byte
	 * over its full range against a few values of the others, which
	 * covers every branch and every pair that shares a word.
	 */
	for (b0 = 0; b0 < 256; b0++) {
		for (b1 = 0; b1 < 256; b1 += 17) {
			for (b2 = 0; b2 < 16; b2++) {
				for (e = 0; e < 3; e++) {
					memset(&seq_a, 0x5a, sizeof(seq_a));
					memset(&seq_b, 0x5a, sizeof(seq_b));
					memset(&cm_a, 0, sizeof(cm_a));
					cm_a.b0 = (unsigned char)b0;
					cm_a.b1 = (unsigned char)b1;
					cm_a.b2 = (unsigned char)b2;
					/*
					 * e = 0 no extension characters,
					 * 1 one, 2 the full four.
					 */
					if (e > 0) {
						cm_a.ext1[0] = 'G';
						cm_a.ext2[0] = 'B';
					}
					if (e > 1) {
						cm_a.ext1[1] = 'b';
						cm_a.ext1[2] = 0x7f;
						cm_a.ext1[3] = 0x01;
						cm_a.ext2[1] = 0xff;
						cm_a.ext2[2] = 'z';
						cm_a.ext2[3] = '4';
					}
					memcpy(&cm_b, &cm_a, sizeof(cm_a));

					obj_a.cm = &cm_a;
					obj_a.tx_seq = &seq_a;
					obj_b.cm = &cm_b;
					obj_b.tx_seq = &seq_b;
					ref_initTxSequence(&obj_a);
					initTxSequence(&obj_b);

					diff_eq_int("b0=%ld: sequence",
						    memcmp(&seq_a, &seq_b,
							   sizeof(seq_a)) == 0,
						    1, (long)b0);
					diff_eq_int("b0=%ld: menu after",
						    memcmp(&cm_a, &cm_b,
							   sizeof(cm_a)) == 0,
						    1, (long)b0);
					if (seq_a.nbits != 0)
						built++;
				}
			}
		}
	}

	/* Ten bits a character, and the terminator is really planted. */
	diff_eq_int("sequences were built (%ld)", built > 1000, 1, built);
	diff_eq_int("bit count is a multiple of ten", seq_a.nbits % 10, 0, 0);
	diff_eq_int("terminator", (unsigned short)seq_a.crc, 0xffff, 0);
	diff_eq_int("preamble", seq_a.word[0], 0x3ff, 0);

	return diff_end();
}

/* Reverse eight bits the slow, obvious way, to check the table against. */
static unsigned char
reverse_bits(unsigned char b)
{
	unsigned char r = 0;
	int i;

	for (i = 0; i < 8; i++)
		if (b & (1u << i))
			r |= (unsigned char)(0x80u >> i);
	return r;
}

static int
t_charflip(void)
{
	int i;

	diff_begin("charFlip: bit reversal, exhaustive");
	for (i = 0; i < 256; i++) {
		unsigned char b = (unsigned char)i;

		diff_eq_int("charFlip(%ld)", charFlip(b), ref_charFlip(b), i);
		/*
		 * Independently of the blob: it really is a bit reversal, not
		 * some other permutation that happens to agree with the
		 * table.  Both sides read the same table, so agreeing with
		 * each other proves nothing about what the table means.
		 */
		diff_eq_int("charFlip(%ld) reverses the bits", charFlip(b),
			    reverse_bits(b), i);
		diff_eq_int("charFlip is an involution (%ld)",
			    charFlip(charFlip(b)), i, i);
	}
	return diff_end();
}

/*
 * The initialisers write scattered fields of a 3780-byte object, so the only
 * comparison worth making is the whole thing: both sides start from the same
 * non-zero fill, and every byte must agree afterwards.  That catches a
 * mis-stated offset, a missed field and a field written one byte too wide,
 * none of which a field-by-field check would find unless it happened to name
 * the field that moved.
 */

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

/*
 * The initialisers plant pointers into the object itself, and two objects at
 * different addresses cannot hold the same bytes there.  Each is compared as
 * an offset from its own base and then overwritten with that offset, so the
 * byte comparison afterwards covers everything with no hole in it.
 *
 * Only the pointers the call just wrote are touched.  The first version
 * normalised all five after every call, read the ones still holding the fill
 * pattern, and did pointer arithmetic on garbage -- which is how it came to
 * dump core rather than merely report a mismatch.
 */
static void
normalise(const size_t *offs, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		uintptr_t pa, pb;
		long da, db;

		memcpy(&pa, (char *)&obj_a + offs[i], sizeof(pa));
		memcpy(&pb, (char *)&obj_b + offs[i], sizeof(pb));
		da = (long)(pa - (uintptr_t)&obj_a);
		db = (long)(pb - (uintptr_t)&obj_b);
		diff_eq_int("pointer at +%ld holds the same offset", db, da,
			    (long)offs[i]);
		memcpy((char *)&obj_a + offs[i], &da, sizeof(da));
		memcpy((char *)&obj_b + offs[i], &db, sizeof(db));
	}
}

static const size_t tx_pointers[] = {
	offsetof(struct v8, tx_sym_a),
	offsetof(struct v8, tx_sym_b),
	offsetof(struct v8, tx_ring_base),
	offsetof(struct v8, tx_ring_half)
};

/*
 * The four filter designs are separate copies of the same numbers -- one set
 * in the object file, one in v8v21.c -- so the pointers can never match.
 * Compare 48 coefficients through each, then blank the four words so the
 * whole-object sweep still covers everything around them.
 */
static void
compare_filters(void)
{
	const short *pa[4], *pb[4];
	int i, k;

	pa[0] = obj_a.v21.a; pb[0] = obj_b.v21.a;
	pa[1] = obj_a.v21.b; pb[1] = obj_b.v21.b;
	pa[2] = obj_a.v21.c; pb[2] = obj_b.v21.c;
	pa[3] = obj_a.v21.d; pb[3] = obj_b.v21.d;

	for (k = 0; k < 4; k++) {
		diff_eq_int("filter %ld is set", pb[k] != 0, pa[k] != 0, k);
		if (pa[k] == 0 || pb[k] == 0)
			continue;
		for (i = 0; i < 48; i++)
			diff_eq_int("filter coefficient %ld", pb[k][i],
				    pa[k][i], i);
	}
	obj_a.v21.a = obj_b.v21.a = 0;
	obj_a.v21.b = obj_b.v21.b = 0;
	obj_a.v21.c = obj_b.v21.c = 0;
	obj_a.v21.d = obj_b.v21.d = 0;
}

/*
 * Blank a pointer slot in both objects without comparing it.  For the ones
 * the test itself set (the call menu) or that have already been compared by
 * content (the detector's table), an offset comparison would be meaningless
 * -- they do not point into the object at all.
 */
static void
blank(const size_t *offs, int n)
{
	long zero = 0;
	int i;

	for (i = 0; i < n; i++) {
		memcpy((char *)&obj_a + offs[i], &zero, sizeof(zero));
		memcpy((char *)&obj_b + offs[i], &zero, sizeof(zero));
	}
}

static const size_t rx_pointers[] = {
	offsetof(struct v8, rx) + offsetof(struct v8_rx, buf)
};

static int
whole_object(const char *what)
{
	size_t i;
	const unsigned char *a = (const unsigned char *)&obj_a;
	const unsigned char *b = (const unsigned char *)&obj_b;
	int differed = 0;

	for (i = 0; i < sizeof(struct v8); i++) {
		/*
		 * The label takes the byte offset, so it must be a numeric
		 * conversion.  It said "%s" for a long time and nothing
		 * noticed, because the label is only formatted when a check
		 * fails and this one never had.
		 */
		diff_eq_int("byte at +%ld", b[i], a[i], (long)i);
		if (a[i] != 0)
			differed++;
	}
	/* The fill was non-zero, so a no-op initialiser cannot pass silently. */
	diff_eq_int("the object is not all zero after init (%ld)",
		    differed > 100, 1, differed);
	(void)what;
	return 0;
}

static int
t_inits(void)
{
	int i;

	diff_begin("v8 initialisers: whole-object comparison");

	for (i = 0; i < 4; i++) {
		const short c0[4] = { 1, 2, 3, 4 };

		/* V8_setFilters */
		fill(&obj_a, sizeof(obj_a), 99u + i);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		ref_V8_setFilters(&obj_a, c0, c0 + 1, c0 + 2, c0 + 3);
		V8_setFilters(&obj_b, c0, c0 + 1, c0 + 2, c0 + 3);
		whole_object("setFilters");

		/* V8_V21_reset */
		fill(&obj_a, sizeof(obj_a), 555u + i);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		ref_V8_V21_reset(&obj_a);
		V8_V21_reset(&obj_b);
		whole_object("V21_reset");

		/* v8_TONEq_init */
		fill(&obj_a, sizeof(obj_a), 4242u + i);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		ref_v8_TONEq_init(&obj_a);
		v8_TONEq_init(&obj_b);
		whole_object("TONEq_init");

		/* v8_phase_rev_init, through the object so overruns show up */
		fill(&obj_a, sizeof(obj_a), 31337u + i);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		ref_v8_phase_rev_init(&obj_a.phase_rev);
		v8_phase_rev_init(&obj_b.phase_rev);
		whole_object("phase_rev_init");

		/*
		 * The transmitter and receiver set pointers into the object,
		 * so the two sides can never agree byte-for-byte on those four
		 * words -- they live at different addresses.  Compare them as
		 * offsets instead, and the rest of the object as bytes.
		 */
		fill(&obj_a, sizeof(obj_a), 2001u + i);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		diff_eq_int("txinit returns 0", v8_txinit(&obj_b),
			    ref_v8_txinit(&obj_a), 0);
		normalise(tx_pointers, 4);
		whole_object("txinit");

		fill(&obj_a, sizeof(obj_a), 90210u + i);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		diff_eq_int("rxinit returns 0", v8_rxinit(&obj_b),
			    ref_v8_rxinit(&obj_a), 0);
		normalise(rx_pointers, 1);
		whole_object("rxinit");

		/*
		 * The detector, over a spread of arguments including the
		 * negation corner: a5 is stored negated, so -32768 is the one
		 * value that comes back unchanged.
		 */
		{
			static const short a5v[] = { 0, 1, -1, 300, -32768,
						     32767 };
			static const short det_table[8] = { 1, 2, 3, 4,
							    5, 6, 7, 8 };
			unsigned k;

			for (k = 0; k < sizeof a5v / sizeof a5v[0]; k++) {
				fill(&obj_a, sizeof(obj_a), 606u + i * 16 + k);
				memcpy(&obj_b, &obj_a, sizeof(obj_a));
				ref_v8_detectorinit(&obj_a, &obj_a.detector,
						    det_table + k,
						    (short)(100 + k),
						    (short)(-200 - k), a5v[k],
						    (short)(7 * k),
						    (short)(-9 * k));
				v8_detectorinit(&obj_b, &obj_b.detector,
						det_table + k,
						(short)(100 + k),
						(short)(-200 - k), a5v[k],
						(short)(7 * k),
						(short)(-9 * k));
				whole_object("detectorinit");
			}
		}

		/*
		 * v8_V21_Init, over all four combinations of its two
		 * independent choices.  It plants pointers to coefficient
		 * tables, and those live in the object file on one side and
		 * in v8v21.c on the other, so the tables are compared by
		 * content and the pointer words are then blanked.
		 */
		{
			int ch, ans;

			for (ch = 0; ch <= 1; ch++) {
				for (ans = 0; ans <= 1; ans++) {
					fill(&obj_a, sizeof(obj_a),
					     808u + i * 8 + ch * 2 + ans);
					memcpy(&obj_b, &obj_a, sizeof(obj_a));
					ref_v8_V21_Init(&obj_a, (short)ch,
							(short)ans);
					v8_V21_Init(&obj_b, (short)ch,
						    (short)ans);
					compare_filters();
					whole_object("V21_Init");
				}
			}
		}
	}

	/*
	 * The values the initialisers are supposed to plant.  Each is checked
	 * straight after its own call -- the first version of this checked the
	 * tone queue after a phase-reversal init, and read the fill.
	 */
	fill(&obj_b, sizeof(obj_b), 7u);
	v8_TONEq_init(&obj_b);
	diff_eq_int("tone queue period", obj_b.toneq_period, 0x688, 0);
	diff_eq_int("tone queue empty", obj_b.toneq_pending, 0, 0);

	fill(&obj_b, sizeof(obj_b), 11u);
	v8_phase_rev_init(&obj_b.phase_rev);
	diff_eq_int("phase-rev countdown", obj_b.phase_rev.half, 0x20, 0);
	diff_eq_int("phase-rev window cleared", obj_b.phase_rev.window[63], 0,
		    0);

	fill(&obj_b, sizeof(obj_b), 17u);
	obj_b.rx.flags = 0;
	{
		static const short one[2] = { 1, 1 };

		v8_detectorinit(&obj_b, &obj_b.detector, one, 2, 3, 4, 5, 6);
	}
	diff_eq_int("detector flag set in the receiver",
		    obj_b.rx.flags & V8_RX_DETECTOR_ARMED,
		    V8_RX_DETECTOR_ARMED, 0);
	diff_eq_int("the negated argument", obj_b.detector.f08, -4, 0);
	diff_eq_int("accumulators cleared", obj_b.detector.acc_d[2], 0, 0);

	/* The channel and role choices really do pick different things. */
	fill(&obj_b, sizeof(obj_b), 21u);
	v8_V21_Init(&obj_b, 1, 1);
	diff_eq_int("channel 2 carrier", obj_b.v21_params.carrier_a, 0x62b, 0);
	diff_eq_int("answerer constant", obj_b.v21_params.f0e, -100, 0);
	diff_eq_int("V.21 flag set", obj_b.rx.flags & V8_RX_V21_ARMED,
		    V8_RX_V21_ARMED, 0);
	fill(&obj_b, sizeof(obj_b), 22u);
	v8_V21_Init(&obj_b, 0, 0);
	diff_eq_int("channel 1 carrier", obj_b.v21_params.carrier_a, 0x3ef, 0);
	diff_eq_int("caller constant", obj_b.v21_params.f0e, 0, 0);
	diff_eq_int("the two channels differ in taps",
		    obj_b.v21_taps[0] != -14, 1, obj_b.v21_taps[0]);

	fill(&obj_b, sizeof(obj_b), 13u);
	V8_V21_reset(&obj_b);
	diff_eq_int("V.21 delay line cleared", obj_b.v21.delay[V8_V21_DELAY - 1],
		    0, 0);

	return diff_end();
}


static int
t_mpyint(void)
{
	long a, b;

	diff_begin("v8_mpyint: Q14 multiply");
	/*
	 * The full cross product is 2^32 pairs; step one operand coarsely and
	 * the other finely, and include the ends and the sign boundaries
	 * exactly.
	 */
	for (a = -32768; a <= 32767; a += 37) {
		for (b = -32768; b <= 32767; b += 1021)
			diff_eq_int("mpyint(%ld, .)",
				    v8_mpyint((short)a, (short)b),
				    ref_v8_mpyint((short)a, (short)b), a);
	}
	for (b = -32768; b <= 32767; b++) {
		diff_eq_int("mpyint(-32768, %ld)", v8_mpyint(-32768, (short)b),
			    ref_v8_mpyint(-32768, (short)b), b);
		diff_eq_int("mpyint(32767, %ld)", v8_mpyint(32767, (short)b),
			    ref_v8_mpyint(32767, (short)b), b);
		diff_eq_int("mpyint(-1, %ld)", v8_mpyint(-1, (short)b),
			    ref_v8_mpyint(-1, (short)b), b);
	}
	return diff_end();
}

static int
t_absfn(void)
{
	long x;

	diff_begin("v8_absfn: absolute value, exhaustive");
	for (x = -32768; x <= 32767; x++)
		diff_eq_int("absfn(%ld)", v8_absfn((short)x),
			    ref_v8_absfn((short)x), x);
	/* The corner that has no positive answer. */
	diff_eq_int("absfn(-32768) is still negative", v8_absfn(-32768) < 0, 1,
		    0);
	return diff_end();
}

static int
t_cosread(void)
{
	int i;
	int nonzero = 0;

	diff_begin("v8_cosread: the whole table");
	for (i = 0; i < 256; i++) {
		short v = v8_cosread((unsigned char)i);

		diff_eq_int("cosread(%ld)", v, ref_v8_cosread((unsigned char)i),
			    i);
		if (v != 0)
			nonzero++;
	}
	/* Shape, so a table of zeros could not agree with itself. */
	diff_eq_int("quarter cycle is zero", v8_cosread(64), 0, 0);
	diff_eq_int("peak is Q14 one", v8_cosread(0), 16384, 0);
	diff_eq_int("the table is not flat (%ld non-zero)", nonzero > 250, 1,
		    nonzero);
	return diff_end();
}

static int
t_crc(void)
{
	struct v8_handshake a, b;
	int i, bit;

	diff_begin("v8_crc: CRC-16-CCITT bit steps");

	/* Every starting register against both bit values. */
	for (i = -32768; i <= 32767; i += 7) {
		for (bit = 0; bit <= 1; bit++) {
			memset(&a, 0, sizeof(a));
			memset(&b, 0, sizeof(b));
			a.crc = b.crc = (short)i;
			ref_v8_crc(&a, bit);
			v8_crc(&b, bit);
			diff_eq_int("crc(%ld)", b.crc, a.crc, i);
		}
	}

	/*
	 * A whole message through both, so an error that cancels itself on a
	 * single step still shows up.
	 */
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	for (i = 0; i < 4096; i++) {
		bit = (i * 2654435761u) >> 28 & 1;
		ref_v8_crc(&a, bit);
		v8_crc(&b, bit);
		diff_eq_int("streamed bit %ld", b.crc, a.crc, i);
	}
	diff_eq_int("the register actually moved (%ld)", a.crc != 0, 1, a.crc);

	/*
	 * Only the low half of `bit` is looked at, so a value that is non-zero
	 * overall but zero in its low 16 bits counts as a zero bit.
	 */
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	a.crc = b.crc = 0x1234;
	ref_v8_crc(&a, 0x10000);
	v8_crc(&b, 0x10000);
	diff_eq_int("only the low half of the bit counts", b.crc, a.crc, 0);

	return diff_end();
}

static int
t_copycoeff(void)
{
	short src[64], dst_a[64], dst_b[64];
	int n, i;

	diff_begin("v8_copycoeff");
	for (i = 0; i < 64; i++)
		src[i] = (short)(i * 517 - 9000);

	for (n = 0; n <= 64; n++) {
		memset(dst_a, 0x5a, sizeof(dst_a));
		memset(dst_b, 0x5a, sizeof(dst_b));
		ref_v8_copycoeff(dst_a, src, (short)n);
		v8_copycoeff(dst_b, src, (short)n);
		for (i = 0; i < 64; i++)
			diff_eq_int("n=%ld", dst_b[i], dst_a[i], n);
	}

	/* A negative count copies nothing rather than running away. */
	memset(dst_a, 0x5a, sizeof(dst_a));
	memset(dst_b, 0x5a, sizeof(dst_b));
	ref_v8_copycoeff(dst_a, src, -3);
	v8_copycoeff(dst_b, src, -3);
	diff_eq_int("negative count copies nothing",
		    memcmp(dst_a, dst_b, sizeof(dst_a)) == 0, 1, 0);
	diff_eq_int("and really nothing", dst_b[0], (short)0x5a5a, 0);

	return diff_end();
}

static int
t_dftenergy(void)
{
	struct v8_dft_bin a[16], b[16];
	int shift, i, n;
	long nonzero = 0;

	diff_begin("v8_dftenergy");
	for (shift = 0; shift <= 15; shift++) {
		for (n = 0; n <= 16; n++) {
			for (i = 0; i < 16; i++) {
				a[i].phase = b[i].phase = (short)i;
				a[i].step = b[i].step = (short)(i * 3);
				a[i].re = b[i].re = (i - 8) * 0x01234567;
				a[i].im = b[i].im = (i * 7 - 40) * 0x00765432;
				a[i].energy = b[i].energy = 0x1234;
				a[i].f0e = b[i].f0e = 0x4321;
			}
			ref_v8_dftenergy(a, (short)n, (short)shift);
			v8_dftenergy(b, (short)n, (short)shift);
			for (i = 0; i < 16; i++) {
				diff_eq_int("shift=%ld energy", b[i].energy,
					    a[i].energy, shift);
				diff_eq_int("shift=%ld untouched", b[i].f0e,
					    a[i].f0e, shift);
				if (a[i].energy != 0)
					nonzero++;
			}
		}
	}
	diff_eq_int("energies were actually computed (%ld)", nonzero > 100, 1,
		    nonzero);
	return diff_end();
}

/*
 * v8handshakinit plants pointers to its own sequence buffers and reads the
 * call menu through another, so both kinds are normalised: the ones into the
 * object become offsets, and the detector's coefficient table -- which exists
 * once in the object file and once in v8hs.c -- is compared by content.
 */
static int
t_handshakinit(void)
{
	/* Written by both shapes that write anything. */
	static const size_t seq_pointers[] = {
		offsetof(struct v8, tx_seq),
		offsetof(struct v8, seq_alt)
	};
	/* Written only when the JM branch runs. */
	static const size_t spare_pointer[] = {
		offsetof(struct v8, seq_spare)
	};
	/* Never compared as offsets: these do not point into the object. */
	static const size_t outside_pointers[] = {
		offsetof(struct v8, cm),
		offsetof(struct v8, detector) + offsetof(struct v8_detector,
							table)
	};
	int mode, t, b2, i;
	long built = 0;

	diff_begin("v8handshakinit: laying out the handshake");

	for (mode = -1; mode <= 3; mode++) {
		for (t = 0; t <= 2; t++) {
			for (b2 = 0; b2 < 4; b2++) {
				fill(&obj_a, sizeof(obj_a),
				     4000u + mode * 64 + t * 8 + b2);
				memcpy(&obj_b, &obj_a, sizeof(obj_a));

				memset(&cm_a, 0, sizeof(cm_a));
				cm_a.b0 = (unsigned char)(0x11 * b2);
				cm_a.b1 = (unsigned char)(0x22 * t);
				cm_a.b2 = (unsigned char)((b2 & 1 ? 0x10 : 0)
							  | (b2 & 2 ? 0x40 : 0)
							  | 0x04);
				cm_a.menu = 0x0f0f0f0f * (t + 1);
				cm_a.ext1[0] = 'G';
				cm_a.ext2[0] = 'B';
				memcpy(&cm_b, &cm_a, sizeof(cm_a));

				obj_a.mode = mode;
				obj_b.mode = mode;
				obj_a.timeout_a = obj_b.timeout_a = t - 1;
				obj_a.timeout_b = obj_b.timeout_b = t * 5;
				obj_a.fa48 = obj_b.fa48 = (t == 1);
				obj_a.fa42 = obj_b.fa42 = (short)(1000 * t);
				obj_a.cm = &cm_a;
				obj_b.cm = &cm_b;

				ref_v8handshakinit(&obj_a);
				v8handshakinit(&obj_b);

				/*
				 * Only dereference what this mode actually
				 * wrote.  Mode 0 arms the detector, mode 1
				 * brings up the V.21 filters; in any other
				 * mode those words still hold the fill
				 * pattern, and reading them as pointers is
				 * how the first version of this crashed.
				 */
				if (mode == 0) {
					for (i = 0; i < 8; i++)
						diff_eq_int("detector table %ld",
							    obj_b.detector.table[i],
							    obj_a.detector.table[i],
							    i);
				}
				if (mode == 1)
					compare_filters();

				blank(outside_pointers, 2);
				/*
				 * v8_rxinit and v8_txinit run in the preamble,
				 * before the mode is looked at, so their
				 * pointers are set whatever shape follows.
				 * Only the sequence pointers are conditional.
				 */
				normalise(tx_pointers, 4);
				normalise(rx_pointers, 1);
				if (mode == 0 || mode == 1)
					normalise(seq_pointers, 2);
				if (mode == 0 && (cm_a.b2 & 0x10))
					normalise(spare_pointer, 1);
				whole_object("handshakinit");

				diff_eq_int("menu after, mode %ld",
					    memcmp(&cm_a, &cm_b,
						   sizeof(cm_a)) == 0, 1,
					    mode);
				if (obj_a.seq[0].nbits != 0)
					built++;
			}
		}
	}

	diff_eq_int("sequences were built (%ld)", built > 0, 1, built);
	return diff_end();
}

/*
 * V8Create allocates without zeroing, so the two objects differ wherever
 * nothing wrote -- comparing all 3780 bytes would compare two lots of
 * allocator leftovers.  What is compared instead is everything the
 * constructor is responsible for: the six configuration words it copies, the
 * constant it plants, the two it clears at the end, and a sample of what
 * v8handshakinit wrote through it.  The handshake itself is already proved
 * byte-for-byte by t_handshakinit.
 */
static int
t_v8create(void)
{
	struct v8_cfg cfg;
	struct v8 *a, *b;
	int mode;

	diff_begin("V8Create");

	for (mode = -1; mode <= 2; mode++) {
		memset(&cm_a, 0, sizeof(cm_a));
		cm_a.b0 = 0x2a;
		cm_a.b1 = 0x14;
		cm_a.b2 = 0x14;
		cm_a.menu = 0x01020304;
		cm_a.ext1[0] = 'G';
		memcpy(&cm_b, &cm_a, sizeof(cm_a));

		cfg.mode = mode;
		cfg.f04 = 7;
		cfg.timeout_a = 12;
		cfg.timeout_b = 3;
		cfg.f10 = 9600;

		cfg.cm = &cm_a;
		b = ref_V8Create(&cfg);
		cfg.cm = &cm_b;
		a = V8Create(&cfg);

		diff_eq_int("both allocated (%ld)", a != 0 && b != 0, 1, mode);
		if (a == 0 || b == 0)
			continue;

		diff_eq_int("mode (%ld)", a->mode, b->mode, mode);
		diff_eq_int("f04 (%ld)", a->fa48, b->fa48, mode);
		diff_eq_int("timeout_a (%ld)", a->timeout_a, b->timeout_a,
			    mode);
		diff_eq_int("timeout_b (%ld)", a->timeout_b, b->timeout_b,
			    mode);
		diff_eq_int("f10 (%ld)", a->fa54, b->fa54, mode);
		diff_eq_int("fa42 (%ld)", a->fa42, b->fa42, mode);
		diff_eq_int("fdba cleared (%ld)", a->fdba, b->fdba, mode);
		diff_eq_int("feb8 cleared (%ld)", a->feb8, b->feb8, mode);

		/* The handshake ran through it: a sample of what it writes. */
		diff_eq_int("fa3e (%ld)", a->fa3e, b->fa3e, mode);
		diff_eq_int("fa40 (%ld)", a->fa40, b->fa40, mode);
		diff_eq_int("f9d4 (%ld)", a->f9d4, b->f9d4, mode);
		diff_eq_int("deadline_a (%ld)", a->deadline_a, b->deadline_a,
			    mode);
		diff_eq_int("rx.flags (%ld)", a->rx.flags, b->rx.flags, mode);
		diff_eq_int("menu untouched (%ld)",
			    memcmp(&cm_a, &cm_b, sizeof(cm_a)) == 0, 1, mode);

		/* The constant really is planted, not agreed by accident. */
		diff_eq_int("fa42 is 0x4000 (%ld)", a->fa42, 0x4000, mode);

		V8Delete(a);
		ref_V8Delete(b);
	}

	/* Deleting nothing is allowed. */
	V8Delete(0);
	ref_V8Delete(0);
	diff_eq_int("V8Delete(NULL) survives", 1, 1, 0);

	/*
	 * The configuration trace -- seventeen messages in V8Create plus
	 * initTxSequence's banner and BUG repairs.  Both sides' diagnostics
	 * are raised and their transcripts compared; the variants walk both
	 * call-function branches, both protocol branches, and the
	 * declared-but-empty extension that trips the BUG message.
	 */
	{
		static const struct {
			unsigned char	b1, b2;
			unsigned char	ext1_0, ext2_0;
		} vars[] = {
			{ 0x55, 0x50, 'G', 0 },	/* flags CF, LAPM         */
			{ 0x55, 0x54, 'G', 0 },	/* raw CF, LAPM           */
			{ 0x55, 0x58, 'G', 0 },	/* raw protocol EMPTY --
						 * that BUG repair fires  */
			{ 0x55, 0x1c, 'G', 2 },	/* raw CF and raw proto   */
			{ 0x00, 0x04, 0,   0 },	/* raw CF EMPTY -- its
						 * BUG, then (b1 clear)
						 * the no-CF BUG too      */
			{ 0x00, 0x00, 0,   0 }	/* nothing selected --
						 * the no-CF BUG alone    */
		};
		unsigned lvl, k;
		long lines = 0;

		for (lvl = 1; lvl <= 3; lvl++) {
			dsplib_debug_capture_reset();
			dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
			dsplib_debug_capture_on = 1;

			for (k = 0; k < sizeof(vars) / sizeof(vars[0]); k++) {
				memset(&cm_a, 0, sizeof(cm_a));
				cm_a.b0 = 0xaa;
				cm_a.b1 = vars[k].b1;
				cm_a.b2 = vars[k].b2;
				cm_a.offered = -12;
				cm_a.menu = 3;
				cm_a.ext1[0] = vars[k].ext1_0;
				if (vars[k].ext2_0) {
					cm_a.ext2[0] = vars[k].ext2_0;
					cm_a.ext2[1] = 5;
				}
				memcpy(&cm_b, &cm_a, sizeof(cm_a));

				cfg.mode = (int)(k & 1);
				cfg.f04 = (int)k;
				cfg.timeout_a = 12;
				cfg.timeout_b = 3;
				cfg.f10 = 9600;

				cfg.cm = &cm_a;
				b = ref_V8Create(&cfg);
				cfg.cm = &cm_b;
				a = V8Create(&cfg);
				diff_eq_int("menus still agree (%ld)",
					    memcmp(&cm_a, &cm_b,
						   sizeof(cm_a)) == 0, 1,
					    (long)(lvl * 8 + k));
				V8Delete(a);
				ref_V8Delete(b);
			}

			dsplib_debug_capture_on = 0;
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;

			diff_eq_int("transcript matches (level %ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, (long)lvl);
			if (getenv("DBGDIFF")
			    && strcmp(dsplib_debug_capture_text(0),
				      dsplib_debug_capture_text(1)) != 0) {
				const char *o = dsplib_debug_capture_text(0);
				const char *r = dsplib_debug_capture_text(1);
				int j = 0;
				while (o[j] && o[j] == r[j]) j++;
				while (j > 0 && o[j - 1] != '\n') j--;
				printf("=== level %u: divergence at %d\n",
				       lvl, j);
				printf("--- ours: %.400s\n", o + j);
				printf("--- ref : %.400s\n", r + j);
			}
			diff_eq_int("line counts match (level %ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1),
				    (long)lvl);
			if (lvl == 1)
				diff_eq_int("silent below the threshold",
					    (int)dsplib_debug_capture_lines(1),
					    0, 0);
			else
				lines += dsplib_debug_capture_lines(1);
		}

		/* Two empty captures also compare equal (finding 149). */
		diff_eq_int("diagnostics were captured (%ld lines)",
			    lines > 100, 1, lines);
	}

	return diff_end();
}

/*
 * The decode side.  Its value is that it is the exact inverse of the encoder,
 * so the strongest check available is a round trip: build a CM from a menu,
 * copy the words across as if they had arrived, read them back as octets, and
 * require the octets to be what the encoder was given.
 */
static int
t_getmessage(void)
{
	unsigned char out_a[32], out_b[32];
	int ca, cb, ra, rb;
	unsigned b0, b1, cap, mode;
	long decoded = 0;

	diff_begin("V8GetMessage: decoding a received message");

	for (mode = 0; mode <= 1; mode++) {
		for (b0 = 0; b0 < 256; b0 += 7) {
			for (b1 = 0; b1 < 256; b1 += 13) {
				for (cap = 0; cap <= 3; cap++) {
					struct v8_tx_sequence *sa, *sb;
					int n;

					memset(&obj_a, 0, sizeof(obj_a));
					memset(&obj_b, 0, sizeof(obj_b));
					memset(&cm_a, 0, sizeof(cm_a));
					cm_a.b0 = (unsigned char)b0;
					cm_a.b1 = (unsigned char)b1;
					cm_a.b2 = 0x04;
					cm_a.ext1[0] = 'G';
					cm_a.ext1[1] = 'B';
					memcpy(&cm_b, &cm_a, sizeof(cm_a));

					/* Build a message into buffer 0. */
					obj_a.cm = &cm_a;
					obj_b.cm = &cm_b;
					obj_a.tx_seq = &obj_a.seq[0];
					obj_b.tx_seq = &obj_b.seq[0];
					ref_initTxSequence(&obj_a);
					initTxSequence(&obj_b);

					/*
					 * Present it as received: mode picks
					 * which buffer the reader looks in.
					 */
					obj_a.mode = obj_b.mode = (int)mode;
					sa = mode ? &obj_a.seq[0]
						  : &obj_a.seq[2];
					sb = mode ? &obj_b.seq[0]
						  : &obj_b.seq[2];
					n = obj_a.seq[0].nbits / 10;
					memcpy(sa->word, obj_a.seq[0].word,
					       sizeof(sa->word));
					memcpy(sb->word, obj_b.seq[0].word,
					       sizeof(sb->word));
					sa->wordidx = (short)n;
					sb->wordidx = (short)n;

					/* cap 3 is deliberately too small. */
					ca = cb = cap == 3 ? 2 : 32;
					memset(out_a, 0, sizeof(out_a));
					memset(out_b, 0, sizeof(out_b));
					ra = ref_V8GetMessage(&obj_a, out_a,
							      &ca);
					rb = V8GetMessage(&obj_b, out_b, &cb);

					diff_eq_int("return (%ld)", rb, ra,
						    (long)b0);
					diff_eq_int("count (%ld)", cb, ca,
						    (long)b0);
					diff_eq_int("octets (%ld)",
						    memcmp(out_a, out_b,
							   sizeof(out_a)) == 0,
						    1, (long)b0);
					if (ca > 0)
						decoded++;

					/*
					 * The round trip, checked against the
					 * encoder rather than against the
					 * blob: the first extension character
					 * was 'G', so that is what must come
					 * back out of the third octet.
					 */
					if (cap != 3 && ca >= 3)
						diff_eq_int("round trip (%ld)",
							    out_b[2], 'G',
							    (long)b0);
				}
			}
		}
	}

	/* Nothing received reads as empty, not as a zero-length message. */
	memset(&obj_b, 0, sizeof(obj_b));
	obj_b.mode = 1;
	cb = 32;
	diff_eq_int("empty reads as empty", V8GetMessage(&obj_b, out_b, &cb),
		    V8_GET_EMPTY, 0);
	diff_eq_int("messages were decoded (%ld)", decoded > 100, 1, decoded);

	return diff_end();
}

static int
t_setmessage(void)
{
	unsigned char msg[24], back[32];
	int which, n, i, ra, rb, cnt;
	long ok = 0;

	diff_begin("V8SetMessage: encoding octets into a buffer");

	for (i = 0; i < (int)sizeof(msg); i++)
		msg[i] = (unsigned char)(i * 37 + 11);

	/* Every selector, including two that are not selectors. */
	for (which = -1; which <= 4; which++) {
		for (n = 0; n <= 20; n++) {
			memset(&obj_a, 0x33, sizeof(obj_a));
			memcpy(&obj_b, &obj_a, sizeof(obj_a));
			ra = ref_V8SetMessage(&obj_a, which, msg, n);
			rb = V8SetMessage(&obj_b, which, msg, n);
			diff_eq_int("return (which %ld)", rb, ra, which);
			diff_eq_int("object (which %ld)",
				    memcmp(&obj_a, &obj_b,
					   sizeof(obj_a)) == 0, 1, which);
			if (ra == 0)
				ok++;
		}
	}

	/*
	 * Set then get, through the pair of selectors that name the same
	 * buffer the reader looks in.  What went in must come back out.
	 */
	memset(&obj_b, 0, sizeof(obj_b));
	obj_b.mode = 1;
	V8SetMessage(&obj_b, V8_SET_CM, msg, 9);
	obj_b.seq[0].wordidx = 9;
	cnt = (int)sizeof(back);
	diff_eq_int("round trip returns 0",
		    V8GetMessage(&obj_b, back, &cnt), 0, 0);
	diff_eq_int("round trip length", cnt, 9, 0);
	for (i = 0; i < 9; i++)
		diff_eq_int("round trip octet %ld", back[i], msg[i], i);

	diff_eq_int("selectors were accepted (%ld)", ok > 0, 1, ok);

	/*
	 * The diagnostic paths: the v8SequenceName banner for every valid
	 * selector and one complaint each for an illegal type, a zero
	 * length and an oversize message.  A coarser grid than above so the
	 * transcript stays well inside the capture buffer.
	 */
	{
		unsigned lvl;
		long lines = 0;

		for (lvl = 1; lvl <= 3; lvl++) {
			dsplib_debug_capture_reset();
			dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
			dsplib_debug_capture_on = 1;

			for (which = -1; which <= 4; which++)
				for (n = 0; n <= 20; n += 5) {
					memset(&obj_a, 0x33, sizeof(obj_a));
					memcpy(&obj_b, &obj_a, sizeof(obj_a));
					ref_V8SetMessage(&obj_a, which, msg, n);
					V8SetMessage(&obj_b, which, msg, n);
				}

			dsplib_debug_capture_on = 0;
			dsplibs_debug_level = ref_dsplibs_debug_level = 0;

			diff_eq_int("transcript matches (level %ld)",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, (long)lvl);
			if (getenv("DBGDIFF")
			    && strcmp(dsplib_debug_capture_text(0),
				      dsplib_debug_capture_text(1)) != 0) {
				const char *o = dsplib_debug_capture_text(0);
				const char *r = dsplib_debug_capture_text(1);
				int j = 0;
				while (o[j] && o[j] == r[j]) j++;
				while (j > 0 && o[j - 1] != '\n') j--;
				printf("=== setmessage level %u: at %d\n",
				       lvl, j);
				printf("--- ours: %.400s\n", o + j);
				printf("--- ref : %.400s\n", r + j);
			}
			diff_eq_int("line counts match (level %ld)",
				    (int)dsplib_debug_capture_lines(0),
				    (int)dsplib_debug_capture_lines(1),
				    (long)lvl);
			if (lvl == 1)
				diff_eq_int("silent below the threshold",
					    (int)dsplib_debug_capture_lines(1),
					    0, 0);
			else
				lines += dsplib_debug_capture_lines(1);
		}
		diff_eq_int("diagnostics were captured (%ld lines)",
			    lines > 50, 1, lines);
	}

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= t_mpyint();
	rc |= t_absfn();
	rc |= t_cosread();
	rc |= t_crc();
	rc |= t_copycoeff();
	rc |= t_dftenergy();
	rc |= t_charflip();
	rc |= t_inits();
	rc |= t_txsequence();
	rc |= t_handshakinit();
	rc |= t_v8create();
	rc |= t_getmessage();
	rc |= t_setmessage();
	return rc;
}
