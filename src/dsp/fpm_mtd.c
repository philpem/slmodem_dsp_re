/*
 * fpm_mtd.c -- multi-tone detector.
 *
 * Reconstructed from dsplibs.o fpm_mtd.c:
 *   FPM_MTD_create  .text 0x0a90f0
 *   FPM_MTD_delete  .text 0x0a91b0
 *
 * FPM_MTD_detect (.text 0x0a91d0, 297 bytes) is not yet reconstructed.
 */

#include <stddef.h>

#include "dsplib/fpm_mtd.h"

extern void *sysdep_malloc(unsigned size);
extern void sysdep_free(void *ptr);

struct fpm_mtd *
FPM_MTD_create(struct fpm_mtd *state, const struct fpm_mtd_cfg *cfg)
{
	int owned = 0;
	int i;

	if (state == NULL) {
		state = (struct fpm_mtd *)sysdep_malloc(sizeof(*state));
		if (state == NULL)
			return NULL;
		owned = 1;
	}

	/* Only 12 bytes are copied -- the config's first three words. */
	if (cfg != NULL) {
		state->cfg.coeff = cfg->coeff;
		state->cfg.tones = cfg->tones;
		state->cfg.f06 = cfg->f06;
		state->cfg.f08 = cfg->f08;
		state->cfg.f0a = cfg->f0a;
	} else {
		state->cfg = FPM_MTD_CFG_data;
	}

	/* As elsewhere in fpm_*, buffers follow the object's ownership. */
	if (owned)
		state->acc = (short *)sysdep_malloc(
			(unsigned)state->cfg.tones * 2 * sizeof(short));

	/* Two accumulators per tone. */
	for (i = 0; i < state->cfg.tones; i++) {
		state->acc[i * 2] = 0;
		state->acc[i * 2 + 1] = 0;
	}

	state->f10 = 0;
	state->f12 = 0;
	state->f14 = 0;
	state->f16 = 0;

	return state;
}

/*
 * Frees the accumulator array and the state, both unconditionally -- the same
 * shape as FPM_TONE_delete, and carrying the same asymmetry with create (see
 * D5).  Bell 103 does not call it: B103FP_create supplies its own embedded
 * state, so tearing that down through here would free memory it does not own.
 */
void
FPM_MTD_delete(struct fpm_mtd *state)
{
	sysdep_free(state->acc);
	sysdep_free(state);
}
