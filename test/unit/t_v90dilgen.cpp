/*
 * t_v90dilgen.cpp -- differential test of V90Phase3Modulator::generateDIL.
 *
 * `generateDIL` IS CALLERLESS IN THE BLOB.  Zero relocations of any kind name
 * `_ZN18V90Phase3Modulator11generateDILEv` in the 1.2 MB object -- against
 * two for `generateV90Symbol`, which is how that measurement is shown to fire
 * -- because the compiler inlined the body into `generateV90Symbol` and
 * `generateV92Symbol` and emitted the out-of-line copy only because the
 * symbol is external.  That does NOT put it out of reach of a differential
 * test: the harness links the blob with every symbol renamed `ref_*` and
 * calls the alias directly, rather than driving the shipped modem down to it
 * (finding F7000, which corrected 702 on exactly this point).  So this file
 * calls `ref__ZN18V90Phase3Modulator11generateDILEv` and nothing anywhere
 * calls the reconstruction's `generateDIL` except this test and the two
 * generators that inline it.
 *
 * THE RETURN TYPE IS THE FIRST THING UNDER TEST.  A return type is not
 * mangled, so `int` is a claim derived from the object's `neg %ecx ; movswl
 * %cx,%edi ; ... ; mov %edi,%eax` and not from the symbol.  Declaring the
 * alias `int` here and comparing all thirty-two bits is what makes the claim
 * falsifiable: a reconstruction returning `unsigned short`, or one that
 * dropped the re-narrowing, differs in bits the low sixteen cannot show.
 *
 * THE FIXTURE IS SEEDED WITH VARIED BYTES AND RESEEDED EVERY TRIAL.  Both
 * sides get the same pseudorandom fill, so a field neither side writes cannot
 * pass by accident (findings F223, F224) -- and the reseed is not optional
 * here: the segment-restart arm ZEROES `segmentPos`, `seq1Index` and
 * `seq2Index`, so a fixture that seeded once and looped would be testing the
 * post-restart state from the second trial onwards (finding F7105).  The two
 * fields the function always writes, `usingSegmentLevel` and `dilPcmCode`,
 * are pre-set to sentinels that no correct result can equal by accident, so a
 * dropped store is a difference rather than a coincidence.
 *
 * `pcmType` IS ALWAYS FORCED TO 0 OR 1, as in t_v90p3mod: the restart arm
 * indexes `codeSegmentsBoundriesLookupTable` at `8 * pcmType` over sixteen
 * ints, and a random 32-bit `pcmType` would read out of OUR table on one side
 * and out of the REFERENCE table on the other -- two different objects at two
 * different addresses, which tests nothing.
 *
 * WHAT IS DELIBERATELY OUT OF RANGE AND STAYS THAT WAY.  `segmentIndex` can
 * be 8, one past the end of both `segmentLength[8]` and `segmentLevel[8]`,
 * because that is what the object's own boundary search produces when no
 * boundary matches.  `segmentLength[8]` is the four bytes at +0x178, which is
 * `segmentLevel[0..1]`; `segmentLevel[8]` is the two bytes at +0x188, which
 * is `dilLevel[0]`.  Both land inside the object, both are seeded identically
 * on the two sides, and the test drives them on purpose rather than avoiding
 * them.  The alias is read with `memcpy` rather than computed from the two
 * shorts, so nothing here depends on byte order.
 *
 * The `ref_` alias is reached through an asm() label rather than by spelling
 * the alias as an identifier, which sidesteps finding F225.  The convention is
 * plain cdecl with `this` as the first stack argument (finding F215).
 */

#include <string.h>

#include "harness.h"
#include "dsplib/V90Phase3Modulator.h"

extern "C" {
int ref_generateDIL(void *self)
	asm("ref__ZN18V90Phase3Modulator11generateDILEv");
}

/* The object, plus room past its end to catch a store that overruns it. */
#define SLOT 1024

/*
 * The two empty special members are load-bearing for the same reason as in
 * t_v90p3mod: `Scrambler` has a constructor and a destructor, which deletes
 * both of a union holding a `V90Phase3Modulator`.  A user-provided pair that
 * constructs and destroys no variant member restores them and changes nothing
 * else.
 */
struct mod_slot {
	union {
		unsigned char raw[SLOT];
		double align_;		/* alignment only; trivial */
	};
	V90Phase3Modulator &o;

	mod_slot() : o(*(V90Phase3Modulator *)raw) {}
};

static struct mod_slot ours, theirs;

static unsigned lfsr_state;

static unsigned char
next_byte(void)
{
	lfsr_state = (lfsr_state >> 1) ^ (-(int)(lfsr_state & 1u) & 0xb400u);
	return (unsigned char)(lfsr_state >> 3);
}

/*
 * `mode` picks how varied the fill is.  Mode 1 is the harness's own malloc
 * fill, which is what an unseeded object would look like; modes 2 and 3 hold
 * the top bit of every byte constant, which matters because the companded
 * code's top bit is supplied rather than carried.
 */
static void
seed(int trial, int mode)
{
	int i;

	lfsr_state = 0x2d1fu + 0x9e37u * (unsigned)trial + (unsigned)mode;

	for (i = 0; i < SLOT; i++) {
		unsigned char v;

		switch (mode) {
		case 0:
			v = next_byte();
			break;
		case 1:
			v = 0xa5;
			break;
		case 2:
			v = (unsigned char)(next_byte() | 0x80);
			break;
		default:
			v = (unsigned char)(next_byte() & 0x7f);
			break;
		}
		ours.raw[i] = v;
		theirs.raw[i] = v;
	}
}

static int
guard_equal(void)
{
	return memcmp(ours.raw + sizeof(V90Phase3Modulator),
		      theirs.raw + sizeof(V90Phase3Modulator),
		      SLOT - sizeof(V90Phase3Modulator)) == 0;
}

/*
 * `segmentLength[segmentIndex]` when `segmentIndex` is 8: the four bytes at
 * +0x178, which the class declares as `segmentLevel[0]` and `segmentLevel[1]`.
 * Read as bytes so the test does not assume an endianness the reconstruction
 * does not assume either.
 */
static unsigned int
segment_length_of(const V90Phase3Modulator &m, unsigned int idx)
{
	unsigned int v;

	if (idx < 8)
		return m.segmentLength[idx];
	memcpy(&v, &m.segmentLevel[0], sizeof(v));
	return v;
}

/*
 * Both sides get the same DIL state.  Everything the generator reads is set
 * here; everything else keeps the seeded fill, identical on the two sides.
 */
static void
set_both(int law)
{
	ours.o.pcmType = theirs.o.pcmType =
	    law ? PCM_TYPE_A_LAW : PCM_TYPE_MU_LAW;

	/* Sentinels: no correct result is 0x5a5a or 0x37. */
	ours.o.usingSegmentLevel = theirs.o.usingSegmentLevel = 0x5a5a;
	ours.o.dilPcmCode = theirs.o.dilPcmCode = 0x37;
}

static void
copy_to_theirs(void)
{
	memcpy(theirs.raw, ours.raw, SLOT);
}

/* ------------------------------------------------------------------ */
/* Coverage.  Every one of these is asserted non-zero at the end.      */

static int cov_law[2];
static int cov_seq2_zero, cov_seq2_nonzero;
static int cov_seq1_zero, cov_seq1_nonzero;
static int cov_restart, cov_norestart;
static int cov_segidx_in[9];		/* segmentIndex going in, 0..8   */
static int cov_segidx_out[9];		/* and after a restart           */
static int cov_seq1_wrap, cov_seq1_nowrap;
static int cov_seq2_wrap, cov_seq2_nowrap;
static int cov_len1_zero, cov_len2_zero;
static int cov_dilindex_wrap, cov_dilindex_step;
static int cov_level_neg, cov_level_pos, cov_level_zero;
static int cov_boundary_exact, cov_boundary_below, cov_boundary_above;
static int cov_code_high;		/* a level the search reads > 0x7fff */
static int cov_idx_past_seq;		/* seq1Index or seq2Index >= 128     */
static int saw_int16_min;		/* a symbol of -32768 came back      */

static long ntrial;

static void
cov_reset(void)
{
	int i;

	cov_law[0] = cov_law[1] = 0;
	cov_seq2_zero = cov_seq2_nonzero = 0;
	cov_seq1_zero = cov_seq1_nonzero = 0;
	cov_restart = cov_norestart = 0;
	for (i = 0; i < 9; i++)
		cov_segidx_in[i] = cov_segidx_out[i] = 0;
	cov_seq1_wrap = cov_seq1_nowrap = 0;
	cov_seq2_wrap = cov_seq2_nowrap = 0;
	cov_len1_zero = cov_len2_zero = 0;
	cov_dilindex_wrap = cov_dilindex_step = 0;
	cov_level_neg = cov_level_pos = cov_level_zero = 0;
	cov_boundary_exact = cov_boundary_below = cov_boundary_above = 0;
	cov_code_high = 0;
	cov_idx_past_seq = 0;
	saw_int16_min = 0;
	ntrial = 0;
}

/*
 * One call on each side, everything compared.  Reading the "before" state out
 * of `ours` and the "after" state out of `ours` is safe only because the
 * comparison below proves the two sides agree; the coverage is recorded after
 * the comparison for that reason.
 */
static void
drive(long input)
{
	unsigned char i1 = ours.o.seq1Index, i2 = ours.o.seq2Index;
	unsigned char dili = ours.o.dilIndex;
	unsigned char segi = ours.o.segmentIndex;
	unsigned int pos = ours.o.segmentPos;
	unsigned int seglen = segment_length_of(ours.o, segi);
	int s1z = (ours.o.seq1[i1] == 0);
	int s2z = (ours.o.seq2[i2] == 0);
	int restart = (pos + 1u == seglen);
	int a, b;

	a = ours.o.generateDIL();
	b = ref_generateDIL(&theirs.o);

	diff_eq_int("generateDIL returned (case %ld)", a, b, input);
	diff_eq_obj("after generateDIL", V90Phase3Modulator,
		    &ours.o, &theirs.o, input);
	diff_eq_int("no store past the object (case %ld)",
		    guard_equal(), 1, input);

	/*
	 * Two claims about the return that the whole-object comparison cannot
	 * make, because the value is not stored anywhere: it is a sign-
	 * extended sixteen-bit quantity, and it is the level the object says
	 * it chose.  `-32768` negates to itself in sixteen bits, which is why
	 * the second check is written against the stored flag rather than
	 * against a recomputed sign.
	 */
	diff_eq_int("the symbol is a sign-extended short (case %ld)",
		    a, (long)(short)a, input);
	diff_eq_int("usingSegmentLevel was written (case %ld)",
		    ours.o.usingSegmentLevel == 0x5a5a ? 1 : 0, 0, input);
	diff_eq_int("dilPcmCode was written (case %ld)",
		    (ours.o.dilPcmCode == 0x37 && theirs.o.dilPcmCode != 0x37)
			? 1 : 0, 0, input);

	ntrial++;
	cov_law[ours.o.pcmType == PCM_TYPE_A_LAW ? 1 : 0] = 1;
	if (s2z)
		cov_seq2_zero = 1;
	else
		cov_seq2_nonzero = 1;
	if (s1z)
		cov_seq1_zero = 1;
	else
		cov_seq1_nonzero = 1;
	if (segi <= 8)
		cov_segidx_in[segi] = 1;
	if (i1 >= 128 || i2 >= 128)
		cov_idx_past_seq = 1;
	if (restart) {
		cov_restart = 1;
		if (ours.o.segmentIndex <= 8)
			cov_segidx_out[ours.o.segmentIndex] = 1;
		if (ours.o.dilIndex == 0 && dili != 0)
			cov_dilindex_wrap = 1;
		else if (ours.o.dilIndex != dili)
			cov_dilindex_step = 1;
	} else {
		cov_norestart = 1;
		if (ours.o.seq1Index == 0 && i1 != 0)
			cov_seq1_wrap = 1;
		else if (ours.o.seq1Index != i1)
			cov_seq1_nowrap = 1;
		if (ours.o.seq2Index == 0 && i2 != 0)
			cov_seq2_wrap = 1;
		else if (ours.o.seq2Index != i2)
			cov_seq2_nowrap = 1;
		if (ours.o.seq1Length == 0)
			cov_len1_zero = 1;
		if (ours.o.seq2Length == 0)
			cov_len2_zero = 1;
	}
	if (a == -32768)
		saw_int16_min = 1;
	if (a < 0)
		cov_level_neg = 1;
	else if (a > 0)
		cov_level_pos = 1;
	else
		cov_level_zero = 1;
}

/* ------------------------------------------------------------------ */

/*
 * The eight G.711 boundaries per law, as `resetDILGenerator` and the restart
 * arm read them.  Row 0 is mu-law, 124 + 256 * (2**k - 1); row 1 is A-law,
 * 256 << k.  Duplicated here rather than read out of the class so that a
 * mutation of the class's own table is caught by this test as well.
 */
static const long boundary[2][8] = {
	{   124,   380,   892,  1916,  3964,  8060, 16252, 32636 },
	{   256,   512,  1024,  2048,  4096,  8192, 16384, 32768 }
};

/*
 * A DIL state that exercises one combination.  `restart` puts `segmentPos`
 * one below the length the generator will compare against -- including when
 * `segmentIndex` is 8 and that length is the aliased `segmentLevel[0..1]`.
 */
static void
prepare(int trial, int mode, int law, int segidx, int restart,
	int s1zero, int s2zero, int lenmode, long level)
{
	unsigned int i, seglen;
	unsigned char i1, i2;

	seed(trial, mode);
	set_both(law);

	for (i = 0; i < 8; i++) {
		/* The lengths resetDILGenerator itself produces. */
		ours.o.segmentLength[i] = 6u * (i + 1u) + 6u;
		/* Levels spread over the rows, both signs. */
		ours.o.segmentLevel[i] =
		    (short)(boundary[law][i] - 1 - (trial & 3));
	}

	for (i = 0; i < 256; i++)
		ours.o.dilLevel[i] = (short)(0x100 * (i & 7) + trial);
	ours.o.dilCount = (unsigned char)(1 + (trial % 5));
	ours.o.dilIndex = (unsigned char)(trial % 6);
	ours.o.dilLevel[ours.o.dilIndex] = (short)level;
	/* The entry the restart arm will step to, and search on. */
	ours.o.dilLevel[(unsigned char)(ours.o.dilIndex + 1)] = (short)level;
	ours.o.dilLevel[0] = (short)level;

	switch (lenmode) {
	case 0:				/* never wraps */
		ours.o.seq1Length = 0;
		ours.o.seq2Length = 0;
		i1 = (unsigned char)(trial % 13);
		i2 = (unsigned char)(trial % 11);
		break;
	case 1:				/* wraps on the next step */
		ours.o.seq1Length = (unsigned char)(1 + trial % 9);
		ours.o.seq2Length = (unsigned char)(1 + trial % 7);
		i1 = (unsigned char)(ours.o.seq1Length - 1);
		i2 = (unsigned char)(ours.o.seq2Length - 1);
		break;
	case 2:				/* mid-sequence */
		ours.o.seq1Length = 17;
		ours.o.seq2Length = 23;
		i1 = (unsigned char)(trial % 17);
		i2 = (unsigned char)(trial % 23);
		break;
	default:			/* indices past the 128-byte arrays */
		ours.o.seq1Length = 0;
		ours.o.seq2Length = 0;
		i1 = (unsigned char)(128 + (trial % 100));
		i2 = (unsigned char)(128 + (trial % 90));
		break;
	}
	ours.o.seq1Index = i1;
	ours.o.seq2Index = i2;

	for (i = 0; i < 128; i++) {
		unsigned char v = next_byte();

		if (v == 0)
			v = 1;
		ours.o.seq1[i] = v;
		v = next_byte();
		if (v == 0)
			v = 1;
		ours.o.seq2[i] = v;
	}
	/*
	 * The two branch bytes are planted at the index actually used, which
	 * is only inside `seq1`/`seq2` when the index is under 128.  Past
	 * that the object reads on into itself and the arm is whatever the
	 * seeded fill left there -- read back below rather than forced.
	 */
	if (i1 < 128)
		ours.o.seq1[i1] = s1zero ? 0 : 0x5b;
	if (i2 < 128)
		ours.o.seq2[i2] = s2zero ? 0 : 0x2c;

	ours.o.segmentIndex = (unsigned char)segidx;
	seglen = segment_length_of(ours.o, (unsigned int)segidx);
	if (restart)
		ours.o.segmentPos = seglen - 1u;
	else
		ours.o.segmentPos = seglen + 1u + (unsigned)trial;

	copy_to_theirs();
}

/*
 * The structured matrix: both laws, every `segmentIndex` including 8, the
 * restart edge taken and not taken, both arms of both sequence bytes, and
 * four length regimes -- zero-length (never wraps), about to wrap,
 * mid-sequence, and an index past the end of the 128-byte arrays.
 */
static int
run_matrix(void)
{
	int law, segidx, restart, s1z, s2z, lenmode;
	long input = 0;

	diff_begin("V90Phase3Modulator::generateDIL");
	cov_reset();

	for (law = 0; law < 2; law++)
	for (segidx = 0; segidx <= 8; segidx++)
	for (restart = 0; restart < 2; restart++)
	for (s1z = 0; s1z < 2; s1z++)
	for (s2z = 0; s2z < 2; s2z++)
	for (lenmode = 0; lenmode < 4; lenmode++) {
		long level = boundary[law][(int)(input % 8)] - (input % 3) + 1;

		prepare((int)input, (int)(input % 4), law, segidx, restart,
			s1z, s2z, lenmode, level);
		drive(input);
		input++;
	}

	diff_eq_int("matrix trials driven (%ld)", ntrial, 576, 0);
	diff_eq_int("both companding laws (%ld)",
		    cov_law[0] + cov_law[1], 2, 0);
	diff_eq_int("seq2[i] == 0 seen (%ld)", cov_seq2_zero, 1, 0);
	diff_eq_int("seq2[i] != 0 seen (%ld)", cov_seq2_nonzero, 1, 0);
	diff_eq_int("seq1[i] == 0 seen (%ld)", cov_seq1_zero, 1, 0);
	diff_eq_int("seq1[i] != 0 seen (%ld)", cov_seq1_nonzero, 1, 0);
	diff_eq_int("the segment restarted (%ld)", cov_restart, 1, 0);
	diff_eq_int("the segment did not restart (%ld)", cov_norestart, 1, 0);
	diff_eq_int("segmentIndex 8 driven in (%ld)", cov_segidx_in[8], 1, 0);
	diff_eq_int("seq1Index wrapped (%ld)", cov_seq1_wrap, 1, 0);
	diff_eq_int("seq1Index stepped (%ld)", cov_seq1_nowrap, 1, 0);
	diff_eq_int("seq2Index wrapped (%ld)", cov_seq2_wrap, 1, 0);
	diff_eq_int("seq2Index stepped (%ld)", cov_seq2_nowrap, 1, 0);
	diff_eq_int("a zero seq1Length was driven (%ld)", cov_len1_zero, 1, 0);
	diff_eq_int("a zero seq2Length was driven (%ld)", cov_len2_zero, 1, 0);
	diff_eq_int("dilIndex wrapped on dilCount (%ld)",
		    cov_dilindex_wrap, 1, 0);
	diff_eq_int("dilIndex stepped without wrapping (%ld)",
		    cov_dilindex_step, 1, 0);
	diff_eq_int("a negative symbol was emitted (%ld)", cov_level_neg, 1, 0);
	diff_eq_int("a positive symbol was emitted (%ld)", cov_level_pos, 1, 0);
	diff_eq_int("an index past the sequences was driven (%ld)",
		    cov_idx_past_seq, 1, 0);

	return diff_end();
}

/*
 * The boundary sweep.  Every trial restarts the segment, so the search in
 * `updateCodeSegmentPointer` runs on a level this loop chooses: each of the
 * eight boundaries of each row, one below it, exactly on it, and one above
 * it, plus the values that make the zero-extended read exceed 0x7fff.  The
 * compare is `<=` with a SIGNED branch on an UNSIGNED sixteen-bit load, so
 * "exactly on it" and "one above the last one" are the two cases that
 * separate a correct search from a plausible one.
 */
static int
run_boundaries(void)
{
	static const long extra[] = {
		0, 1, -1,		/* -1 reads as 65535 */
		0x7fff, -0x8000,	/* -32768 reads as 32768: A-law's last */
		-2, 0x4000, 0x2000
	};
	int law, k, d;
	long input = 0;

	diff_begin("V90Phase3Modulator::generateDIL boundary search");
	cov_reset();

	for (law = 0; law < 2; law++) {
		for (k = 0; k < 8; k++) {
			for (d = -1; d <= 1; d++) {
				long level = boundary[law][k] + d;
				unsigned int u;

				prepare((int)input, (int)(input % 4), law,
					(int)(input % 9), 1,
					(int)(input & 1), (int)((input >> 1) & 1),
					(int)(input % 3), level);
				drive(input);

				u = (unsigned int)(unsigned short)(short)level;
				if (d == 0)
					cov_boundary_exact = 1;
				else if (d < 0)
					cov_boundary_below = 1;
				else
					cov_boundary_above = 1;
				if (u > 0x7fffu)
					cov_code_high = 1;
				input++;
			}
		}
		for (k = 0; k < (int)(sizeof(extra) / sizeof(extra[0])); k++) {
			unsigned int u;

			prepare((int)input, (int)(input % 4), law,
				(int)(input % 9), 1,
				(int)(input & 1), (int)((input >> 1) & 1),
				(int)(input % 3), extra[k]);
			drive(input);

			u = (unsigned int)(unsigned short)(short)extra[k];
			if (u > 0x7fffu)
				cov_code_high = 1;
			input++;
		}
	}

	diff_eq_int("boundary trials driven (%ld)", ntrial, 64, 0);
	diff_eq_int("every trial restarted the segment (%ld)",
		    cov_norestart, 0, 0);
	diff_eq_int("a level exactly on a boundary (%ld)",
		    cov_boundary_exact, 1, 0);
	diff_eq_int("a level one below a boundary (%ld)",
		    cov_boundary_below, 1, 0);
	diff_eq_int("a level one above a boundary (%ld)",
		    cov_boundary_above, 1, 0);
	diff_eq_int("a level the search reads above 0x7fff (%ld)",
		    cov_code_high, 1, 0);
	diff_eq_int("the search produced segmentIndex 8 (%ld)",
		    cov_segidx_out[8], 1, 0);
	diff_eq_int("the search produced segmentIndex 7 (%ld)",
		    cov_segidx_out[7], 1, 0);
	diff_eq_int("the search produced segmentIndex 0 (%ld)",
		    cov_segidx_out[0], 1, 0);

	return diff_end();
}

/*
 * A DIL sequence run to its natural end and past it, without reseeding.  The
 * matrix above reseeds every trial on purpose; this one does not, because the
 * only way to see the four cursors interact -- a segment ending inside a
 * sequence that has not wrapped, a `dilIndex` walking a whole `dilCount`, a
 * segment index recomputed from a level the previous step chose -- is to let
 * one state feed the next.
 *
 * The grounded rate is finding F7621's: the digital arm emits 80 symbols a
 * block at 8 kHz, so 480 consecutive symbols is six blocks of a real session
 * and comfortably longer than any segment (`6 * size + 6` tops out at 54).
 */
static int
run_sequence(void)
{
	int law, mode;
	long input = 0;

	diff_begin("V90Phase3Modulator::generateDIL in sequence");
	cov_reset();

	for (law = 0; law < 2; law++) {
		for (mode = 0; mode < 4; mode++) {
			int step;

			prepare((int)input + 7, mode, law, mode % 9, 0,
				mode & 1, (mode >> 1) & 1, mode % 3,
				boundary[law][mode % 8]);
			/*
			 * Start inside the first segment rather than one below
			 * its end, so the run crosses the restart edge on its
			 * own schedule rather than immediately.
			 */
			ours.o.segmentPos = 0;
			ours.o.dilCount = (unsigned char)(2 + mode);
			ours.o.dilIndex = 0;
			copy_to_theirs();

			for (step = 0; step < 480; step++) {
				/*
				 * The two sentinels are restored every step:
				 * the previous call wrote them, and leaving
				 * them written would make a dropped store on
				 * the next call invisible whenever the value
				 * happened to repeat.
				 */
				ours.o.usingSegmentLevel =
				    theirs.o.usingSegmentLevel = 0x5a5a;
				ours.o.dilPcmCode =
				    theirs.o.dilPcmCode = 0x37;
				drive(input);
				input++;
			}
		}
	}

	diff_eq_int("sequence symbols driven (%ld)", ntrial, 3840, 0);
	diff_eq_int("both companding laws (%ld)",
		    cov_law[0] + cov_law[1], 2, 0);
	diff_eq_int("the segment restarted (%ld)", cov_restart, 1, 0);
	diff_eq_int("the segment did not restart (%ld)", cov_norestart, 1, 0);
	diff_eq_int("seq2[i] == 0 seen (%ld)", cov_seq2_zero, 1, 0);
	diff_eq_int("seq2[i] != 0 seen (%ld)", cov_seq2_nonzero, 1, 0);
	diff_eq_int("seq1[i] == 0 seen (%ld)", cov_seq1_zero, 1, 0);
	diff_eq_int("seq1[i] != 0 seen (%ld)", cov_seq1_nonzero, 1, 0);
	diff_eq_int("dilIndex wrapped on dilCount (%ld)",
		    cov_dilindex_wrap, 1, 0);
	diff_eq_int("seq1Index wrapped (%ld)", cov_seq1_wrap, 1, 0);
	diff_eq_int("seq2Index wrapped (%ld)", cov_seq2_wrap, 1, 0);

	return diff_end();
}

/*
 * The two edges the first version of this test missed, and mutation is what
 * found them (both rows are in test/mutations/v90dilgen.json).
 *
 * `pcmType` OUTSIDE {0, 1}.  The object's companding branch is `mov
 * 0x4(%ebx),%ecx ; test %ecx,%ecx ; je` -- it asks whether the law is mu-law
 * and not whether it is A-law -- so `pcmType != PCM_TYPE_MU_LAW` and
 * `pcmType == PCM_TYPE_A_LAW` agree over every value the rest of this file
 * drives and disagree for 2, 3 and every other non-zero value.  Those are
 * drivable HERE and only here, because the field is dangerous for exactly one
 * reason -- the boundary search indexes the table at `8 * pcmType` -- and the
 * search runs only on the restart arm.  So every trial in this group keeps
 * `segmentPos` well short of the segment's end, the table is never read, and
 * the only thing `pcmType` selects is the companding law.
 *
 * A LEVEL OF -32768.  It is the one value whose negation does not fit a
 * `short`, which is the whole content of the object's `neg %ecx ; movswl
 * %cx,%edi`: the result wraps back to -32768 rather than becoming 32768.  The
 * random and boundary groups never landed on it with `seq1[seq1Index] == 0`
 * at the same time, so it is driven deliberately, from both level sources and
 * on both arms of the restart.
 */
static int
run_edges(void)
{
	static const long odd_law[] = { 2, 3, 0xff, 0x4000, -1 };
	static const long extreme[] = { -0x8000, 0x7fff, 0, -1, 1 };
	int k, s2z, restart, law;
	long input = 0;

	diff_begin("V90Phase3Modulator::generateDIL edges");
	cov_reset();

	for (k = 0; k < (int)(sizeof(odd_law) / sizeof(odd_law[0])); k++) {
		for (s2z = 0; s2z < 2; s2z++) {
			int s1z;

			for (s1z = 0; s1z < 2; s1z++) {
				int c;

				/*
				 * THE LEVEL IS SWEPT, and a single fixed one
				 * was not enough: the two spellings of the law
				 * test choose different companding functions,
				 * and `linear2alaw(v) ^ 0xd5` and
				 * `~linear2ulaw(v)` agree on plenty of
				 * individual values.  The mutation row
				 * survived on one level and dies on sixteen.
				 */
				for (c = 0; c < 16; c++) {
					/*
					 * `prepare` is called for mu-law and
					 * the field is overwritten after: the
					 * group needs the rest of its state,
					 * and `restart` is 0 so nothing
					 * indexes the boundary table.
					 */
					prepare((int)input + 3,
						(int)(input % 4), 0,
						(int)(input % 8), 0, s1z, s2z,
						(int)(input % 3),
						(long)(short)(0x1111 * c + 7));
					ours.o.pcmType = theirs.o.pcmType =
					    (PcmType)odd_law[k];
					drive(input);
					input++;
				}
			}
		}
	}

	diff_eq_int("a pcmType outside {0,1} was driven (%ld)",
		    ntrial, 320, 0);
	diff_eq_int("no trial in the group restarted (%ld)", cov_restart, 0, 0);

	for (law = 0; law < 2; law++)
	for (k = 0; k < (int)(sizeof(extreme) / sizeof(extreme[0])); k++)
	for (restart = 0; restart < 2; restart++)
	for (s2z = 0; s2z < 2; s2z++) {
		/*
		 * `s1zero` is 1 throughout: the point of the group is the
		 * NEGATED extreme, and `seq1[seq1Index] == 0` is what negates.
		 * `s2zero` picks which array the level came from, so -32768 is
		 * negated once as a segment level and once as a DIL entry.
		 */
		prepare((int)input + 11, (int)(input % 4), law,
			(int)(input % 8), restart, 1, s2z,
			(int)(input % 3), extreme[k]);
		ours.o.segmentLevel[ours.o.segmentIndex & 7] =
		    theirs.o.segmentLevel[theirs.o.segmentIndex & 7] =
		    (short)extreme[k];
		copy_to_theirs();
		drive(input);
		input++;
	}

	diff_eq_int("edge trials driven (%ld)", ntrial, 360, 0);
	diff_eq_int("the negated symbol reached -32768 (%ld)",
		    saw_int16_min, 1, 0);
	diff_eq_int("both companding laws (%ld)",
		    cov_law[0] + cov_law[1], 2, 0);
	diff_eq_int("the segment restarted (%ld)", cov_restart, 1, 0);
	diff_eq_int("the segment did not restart (%ld)", cov_norestart, 1, 0);

	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_matrix();
	rc |= run_boundaries();
	rc |= run_sequence();
	rc |= run_edges();

	return rc;
}
