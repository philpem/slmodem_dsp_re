/*
 * Boundary checks for #51's normal (non-reproduction) allocation arithmetic.
 * These do not link the blob: DSPLIB_REPRODUCE_BUGS deliberately retains its
 * undersized requests for the differential tier.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_fse.h"
#include "dsplib/fpm_sre.h"
#include "dsplib/sgd.h"

static short sre_proto[32768];
static short fse_i[32767];
static short fse_q[32767];

static int
check_sre(void)
{
	struct fpm_sre state;
	struct fpm_sre_cfg cfg;
	int rc;

	memset(&state, 0, sizeof(state));
	memset(&cfg, 0, sizeof(cfg));
	cfg.coeffs = 32767;
	cfg.rms_len = 32767;
	cfg.proto = sre_proto;

	harness_alloc_reset();
	diff_begin("#51 FPM_SRE full-width allocations");
	FPM_SRE_init(&state, &cfg, 1);
	diff_eq_int("coefficient request (%ld)", harness_alloc_reqsize(state.coeff),
			    65534, 0);
	diff_eq_int("RMS request (%ld)", harness_alloc_reqsize(state.rms_buf),
			    65534, 0);
	FPM_SRE_free(&state);
	diff_eq_int("no live allocations (%ld)", harness_alloc.live, 0, 0);
	rc = diff_end();
	return rc;
}

static int
check_fse(void)
{
	struct fpm_fse state;
	struct fpm_fse_cfg cfg;
	int rc;

	memset(&state, 0, sizeof(state));
	memset(&cfg, 0, sizeof(cfg));
	cfg.block = 32767;
	cfg.interp = 1;
	cfg.taps = 32767;
	cfg.icoff = fse_i;
	cfg.qcoff = fse_q;

	harness_alloc_reset();
	diff_begin("#51 FPM_FSE full-width allocations");
	FPM_FSE_init(&state, &cfg, 1);
	diff_eq_int("coefficient request (%ld)", harness_alloc_reqsize(state.icoeff),
			    65534, 0);
	diff_eq_int("output request (%ld)", harness_alloc_reqsize(state.out_i),
			    65538, 0);
	FPM_FSE_free(&state);
	diff_eq_int("no live allocations (%ld)", harness_alloc.live, 0, 0);
	rc = diff_end();
	return rc;
}

static int
check_sgd(void)
{
	struct sgd_cfg cfg;
	struct sgd *state;
	int rc;

	memset(&cfg, 0, sizeof(cfg));
	cfg.hist_len = 32767;
	cfg.hist_extra = 2;
	cfg.det.ref_len = 1;

	harness_alloc_reset();
	diff_begin("#51 SGD full-width history allocation");
	state = SGD_create(0, &cfg);
	diff_eq_int("wrapped-boundary request (%ld)",
			    harness_alloc_reqsize(state->hist), 65536, 0);
	SGD_delete(state);
	diff_eq_int("no live allocations (%ld)", harness_alloc.live, 0, 0);
	rc = diff_end();

	memset(&cfg, 0, sizeof(cfg));
	cfg.hist_len = 2;
	cfg.hist_extra = 1;
	cfg.det.ref_len = 2;
	harness_alloc_reset();
	diff_begin("#51 SGD allocates the cleared span");
	state = SGD_create(0, &cfg);
	diff_eq_int("history covers ref_len (%ld)",
			    harness_alloc_reqsize(state->hist), 6, 0);
	SGD_delete(state);
	diff_eq_int("no live allocations (%ld)", harness_alloc.live, 0, 0);
	rc |= diff_end();
	return rc;
}

int
main(void)
{
	int rc = 0;

#ifdef DSPLIB_REPRODUCE_BUGS
#error "t_alloc_sizes must exercise the normal safety build"
#endif
	rc |= check_sre();
	rc |= check_fse();
	rc |= check_sgd();
	return rc;
}
