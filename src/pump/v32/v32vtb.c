/*
 * v32vtb.c -- V.32bis' half of the Viterbi trellis decoder: its setup.
 *
 * Reconstructed from dsplibs.o:
 *   VTBv32_init  .text 0x7e700  355
 *
 * The decoder itself is protocol-independent and lives in src/dsp/vtb.c; all
 * this does is point the state at V.32bis' tables for one rate, size the
 * survivor ring and clear it.
 *
 * `alloc` IS SEPARATE FROM THE RESET because a rate change re-enters this
 * with an already-allocated ring: non-zero allocates a new 0x200-byte block
 * and stores it at +0x00, zero keeps whatever is there.  The clear that
 * follows is unconditional and runs over 128 nodes either way, which is what
 * makes the two paths differ only in the pointer.
 *
 * THE RATE CODE.  Modes 2, 3 and 4 are 9600, 7200 and 12000; ANYTHING ELSE
 * takes the 128-point 14400 branch, including 14400's own code and every
 * value outside the set.  That is the object's own `switch` shape -- a
 * three-way compare chain with the 128-point case as the fall-through -- and
 * not a default supplied here.
 *
 * `nsub` is the number of bits the trellis leaves below the two differential
 * ones, so 1, 2, 3 and 4 for the four constellations; the grid is 2*nsub and
 * the mask is (1 << (nsub + 2)) - 1.  Both are written as literals because
 * that is how the object writes them: four independent `movw` immediates per
 * branch, with no arithmetic between them.
 */

#include "dsplib/sysdep.h"
#include "dsplib/vtb.h"

void
VTBv32_init(struct vtb *state, short mode, int alloc)
{
	struct vtb_path *paths;
	int i;

	if (alloc)
		state->paths = (struct vtb_path *)sysdep_malloc(
			16 * 8 * sizeof(struct vtb_path));
	paths = state->paths;

	for (i = 0; (short)i <= 0x7f; i++) {
		paths[i].surv = 0;
		paths[i].sym = 0;
	}

	state->ring = 0;
	state->prev = 0;
	state->depth = 0x10;

	if (mode == 3) {
		state->nsub = 1;
		state->imap = VTBv32_IMAP16T;
		state->qmap = VTBv32_QMAP16T;
		state->bound = VTB_BOUND_7200;
		state->region = VTB_REGION_7200;
		state->grid = 2;
		state->mask = 0x7;
	} else if (mode == 4) {
		state->nsub = 3;
		state->imap = VTBv32_IMAP64;
		state->qmap = VTBv32_QMAP64;
		state->bound = VTB_BOUND_12000;
		state->region = VTB_REGION_12000;
		state->grid = 6;
		state->mask = 0x1f;
	} else if (mode == 2) {
		state->nsub = 2;
		state->imap = VTBv32_IMAP32;
		state->qmap = VTBv32_QMAP32;
		state->bound = VTB_BOUND_9600;
		state->region = VTB_REGION_9600;
		state->grid = 4;
		state->mask = 0xf;
	} else {
		state->nsub = 4;
		state->imap = VTBv32_IMAP128;
		state->qmap = VTBv32_QMAP128;
		state->bound = VTB_BOUND_14400;
		state->region = VTB_REGION_14400;
		state->grid = 8;
		state->mask = 0x3f;
	}

	state->metric[0] = 0;
	state->shift = (short)state->nsub;

	for (i = 1; (short)i <= 7; i++)
		state->metric[i] = 0;
}
