/*
 * t_v22ctl.c -- differential test of V.22's four exported accessors.
 *
 * `V22FP_control` is swept over ITS WHOLE INPUT DOMAIN.  Both of the bytes it
 * reads are eight bits wide and it reads nothing else, so 65,536 calls is the
 * entire space and there is no sampling argument to make: every flag, every
 * value of the two-bit field, and every combination of the two, including the
 * one where the flag and the field both fire and the second overwrites the
 * first.  The two graphs are built once per configuration and driven through
 * the whole sweep in step, which also means the sweep tests the ONE piece of
 * state that carries between calls -- the half-duplex pair is written only
 * conditionally, so a control byte that selects neither leaves whatever the
 * previous one put there.
 *
 * ScramblerOn and DescramblerOn are two-instruction readers, and what can be
 * wrong about them is WHICH field, which no input can expose on its own: both
 * fields hold the same value in a freshly built object.  So the sweep pokes
 * the two to DIFFERENT values before every call, and the two readings are
 * separated by construction.
 *
 * V22FP_GetDiagnostics IS BARELY TESTED AND THIS SAYS SO.  Its callee
 * `V22_FSE_getdiag` is a three-instruction stub that returns 0 and reads
 * nothing (src/pump/v22/v22_fse.c), so the +0x164 offset this function
 * computes reaches an argument nobody looks at.  The check below is that both
 * sides return the same constant; the offset is covered by NOTHING, and it is
 * the same shape as findings F860-862's `loadParams`.  Recorded rather than
 * dressed up.
 */

#include <string.h>

#include "harness.h"

#include "dsplib/fpm_agc.h"
#include "dsplib/fpm_mtd.h"
#include "dsplib/fpm_smc.h"
#include "dsplib/fpm_tone.h"
#include "dsplib/v22_fse.h"
#include "dsplib/v22_mrf.h"
#include "dsplib/v22_pps.h"
#include "dsplib/v22_sre.h"
#include "dsplib/v22ctl.h"
#include "dsplib/v22fp.h"

extern int ref_V22FP_control(struct v22fp *fp, const struct v22fp_ctl *ctl);
extern int ref_V22FP_GetDiagnostics(struct v22fp *fp);
extern int ref_ScramblerOn(struct v22fp *fp);
extern int ref_DescramblerOn(struct v22fp *fp);

/* ------------------------------------------------------------------------ */

/* Copies with every pointer blanked, so the rest can be compared entire. */
static struct v22fp
blank_obj(const struct v22fp *fp)
{
	struct v22fp c = *fp;

	c.out_i = NULL;
	c.out_q = NULL;
	c.n_out = NULL;
	c.icoeff = NULL;
	c.qcoeff = NULL;
	c.hdx = NULL;
	c.dsp = NULL;
	return c;
}

static struct v22fp_hdx
blank_hdx(const struct v22fp_hdx *h)
{
	struct v22fp_hdx c = *h;

	c.tone = NULL;
	c.mtd = NULL;
	c.mtd_s1 = NULL;
	c.mtd2 = NULL;
	c.iir = NULL;
	return c;
}

static struct v22fp_dsp
blank_dsp(const struct v22fp_dsp *d)
{
	struct v22fp_dsp c = *d;

	c.ra8 = NULL;
	c.rx_scratch = NULL;
	c.pps_coff_i = NULL;
	c.pps_coff_q = NULL;
	c.mrf_coeff = NULL;
	c.fse_coff_i = NULL;
	c.fse_coff_q = NULL;
	c.pps.cfg.coeff_i = NULL;
	c.pps.cfg.coeff_q = NULL;
	c.pps.hist_i = NULL;
	c.pps.hist_q = NULL;
	c.pps.imap = NULL;
	c.pps.qmap = NULL;
	c.smc.cfg.pmap = NULL;
	c.smc.cfg.imap = NULL;
	c.smc.cfg.qmap = NULL;
	c.agc.cfg.alpha = NULL;
	c.agc.cfg.beta = NULL;
	c.agc2.cfg.alpha = NULL;
	c.agc2.cfg.beta = NULL;
	c.mrf.cfg.coeff = NULL;
	c.mrf.history = NULL;
	c.sre.coeff = NULL;
	c.sre.hist = NULL;
	c.sre.clk = NULL;
	c.fse.icoff = NULL;
	c.fse.qcoff = NULL;
	c.fse.out_i = NULL;
	c.fse.out_q = NULL;
	c.fse.icoeff = NULL;
	c.fse.qcoeff = NULL;
	c.fse.hist = NULL;
	c.fse.r44 = NULL;
	c.fse.r48 = NULL;
	c.fse.prev_quad = NULL;
	c.fse.decision = NULL;
	return c;
}

static void
compare_graphs(struct v22fp *mine, struct v22fp *theirs, long tag)
{
	struct v22fp oa = blank_obj(mine);
	struct v22fp ob = blank_obj(theirs);
	struct v22fp_hdx ha = blank_hdx(mine->hdx);
	struct v22fp_hdx hb = blank_hdx(theirs->hdx);
	struct v22fp_dsp da = blank_dsp(mine->dsp);
	struct v22fp_dsp db = blank_dsp(theirs->dsp);

	diff_eq_obj("object", struct v22fp, &oa, &ob, tag);
	diff_eq_obj("hdx", struct v22fp_hdx, &ha, &hb, tag);
	diff_eq_obj("dsp", struct v22fp_dsp, &da, &db, tag);
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

static long saw_hdx_six, saw_hdx_four, saw_hdx_both, saw_hdx_neither;
static long saw_scrambler_on, saw_scrambler_off;
static long saw_descrambler_on, saw_descrambler_off;
static long saw_freeze, saw_thaw;
static long saw_bit9_set, saw_bit9_clear;

static int
run_control(void)
{
	long tag = 0;
	int mode, f14;
	unsigned b0c, b0d;

	diff_begin("V22FP_control, the whole 16-bit input domain");

	for (mode = 0; mode <= 1; mode++)
		for (f14 = 0; f14 <= 1; f14++) {
			struct v22fp_cfg cfg = base_cfg(mode, f14);
			struct v22fp *a = V22FP_create(0, &cfg);
			struct v22fp *b = V22FP_create(0, &cfg);

			for (b0d = 0; b0d < 256; b0d++)
				for (b0c = 0; b0c < 256; b0c++) {
					struct v22fp_ctl ctl;
					int ra, rb;
					int six, four;

					memset(&ctl, 0, sizeof(ctl));
					ctl.flags_0c = (unsigned char)b0c;
					ctl.flags_0d = (unsigned char)b0d;

					ra = ref_V22FP_control(a, &ctl);
					rb = V22FP_control(b, &ctl);

					diff_eq_int("return, case %ld", rb, ra,
						    tag);
					compare_graphs(b, a, tag);

					six = (b0d & V22_CTL_RETRAIN) != 0;
					four = (b0d >> V22_CTL_HDX_SHIFT)
					       == V22_CTL_HDX_ORG_RMLOOP2;
					if (six && four)
						saw_hdx_both++;
					else if (six)
						saw_hdx_six++;
					else if (four)
						saw_hdx_four++;
					else
						saw_hdx_neither++;

					if (b0c & V22_CTL_SCRAMBLER)
						saw_scrambler_on++;
					else
						saw_scrambler_off++;
					if (b0c & V22_CTL_DESCRAMBLER)
						saw_descrambler_on++;
					else
						saw_descrambler_off++;
					if (b0c & V22_CTL_FREEZE_ADAPT)
						saw_freeze++;
					else
						saw_thaw++;
					if (b0c & V22_CTL_PARAM_BIT9)
						saw_bit9_set++;
					else
						saw_bit9_clear++;
					tag++;
				}

			V22FP_delete(a);
			V22FP_delete(b);
		}

	return diff_end();
}

/* ------------------------------------------------------------------------ */

/*
 * The two readers.  Poked to DIFFERENT values on every trial, because a
 * freshly built object holds the same value in both and no input could then
 * separate "reads r18" from "reads r1c".
 */
static int
run_readers(void)
{
	static const int values[] = {
		0, 1, -1, 2, 0x7fffffff, -0x7fffffff - 1, 0x12345678, 0x40
	};
	struct v22fp_cfg cfg = base_cfg(0, 0);
	struct v22fp *fp = V22FP_create(0, &cfg);
	unsigned i, j;
	long tag = 0;

	diff_begin("ScramblerOn / DescramblerOn");

	for (i = 0; i < sizeof(values) / sizeof(values[0]); i++)
		for (j = 0; j < sizeof(values) / sizeof(values[0]); j++) {
			fp->dsp->r18 = values[i];
			fp->dsp->r1c = values[j];

			diff_eq_int("ScramblerOn, case %ld", ScramblerOn(fp),
				    ref_ScramblerOn(fp), tag);
			diff_eq_int("DescramblerOn, case %ld",
				    DescramblerOn(fp), ref_DescramblerOn(fp),
				    tag);
			tag++;
		}

	V22FP_delete(fp);
	return diff_end();
}

/*
 * The diagnostic word.  See the header note: its callee is a stub, so this
 * check is that the two sides agree on a constant and NOT that the offset is
 * right.  It is kept because a reconstruction that faulted, or that returned
 * something else, would still be caught.
 */
static int
run_diagnostics(void)
{
	struct v22fp_cfg cfg = base_cfg(0, 0);
	struct v22fp *fp = V22FP_create(0, &cfg);
	long tag;

	diff_begin("V22FP_GetDiagnostics");
	for (tag = 0; tag < 4; tag++) {
		fp->dsp->fse.mse = (short)(tag * 1000);
		fp->dsp->fse.err_avg = (short)(-tag);
		diff_eq_int("case %ld", V22FP_GetDiagnostics(fp),
			    ref_V22FP_GetDiagnostics(fp), tag);
	}
	V22FP_delete(fp);
	return diff_end();
}

/* ------------------------------------------------------------------------ */

int
main(void)
{
	int rc = 0;

	rc |= run_control();
	rc |= run_readers();
	rc |= run_diagnostics();

	diff_begin("v22ctl coverage guards");
	diff_eq_int("hdx flag alone (%ld)", saw_hdx_six > 0, 1, 0);
	diff_eq_int("hdx field alone (%ld)", saw_hdx_four > 0, 1, 0);
	diff_eq_int("hdx flag and field together (%ld)", saw_hdx_both > 0, 1,
		    0);
	diff_eq_int("hdx neither (%ld)", saw_hdx_neither > 0, 1, 0);
	diff_eq_int("scrambler enabled (%ld)", saw_scrambler_on > 0, 1, 0);
	diff_eq_int("scrambler disabled (%ld)", saw_scrambler_off > 0, 1, 0);
	diff_eq_int("descrambler enabled (%ld)", saw_descrambler_on > 0, 1, 0);
	diff_eq_int("descrambler disabled (%ld)", saw_descrambler_off > 0, 1,
		    0);
	diff_eq_int("adaptation frozen (%ld)", saw_freeze > 0, 1, 0);
	diff_eq_int("adaptation thawed (%ld)", saw_thaw > 0, 1, 0);
	diff_eq_int("params bit 9 set (%ld)", saw_bit9_set > 0, 1, 0);
	diff_eq_int("params bit 9 cleared (%ld)", saw_bit9_clear > 0, 1, 0);
	rc |= diff_end();

	return rc;
}
