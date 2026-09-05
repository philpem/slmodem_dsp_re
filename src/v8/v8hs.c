/*
 * v8hs.c -- laying out the handshake.
 *
 * `V8Create` allocates the object and plants a handful of configuration
 * values in it; this is what reads them back and builds the machine to
 * match.  It is wide rather than deep -- sixty-five fields written -- and
 * almost all of the width is one of three shapes chosen by `v->side`:
 *
 *   0  the full handshake: tone detector, phase-reversal detector, a CM
 *      built by initTxSequence and a short fixed sequence hand-built beside
 *      it;
 *   1  the answering side: a tone generator, the V.21 modem brought up on
 *      channel 2, and one sequence;
 *   anything else  nothing beyond the preamble every shape shares.
 */

#include "dsplib/debug.h"
#include "dsplib/v8.h"
#include "dsplib/sysdep.h"

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
	v->tx_fill_target = 0x10;
	v->dft.phase = 0;
	v->dft.step = 0x1a;
	v->dft.im = 0;
	v->dft.energy = 0;
	v->dft.re = 0;
	v->short_a40 = 0x200;

	v8_rxinit(v);
	v8_txinit(v);

	v->quick_connect = 0;
	v->qca1a_done = 0;
	v->ext2_word = 0;
	mode = v->side;
	v->lapm_indication = 0;
	v->anspcm_level = 0;
	v->fn_matched = 0;
	v->ext2_matched = 0;
	v->fn_word = 0;

	if (mode == 0) {
		v->tx_state = 5;
		v->rx_substate = 0x19;
		v->rx_state = 0x19;
		/*
		 * Assigned, not or-ed: this drops whatever v8_rxinit left in
		 * the flag word, and the two detectors below then set their
		 * own bits on top.
		 */
		v->rx.flags = 0x8000;

		v->deadline_a = deadline(v->timeout_a);
		v->deadline_b = deadline(v->timeout_b);
		v->elapsed = 0;

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
		v->seq[1].crc = -1;
		v->seq[1].nbits = 30;
		v->seq[1].wordbits = 10;
		v->seq[1].repeat = 1;
		v->seq[1].crc_enable = 0;
		v->seq[1].bitpos = 0;
		v->seq[1].wordidx = 0;
		v->seq[1].repeats = 0;
		v->seq[1].shifter = 0;
		v->seq[1].nleft = 0;
		v->seq[1].shifter0 = 0;
		v->seq[1].nleft0 = 0;

		v->cm_ready = (short)(v->op_mode == 0);
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

			v->cm_bit_count = 0;
			v->toneq_pending = 0;
			v->toneq_period = 0x688;

			v->seq[3].crc = -1;
			v->seq[3].bitpos = 0;
			v->seq[3].wordidx = 0;
			v->seq[3].repeats = 0;
			v->seq[3].nbits = 60;
			v->seq[3].wordbits = 10;
			v->seq[3].crc_enable = 0;
			v->seq[3].shifter = 0;
			v->seq[3].shifter0 = 0;
			v->seq[3].nleft = 0;
			v->seq[3].nleft0 = 0;
			v->seq[3].repeat = 1;
		}

		v->block_count = 0;
		v->short_db4 = 0;
		return;
	}

	if (mode != 1)
		return;

	v->rx_state = 0x20;
	v->tx_state = 6;
	v->deadline_a = deadline(v->timeout_a);
	v->deadline_b = deadline(v->timeout_b);

	v->tone.mod_phase = 0;
	v->tone.carrier_step = 0x1a;
	v->tone.mod_step = 0xe00;
	v->tone.reversal_count = 0;
	v->elapsed = 0;
	v->tone.carrier_phase = 0;
	v->tone.amplitude = v8_mpyint(0x3e80, v->tx_gain);
	v->tone.reversal_enable = 1;

	v8_V21_Init(v, 1, 0);

	v->tx_seq = &v->seq[2];
	v->seq_alt = &v->seq[0];
	/* Again assigned, so the bit v8_V21_Init just set is dropped. */
	v->rx.flags = 0x8004;
	initTxSequence(v);

	v->cj_zero_run = 0;
	v->block_count = 0;
	v->short_db4 = 0;
}

/*
 * Build a handshake.
 *
 * Note what is NOT here: the object is allocated and never zeroed.  Only the
 * six configuration words below and whatever `v8handshakinit` writes are
 * defined when this returns, and the rest is whatever the allocator had.  A
 * caller that reads anything else is reading rubbish -- which is worth
 * knowing, because on a fresh page that rubbish is usually zero and so looks
 * deliberate.
 *
 * THE GUARD IS SINGLE-EXIT AND THAT IS READ OFF THE OBJECT, not a style
 * choice.  The blob's `je` lands on `add $0x34,%esp` with the `mov %esi,%eax`
 * BELOW it, so the failure path falls through the same return the success
 * path uses.  Written as an early `return`, GCC 3.4.2 has a second value to
 * materialise, spends `xor %eax,%eax` on it before the first `cfg` copy --
 * which costs `%eax` as a store base for two of them -- and has to jump PAST
 * the shared `mov`.  All three early-return spellings (`return 0`, `return v`,
 * `if (!v) return v`) compile to ONE emission, because the compiler knows `v`
 * is null on that arm and the returned expression is free; both single-exit
 * spellings compile to another.  A two-element domain, exhausted, and only
 * one element produces the object's control flow: 167 differing bytes to 25.
 *
 * THE RESIDUAL 25 ARE NOT STATEMENT ORDER, and that is measured.  All 720
 * orderings of the six configuration copies below were compiled: 76 distinct
 * emissions, NONE at zero, best 12.  Our source order IS the blob's emission
 * order (0xa44, 0xa48, 0xa4c, 0xa50, 0xa54, 0xa58) and it is OUR compiler
 * that permutes it, hoisting the `cfg->cm` load; no source order is the
 * preimage.  Rule 0's third case -- the difference is not what it looks like.
 * Declined rather than hill-climbed to 12.
 */
struct v8 *
V8Create(const struct v8_cfg *cfg)
{
	struct v8 *v = sysdep_malloc(sizeof(struct v8));

	if (v != 0) {
		v->side = cfg->side;
		v->op_mode = cfg->op_mode;
		v->timeout_a = cfg->timeout_a;
		v->timeout_b = cfg->timeout_b;
		v->rate = cfg->rate;
		v->cm = cfg->cm;

		/*
		 * The configuration trace: seventeen messages, each behind its own
		 * gate.  This is where the author dates the module (23/09/03) and
		 * names what the fields mean -- `side` and `op_mode` were called
		 * operation mode, `offered` the ansPcmLevel, `menu` the ucodeForQts,
		 * and the two CM extension fields are raw call-function and protocol
		 * octets.  See finding F164.
		 *
		 * Every message ends \r\n -- all of V8's diagnostics do.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V8: Create called, V8 version 23/09/03 .\r\n");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("#################################"
					     "###########################\r\n");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V8: local configuration : \r\n");
		/*
		 * `== 0` with "Caller" first, and both halves of that are the
		 * object's.  The blob branches `je` where we branched `jne` --
		 * one byte, 0x74 against 0x75, same displacement, same target
		 * -- and its `.rodata.str1.1` holds "Caller" before "Answer"
		 * where ours held them the other way round.  GCC interns
		 * literals in source-text order, so the pool order is a second
		 * observable that agrees without reference to any instruction
		 * (lever 2).  Four spellings compiled; the two that put
		 * "Caller" first both reach it, so what is decoded is the arm
		 * ORDER and not `== 0` against `!v->side`.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tSide = %s\r\n",
					     v->side == 0 ? "Caller" : "Answer");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tOperation Mode = %d\r\n", v->op_mode);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "\tModulations - V90=%d, V34=%d, V34HD=%d, V32=%d, "
			    "V22=%d, V17=%d, V29=%d, V27=%d, V23=%d, V21=%d\r\n",
			    (v->cm->b0 >> 3) & 1, (v->cm->b0 >> 5) & 1,
			    (v->cm->b0 >> 6) & 1, v->cm->b0 >> 7,
			    v->cm->b1 & 1, (v->cm->b1 >> 1) & 1,
			    (v->cm->b1 >> 2) & 1, (v->cm->b1 >> 3) & 1,
			    (v->cm->b1 >> 4) & 1, (v->cm->b1 >> 5) & 1);

		/* The presence bits are tested outside the gates, not inside. */
		if (v->cm->b2 & V8_CM_EXT1_PRESENT) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "\tCall Functions - raw CF specified: cf[0]=%d , "
				    "cf[1]=%d , cf[2]=%d , cf[3]=%d\r\n",
				    v->cm->ext1[0], v->cm->ext1[1],
				    v->cm->ext1[2], v->cm->ext1[3]);
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf(
			    "\tCall Functions - Data=%d, CallRxFax=%d, CallTxFax=%d, "
			    "V.80=%d\r\n",
			    (v->cm->b1 >> 6) & 1, v->cm->b1 >> 7,
			    v->cm->b2 & 1, (v->cm->b2 >> 1) & 1);
		}

		if (v->cm->b2 & V8_CM_EXT2_PRESENT) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "\tProtocol - raw Protocol specified: prot[0]=%d "
				    ", prot[1]=%d , prot[2]=%d , prot[3]=%d\r\n",
				    v->cm->ext2[0], v->cm->ext2[1],
				    v->cm->ext2[2], v->cm->ext2[3]);
		} else if (DSPLIB_DEBUG_ON()) {
			dsplibs_debug_printf("\tProtocol - LAPM V.42\r\n");
		}

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tv8bisIndication - %d\r\n",
					     (v->cm->b0 >> 1) & 1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\ttimeouts - signal detect %d sec, "
					     "message detect %d sec\r\n",
					     v->timeout_a, v->timeout_b);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tquickConnectEnabled - %d\r\n",
					     (v->cm->b2 >> 4) & 1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tlapmIndication - %d\r\n",
					     (v->cm->b2 >> 6) & 1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tucodeForQts - %d\r\n", v->cm->menu);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\tansPcmLevel - %d\r\n", v->cm->offered);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("#################################"
					     "###########################\r\n");

		v->tx_gain = 0x4000;
		v8handshakinit(v);

		v->pole_state = 0;
		v->prev_status = 0;
	}
	return v;
}

void
V8Delete(struct v8 *v)
{
	if (v != 0)
		sysdep_free(v);
}
