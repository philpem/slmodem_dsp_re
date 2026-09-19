/*
 * t_v32seq.c -- differential test of V.32's rate-signal codec, sequence
 *               generator and sequence detector against the blob.
 *
 * The instance is not modelled as a struct on either side, so the fixture is
 * three raw buffers with the two pointers the object expects planted at
 * +0x64 and +0x68.  Ours and the reference get INDEPENDENT copies filled from
 * the same generator, and what is compared after every call is the whole of
 * both sub-blocks byte for byte -- not just the fields this file knows about.
 * A write to an offset nobody has named yet fails here.
 *
 * The two obj blocks are NOT compared: they differ in the two pointers by
 * construction, for ever, and nothing in this batch writes anything else in
 * them.  That is the "two heap pointers hold two different addresses" case
 * CLAUDE.md licenses a loop for.
 *
 * THE RATE LADDER IS SWEPT EXHAUSTIVELY.  All 65,536 rate signals against all
 * seven local rate indices, for each of the five functions that carry the
 * ladder: 2,293,760 comparisons.  Those are aggregated into one verdict per
 * (function, local index) so the log stays readable, and the verdict PRINTS
 * ITS DENOMINATOR -- a sweep that silently swept nothing is the failure mode
 * findings F2400 and F3100 are about.  A short list of hand-picked signals is
 * checked value by value as well, so a failure names an input.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/v32seq.h"

extern short ref_V32_RATE_SEQ[7];
extern short ref_V32_FINAL_RATE_SEQ[7];
extern short ref_V32_ESEQ[7];

extern unsigned short ref_RateToSeq(void *modem, short rate);
extern int ref_SeqToRate(void *modem, unsigned short seq);
extern short ref_DecodeRateSeq(void *modem, unsigned short seq);
extern unsigned short ref_CodeRateSeq(void *modem, unsigned short seq);
extern unsigned short ref_CodeFinalRateSeq(void *modem, unsigned short seq);
extern unsigned short ref_CodeESeq(void *modem, unsigned short seq);
extern void ref_InitGenSequence(void *modem, unsigned short pattern,
				unsigned short total, unsigned short width);
extern void ref_GenSequence(void *modem, short *out, unsigned short count);
extern void ref_InitDetSequence(void *modem, int target, int mask,
				int out_mask, unsigned short width);
extern short ref_DetSequence(void *modem, const short *data,
			     unsigned short count);
extern int ref_GetSequence(void *modem);
extern short ref_LoadReg(void *modem, short reg);
extern void ref_StoreReg(void *modem, short value, short reg);

#define OBJ_LEN		0x80
#define HDX_LEN		0x100
#define FP_LEN		0x40

struct fx {
	unsigned char obj[OBJ_LEN];
	unsigned char hdx[HDX_LEN];
	unsigned char fp[FP_LEN];
};

static struct fx ours;
static struct fx theirs;

static struct v32_modem *
modem_of(struct fx *f)
{
	return (struct v32_modem *)(void *)f->obj;
}

/*
 * `diff_begin` ZEROES `diff_failures`, so a section's verdict is only
 * readable between its own begin and the next one.  `main` used to end
 * `return diff_failures != 0`, which reported the LAST section and nothing
 * else -- 37 of this file's 54 mutations came back NOT CAUGHT because the
 * binary exited 0 with the failures printed above it.  Every `diff_end` is
 * accumulated into this for that reason.
 */
static int failed;

/*
 * Fill both sides identically from one deterministic generator, then plant
 * each side's own pointers.  The fill is not zero on purpose: a body that
 * fails to write a field it should have written leaves the pattern behind and
 * the byte comparison sees it.
 */
static void
fx_init(unsigned int seed)
{
	unsigned int s = seed | 1u;
	unsigned int i;

	for (i = 0; i < HDX_LEN; i++) {
		s = s * 1103515245u + 12345u;
		ours.hdx[i] = theirs.hdx[i] = (unsigned char)(s >> 16);
	}
	for (i = 0; i < FP_LEN; i++) {
		s = s * 1103515245u + 12345u;
		ours.fp[i] = theirs.fp[i] = (unsigned char)(s >> 16);
	}
	memset(ours.obj, 0, sizeof(ours.obj));
	memset(theirs.obj, 0, sizeof(theirs.obj));

	*(void **)(void *)(ours.obj + 0x64) = ours.hdx;
	*(void **)(void *)(ours.obj + 0x68) = ours.fp;
	*(void **)(void *)(theirs.obj + 0x64) = theirs.hdx;
	*(void **)(void *)(theirs.obj + 0x68) = theirs.fp;
}

/* Both sub-blocks, byte for byte.  Reports the first differing offset. */
static void
fx_same(const char *what, int input)
{
	int off = -1;
	unsigned int i;

	for (i = 0; i < HDX_LEN; i++)
		if (ours.hdx[i] != theirs.hdx[i]) {
			off = (int)i;
			break;
		}
	diff_eq_int("hdx differs at offset %ld", off, -1, off);
	(void)what;

	off = -1;
	for (i = 0; i < FP_LEN; i++)
		if (ours.fp[i] != theirs.fp[i]) {
			off = (int)i;
			break;
		}
	diff_eq_int("fp differs at offset %ld", off, -1, off);
	(void)input;
}

static void
set_rate_index(short idx)
{
	*(short *)(void *)(ours.fp + V32FP_RX_RATE_INDEX) = idx;
	*(short *)(void *)(theirs.fp + V32FP_RX_RATE_INDEX) = idx;
}

/* ------------------------------------------------------------------ */

static void
tables(void)
{
	int i;

	diff_begin("the three rate-signal tables are the blob's");
	for (i = 0; i < V32_RATE_COUNT; i++) {
		diff_eq_int("V32_RATE_SEQ[%ld]", V32_RATE_SEQ[i],
			    ref_V32_RATE_SEQ[i], i);
		diff_eq_int("V32_FINAL_RATE_SEQ[%ld]", V32_FINAL_RATE_SEQ[i],
			    ref_V32_FINAL_RATE_SEQ[i], i);
		diff_eq_int("V32_ESEQ[%ld]", V32_ESEQ[i], ref_V32_ESEQ[i], i);
	}
	/*
	 * The two rate tables differ at exactly one index.  Stated as a check
	 * rather than as a comment so that a table edit that loses it fails.
	 */
	for (i = 0; i < V32_RATE_COUNT; i++)
		diff_eq_int("RATE_SEQ and FINAL_RATE_SEQ agree except at 5 (%ld)",
			    V32_RATE_SEQ[i] == V32_FINAL_RATE_SEQ[i],
			    i != 5, i);
	failed |= diff_end();
}

static void
rate_to_seq(void)
{
	short r;

	diff_begin("RateToSeq over every index the table has");
	fx_init(0x5eed0001u);
	for (r = 0; r < V32_RATE_COUNT; r++) {
		diff_eq_int("RateToSeq(%ld)", RateToSeq(modem_of(&ours), r),
			    ref_RateToSeq(theirs.obj, r), r);
		fx_same("RateToSeq", r);
	}
	failed |= diff_end();
}

/*
 * The exhaustive ladder sweep.  One verdict per (function, local index), each
 * carrying the number of signals it compared -- see the header note on
 * denominators.
 */
static void
ladder_sweep(void)
{
	short idx;

	diff_begin("the rate ladder, all 65536 signals x 7 local indices");
	fx_init(0x5eed0002u);
	for (idx = 0; idx < V32_RATE_COUNT; idx++) {
		long bad_seq = 0, bad_dec = 0, bad_code = 0;
		long bad_final = 0, bad_e = 0, n = 0;
		unsigned int s;

		set_rate_index(idx);
		for (s = 0; s <= 0xffffu; s++) {
			unsigned short q = (unsigned short)s;

			n++;
			if (SeqToRate(modem_of(&ours), q) != ref_SeqToRate(theirs.obj, q))
				bad_seq++;
			if (DecodeRateSeq(modem_of(&ours), q) !=
			    ref_DecodeRateSeq(theirs.obj, q))
				bad_dec++;
			if (CodeRateSeq(modem_of(&ours), q) !=
			    ref_CodeRateSeq(theirs.obj, q))
				bad_code++;
			if (CodeFinalRateSeq(modem_of(&ours), q) !=
			    ref_CodeFinalRateSeq(theirs.obj, q))
				bad_final++;
			if (CodeESeq(modem_of(&ours), q) !=
			    ref_CodeESeq(theirs.obj, q))
				bad_e++;
		}
		diff_eq_int("SeqToRate: mismatches over 65536 signals, index %ld",
			    (int)bad_seq, 0, idx);
		diff_eq_int("DecodeRateSeq: mismatches over 65536, index %ld",
			    (int)bad_dec, 0, idx);
		diff_eq_int("CodeRateSeq: mismatches over 65536, index %ld",
			    (int)bad_code, 0, idx);
		diff_eq_int("CodeFinalRateSeq: mismatches over 65536, index %ld",
			    (int)bad_final, 0, idx);
		diff_eq_int("CodeESeq: mismatches over 65536, index %ld",
			    (int)bad_e, 0, idx);
		diff_eq_int("signals compared at index %ld", (int)n, 65536, idx);
		fx_same("ladder", idx);
	}
	failed |= diff_end();
}

/*
 * The named signals: every table entry, the two sentinels, and the pairs that
 * separate one arm of the ladder from the next.  A failure here names the
 * input the sweep above can only count.
 */
static void
ladder_named(void)
{
	static const unsigned short interesting[] = {
		0x0000, 0xffff, 0x0111, 0xf111,
		0x0d11, 0x0b11, 0x0b91, 0x09d1, 0x09b1, 0x0ff9, 0x0997,
		0x0008, 0x0020, 0x0040, 0x0080, 0x0200, 0x0400,
		0x0280, 0x0200 | 0x0040, 0x0400 | 0x0040, 0x0008 | 0x0400
	};
	short idx;
	unsigned int k;

	diff_begin("the rate ladder on named signals");
	fx_init(0x5eed0003u);
	for (idx = 0; idx < V32_RATE_COUNT; idx++) {
		set_rate_index(idx);
		for (k = 0; k < sizeof(interesting) / sizeof(interesting[0]);
		     k++) {
			unsigned short q = interesting[k];
			int in = (int)((unsigned)idx << 16) | (int)q;

			diff_eq_int("SeqToRate(idx<<16|seq = %#lx)",
				    SeqToRate(modem_of(&ours), q),
				    ref_SeqToRate(theirs.obj, q), in);
			diff_eq_int("DecodeRateSeq(%#lx)",
				    DecodeRateSeq(modem_of(&ours), q),
				    ref_DecodeRateSeq(theirs.obj, q), in);
			diff_eq_int("CodeRateSeq(%#lx)",
				    CodeRateSeq(modem_of(&ours), q),
				    ref_CodeRateSeq(theirs.obj, q), in);
			diff_eq_int("CodeFinalRateSeq(%#lx)",
				    CodeFinalRateSeq(modem_of(&ours), q),
				    ref_CodeFinalRateSeq(theirs.obj, q), in);
			diff_eq_int("CodeESeq(%#lx)",
				    CodeESeq(modem_of(&ours), q),
				    ref_CodeESeq(theirs.obj, q), in);
		}
		fx_same("ladder named", idx);
	}
	/*
	 * NON-VACUITY.  Finding F8163: a guard proves a branch was reached,
	 * not that the test separates it from its alternative.  These assert
	 * that the ladder actually produces every one of its seven answers
	 * over the inputs above, so a body that collapsed two arms could not
	 * pass the sweep by accident.
	 */
	{
		int seen[V32_RATE_COUNT + 1];
		unsigned int s;
		int r;

		for (r = 0; r <= V32_RATE_COUNT; r++)
			seen[r] = 0;
		set_rate_index(5);		/* the all-bits entry */
		for (s = 0; s <= 0xffffu; s++) {
			r = SeqToRate(modem_of(&ours), (unsigned short)s);
			if (r >= 0 && r <= V32_RATE_COUNT)
				seen[r]++;
		}
		for (r = 0; r < V32_RATE_COUNT; r++)
			diff_eq_int("the ladder can answer %ld at all",
				    seen[r] > 0, 1, r);
	}
	failed |= diff_end();
}

static void
regs(void)
{
	static const short idxs[] = { -32768, -2, -1, 0, 1, 2, 3, 4, 5, 6,
				      100, 32767 };
	unsigned int k;

	diff_begin("LoadReg and StoreReg, in range and out");
	fx_init(0x5eed0004u);
	for (k = 0; k < sizeof(idxs) / sizeof(idxs[0]); k++) {
		short r = idxs[k];
		short v = (short)(0x4100 + k);

		StoreReg(modem_of(&ours), v, r);
		ref_StoreReg(theirs.obj, v, r);
		fx_same("StoreReg", r);
		diff_eq_int("LoadReg(%ld) after StoreReg", LoadReg(modem_of(&ours), r),
			    ref_LoadReg(theirs.obj, r), r);
		fx_same("LoadReg", r);
	}
	/* Every in-range slot must be readable back as what was written. */
	for (k = 0; k < V32HDX_NREGS; k++) {
		StoreReg(modem_of(&ours), (short)(0x1000 + k), (short)k);
		ref_StoreReg(theirs.obj, (short)(0x1000 + k), (short)k);
	}
	for (k = 0; k < V32HDX_NREGS; k++) {
		diff_eq_int("register %ld round-trips",
			    LoadReg(modem_of(&ours), (short)k), (int)(0x1000 + k),
			    (int)k);
		diff_eq_int("register %ld agrees with the blob",
			    LoadReg(modem_of(&ours), (short)k),
			    ref_LoadReg(theirs.obj, (short)k), (int)k);
	}
	fx_same("regs", 0);
	failed |= diff_end();
}

static void
generator(void)
{
	static const unsigned short cfg[][3] = {
		/* pattern, total, width */
		{ 0x1234, 16, 4 },
		{ 0xabcd, 16, 4 },
		{ 0xffff, 16, 1 },
		{ 0x8001, 16, 2 },
		{ 0x0f0f, 16, 8 },
		{ 0x1234, 12, 4 },	/* count is not a power of two */
		{ 0x1234,  8, 4 },
		{ 0xdead, 16, 16 },	/* one field, the whole word */
		{ 0x0000, 16, 4 }
	};
	short a[64], b[64];
	unsigned int k, i;

	diff_begin("InitGenSequence and GenSequence");
	for (k = 0; k < sizeof(cfg) / sizeof(cfg[0]); k++) {
		fx_init(0x5eed0010u + k);
		InitGenSequence(modem_of(&ours), cfg[k][0], cfg[k][1], cfg[k][2]);
		ref_InitGenSequence(theirs.obj, cfg[k][0], cfg[k][1],
				    cfg[k][2]);
		fx_same("InitGenSequence", (int)k);

		/* Three calls, so the index carries across the boundary. */
		for (i = 0; i < 3; i++) {
			unsigned short n = (unsigned short)(1 + i * 7);
			unsigned int j;

			memset(a, 0x5a, sizeof(a));
			memset(b, 0x5a, sizeof(b));
			GenSequence(modem_of(&ours), a, n);
			ref_GenSequence(theirs.obj, b, n);
			for (j = 0; j < sizeof(a) / sizeof(a[0]); j++)
				diff_eq_int("GenSequence word %ld", a[j], b[j],
					    (int)j);
			fx_same("GenSequence", (int)(k * 10 + i));
		}
	}
	/* A zero count must emit nothing and must still write the index back. */
	fx_init(0x5eed0020u);
	InitGenSequence(modem_of(&ours), 0x1234, 16, 4);
	ref_InitGenSequence(theirs.obj, 0x1234, 16, 4);
	memset(a, 0x5a, sizeof(a));
	memset(b, 0x5a, sizeof(b));
	GenSequence(modem_of(&ours), a, 0);
	ref_GenSequence(theirs.obj, b, 0);
	diff_eq_int("zero count writes nothing", memcmp(a, b, sizeof(a)), 0, 0);
	diff_eq_int("zero count leaves the buffer alone", a[0], 0x5a5a, 0);
	fx_same("GenSequence zero", 0);
	failed |= diff_end();
}

static void
detector(void)
{
	static const struct {
		int target, mask, out_mask;
		unsigned short width;
	} arm[] = {
		{ 0x0a,       0x0f,       0x0f,       4 },
		{ 0x00,       0x0f,       0xffff,     4 },
		{ 0x1234,     0xffff,     0xffff,    16 },
		{ 0x01,       0x01,       0x03,       1 },
		{ -1,         -1,         -1,         4 },  /* the reg != -1 arm */
		{ 0x55,       0xff,       0x0f,       8 },
		{ 0x0a,       0x0f,       0x0f,       0 }   /* no bits at all */
	};
	static const short stream[] = {
		0x0000, 0x000a, 0x00ff, 0x1234, (short)0xffff, 0x0055,
		0x00a0, 0x5555, 0x0001, 0x7fff, (short)0x8000, 0x000a
	};
	unsigned int k, i;

	diff_begin("InitDetSequence, DetSequence and GetSequence");
	for (k = 0; k < sizeof(arm) / sizeof(arm[0]); k++) {
		fx_init(0x5eed0030u + k);
		/*
		 * PIN THE GENERATOR'S WIDTH SMALL, and do not remove this.
		 * The detector's bit-width field sits four bytes below the
		 * generator's, so "read the wrong width" is the natural
		 * mutation -- and with the random fill left in place the
		 * mutant read a width near 65535, overflowed `short bit` and
		 * ran for ever, so the suite recorded it `caught (hang)`.  A
		 * hang is the one verdict that is not reproducible in
		 * principle (tools/mutsnap.py's DETERMINISM note); pinning
		 * this makes the same mutation fail a CHECK instead.  See
		 * D403 for why the overflow is the object's behaviour and not
		 * ours.
		 */
		*(unsigned short *)(void *)(ours.hdx + V32HDX_GEN_WIDTH) = 3;
		*(unsigned short *)(void *)(theirs.hdx + V32HDX_GEN_WIDTH) = 3;
		InitDetSequence(modem_of(&ours), arm[k].target, arm[k].mask,
				arm[k].out_mask, arm[k].width);
		ref_InitDetSequence(theirs.obj, arm[k].target, arm[k].mask,
				    arm[k].out_mask, arm[k].width);
		fx_same("InitDetSequence", (int)k);

		/*
		 * Fed in three bites, so the shift register has to survive
		 * the call boundary -- which is what the write-back on the
		 * no-match path is for.
		 */
		for (i = 0; i < 3; i++) {
			const short *p = stream + i * 4;

			diff_eq_int("DetSequence arm %ld",
				    DetSequence(modem_of(&ours), p, 4),
				    ref_DetSequence(theirs.obj, p, 4),
				    (int)(k * 10 + i));
			fx_same("DetSequence", (int)(k * 10 + i));
			diff_eq_int("GetSequence arm %ld",
				    GetSequence(modem_of(&ours)),
				    ref_GetSequence(theirs.obj),
				    (int)(k * 10 + i));
		}

		/* A zero count still has to leave the register where it was. */
		diff_eq_int("DetSequence with count 0, arm %ld",
			    DetSequence(modem_of(&ours), stream, 0),
			    ref_DetSequence(theirs.obj, stream, 0), (int)k);
		fx_same("DetSequence zero", (int)k);
	}

	/*
	 * NON-VACUITY.  A match and a miss must both occur, or the arms above
	 * prove nothing about the branch that reports one.
	 */
	{
		int hits = 0, misses = 0;
		unsigned int i2;

		fx_init(0x5eed0040u);
		*(unsigned short *)(void *)(ours.hdx + V32HDX_GEN_WIDTH) = 3;
		*(unsigned short *)(void *)(theirs.hdx + V32HDX_GEN_WIDTH) = 3;
		InitDetSequence(modem_of(&ours), 0x0a, 0x0f, 0x0f, 4);
		ref_InitDetSequence(theirs.obj, 0x0a, 0x0f, 0x0f, 4);
		for (i2 = 0; i2 < sizeof(stream) / sizeof(stream[0]); i2++) {
			if (DetSequence(modem_of(&ours), stream + i2, 1) < 0)
				misses++;
			else
				hits++;
			(void)ref_DetSequence(theirs.obj, stream + i2, 1);
		}
		diff_eq_int("the detector reports at least one hit", hits > 0,
			    1, hits);
		diff_eq_int("the detector reports at least one miss",
			    misses > 0, 1, misses);
		fx_same("non-vacuity", 0);
	}

	/*
	 * THE ALL-ONES GUARD, which nothing above reaches.
	 *
	 * `reg != -1` can only matter once the register has taken 32 one-bits
	 * in a row, and the arms above shift four bits per word out of a
	 * stream that never has eight consecutive 0xf nibbles -- so the guard
	 * was proved to exist by no check at all, and two mutations that
	 * delete it came back NOT CAUGHT.  This drives it: sixteen bits per
	 * word, four words of 0xffff, and a target the low nibble satisfies at
	 * every step.  The register reaches 0xffffffff during the third word,
	 * and there the object DECLINES to record a match it would otherwise
	 * have recorded -- so the value left behind is the previous step's
	 * 0x7fffffff and not -1, which is what separates the guard from its
	 * absence and from the (reg & mask) spelling of it.
	 *
	 * F8163: the assertion that the register actually reaches -1 is what
	 * makes this a test of the guard rather than of the arms around it.
	 */
	{
		static const short ones[] = {
			(short)0xffff, (short)0xffff, (short)0xffff,
			(short)0xffff
		};
		int reached = 0;
		unsigned int i2;

		fx_init(0x5eed0050u);
		*(unsigned short *)(void *)(ours.hdx + V32HDX_GEN_WIDTH) = 3;
		*(unsigned short *)(void *)(theirs.hdx + V32HDX_GEN_WIDTH) = 3;
		InitDetSequence(modem_of(&ours), 0x0f, 0x0f, -1, 16);
		ref_InitDetSequence(theirs.obj, 0x0f, 0x0f, -1, 16);
		for (i2 = 0; i2 < sizeof(ones) / sizeof(ones[0]); i2++) {
			diff_eq_int("all-ones word %ld",
				    DetSequence(modem_of(&ours), ones + i2, 1),
				    ref_DetSequence(theirs.obj, ones + i2, 1),
				    (int)i2);
			diff_eq_int("all-ones match value after word %ld",
				    GetSequence(modem_of(&ours)),
				    ref_GetSequence(theirs.obj), (int)i2);
			fx_same("all-ones", (int)i2);
			if (*(int *)(void *)(ours.hdx + V32HDX_DET_REG) == -1)
				reached = 1;
		}
		diff_eq_int("the shift register does reach all ones", reached,
			    1, 0);
		diff_eq_int("and the guard kept it out of the match value",
			    GetSequence(modem_of(&ours)) != -1, 1, 0);
	}
	failed |= diff_end();
}

int
main(void)
{
	tables();
	rate_to_seq();
	ladder_named();
	ladder_sweep();
	regs();
	generator();
	detector();

	return failed != 0;
}
