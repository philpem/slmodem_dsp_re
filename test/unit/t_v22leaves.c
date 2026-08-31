/*
 * t_v22leaves.c -- differential test of the four V.22 control/status leaves
 * that reach the datapump from outside:
 *
 *   V22FP_control         .text 0x08c3b0
 *   ScramblerOn           .text 0x08e670
 *   DescramblerOn         .text 0x08e680
 *   V22FP_GetDiagnostics  .text 0x088480
 *
 * None of them is reached by anything inside the object (finding F8320's
 * exported-API bucket), so the harness fabricates the object rather than
 * building one: a modem block whose +0x50 and +0x54 point at an hdx and a
 * dsp buffer, everything random-filled and mirrored, and the writes the
 * functions make are compared byte for byte over the WHOLE of all three
 * buffers plus guard bands -- so a stray store lands in the diff, not past
 * it.  The two embedded pointers are skipped by offset; they hold two
 * different addresses by construction (the loop idiom CLAUDE.md licenses
 * for exactly this case).
 *
 * V22FP_control's first flag byte is swept exhaustively (256 values); the
 * second is swept exhaustively against three fixed first bytes, so both of
 * its live encodings (bit 2, and bits 7:6 == 10) fire with and without every
 * bit of the first byte.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v22prc.h"

extern int ref_V22FP_control(void *modem, const unsigned char *ctl);
extern int ref_ScramblerOn(void *modem);
extern int ref_DescramblerOn(void *modem);
extern int ref_V22FP_GetDiagnostics(void *modem);

/*
 * Ours is declared on `struct v22fp *`; this test builds the object as raw
 * bytes, so it declares the symbol the way the linker sees it.  cdecl makes
 * the mismatch harmless and the differential decides the behaviour.
 */
extern int V22FP_GetDiagnostics(void *modem);
/*
 * Declared here rather than taken from a header: these three now live in
 * `v22ctl.c` typed against `struct v22fp`, and this test drives them through
 * the object pointer it already holds.  Both spellings pass the same pointer.
 */
extern int V22FP_control(void *modem, const unsigned char *ctl);
extern int ScramblerOn(void *modem);
extern int DescramblerOn(void *modem);

#define MODEM_BYTES	0x60
#define HDX_BYTES	0x40
#define DSP_BYTES	0x1f8
#define GUARD		16

struct box {
	unsigned char modem[MODEM_BYTES];
	unsigned char g0[GUARD];
	unsigned char hdx[HDX_BYTES];
	unsigned char g1[GUARD];
	unsigned char dsp[DSP_BYTES];
	unsigned char g2[GUARD];
};

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

static void
wire(struct box *x)
{
	unsigned char *hdx = x->hdx;
	unsigned char *dsp = x->dsp;

	memcpy(x->modem + V22_OBJ_GTIMER, &hdx, sizeof(hdx));
	memcpy(x->modem + V22_OBJ_FP, &dsp, sizeof(dsp));
}

/* Random-fill one box, mirror it into the other, wire both. */
static void
fresh(struct box *a, struct box *b)
{
	fill(a, (unsigned)sizeof(*a));
	memcpy(b, a, sizeof(*a));
	wire(a);
	wire(b);
}

/*
 * First differing offset, or -1 -- one check per buffer per call, and a
 * failure names the offset, which is what `whichfield` wants anyway.
 */
static long
first_diff(const unsigned char *a, const unsigned char *b, unsigned n,
	   unsigned skip_from, unsigned skip_to)
{
	unsigned i;

	for (i = 0; i < n; i++) {
		if (i >= skip_from && i < skip_to)
			continue;
		if (a[i] != b[i])
			return (long)i;
	}
	return -1;
}

static void
compare(struct box *a, struct box *b, long tag)
{
	/* the two pointers at +0x50 / +0x54 differ by design */
	diff_eq_int("modem first differing byte (input %ld)",
		    first_diff(a->modem, b->modem, MODEM_BYTES,
			       V22_OBJ_GTIMER, V22_OBJ_FP + 4), -1, tag);
	diff_eq_int("hdx first differing byte (input %ld)",
		    first_diff(a->hdx, b->hdx, HDX_BYTES, 1, 0), -1, tag);
	diff_eq_int("dsp first differing byte (input %ld)",
		    first_diff(a->dsp, b->dsp, DSP_BYTES, 1, 0), -1, tag);
	diff_eq_int("guards intact, input %ld",
		    memcmp(a->g0, b->g0, GUARD) | memcmp(a->g1, b->g1, GUARD)
		    | memcmp(a->g2, b->g2, GUARD), 0, tag);
}

static void
one_control(struct box *a, struct box *b, unsigned c, unsigned d)
{
	unsigned char ctl[16];
	int ra, rb;
	long tag = (long)((c << 8) | d);

	fresh(a, b);
	fill(ctl, (unsigned)sizeof(ctl));
	ctl[V22CTL_FLAGS] = (unsigned char)c;
	ctl[V22CTL_MODE] = (unsigned char)d;

	ra = ref_V22FP_control(a->modem, ctl);
	rb = V22FP_control(b->modem, ctl);
	diff_eq_int("V22FP_control return, ctl 0x%04lx", rb, ra, tag);
	compare(a, b, tag);
}

int
main(void)
{
	static struct box a, b;
	int rc = 0;
	unsigned c, d;

	/* The eight combinations of MODE's live bits, plus both dead-bit
	 * extremes. */
	static const unsigned mode_probe[] = {
		0x00, 0x04, 0x40, 0x44, 0x80, 0x84, 0xc0, 0xc4, 0x3b, 0xff
	};

	diff_begin("V22FP_control");
	for (c = 0; c < 256; c++)
		for (d = 0; d < sizeof(mode_probe) / sizeof(mode_probe[0]);
		     d++)
			one_control(&a, &b, c, mode_probe[d]);
	for (d = 0; d < 256; d++) {
		one_control(&a, &b, 0x00, d);
		one_control(&a, &b, 0xff, d);
		one_control(&a, &b, 0xa5, d);
	}
	rc |= diff_end();

	diff_begin("ScramblerOn / DescramblerOn / V22FP_GetDiagnostics");
	for (c = 0; c < 64; c++) {
		fresh(&a, &b);
		diff_eq_int("ScramblerOn (%ld)",
			    ScramblerOn(b.modem), ref_ScramblerOn(a.modem),
			    (long)c);
		diff_eq_int("DescramblerOn (%ld)",
			    DescramblerOn(b.modem),
			    ref_DescramblerOn(a.modem), (long)c);
		diff_eq_int("V22FP_GetDiagnostics (%ld)",
			    V22FP_GetDiagnostics(b.modem),
			    ref_V22FP_GetDiagnostics(a.modem), (long)c);
		compare(&a, &b, (long)c);
	}
	rc |= diff_end();

	return rc;
}
