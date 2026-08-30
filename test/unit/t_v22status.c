/*
 * t_v22status.c -- differential test of V22_status.
 *
 * ONE GRAPH, TWO REPORTS.  `V22_status` reads the datapump and writes nothing
 * back to it, so unlike the rest of the V.22 tests this one does not need two
 * graphs at all: both sides are driven against the same object and what is
 * compared is the two report blocks, byte for byte, plus the return.  That is
 * strictly stronger than comparing two graphs would be, because it removes
 * the two-different-addresses problem entirely.
 *
 * WHAT THE SWEEP HAS TO REACH, and why each dimension is here:
 *
 *   - `hdx->r0e` indexes PROTOCOL with no bounds check, so all seven in-range
 *     values are driven.  Out of range is NOT driven: the object reads past
 *     the table and there is nothing to be differentially identical to.
 *   - `fse.mse` is subtracted from 2048 with no clamp, so the values include
 *     both signs, the 6478 boundary where the 1200 report goes negative, and
 *     both sixteen-bit extremes.  It is SCALED on the 1200 arm and not on the
 *     2400 arm (finding F8528), so `dsp->r2a` is driven both ways on every
 *     mse -- which is what caught that in the first place.
 *   - the six enable words each contribute one bit of the flag byte, three of
 *     them inverted.  A wrong pairing -- r18 with bit 1, or r00 with bit 4 --
 *     is invisible unless the six hold DIFFERENT values on the same trial, so
 *     they are driven as a six-bit counter over the whole 64 combinations
 *     rather than set together.
 *   - the report's own +0x14 and +0x15 are poisoned before every call, in two
 *     patterns, because the object preserves the caller's upper seven bits at
 *     +0x15 and preserves nothing at +0x14.  A reconstruction that assigned
 *     +0x15 outright passes every check that starts from a zeroed block.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/v22_fse.h"
#include "dsplib/v22fp.h"
#include "dsplib/v22status.h"

extern int ref_V22_status(struct v22fp *fp, struct v22_status *st);

/* ------------------------------------------------------------------------ */

static void
cmp_raw(const char *what, const void *a, const void *b, unsigned long n,
	long input)
{
	const unsigned char *pa = (const unsigned char *)a;
	const unsigned char *pb = (const unsigned char *)b;
	unsigned long i;

	for (i = 0; i < n; i++)
		if (pa[i] != pb[i])
			break;
	diff_eq_int(what, i == n ? -1L : (long)i, -1L, input);
}

static struct v22fp_cfg
base_cfg(int mode, int f14)
{
	struct v22fp_cfg c;

	c.mode = mode;
	c.rate = 0;
	c.f08 = 60000;
	c.f0c = 0;
	c.f10 = 700;
	c.f14 = f14;
	c.f18 = 1;
	return c;
}

/* ------------------------------------------------------------------------ */

static long saw_quality_negative, saw_quality_positive;
static long saw_flags2_preserved;
static long saw_rx_1200, saw_rx_2400;
static long protocol_seen[7];

int
main(void)
{
	static const short mses[] = {
		0, 1, -1, 100, 6477, 6478, 6479, 32767, -32768, -6478
	};
	static const short rates[] = { 0, 1, -1, 2 };
	struct v22fp_cfg cfg = base_cfg(0, 0);
	struct v22fp *fp = V22FP_create(0, &cfg);
	long tag = 0;
	int rc = 0;
	int r0e, mi, enables, poison, ri, fl;

	diff_begin("V22_status");

	for (r0e = 0; r0e < 7; r0e++)
		for (mi = 0; mi < (int)(sizeof(mses) / sizeof(mses[0])); mi++)
			for (enables = 0; enables < 64; enables++)
				for (poison = 0; poison < 2; poison++) {
					struct v22_status sa, sb;
					int ra, rb;

					ri = (r0e + mi + enables) % 4;
					fl = (mi + enables) & 3;

					fp->hdx->r0e = (short)r0e;
					fp->dsp->fse.mse = mses[mi];
					fp->dsp->r28 = rates[ri];
					fp->dsp->r2a = rates[(ri + 1) & 3];

					/*
					 * Six independent bits: a wrong
					 * pairing shows only when the six
					 * disagree.
					 */
					fp->dsp->r18 = (enables >> 0) & 1;
					fp->dsp->r1c = (enables >> 1) & 1;
					fp->dsp->r20 = (enables >> 2) & 1;
					fp->dsp->r00 = (enables >> 3) & 1;
					fp->dsp->r0c = (enables >> 4) & 1;
					fp->dsp->eq_adapt = (enables >> 5) & 1;

					fp->params.flags =
						(fp->params.flags
						 & ~(V22_PARAMS_BIT9
						     | V22_PARAMS_BIT10))
						| ((fl & 1) ? V22_PARAMS_BIT9
							    : 0u)
						| ((fl & 2) ? V22_PARAMS_BIT10
							    : 0u);

					memset(&sa, poison ? 0xff : 0x00,
					       sizeof(sa));
					memset(&sb, poison ? 0xff : 0x00,
					       sizeof(sb));

					ra = ref_V22_status(fp, &sa);
					rb = V22_status(fp, &sb);

					diff_eq_int("return, case %ld", rb, ra,
						    tag);
					cmp_raw("report, case %ld", &sb, &sa,
						sizeof(sa), tag);

					if (sa.quality < 0)
						saw_quality_negative++;
					else
						saw_quality_positive++;
					if (sa.rx_bps == V22_STATUS_BPS_2400)
						saw_rx_2400++;
					else
						saw_rx_1200++;
					if (poison && (sa.flags2 & 0xfe) != 0)
						saw_flags2_preserved++;
					protocol_seen[r0e]++;
					tag++;
				}

	rc |= diff_end();
	V22FP_delete(fp);

	diff_begin("v22status coverage guards");
	for (r0e = 0; r0e < 7; r0e++)
		diff_eq_int("PROTOCOL index %ld driven",
			    protocol_seen[r0e] > 0, 1, r0e);
	diff_eq_int("quality went negative (%ld)", saw_quality_negative > 0, 1,
		    0);
	diff_eq_int("quality stayed positive (%ld)", saw_quality_positive > 0,
		    1, 0);
	/*
	 * The one property a zero-filled report can never show: the object
	 * preserves the caller's upper seven bits at +0x15.
	 */
	diff_eq_int("flags2's upper bits survived (%ld)",
		    saw_flags2_preserved > 0, 1, 0);
	/*
	 * F8528's asymmetry: the Q14 scale is applied on the 1200 arm only, so
	 * a sweep that took one arm would agree with a reconstruction that
	 * scaled both or neither.
	 */
	diff_eq_int("rx 1200 arm taken (%ld)", saw_rx_1200 > 0, 1, 0);
	diff_eq_int("rx 2400 arm taken (%ld)", saw_rx_2400 > 0, 1, 0);
	rc |= diff_end();

	return rc;
}
