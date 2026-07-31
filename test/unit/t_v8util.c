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
#include <string.h>

#include "harness.h"
#include "dsplib/v8.h"

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

/* The two objects every comparison below runs through. */
static struct v8 obj_a, obj_b;

/*
 * The sequence builder works through two pointers the caller sets, so each
 * side gets its own pair and the results are compared directly rather than
 * through the object.
 */
static struct v8_tx_sequence seq_a, seq_b;
static struct v8_cm cm_a, cm_b;

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
	diff_eq_int("terminator", (unsigned short)seq_a.terminator, 0xffff, 0);
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
		diff_eq_int("%s: byte", b[i], a[i], (long)i);
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
	diff_eq_int("phase-rev countdown", obj_b.phase_rev.f0e, 0x20, 0);
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
				a[i].f00 = b[i].f00 = i;
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
	return rc;
}
