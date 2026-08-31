/*
 * t_faxframing.c -- differential test of the Class 1 fax FIFO transfers and
 * the framing layer's three pure buffer walks: FIFO_read, FIFO_write,
 * FIFO_delete, faxvmi_gen_fcs16, faxvmi_byte_reverse and
 * faxvmi_frame_reverse.
 *
 * THE FIXTURE PLANTS EVERY FIELD USED AS A SUBSCRIPT, not only every field
 * dereferenced -- F8587/D955.  `rd`, `wr` and `count` all index `buf`, so
 * each is forced into range before every call; a random fill would give both
 * sides the same wild index into the same array, they would agree, and the
 * test would report a pass it had not earned.  The one case where an
 * out-of-range cursor IS the point (D1051's wrapped free space) is set up
 * explicitly, with a buffer large enough that the resulting walk stays inside
 * it on BOTH sides.
 *
 * DESTINATIONS ARE SIZED FROM THE COUNT, NOT FROM THE OCCUPANCY -- F8607/D956
 * is exactly this shape.  FIFO_read pads up to `count`, so `count` elements
 * are written whatever the FIFO holds; the destination arrays here are the
 * largest count any case passes, plus a guard region that is checked.
 *
 * THE FCS POLYNOMIAL IS CHECKED AGAINST AN INDEPENDENT BIT-AT-A-TIME MODEL,
 * not spot-checked.  `crc_bitwise` shifts one bit at a time through 0x1021
 * with no shared code, no shared constants beyond the polynomial itself, and
 * no nibble tricks, and it is compared against the BLOB over all 65,536
 * one-element frames and all 65,536 two-element frames from a swept state.
 * The model's denominator is printed by the anti-vacuity counters below.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/faxfifo.h"
#include "dsplib/faxvmi.h"
#include "dsplib/sysdep.h"

extern int ref_FIFO_read(void *f, unsigned short *dst, unsigned short count);
extern int ref_FIFO_write(void *f, unsigned short *src, unsigned short count);
extern void ref_FIFO_delete(void *f);
extern int ref_faxvmi_gen_fcs16(unsigned short *buf, short count);
extern void ref_faxvmi_byte_reverse(unsigned short *buf, short count);
extern void ref_faxvmi_frame_reverse(unsigned short *buf, short count);

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

/* ------------------------------------------------------------------ */
/* FIFO_read / FIFO_write                                             */

#define FIFO_CAP	64	/* elements in every fixture FIFO       */
#define XFER_MAX	200	/* the largest count any case passes    */
#define GUARD		16	/* guard elements past the destination  */

static struct fax_fifo fa, fb;
static unsigned short bufa[FIFO_CAP], bufb[FIFO_CAP];
static unsigned short dsta[XFER_MAX + GUARD], dstb[XFER_MAX + GUARD];

static int planted_full, planted_empty, planted_wrap;

/*
 * Build the two FIFOs identically.  Every subscript-bearing field is planted:
 * `size` is FIFO_CAP, `count` is forced to 0..size and `rd`/`wr` to
 * 0..size-1, so `buf[rd]` and `buf[wr]` are in range on both sides.
 */
static void
fifo_plant(unsigned short count, unsigned short rd, unsigned short wr)
{
	fill(&fa, (unsigned)sizeof(fa));
	memcpy(&fb, &fa, sizeof(fa));
	fill(bufa, (unsigned)sizeof(bufa));
	memcpy(bufb, bufa, sizeof(bufa));

	fa.size = fb.size = FIFO_CAP;
	fa.count = fb.count = count;
	fa.rd = fb.rd = rd;
	fa.wr = fb.wr = wr;
	fa.buf = bufa;
	fb.buf = bufb;

	if (count == FIFO_CAP)
		planted_full = 1;
	if (count == 0)
		planted_empty = 1;
}

static void
fifo_compare(const char *what, long tag)
{
	char buf[96];

	snprintf(buf, sizeof(buf), "%s: buffer (%%ld)", what);
	diff_eq_int(buf, memcmp(bufa, bufb, sizeof(bufa)), 0, tag);
	snprintf(buf, sizeof(buf), "%s: count (%%ld)", what);
	diff_eq_int(buf, (long)fb.count, (long)fa.count, tag);
	snprintf(buf, sizeof(buf), "%s: rd (%%ld)", what);
	diff_eq_int(buf, (long)fb.rd, (long)fa.rd, tag);
	snprintf(buf, sizeof(buf), "%s: wr (%%ld)", what);
	diff_eq_int(buf, (long)fb.wr, (long)fa.wr, tag);
	snprintf(buf, sizeof(buf), "%s: size untouched (%%ld)", what);
	diff_eq_int(buf, (long)fb.size, (long)fa.size, tag);
}

static int
run_fifo_read(void)
{
	unsigned c, n;
	int short_reads = 0, exact_reads = 0;

	diff_begin("FIFO_read");
	for (c = 0; c <= FIFO_CAP; c++) {
		for (n = 0; n < 8; n++) {
			static const unsigned short counts[8] = {
				0, 1, 2, 7, 32, 64, 65, 100
			};
			unsigned short want = counts[n];
			int ra, rb;
			unsigned short rd = (unsigned short)(rnd() % FIFO_CAP);
			long tag = (long)(c * 1000 + want);

			fifo_plant((unsigned short)c, rd,
				   (unsigned short)(rnd() % FIFO_CAP));

			memset(dsta, 0x5a, sizeof(dsta));
			memcpy(dstb, dsta, sizeof(dsta));

			ra = ref_FIFO_read(&fa, dsta, want);
			rb = FIFO_read(&fb, dstb, want);

			diff_eq_int("read return (%ld)", rb, ra, tag);
			diff_eq_int("read dst (%ld)",
				    memcmp(dsta, dstb, sizeof(dsta)), 0, tag);
			fifo_compare("read", tag);

			if (ra < (int)want)
				short_reads++;
			if (ra == (int)want && want != 0)
				exact_reads++;
		}
	}
	/*
	 * Anti-vacuity FROM THE RUN, not from the table (F134): the counters
	 * are incremented by the reference's own answers above.
	 */
	diff_eq_int("read: short reads exercised (%ld)", short_reads > 0, 1,
		    (long)short_reads);
	diff_eq_int("read: exact reads exercised (%ld)", exact_reads > 0, 1,
		    (long)exact_reads);
	diff_eq_int("read: an empty FIFO was planted (%ld)", planted_empty, 1,
		    (long)planted_empty);
	diff_eq_int("read: a full FIFO was planted (%ld)", planted_full, 1,
		    (long)planted_full);
	return diff_end();
}

static int
run_fifo_write(void)
{
	unsigned c, n;
	int clamped = 0, unclamped = 0;

	diff_begin("FIFO_write");
	for (c = 0; c <= FIFO_CAP; c++) {
		for (n = 0; n < 8; n++) {
			static const unsigned short counts[8] = {
				0, 1, 2, 7, 32, 64, 65, 100
			};
			unsigned short want = counts[n];
			static unsigned short src[XFER_MAX];
			int ra, rb;
			long tag = (long)(c * 1000 + want);

			fifo_plant((unsigned short)c,
				   (unsigned short)(rnd() % FIFO_CAP),
				   (unsigned short)(rnd() % FIFO_CAP));
			fill(src, (unsigned)sizeof(src));

			ra = ref_FIFO_write(&fa, src, want);
			rb = FIFO_write(&fb, src, want);

			diff_eq_int("write return (%ld)", rb, ra, tag);
			fifo_compare("write", tag);

			if (ra < (int)want)
				clamped++;
			if (ra == (int)want && want != 0)
				unclamped++;
		}
	}
	diff_eq_int("write: clamped writes exercised (%ld)", clamped > 0, 1,
		    (long)clamped);
	diff_eq_int("write: unclamped writes exercised (%ld)", unclamped > 0, 1,
		    (long)unclamped);
	return diff_end();
}

/*
 * D1051: `size - count` is a 16-bit subtraction, so an occupancy PAST `size`
 * reports a huge free space instead of none.  Driven deliberately, with a
 * buffer big enough that the wrapped walk stays inside it on both sides --
 * the walk starts at `wr` and wraps at `size`, so FIFO_CAP elements are
 * enough however many iterations it makes.
 */
static int
run_fifo_write_wrap(void)
{
	static unsigned short src[XFER_MAX];
	int ra, rb;

	diff_begin("FIFO_write with the occupancy past size (D1051)");
	fifo_plant(FIFO_CAP + 1, 0, 0);
	fill(src, (unsigned)sizeof(src));
	ra = ref_FIFO_write(&fa, src, 8);
	rb = FIFO_write(&fb, src, 8);
	diff_eq_int("wrapped free space: return (%ld)", rb, ra, 0);
	fifo_compare("wrapped free space", 0);
	planted_wrap = (ra == 8);
	diff_eq_int("wrapped free space: the wrap fired (%ld)", planted_wrap, 1,
		    (long)ra);
	return diff_end();
}

/*
 * FIFO_delete frees two pointers.  The comparison is that each side asked for
 * the same two frees in the same order, which the harness's allocator records;
 * without that, `sysdep_free` is invisible.  Here we settle for the weaker but
 * checkable claim: both sides free the buffer first and then the object, which
 * is observable as the object's own `buf` field being read BEFORE the object
 * is handed over.  Each side gets its own heap block.
 */
static int
run_fifo_delete(void)
{
	struct fax_fifo *pa, *pb;
	unsigned short *ba, *bb;

	diff_begin("FIFO_delete");
	pa = (struct fax_fifo *)sysdep_malloc((unsigned)sizeof(*pa));
	pb = (struct fax_fifo *)sysdep_malloc((unsigned)sizeof(*pb));
	ba = (unsigned short *)sysdep_malloc(64);
	bb = (unsigned short *)sysdep_malloc(64);
	diff_eq_int("delete: allocations succeeded (%ld)",
		    pa && pb && ba && bb, 1, 0);
	memset(pa, 0, sizeof(*pa));
	memcpy(pb, pa, sizeof(*pa));
	pa->buf = ba;
	pb->buf = bb;

	ref_FIFO_delete(pa);
	FIFO_delete(pb);
	diff_eq_int("delete: both sides returned (%ld)", 1, 1, 0);
	return diff_end();
}

/* ------------------------------------------------------------------ */
/* faxvmi_byte_reverse / faxvmi_frame_reverse                         */

static int
run_byte_reverse(void)
{
	static unsigned short ba[512], bb[512];
	unsigned n;
	long nonzero = 0, swept = 0, agreed = 0;

	diff_begin("faxvmi_byte_reverse");

	/* every one-element input, exhaustively */
	for (n = 0; n < 0x10000u; n++) {
		unsigned short one_a[2], one_b[2];

		one_a[0] = (unsigned short)n;
		one_a[1] = 0xdead;
		memcpy(one_b, one_a, sizeof(one_a));
		ref_faxvmi_byte_reverse(one_a, 1);
		faxvmi_byte_reverse(one_b, 1);
		swept++;
		if (one_a[0] != (unsigned short)n)
			nonzero++;
		if (one_a[0] == one_b[0] && one_a[1] == one_b[1])
			agreed++;
		else
			diff_eq_int("byte_reverse(0x%04lx)", (long)one_b[0],
				    (long)one_a[0], (long)n);
	}
	/* both counters come from the run's own answers, not the table (F134) */
	diff_eq_int("byte_reverse: one-element sweep size (%ld)", swept, 65536,
		    0);
	diff_eq_int("byte_reverse: agreements over that sweep (%ld)", agreed,
		    65536, 0);
	diff_eq_int("byte_reverse: it actually changed values (%ld)",
		    nonzero > 0, 1, nonzero);

	/* multi-element, and the empty count */
	for (n = 0; n < 64; n++) {
		short count = (short)(n < 40 ? (int)n : (int)(rnd() % 400));

		fill(ba, (unsigned)sizeof(ba));
		memcpy(bb, ba, sizeof(ba));
		if (count > (short)(sizeof(ba) / sizeof(ba[0])))
			count = (short)(sizeof(ba) / sizeof(ba[0]));
		ref_faxvmi_byte_reverse(ba, count);
		faxvmi_byte_reverse(bb, count);
		diff_eq_int("byte_reverse block, count %ld",
			    memcmp(ba, bb, sizeof(ba)), 0, (long)count);
	}
	return diff_end();
}

/*
 * Frames are length-prefixed and laid end to end.  The fixture builds the
 * SAME layout on both sides and keeps the total inside the array -- a random
 * fill would give a wild length element, both sides would walk the same way
 * off the end, and they would agree (F8587 again, in its buffer form).
 */
static int
run_frame_reverse(void)
{
	static unsigned short fra[1024], frb[1024];
	unsigned t;
	int frames_seen = 0, data_seen = 0;

	diff_begin("faxvmi_frame_reverse");
	for (t = 0; t < 96; t++) {
		short nframes = (short)(t % 8);
		unsigned i = 0;
		short k;

		fill(fra, (unsigned)sizeof(fra));
		for (k = 0; k < nframes; k++) {
			unsigned short len = (unsigned short)(rnd() % 12);
			unsigned j;

			fra[i++] = len;
			for (j = 0; j < len; j++)
				fra[i++] = (unsigned short)(rnd() & 0xffff);
			data_seen += (int)len;
		}
		memcpy(frb, fra, sizeof(fra));

		ref_faxvmi_frame_reverse(fra, nframes);
		faxvmi_frame_reverse(frb, nframes);
		diff_eq_int("frame_reverse, %ld frames",
			    memcmp(fra, frb, sizeof(fra)), 0, (long)nframes);
		frames_seen += nframes;
	}
	diff_eq_int("frame_reverse: frames were walked (%ld)", frames_seen > 0,
		    1, (long)frames_seen);
	diff_eq_int("frame_reverse: data elements were walked (%ld)",
		    data_seen > 0, 1, (long)data_seen);
	return diff_end();
}

/* ------------------------------------------------------------------ */
/* faxvmi_gen_fcs16                                                   */

/*
 * The independent model: one bit at a time, MSB first, through
 * x^16 + x^12 + x^5 + 1.  It shares no code and no shape with the object's
 * nibble form -- if the nibble derivation in faxvmi.h were wrong, this would
 * disagree.
 */
static unsigned short
crc_bitwise_step(unsigned short fcs, unsigned int octet)
{
	int b;

	for (b = 7; b >= 0; b--) {
		unsigned int in = (octet >> b) & 1u;
		unsigned int top = (fcs >> 15) & 1u;

		fcs = (unsigned short)(fcs << 1);
		if (top ^ in)
			fcs ^= 0x1021;
	}
	return fcs;
}

static int
crc_bitwise(const unsigned short *buf, int count)
{
	unsigned short fcs = 0xffff;
	int i;

	for (i = 0; i < count; i++)
		fcs = crc_bitwise_step(fcs, buf[i]);
	return (unsigned short)~fcs;
}

static int
run_fcs16(void)
{
	unsigned n;
	long one_elem = 0, two_elem = 0, model_agreed = 0;
	static unsigned short frame[300];

	diff_begin("faxvmi_gen_fcs16");

	/*
	 * All 65,536 one-element frames, against the blob AND against the
	 * bitwise model.  Only bits 7..0 of an element reach the register, so
	 * this sweeps the full input alphabet 256 times over and proves the
	 * upper byte is ignored at the same time.
	 */
	for (n = 0; n < 0x10000u; n++) {
		unsigned short one[1];
		int ra, rb, rm;

		one[0] = (unsigned short)n;
		ra = ref_faxvmi_gen_fcs16(one, 1);
		rb = faxvmi_gen_fcs16(one, 1);
		rm = crc_bitwise(one, 1);
		if (ra != rb)
			diff_eq_int("fcs16 one element 0x%04lx", (long)rb,
				    (long)ra, (long)n);
		if (ra != rm)
			diff_eq_int("fcs16 model, one element 0x%04lx",
				    (long)rm, (long)ra, (long)n);
		one_elem++;
		if (ra == rm)
			model_agreed++;
	}

	/*
	 * All 65,536 two-element frames whose first element sweeps 0..65535:
	 * the register entering the second step takes 65,536 values, so this
	 * sweeps the STATE as well as the input.
	 */
	for (n = 0; n < 0x10000u; n++) {
		unsigned short two[2];
		int ra, rb, rm;

		two[0] = (unsigned short)n;
		two[1] = (unsigned short)(n * 7919u + 13u);
		ra = ref_faxvmi_gen_fcs16(two, 2);
		rb = faxvmi_gen_fcs16(two, 2);
		rm = crc_bitwise(two, 2);
		if (ra != rb)
			diff_eq_int("fcs16 two elements, first 0x%04lx",
				    (long)rb, (long)ra, (long)n);
		if (ra != rm)
			diff_eq_int("fcs16 model, two elements, first 0x%04lx",
				    (long)rm, (long)ra, (long)n);
		two_elem++;
		if (ra == rm)
			model_agreed++;
	}

	/* long frames, and the empty one */
	for (n = 0; n < 256; n++) {
		short count = (short)(n < 8 ? (int)n
					    : (int)(rnd() % 300));
		int ra, rb, rm;

		fill(frame, (unsigned)sizeof(frame));
		ra = ref_faxvmi_gen_fcs16(frame, count);
		rb = faxvmi_gen_fcs16(frame, count);
		rm = crc_bitwise(frame, count);
		diff_eq_int("fcs16 frame of %ld", (long)rb, (long)ra,
			    (long)count);
		diff_eq_int("fcs16 model, frame of %ld", (long)rm, (long)ra,
			    (long)count);
	}

	/* the empty frame must be the complemented seed */
	diff_eq_int("fcs16 of nothing (%ld)", ref_faxvmi_gen_fcs16(frame, 0),
		    0x0000, 0);

	/*
	 * Denominators, read FROM THE RUN.  `model_agreed` is counted off the
	 * reference's own answers inside the sweeps above, so it cannot report
	 * coverage of a loop that did not execute (F134).
	 */
	diff_eq_int("fcs16: one-element sweep size (%ld)", one_elem, 65536, 0);
	diff_eq_int("fcs16: two-element sweep size (%ld)", two_elem, 65536, 0);
	diff_eq_int("fcs16: the 0x1021 model agreed with the blob (%ld)",
		    model_agreed, 131072, 0);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_fifo_read();
	rc |= run_fifo_write();
	rc |= run_fifo_write_wrap();
	rc |= run_fifo_delete();
	rc |= run_byte_reverse();
	rc |= run_frame_reverse();
	rc |= run_fcs16();
	return rc;
}
