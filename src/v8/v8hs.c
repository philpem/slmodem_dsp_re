/*
 * v8hs.c -- laying out the handshake.
 *
 * `V8Create` allocates the object and plants a handful of configuration
 * values in it; this is what reads them back and builds the machine to
 * match.  It is wide rather than deep -- sixty-five fields written -- and
 * almost all of the width is one of three shapes chosen by `v->mode`:
 *
 *   0  the full handshake: tone detector, phase-reversal detector, a CM
 *      built by initTxSequence and a short fixed sequence hand-built beside
 *      it;
 *   1  the answering side: a tone generator, the V.21 modem brought up on
 *      channel 2, and one sequence;
 *   anything else  nothing beyond the preamble every shape shares.
 */

#include "dsplib/v8.h"

/*
 * The detector's coefficient table, .rodata+0x5670.  Eight entries, not the
 * hundred-odd the gap to the next known table suggests: relocations begin at
 * .rodata+0x5680, so everything past index 8 is a pointer array belonging to
 * something else.  A verbatim copy of that region would hold link-time
 * addends where the running object holds addresses, which is exactly how the
 * first version failed.
 */
static const short detector_table[8] = {
	     0,      0,      0,      0,  -6608,  15416,  -5792,  15416
};

/* The two configured timeouts are in units of a quarter of a 9600 Hz second. */
static int
deadline(int units)
{
	if (units <= 0)
		return -1;
	return (units * 9600) >> 2;
}

void
v8handshakinit(struct v8 *v)
{
	struct v8_cm *cm;
	int mode;

	/* The preamble, common to every shape. */
	v->fa3e = 0x10;
	v->fd94 = 0;
	v->fd96 = 0x1a;
	v->fd9c = 0;
	v->fda0 = 0;
	v->fd98 = 0;
	v->fa40 = 0x200;

	v8_rxinit(v);
	v8_txinit(v);

	v->fdc4 = 0;
	v->fdd0 = 0;
	v->fec2 = 0;
	mode = v->mode;
	v->fdc8 = 0;
	v->fdcc = 0;
	v->febc = 0;
	v->febe = 0;
	v->fec0 = 0;

	if (mode == 0) {
		v->f9d4 = 5;
		v->f9d8 = 0x19;
		v->f9d6 = 0x19;
		/*
		 * Assigned, not or-ed: this drops whatever v8_rxinit left in
		 * the flag word, and the two detectors below then set their
		 * own bits on top.
		 */
		v->rx.flags = 0x8000;

		v->deadline_a = deadline(v->timeout_a);
		v->deadline_b = deadline(v->timeout_b);
		v->fe64 = 0;

		v8_detectorinit(v, &v->detector, detector_table, 0, 100, 50,
				1500, 0);
		v8_phase_rev_init(&v->phase_rev);

		v->tx_seq = &v->seq[0];
		v->seq_alt = &v->seq[2];
		initTxSequence(v);

		/* A three-character sequence of all ones, built by hand. */
		v->seq[1].word[0] = 1;
		v->seq[1].word[1] = 1;
		v->seq[1].word[2] = 1;
		v->seq[1].terminator = -1;
		v->seq[1].nbits = 30;
		v->seq[1].f26 = 10;
		v->seq[1].f2a = 1;
		v->seq[1].f20 = 0;
		v->seq[1].f24 = 0;
		v->seq[1].f28 = 0;
		v->seq[1].f2c = 0;
		v->seq[1].f30 = 0;
		v->seq[1].f34 = 0;
		v->seq[1].f38 = 0;
		v->seq[1].f3c = 0;

		v->fdbe = (short)(v->fa48 == 0);
		cm = v->cm;

		if (cm->b2 & 0x10) {
			int bits;
			int menu = cm->menu;

			v->tx_seq = &v->seq[3];
			v->seq_spare = &v->seq[4];

			v->seq[3].word[0] = 0x3ff;
			v->seq[3].word[1] = 0x155;

			bits = ((menu * 2) & 0x04) | ((menu * 2) & 0x08)
			       | ((menu * 4) & 0x20);
			bits |= (cm->b2 & 0x40) ? 0x41 : 0x01;
			if (menu & 0x01)
				bits |= 0x02;

			v->seq[3].word[2] = (short)bits;
			v->seq[3].word[5] = (short)bits;
			v->seq[3].word[3] = 0x3ff;
			v->seq[3].word[4] = 0x155;

			v->fdc0 = 0;
			v->toneq_pending = 0;
			v->toneq_period = 0x688;

			v->seq[3].terminator = -1;
			v->seq[3].f24 = 0;
			v->seq[3].f28 = 0;
			v->seq[3].f2c = 0;
			v->seq[3].nbits = 60;
			v->seq[3].f26 = 10;
			v->seq[3].f20 = 0;
			v->seq[3].f30 = 0;
			v->seq[3].f38 = 0;
			v->seq[3].f34 = 0;
			v->seq[3].f3c = 0;
			v->seq[3].f2a = 1;
		}

		v->fdb6 = 0;
		v->fdb4 = 0;
		return;
	}

	if (mode != 1)
		return;

	v->f9d6 = 0x20;
	v->f9d4 = 6;
	v->deadline_a = deadline(v->timeout_a);
	v->deadline_b = deadline(v->timeout_b);

	v->tone.f02 = 0;
	v->tone.f04 = 0x1a;
	v->tone.f06 = 0xe00;
	v->tone.f0a = 0;
	v->fe64 = 0;
	v->tone.f00 = 0;
	v->tone.f08 = v8_mpyint(0x3e80, v->fa42);
	v->tone.f0e = 1;

	v8_V21_Init(v, 1, 0);

	v->tx_seq = &v->seq[2];
	v->seq_alt = &v->seq[0];
	/* Again assigned, so the bit v8_V21_Init just set is dropped. */
	v->rx.flags = 0x8004;
	initTxSequence(v);

	v->fdb8 = 0;
	v->fdb6 = 0;
	v->fdb4 = 0;
}
