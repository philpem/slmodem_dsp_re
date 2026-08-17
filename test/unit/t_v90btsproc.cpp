/*
 * t_v90btsproc.cpp -- differential test of `V90BitsToSymbol::process(unsigned
 * int &, short *)`, `::setSymbolsBlockSize` and `::nofBitsForNextTime`.
 *
 * THE OBJECT IS 0x24 BYTES AND CARRIES THREE POINTERS, two of which nothing
 * under test dereferences.  `mapper` and `params` are therefore installed
 * with the SAME fake addresses on both sides and compare equal like any other
 * word.  `symbols` cannot be: `process` writes through it -- the leftover
 * shift is `symbols[j++] = symbols[i]` -- so the two sides need two buffers,
 * and the whole-object comparison normalises that one field and nothing else.
 * Each side's `symbols` is separately required to come back unchanged, so
 * normalising it cannot hide a body that moved it.
 *
 * WHAT EACH RUN HAS TO SEPARATE:
 *
 *   nofBitsForNextTime  BOTH ARMS OF THE DIVISION.  When the symbol count
 *                       divides by six the object multiplies first and
 *                       divides the product; when it does not, it divides
 *                       first and multiplies the incremented quotient.  The
 *                       two agree for every input, so no trial can tell them
 *                       apart -- what the sweep must do instead is reach both
 *                       BLOCKS, and it counts the trials that did.  It also
 *                       has to reach the early zero (`symbolsBlockSize <=
 *                       symbolsDone`) and both sides of `extraSymbolsPending`,
 *                       because the pending flag is the only thing that puts
 *                       `extraSymbols` into the sum.
 *
 *   setSymbolsBlockSize IT IS NOT `nofBitsForNextTime`.  The store to +0x1c
 *                       is what separates them, so every trial asks for a
 *                       block size the object does not already hold and
 *                       requires +0x1c to have moved.  The two members are
 *                       also run back to back on the same state and required
 *                       to agree, which is the inlining the blob shows.
 *
 *   process             FOUR OUTCOMES, counted: status 1 (`symbolsBlockSize`
 *                       zero), status 3 (fewer symbols ready than a block),
 *                       status 0 with nothing left over, and status 0 with a
 *                       leftover run that the shift loop actually moves.  The
 *                       last is the one a lazy sweep misses: with
 *                       `symbolsDone == symbolsBlockSize` the shift loop is
 *                       empty and a body that dropped it passes.
 *
 * THE REFERENCE PARAMETER IS SEEDED DIFFERENTLY ON THE TWO SIDES, because the
 * size-not-set path does NOT write it.  So "the two agree" would be satisfied
 * by two untouched zeros; instead each side starts from its own value, the
 * paths that write it are required to agree, and the path that does not is
 * required to leave each side's own value alone.
 *
 * THE TWO MESSAGES ARE COMPARED AS TRANSCRIPTS.  "SIZE_NOT_SET" and
 * "BUFFER_UNDERFLOW" name the two non-zero statuses, and neither string
 * reaches the object, so swapping them is invisible to every byte comparison
 * here.  The capture stays ENCODED -- `dsplib_encode_plain` is ours alone
 * (D40) -- and the run also requires the BLOB's own transcript for one status
 * to differ from its transcript for the other, which is a property of the
 * object rather than of this reconstruction.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/V90BitsToSymbol.h"

extern "C" {
extern unsigned int ref_dsplibs_debug_level;

unsigned int ref_bts_nofbits(void *)
	asm("ref__ZN15V90BitsToSymbol18nofBitsForNextTimeEv");
unsigned int ref_bts_setblocksize(void *, unsigned int)
	asm("ref__ZN15V90BitsToSymbol19setSymbolsBlockSizeEj");
/*
 * `unsigned int *` where the member takes `unsigned int &`: the same thing to
 * the ABI, and this side has to name a C type because the blob's symbol has
 * no class to be a member of.
 */
unsigned int ref_bts_process(void *, unsigned int *, short *)
	asm("ref__ZN15V90BitsToSymbol7processERjPs");
}

/* ------------------------------------------------------------------ seeds */

static unsigned lfsr;

static unsigned char
next_byte(int mode)
{
	lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xb400u);
	switch (mode) {
	case 1:
		return 0x5a;
	case 2:
		return 0xff;
	default:
		return (unsigned char)(lfsr >> 3);
	}
}

static void
set_level(unsigned lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/* ----------------------------------------------------------- the storage */

#define BTS_SLOT	((unsigned)sizeof(V90BitsToSymbol) + 32u)
#define SYM_N		64			/* symbols in each buffer   */
#define SYM_SLOT	(SYM_N + 16)		/* and a guard past them    */

static unsigned char bts_a[BTS_SLOT] __attribute__((aligned(8)));
static unsigned char bts_b[BTS_SLOT] __attribute__((aligned(8)));
static unsigned char bts_s[BTS_SLOT];

static short sym_a[SYM_SLOT], sym_b[SYM_SLOT], sym_s[SYM_SLOT];
static short out_a[SYM_SLOT], out_b[SYM_SLOT], out_s[SYM_SLOT];
static unsigned char before_a[BTS_SLOT], before_b[BTS_SLOT];

#define BTSA	((V90BitsToSymbol *)bts_a)
#define BTSB	((V90BitsToSymbol *)bts_b)

/*
 * Two addresses nothing here dereferences, installed identically on both
 * sides so the two pointer fields compare equal.  They are not null, because
 * a null would be a value the destructor treats specially and this file must
 * not depend on which of the two it looks like.
 */
static unsigned char fake_mapper[8];
static unsigned char fake_params[8];

static void
seed_pair(int trial, int mode)
{
	unsigned i;

	lfsr = 0x1d4bu + 0x9e37u * (unsigned)trial + 0x51edu * (unsigned)mode;
	for (i = 0; i < BTS_SLOT; i++) {
		unsigned char v = next_byte(mode);

		bts_a[i] = v;
		bts_b[i] = v;
		bts_s[i] = v;
	}
	for (i = 0; i < SYM_SLOT; i++) {
		short v = (short)(0x2000 + (int)i * 37 + trial);

		sym_a[i] = sym_b[i] = sym_s[i] = v;
		out_a[i] = out_b[i] = out_s[i] = (short)(0x7000 - (int)i * 11);
	}

	BTSA->mapper = BTSB->mapper = (V90Mapper *)fake_mapper;
	BTSA->params = BTSB->params = (V90Parameters *)fake_params;
	BTSA->symbols = sym_a;
	BTSB->symbols = sym_b;
	BTSA->nofSymbols = BTSB->nofSymbols = SYM_N;
}

/*
 * Compare the whole object with the ONE field that cannot match normalised
 * away, and check separately that neither side moved it.
 */
static void
compare_pair(const char *what, long tag)
{
	unsigned char na[BTS_SLOT], nb[BTS_SLOT];
	unsigned n = BTS_SLOT - (unsigned)sizeof(V90BitsToSymbol);
	short *za = 0;

	diff_eq_int("ours kept its symbol buffer (%ld)",
		    (long)(BTSA->symbols == sym_a), 1, tag);
	diff_eq_int("the blob kept its symbol buffer (%ld)",
		    (long)(BTSB->symbols == sym_b), 1, tag);

	memcpy(na, bts_a, BTS_SLOT);
	memcpy(nb, bts_b, BTS_SLOT);
	memcpy(na + __builtin_offsetof(V90BitsToSymbol, symbols), &za,
	       sizeof(za));
	memcpy(nb + __builtin_offsetof(V90BitsToSymbol, symbols), &za,
	       sizeof(za));

	diff_eq_obj_(__FILE__, __LINE__, what, "V90BitsToSymbol", na, nb,
		     sizeof(V90BitsToSymbol), tag);
	diff_eq_int("ours stored past the object (%ld)",
		    memcmp(bts_a + sizeof(V90BitsToSymbol),
			   bts_s + sizeof(V90BitsToSymbol), n) == 0, 1, tag);
	diff_eq_int("the blob stored past the object (%ld)",
		    memcmp(bts_b + sizeof(V90BitsToSymbol),
			   bts_s + sizeof(V90BitsToSymbol), n) == 0, 1, tag);
}

/* Both symbol buffers, both output buffers, and the guards past all four. */
static void
compare_buffers(long tag)
{
	diff_eq_int("the symbol buffers agree (%ld)",
		    memcmp(sym_a, sym_b, SYM_N * sizeof(short)) == 0, 1, tag);
	diff_eq_int("the output buffers agree (%ld)",
		    memcmp(out_a, out_b, SYM_N * sizeof(short)) == 0, 1, tag);
	diff_eq_int("ours stayed inside the symbol buffer (%ld)",
		    memcmp(sym_a + SYM_N, sym_s + SYM_N,
			   (SYM_SLOT - SYM_N) * sizeof(short)) == 0, 1, tag);
	diff_eq_int("the blob stayed inside the symbol buffer (%ld)",
		    memcmp(sym_b + SYM_N, sym_s + SYM_N,
			   (SYM_SLOT - SYM_N) * sizeof(short)) == 0, 1, tag);
	diff_eq_int("ours stayed inside the output buffer (%ld)",
		    memcmp(out_a + SYM_N, out_s + SYM_N,
			   (SYM_SLOT - SYM_N) * sizeof(short)) == 0, 1, tag);
	diff_eq_int("the blob stayed inside the output buffer (%ld)",
		    memcmp(out_b + SYM_N, out_s + SYM_N,
			   (SYM_SLOT - SYM_N) * sizeof(short)) == 0, 1, tag);
}

/* ------------------------------------------- nofBitsForNextTime (134 B) */

/*
 * The states the sweep visits, chosen so that every arm is reached and the
 * `% 6` question is answered both ways.  `block`, `done`, `extra`, `pending`,
 * `bpf`.
 */
struct bts_state {
	unsigned int block, done, extra, bpf;
	unsigned char pending;
};

static const struct bts_state states[] = {
	{  0u,  0u,  4u, 28u, 1 },	/* block 0: the early zero        */
	{  6u,  6u,  4u, 28u, 1 },	/* equal:   the early zero        */
	{  6u,  9u,  4u, 28u, 0 },	/* done past block: early zero    */
	{ 12u,  6u,  0u, 28u, 1 },	/* 6 owed,  pending, extra 0: %6  */
	{ 12u,  6u,  3u, 28u, 1 },	/* 9 owed:  not a multiple of 6   */
	{ 12u,  6u,  3u, 28u, 0 },	/* pending clear: 6, a multiple   */
	{ 18u,  1u,  0u, 32u, 0 },	/* 17 owed: not a multiple        */
	{ 24u,  0u, 12u, 40u, 1 },	/* 36 owed: a multiple            */
	{ 24u,  0u, 11u, 40u, 1 },	/* 35 owed: not                   */
	{  1u,  0u,  0u,  7u, 0 },	/* 1 owed:  not                   */
	{ 60u,  0u,  0u, 56u, 0 },	/* 60 owed: a multiple            */
	{ 63u,  3u,  6u, 56u, 1 }	/* 66 owed: a multiple            */
};

#define NSTATES	((int)(sizeof(states) / sizeof(states[0])))

static void
install(const struct bts_state *s)
{
	BTSA->symbolsBlockSize = BTSB->symbolsBlockSize = s->block;
	BTSA->symbolsDone = BTSB->symbolsDone = s->done;
	BTSA->extraSymbols = BTSB->extraSymbols = s->extra;
	BTSA->bitsPerFrame = BTSB->bitsPerFrame = s->bpf;
	BTSA->extraSymbolsPending = BTSB->extraSymbolsPending = s->pending;
}

static int
run_bts_nofbits(void)
{
	int trial;
	int zero = 0, mult = 0, notmult = 0, pend = 0, nopend = 0, varied = 0;
	unsigned int first = 0;

	diff_begin("V90BitsToSymbol::nofBitsForNextTime");
	set_level(0);

	for (trial = 0; trial < 3 * NSTATES; trial++) {
		const struct bts_state *s = &states[trial % NSTATES];
		long tag = 5000 + trial;
		unsigned int ra, rb, owed;

		seed_pair(trial + 100, trial % 3);
		install(s);
		memcpy(before_a, bts_a, BTS_SLOT);
		memcpy(before_b, bts_b, BTS_SLOT);

		ra = BTSA->nofBitsForNextTime();
		rb = ref_bts_nofbits(bts_b);

		diff_eq_int("the answers match (%ld)", (long)ra, (long)rb,
			    tag);
		compare_pair("after nofBitsForNextTime", tag);
		compare_buffers(tag);

		/* It reads and does not write: neither object may move. */
		diff_eq_int("ours wrote nothing (%ld)",
			    memcmp(before_a, bts_a, BTS_SLOT) == 0, 1, tag);
		diff_eq_int("the blob wrote nothing (%ld)",
			    memcmp(before_b, bts_b, BTS_SLOT) == 0, 1, tag);

		if (s->block <= s->done) {
			zero = 1;
			diff_eq_int("the early zero (%ld)", (long)rb, 0, tag);
		} else {
			owed = s->block - s->done;
			if (s->pending)
				owed += s->extra;
			if (owed % 6u == 0u) {
				mult = 1;
				diff_eq_int("the exact arm (%ld)", (long)rb,
					    (long)(owed * s->bpf / 6u), tag);
			} else {
				notmult = 1;
				diff_eq_int("the rounding arm (%ld)", (long)rb,
					    (long)((owed / 6u + 1u) * s->bpf),
					    tag);
			}
			if (s->pending)
				pend = 1;
			else
				nopend = 1;
			if (trial == 0 || first == 0)
				first = rb;
			else if (rb != first)
				varied = 1;
		}
	}

	diff_eq_int("the early zero was reached", zero, 1, 0);
	diff_eq_int("the exact arm was reached", mult, 1, 0);
	diff_eq_int("the rounding arm was reached", notmult, 1, 0);
	diff_eq_int("a pending extra was counted in", pend, 1, 0);
	diff_eq_int("a cleared pending flag was tried", nopend, 1, 0);
	diff_eq_int("the answer varied between trials", varied, 1, 0);
	return diff_end();
}

/* ------------------------------------------ setSymbolsBlockSize (137 B) */

static int
run_bts_setblocksize(void)
{
	static const unsigned int sizes[] = { 0u, 1u, 6u, 7u, 12u, 30u, 63u };
	int trial, moved = 0, agreed = 0, differed = 0;

	diff_begin("V90BitsToSymbol::setSymbolsBlockSize");
	set_level(0);

	for (trial = 0; trial < 3 * NSTATES; trial++) {
		const struct bts_state *s = &states[trial % NSTATES];
		unsigned int want =
		    sizes[(unsigned)trial % (sizeof(sizes) /
					     sizeof(sizes[0]))];
		long tag = 5400 + trial;
		unsigned int ra, rb, again;

		seed_pair(trial + 200, trial % 3);
		install(s);

		ra = BTSA->setSymbolsBlockSize(want);
		rb = ref_bts_setblocksize(bts_b, want);

		diff_eq_int("the answers match (%ld)", (long)ra, (long)rb,
			    tag);
		diff_eq_int("the block size was stored (%ld)",
			    (long)BTSB->symbolsBlockSize, (long)want, tag);
		compare_pair("after setSymbolsBlockSize", tag);
		compare_buffers(tag);

		if (want != s->block)
			moved = 1;

		/*
		 * It is `nofBitsForNextTime` with a store in front: asking the
		 * blob the second question straight afterwards must give the
		 * same number, and the state it left behind must not move.
		 */
		again = ref_bts_nofbits(bts_b);
		diff_eq_int("and it is nofBitsForNextTime (%ld)", (long)again,
			    (long)rb, tag);
		if (rb != 0)
			agreed = 1;
		if (want <= BTSB->symbolsDone)
			differed = 1;
	}

	diff_eq_int("the block size actually changed", moved, 1, 0);
	diff_eq_int("a non-zero demand was produced", agreed, 1, 0);
	diff_eq_int("a zero demand was produced", differed, 1, 0);
	return diff_end();
}

/* ----------------------------------------------------- process (375 B) */

#define BTS_TEXT	512

static int
run_bts_process(void)
{
	static char text_size[BTS_TEXT];
	static char text_under[BTS_TEXT];
	int trial;
	int st0 = 0, st1 = 0, st3 = 0, kept_run = 0, kept_none = 0;
	int spoke = 0, silent = 0, cleared = 0, wascLear = 0;
	int size_loud = 0, midgate = 0;

	diff_begin("V90BitsToSymbol::process");

	for (trial = 0; trial < 4 * NSTATES; trial++) {
		const struct bts_state *s = &states[trial % NSTATES];
		long tag = 5800 + trial;
		unsigned int bits_a = 0xa5a5a5a5u, bits_b = 0x5a5a5a5au;
		unsigned int ra, rb, done_before, block_before;
		/*
		 * THREE LEVELS AND NOT TWO.  `dsplibs_debug_level > 1` and
		 * `> 0` differ at exactly one value, so a sweep that only ever
		 * uses 0 and 2 cannot tell the object's gate from a looser
		 * one.  The level changes once per PASS over the state table
		 * rather than per trial: `trial % 3` shares a factor with the
		 * table's twelve entries, and the state that answers
		 * SIZE_NOT_SET then never coincides with a level above the
		 * gate -- which is exactly the hole the swapped-message
		 * mutation lived in.
		 */
		unsigned lvl = (unsigned)((trial / NSTATES) % 3);

		seed_pair(trial + 300, trial % 3);
		install(s);

		/*
		 * Every fourth pass leaves MORE symbols ready than a block, so
		 * the leftover shift has something to move.  Nothing else in
		 * the sweep reaches it.
		 */
		if ((trial % 4) == 3 && s->block != 0) {
			BTSA->symbolsDone = BTSB->symbolsDone =
			    s->block + 1u + (unsigned)(trial % 5);
		}
		done_before = BTSB->symbolsDone;
		block_before = BTSB->symbolsBlockSize;

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		ra = BTSA->process(bits_a, out_a);
		rb = ref_bts_process(bts_b, &bits_b, out_b);
		dsplib_debug_capture_on = 0;

		diff_eq_int("the statuses match (%ld)", (long)ra, (long)rb,
			    tag);
		diff_eq_int("the transcripts match (%ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1, tag);
		compare_pair("after process", tag);
		compare_buffers(tag);

		if (s->pending)
			cleared = 1;
		else
			wascLear = 1;
		diff_eq_int("the pending flag is clear afterwards (%ld)",
			    (long)BTSB->extraSymbolsPending, 0, tag);

		if (block_before == 0u) {
			st1 = 1;
			diff_eq_int("SIZE_NOT_SET is 1 (%ld)", (long)rb, 1,
				    tag);
			/*
			 * THE ONE PATH THAT DOES NOT WRITE THE REFERENCE, so
			 * each side must still hold its own seed.
			 */
			diff_eq_int("ours left the reference alone (%ld)",
				    (long)bits_a, (long)0xa5a5a5a5u, tag);
			diff_eq_int("the blob left the reference alone (%ld)",
				    (long)bits_b, (long)0x5a5a5a5au, tag);
			diff_eq_int("nothing was copied out (%ld)",
				    memcmp(out_b, out_s,
					   SYM_N * sizeof(short)) == 0, 1,
				    tag);
			diff_eq_int("symbolsDone was not touched (%ld)",
				    (long)BTSB->symbolsDone,
				    (long)done_before, tag);
		} else {
			diff_eq_int("the bit demands match (%ld)",
				    (long)bits_a, (long)bits_b, tag);
			/*
			 * THE DEMAND IS COMPUTED BEFORE THE PENDING FLAG IS
			 * CLEARED, so asking the blob afterwards only agrees
			 * with the flag put back as it was.  That ordering is
			 * itself the claim being checked.
			 */
			BTSB->extraSymbolsPending = s->pending;
			diff_eq_int("the demand is nofBitsForNextTime (%ld)",
				    (long)bits_b,
				    (long)ref_bts_nofbits(bts_b), tag);
			BTSB->extraSymbolsPending = 0;

			if (done_before < block_before) {
				st3 = 1;
				diff_eq_int("BUFFER_UNDERFLOW is 3 (%ld)",
					    (long)rb, 3, tag);
				diff_eq_int("nothing was kept (%ld)",
					    (long)BTSB->symbolsDone, 0, tag);
				diff_eq_int("what was ready was copied out "
					    "(%ld)",
					    memcmp(out_b, sym_s,
						   done_before *
						   sizeof(short)) == 0, 1,
					    tag);
			} else {
				st0 = 1;
				diff_eq_int("the quiet path is 0 (%ld)",
					    (long)rb, 0, tag);
				diff_eq_int("a whole block was copied out "
					    "(%ld)",
					    memcmp(out_b, sym_s,
						   block_before *
						   sizeof(short)) == 0, 1,
					    tag);
				diff_eq_int("what was left over is kept "
					    "(%ld)",
					    (long)BTSB->symbolsDone,
					    (long)(done_before -
						   block_before), tag);
				if (done_before > block_before) {
					kept_run = 1;
					diff_eq_int("and it was shifted down "
						    "(%ld)",
						    memcmp(sym_b,
							   sym_s +
							   block_before,
							   (done_before -
							    block_before) *
							   sizeof(short)) == 0,
						    1, tag);
				} else {
					kept_none = 1;
				}
			}
		}

		if (lvl > 1) {
			if (rb != 0) {
				if (rb == 1)
					size_loud = 1;
				diff_eq_int("above the gate the blob spoke "
					    "(%ld)",
					    (long)(strlen(
						dsplib_debug_capture_text(1)) >
						0), 1, tag);
				spoke = 1;
			}
		} else {
			diff_eq_int("below the gate the blob was silent (%ld)",
				    (long)strlen(dsplib_debug_capture_text(1)),
				    0, tag);
			diff_eq_int("below the gate ours was silent (%ld)",
				    (long)strlen(dsplib_debug_capture_text(0)),
				    0, tag);
			if (lvl == 1u && rb != 0)
				midgate = 1;
			silent = 1;
		}
	}

	/*
	 * THE TWO MESSAGES ARE DIFFERENT MESSAGES, answered by two runs of
	 * the BLOB and nothing else.  Swap the strings in the source and this
	 * is the only check in the file that moves.
	 */
	{
		unsigned int bits = 0;
		unsigned n;

		set_level(2);

		seed_pair(900, 0);
		install(&states[0]);		/* block 0: SIZE_NOT_SET */
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		diff_eq_int("the blob answered SIZE_NOT_SET (%ld)",
			    (long)ref_bts_process(bts_b, &bits, out_b), 1,
			    6400);
		dsplib_debug_capture_on = 0;
		n = (unsigned)strlen(dsplib_debug_capture_text(1));
		if (n >= BTS_TEXT)
			n = BTS_TEXT - 1;
		memcpy(text_size, dsplib_debug_capture_text(1), n);
		text_size[n] = '\0';

		seed_pair(901, 0);
		install(&states[3]);
		BTSB->symbolsDone = 0;		/* fewer ready than a block */
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		diff_eq_int("the blob answered BUFFER_UNDERFLOW (%ld)",
			    (long)ref_bts_process(bts_b, &bits, out_b), 3,
			    6401);
		dsplib_debug_capture_on = 0;
		n = (unsigned)strlen(dsplib_debug_capture_text(1));
		if (n >= BTS_TEXT)
			n = BTS_TEXT - 1;
		memcpy(text_under, dsplib_debug_capture_text(1), n);
		text_under[n] = '\0';

		diff_eq_int("the blob said something (%ld)",
			    (long)(strlen(text_size) > 0), 1, 6402);
		diff_eq_int("the two messages are different (%ld)",
			    strcmp(text_size, text_under) != 0, 1, 6402);
	}

	set_level(0);
	diff_eq_int("the quiet status was reached", st0, 1, 0);
	diff_eq_int("SIZE_NOT_SET was reached", st1, 1, 0);
	diff_eq_int("BUFFER_UNDERFLOW was reached", st3, 1, 0);
	diff_eq_int("a leftover run was shifted down", kept_run, 1, 0);
	diff_eq_int("an exactly-emptied block was tried", kept_none, 1, 0);
	diff_eq_int("the gate was tried open", spoke, 1, 0);
	diff_eq_int("SIZE_NOT_SET was printed", size_loud, 1, 0);
	diff_eq_int("the level ON the gate was tried", midgate, 1, 0);
	diff_eq_int("the gate was tried shut", silent, 1, 0);
	diff_eq_int("a set pending flag was cleared", cleared, 1, 0);
	diff_eq_int("an already-clear pending flag was tried", wascLear, 1, 0);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_bts_nofbits();
	rc |= run_bts_setblocksize();
	rc |= run_bts_process();

	return rc;
}
