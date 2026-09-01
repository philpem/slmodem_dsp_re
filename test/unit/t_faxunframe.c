/*
 * t_faxunframe.c -- differential test of the Class 1 fax VMI's framing unit:
 * faxvmi_simp_unpack, faxvmi_asyc_unpack, faxvmi_hdlc_unframe,
 * faxvmi_write_fifo and faxvmi_write_frame.
 *
 * THE FIVE SHARE ONE OBJECT and that is why they share one fixture.  Every
 * one of them reaches `vmi->framer`, an 88-byte block that is a ring, a
 * bit-level unpacker and an HDLC receiver at once, plus `vmi->link` for the
 * input elements and their width.
 *
 * THE FIXTURE PLANTS EVERY FIELD USED AS A SUBSCRIPT, not only every field
 * dereferenced -- F8587/D955.  Four fields index an array here:
 *
 *     framer->wr          -> framer->fifo    (both writers)
 *     framer->frame_len   -> framer->frame   (hdlc_unframe's octet store)
 *     framer->mask        -> selects a bit, and a wild one only reads
 *     link->buf            -> `count` elements, walked by all three unpackers
 *
 * and the SHIFT REPAIR reads `framer->frame[len]`, one element past the
 * octets that were stored (D1074), so the frame array is one longer than the
 * capacity the fixture plants and the extra element is filled identically on
 * both sides and compared.  A blob-against-blob run could not have caught
 * that: both sides would read the same rubbish and agree.
 *
 * DESTINATIONS ARE SIZED FROM THE INPUT, NOT FROM `vmi->max_frame` --
 * F8607/D956.  `faxvmi_hdlc_unframe`'s output guard is
 * `emitted + frame_len >= max_frame` and the object never accumulates
 * `emitted` (D1073), so several frames in one call write several times
 * `max_frame` elements.  Every destination here is sized for `count * width`
 * elements, which is an upper bound because each output element costs at
 * least one input bit, and each carries a guard region that is compared.
 *
 * THE HDLC CASES ARE CONSTRUCTED, NOT RANDOM.  A random bit stream reaches
 * the destuffer and the flag detector but essentially never produces a frame
 * whose FCS checks, so `hdlc_encode` below builds well-formed frames -- flag,
 * stuffed octets, the FCS the receiver wants, flag -- and the deliberate
 * corruptions drive the three repair arms.  Both are run: the random stream
 * is what exercises the bad-CRC path and the shift loop, the constructed one
 * is what exercises everything after a good CRC.  Every anti-vacuity counter
 * below is incremented FROM THE REFERENCE'S OWN ANSWERS (F134), never from
 * the table that produced the input.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/faxvmi.h"

extern int ref_faxvmi_simp_unpack(void *vmi, unsigned short *dst, short count);
extern int ref_faxvmi_asyc_unpack(void *vmi, unsigned short *dst, short count);
extern int ref_faxvmi_hdlc_unframe(void *vmi, unsigned short *dst,
				   short count);
extern int ref_faxvmi_write_fifo(void *vmi, unsigned short **src, short count);
extern int ref_faxvmi_write_frame(void *vmi, unsigned short **src,
				  short count);
extern int ref_faxvmi_gen_fcs16(unsigned short *buf, short count);

/* ------------------------------------------------------------------ */

#define MAXIN		64	/* input elements per call             */
#define MAXWIDTH	16
#define MAXOUT		(MAXIN * MAXWIDTH)	/* the output bound    */
#define GUARD		16
#define FRAMECAP	40	/* framer->frame_size                  */
#define FIFOCAP		48	/* framer->fifo_size                   */
#define SRCMAX		256	/* elements in the writers' source     */

static struct faxvmi va, vb;
static struct faxvmi_framer fra, frb;
static struct faxvmi_link lka, lkb;

static unsigned short fifoa[FIFOCAP + GUARD], fifob[FIFOCAP + GUARD];
/* +1 for D1074's read one past the stored octets */
static unsigned short framea[FRAMECAP + 1 + GUARD];
static unsigned short frameb[FRAMECAP + 1 + GUARD];
static unsigned short rxa[MAXIN + GUARD], rxb[MAXIN + GUARD];
static unsigned short dsta[MAXOUT + GUARD], dstb[MAXOUT + GUARD];
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
 * field this batch believes is untouched has to STAY untouched on both sides
 * to pass; then every subscript-bearing field is forced into range.
 */
static void
plant(unsigned short mask, unsigned short bit, short flen,
      unsigned short flags_wanted, short ones, int in_frame,
      unsigned short width, unsigned short max_frame,
      unsigned short wr, unsigned short count, int async_hunt)
{
	fill(&fra, (unsigned)sizeof(fra));
	fill(&va, (unsigned)sizeof(va));
	fill(&lka, (unsigned)sizeof(lka));
	fill(fifoa, (unsigned)sizeof(fifoa));
	fill(framea, (unsigned)sizeof(framea));
	fill(rxa, (unsigned)sizeof(rxa));
	memset(dsta, 0x5a, sizeof(dsta));

	fra.fifo = fifoa;
	fra.fifo_size = FIFOCAP;
	fra.wr = wr;
	fra.count = count;
	fra.unpack_mask = mask;
	fra.unpack_bit = bit;
	fra.async_hunt = async_hunt;
	fra.frame = framea;
	fra.frame_size = FRAMECAP;
	fra.frame_len = flen;
	fra.flags_wanted = flags_wanted;
	fra.ones = ones;
	fra.in_frame = in_frame;

	lka.buf = rxa;
	lka.unpack_width = width;

	va.framer = &fra;
	va.link = &lka;
	va.max_frame = max_frame;

	memcpy(&frb, &fra, sizeof(fra));
	memcpy(&vb, &va, sizeof(va));
	memcpy(&lkb, &lka, sizeof(lka));
	memcpy(fifob, fifoa, sizeof(fifoa));
	memcpy(frameb, framea, sizeof(framea));
	memcpy(rxb, rxa, sizeof(rxa));
	memcpy(dstb, dsta, sizeof(dsta));

	frb.fifo = fifob;
	frb.frame = frameb;
	lkb.buf = rxb;
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
	long a_zero_run_bits = fra.zero_run_bits, b_zero_run_bits = frb.zero_run_bits;
	long a_zero_run_send = fra.zero_run_send, b_zero_run_send = frb.zero_run_send;
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
	snprintf(msg, sizeof(msg), "%s: vmi->max_frame (%%ld)", what);
	diff_eq_int(msg, (long)vb.max_frame, (long)va.max_frame, tag);

	snprintf(msg, sizeof(msg), "%s: fifo+guard (%%ld)", what);
	diff_eq_int(msg, memcmp(fifoa, fifob, sizeof(fifoa)), 0, tag);
	snprintf(msg, sizeof(msg), "%s: frame+guard (%%ld)", what);
	diff_eq_int(msg, memcmp(framea, frameb, sizeof(framea)), 0, tag);
	snprintf(msg, sizeof(msg), "%s: rx untouched (%%ld)", what);
	diff_eq_int(msg, memcmp(rxa, rxb, sizeof(rxa)), 0, tag);
	snprintf(msg, sizeof(msg), "%s: dst+guard (%%ld)", what);
	diff_eq_int(msg, memcmp(dsta, dstb, sizeof(dsta)), 0, tag);
}

/* ------------------------------------------------------------------ */
/* faxvmi_simp_unpack and faxvmi_asyc_unpack                          */

static int simp_full, simp_overflowed, simp_produced;

static int
run_simp(void)
{
	unsigned t;

	diff_begin("faxvmi_simp_unpack");
	for (t = 0; t < 400; t++) {
		unsigned short width = (unsigned short)(1 + rnd() % MAXWIDTH);
		short count = (short)(rnd() % (MAXIN + 1));
		unsigned short max_frame =
		    (unsigned short)(t % 5 == 0 ? 1 + rnd() % 4
					        : 1 + rnd() % 200);
		unsigned short mask;
		int ra, rb;
		long tag = (long)t;

		/* mask is 0 (fetch) or a bit at or below the top */
		mask = (unsigned short)(rnd() % 2 ? 0
			: (unsigned short)(1u << (rnd() % width)));

		plant(mask, (unsigned short)(rnd() % 8), 0, 0, 0, 0,
		      width, max_frame, 0, 0, 0);

		ra = ref_faxvmi_simp_unpack(&va, dsta, count);
		rb = faxvmi_simp_unpack(&vb, dstb, count);

		diff_eq_int("simp return (%ld)", (long)rb, (long)ra, tag);
		compare("simp", tag);

		if (ra > 0)
			simp_produced++;
		if (va.overflow)
			simp_overflowed++;
		if (ra >= (int)max_frame)
			simp_full++;
	}
	diff_eq_int("simp: octets were produced (%ld)", simp_produced > 0, 1,
		    (long)simp_produced);
	diff_eq_int("simp: the destination cap was reached (%ld)",
		    simp_overflowed > 0, 1, (long)simp_overflowed);
	diff_eq_int("simp: a run stopped exactly at the cap (%ld)",
		    simp_full > 0, 1, (long)simp_full);
	return diff_end();
}

static int asyc_produced, asyc_overflowed, asyc_zero_run, asyc_hunted;

static int
run_asyc(void)
{
	unsigned t;

	diff_begin("faxvmi_asyc_unpack");
	for (t = 0; t < 500; t++) {
		unsigned short width = (unsigned short)(1 + rnd() % MAXWIDTH);
		short count = (short)(rnd() % (MAXIN + 1));
		unsigned short max_frame =
		    (unsigned short)(t % 5 == 0 ? 1 + rnd() % 4
						: 1 + rnd() % 200);
		unsigned short mask;
		int ra, rb;
		long tag = (long)t;

		mask = (unsigned short)(rnd() % 2 ? 0
			: (unsigned short)(1u << (rnd() % width)));

		plant(mask, (unsigned short)(rnd() % 8), 0, 0, 0, 0,
		      width, max_frame, 0, 0, (int)(rnd() % 2));

		/*
		 * Every fifth case drives the long-zero detector: 23 zero
		 * bits cannot come out of a uniform random fill often enough
		 * to be relied on, so a quarter of the input is zeroed.
		 */
		if (t % 5 == 0) {
			unsigned i;

			for (i = 0; i < MAXIN / 4; i++) {
				rxa[i] = 0;
				rxb[i] = 0;
			}
		}

		ra = ref_faxvmi_asyc_unpack(&va, dsta, count);
		rb = faxvmi_asyc_unpack(&vb, dstb, count);

		diff_eq_int("asyc return (%ld)", (long)rb, (long)ra, tag);
		compare("asyc", tag);

		if (ra > 0)
			asyc_produced++;
		if (va.overflow)
			asyc_overflowed++;
		if (fra.zero_run_seen)
			asyc_zero_run++;
		if (fra.async_hunt)
			asyc_hunted++;
	}
	diff_eq_int("asyc: characters were produced (%ld)", asyc_produced > 0,
		    1, (long)asyc_produced);
	diff_eq_int("asyc: the destination cap was reached (%ld)",
		    asyc_overflowed > 0, 1, (long)asyc_overflowed);
	diff_eq_int("asyc: the long-zero latch fired (%ld)", asyc_zero_run > 0,
		    1, (long)asyc_zero_run);
	diff_eq_int("asyc: the hunt state was left set (%ld)", asyc_hunted > 0,
		    1, (long)asyc_hunted);
	return diff_end();
}

/* ------------------------------------------------------------------ */
/* faxvmi_hdlc_unframe                                                */

/*
 * A bit sink that packs bits into `width`-bit elements the way the receiver
 * takes them out: the first bit written is the element's most significant
 * significant bit, because the receiver's mask starts at 1 << (width - 1).
 */
struct bitsink {
	unsigned short *buf;
	int cap;
	int nelem;
	unsigned width;
	unsigned acc;
	unsigned nbits;
	int ones;
};

static void
sink_init(struct bitsink *s, unsigned short *buf, int cap, unsigned width)
{
	s->buf = buf;
	s->cap = cap;
	s->nelem = 0;
	s->width = width;
	s->acc = 0;
	s->nbits = 0;
	s->ones = 0;
}

static void
put_bit(struct bitsink *s, int b)
{
	s->acc = (s->acc << 1) | (unsigned)(b & 1);
	s->nbits++;
	if (s->nbits == s->width) {
		if (s->nelem < s->cap)
			s->buf[s->nelem] = (unsigned short)s->acc;
		s->nelem++;
		s->acc = 0;
		s->nbits = 0;
	}
}

/* Pad the part-filled element with mark, which is what an idle line is. */
static void
sink_flush(struct bitsink *s)
{
	while (s->nbits != 0)
		put_bit(s, 1);
}

/*
 * One octet, most significant bit first, with HDLC's zero inserted after
 * every fifth consecutive one when `stuff` is set.  The receiver drops that
 * zero at `ones == 5`, so the two rules are each other's inverse.
 */
static void
put_octet(struct bitsink *s, unsigned oct, int stuff)
{
	int i;

	for (i = 7; i >= 0; i--) {
		int b = (int)((oct >> i) & 1u);

		put_bit(s, b);
		if (b) {
			s->ones++;
			if (stuff && s->ones == 5) {
				put_bit(s, 0);
				s->ones = 0;
			}
		} else {
			s->ones = 0;
		}
	}
}

static void
put_flag(struct bitsink *s)
{
	put_octet(s, 0x7e, 0);
	s->ones = 0;
}

/*
 * A whole frame: flag, the octets with stuffing, the FCS the receiver's own
 * check wants, flag.  The FCS is taken with `faxvmi_gen_fcs16` itself -- the
 * function this tree has already proved against an independent bit-at-a-time
 * model (F8932) -- exactly as `faxvmi_write_frame` takes it, so the encoder
 * asserts nothing about the polynomial that is not already established.
 */
static void
put_frame(struct bitsink *s, const unsigned char *oct, int n)
{
	unsigned short tmp[FRAMECAP + 2];
	unsigned short fcs;
	int i;

	for (i = 0; i < n; i++)
		tmp[i] = oct[i];
	fcs = (unsigned short)ref_faxvmi_gen_fcs16(tmp, (short)n);

	put_flag(s);
	for (i = 0; i < n; i++)
		put_octet(s, oct[i], 1);
	put_octet(s, (unsigned)(fcs >> 8), 1);
	put_octet(s, (unsigned)(fcs & 0xff), 1);
	put_flag(s);
}

static int hdlc_frames, hdlc_zero_frames, hdlc_good_first, hdlc_overflowed;
static int hdlc_flags_eaten, hdlc_stored;

/*
 * One HDLC case.  `count` elements of `rx` are already planted; run both
 * sides and account for what the REFERENCE did.
 */
static int
hdlc_case(short count, long tag)
{
	int ra, rb;

	ra = ref_faxvmi_hdlc_unframe(&va, dsta, count);
	rb = faxvmi_hdlc_unframe(&vb, dstb, count);

	diff_eq_int("hdlc return (%ld)", (long)rb, (long)ra, tag);
	compare("hdlc", tag);

	hdlc_frames += ra;
	if (ra > 0 && dsta[0] == 0)
		hdlc_zero_frames++;
	if (ra > 0 && dsta[0] != 0)
		hdlc_good_first++;
	if (va.overflow)
		hdlc_overflowed++;
	if (fra.frame_len != 0)
		hdlc_stored++;
	return ra;
}

static int
run_hdlc_random(void)
{
	unsigned t;
	unsigned short before;

	diff_begin("faxvmi_hdlc_unframe, random bit streams");
	for (t = 0; t < 400; t++) {
		unsigned short width = (unsigned short)(1 + rnd() % MAXWIDTH);
		short count = (short)(rnd() % (MAXIN + 1));
		unsigned short flags_wanted = (unsigned short)(rnd() % 3);
		unsigned short max_frame = (unsigned short)(1 + rnd() % 48);
		unsigned short mask;

		mask = (unsigned short)(rnd() % 2 ? 0
			: (unsigned short)(1u << (rnd() % width)));

		plant(mask, (unsigned short)(rnd() % 8),
		      (short)(rnd() % (FRAMECAP - 4)), flags_wanted,
		      (short)(rnd() % 8), (int)(rnd() % 2),
		      width, max_frame, 0, 0, 0);

		/*
		 * A quarter of the cases get long runs of ones, which is what
		 * makes flags and stuffed zeros common enough to matter.
		 */
		if (t % 4 == 0) {
			unsigned i;

			for (i = 0; i < MAXIN; i++) {
				unsigned short v = (unsigned short)
				    (rnd() % 4 ? 0xffff : rnd());

				rxa[i] = v;
				rxb[i] = v;
			}
		}

		before = fra.flags_wanted;
		hdlc_case(count, (long)t);
		if (fra.flags_wanted < before)
			hdlc_flags_eaten++;
	}
	/*
	 * Anti-vacuity, all six read back from the REFERENCE's own state
	 * after its own calls (F134).  A random bit stream reaches these
	 * arms; what it does NOT reach is a frame whose FCS checks, which is
	 * why the constructed cases exist as well.
	 */
	diff_eq_int("hdlc random: frames were completed (%ld)",
		    hdlc_frames > 0, 1, (long)hdlc_frames);
	diff_eq_int("hdlc random: a frame was given up on and emitted empty "
		    "(%ld)", hdlc_zero_frames > 0, 1, (long)hdlc_zero_frames);
	diff_eq_int("hdlc random: octets were assembled into the frame buffer "
		    "(%ld)", hdlc_stored > 0, 1, (long)hdlc_stored);
	diff_eq_int("hdlc random: opening flags were counted down (%ld)",
		    hdlc_flags_eaten > 0, 1, (long)hdlc_flags_eaten);
	return diff_end();
}

/*
 * Constructed frames: the good-FCS path, then each repair arm in turn.
 *
 * The three corruptions are chosen so that exactly one repair can rescue
 * each: the address octet, the control octet, and a whole-frame one-bit
 * slip.  The counters below say whether the REFERENCE actually recovered
 * them, which is the only evidence that the arm ran at all.
 */
static int repaired_addr, repaired_ctl, repaired_shift, encoded_ok;

/*
 * MARK AFTER THE CLOSING FLAG IS NOT DECORATION.  The unpacker's loop is a
 * do-while on the ELEMENT count, so the last element of a call contributes
 * exactly ONE bit and its remaining `width - 1` bits stay in `framer->word`
 * for the next call.  Without trailing mark the closing flag's final zero is
 * simply not reached and no frame is ever completed -- which is how this
 * fixture found the property.
 */
static void
put_mark(struct bitsink *s, unsigned width)
{
	unsigned i;

	for (i = 0; i < 2 * width; i++)
		put_bit(s, 1);
	sink_flush(s);
}

static void
build_and_run(const unsigned char *oct, int n, int corrupt, unsigned width,
	      long tag)
{
	struct bitsink s;
	unsigned short tmp[FRAMECAP + 2];
	unsigned short fcs;
	int i;

	for (i = 0; i < n; i++)
		tmp[i] = oct[i];
	fcs = (unsigned short)ref_faxvmi_gen_fcs16(tmp, (short)n);

	plant(0, 0, 0, 0, 0, 0, (unsigned short)width, FRAMECAP, 0, 0, 0);
	sink_init(&s, rxa, MAXIN, width);
	put_flag(&s);

	/*
	 * corrupt == 3 is a one-bit SLIP, and it takes a bit at each end.
	 * The leading bit makes every octet the receiver assembles one bit
	 * late; the seven trailing ones make it assemble one octet MORE, so
	 * that after the repair's single left shift the first `n + 2` octets
	 * are the true frame and the FCS over them checks.  Without the
	 * trailing seven the shift would leave the frame an octet short.
	 */
	if (corrupt == 3) {
		put_bit(&s, 0);
		s.ones = 0;
	}
	for (i = 0; i < n; i++) {
		unsigned o = oct[i];

		if (i == 0 && corrupt == 1)
			o = 0x31;	/* not the T.30 address octet */
		if (i == 1 && corrupt == 2)
			o = 0x77;	/* not the T.30 control octet */
		put_octet(&s, o, 1);
	}
	put_octet(&s, (unsigned)(fcs >> 8), 1);
	put_octet(&s, (unsigned)(fcs & 0xff), 1);
	if (corrupt == 3) {
		for (i = 0; i < 7; i++)
			put_bit(&s, 0);
		s.ones = 0;
	}
	put_flag(&s);
	put_mark(&s, width);
	memcpy(rxb, rxa, sizeof(rxa));

	hdlc_case((short)(s.nelem < MAXIN ? s.nelem : MAXIN), tag);
}

/*
 * Run one constructed case and attribute the recovery, if there was one, to
 * the arm the case was built for.  `hdlc_good_first` is only incremented when
 * the REFERENCE emitted a frame with a non-zero length, which for `corrupt`
 * other than zero can only have happened through a repair.
 */
static void
attribute(const unsigned char *oct, int n, int corrupt, unsigned width,
	  long tag)
{
	int before = hdlc_good_first;

	build_and_run(oct, n, corrupt, width, tag);
	if (hdlc_good_first == before)
		return;
	switch (corrupt) {
	case 0:
		encoded_ok++;
		break;
	case 1:
		repaired_addr++;
		break;
	case 2:
		repaired_ctl++;
		break;
	default:
		repaired_shift++;
		break;
	}
}

static int
run_hdlc_frames(void)
{
	static const unsigned char t30_dis[] = {
		0xff, 0xc8, 0x01, 0x02, 0x03, 0x04
	};
	static const unsigned char t30_short[] = { 0xff, 0xc8 };
	unsigned w;
	int c;

	diff_begin("faxvmi_hdlc_unframe, constructed frames");

	for (w = 8; w <= 16; w += 4) {
		for (c = 0; c <= 3; c++) {
			attribute(t30_dis, (int)sizeof(t30_dis), c, w,
				  (long)(w * 10 + c));
			attribute(t30_short, (int)sizeof(t30_short), c, w,
				  (long)(w * 10 + c) + 1000);
		}
	}

	/*
	 * Anti-vacuity, and every one of these is read back from what the
	 * REFERENCE produced (F134).
	 */
	diff_eq_int("hdlc: a clean frame was decoded (%ld)", encoded_ok > 0, 1,
		    (long)encoded_ok);
	diff_eq_int("hdlc: the address substitution rescued a frame (%ld)",
		    repaired_addr > 0, 1, (long)repaired_addr);
	diff_eq_int("hdlc: the control substitution rescued a frame (%ld)",
		    repaired_ctl > 0, 1, (long)repaired_ctl);
	diff_eq_int("hdlc: the bit-shift repair rescued a frame (%ld)",
		    repaired_shift > 0, 1, (long)repaired_shift);
	diff_eq_int("hdlc: a frame was given up on and emitted empty (%ld)",
		    hdlc_zero_frames > 0, 1, (long)hdlc_zero_frames);
	return diff_end();
}

/*
 * D1075, driven: a frame the output guard refuses is NOT rewound.  One
 * well-formed frame with `max_frame` at 1, so the guard refuses it whatever
 * it contains, and then the three things the object does about that --
 * `vmi->overflow` set, no frame emitted, and the assembly buffer left exactly
 * where it was.  The third is the deviation; the first two are what make it
 * reachable.
 */
static int
run_hdlc_overflow(void)
{
	static const unsigned char body[] = { 0xff, 0xc8, 0x11, 0x22 };
	struct bitsink s;
	int ra, refused;
	short refused_len;

	diff_begin("faxvmi_hdlc_unframe: a frame refused for want of room");

	plant(0, 0, 0, 0, 0, 0, 16, /* max_frame */ 1, 0, 0, 0);
	sink_init(&s, rxa, MAXIN, 16);
	put_frame(&s, body, (int)sizeof(body));
	put_mark(&s, 16);
	memcpy(rxb, rxa, sizeof(rxa));

	ra = hdlc_case((short)(s.nelem < MAXIN ? s.nelem : MAXIN), 9100);
	refused = ra;
	refused_len = fra.frame_len;

	diff_eq_int("overflow: the reference refused the frame (%ld)",
		    ra, 0, (long)ra);
	diff_eq_int("overflow: vmi->overflow was set (%ld)",
		    va.overflow != 0, 1, (long)va.overflow);
	diff_eq_int("overflow: nothing was written to the destination (%ld)",
		    dsta[0] == 0x5a5a, 1, (long)dsta[0]);

	/*
	 * The SAME input again with room for the frame, so it is accepted and
	 * the assembly buffer IS rewound.  The trailing mark contributes a few
	 * octets of its own to both runs -- once `ones` passes six every mark
	 * bit goes back down the data path -- so the deviation is measured as
	 * the DIFFERENCE between the two, which is exactly the six octets
	 * (four of body, two of FCS) the refusal did not discard.  A magic
	 * constant here would have been counting the padding as well.
	 */
	plant(0, 0, 0, 0, 0, 0, 16, /* max_frame */ FRAMECAP, 0, 0, 0);
	sink_init(&s, rxa, MAXIN, 16);
	put_frame(&s, body, (int)sizeof(body));
	put_mark(&s, 16);
	memcpy(rxb, rxa, sizeof(rxa));

	ra = hdlc_case((short)(s.nelem < MAXIN ? s.nelem : MAXIN), 9101);
	diff_eq_int("overflow: the control run accepted the frame (%ld)",
		    ra, 1, (long)ra);
	diff_eq_int("overflow: the control run emitted six octets (%ld)",
		    (long)dsta[0], 6, (long)dsta[0]);
	diff_eq_int("D1075: the refusal left six octets the acceptance "
		    "discarded (%ld)",
		    (long)(refused_len - fra.frame_len), 6,
		    (long)refused_len);
	(void)refused;
	return diff_end();
}

/*
 * D1073, driven rather than described: several frames in ONE call write more
 * than `max_frame` elements into the destination, because the guard does not
 * accumulate.  The destination here is sized for the whole input, so the
 * overrun is measured instead of being a crash.
 */
static int d1073_fired;

static int
run_hdlc_d1073(void)
{
	struct bitsink s;
	static const unsigned char body[] = { 0xff, 0xc8, 0x11, 0x22 };
	int written, k;

	diff_begin("faxvmi_hdlc_unframe: the output guard does not accumulate");

	plant(0, 0, 0, 0, 0, 0, 16, /* max_frame */ 8, 0, 0, 0);
	sink_init(&s, rxa, MAXIN, 16);
	put_frame(&s, body, (int)sizeof(body));
	put_frame(&s, body, (int)sizeof(body));
	put_frame(&s, body, (int)sizeof(body));
	sink_flush(&s);
	memcpy(rxb, rxa, sizeof(rxa));

	hdlc_case((short)(s.nelem < MAXIN ? s.nelem : MAXIN), 9000);

	/*
	 * Count what the REFERENCE wrote: each frame is one length element
	 * plus that many octets, and max_frame is 8.  More than 8 elements
	 * written is the deviation firing.
	 */
	written = 0;
	k = 0;
	while (k < MAXOUT && dsta[k] != 0x5a5a) {
		written += 1 + (int)dsta[k];
		k += 1 + (int)dsta[k];
	}
	if (written > 8)
		d1073_fired = 1;
	diff_eq_int("D1073: more than max_frame elements were written (%ld)",
		    d1073_fired, 1, (long)written);
	return diff_end();
}

/*
 * CONSECUTIVE CALLS ON ONE OBJECT, which every suite above forgot.
 *
 * The whole point of the framer is that a call ends mid-element and the next
 * one carries on: `mask`, `word`, `acc`, `bit`, `ones`, `frame_len` and the
 * async hunt all survive the return.  Re-planting before every call compares
 * the SAVE but never the RESTORE, and the injection ritual proved it -- a
 * mutant that wrote the input cursor back to `link->buf` (D1072) survived
 * every single-call suite, because a fixture that re-points `rx` each time
 * cannot see it.  F8790's argument with a different field.
 *
 * So: plant once, then five calls with no replanting, comparing the whole
 * state after each.
 */
static int multi_carried, multi_produced;

static int
run_multicall(void)
{
	unsigned t;

	diff_begin("the unpackers across consecutive calls");
	for (t = 0; t < 90; t++) {
		unsigned short width = (unsigned short)(1 + rnd() % MAXWIDTH);
		int which = (int)(t % 3);
		unsigned i;
		int k;

		plant(0, 0, 0, 0, 0, 0, width, 200, 0, 0, 1);
		/*
		 * Flag-rich, so the HDLC arm has frames to find; the same
		 * stream serves all three unpackers.
		 */
		for (i = 0; i < MAXIN; i++) {
			unsigned short v = (unsigned short)
			    (rnd() % 3 ? 0xffff : rnd());

			rxa[i] = v;
			rxb[i] = v;
		}

		for (k = 0; k < 5; k++) {
			short count = (short)(1 + rnd() % 8);
			long tag = (long)(t * 10 + k);
			int ra, rb;

			if (fra.unpack_mask != 0)
				multi_carried++;

			if (which == 0) {
				ra = ref_faxvmi_simp_unpack(&va, dsta, count);
				rb = faxvmi_simp_unpack(&vb, dstb, count);
			} else if (which == 1) {
				ra = ref_faxvmi_asyc_unpack(&va, dsta, count);
				rb = faxvmi_asyc_unpack(&vb, dstb, count);
			} else {
				ra = ref_faxvmi_hdlc_unframe(&va, dsta, count);
				rb = faxvmi_hdlc_unframe(&vb, dstb, count);
			}
			diff_eq_int("multicall return (%ld)", (long)rb,
				    (long)ra, tag);
			compare("multicall", tag);
			if (ra > 0)
				multi_produced++;
		}
	}
	/*
	 * Both counters come from the reference's own state and answers: the
	 * first says a call really did start part-way through an element,
	 * which is the state the restore has to reproduce.
	 */
	diff_eq_int("multicall: a call resumed mid-element (%ld)",
		    multi_carried > 0, 1, (long)multi_carried);
	diff_eq_int("multicall: output was produced (%ld)", multi_produced > 0,
		    1, (long)multi_produced);
	return diff_end();
}

/* ------------------------------------------------------------------ */
/* faxvmi_write_fifo and faxvmi_write_frame                           */

static int wf_full, wf_partial, wf_whole, wf_cleared, wf_untouched;

static int
run_write_fifo(void)
{
	unsigned t;

	diff_begin("faxvmi_write_fifo");
	for (t = 0; t < 400; t++) {
		unsigned short *pa, *pb;
		unsigned short occ = (unsigned short)(rnd() % (FIFOCAP + 1));
		unsigned short wr = (unsigned short)(rnd() % FIFOCAP);
		short count = (short)(rnd() % 80);
		int ra, rb, before;
		long tag = (long)t;

		plant(0, 0, 0, 0, 0, 0, 8, 64, wr, occ, 0);
		fill(srca, (unsigned)sizeof(srca));
		memcpy(srcb, srca, sizeof(srca));

		before = va.underrun;
		pa = srca;
		pb = srcb;
		ra = ref_faxvmi_write_fifo(&va, &pa, count);
		rb = faxvmi_write_fifo(&vb, &pb, count);

		diff_eq_int("write_fifo return (%ld)", (long)rb, (long)ra,
			    tag);
		/* the cursor by OFFSET, never by pointer value (F8497) */
		diff_eq_int("write_fifo cursor (%ld)", (long)(pb - srcb),
			    (long)(pa - srca), tag);
		diff_eq_int("write_fifo source untouched (%ld)",
			    memcmp(srca, srcb, sizeof(srca)), 0, tag);
		compare("write_fifo", tag);

		if (ra < (int)count)
			wf_full++;
		else if (count != 0)
			wf_whole++;
		if (ra > 0 && ra < (int)count)
			wf_partial++;
		if (va.underrun == 0 && before != 0)
			wf_cleared++;
		if (ra < (int)count && ra > 0 && va.underrun == before
		    && before != 0)
			wf_untouched++;
	}
	diff_eq_int("write_fifo: a full ring was hit (%ld)", wf_full > 0, 1,
		    (long)wf_full);
	diff_eq_int("write_fifo: whole requests were taken (%ld)", wf_whole > 0,
		    1, (long)wf_whole);
	diff_eq_int("write_fifo: partial writes happened (%ld)", wf_partial > 0,
		    1, (long)wf_partial);
	diff_eq_int("write_fifo: underrun was cleared (%ld)", wf_cleared > 0, 1,
		    (long)wf_cleared);
	/*
	 * D1070, and it is the whole point of the register-held flag: a
	 * partial write leaves the field ALONE.  Counted from the reference.
	 */
	diff_eq_int("D1070: a partial write left underrun alone (%ld)",
		    wf_untouched > 0, 1, (long)wf_untouched);
	return diff_end();
}

static int wr_frames, wr_stopped, wr_partial, wr_untouched;

/* `nf` length-prefixed frames laid end to end, all inside `srca`. */
static void
build_frames(int nf, int maxlen)
{
	int i = 0, k;

	fill(srca, (unsigned)sizeof(srca));
	for (k = 0; k < nf && i + maxlen + 2 < SRCMAX; k++) {
		int len = (int)(rnd() % (unsigned)(maxlen + 1));
		int j;

		srca[i++] = (unsigned short)len;
		for (j = 0; j < len; j++)
			srca[i++] = (unsigned short)(rnd() & 0xff);
	}
	srca[i] = 0;
	memcpy(srcb, srca, sizeof(srca));
}

static int
run_write_frame(void)
{
	unsigned t;

	diff_begin("faxvmi_write_frame");
	for (t = 0; t < 400; t++) {
		unsigned short *pa, *pb;
		unsigned short occ = (unsigned short)(rnd() % (FIFOCAP + 1));
		unsigned short wr = (unsigned short)(rnd() % FIFOCAP);
		short count = (short)(rnd() % 6);
		int ra, rb, before;
		long tag = (long)t;

		plant(0, 0, 0, 0, 0, 0, 8, 64, wr, occ, 0);
		build_frames((int)count + 2, (int)(3 + rnd() % 12));

		before = va.underrun;
		pa = srca;
		pb = srcb;
		ra = ref_faxvmi_write_frame(&va, &pa, count);
		rb = faxvmi_write_frame(&vb, &pb, count);

		diff_eq_int("write_frame return (%ld)", (long)rb, (long)ra,
			    tag);
		diff_eq_int("write_frame cursor (%ld)", (long)(pb - srcb),
			    (long)(pa - srca), tag);
		diff_eq_int("write_frame source untouched (%ld)",
			    memcmp(srca, srcb, sizeof(srca)), 0, tag);
		compare("write_frame", tag);

		wr_frames += ra;
		if (ra < (int)count)
			wr_stopped++;
		if (ra > 0 && ra < (int)count)
			wr_partial++;
		if (ra > 0 && va.underrun == before && before != 0)
			wr_untouched++;
	}
	diff_eq_int("write_frame: frames were written (%ld)", wr_frames > 0, 1,
		    (long)wr_frames);
	diff_eq_int("write_frame: the fit test stopped a run (%ld)",
		    wr_stopped > 0, 1, (long)wr_stopped);
	diff_eq_int("write_frame: partial runs happened (%ld)", wr_partial > 0,
		    1, (long)wr_partial);
	return diff_end();
}

/*
 * The FCS the ring receives is `faxvmi_gen_fcs16`'s, high octet first.
 * Checked against the object rather than asserted: one frame into an empty
 * ring, and the last two elements written are compared with the value the
 * already-proved FCS function gives for the same data.
 */
static int
run_write_frame_fcs(void)
{
	unsigned short *pa, *pb;
	unsigned short expect;
	int ra, rb;
	int i;

	diff_begin("faxvmi_write_frame: the FCS it appends");
	plant(0, 0, 0, 0, 0, 0, 8, 64, 0, 0, 0);
	fill(srca, (unsigned)sizeof(srca));
	srca[0] = 6;
	for (i = 0; i < 6; i++)
		srca[1 + i] = (unsigned short)(0x30 + i);
	srca[7] = 0;
	memcpy(srcb, srca, sizeof(srca));

	pa = srca;
	pb = srcb;
	ra = ref_faxvmi_write_frame(&va, &pa, 1);
	rb = faxvmi_write_frame(&vb, &pb, 1);
	diff_eq_int("fcs case: return (%ld)", (long)rb, (long)ra, 0);
	compare("fcs case", 0);
	diff_eq_int("fcs case: one frame was written (%ld)", ra, 1, 0);

	expect = (unsigned short)ref_faxvmi_gen_fcs16(&srca[1], 6);
	diff_eq_int("fcs case: length element is len + 2 (%ld)",
		    (long)fifoa[0], 8, 0);
	diff_eq_int("fcs case: high octet (%ld)", (long)fifoa[7],
		    (long)(expect >> 8), 0);
	diff_eq_int("fcs case: low octet (%ld)", (long)fifoa[8],
		    (long)(expect & 0xff), 0);
	diff_eq_int("fcs case: occupancy is len + 3 (%ld)", (long)fra.count, 9,
		    0);
	return diff_end();
}

/* ------------------------------------------------------------------ */

int
main(void)
{
	int bad = 0;

	/*
	 * The repair pass's diagnostics are the author's own words and they
	 * are real branches: run the whole suite at level 0 and then the
	 * constructed frames again at level 2, so the printing arms are
	 * compared too.  F150's argument, and DSPLIB_DEBUG_ON() is `> 1`.
	 */
	dsplibs_debug_level = 0;

	bad |= run_simp();
	bad |= run_asyc();
	bad |= run_hdlc_random();
	bad |= run_hdlc_frames();
	bad |= run_hdlc_overflow();
	bad |= run_hdlc_d1073();
	bad |= run_multicall();
	bad |= run_write_fifo();
	bad |= run_write_frame();
	bad |= run_write_frame_fcs();

	dsplibs_debug_level = 2;
	bad |= run_hdlc_frames();
	bad |= run_hdlc_random();
	dsplibs_debug_level = 0;

	return bad;
}
