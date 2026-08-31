/*
 * t_class1handlers.c -- differential test of the Class 1 host-link handlers:
 * `_hdlc_receive_state_init`, `_handle_data_input`, `_handle_hdlc_input` and
 * `_handle_data_output`.
 *
 * EVERY DESTINATION IS SIZED FROM WHAT THE FUNCTION CAN WRITE, NOT FROM ITS
 * INPUT (F8607/D956).  `_handle_data_input` appends twenty zero elements
 * after DLE ETX, so its destination is `IN_MAX + 20 + GUARD`;
 * `_handle_hdlc_input` writes from the SESSION's cursor, so its destination is
 * `CURSOR_MAX + IN_MAX + GUARD`; `_handle_data_output` doubles every DLE and
 * may append two more, so its destination is `2 * IN_MAX + 2 + GUARD`.  Each
 * guard region is compared, so an overrun on either side is a failure rather
 * than a silent memory corruption.
 *
 * EVERY FIELD USED AS A SUBSCRIPT IS PLANTED (F8587/D955).  The only one is
 * `f1250`, `_handle_hdlc_input`'s write cursor, which the object takes
 * straight out of the session and indexes `dst` with -- a random fill would
 * make both sides write to the same address megabytes away and agree.
 *
 * THE ARM COUNTERS COME FROM THE RUN (F134).  They are incremented from what
 * the REFERENCE returned and left behind -- the escape flag it set, the
 * closure it latched, the alignment it locked -- so a case table that stopped
 * reaching an arm fails a denominator instead of passing quietly.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/class1.h"
#include "dsplib/class1tx.h"
#include "dsplib/debug.h"

extern unsigned int ref_dsplibs_debug_level;

extern int ref__hdlc_receive_state_init(void *ctx);
extern int ref__handle_data_input(void *ctx, const unsigned char *src,
				  unsigned short *dst, int *count);
extern int ref__handle_hdlc_input(void *ctx, const unsigned char *src,
				  unsigned short *dst, int *count);
extern int ref__handle_data_output(void *ctx, const unsigned short *src,
				   unsigned char *dst, int count,
				   int terminate);

#define IN_MAX		64
#define CURSOR_MAX	8
#define GUARD		16

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

static struct fax_class1 ctx_a, ctx_b;

static void
ctx_plant(void)
{
	fill(&ctx_a, (unsigned)sizeof(ctx_a));
	memcpy(&ctx_b, &ctx_a, sizeof(ctx_a));
}

static void
ctx_compare(const char *what, long tag)
{
	char buf[96];

	snprintf(buf, sizeof(buf), "%s: ctx (%%ld)", what);
	diff_eq_int(buf, memcmp(&ctx_a, &ctx_b, sizeof(ctx_a)), 0, tag);
}

/* ------------------------------------------------------------------ */

static int
run_state_init(void)
{
	unsigned i, level;

	diff_begin("_hdlc_receive_state_init");
	for (level = 0; level < 3; level++) {
		for (i = 0; i < 8; i++) {
			int ra, rb;

			ctx_plant();
			ctx_a.clock_sec = ctx_b.clock_sec = (int)(i * 13);
			ctx_a.clock_frac = ctx_b.clock_frac = (int)(i * 7);
			dsplibs_debug_level = ref_dsplibs_debug_level = level;

			ra = ref__hdlc_receive_state_init(&ctx_a);
			rb = _hdlc_receive_state_init(&ctx_b);
			diff_eq_int("state_init return (%ld)", rb, ra,
				    (long)(level * 100 + i));
			ctx_compare("state_init", (long)(level * 100 + i));
		}
	}
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	return diff_end();
}

/*
 * Build a byte stream that is guaranteed to reach every arm: literal bytes,
 * a bare DLE (escape), DLE DLE (a literal 0x10), a DLE followed by something
 * that is neither (the dropped byte), and, on some cases, DLE ETX.
 */
static int
build_stream(unsigned char *src, unsigned pattern, int want_etx)
{
	int n = 0;

	src[n++] = (unsigned char)(0x41 + (pattern & 7));
	src[n++] = CLASS1_DLE;
	src[n++] = CLASS1_DLE;			/* -> one literal 0x10 */
	src[n++] = (unsigned char)(rnd() & 0xff);
	if (pattern & 1) {
		src[n++] = CLASS1_DLE;
		src[n++] = 0x55;		/* neither DLE nor ETX */
	}
	if ((pattern & 2) && !want_etx)
		src[n++] = CLASS1_DLE;		/* a trailing bare DLE */
	if (want_etx) {
		src[n++] = CLASS1_DLE;
		src[n++] = CLASS1_ETX;
		src[n++] = 0x7e;		/* must not be consumed */
	}
	while (n < (int)(4 + (pattern % 9)))
		src[n++] = (unsigned char)(rnd() & 0xff);
	return n;
}

static int
run_data_input(void)
{
	static unsigned char src[IN_MAX];
	static unsigned short dst_a[IN_MAX + 20 + GUARD];
	static unsigned short dst_b[IN_MAX + 20 + GUARD];
	unsigned p, level;
	long etx_seen = 0, escaped_out = 0, closed_early = 0;

	diff_begin("_handle_data_input");
	for (level = 0; level < 4; level++) {
		for (p = 0; p < 24; p++) {
			int want_etx = (p % 3) == 0;
			int n = build_stream(src, p, want_etx);
			int ca, cb, ra, rb, was_closed;
			long tag = (long)(level * 100 + p);

			ctx_plant();
			ctx_a.dle_seen = ctx_b.dle_seen = (int)(p & 1);
			was_closed = ((p % 7) == 6);
			ctx_a.data_input_closed = ctx_b.data_input_closed =
			    was_closed;
			memset(dst_a, 0x5a, sizeof(dst_a));
			memcpy(dst_b, dst_a, sizeof(dst_a));
			ca = cb = n;
			dsplibs_debug_level = ref_dsplibs_debug_level = level;

			ra = ref__handle_data_input(&ctx_a, src, dst_a, &ca);
			rb = _handle_data_input(&ctx_b, src, dst_b, &cb);

			diff_eq_int("data_input return (%ld)", rb, ra, tag);
			diff_eq_int("data_input count (%ld)", (long)cb,
				    (long)ca, tag);
			diff_eq_int("data_input dst and guard (%ld)",
				    memcmp(dst_a, dst_b, sizeof(dst_a)), 0,
				    tag);
			ctx_compare("data_input", tag);

			if (was_closed) {
				/* the early return: nothing written, and
				 * the reference said so */
				if (ca == 0 && dst_a[0] == 0x5a5a)
					closed_early++;
			} else if (ctx_a.data_input_closed)
				etx_seen++;
			if (ctx_a.dle_seen)
				escaped_out++;
		}
	}
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	diff_eq_int("data_input: DLE ETX was reached (%ld)", etx_seen > 0, 1,
		    etx_seen);
	diff_eq_int("data_input: a call left the escape armed (%ld)",
		    escaped_out > 0, 1, escaped_out);
	diff_eq_int("data_input: the already-closed early return ran (%ld)",
		    closed_early > 0, 1, closed_early);
	return diff_end();
}

static int
run_hdlc_input(void)
{
	static unsigned char src[IN_MAX];
	static unsigned short dst_a[CURSOR_MAX + IN_MAX + GUARD];
	static unsigned short dst_b[CURSOR_MAX + IN_MAX + GUARD];
	unsigned p;
	long frames = 0, partials = 0, latched = 0, unlatched = 0;

	diff_begin("_handle_hdlc_input");
	for (p = 0; p < 48; p++) {
		int want_etx = (p % 3) == 0;
		int n = build_stream(src, p, want_etx);
		int cursor = (int)(p % CURSOR_MAX);
		int ca, cb, ra, rb;
		long tag = (long)p;

		ctx_plant();
		ctx_a.dle_seen = ctx_b.dle_seen = (int)(p & 1);
		ctx_a.f1250 = ctx_b.f1250 = cursor;
		ctx_a.flags004 = ctx_b.flags004 = (unsigned char)
		    ((p & 4) ? CLASS1_FLAG_FRAME_END_LATCH : 0);
		memset(dst_a, 0x5a, sizeof(dst_a));
		memcpy(dst_b, dst_a, sizeof(dst_a));
		ca = cb = n;

		ra = ref__handle_hdlc_input(&ctx_a, src, dst_a, &ca);
		rb = _handle_hdlc_input(&ctx_b, src, dst_b, &cb);

		diff_eq_int("hdlc_input return (%ld)", rb, ra, tag);
		diff_eq_int("hdlc_input count (%ld)", (long)cb, (long)ca, tag);
		diff_eq_int("hdlc_input dst and guard (%ld)",
			    memcmp(dst_a, dst_b, sizeof(dst_a)), 0, tag);
		ctx_compare("hdlc_input", tag);

		if (ra == 1) {
			frames++;
			if (ctx_a.flags004 & CLASS1_FLAG_FRAME_END_LATCH)
				latched++;
			else
				unlatched++;
		} else
			partials++;
	}
	diff_eq_int("hdlc_input: a frame completed (%ld)", frames > 0, 1,
		    frames);
	diff_eq_int("hdlc_input: a call did not complete one (%ld)",
		    partials > 0, 1, partials);
	diff_eq_int("hdlc_input: the end-of-frame latch fired (%ld)",
		    latched > 0, 1, latched);
	diff_eq_int("hdlc_input: and did not, with the flag clear (%ld)",
		    unlatched > 0, 1, unlatched);
	return diff_end();
}

static int
run_data_output(void)
{
	static unsigned short src[IN_MAX];
	static unsigned char dst_a[2 * IN_MAX + 2 + GUARD];
	static unsigned char dst_b[2 * IN_MAX + 2 + GUARD];
	unsigned p, w;
	long locked = 0, gave_up = 0, doubled = 0, terminated = 0;

	diff_begin("_handle_data_output");
	for (w = 0; w < 12; w++) {
		for (p = 0; p < 16; p++) {
			int count = (w == 0) ? 1
					     : (int)(p % (IN_MAX / 2)) + 1;
			int terminate = (p & 4) != 0;
			int locked_in = (p & 8) != 0;
			unsigned int window;
			int ra, rb, i;
			long tag = (long)(w * 100 + p);

			/*
			 * w picks the byte the search walks over: 0xff in
			 * bits 16..23 makes the search exhaust, anything
			 * with a zero bit makes it lock at that bit.
			 */
			window = ((w == 0 ? 0xffu : (0xffu >> (w & 7)))
				  << 16) | (unsigned int)(rnd() & 0xffff);

			ctx_plant();
			ctx_a.async_locked = ctx_b.async_locked = locked_in;
			ctx_a.async_window = ctx_b.async_window = window;
			ctx_a.async_shift = ctx_b.async_shift = 16;
			ctx_a.async_mask = ctx_b.async_mask = 0xff0000;

			for (i = 0; i < IN_MAX; i++)
				src[i] = (unsigned short)(rnd() & 0xffff);
			/* force a recovered DLE somewhere */
			src[0] = 0x10;
			memset(dst_a, 0x5a, sizeof(dst_a));
			memcpy(dst_b, dst_a, sizeof(dst_a));

			ra = ref__handle_data_output(&ctx_a, src, dst_a, count,
						     terminate);
			rb = _handle_data_output(&ctx_b, src, dst_b, count,
						 terminate);

			diff_eq_int("data_output return (%ld)", rb, ra, tag);
			diff_eq_int("data_output dst and guard (%ld)",
				    memcmp(dst_a, dst_b, sizeof(dst_a)), 0,
				    tag);
			ctx_compare("data_output", tag);

			if (ra > count)
				doubled++;
			if (terminate)
				terminated++;
			if (!locked_in && ctx_a.async_locked)
				locked++;
			if (!locked_in && !ctx_a.async_locked)
				gave_up++;
		}
	}
	diff_eq_int("data_output: the search locked (%ld)", locked > 0, 1,
		    locked);
	diff_eq_int("data_output: the search gave up (%ld)", gave_up > 0, 1,
		    gave_up);
	diff_eq_int("data_output: a DLE was doubled (%ld)", doubled > 0, 1,
		    doubled);
	diff_eq_int("data_output: the terminator was appended (%ld)",
		    terminated > 0, 1, terminated);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_state_init();
	rc |= run_data_input();
	rc |= run_hdlc_input();
	rc |= run_data_output();
	return rc;
}
