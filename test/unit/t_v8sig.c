/*
 * t_v8sig.c -- differential test of the handshake's signal plumbing.
 *
 * All five take the whole object and write scattered parts of it, so the
 * comparison is the whole object every time, with the pointers the queue
 * functions move normalised to offsets first.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/v8.h"

extern unsigned int ref_dsplibs_debug_level;

extern void ref_v8_ansaminit(struct v8 *v);
extern void ref_v8_TONEq_generate(struct v8 *v, short *out);
extern int ref_v8_rxreadqueue(struct v8 *v);
extern int ref_v8_txwritequeue(struct v8 *v);
extern short ref_v8_fsktxfilter(struct v8 *v, short sample);
extern int ref_v8_txinit(struct v8 *v);
extern void ref_v8_dftupdate(struct v8_dft_bin *b, short n, const short *s,
			     short ns);
extern int ref_v8_fskmodulate(struct v8 *v, short which);
extern int ref_v8_agcadapt(struct v8 *v);
extern void ref_v8_ansamgenerate(struct v8 *v, short *out);
extern int ref_V8Control(struct v8 *v, int what);
extern int ref_v8_getbit(struct v8_tx_sequence *s);
extern void ref_initTxSequence(struct v8 *v);
extern void ref_v8_phase_rev_detect(struct v8_phase_rev *pr, const short *in,
				    short count);
extern void ref_v8_phase_rev_init(struct v8_phase_rev *pr);
extern short ref_notch_filter(const short *in, struct v8_detector *d);
extern short ref_biquad_filter(short in, struct v8_detector *d,
			       const short *coeff);
extern int ref_v8_tone_detect(struct v8 *v, struct v8_detector *d, short *in);
extern void ref_v8_detectorinit(struct v8 *v, struct v8_detector *d,
				const short *t, short a3, short a4, short a5,
				short a6, short a7);
extern void ref_v8_fskdemodulate(struct v8 *v);
extern void ref_v8_V21_Init(struct v8 *v, short ch, short ans);
extern int ref_V8agc(struct v8 *v);
extern int ref_v8_rxinit(struct v8 *v);
extern void ref_checkSignalStability(struct v8 *v);

static struct v8 obj_a, obj_b;

static void
fill(void *p, size_t n, unsigned seed)
{
	unsigned char *b = p;
	size_t i;

	for (i = 0; i < n; i++) {
		seed = seed * 1103515245u + 12345u;
		b[i] = (unsigned char)(seed >> 16);
	}
}

/* The four filter pointers exist twice; compare by content, then blank. */
static void
compare_v21(void)
{
	const short *pa[4], *pb[4];
	int i, k2;

	pa[0] = obj_a.v21.a; pb[0] = obj_b.v21.a;
	pa[1] = obj_a.v21.b; pb[1] = obj_b.v21.b;
	pa[2] = obj_a.v21.c; pb[2] = obj_b.v21.c;
	pa[3] = obj_a.v21.d; pb[3] = obj_b.v21.d;
	for (k2 = 0; k2 < 4; k2++) {
		if (pa[k2] == 0 || pb[k2] == 0)
			continue;
		for (i = 0; i < 40; i++)
			diff_eq_int("v21 coeff %ld", pb[k2][i], pa[k2][i], i);
	}
	obj_a.v21.a = obj_b.v21.a = 0;
	obj_a.v21.b = obj_b.v21.b = 0;
	obj_a.v21.c = obj_b.v21.c = 0;
	obj_a.v21.d = obj_b.v21.d = 0;
}

/* A pointer into the object, compared as an offset and then blanked. */
static void
normalise(size_t off)
{
	uintptr_t pa, pb;
	long da, db;

	memcpy(&pa, (char *)&obj_a + off, sizeof(pa));
	memcpy(&pb, (char *)&obj_b + off, sizeof(pb));
	da = (long)(pa - (uintptr_t)&obj_a);
	db = (long)(pb - (uintptr_t)&obj_b);
	diff_eq_int("pointer at +%ld", db, da, (long)off);
	memcpy((char *)&obj_a + off, &da, sizeof(da));
	memcpy((char *)&obj_b + off, &db, sizeof(db));
}

static void
whole(long tag)
{
	size_t i;
	const unsigned char *a = (const unsigned char *)&obj_a;
	const unsigned char *b = (const unsigned char *)&obj_b;

	for (i = 0; i < sizeof(struct v8); i++)
		diff_eq_int("byte at +%ld", b[i], a[i], (long)i);
	(void)tag;
}

/*
 * The five diagnostics in this file: V8Control's two, V8agc's two and the
 * phase detector's one.  Small deliberate scenarios rather than the sweeps
 * above, because a sweep at level 2 would fill the capture buffer and the
 * comparison would then be between two truncations.
 */
static int
t_sig_trace(void)
{
	static struct v8_phase_rev pa, pb;
	static short air[4096];
	unsigned lvl;
	long lines = 0;
	int k, what, blk, m;

	diff_begin("v8sig: the trace");

	for (lvl = 1; lvl <= 3; lvl++) {
		dsplib_debug_capture_reset();
		dsplibs_debug_level = ref_dsplibs_debug_level = lvl;
		dsplib_debug_capture_on = 1;

		/* Every request, accepted and refused, plus two unknown. */
		for (k = 0; k < 4; k++) {
			for (what = -1; what <= 3; what++) {
				fill(&obj_a, sizeof(obj_a), 3100u + k);
				memcpy(&obj_b, &obj_a, sizeof(obj_a));
				obj_a.side = obj_b.side = k & 1;
				obj_a.rx_state = obj_b.rx_state =
					(short)(k & 2 ? 0x19 : 0x18);
				obj_a.cm_ready = obj_b.cm_ready = 0;
				obj_a.rx_substate = obj_b.rx_substate =
					(short)(k & 1 ? V8_HS_TAKEN_RX
						      : V8_HS_TAKEN_TX);
				diff_eq_int("control returns (%ld)",
					    V8Control(&obj_b, what),
					    ref_V8Control(&obj_a, what),
					    (long)(lvl * 16 + what));
			}
		}

		/*
		 * A gain far past saturation, with the handshake waiting on
		 * the tone: every sample clips, and after ten blocks the run
		 * of clipping sends it back to look for ANSam.
		 */
		for (k = 0; k < 2; k++) {
			fill(&obj_a, sizeof(obj_a), 8800u + k);
			memcpy(&obj_b, &obj_a, sizeof(obj_a));
			ref_v8_txinit(&obj_a);
			ref_v8_txinit(&obj_b);
			ref_v8_rxinit(&obj_a);
			ref_v8_rxinit(&obj_b);
			obj_a.side = obj_b.side = 0;
			obj_a.rx.gain = obj_b.rx.gain = 0x7000;
			obj_a.rx.clip_count = obj_b.rx.clip_count = 0;
			obj_a.rx.flags = obj_b.rx.flags = 0;
			obj_a.rx_substate = obj_b.rx_substate = (short)(k == 0 ? 0x24
								 : 0x19);

			for (blk = 0; blk < 4; blk++) {
				for (m = 0; m < V8_TX_SYMBOLS; m++) {
					short x = (short)((m & 4) ? 30000
								  : -30000);

					obj_a.tx_symbols[m] = x;
					obj_b.tx_symbols[m] = x;
				}
				diff_eq_int("agc returns (%ld)", V8agc(&obj_b),
					    ref_V8agc(&obj_a),
					    (long)(lvl * 16 + k * 4 + blk));
				diff_eq_int("clip count (%ld)", obj_b.rx.clip_count,
					    obj_a.rx.clip_count,
					    (long)(lvl * 16 + k * 4 + blk));
				diff_eq_int("sub-state (%ld)", obj_b.rx_substate,
					    obj_a.rx_substate,
					    (long)(lvl * 16 + k * 4 + blk));
			}
		}

		/* And a stretch of ANSam, long enough for the reversals. */
		fill(&obj_a, sizeof(obj_a), 5000u);
		ref_v8_txinit(&obj_a);
		ref_v8_ansaminit(&obj_a);
		obj_a.tone.amplitude = 6000;
		obj_a.tone.reversal_enable = 1;
		obj_a.tone.reversal_count = 0x430;
		for (blk = 0; blk < 1024; blk++)
			ref_v8_ansamgenerate(&obj_a, air + blk * 4);

		memset(&pa, 0, sizeof(pa));
		memset(&pb, 0, sizeof(pb));
		ref_v8_phase_rev_init(&pa);
		ref_v8_phase_rev_init(&pb);
		for (blk = 0; blk < 4096; blk += 64) {
			/*
			 * ANSam's own reversals come far too close together to
			 * pass the spacing test -- the sweep above never sees
			 * a detection at all, which is why it does not assert
			 * one.  So the run is placed where a real ANSam would
			 * have taken it after four thousand steady samples:
			 * 4200 maps to a spacing of 437, inside the window,
			 * and both sides are placed there alike.
			 */
			pa.run = pb.run = 4200;
			pa.reversals = pb.reversals = 2;
			pa.detected = pb.detected = 0;
			ref_v8_phase_rev_detect(&pa, air + blk, 64);
			v8_phase_rev_detect(&pb, air + blk, 64);
			diff_eq_int("detected (%ld)", pb.detected, pa.detected,
				    (long)(lvl * 64 + blk / 64));
			diff_eq_int("run (%ld)", pb.run, pa.run,
				    (long)(lvl * 64 + blk / 64));
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;

		diff_eq_int("transcript matches (level %ld)",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
		if (getenv("DBGDIFF")
		    && strcmp(dsplib_debug_capture_text(0),
			      dsplib_debug_capture_text(1)) != 0) {
			const char *o = dsplib_debug_capture_text(0);
			const char *r = dsplib_debug_capture_text(1);
			int j = 0;

			while (o[j] && o[j] == r[j])
				j++;
			while (j > 0 && o[j - 1] != '\n')
				j--;
			printf("=== level %u: divergence at %d\n", lvl, j);
			printf("--- ours: %.400s\n", o + j);
			printf("--- ref : %.400s\n", r + j);
		}
		diff_eq_int("line counts match (level %ld)",
			    (int)dsplib_debug_capture_lines(0),
			    (int)dsplib_debug_capture_lines(1), (long)lvl);
		if (lvl == 1)
			diff_eq_int("silent below the threshold",
				    (int)dsplib_debug_capture_lines(1), 0, 0);
		else
			lines += dsplib_debug_capture_lines(1);
	}

	diff_eq_int("diagnostics were captured (%ld lines)", lines > 40, 1,
		    lines);

	return diff_end();
}

int
main(void)
{
	int rc = 0;
	int k, i, n;
	long moved = 0;

	diff_begin("v8_ansaminit");
	for (k = 0; k < 8; k++) {
		fill(&obj_a, sizeof(obj_a), 11u + k);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		obj_a.tx_gain = obj_b.tx_gain = (short)(k * 4001 - 16000);
		ref_v8_ansaminit(&obj_a);
		v8_ansaminit(&obj_b);
		whole(k);
	}
	diff_eq_int("the scaled field was set", obj_b.tone.reversal_enable, 1, 0);
	rc |= diff_end();

	diff_begin("v8_TONEq_generate");
	{
		short out_a[4], out_b[4];
		int nonzero = 0;

		for (k = 0; k < 64; k++) {
			fill(&obj_a, sizeof(obj_a), 700u + k);
			memcpy(&obj_b, &obj_a, sizeof(obj_a));
			obj_a.toneq_period = obj_b.toneq_period =
				(short)(k * 101);
			memset(out_a, 0x5a, sizeof(out_a));
			memset(out_b, 0x5a, sizeof(out_b));
			ref_v8_TONEq_generate(&obj_a, out_a);
			v8_TONEq_generate(&obj_b, out_b);
			for (i = 0; i < 4; i++) {
				diff_eq_int("sample %ld", out_b[i], out_a[i],
					    i);
				if (out_a[i] != 0)
					nonzero++;
			}
			whole(k);
		}
		diff_eq_int("a tone was generated (%ld)", nonzero > 100, 1,
			    nonzero);
	}
	rc |= diff_end();

	/*
	 * The queue functions walk pointers the transmitter set up, so the
	 * object is built by v8_txinit first rather than filled at random --
	 * a random pointer would walk off the object.
	 */
	diff_begin("v8_rxreadqueue and v8_txwritequeue");
	for (k = 0; k < 40; k++) {
		fill(&obj_a, sizeof(obj_a), 900u + k);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		ref_v8_txinit(&obj_a);
		ref_v8_txinit(&obj_b);

		for (i = 0; i < k % 7 + 1; i++) {
			diff_eq_int("rxread returns (%ld)",
				    v8_rxreadqueue(&obj_b),
				    ref_v8_rxreadqueue(&obj_a), k);
			diff_eq_int("txwrite returns (%ld)",
				    v8_txwritequeue(&obj_b),
				    ref_v8_txwritequeue(&obj_a), k);
			moved++;
		}
		normalise(offsetof(struct v8, tx_sym_a));
		normalise(offsetof(struct v8, tx_sym_b));
		normalise(offsetof(struct v8, tx_ring_base));
		normalise(offsetof(struct v8, tx_ring_half));
		whole(k);
	}
	diff_eq_int("blocks were moved (%ld)", moved > 50, 1, moved);
	rc |= diff_end();

	diff_begin("v8_fsktxfilter");
	for (k = 0; k < 24; k++) {
		fill(&obj_a, sizeof(obj_a), 1300u + k);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		for (i = 0; i < 80; i++) {
			short s = (short)((i * 811 + k * 97) % 20001 - 10000);

			diff_eq_int("sample %ld", v8_fsktxfilter(&obj_b, s),
				    ref_v8_fsktxfilter(&obj_a, s), i);
		}
		whole(k);
	}
	rc |= diff_end();

	diff_begin("v8_dftupdate");
	{
		struct v8_dft_bin ba[8], bb[8];
		short samp[16];
		int n, ns;
		long moved2 = 0;

		for (i = 0; i < 16; i++)
			samp[i] = (short)(i * 1301 - 9000);
		for (n = 0; n <= 8; n++) {
			for (ns = 0; ns <= 16; ns += 4) {
				for (i = 0; i < 8; i++) {
					ba[i].phase = bb[i].phase =
						(short)(i * 511);
					ba[i].step = bb[i].step =
						(short)(i * 97 + 13);
					ba[i].re = bb[i].re = i * 1000;
					ba[i].im = bb[i].im = -i * 700;
					ba[i].energy = bb[i].energy = 0x1234;
					ba[i].f0e = bb[i].f0e = 0x4321;
				}
				ref_v8_dftupdate(ba, (short)n, samp,
						 (short)ns);
				v8_dftupdate(bb, (short)n, samp, (short)ns);
				for (i = 0; i < 8; i++) {
					diff_eq_int("bin %ld phase", bb[i].phase,
						    ba[i].phase, i);
					diff_eq_int("bin %ld re", bb[i].re,
						    ba[i].re, i);
					diff_eq_int("bin %ld im", bb[i].im,
						    ba[i].im, i);
					if (ba[i].re != i * 1000)
						moved2++;
				}
			}
		}
		diff_eq_int("bins accumulated (%ld)", moved2 > 20, 1, moved2);
	}
	rc |= diff_end();

	diff_begin("v8_fskmodulate");
	for (k = 0; k < 24; k++) {
		int w;

		fill(&obj_a, sizeof(obj_a), 1700u + k);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		ref_v8_txinit(&obj_a);
		ref_v8_txinit(&obj_b);
		obj_a.v21_params.carrier_a = obj_b.v21_params.carrier_a =
			(short)(300 + k * 11);
		obj_a.v21_params.carrier_b = obj_b.v21_params.carrier_b =
			(short)(700 + k * 13);
		obj_a.v21_params.tx_level = obj_b.v21_params.tx_level =
			(short)(4000 + k * 100);
		for (w = 0; w <= 1; w++) {
			diff_eq_int("returns (%ld)",
				    v8_fskmodulate(&obj_b, (short)w),
				    ref_v8_fskmodulate(&obj_a, (short)w), w);
		}
		normalise(offsetof(struct v8, tx_ring_half));
		normalise(offsetof(struct v8, tx_ring_base));
		normalise(offsetof(struct v8, tx_sym_a));
		normalise(offsetof(struct v8, tx_sym_b));
		whole(k);
	}
	rc |= diff_end();

	diff_begin("v8_agcadapt");
	for (k = 0; k < 400; k++) {
		fill(&obj_a, sizeof(obj_a), 2100u + k);
		memcpy(&obj_b, &obj_a, sizeof(obj_a));
		/* Sweep the two dead bands and the armed flag. */
		obj_a.rx.level = obj_b.rx.level = (short)(k * 163 - 32000);
		obj_a.rx.energy_hi = obj_b.rx.energy_hi = (short)(k * 71);
		obj_a.rx.accum = obj_b.rx.accum = (short)(k * 37 - 1200);
		obj_a.rx.adapt_rate = obj_b.rx.adapt_rate = (short)(k * 29 - 4000);
		obj_a.rx.gain = obj_b.rx.gain = (short)(k * 211);
		obj_a.rx.flags = obj_b.rx.flags =
			(unsigned short)(k & 1 ? V8_RX_DETECTOR_ARMED : 0);
		diff_eq_int("returns (%ld)", v8_agcadapt(&obj_b),
			    ref_v8_agcadapt(&obj_a), k);
		whole(k);
	}
	rc |= diff_end();

	diff_begin("v8_ansamgenerate");
	{
		short out_a[4], out_b[4];
		int nonzero = 0, reversals = 0;

		for (k = 0; k < 32; k++) {
			fill(&obj_a, sizeof(obj_a), 2600u + k);
			memcpy(&obj_b, &obj_a, sizeof(obj_a));
			ref_v8_txinit(&obj_a);
			ref_v8_txinit(&obj_b);
			ref_v8_ansaminit(&obj_a);
			ref_v8_ansaminit(&obj_b);
			obj_a.tone.amplitude = obj_b.tone.amplitude = (short)(8000 - k * 5);
			/*
			 * Land on the reversal boundary in some runs, and
			 * make sure the enable is set in exactly those --
			 * the first version gated the enable on odd k and
			 * the boundary on even, so no reversal could ever
			 * happen and the guard caught it.
			 */
			obj_a.tone.reversal_enable = obj_b.tone.reversal_enable =
				(short)(k % 4 == 0 ? 1 : (k & 1));
			obj_a.tone.reversal_count = obj_b.tone.reversal_count =
				(short)(k % 4 == 0 ? 0x437 : k * 13);

			for (i = 0; i < 6; i++) {
				short before = obj_a.tone.amplitude;

				memset(out_a, 0x5a, sizeof(out_a));
				memset(out_b, 0x5a, sizeof(out_b));
				ref_v8_ansamgenerate(&obj_a, out_a);
				v8_ansamgenerate(&obj_b, out_b);
				for (n = 0; n < 4; n++) {
					diff_eq_int("sample %ld", out_b[n],
						    out_a[n], n);
					if (out_a[n] != 0)
						nonzero++;
				}
				if (obj_a.tone.amplitude != before)
					reversals++;
			}
			normalise(offsetof(struct v8, tx_ring_half));
			normalise(offsetof(struct v8, tx_ring_base));
			normalise(offsetof(struct v8, tx_sym_a));
			normalise(offsetof(struct v8, tx_sym_b));
			whole(k);
		}
		diff_eq_int("a tone came out (%ld)", nonzero > 100, 1,
			    nonzero);
		diff_eq_int("the phase reversed (%ld)", reversals > 0, 1,
			    reversals);
	}
	rc |= diff_end();

	diff_begin("V8Control");
	{
		long accepted = 0;
		int what;

		for (k = 0; k < 64; k++) {
			for (what = -1; what <= 3; what++) {
				fill(&obj_a, sizeof(obj_a), 3100u + k);
				memcpy(&obj_b, &obj_a, sizeof(obj_a));
				/* Sweep the states each request needs. */
				obj_a.side = obj_b.side = k & 1;
				obj_a.rx_state = obj_b.rx_state =
					(short)(k & 2 ? 0x19 : 0x18);
				obj_a.cm_ready = obj_b.cm_ready = (short)(k & 4);
				obj_a.rx_substate = obj_b.rx_substate =
					(short)(k & 8 ? 0x32
						      : k & 16 ? 0x33 : 0x30);
				n = V8Control(&obj_b, what);
				diff_eq_int("returns (%ld)", n,
					    ref_V8Control(&obj_a, what), what);
				whole(k);
				if (n == 0)
					accepted++;
			}
		}
		diff_eq_int("requests were accepted (%ld)", accepted > 10, 1,
			    accepted);
	}
	rc |= diff_end();

	diff_begin("v8_getbit");
	{
		static struct v8_cm cm_a, cm_b;
		long bits = 0, ends = 0, ones = 0;

		for (k = 0; k < 48; k++) {
			struct v8_tx_sequence *sa, *sb;

			memset(&obj_a, 0, sizeof(obj_a));
			memset(&obj_b, 0, sizeof(obj_b));
			memset(&cm_a, 0, sizeof(cm_a));
			cm_a.b0 = (unsigned char)(k * 5);
			cm_a.b1 = (unsigned char)(k * 11);
			cm_a.b2 = (unsigned char)(k & 3 ? 0x04 : 0);
			cm_a.ext1[0] = 'G';
			memcpy(&cm_b, &cm_a, sizeof(cm_a));

			obj_a.cm = &cm_a;
			obj_b.cm = &cm_b;
			obj_a.tx_seq = &obj_a.seq[0];
			obj_b.tx_seq = &obj_b.seq[0];
			ref_initTxSequence(&obj_a);
			initTxSequence(&obj_b);
			sa = &obj_a.seq[0];
			sb = &obj_b.seq[0];

			/* Sweep the CRC and repeat switches. */
			sa->crc_enable = sb->crc_enable = (short)(k & 1);
			sa->repeat = sb->repeat = (short)(k & 2 ? 1 : 0);

			/* Well past the end, so the tail cases all run. */
			for (i = 0; i < 400; i++) {
				int ba = ref_v8_getbit(sa);
				int bb = v8_getbit(sb);

				diff_eq_int("bit %ld", bb, ba, i);
				diff_eq_int("crc after %ld", sb->crc, sa->crc,
					    i);
				diff_eq_int("nleft after %ld", sb->nleft,
					    sa->nleft, i);
				diff_eq_int("bitpos after %ld", sb->bitpos,
					    sa->bitpos, i);
				if (ba == V8_GETBIT_END)
					ends++;
				else {
					bits++;
					if (ba == 1)
						ones++;
				}
			}
			diff_eq_int("sequence state (%ld)",
				    memcmp(sa, sb, sizeof(*sa)) == 0, 1, k);
		}
		diff_eq_int("bits came out (%ld)", bits > 1000, 1, bits);
		diff_eq_int("both values appeared (%ld)",
			    ones > 0 && ones < bits, 1, ones);
		diff_eq_int("the end was reached (%ld)", ends > 0, 1, ends);
	}
	rc |= diff_end();

	/*
	 * The detector, fed real ANSam: the generator's own output, so the
	 * signal has the phase reversals the detector exists to find.
	 */
	diff_begin("v8_phase_rev_detect");
	{
		static struct v8_phase_rev pa, pb;
		static short air[4096];
		long detected = 0, reversals = 0;

		for (k = 0; k < 6; k++) {
			int blk;

			/* Generate a stretch of ANSam into `air`. */
			fill(&obj_a, sizeof(obj_a), 5000u + k);
			ref_v8_txinit(&obj_a);
			ref_v8_ansaminit(&obj_a);
			obj_a.tone.amplitude = (short)(6000 + k * 400);
			obj_a.tone.reversal_enable = 1;
			/* Start near a reversal so several happen. */
			obj_a.tone.reversal_count = (short)(0x430 - k);
			for (blk = 0; blk < 1024; blk++)
				ref_v8_ansamgenerate(&obj_a, air + blk * 4);

			memset(&pa, 0, sizeof(pa));
			memset(&pb, 0, sizeof(pb));
			ref_v8_phase_rev_init(&pa);
			ref_v8_phase_rev_init(&pb);

			for (blk = 0; blk < 4096; blk += 64) {
				ref_v8_phase_rev_detect(&pa, air + blk, 64);
				v8_phase_rev_detect(&pb, air + blk, 64);
				diff_eq_int("corr (%ld)", pb.corr, pa.corr, k);
				diff_eq_int("energy (%ld)", pb.energy,
					    pa.energy, k);
				diff_eq_int("smoothed (%ld)", pb.smoothed,
					    pa.smoothed, k);
				diff_eq_int("run (%ld)", pb.run, pa.run, k);
				diff_eq_int("detected (%ld)", pb.detected,
					    pa.detected, k);
			}
			diff_eq_int("state (%ld)",
				    memcmp(&pa, &pb, sizeof(pa)) == 0, 1, k);
			if (pa.detected)
				detected++;
			reversals += pa.reversals;
		}
		/*
		 * Anti-vacuity: the detector must actually have seen
		 * reversals in the generator's output, or it agreed with
		 * itself about silence.
		 */
		diff_eq_int("reversals were seen (%ld)", reversals > 0, 1,
			    reversals);
		(void)detected;
	}
	rc |= diff_end();

	diff_begin("v8_tone_detect");
	{
		static short air_a[512], air_b[512];
		static const short tab[8] = {
			15000, -9000, 14000, -8000, -7000, 13000, -6000, 12000
		};
		long asserted = 0, moved3 = 0;

		for (k = 0; k < 12; k++) {
			int blk;

			/* ANSam again, so the detector sees a real tone. */
			fill(&obj_a, sizeof(obj_a), 6100u + k);
			ref_v8_txinit(&obj_a);
			ref_v8_ansaminit(&obj_a);
			obj_a.tone.amplitude = (short)(9000 - k * 300);
			obj_a.tone.reversal_enable = 0;
			for (blk = 0; blk < 128; blk++)
				ref_v8_ansamgenerate(&obj_a, air_a + blk * 4);
			memcpy(air_b, air_a, sizeof(air_a));

			memset(&obj_a, 0, sizeof(obj_a));
			memset(&obj_b, 0, sizeof(obj_b));
			ref_v8_detectorinit(&obj_a, &obj_a.detector, tab, 0,
					    100, 50, 1500, 0);
			ref_v8_detectorinit(&obj_b, &obj_b.detector, tab, 0,
					    100, 50, 1500, 0);
			/* Sweep the three rules the verdict can follow. */
			obj_a.detector.lo_rule = obj_b.detector.lo_rule =
				(short)(k % 3 == 0);
			obj_a.detector.armed = obj_b.detector.armed =
				(short)(k % 3 == 1);
			/*
			 * Thresholds that let each rule actually fire.  The
			 * first version used 400 and 200 for both, which for
			 * the f04 rule means every level either increments
			 * the counter and resets it, or does neither -- so it
			 * could never reach the count and the guard caught
			 * it.
			 */
			if (k % 3 == 0) {
				obj_a.detector.lo_thresh = obj_b.detector.lo_thresh =
					30000;
				obj_a.detector.hi_thresh = obj_b.detector.hi_thresh =
					30000;
			} else {
				obj_a.detector.lo_thresh = obj_b.detector.lo_thresh = 400;
				obj_a.detector.hi_thresh = obj_b.detector.hi_thresh = 5;
			}
			obj_a.detector.count_limit = obj_b.detector.count_limit = 3;
			/*
			 * v8_detectorinit seeds the counter with the negated
			 * argument -- here -50 -- which is a deliberate
			 * warm-up: the detector cannot assert for fifty
			 * blocks however loud the tone.  Cleared so the
			 * verdict is reachable in a test of this length.
			 */
			obj_a.detector.counter = obj_b.detector.counter = 0;

			for (blk = 0; blk + 64 <= 512; blk += 64) {
				int ra, rb;

				obj_a.rx.buf = air_a + blk + 64;
				obj_b.rx.buf = air_b + blk + 64;
				ra = ref_v8_tone_detect(&obj_a,
							&obj_a.detector,
							air_a + blk);
				rb = v8_tone_detect(&obj_b, &obj_b.detector,
						    air_b + blk);
				diff_eq_int("verdict (%ld)", rb, ra, k);
				diff_eq_int("integrator (%ld)",
					    obj_b.detector.integrator,
					    obj_a.detector.integrator, k);
				for (i = 0; i < 64; i++)
					diff_eq_int("filtered %ld",
						    air_b[blk + i],
						    air_a[blk + i], i);
				if (ra)
					asserted++;
				if (obj_a.detector.integrator != 0)
					moved3++;
			}
			diff_eq_int("detector state (%ld)",
				    memcmp(&obj_a.detector, &obj_b.detector,
					   sizeof(obj_a.detector)) == 0, 1, k);
		}
		diff_eq_int("the integrator moved (%ld)", moved3 > 10, 1,
			    moved3);
		diff_eq_int("the detector asserted (%ld)", asserted > 0, 1,
			    asserted);
	}
	rc |= diff_end();

	/*
	 * The same two filter sections as standalone functions.  Nothing in
	 * the object calls either, so this is the only thing that ever runs
	 * them; they get their own group because they are not quite what
	 * v8_tone_detect has inlined -- each product is truncated to a short
	 * before it is accumulated.
	 */
	diff_begin("notch_filter and biquad_filter");
	{
		static const short tab[8] = {
			15000, -9000, 14000, -8000, -7000, 13000, -6000, 12000
		};
		long moved = 0;

		for (k = 0; k < 16; k++) {
			int n;

			fill(&obj_a, sizeof(obj_a), 8800u + k);
			memcpy(&obj_b, &obj_a, sizeof(obj_a));

			for (n = 0; n < 200; n++) {
				short x = (short)((n * 977 + k * 313) % 20001
						  - 10000);
				short in_a = x, in_b = x;
				int ra, rb;

				ra = ref_notch_filter(&in_a, &obj_a.detector);
				rb = notch_filter(&in_b, &obj_b.detector);
				diff_eq_int("notch (%ld)", rb, ra, (long)n);

				ra = ref_biquad_filter((short)ra,
						       &obj_a.detector, tab);
				rb = biquad_filter((short)rb,
						   &obj_b.detector, tab);
				diff_eq_int("biquad (%ld)", rb, ra, (long)n);
				if (ra != 0)
					moved++;
			}
			diff_eq_int("detector state (%ld)",
				    memcmp(&obj_a.detector, &obj_b.detector,
					   sizeof(obj_a.detector)) == 0, 1, k);
		}
		diff_eq_int("the sections produced something (%ld)",
			    moved > 100, 1, moved);
	}
	rc |= diff_end();

	/*
	 * The demodulator, fed the modulator's own output so the correlator
	 * sees a real V.21 signal rather than noise.
	 */
	diff_begin("v8_fskdemodulate");
	{
		long bits = 0;

		for (k = 0; k < 16; k++) {
			int blk;

			fill(&obj_a, sizeof(obj_a), 7700u + k);
			memcpy(&obj_b, &obj_a, sizeof(obj_a));
			ref_v8_txinit(&obj_a);
			ref_v8_txinit(&obj_b);
			ref_v8_V21_Init(&obj_a, (short)(k & 1),
					(short)((k >> 1) & 1));
			ref_v8_V21_Init(&obj_b, (short)(k & 1),
					(short)((k >> 1) & 1));
			obj_a.v21_params.inbuf_pos = obj_b.v21_params.inbuf_pos = 0;
			obj_a.v21.pos = obj_b.v21.pos = 0;
			obj_a.v21.mark_run = obj_b.v21.mark_run = 0;
			obj_a.v21.space_run = obj_b.v21.space_run = 0;

			for (blk = 0; blk < 60; blk++) {
				/* Alternate mark and space every few blocks. */
				short w = (short)((blk / 5) & 1);
				int m;

				ref_v8_fskmodulate(&obj_a, w);
				ref_v8_fskmodulate(&obj_b, w);
				/* The modulator wrote the ring; feed the
				 * staging buffer from it so both sides see
				 * identical input. */
				for (m = 0; m < 4; m++) {
					obj_a.rx_stage[m] = obj_a.tx_stage[m];
					obj_b.rx_stage[m] = obj_b.tx_stage[m];
				}
				ref_v8_fskdemodulate(&obj_a);
				v8_fskdemodulate(&obj_b);

				diff_eq_int("bitcount (%ld)",
					    obj_b.v21_params.bitcount,
					    obj_a.v21_params.bitcount, k);
				diff_eq_int("bits (%ld)", obj_b.v21_params.bits,
					    obj_a.v21_params.bits, k);
				diff_eq_int("mark run (%ld)", obj_b.v21.mark_run,
					    obj_a.v21.mark_run, k);
				diff_eq_int("space run (%ld)",
					    obj_b.v21.space_run,
					    obj_a.v21.space_run, k);
				for (i = 0; i < V8_V21_DELAY; i++)
					diff_eq_int("tap %ld",
						    obj_b.v21.delay[i],
						    obj_a.v21.delay[i], i);
			}
			bits += obj_a.v21_params.bitcount;
			normalise(offsetof(struct v8, tx_ring_half));
			normalise(offsetof(struct v8, tx_ring_base));
			normalise(offsetof(struct v8, tx_sym_a));
			normalise(offsetof(struct v8, tx_sym_b));
			compare_v21();
			whole(k);
		}
		diff_eq_int("bits were recovered (%ld)", bits > 0, 1, bits);
	}
	rc |= diff_end();

	diff_begin("V8agc");
	{
		long adapted = 0, clipped = 0;

		for (k = 0; k < 24; k++) {
			int blk;

			fill(&obj_a, sizeof(obj_a), 8800u + k);
			memcpy(&obj_b, &obj_a, sizeof(obj_a));
			ref_v8_txinit(&obj_a);
			ref_v8_txinit(&obj_b);
			ref_v8_rxinit(&obj_a);
			ref_v8_rxinit(&obj_b);
			obj_a.side = obj_b.side = k & 1;
			/* Sweep the gain across the saturating range. */
			obj_a.rx.gain = obj_b.rx.gain = (short)(200 + k * 1300);
			obj_a.rx.level = obj_b.rx.level = (short)(k * 900 - 8000);
			obj_a.rx.accum = obj_b.rx.accum = (short)(k * 41);
			obj_a.rx.adapt_rate = obj_b.rx.adapt_rate = (short)(k * 133 - 1000);
			obj_a.rx.flags = obj_b.rx.flags =
				(unsigned short)(k & 2 ? V8_RX_DETECTOR_ARMED
						       : 0);
			obj_a.rx_substate = obj_b.rx_substate = (short)(k & 4 ? 0x24 : 0x19);

			for (blk = 0; blk < 40; blk++) {
				int m;
				short lvl = (short)(3000 + k * 900);

				/* Something with energy in the passband. */
				for (m = 0; m < V8_TX_SYMBOLS; m++) {
					short x = (short)((m & 4) ? lvl
							          : -lvl);
					obj_a.tx_symbols[m] = x;
					obj_b.tx_symbols[m] = x;
				}
				diff_eq_int("returns (%ld)", V8agc(&obj_b),
					    ref_V8agc(&obj_a), k);
				for (m = 0; m < V8_QUEUE_BLOCK; m++)
					diff_eq_int("sample %ld",
						    obj_b.rx_stage[m],
						    obj_a.rx_stage[m], m);
				diff_eq_int("gain (%ld)", obj_b.rx.gain,
					    obj_a.rx.gain, k);
				diff_eq_int("clip count (%ld)", obj_b.rx.clip_count,
					    obj_a.rx.clip_count, k);
				if (obj_a.rx.clip_count != 0)
					clipped++;
				if (obj_a.rx.gain != (short)(200 + k * 1300))
					adapted++;
			}
			normalise(offsetof(struct v8, tx_sym_a));
			normalise(offsetof(struct v8, tx_sym_b));
			normalise(offsetof(struct v8, tx_ring_base));
			normalise(offsetof(struct v8, tx_ring_half));
			normalise(offsetof(struct v8, rx)
				  + offsetof(struct v8_rx, buf));
			whole(k);
		}
		diff_eq_int("the gain moved (%ld)", adapted > 0, 1, adapted);
		diff_eq_int("saturation happened (%ld)", clipped > 0, 1,
			    clipped);
	}
	rc |= diff_end();

	diff_begin("checkSignalStability");
	{
		long stable = 0, reset = 0;

		for (k = 0; k < 40; k++) {
			int step;

			fill(&obj_a, sizeof(obj_a), 9600u + k);
			memcpy(&obj_b, &obj_a, sizeof(obj_a));
			obj_a.rx.refresh_timer = obj_b.rx.refresh_timer = (short)(k * 25);
			obj_a.rx.gain_ref = obj_b.rx.gain_ref = (short)(400 + k * 90);
			obj_a.rx.stable_timer = obj_b.rx.stable_timer = (short)(k * 24);
			obj_a.rx.stable = obj_b.rx.stable = 0;
			obj_a.rx.gain = obj_b.rx.gain = (short)(400 + k * 90);

			/*
			 * Hold the gain steady for a while, then move it
			 * enough to break the tolerance, so both the settling
			 * and the reset paths run.
			 */
			for (step = 0; step < 400; step++) {
				if (step == 300) {
					obj_a.rx.gain = (short)(obj_a.rx.gain * 2);
					obj_b.rx.gain = obj_a.rx.gain;
				}
				ref_checkSignalStability(&obj_a);
				checkSignalStability(&obj_b);
				diff_eq_int("f84 (%ld)", obj_b.rx.refresh_timer,
					    obj_a.rx.refresh_timer, k);
				diff_eq_int("f86 (%ld)", obj_b.rx.gain_ref,
					    obj_a.rx.gain_ref, k);
				diff_eq_int("f88 (%ld)", obj_b.rx.stable_timer,
					    obj_a.rx.stable_timer, k);
				diff_eq_int("stable (%ld)", obj_b.rx.stable,
					    obj_a.rx.stable, k);
				if (obj_a.rx.stable)
					stable++;
				if (obj_a.rx.stable_timer == 0)
					reset++;
			}
			whole(k);
		}
		diff_eq_int("the line settled (%ld)", stable > 0, 1, stable);
		diff_eq_int("and was reset (%ld)", reset > 0, 1, reset);
	}
	rc |= diff_end();

	rc |= t_sig_trace();
	return rc;
}
