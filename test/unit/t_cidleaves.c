/*
 * t_cidleaves.c -- differential test of the Caller ID leaves:
 *
 *   cid_threshold         .text 0x08fdd0      (cid.c)
 *   cid_value             .text 0x08fe00      (cid.c)
 *   _look_for             .text 0x0903c0      (cid.c)
 *   _look_for_other_than  .text 0x090410      (cid.c)
 *   data_raw              .text 0x090470      (Data.c)
 *   pack_next_bit         .text 0x091b80      (Rxcid.c)
 *
 * The TLV walkers get constructed messages, not raw noise: a random length
 * byte can send the walk BACKWARDS (it is a signed char) or into a cycle,
 * and a non-terminating input would hang both sides identically -- proving
 * nothing.  So the messages are built entry by entry with small positive
 * lengths, and the hostile shapes (a negative length, a length overshooting
 * the end) are added as bounded hand-built cases with guard space around the
 * buffer for the out-of-range reads both sides then perform.
 *
 * pack_next_bit is driven as a stream -- the framer's whole point is what
 * carries between calls -- with bit values 0 and 1 plus the occasional 2
 * and -1, which state 0 must count as spaces and state 2 must smear into
 * high positions exactly as the object does.  threshold stays small (0..7): the
 * shift count in state 2 starts at threshold, and a huge value would make the
 * C shift undefined where the hardware masks by 31 -- unreachable from
 * create_cid's seed of 2 and untestable honestly.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/cid.h"
#include "dsplib/cid_modem.h"
#include "dsplib/dtmf_rx.h"

extern void ref_cid_threshold(void *ctx, int thr);
extern void ref_cid_value(void *ctx, int v);
extern int ref__look_for(const char *buf, char tag);
extern int ref__look_for_other_than(const char *buf, short start, int except);
extern void ref_data_raw(const char *buf, char *out);
extern void ref_pack_next_bit(short bit, void *cid);

static unsigned long seed = 20260830UL;

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

static long
first_diff(const void *pa, const void *pb, unsigned n)
{
	const unsigned char *a = (const unsigned char *)pa;
	const unsigned char *b = (const unsigned char *)pb;
	unsigned i;

	for (i = 0; i < n; i++)
		if (a[i] != b[i])
			return (long)i;
	return -1;
}

/* ------------------------------------------------------------------ */
/* cid_threshold / cid_value                                          */

struct cbox {
	struct cid_modem ctx;
	struct dtmf_rx dtmf;
	struct cid fsk;
	unsigned char guard[16];
};

static void
cfresh(struct cbox *a, struct cbox *b, int mode)
{
	fill(a, (unsigned)sizeof(*a));
	memcpy(&a->ctx.mode, &mode, sizeof(mode));
	memcpy(b, a, sizeof(*a));
	a->ctx.dtmf = &a->dtmf;
	a->ctx.fsk = &a->fsk;
	b->ctx.dtmf = &b->dtmf;
	b->ctx.fsk = &b->fsk;
}

static void
ccompare(struct cbox *a, struct cbox *b, long tag)
{
	diff_eq_int("ctx first differing byte (mode %ld)",
		    first_diff((unsigned char *)&a->ctx + 8,
			       (unsigned char *)&b->ctx + 8,
			       (unsigned)sizeof(a->ctx) - 8), -1, tag);
	diff_eq_int("dtmf first differing byte (mode %ld)",
		    first_diff(&a->dtmf, &b->dtmf, (unsigned)sizeof(a->dtmf)),
		    -1, tag);
	diff_eq_int("fsk first differing byte (mode %ld)",
		    first_diff(&a->fsk, &b->fsk, (unsigned)sizeof(a->fsk)),
		    -1, tag);
	diff_eq_int("guard intact (mode %ld)",
		    memcmp(a->guard, b->guard, sizeof(a->guard)), 0, tag);
}

static int
run_setters(void)
{
	static const int modes[] = { 0, 1, 2, 5, 7, -1 };
	static struct cbox a, b;
	unsigned m, t;

	diff_begin("cid_threshold / cid_value");
	for (m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
		for (t = 0; t < 40; t++) {
			int thr = (int)(rnd() & 0x1ffff) - 0x8000;
			int v = (int)rnd() - 0x800000;

			cfresh(&a, &b, modes[m]);
			ref_cid_threshold(&a.ctx, thr);
			cid_threshold(&b.ctx, thr);
			ccompare(&a, &b, modes[m]);

			ref_cid_value(&a.ctx, v);
			cid_value(&b.ctx, v);
			ccompare(&a, &b, modes[m]);
		}
	}
	return diff_end();
}

/* ------------------------------------------------------------------ */
/* the TLV walkers, and data_raw                                      */

#define ARENA	1024
#define BUFAT	256		/* guard space before AND after       */

static int
run_walkers(void)
{
	static unsigned char arena_a[ARENA];
	char *buf;
	unsigned trial;

	buf = (char *)arena_a + BUFAT;

	diff_begin("_look_for / _look_for_other_than / data_raw");
	for (trial = 0; trial < 400; trial++) {
		static char out_a[ARENA], out_b[ARENA];
		unsigned pos = 2, nent = rnd() % 6;
		unsigned e;
		int ra, rb;
		char tag;
		short start;
		int except;

		/*
		 * Every byte positive: the walk then only ever moves forward
		 * (len + 2 >= 2 per step), so any message length terminates.
		 */
		fill(arena_a, ARENA);
		for (e = 0; e < ARENA; e++)
			arena_a[e] &= 0x7f;

		/* Build nent entries of small length over the random fill. */
		for (e = 0; e < nent; e++) {
			unsigned len = rnd() % 8;

			buf[pos] = (char)(rnd() % 10);	/* tag, incl 1/2/7 */
			buf[pos + 1] = (char)len;
			pos += len + 2;
		}
		buf[0] = (char)(rnd() & 0x7f);
		/* Message length: exactly the built span, short of it, past
		 * it into the (positive, so bounded) random tail, or
		 * negative -- which exits the walk before its first read. */
		switch (rnd() % 4) {
		case 0:
			buf[1] = (char)pos;
			break;
		case 1:
			buf[1] = (char)(pos > 4 ? pos - 3 : 0);
			break;
		case 2:
			buf[1] = (char)((pos + rnd() % 20) & 0x7f);
			break;
		default:
			buf[1] = (char)(0x80 | (rnd() & 0x7f));	/* negative */
			break;
		}

		tag = (char)(rnd() % 12);
		start = (short)(rnd() % 8);
		except = (int)(rnd() % 24);

		ra = ref__look_for(buf, tag);
		rb = _look_for(buf, tag);
		diff_eq_int("_look_for trial %ld", rb, ra, (long)trial);

		ra = ref__look_for_other_than(buf, start, except);
		rb = _look_for_other_than(buf, start, except);
		diff_eq_int("_look_for_other_than trial %ld", rb, ra,
			    (long)trial);

		memset(out_a, 0x5a, sizeof(out_a));
		memset(out_b, 0x5a, sizeof(out_b));
		ref_data_raw(buf, out_a);
		data_raw(buf, out_b);
		diff_eq_int("data_raw first differing byte (trial %ld)",
			    first_diff(out_a, out_b, sizeof(out_a)), -1,
			    (long)trial);
	}

	/*
	 * The signed-length hazard, hand-built so it terminates: an entry
	 * with length -1 steps the position forward by ONE (2 + -1 + 2 = 3
	 * from 2), landing on what was the length byte; the entry planted
	 * there then jumps clear of the message.  Both sides must take the
	 * same crooked walk.
	 */
	{
		int ra, rb;

		memset(arena_a, 0, ARENA);
		buf[0] = 4;
		buf[1] = 10;
		buf[2] = 5;	/* tag 5, length -1 */
		buf[3] = (char)-1;
		buf[4] = 30;	/* read as the next tag from pos 3's view:
				 * pos 3 -> tag buf[3] = -1, len buf[4]=30 */
		ra = ref__look_for(buf, (char)9);
		rb = _look_for(buf, (char)9);
		diff_eq_int("negative length, _look_for (%ld)", rb, ra, 0);
		ra = ref__look_for_other_than(buf, (short)2, 99);
		rb = _look_for_other_than(buf, (short)2, 99);
		diff_eq_int("negative length, _look_for_other_than (%ld)",
			    rb, ra, 0);
	}
	return diff_end();
}

/* ------------------------------------------------------------------ */
/* pack_next_bit                                                      */

static int
run_framer(void)
{
	static struct cid a, b;
	unsigned stream, i;
	int seen_bytes = 0;

	diff_begin("pack_next_bit");
	for (stream = 0; stream < 12; stream++) {
		fill(&a, (unsigned)sizeof(a));
		a.threshold = (short)(stream % 8);
		a.pack_state = (short)((stream & 3) == 3 ? rnd() % 4 : 0);
		a.pack_pos = (short)(rnd() % 8);
		a.pack_len = (short)(rnd() % 16);
		a.mark_bal = (short)((int)(rnd() % 24) - 8);
		memcpy(&b, &a, sizeof(a));

		for (i = 0; i < 3000; i++) {
			short bit;
			unsigned r = (unsigned)rnd() % 16;

			bit = (short)(r < 7 ? 1 : (r < 14 ? 0
				      : (r == 14 ? 2 : -1)));
			ref_pack_next_bit(bit, &a);
			pack_next_bit(bit, &b);

			if ((i % 250) == 249 || a.pack_len > 100) {
				diff_eq_int(
				    "object first differing byte (bit %ld)",
				    first_diff(&a, &b, (unsigned)sizeof(a)),
				    -1, (long)(stream * 10000 + i));
				if (a.pack_len > 100) {
					if (a.pack_len > 0)
						seen_bytes = 1;
					a.pack_len = 0;
					b.pack_len = 0;
				}
			}
			if (a.pack_len > 4)
				seen_bytes = 1;
		}
		diff_eq_int("object first differing byte (stream %ld)",
			    first_diff(&a, &b, (unsigned)sizeof(a)), -1,
			    (long)stream);
	}
	diff_eq_int("the framer assembled bytes at all (%ld)",
		    seen_bytes, 1, (long)seen_bytes);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_setters();
	rc |= run_walkers();
	rc |= run_framer();
	return rc;
}
