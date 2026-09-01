/*
 * t_faxpack.c -- differential test of the Class 1 fax VMI's PACK side:
 * faxvmi_simp_pack and faxvmi_asyc_pack.
 *
 * THE TWO SHARE ONE OBJECT with the unpackers and with the ring writers, so
 * this fixture is `t_faxunframe.c`'s built the other way round.  What the
 * packers touch is the ring (+0x00..+0x0d), the PACK bit engine
 * (+0x10..+0x1c), the async break pair (+0x34/+0x38), `vmi->underrun` and
 * `link->buf`; everything else in the 88-byte framer must come back
 * unchanged, and is randomised first so that it has to.
 *
 * EVERY FIELD USED AS A SUBSCRIPT IS PLANTED, not only every field
 * dereferenced -- F8587/D955.  Four of them index an array here:
 *
 *     framer->rd     -> framer->fifo   (the packers' source element)
 *     framer->wr     -> framer->fifo   (faxvmi_write_fifo, which each packer
 *                                       calls TWICE, before and after)
 *     framer->count  -> bounds both, and must not exceed fifo_size
 *     link->buf      -> written `link->pack_count` elements deep
 *
 * `framer->rd` is the one a blob-against-blob run could not have caught: both
 * sides would read the same out-of-range neighbour and agree.
 *
 * THE DESTINATION IS SIZED FROM `pack_count`, NOT FROM `count` -- F8607/D956
 * upside down.  A packer writes exactly `pack_count` elements into
 * `link->buf` whatever the input was, because a starved ring is PADDED rather
 * than short-blocking, so `buf` carries `pack_count` plus a guard region that
 * is compared.  (In the object `buf` is 50 elements from
 * `sysdep_malloc(0x64)`, and a `pack_count` above that overruns it there too;
 * this test stays inside its own array.)
 *
 * `pack_bit` IS PLANTED BELOW `pack_width` ON PURPOSE.  The emit test is
 * `bit == width` on unsigned shorts, so a planted `bit` above `width` costs
 * 65,536 iterations per output element before the counter wraps round to it.
 * That is the object's behaviour and not a defect; it is simply not what this
 * test is for.
 *
 * MULTI-CALL IS NOT A NICETY HERE -- F9022.  A fixture that re-plants before
 * every call tests the framer's SAVE without its RESTORE, and the pack engine
 * carries mask, word, acc and bit across a return exactly as the unpack one
 * does.  `run_multicall` plants once and makes five consecutive calls,
 * comparing the whole object after each.
 *
 * Every anti-vacuity counter is incremented FROM THE REFERENCE'S OWN state or
 * answers (F134), never from the table that produced the input.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/faxvmi.h"

extern int ref_faxvmi_simp_pack(void *vmi, unsigned short *src, short count);
extern int ref_faxvmi_asyc_pack(void *vmi, unsigned short *src, short count);

/* ------------------------------------------------------------------ */

#define FIFOCAP		48	/* framer->fifo_size                   */
#define PACKMAX		24	/* the largest link->pack_count driven */
#define GUARD		16
#define SRCMAX		96	/* elements in the caller's buffer     */
#define MAXWIDTH	16

static struct faxvmi va, vb;
static struct faxvmi_framer fra, frb;
static struct faxvmi_link lka, lkb;

static unsigned short fifoa[FIFOCAP + GUARD], fifob[FIFOCAP + GUARD];
static unsigned short bufa[PACKMAX + GUARD], bufb[PACKMAX + GUARD];
static unsigned short srca[SRCMAX], srcb[SRCMAX];

static unsigned long seed = 20260901UL;

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

/*
 * Build the two sides identically.  Everything is randomised first, so a
 * field this batch believes is untouched has to STAY untouched to pass; then
 * every subscript-bearing field is forced into range.
 */
static void
plant(unsigned short mask, unsigned short bit, unsigned short width,
      short pack_count, unsigned short rd, unsigned short wr,
      unsigned short count, int zero_run_send, unsigned short zero_run_bits)
{
	fill(&fra, (unsigned)sizeof(fra));
	fill(&va, (unsigned)sizeof(va));
	fill(&lka, (unsigned)sizeof(lka));
	fill(fifoa, (unsigned)sizeof(fifoa));
	fill(srca, (unsigned)sizeof(srca));
	memset(bufa, 0x5a, sizeof(bufa));

	fra.fifo = fifoa;
	fra.fifo_size = FIFOCAP;
	fra.rd = rd;
	fra.wr = wr;
	fra.count = count;
	fra.pack_mask = mask;
	fra.pack_bit = bit;
	fra.zero_run_send = zero_run_send;
	fra.zero_run_bits = zero_run_bits;

	lka.buf = bufa;
	lka.pack_count = pack_count;
	lka.pack_width = width;

	va.framer = &fra;
	va.link = &lka;

	memcpy(&frb, &fra, sizeof(fra));
	memcpy(&vb, &va, sizeof(va));
	memcpy(&lkb, &lka, sizeof(lka));
	memcpy(fifob, fifoa, sizeof(fifoa));
	memcpy(srcb, srca, sizeof(srca));
	memcpy(bufb, bufa, sizeof(bufa));

	frb.fifo = fifob;
	lkb.buf = bufb;
	vb.framer = &frb;
	vb.link = &lkb;
}

/*
 * Compare everything except the four pointers, which hold two different
 * addresses by construction and always will.
 */
static void
compare(const char *what, long tag)
{
	char msg[128];

#define CMP(expr)							\
	do {								\
		snprintf(msg, sizeof(msg), "%s: %s (%%ld)", what, #expr); \
		diff_eq_int(msg, (long)(b_ ## expr), (long)(a_ ## expr), \
			    tag);					\
	} while (0)

	long a_fifo_size = fra.fifo_size, b_fifo_size = frb.fifo_size;
	long a_rd = fra.rd, b_rd = frb.rd;
	long a_wr = fra.wr, b_wr = frb.wr;
	long a_count = fra.count, b_count = frb.count;
	long a_residue = fra.residue, b_residue = frb.residue;
	long a_pack_mask = fra.pack_mask, b_pack_mask = frb.pack_mask;
	long a_pack_word = fra.pack_word, b_pack_word = frb.pack_word;
	long a_pack_acc = fra.pack_acc, b_pack_acc = frb.pack_acc;
	long a_pack_bit = fra.pack_bit, b_pack_bit = frb.pack_bit;
	long a_pad_001e = fra.pad_001e, b_pad_001e = frb.pad_001e;
	long a_unpack_mask = fra.unpack_mask, b_unpack_mask = frb.unpack_mask;
	long a_unpack_word = fra.unpack_word, b_unpack_word = frb.unpack_word;
	long a_unpack_acc = fra.unpack_acc, b_unpack_acc = frb.unpack_acc;
	long a_unpack_bit = fra.unpack_bit, b_unpack_bit = frb.unpack_bit;
	long a_short_002e = fra.short_002e, b_short_002e = frb.short_002e;
	long a_async_hunt = fra.async_hunt, b_async_hunt = frb.async_hunt;
	long a_zero_run_bits = fra.zero_run_bits;
	long b_zero_run_bits = frb.zero_run_bits;
	long a_zero_run_send = fra.zero_run_send;
	long b_zero_run_send = frb.zero_run_send;
	long a_zero_run_seen = fra.zero_run_seen;
	long b_zero_run_seen = frb.zero_run_seen;
	long a_frame_size = fra.frame_size, b_frame_size = frb.frame_size;
	long a_short_0046 = fra.short_0046, b_short_0046 = frb.short_0046;
	long a_frame_len = fra.frame_len, b_frame_len = frb.frame_len;
	long a_flags_wanted = fra.flags_wanted;
	long b_flags_wanted = frb.flags_wanted;
	long a_int_004c = fra.int_004c, b_int_004c = frb.int_004c;
	long a_ones = fra.ones, b_ones = frb.ones;
	long a_in_frame = fra.in_frame, b_in_frame = frb.in_frame;

	CMP(fifo_size);
	CMP(rd);
	CMP(wr);
	CMP(count);
	CMP(residue);
	CMP(pack_mask);
	CMP(pack_word);
	CMP(pack_acc);
	CMP(pack_bit);
	CMP(pad_001e);
	CMP(unpack_mask);
	CMP(unpack_word);
	CMP(unpack_acc);
	CMP(unpack_bit);
	CMP(short_002e);
	CMP(async_hunt);
	CMP(zero_run_bits);
	CMP(zero_run_send);
	CMP(zero_run_seen);
	CMP(frame_size);
	CMP(short_0046);
	CMP(frame_len);
	CMP(flags_wanted);
	CMP(int_004c);
	CMP(ones);
	CMP(in_frame);
#undef CMP

	snprintf(msg, sizeof(msg), "%s: vmi->underrun (%%ld)", what);
	diff_eq_int(msg, (long)vb.underrun, (long)va.underrun, tag);
	snprintf(msg, sizeof(msg), "%s: vmi->overflow (%%ld)", what);
	diff_eq_int(msg, (long)vb.overflow, (long)va.overflow, tag);
	snprintf(msg, sizeof(msg), "%s: vmi->status (%%ld)", what);
	diff_eq_int(msg, (long)vb.status, (long)va.status, tag);
	snprintf(msg, sizeof(msg), "%s: link->pack_count (%%ld)", what);
	diff_eq_int(msg, (long)lkb.pack_count, (long)lka.pack_count, tag);
	snprintf(msg, sizeof(msg), "%s: link->pack_width (%%ld)", what);
	diff_eq_int(msg, (long)lkb.pack_width, (long)lka.pack_width, tag);

	/*
	 * And the two objects wholesale, minus the four pointers that hold
	 * two different addresses by construction: `struct faxvmi` up to its
	 * `framer` at +0x24, and `struct faxvmi_link` past its `buf` at
	 * +0x08.  This is what catches a store to a field nobody thought to
	 * name -- the named comparisons above leave `pad_0036` and the
	 * framer's own padding uncovered.
	 */
	snprintf(msg, sizeof(msg), "%s: vmi head (%%ld)", what);
	diff_eq_int(msg, memcmp(&va, &vb, 0x24), 0, tag);
	snprintf(msg, sizeof(msg), "%s: link tail (%%ld)", what);
	diff_eq_int(msg, memcmp((char *)&lka + 8, (char *)&lkb + 8,
			       sizeof(lka) - 8), 0, tag);
	snprintf(msg, sizeof(msg), "%s: framer ring+engines (%%ld)", what);
	diff_eq_int(msg, memcmp((char *)&fra + 4, (char *)&frb + 4, 0x3c), 0,
		    tag);
	snprintf(msg, sizeof(msg), "%s: framer hdlc tail (%%ld)", what);
	diff_eq_int(msg, memcmp((char *)&fra + 0x44, (char *)&frb + 0x44,
			       sizeof(fra) - 0x44), 0, tag);

	snprintf(msg, sizeof(msg), "%s: fifo+guard (%%ld)", what);
	diff_eq_int(msg, memcmp(fifoa, fifob, sizeof(fifoa)), 0, tag);
	snprintf(msg, sizeof(msg), "%s: buf+guard (%%ld)", what);
	diff_eq_int(msg, memcmp(bufa, bufb, sizeof(bufa)), 0, tag);
	snprintf(msg, sizeof(msg), "%s: src untouched (%%ld)", what);
	diff_eq_int(msg, memcmp(srca, srcb, sizeof(srca)), 0, tag);
}

/*
 * A width, a mask that belongs to one of the two source shapes, and a bit
 * position below the width.
 */
static unsigned short
some_mask(void)
{
	static const unsigned short m[6] = { 0, 1, 2, 4, 0x80, 0x200 };

	return m[rnd() % 6];
}

/* ------------------------------------------------------------------ */
/* faxvmi_simp_pack                                                    */

static int simp_underran, simp_fed, simp_short, simp_residue, simp_full;

static int
run_simp(void)
{
	unsigned t;

	diff_begin("faxvmi_simp_pack");
	for (t = 0; t < 500; t++) {
		unsigned short width = (unsigned short)(1 + rnd() % MAXWIDTH);
		short pack_count = (short)(rnd() % (PACKMAX + 1));
		unsigned short occ = (unsigned short)(rnd() % (FIFOCAP + 1));
		unsigned short rd = (unsigned short)(rnd() % FIFOCAP);
		unsigned short wr = (unsigned short)(rnd() % FIFOCAP);
		short count = (short)(rnd() % (SRCMAX + 1));
		int ra, rb;
		long tag = (long)t;

		/*
		 * Every fifth case starves the ring outright, because a
		 * uniform draw over the occupancy rarely empties it before
		 * `pack_count` elements have been built.
		 */
		if (t % 5 == 0) {
			occ = 0;
			count = (short)(rnd() % 3);
		}

		plant(some_mask(), (unsigned short)(rnd() % width), width,
		      pack_count, rd, wr, occ, 0, 0);

		ra = ref_faxvmi_simp_pack(&va, srca, count);
		rb = faxvmi_simp_pack(&vb, srcb, count);

		diff_eq_int("simp return (%ld)", (long)rb, (long)ra, tag);
		compare("simp", tag);

		if (va.underrun)
			simp_underran++;
		if (ra > 0)
			simp_fed++;
		if (ra < (int)pack_count)
			simp_short++;
		if (ra == (int)pack_count && pack_count > 0)
			simp_full++;
		if (fra.residue != 0)
			simp_residue++;
	}
	diff_eq_int("simp: the ring ran dry (%ld)", simp_underran > 0, 1,
		    (long)simp_underran);
	diff_eq_int("simp: elements were built from real data (%ld)",
		    simp_fed > 0, 1, (long)simp_fed);
	diff_eq_int("simp: the `real` latch cut the count short (%ld)",
		    simp_short > 0, 1, (long)simp_short);
	diff_eq_int("simp: a whole block came from the ring (%ld)",
		    simp_full > 0, 1, (long)simp_full);
	diff_eq_int("simp: input was left over (%ld)", simp_residue > 0, 1,
		    (long)simp_residue);
	return diff_end();
}

/* ------------------------------------------------------------------ */
/* faxvmi_asyc_pack                                                    */

static int asyc_underran, asyc_chars, asyc_run, asyc_run_ended, asyc_residue;

static int
run_asyc(void)
{
	unsigned t;

	diff_begin("faxvmi_asyc_pack");
	for (t = 0; t < 600; t++) {
		unsigned short width = (unsigned short)(1 + rnd() % MAXWIDTH);
		short pack_count = (short)(rnd() % (PACKMAX + 1));
		unsigned short occ = (unsigned short)(rnd() % (FIFOCAP + 1));
		unsigned short rd = (unsigned short)(rnd() % FIFOCAP);
		unsigned short wr = (unsigned short)(rnd() % FIFOCAP);
		short count = (short)(rnd() % (SRCMAX + 1));
		int send = 0;
		unsigned short bits = 0;
		int ra, rb;
		long tag = (long)t;

		if (t % 5 == 0) {
			occ = 0;
			count = (short)(rnd() % 3);
		}
		/*
		 * A third of the cases are in a zero run, and the run length
		 * is small so that the arm which ENDS it -- D1120, the one
		 * that leaves the mask at zero -- is reached inside the block
		 * rather than only at the far end of a long countdown.
		 */
		if (t % 3 == 0) {
			send = 1;
			bits = (unsigned short)(rnd() % 4);
		}

		plant(some_mask(), (unsigned short)(rnd() % width), width,
		      pack_count, rd, wr, occ, send, bits);

		ra = ref_faxvmi_asyc_pack(&va, srca, count);
		rb = faxvmi_asyc_pack(&vb, srcb, count);

		diff_eq_int("asyc return (%ld)", (long)rb, (long)ra, tag);
		diff_eq_int("asyc returns pack_count (%ld)", (long)ra,
			    (long)pack_count, tag);
		compare("asyc", tag);

		if (va.underrun)
			asyc_underran++;
		/*
		 * The read cursor moving is what says a character was taken
		 * from the ring; the occupancy cannot say it, because the
		 * two faxvmi_write_fifo calls raise it in the same call.
		 */
		if (fra.rd != rd)
			asyc_chars++;
		if (send && fra.zero_run_bits < bits)
			asyc_run++;
		if (send && fra.zero_run_send == 0)
			asyc_run_ended++;
		if (fra.residue != 0)
			asyc_residue++;
	}
	diff_eq_int("asyc: the ring ran dry (%ld)", asyc_underran > 0, 1,
		    (long)asyc_underran);
	diff_eq_int("asyc: characters were taken from the ring (%ld)",
		    asyc_chars > 0, 1, (long)asyc_chars);
	diff_eq_int("asyc: a zero run sent bits (%ld)", asyc_run > 0, 1,
		    (long)asyc_run);
	diff_eq_int("asyc: a zero run ended, D1120's arm (%ld)",
		    asyc_run_ended > 0, 1, (long)asyc_run_ended);
	diff_eq_int("asyc: input was left over (%ld)", asyc_residue > 0, 1,
		    (long)asyc_residue);
	return diff_end();
}

/* ------------------------------------------------------------------ */
/*
 * The packers across consecutive calls, with NO replanting.  F9022: a fixture
 * that plants before every call compares the save and never the restore, and
 * the pack engine's mask, word, acc and bit exist only to be carried across
 * a return.
 */

static int multi_carried, multi_midbyte, multi_produced;

static int
run_multicall(void)
{
	unsigned t;

	diff_begin("the packers across consecutive calls");
	for (t = 0; t < 120; t++) {
		unsigned short width = (unsigned short)(1 + rnd() % MAXWIDTH);
		short pack_count = (short)(1 + rnd() % 6);
		int which = (int)(t % 2);
		int k;

		plant(0, 0, width, pack_count, 0, 0, 0,
		      (int)(t % 6 == 0), (unsigned short)(rnd() % 5));

		for (k = 0; k < 5; k++) {
			short count = (short)(rnd() % 12);
			long tag = (long)(t * 10 + k);
			int ra, rb;

			if (fra.pack_mask != 0)
				multi_carried++;
			if (lka.pack_count > 0)
				multi_midbyte++;

			if (which == 0) {
				ra = ref_faxvmi_simp_pack(&va, srca, count);
				rb = faxvmi_simp_pack(&vb, srcb, count);
			} else {
				ra = ref_faxvmi_asyc_pack(&va, srca, count);
				rb = faxvmi_asyc_pack(&vb, srcb, count);
			}
			diff_eq_int("multicall return (%ld)", (long)rb,
				    (long)ra, tag);
			compare("multicall", tag);
			/*
			 * F9200, asserted rather than assumed: with a
			 * non-zero `pack_count` the loop can only end just
			 * after an emit, and an emit clears `pack_bit`, so
			 * the REFERENCE leaves it at zero on every such
			 * return.  The element boundary is therefore never
			 * carried across a call; the SOURCE boundary,
			 * `pack_mask`, always is.
			 */
			if (lka.pack_count > 0)
				diff_eq_int("multicall: the object left "
					    "pack_bit zero (%ld)",
					    (long)fra.pack_bit, 0, tag);
			if (ra > 0)
				multi_produced++;
		}
	}
	/*
	 * Both counters are read off the REFERENCE's state at the top of a
	 * call, so they say a call really did resume part-way through a
	 * source element and part-way through an output element -- which is
	 * the state the restore has to reproduce.
	 */
	diff_eq_int("multicall: a call resumed mid-source (%ld)",
		    multi_carried > 0, 1, (long)multi_carried);
	diff_eq_int("multicall: pack_bit was checked at a boundary (%ld)",
		    multi_midbyte > 0, 1, (long)multi_midbyte);
	diff_eq_int("multicall: output was produced (%ld)", multi_produced > 0,
		    1, (long)multi_produced);
	return diff_end();
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	int bad = 0;

	bad |= run_simp();
	bad |= run_asyc();
	bad |= run_multicall();

	return bad;
}
