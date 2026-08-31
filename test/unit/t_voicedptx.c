/*
 * t_voicedptx.c -- differential test of `voice_tx`, the voice service's
 * transmit block handler (0xafd60, 1150 bytes).
 *
 * IT HAS TWO HALVES WITH A FIFO BETWEEN THEM and they are driven separately.
 * The first un-escapes DLE out of the host's byte stream and writes what
 * survives to the ring; the second takes a whole block back out, removes its
 * mean and converts.  A fixture that only fed the first half would leave the
 * second running on an empty ring for ever, so the ring is PRE-LOADED for the
 * cases that need it, identically on both sides.
 *
 * EVERY INPUT IS CONSTRUCTED.  `voice_tx` has no caller in the blob except
 * through the handler slot; it is GLOBAL, so the `ref_` alias takes the
 * ordinary convention (finding F8787 reads the prologue rather than trusting
 * the symbol table, which is F8770's correction).
 *
 * THE CONTEXT IS COMPARED WHOLE, 2,012 bytes, with only the per-side pointer
 * slots masked -- D955/F8587 again: most of `struct voice_ctx` is `pad_*`,
 * `voice_tx` uses two of its fields as SUBSCRIPT BASES, and a blob-against-
 * blob dry run cannot see an unplanted subscript because both sides read the
 * same wild index.  Both sides are prefilled with 0xa5 and only what a case
 * needs is planted on top.
 *
 * THE RING IS COMPARED TOO, contents included, because a wrong count handed
 * to FIFO8_write shows there and nowhere else.
 *
 * FLOATS ARE COMPARED EXACTLY.  `voice_tx`'s mean is a float accumulator
 * narrowed on every iteration (the object stores and reloads it through
 * `0x28(%esp)`), so a `double` accumulator would drift and a tolerance would
 * hide it.
 *
 * THREE DEVIATIONS ARE DRIVEN ON PURPOSE, not merely tolerated: D996 (222
 * bytes read into a 200-byte staging area at 11025 Hz), D998 (the scan reads
 * one byte past the count when the last byte is a DLE -- the byte after the
 * count is PLANTED so both sides read the same thing) and D999 (the float
 * arm of the short-data fill is a fixed 160 and the linear arm is `*countp`).
 *
 * COVERAGE IS ASSERTED FROM THE RUN (F134).
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/fifo8.h"
#include "dsplib/sysdep.h"
#include "dsplib/voice.h"
#include "dsplib/voicecmd.h"

extern unsigned int ref_dsplibs_debug_level;

extern int ref_voice_tx(struct voice_ctx *v, short *rx_lin, float *rx_flt,
			float *tx_flt, short *tx_lin,
			unsigned short *hostcount, unsigned short *countp);
extern struct fifo8 *ref_FIFO8_create(struct fifo8 *f,
				      const struct fifo8_cfg *cfg);
extern short ref_FIFO8_write(struct fifo8 *f, const unsigned char *src,
			     unsigned short n);

#define NBUF	512

/* A ring long enough for every case here, with a distinctive pad byte. */
static const struct fifo8_cfg ring_cfg = { 0x1111, 400, 0x5b };

/* The last context OUR side produced, so a case can be checked absolutely. */
static struct voice_ctx last_ours;
static int last_ret;

static long n_cases;
static long arm_enough, arm_short, arm_report, arm_latched, arm_etx;
static long arm_dle_double, arm_dle_cmd, arm_dle_tail, arm_default_fmt;
static long arm_lin_out, arm_flt_out, arm_overrun;

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

static void
ctx_compare(const char *what, const struct voice_ctx *ours,
	    const struct voice_ctx *ref, long tag)
{
	static struct voice_ctx ca, cb;

	cb = *ours;
	ca = *ref;
	cb.beepgen = 0;
	ca.beepgen = 0;
	cb.detector = 0;
	ca.detector = 0;
	cb.handler = 0;
	ca.handler = 0;
	cb.fifo = 0;
	ca.fifo = 0;
	cb.silence = 0;
	ca.silence = 0;
	cb.dp = 0;
	ca.dp = 0;
	diff_eq_obj(what, struct voice_ctx, &cb, &ca, tag);
}

static void
fifo_compare(const char *what, const struct fifo8 *ours,
	     const struct fifo8 *ref, long tag)
{
	static struct fifo8 ca, cb;
	int i;

	cb = *ours;
	ca = *ref;
	cb.buf = 0;
	ca.buf = 0;
	diff_eq_obj(what, struct fifo8, &cb, &ca, tag);
	for (i = 0; i < (int)ring_cfg.size; i++)
		if (ours->buf[i] != ref->buf[i]) {
			diff_eq_int("ring contents", i, -1, tag);
			return;
		}
	diff_eq_int("ring contents", 0, 0, tag);
}

static void
flt_compare(const char *what, const float *ours, const float *ref, int n,
	    long tag)
{
	int i;

	for (i = 0; i < n; i++)
		if (ours[i] != ref[i]) {
			diff_eq_int(what, i, -1, tag);
			return;
		}
	diff_eq_int(what, 0, 0, tag);
}

static void
lin_compare(const char *what, const short *ours, const short *ref, int n,
	    long tag)
{
	int i;

	for (i = 0; i < n; i++)
		if (ours[i] != ref[i]) {
			diff_eq_int(what, i, -1, tag);
			return;
		}
	diff_eq_int(what, 0, 0, tag);
}

/*
 * One case.
 *
 *   host[0..hostlen)  the escaped byte stream; host[hostlen] is PLANTED so
 *                     D998's one-past read is the same on both sides.
 *   preload           bytes pushed into the ring before the call, so the
 *                     second half has something to work on.
 */
static void
tx_case(unsigned short bits, unsigned short rate, int out_format,
	const unsigned char *host, unsigned short hostlen, int preload,
	unsigned short count, int dle_etx, short underrun, int mode,
	int int_0014, long tag)
{
	static struct voice_ctx a, b;
	static unsigned char hosta[NBUF], hostb[NBUF];
	static float flta[NBUF], fltb[NBUF];
	static short lina[NBUF], linb[NBUF];
	static float txa[NBUF], txb[NBUF];
	static unsigned char pre[512];
	struct fifo8 *fa, *fb;
	unsigned short hca, hcb, cna, cnb;
	int ra, rb, i;
	int was_short;

	memset(&a, HARNESS_MALLOC_FILL, sizeof a);
	memset(&b, HARNESS_MALLOC_FILL, sizeof b);

	fa = ref_FIFO8_create(0, &ring_cfg);
	fb = FIFO8_create(0, &ring_cfg);
	if (fa == 0 || fb == 0) {
		diff_eq_int("both rings created", 0, 1, tag);
		return;
	}
	for (i = 0; i < preload; i++)
		pre[i] = (unsigned char)(i * 7 + 11);
	if (preload > 0) {
		ref_FIFO8_write(fa, pre, (unsigned short)preload);
		FIFO8_write(fb, pre, (unsigned short)preload);
	}

	a.fifo = fa;
	b.fifo = fb;
	a.rate_bits.s.rate = rate;
	b.rate_bits.s.rate = rate;
	a.rate_bits.s.bits = bits;
	b.rate_bits.s.bits = bits;
	a.out_format = out_format;
	b.out_format = out_format;
	a.dle_etx = dle_etx;
	b.dle_etx = dle_etx;
	a.dle_can = 0;
	b.dle_can = 0;
	a.underrun = underrun;
	b.underrun = underrun;
	a.mode = mode;
	b.mode = mode;
	a.int_0014 = int_0014;
	b.int_0014 = int_0014;

	memset(hosta, 0xc3, sizeof hosta);
	memset(hostb, 0xc3, sizeof hostb);
	memcpy(hosta, host, hostlen + 1u);	/* the planted tail byte */
	memcpy(hostb, host, hostlen + 1u);
	memset(flta, HARNESS_MALLOC_FILL, sizeof flta);
	memset(fltb, HARNESS_MALLOC_FILL, sizeof fltb);
	memset(lina, HARNESS_MALLOC_FILL, sizeof lina);
	memset(linb, HARNESS_MALLOC_FILL, sizeof linb);
	memset(txa, HARNESS_MALLOC_FILL, sizeof txa);
	memset(txb, HARNESS_MALLOC_FILL, sizeof txb);
	hca = hostlen;
	hcb = hostlen;
	cna = count;
	cnb = count;

	was_short = (int)fb->count;

	ra = ref_voice_tx(&a, (short *)hosta, flta, txa, lina, &hca, &cna);
	rb = voice_tx(&b, (short *)hostb, fltb, txb, linb, &hcb, &cnb);

	diff_eq_int("voice_tx return", rb, ra, tag);
	diff_eq_int("voice_tx hostcount", hcb, hca, tag);
	diff_eq_int("voice_tx countp", cnb, cna, tag);
	diff_eq_int("voice_tx leaves countp at 0", cnb, 0, tag);
	ctx_compare("context after voice_tx", &b, &a, tag);
	fifo_compare("ring after voice_tx", fb, fa, tag);
	flt_compare("voice_tx rx_flt", fltb, flta, NBUF, tag);
	lin_compare("voice_tx tx_lin", linb, lina, NBUF, tag);
	flt_compare("voice_tx leaves tx_flt alone", txb, txa, NBUF, tag);
	for (i = 0; i < NBUF; i++)
		if (hosta[i] != hostb[i]) {
			diff_eq_int("voice_tx leaves its input alone", i, -1,
				    tag);
			break;
		}
	if (i == NBUF)
		diff_eq_int("voice_tx leaves its input alone", 0, 0, tag);

	/*
	 * The room reported back, stated absolutely: 170 less what the ring
	 * holds, floored at zero.  This is the half of the sixth argument
	 * that F8786 types, and it is checked against the arithmetic rather
	 * than only against the blob.
	 */
	{
		int room = 170 - (int)fb->count;

		if (room < 0)
			room = 0;
		diff_eq_int("hostcount is the ring's free room", hcb,
			    (long)room, tag);
	}

	n_cases++;
	(void)was_short;
	last_ours = b;
	last_ret = rb;
	/*
	 * A dispatched command is NOT visible in the return value -- the
	 * underrun and mode arms both overwrite it further down -- so it is
	 * counted from the flag the command set instead.
	 */
	if (b.dle_can != 0 || b.dle_etx != dle_etx)
		arm_dle_cmd++;
	if (b.underrun && !underrun)
		arm_report++;
	if (underrun && rb != 13)
		arm_latched++;
	if (dle_etx)
		arm_etx++;
	if ((unsigned int)(out_format - 1) <= 1u)
		arm_lin_out++;
	else
		arm_flt_out++;

	sysdep_free(fa->buf);
	sysdep_free(fa);
	sysdep_free(fb->buf);
	sysdep_free(fb);
}

/* ------------------------------------------------------------------ */

/* Plain data, no DLE anywhere; the tail byte is planted but never read. */
static const unsigned char plain[] = {
	0x00, 0x01, 0x7f, 0x80, 0xff, 0x40, 0x11, 0x0f,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x99
};

/* A doubled DLE in the middle: one literal 0x10 reaches the ring. */
static const unsigned char dbl[] = {
	0x41, 0x10, 0x10, 0x42, 0x43, 0x99
};

/* DLE <ETX>, DLE <CAN>, and DLE <unknown>: three command arms. */
static const unsigned char cmd_etx[] = { 0x41, 0x10, 0x03, 0x42, 0x99 };
static const unsigned char cmd_can[] = { 0x41, 0x10, 0x18, 0x42, 0x99 };
static const unsigned char cmd_oth[] = { 0x41, 0x10, 0x55, 0x42, 0x99 };

/* D998: the last counted byte is a DLE, so the planted 0x10 past it is read. */
static const unsigned char tail_dle_dbl[] = { 0x41, 0x42, 0x10, 0x10 };
/* ...and again with a non-DLE past the end, which becomes a command. */
static const unsigned char tail_dle_cmd[] = { 0x41, 0x42, 0x10, 0x03 };

static int
t_scan(void)
{
	long tag = 0;

	diff_begin("voice_tx's DLE un-escape, every arm");

	tx_case(8, 8000, 0, plain, 16, 0, 40, 0, 0, 2, 2, tag++);
	tx_case(8, 8000, 1, plain, 16, 0, 40, 0, 0, 2, 2, tag++);
	tx_case(8, 8000, 0, plain, 0, 0, 40, 0, 0, 2, 2, tag++);
	tx_case(8, 8000, 0, plain, 1, 0, 40, 0, 0, 2, 2, tag++);

	tx_case(8, 8000, 0, dbl, 5, 0, 40, 0, 0, 2, 2, tag++);
	arm_dle_double++;
	tx_case(8, 8000, 1, dbl, 5, 0, 40, 0, 0, 2, 2, tag++);

	tx_case(8, 8000, 0, cmd_etx, 4, 0, 40, 0, 0, 2, 2, tag++);
	diff_eq_int("<DLE><ETX> set dle_etx", last_ours.dle_etx, 1, tag);
	tx_case(8, 8000, 0, cmd_can, 4, 0, 40, 0, 0, 2, 2, tag++);
	diff_eq_int("<DLE><CAN> set dle_can", last_ours.dle_can, 1, tag);
	tx_case(8, 8000, 0, cmd_oth, 4, 0, 40, 0, 0, 2, 2, tag++);
	diff_eq_int("an unknown command set neither",
		    last_ours.dle_etx == 0 && last_ours.dle_can == 0, 1, tag);

	/*
	 * THE ONE CASE IN WHICH A COMMAND'S ANSWER SURVIVES TO THE CALLER.
	 * `voice_tx` assigns `voice_dle_command`'s result to its own return
	 * value, and both the underrun arm and the mode arm overwrite it
	 * further down -- so the assignment is only observable when the ring
	 * has a whole block AND the two modes agree.  Without this case a
	 * mutant that threw the answer away survived the whole suite, which
	 * is F134's argument arriving from the mutation side.
	 */
	tx_case(8, 8000, 0, cmd_can, 4, 300, 40, 0, 0, 2, 2, tag++);
	diff_eq_int("<DLE><CAN>'s 9 reaches the caller", last_ret,
		    VOICE_DLE_CAN_STATUS, tag);
	tx_case(8, 8000, 0, cmd_etx, 4, 300, 40, 0, 0, 2, 2, tag++);
	diff_eq_int("<DLE><ETX>'s 0 reaches the caller", last_ret, 0, tag);
	arm_enough += 2;

	/*
	 * D998, both shapes of the one-past read, and the second is proof
	 * that the read HAPPENED: the counted bytes end with a bare DLE, so
	 * the 0x03 planted past the count is the only thing that can have set
	 * `dle_etx`.
	 */
	tx_case(8, 8000, 0, tail_dle_dbl, 3, 0, 40, 0, 0, 2, 2, tag++);
	tx_case(8, 8000, 0, tail_dle_cmd, 3, 0, 40, 0, 0, 2, 2, tag++);
	diff_eq_int("D998: the byte past the count reached the dispatcher",
		    last_ours.dle_etx, 1, tag);
	arm_dle_tail += 2;

	return diff_end();
}

/*
 * The six formats the switch knows, plus one it does not, each side of its
 * threshold.  The thresholds are one byte less than a block, so `fill` is
 * driven to exactly the threshold (short) and to one more (enough).
 */
static int
t_formats(void)
{
	static const struct {
		unsigned short bits, rate;
		int thresh;
	} fmts[] = {
		{ 8, 8000, 0x9f }, { 8, 7200, 0x8f }, { 8, 11025, 0xdb },
		{ 4, 8000, 0x4f }, { 4, 7200, 0x47 }, { 4, 11025, 0x6d }
	};
	unsigned int f;
	int o;
	long tag = 100;

	diff_begin("the six known formats, each side of its threshold");

	for (f = 0; f < sizeof fmts / sizeof fmts[0]; f++)
		for (o = 0; o < 4; o++) {
			/* Exactly at the threshold: not enough. */
			tx_case(fmts[f].bits, fmts[f].rate, o, plain, 0,
				fmts[f].thresh, 160, 0, 0, 2, 2, tag++);
			arm_short++;
			/* One byte more: a block comes out. */
			tx_case(fmts[f].bits, fmts[f].rate, o, plain, 0,
				fmts[f].thresh + 1, 160, 0, 0, 2, 2, tag++);
			arm_enough++;
			/* A full ring, which is where D996's overrun lives. */
			tx_case(fmts[f].bits, fmts[f].rate, o, plain, 0, 300,
				160, 0, 0, 2, 2, tag++);
			arm_enough++;
			if (fmts[f].rate == 11025)
				arm_overrun++;
		}

	/* A format the switch does not know: the default arm, always short. */
	tx_case(8, 9600, 0, plain, 0, 300, 160, 0, 0, 2, 2, tag++);
	tx_case(2, 8000, 1, plain, 0, 300, 160, 0, 0, 2, 2, tag++);
	arm_default_fmt += 2;

	return diff_end();
}

/*
 * D999: the short-data fill is `*countp` samples in the linear arm and a
 * fixed 160 floats in the other, so `*countp` is driven above and below 160
 * in both.
 */
static int
t_fill(void)
{
	static const unsigned short counts[] = { 0, 1, 8, 159, 160, 161, 200 };
	unsigned int c;
	int o;
	long tag = 400;

	diff_begin("the short-data zero fill, above and below 160");

	for (c = 0; c < sizeof counts / sizeof counts[0]; c++)
		for (o = 0; o < 3; o++) {
			tx_case(8, 8000, o, plain, 0, 0, counts[c], 0, 0, 2, 2,
				tag++);
			arm_short++;
			tx_case(8, 8000, o, plain, 0, 300, counts[c], 0, 0, 2,
				2, tag++);
			arm_enough++;
		}
	return diff_end();
}

/*
 * The underrun latch and the <DLE><ETX> suppression, which are the two gates
 * on the 13 that `voice_tx` returns when the ring is short.
 */
static int
t_underrun(void)
{
	long tag = 600;

	diff_begin("the underrun report, its latch and its ETX suppression");

	/* Fresh: reports, returns 13, sets the latch. */
	tx_case(8, 8000, 0, plain, 0, 0, 40, 0, 0, 2, 2, tag++);
	/* Latched: silent, returns 0. */
	tx_case(8, 8000, 0, plain, 0, 0, 40, 0, 1, 2, 2, tag++);
	/* ETX seen: silent even unlatched. */
	tx_case(8, 8000, 0, plain, 0, 0, 40, 1, 0, 2, 2, tag++);
	tx_case(8, 8000, 0, plain, 0, 0, 40, 1, 1, 2, 2, tag++);
	/* And a good block clears the latch. */
	tx_case(8, 8000, 0, plain, 0, 300, 40, 0, 1, 2, 2, tag++);
	arm_short += 4;
	arm_enough++;

	/* The mode comparison, both ways. */
	tx_case(8, 8000, 0, plain, 0, 300, 40, 0, 0, 2, 2, tag++);
	tx_case(8, 8000, 0, plain, 0, 300, 40, 0, 0, 2, 5, tag++);
	tx_case(8, 8000, 0, plain, 0, 300, 40, 0, 0, 0, 0, tag++);

	return diff_end();
}

static int
t_debug(void)
{
	static const unsigned int levels[] = { 0, 1, 2 };
	unsigned int l;

	diff_begin("voice_tx's four diagnostic lines at levels 0, 1 and 2");

	for (l = 0; l < 3; l++) {
		unsigned int lvl = levels[l];

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		tx_case(8, 8000, 0, dbl, 5, 0, 40, 0, 0, 2, 2,
			800 + (long)lvl);
		tx_case(8, 8000, 0, cmd_oth, 4, 0, 40, 0, 0, 2, 2,
			810 + (long)lvl);
		dsplib_debug_capture_on = 0;

		diff_eq_int("transcript line count at level %ld",
			    dsplib_debug_capture_lines(0),
			    dsplib_debug_capture_lines(1), (long)lvl);
		diff_eq_int("transcript text at level %ld",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
		if (lvl > 1) {
			const char *t = dsplib_debug_capture_text(1);

			diff_eq_int("the double-dle line is there",
				    strstr(t, "TX: double dle") != 0, 1,
				    (long)lvl);
			diff_eq_int("the command line is there",
				    strstr(t, "TX: shel comando dle") != 0, 1,
				    (long)lvl);
			diff_eq_int("the short-data line is there",
				    strstr(t, "** NO enouch data") != 0, 1,
				    (long)lvl);
			diff_eq_int("the underrun line is there",
				    strstr(t, "VOICE TX: not enough data to tx")
					!= 0, 1, (long)lvl);
		} else {
			diff_eq_int("nothing is printed below level 2",
				    dsplib_debug_capture_lines(1), 0,
				    (long)lvl);
		}
	}
	set_level(0);
	return diff_end();
}

static int
t_coverage(void)
{
	diff_begin("every arm was taken");
	diff_eq_int("cases run", n_cases > 100, 1, 0);
	diff_eq_int("blocks were produced", arm_enough > 0, 1, 0);
	diff_eq_int("short blocks happened", arm_short > 0, 1, 0);
	diff_eq_int("the underrun was reported", arm_report > 0, 1, 0);
	diff_eq_int("the latch suppressed a report", arm_latched > 0, 1, 0);
	diff_eq_int("ETX suppressed a report", arm_etx > 0, 1, 0);
	diff_eq_int("a doubled DLE was un-escaped", arm_dle_double > 0, 1, 0);
	diff_eq_int("a DLE command was dispatched", arm_dle_cmd > 0, 1, 0);
	diff_eq_int("D998's one-past read was driven", arm_dle_tail > 0, 1, 0);
	diff_eq_int("the unknown-format arm was taken", arm_default_fmt > 0, 1,
		    0);
	diff_eq_int("both output formats ran", arm_lin_out > 0 && arm_flt_out > 0,
		    1, 0);
	diff_eq_int("D996's 11025 overrun was driven", arm_overrun > 0, 1, 0);
	diff_eq_int("nothing was double-freed", (long)harness_alloc.bad_free, 0,
		    0);
	fprintf(stderr,
		"t_voicedptx: %ld cases, enough=%ld short=%ld report=%ld "
		"latched=%ld etx=%ld dbl=%ld cmd=%ld tail=%ld deflt=%ld "
		"lin=%ld flt=%ld overrun=%ld\n",
		n_cases, arm_enough, arm_short, arm_report, arm_latched,
		arm_etx, arm_dle_double, arm_dle_cmd, arm_dle_tail,
		arm_default_fmt, arm_lin_out, arm_flt_out, arm_overrun);
	return diff_end();
}

int
main(void)
{
	int failed = 0;

	failed |= t_scan();
	failed |= t_formats();
	failed |= t_fill();
	failed |= t_underrun();
	failed |= t_debug();
	failed |= t_coverage();
	return failed;
}
