/*
 * t_fpm_fsm.c -- differential test of the FSK modulator.
 *
 * FPM_FSM_init is not reconstructed (it builds the FPM_TONE object), so the
 * test builds *two* states with the reference init.  Cloning one would not
 * work: the state holds a pointer to a tone object, so both sides would
 * mutate the same oscillator and the comparison would be meaningless.
 *
 * Our modulate then drives a reference-built tone object through our own
 * FPM_TONE functions, which are separately verified bit-exact -- so a
 * mismatch here is the modulator's, not the oscillator's.
 */

#include <string.h>

#include <stdio.h>

#include "harness.h"
#include "dsplib/fpm_fsm.h"

extern void ref_FPM_FSM_init(void *state, const void *cfg);
extern short ref_FPM_FSM_modulate(void *state, const unsigned short *bits,
				  short *out, unsigned short nbits);
extern short ref_FPM_FSM_CFG[];

#define NBITS 400
#define SPS   24

static unsigned short bits[NBITS];
static short oa[NBITS * SPS], ob[NBITS * SPS];

int
main(void)
{
	struct fpm_fsm a, b;
	unsigned lfsr = 0x51F0u;
	int rc = 0, i, pass;

	for (i = 0; i < NBITS; i++) {
		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		/* Mix runs of identical bits with alternating ones. */
		bits[i] = (unsigned short)((i % 17 < 5) ? (i & 1) : (lfsr & 1));
	}

	/*
	 * `a` is built by the reference, `b` by ours -- so init is under test
	 * too, not just modulate.  Each gets its own tone object; cloning one
	 * would leave both sides driving the same oscillator.
	 */
	memset(&a, 0, sizeof(a));
	memset(&b, 0, sizeof(b));
	ref_FPM_FSM_init(&a, ref_FPM_FSM_CFG);
	FPM_FSM_init(&b, (const struct fpm_fsm_cfg *)ref_FPM_FSM_CFG);

	diff_begin("FSM init");
	diff_eq_int("freq[0] (%ld)", b.cfg.freq[0], a.cfg.freq[0], 0);
	diff_eq_int("freq[1] (%ld)", b.cfg.freq[1], a.cfg.freq[1], 0);
	diff_eq_int("samples per symbol (%ld)", b.cfg.samples_per_sym,
		    a.cfg.samples_per_sym, 0);
	diff_eq_int("scale (%ld)", b.cfg.scale, a.cfg.scale, 0);
	diff_eq_int("scaled[0] (%ld)", b.scaled[0], a.scaled[0], 0);
	diff_eq_int("scaled[1] (%ld)", b.scaled[1], a.scaled[1], 0);
	diff_eq_int("tone built (%ld)", b.tone != 0, 1, 0);
	/* The tone objects differ in address but must agree field for field. */
	{
		const unsigned char *ta = (const unsigned char *)a.tone;
		const unsigned char *tb = (const unsigned char *)b.tone;
		int off;

		for (off = 0; off < 0x2c; off += 2) {
			/*
			 * +0x10 holds a pointer, so both halves differ
			 * between builds -- ours points at our extracted
			 * ToneLPF, the reference at the blob's.  Skip the
			 * whole 4-byte slot, not just its first short.
			 */
			if (off == 0x10 || off == 0x12)
				continue;
			diff_eq_int("tone byte 0x%02lx",
				    *(const short *)(tb + off),
				    *(const short *)(ta + off), off);
		}
		/* Check the prototype itself matches, tap for tap. */
		{
			const short *pa = *(const short *const *)(ta + 0x10);
			const short *pb = *(const short *const *)(tb + 0x10);
			int t;

			for (t = 0; t < 53; t++)
				diff_eq_int("ToneLPF[%ld]", pb[t], pa[t], t);
		}
	}
	rc |= diff_end();

	diff_begin("FSM config");
	diff_eq_int("samples per symbol (%ld)", a.cfg.samples_per_sym, SPS, 0);
	diff_eq_int("mark scaled by 10/9 (%ld)", a.scaled[0],
		    (short)(((int)a.cfg.freq[0] * 0x471c) >> 14), 0);
	diff_eq_int("space scaled by 10/9 (%ld)", a.scaled[1],
		    (short)(((int)a.cfg.freq[1] * 0x471c) >> 14), 0);
	diff_eq_int("tone object built (%ld)", a.tone != 0, 1, 0);
	rc |= diff_end();

	/*
	 * Two passes, so the second starts from a phase the first left behind
	 * -- the oscillator is not reset between calls, and a modulator that
	 * reset it would still pass a single-pass test.
	 */
	for (pass = 0; pass < 2; pass++) {
		char label[48];
		int chunk = pass == 0 ? NBITS : 1;
		int pos = 0, na = 0, nb = 0;

		sprintf(label, "FSM modulate pass %d (chunk %d)", pass, chunk);
		diff_begin(label);

		while (pos < NBITS) {
			int n = (pos + chunk > NBITS) ? NBITS - pos : chunk;
			short ca, cb;
			int k;

			ca = ref_FPM_FSM_modulate(&a, bits + pos, oa + na,
						  (unsigned short)n);
			cb = FPM_FSM_modulate(&b, bits + pos, ob + nb,
					      (unsigned short)n);

			diff_eq_int("at bit %ld: sample count", cb, ca, pos);
			if (ca != cb)
				break;
			for (k = 0; k < ca; k++)
				diff_eq_int("sample %ld", ob[nb + k],
					    oa[na + k], na + k);
			na += ca;
			nb += cb;
			pos += n;
		}
		rc |= diff_end();
	}

	return rc;
}
