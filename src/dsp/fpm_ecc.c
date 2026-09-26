/*
 * fpm_ecc.c -- Fixed Point Modem: Echo Canceller.
 *
 * Reconstructed from dsplibs.o:
 *   FPM_ECC_cancel  .text 0x0a6e00
 *   FPM_ECC_init    .text 0x0a7610
 *   FPM_ECC_free    .text 0x0a7870
 *
 * See dsplib/fpm_ecc.h for the shape of the thing.  In brief: the transmitted
 * symbols go into `line`, two taps `far_lag` apart replay them into two short
 * complex histories, and each history is convolved against its own adaptive
 * coefficients to build the echo estimate that is subtracted from the input.
 */

#include "dsplib/fpm.h"
#include "dsplib/fpm_ecc.h"
#include "dsplib/sysdep.h"

/*
 * One section against one coefficient block.  The history is circular with
 * the newest entry at `idx`, so it is walked backwards from `idx` to 0 and
 * then from the top down to `idx + 1`, while the coefficients run forwards
 * across both halves.  Each product is shifted down 3 before it is
 * accumulated -- the taps carry more than 16 bits of headroom would allow
 * otherwise -- and the caller takes the remaining 14.
 */
static int
ecc_filter(const short *hist, short idx, short len, const short *c)
{
	int acc = 0;
	short i;

	for (i = idx; i >= 0; i--)
		acc += (hist[i] * *c++) >> 3;
	for (i = (short)(len - 1); i > idx; i--)
		acc += (hist[i] * *c++) >> 3;
	return acc;
}

/*
 * The sign-sign-free LMS update over the same walk: each coefficient moves by
 * the residual times the history sample it multiplied, scaled by `mu`.  Both
 * halves round rather than truncate, and the intermediate is squeezed back
 * into 16 bits between the two multiplies.
 */
static void
ecc_adapt(short *c, const short *hist, short idx, short len, int mu, int e)
{
	short i;
	int t;

	for (i = idx; i >= 0; i--) {
		t = (short)((hist[i] * mu + 0x10) >> 5);
		*c = (short)(((t * e + 0x10000) >> 17) + *c);
		c++;
	}
	for (i = (short)(len - 1); i > idx; i--) {
		t = (short)((hist[i] * mu + 0x10) >> 5);
		*c = (short)(((t * e + 0x10000) >> 17) + *c);
		c++;
	}
}

short
FPM_ECC_cancel(struct fpm_ecc *state, short *buf, unsigned short count)
{
	const short *const *imap = state->cfg.imap;
	const short *const *qmap = state->cfg.qmap;
	short *near_i = state->near_i;
	short *near_q = state->near_q;
	short *far_i = state->far_i;
	short *far_q = state->far_q;
	short *line = state->line;
	short cfg_near = state->cfg.near_taps;
	short cfg_far = state->cfg.far_taps;
	short near_idx = state->near_idx;
	short near_len = state->near_len;
	short far_idx = state->far_idx;
	short far_len = state->far_len;
	short near_rd = state->near_rd;
	short far_rd = state->far_rd;
	short line_len = state->line_len;
	short mu = state->mu;
	unsigned short phase = state->phase;
	unsigned short symbols = 0;
	short energy = 0;
	int n;

	/*
	 * Input power, before anything is taken out of it.  The meter is a
	 * one-pole smoother with the pole at 0x666/0x8000, so it follows the
	 * block mean square at about a twentieth of its distance per block.
	 */
	if (state->hold_power == 0) {
		short acc = 0;
		short i;

		for (i = 0; i < count; i++)
			acc = (short)(acc + ((buf[i] * buf[i]) >> 15));
		state->pwr_in = (short)(acc +
			(((state->pwr_in - acc) * 0x666) >> 15));
	}

	for (n = count; n != 0; n--) {
		short *coef = state->coef[phase];
		short *fcoef;
		short *ucoef;
		int est = 0;
		int e;
		short r;

		if (cfg_near != 0) {
			r = (short)(ecc_filter(near_i, near_idx, near_len,
					       coef) >> 14);
			est = (short)((ecc_filter(near_q, near_idx, near_len,
						  coef + cfg_near) >> 14) + r);
			fcoef = coef + 2 * cfg_near;
		} else {
			fcoef = coef;
		}

		r = (short)(ecc_filter(far_i, far_idx, far_len, fcoef) >> 14);
		est += (short)((ecc_filter(far_q, far_idx, far_len,
					   fcoef + cfg_far) >> 14) + r);

		e = (short)(*buf - est);
		*buf++ = (short)e;

		/* Residual power, the same meter on the other side. */
		if (state->hold_power == 0)
			energy = (short)(energy + ((e * e) >> 15));

		if (state->freeze == 0) {
			/*
			 * NOTE the aliasing, which is the original's and is
			 * reproduced deliberately: the far update starts from
			 * wherever the near update left off, and when the near
			 * update is skipped that is the START of the set, not
			 * the far blocks.  So with `adapt_near` off,
			 * `adapt_far` on and a non-zero `near_taps`, the far
			 * sections adapt the NEAR coefficients.  Only when
			 * `near_taps` is zero do the two coincide.
			 */
			ucoef = coef;
			if (state->adapt_near == 1 && cfg_near != 0) {
				ecc_adapt(coef, near_i, near_idx, near_len,
					  mu, e);
				ecc_adapt(coef + cfg_near, near_q, near_idx,
					  near_len, mu, e);
				ucoef = coef + 2 * cfg_near;
			}
			if (state->adapt_far == 1) {
				ecc_adapt(ucoef, far_i, far_idx, far_len,
					  mu, e);
				ecc_adapt(ucoef + cfg_far, far_q, far_idx,
					  far_len, mu, e);
			}
		}

		phase = (unsigned short)(phase + 1);
		if (phase > 2) {
			unsigned short usym;
			short ssym;
			short next;
			short si, sq;

			phase = 0;

			/*
			 * The near tap.  Read UNSIGNED, and the map index is
			 * masked out of the high byte.  The far tap below
			 * reads the same array SIGNED and shifts arithmetically
			 * -- both forced by the object, both indexing a
			 * pointer array, and no test can tell them apart while
			 * the line holds anything under 0x8000.  Writing the
			 * two the same way would be a defect of exactly the
			 * kind finding F613 records.
			 */
			usym = (unsigned short)line[near_rd];
			si = imap[(usym >> 8) & 0xff][usym & 0xff];
			sq = (short)-qmap[(usym >> 8) & 0xff][usym & 0xff];
			next = (short)(near_rd + 1);
			near_rd = (short)((next < line_len) ? next : 0);
			if (cfg_near != 0) {
				next = (short)(near_idx + 1);
				near_idx = (short)((next < near_len) ? next : 0);
				near_i[near_idx] = si;
				near_q[near_idx] = sq;
			}

			/* The far tap, `far_lag` symbols behind. */
			ssym = line[far_rd];
			si = imap[ssym >> 8][ssym & 0xff];
			sq = (short)-qmap[ssym >> 8][ssym & 0xff];
			next = (short)(far_idx + 1);
			far_idx = (short)((next < far_len) ? next : 0);
			far_i[far_idx] = si;
			far_q[far_idx] = sq;
			next = (short)(far_rd + 1);
			far_rd = (short)((next < line_len) ? next : 0);
			symbols++;
		}
	}

	if (state->hold_power == 0)
		state->pwr_out = (short)(energy +
			(((state->pwr_out - energy) * 0x666) >> 15));

	state->far_idx = far_idx;
	state->mu = mu;
	state->phase = phase;
	state->near_rd = near_rd;
	state->near_idx = near_idx;
	state->far_rd = far_rd;
	return (short)symbols;
}

void
FPM_ECC_init(struct fpm_ecc *state, const struct fpm_ecc_cfg *cfg, int fresh)
{
	short line_len;
	short taps;
	short i, j;
	int back;

	if (cfg != 0)
		state->cfg = *cfg;
	else
		state->cfg = ECC_CFG;

	/*
	 * The delay line has to hold the far tap's whole reach.  `taps` is the
	 * complex coefficient count of one section pair, so one coefficient
	 * set is 2 * taps entries.
	 */
	taps = (short)(state->cfg.near_taps + state->cfg.far_taps);
	line_len = (short)(state->cfg.far_lag + state->far_delay +
			   state->near_delay);

	if (fresh) {
		/*
		 * Unconditional, unlike FPM_MRF_init: no existing buffer is
		 * inspected and none is freed, so calling init twice with
		 * `fresh` set leaks.  That is the original's behaviour.
		 */
		state->near_i = sysdep_malloc(
			(unsigned)state->cfg.near_taps * sizeof(short));
		state->near_q = sysdep_malloc(
			(unsigned)state->cfg.near_taps * sizeof(short));
		state->far_i = sysdep_malloc(
			(unsigned)state->cfg.far_taps * sizeof(short));
		state->far_q = sysdep_malloc(
			(unsigned)state->cfg.far_taps * sizeof(short));
		state->line = sysdep_malloc(
			(unsigned)line_len * sizeof(short));
		for (j = 0; j <= 2; j++)
			state->coef[j] = sysdep_malloc(
				(unsigned)(2 * taps) * sizeof(short));
	}

	state->hold_power = 0;
	state->unk1a = 0;
	state->enabled = 1;
	state->pwr_in = 0;
	state->pwr_out = 0;
	state->freeze = 0;
	state->adapt_near = 0;
	state->adapt_far = 0;

	/*
	 * The two read taps.  `back` is where the near tap sits measured from
	 * the top of the line; the far tap is `far_lag` symbols older, and is
	 * NOT reduced modulo the length -- only the near one is.
	 */
	back = line_len - state->near_delay;
	state->near_rd = (short)(back % line_len);
	state->far_rd = (short)(back - state->cfg.far_lag);
	state->line_len = line_len;

	state->near_idx = 0;
	state->near_len = state->cfg.near_taps;
	state->far_idx = 0;
	state->far_len = state->cfg.far_taps;
	state->phase = 0;
	state->mu = 0x29;

	for (i = 0; i < state->near_len; i++) {
		state->near_i[i] = 0;
		state->near_q[i] = 0;
	}
	for (i = 0; i < state->far_len; i++) {
		state->far_i[i] = 0;
		state->far_q[i] = 0;
	}
	for (j = 0; j <= 2; j++)
		for (i = 0; i < 2 * taps; i++)
			state->coef[j][i] = 0;
	for (i = 0; i < line_len; i++)
		state->line[i] = state->cfg.fill;
}

void
FPM_ECC_free(struct fpm_ecc *state)
{
	short j;

	for (j = 2; j >= 0; j--)
		sysdep_free(state->coef[j]);
	sysdep_free(state->line);
	sysdep_free(state->far_q);
	sysdep_free(state->far_i);
	sysdep_free(state->near_q);
	sysdep_free(state->near_i);
}

/*
 * The library default, from .data:0x8114.  Not const: see the header.  No
 * maps and only four far taps -- a template every caller replaces, exactly as
 * FPM_MRF_CFG_data is for the resampler.
 */
struct fpm_ecc_cfg ECC_CFG = {
	480,	/* far_lag   */
	40,	/* near_taps */
	4,	/* far_taps  */
	0,	/* pad06     */
	0,	/* imap      */
	0,	/* qmap      */
	0,	/* fill      */
	0,	/* pad12     */
	0	/* aux       */
};
