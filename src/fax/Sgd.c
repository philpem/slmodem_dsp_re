/*
 * Sgd.c -- the SGD sequence generator/detector.
 *
 * Reconstructed from dsplibs.o:
 *
 *   SGD_create        .text 0x09f300    261 bytes
 *   SGD_delete        .text 0x09f410     32 bytes
 *   SGD_pattern_det   .text 0x09f430    240 bytes
 *   SGD_sequence_det  .text 0x09f520    358 bytes
 *   SGD_symbol_gen    .text 0x09f690    161 bytes
 *   SGD_sequence_gen  .text 0x09f740    246 bytes
 *   SGD_correlate     .text 0x09f840    114 bytes
 *   SGD_control       .text 0x09f8c0    235 bytes
 *   SGD_status        .text 0x09f9b0     84 bytes
 *   SGD_CFG           .data 0x0080e0     52 bytes
 *   SGD_CTL           .bss  0x0008c8      8 bytes
 *
 * Definitions are in the object's own address order.  `include/dsplib/sgd.h`
 * carries the object model and the derivation of every field name; this file
 * carries the arithmetic and the five places it does something a reader
 * would otherwise assume was a transcription error.
 *
 * WHAT THIS FILE IS THE SECOND ATTEMPT AT.  Wave 1 wrote these functions and
 * withdrew them (finding F8497) when 96 of 3,603 checks failed with
 * `status.det_at` landing megabytes outside the history buffer, and read
 * that as a layout error -- "F8495's object model must be treated as
 * UNCONFIRMED for the status half".  Re-reading the object settles it the
 * other way: THE MODEL WAS RIGHT AND THE OBJECT IS DOING THAT ON PURPOSE.
 * `SGD_sequence_det` never initialises its best-alignment index, and when
 * its search loop does not run -- which is exactly n <= 0 -- it both returns
 * that index and computes `det_at` from it.  See D1060 at the function.
 *
 * FIVE THINGS THE ORIGINAL DOES THAT LOOK LIKE MISTAKES AND ARE REPRODUCED:
 * D1060 (the uninitialised index above), D1061 (the history allocation is
 * sized from `hist_extra` and zeroed from `ref_len`, so they must agree),
 * D1062 (`SGD_pattern_det`'s bit mask shifts ARITHMETICALLY, so sym_bits ==
 * 16 spins for ever), D1063 (`SGD_sequence_det`'s window base is truncated
 * to an unsigned short, so n > hist_len indexes the history from 65535
 * downwards) and D1064 (`SGD_correlate`'s scale is 1/(p[0]*n) in INTEGERS --
 * zero for every configuration where that product exceeds 1 in magnitude,
 * and a divide fault when it is zero).  docs/deviations.md has each.
 */

#include <stddef.h>

#include "dsplib/fpm.h"
#include "dsplib/sgd.h"
#include "dsplib/sysdep.h"

/*
 * The default configuration, .data 0x80e0.
 *
 * Both table pointers are relocations against FPM_xor_table, which is a
 * popcount table and not a symbol alphabet: the object ships this default
 * with two PLACEHOLDERS in the pointer slots, not with a usable sequence.
 * Reproduced as the object has it -- `SGD_create(x, 0)` is only a usable
 * construction because every caller replaces both halves through
 * `SGD_control` before generating or detecting anything.
 *
 * It lives in .data, not .rodata, so it is not declared const.
 */
struct sgd_cfg SGD_CFG = {
	8,				/* sym_bits    */
	50,				/* hist_len    */
	1,				/* hist_extra  */
	0,				/* short_0006  */
	{
		(const unsigned short *)FPM_xor_table,	/* gen.seq        */
		1,			/* gen.seq_len    */
		0,			/* gen.short_0006 */
		0,			/* gen.seq_enable */
		0,			/* gen.idle_sym   */
		0,			/* gen.short_000e */
		0,			/* gen.data_word  */
		0,			/* gen.short_0012 */
		1,			/* gen.word_syms  */
		0			/* gen.short_0016 */
	},
	{
		(const unsigned short *)FPM_xor_table,	/* det.ref        */
		1,			/* det.ref_len      */
		0x2000,			/* det.ref_margin   */
		0,			/* det.pat_match    */
		0,			/* det.pat_mask     */
		0			/* det.pat_out_mask */
	}
};

/*
 * .bss 0x0008c8, 8 bytes -- `struct sgd_control_req`, shared TX-side
 * scratch and NOT this file's own protocol table.  V.17, V.27ter and V.29's
 * transmit half-duplex machines (`TxNextStateV17`/`V27`/`V29`) are its only
 * thirteen referrers in the whole object, every one a plain load of the
 * `det` half at +4 -- never a store, anywhere -- so this global's `det` is
 * always NULL wherever those callers use it and its `gen` half is dead.
 * A bare name with no protocol prefix, read symmetrically by all three
 * modulations' transmit families, is shared infrastructure and not any one
 * of theirs; it belongs beside `SGD_CFG`, the object's other shared SGD
 * default.  Findings F9600 and F9700.
 *
 * It is zero-initialised .bss, not .data: the object never stores a value
 * into it, so there is nothing to give it but zero.
 */
struct sgd_control_req SGD_CTL;

/*
 * Construct, or re-arm a caller-supplied object.
 *
 * THE HISTORY BUFFER IS ALLOCATED ONLY ON THE FRESH PATH, and the zeroing
 * that follows runs on BOTH paths.  So a caller passing its own object is
 * required to have `hist` already pointing at something -- the constructor
 * writes hist_span symbols through it either way.
 *
 * D1061: THE ALLOCATION AND THE ZEROED SPAN ARE COMPUTED FROM DIFFERENT
 * FIELDS.  The buffer is 2*(hist_len+hist_extra)-2 bytes, that is
 * hist_len+hist_extra-1 symbols; the span zeroed here, and the span
 * `SGD_sequence_det` slides over, is hist_len+ref_len-1 symbols.  Any
 * configuration with ref_len > hist_extra overruns its own buffer inside the
 * constructor.  `DSPLIB_REPRODUCE_BUGS` retains that object behavior for the
 * differential tier; the normal build allocates the larger span.  The blob's
 * own SGD_CFG has hist_extra == ref_len == 1, which is the only relation that
 * makes the two agree.
 */
struct sgd *
SGD_create(struct sgd *s, const struct sgd_cfg *cfg)
{
	int fresh = 0;
	unsigned short i;

	if (s == NULL) {
		s = sysdep_malloc(sizeof(struct sgd));
		fresh = 1;
	}
	if (cfg == NULL)
		cfg = &SGD_CFG;

	s->cfg = *cfg;
	s->seq_pos = 0;
	s->status.seq_reps = 0;
	s->word_left = (short)(s->cfg.gen.word_syms - 1);

	if (fresh) {
#ifdef DSPLIB_REPRODUCE_BUGS
		s->hist = sysdep_malloc(
		    (unsigned short)(2 * (s->cfg.hist_len + s->cfg.hist_extra)
				     - 2));
#else
		unsigned short hist_len = s->cfg.hist_len;
		unsigned short extra = s->cfg.hist_extra;

		/* The constructor clears through ref_len, not hist_extra. */
		if (extra < s->cfg.det.ref_len)
			extra = s->cfg.det.ref_len;
		s->hist = sysdep_malloc(2U * (hist_len + extra - 1U));
#endif
	}

	s->hist_span = (short)(s->cfg.hist_len + s->cfg.det.ref_len - 1);
	for (i = 0; i < s->hist_span; i++)
		s->hist[i] = 0;

	s->pat_sr = 0;
	s->status.seq_found = 0;
	s->status.det_at = NULL;
	s->status.quality = 0;
	s->status.pat_found = 0;
	s->status.pat_data = 0;
	s->thresh = (short)(s->cfg.sym_bits * s->cfg.det.ref_len
			    * (0x4000 - s->cfg.det.ref_margin));
	return s;
}

void
SGD_delete(struct sgd *s)
{
	sysdep_free(s->hist);
	sysdep_free(s);
}

/*
 * Bit-serial pattern matcher.
 *
 * Each symbol is walked from bit sym_bits-1 down to bit 0, each bit shifted
 * into a 32-bit register, and the register compared against pat_match under
 * pat_mask after EVERY bit.  On a hit the remaining bits of the symbol that
 * carried it are still shifted in -- so a caller resuming after a match sees
 * a register aligned to the symbol boundary, not to the match -- and the
 * index of that symbol is returned.  Returns -1 with the register saved when
 * the whole run passes without a hit.
 *
 * D1062: THE BIT MASK IS A SIGNED SHORT AND SHIFTS ARITHMETICALLY.  It
 * starts at 1 << (sym_bits-1); for sym_bits == 16 that is (short)0x8000, and
 * `>>= 1` walks it to -1 and stays there, so the inner loop never
 * terminates.  Reproduced.  sym_bits == 0 is harmless by accident: the
 * 32-bit shift by 31 leaves zero in the low half and the loop is skipped.
 */
short
SGD_pattern_det(struct sgd *s, const short *sym, short n)
{
	int sr = s->pat_sr;
	int mask = s->cfg.det.pat_mask;
	int match = s->cfg.det.pat_match;
	short i = 0;
	short k;

	for (k = (short)(n - 1); k != -1; k = (short)(k - 1)) {
		short bit = (short)(1 << (s->cfg.sym_bits - 1));
		short w = *sym++;

		while (bit != 0) {
			sr += sr;
			sr |= (w & bit) != 0;
			bit = (short)(bit >> 1);

			if ((sr & mask) == match) {
				s->status.pat_found = 1;
				s->status.pat_data =
				    sr & s->cfg.det.pat_out_mask;
				while (bit != 0) {
					sr += sr;
					sr |= (w & bit) != 0;
					bit = (short)(bit >> 1);
				}
				s->pat_sr = sr;
				return i;
			}
		}
		i = (short)(i + 1);
	}

	s->pat_sr = sr;
	return -1;
}

/*
 * Sequence detector.
 *
 * The history slides down by n symbols, the n new ones are appended at the
 * top, and the reference sequence is then correlated against n successive
 * alignments starting at hist_len-n.  The winner is the smallest Hamming
 * distance, ties going to the earliest alignment, with an early exit on an
 * exact match.  A distance above `thresh` is no detection at all and answers
 * -1 without touching the status block.
 *
 * D1060: `best_i` IS DELIBERATELY LEFT UNINITIALISED, because the object
 * leaves it uninitialised.  It is written only inside the improvement test,
 * and the search loop does not run at all for n <= 0 -- at which point the
 * threshold test still passes (`best` is 0xffff, which the signed compare
 * reads as -1, and thresh is non-negative for every sane configuration), so
 * `det_at` is computed from a stack slot nobody wrote and the function
 * returns its low half.  THIS IS THE WHOLE OF WAVE 1's FAILURE (F8497): a
 * `det_at` offset of -6181684 is that stack slot, not a mis-modelled field.
 * `t_faxsgd` therefore drives n >= 1 and says so.  Reproduced, not repaired
 * -- initialising it would be a behavioural change on a path the object
 * reaches.
 *
 * D1063: `base` IS TRUNCATED TO AN UNSIGNED SHORT.  For n > hist_len it
 * wraps to just under 65536 and every alignment indexes the history far past
 * its end.  Reproduced; the caller is expected to keep n <= hist_len.
 *
 * F8494: `FPM_xor_table` HAS 256 ENTRIES AND IS INDEXED WITH THE FULL 16-BIT
 * XOR, so a reference and a history symbol differing above bit 7 read past
 * it -- into the blob's neighbouring .rodata there and ours here, which are
 * not the same bytes.  Nothing in this function bounds that; the alphabet
 * does.
 */
short
SGD_sequence_det(struct sgd *s, const unsigned short *sym, short n)
{
	unsigned short *hist = s->hist;
	unsigned short *w;
	const unsigned short *r;
	unsigned short best = 0xffff;
	int best_i;			/* D1060 -- uninitialised on purpose */
	unsigned short base;
	unsigned short i;
	unsigned short j;

	w = hist;
	r = hist + n;
	for (j = (unsigned short)((unsigned short)(s->hist_span - n) - 1);
	     j != 0xffffu; j = (unsigned short)(j - 1))
		*w++ = *r++;

	for (j = (unsigned short)((unsigned short)n - 1);
	     j != 0xffffu; j = (unsigned short)(j - 1))
		*w++ = *sym++;

	base = (unsigned short)(s->cfg.hist_len - n);		/* D1063 */

	for (i = 0; (int)i < (int)n; i = (unsigned short)(i + 1)) {
		const unsigned short *p = hist + base + i;
		const unsigned short *q = s->cfg.det.ref;
		unsigned short acc = 0;
		unsigned short c;

		for (c = (unsigned short)(s->cfg.det.ref_len - 1);
		     c != 0xffffu; c = (unsigned short)(c - 1))
			acc = (unsigned short)(acc + FPM_xor_table[
			    (unsigned short)(*q++ ^ *p++)]);

		if (acc < best) {
			best_i = i;
			best = acc;
			if (acc == 0)
				break;
		}
	}

	if (s->thresh < (short)best)
		return -1;

	s->status.seq_found = 1;
	s->status.det_at = hist + base + best_i;
	/*
	 * INTEGER division, so this is 1 for every distance below
	 * sym_bits*ref_len and 0 or negative above it -- a two-or-three-valued
	 * flag rather than the ratio its arithmetic suggests.  It divides by
	 * zero if either factor is zero.
	 */
	s->status.quality = (short)(1 - (short)best
	    / ((short)s->cfg.sym_bits * (short)s->cfg.det.ref_len));
	return (short)best_i;
}

/*
 * Shift one data word out, most significant field first.
 *
 * `word_left` counts down through the fields of `data_word` and wraps back
 * to word_syms-1, so successive calls emit the word over and over.  The
 * return is how many fields of the current word have been consumed:
 * (word_syms-1) - word_left.
 */
short
SGD_symbol_gen(struct sgd *s, unsigned short *out, short n)
{
	int data = s->cfg.gen.data_word;
	int bits = s->cfg.sym_bits;
	int k = s->word_left;
	short last = (short)(s->cfg.gen.word_syms - 1);
	unsigned short mask = (unsigned short)((1 << (unsigned char)bits) - 1);
	short i;

	for (i = (short)(n - 1); i != -1; i = (short)(i - 1)) {
		short next = (short)(k - 1);

		*out++ = (unsigned short)((data >> (k * bits)) & mask);
		k = next < 0 ? last : next;
	}

	s->word_left = (short)k;
	return (short)(last - k);
}

/*
 * Emit the training sequence.
 *
 * The generator is "active" when it is either configured to repeat or has
 * not yet completed one pass; an inactive generator emits `idle_sym`.  On
 * reaching the end of the sequence a repeating generator bumps `seq_reps`
 * and restarts, and a one-shot generator SETS `seq_reps` TO 1 rather than
 * incrementing it and goes inactive -- so seq_reps means "passes completed"
 * for the first, and is a bare "done" flag for the second.
 *
 * The return is how many symbols of the sequence remain: seq_len - seq_pos,
 * which is 0 for a completed one-shot pass and seq_len - (position within
 * the current repetition) for a repeating one.
 */
short
SGD_sequence_gen(struct sgd *s, unsigned short *out, short n)
{
	int enable = s->cfg.gen.seq_enable;
	const unsigned short *seq = s->cfg.gen.seq;
	short i = (short)s->seq_pos;
	int active = 0;
	unsigned short len;
	short k;

	if (enable != 0 || s->status.seq_reps == 0)
		active = 1;

	for (k = (short)(n - 1); k != -1; k = (short)(k - 1)) {
		unsigned short sym;

		if (active != 0) {
			sym = seq[i];
			i = (short)(i + 1);
		} else {
			sym = s->cfg.gen.idle_sym;
		}
		*out++ = sym;

		len = s->cfg.gen.seq_len;
		if ((short)len <= i) {
			if (enable != 0) {
				s->status.seq_reps = (unsigned short)
				    (s->status.seq_reps + 1);
				i = 0;
			} else {
				s->status.seq_reps = 1;
				active = 0;
			}
		}
	}

	s->seq_pos = (unsigned short)i;
	return (short)(s->cfg.gen.seq_len - i);
}

/*
 * Correlate two symbol runs and report 1 - scale*distance.
 *
 * D1064: `scale` IS 1/(p[0]*n) IN INTEGER ARITHMETIC.  That is 1 when the
 * product is 1, -1 when it is -1, ZERO for every other magnitude, and a
 * divide fault when it is zero.  So for any real configuration the function
 * answers a constant 1 whatever the two runs contain, and the whole
 * accumulation is dead.  Reproduced exactly, because the fault and the
 * constant are both observable and neither is ours to remove.
 *
 * F8494 applies here too: both runs must be byte-ranged, or the table index
 * leaves the table.
 */
short
SGD_correlate(const short *p, const unsigned short *a,
	      const unsigned short *b, short n)
{
	int scale = 1 / (*p * n);
	short acc = 0;
	short i;

	for (i = (short)(n - 1); i != -1; i = (short)(i - 1))
		acc = (short)(acc
		    + FPM_xor_table[(unsigned short)(*b++ ^ *a++)]);

	return (short)(1 - acc * scale);
}

/*
 * Replace either half of the configuration, or both, or neither.
 *
 * Each half carries its own re-arming, and the two are NOT symmetric: the
 * generator half resets seq_pos, seq_reps and word_left; the detector half
 * re-derives hist_span and thresh, re-zeroes the history, and clears five of
 * the six status fields -- every one except seq_reps.  So a detector-only
 * control leaves the generator's completion count standing, which is what
 * lets a caller re-point the detector mid-transmission.
 *
 * The detector half's tail is `SGD_create`'s tail, minus the allocation.
 * D1061 applies to the zeroing here exactly as it does there, and is sharper:
 * `hist_extra` is not settable through this interface at all, so a control
 * that raises ref_len above the constructed hist_extra overruns a buffer it
 * cannot resize.
 */
void
SGD_control(struct sgd *s, struct sgd_control_req *req)
{
	unsigned short i;

	if (req->gen != NULL) {
		s->cfg.gen = *req->gen;
		s->seq_pos = 0;
		s->status.seq_reps = 0;
		s->word_left = (short)(s->cfg.gen.word_syms - 1);
	}

	if (req->det != NULL) {
		s->cfg.det = *req->det;

		s->hist_span = (short)(s->cfg.hist_len
				       + s->cfg.det.ref_len - 1);
		for (i = 0; i < s->hist_span; i++)
			s->hist[i] = 0;

		s->pat_sr = 0;
		s->status.seq_found = 0;
		s->status.det_at = NULL;
		s->status.quality = 0;
		s->status.pat_found = 0;
		s->status.pat_data = 0;
		s->thresh = (short)(s->cfg.sym_bits * s->cfg.det.ref_len
				    * (0x4000 - s->cfg.det.ref_margin));
	}
}

/*
 * Read the status block and clear it.
 *
 * The copy out is a STRUCT ASSIGNMENT and the clear is six separate stores,
 * two of them 16-bit.  That is not a tidying of the object's code, it is the
 * object's code: the two 2-byte holes in `struct sgd_status` ride out with
 * the copy and are never written by anything, so what a caller reads there
 * is whatever `SGD_create`'s allocation left behind.  See sgd.h.
 */
void
SGD_status(struct sgd *s, struct sgd_status *out)
{
	*out = s->status;

	s->status.seq_found = 0;
	s->status.det_at = NULL;
	s->status.quality = 0;
	s->status.pat_found = 0;
	s->status.pat_data = 0;
	s->status.seq_reps = 0;
}

/*
 * The layout is a byte count from a build where pointers are four bytes, so
 * these are compiled only under that ABI.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define SGD_ASSERT_OFF(field, off) \
	typedef char sgd_off_##field[ \
		((int)__builtin_offsetof(struct sgd, field) == (off)) ? 1 : -1]

SGD_ASSERT_OFF(cfg, 0x00);
SGD_ASSERT_OFF(status, 0x34);
SGD_ASSERT_OFF(seq_pos, 0x4c);
SGD_ASSERT_OFF(word_left, 0x4e);
SGD_ASSERT_OFF(hist, 0x50);
SGD_ASSERT_OFF(hist_span, 0x54);
SGD_ASSERT_OFF(thresh, 0x56);
SGD_ASSERT_OFF(pat_sr, 0x58);

typedef char sgd_size[(sizeof(struct sgd) == 0x5c) ? 1 : -1];
typedef char sgd_cfg_size[(sizeof(struct sgd_cfg) == 0x34) ? 1 : -1];
typedef char sgd_status_size[(sizeof(struct sgd_status) == 0x18) ? 1 : -1];
typedef char sgd_gen_size[(sizeof(struct sgd_gen_cfg) == 0x18) ? 1 : -1];
typedef char sgd_det_size[(sizeof(struct sgd_det_cfg) == 0x14) ? 1 : -1];
typedef char sgd_control_req_size[
    (sizeof(struct sgd_control_req) == 0x08) ? 1 : -1];

#endif
