/*
 * t_v8sig.c -- differential test of the handshake's signal plumbing.
 *
 * All five take the whole object and write scattered parts of it, so the
 * comparison is the whole object every time, with the pointers the queue
 * functions move normalised to offsets first.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "harness.h"
#include "dsplib/v8.h"

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
		obj_a.fa42 = obj_b.fa42 = (short)(k * 4001 - 16000);
		ref_v8_ansaminit(&obj_a);
		v8_ansaminit(&obj_b);
		whole(k);
	}
	diff_eq_int("the scaled field was set", obj_b.tone.f0e, 1, 0);
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
		obj_a.v21_params.f0a = obj_b.v21_params.f0a =
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
		obj_a.rx.f1a = obj_b.rx.f1a = (short)(k * 163 - 32000);
		obj_a.rx.f16 = obj_b.rx.f16 = (short)(k * 71);
		obj_a.rx.f1e = obj_b.rx.f1e = (short)(k * 37 - 1200);
		obj_a.rx.f20 = obj_b.rx.f20 = (short)(k * 29 - 4000);
		obj_a.rx.f1c = obj_b.rx.f1c = (short)(k * 211);
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
			obj_a.tone.f08 = obj_b.tone.f08 = (short)(8000 - k * 5);
			/*
			 * Land on the reversal boundary in some runs, and
			 * make sure the enable is set in exactly those --
			 * the first version gated the enable on odd k and
			 * the boundary on even, so no reversal could ever
			 * happen and the guard caught it.
			 */
			obj_a.tone.f0e = obj_b.tone.f0e =
				(short)(k % 4 == 0 ? 1 : (k & 1));
			obj_a.tone.f0a = obj_b.tone.f0a =
				(short)(k % 4 == 0 ? 0x437 : k * 13);

			for (i = 0; i < 6; i++) {
				short before = obj_a.tone.f08;

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
				if (obj_a.tone.f08 != before)
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
				obj_a.mode = obj_b.mode = k & 1;
				obj_a.f9d6 = obj_b.f9d6 =
					(short)(k & 2 ? 0x19 : 0x18);
				obj_a.fdbe = obj_b.fdbe = (short)(k & 4);
				obj_a.f9d8 = obj_b.f9d8 =
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
			obj_a.tone.f08 = (short)(6000 + k * 400);
			obj_a.tone.f0e = 1;
			/* Start near a reversal so several happen. */
			obj_a.tone.f0a = (short)(0x430 - k);
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
			obj_a.tone.f08 = (short)(9000 - k * 300);
			obj_a.tone.f0e = 0;
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
			obj_a.detector.f04 = obj_b.detector.f04 =
				(short)(k % 3 == 0);
			obj_a.detector.f06 = obj_b.detector.f06 =
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
				obj_a.detector.f0e = obj_b.detector.f0e =
					30000;
				obj_a.detector.f10 = obj_b.detector.f10 =
					30000;
			} else {
				obj_a.detector.f0e = obj_b.detector.f0e = 400;
				obj_a.detector.f10 = obj_b.detector.f10 = 5;
			}
			obj_a.detector.f0a = obj_b.detector.f0a = 3;
			/*
			 * v8_detectorinit seeds the counter with the negated
			 * argument -- here -50 -- which is a deliberate
			 * warm-up: the detector cannot assert for fifty
			 * blocks however loud the tone.  Cleared so the
			 * verdict is reachable in a test of this length.
			 */
			obj_a.detector.f08 = obj_b.detector.f08 = 0;

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
					    obj_b.detector.f12,
					    obj_a.detector.f12, k);
				for (i = 0; i < 64; i++)
					diff_eq_int("filtered %ld",
						    air_b[blk + i],
						    air_a[blk + i], i);
				if (ra)
					asserted++;
				if (obj_a.detector.f12 != 0)
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
			obj_a.v21_params.f16 = obj_b.v21_params.f16 = 0;
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
					    obj_b.v21_params.f18,
					    obj_a.v21_params.f18, k);
				diff_eq_int("bits (%ld)", obj_b.v21_params.f1a,
					    obj_a.v21_params.f1a, k);
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
			bits += obj_a.v21_params.f18;
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
			obj_a.mode = obj_b.mode = k & 1;
			/* Sweep the gain across the saturating range. */
			obj_a.rx.f1c = obj_b.rx.f1c = (short)(200 + k * 1300);
			obj_a.rx.f1a = obj_b.rx.f1a = (short)(k * 900 - 8000);
			obj_a.rx.f1e = obj_b.rx.f1e = (short)(k * 41);
			obj_a.rx.f20 = obj_b.rx.f20 = (short)(k * 133 - 1000);
			obj_a.rx.flags = obj_b.rx.flags =
				(unsigned short)(k & 2 ? V8_RX_DETECTOR_ARMED
						       : 0);
			obj_a.f9d8 = obj_b.f9d8 = (short)(k & 4 ? 0x24 : 0x19);

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
				diff_eq_int("gain (%ld)", obj_b.rx.f1c,
					    obj_a.rx.f1c, k);
				diff_eq_int("clip count (%ld)", obj_b.rx.fac,
					    obj_a.rx.fac, k);
				if (obj_a.rx.fac != 0)
					clipped++;
				if (obj_a.rx.f1c != (short)(200 + k * 1300))
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
			obj_a.rx.f84 = obj_b.rx.f84 = (short)(k * 25);
			obj_a.rx.f86 = obj_b.rx.f86 = (short)(400 + k * 90);
			obj_a.rx.f88 = obj_b.rx.f88 = (short)(k * 24);
			obj_a.rx.f8a = obj_b.rx.f8a = 0;
			obj_a.rx.f1c = obj_b.rx.f1c = (short)(400 + k * 90);

			/*
			 * Hold the gain steady for a while, then move it
			 * enough to break the tolerance, so both the settling
			 * and the reset paths run.
			 */
			for (step = 0; step < 400; step++) {
				if (step == 300) {
					obj_a.rx.f1c = (short)(obj_a.rx.f1c * 2);
					obj_b.rx.f1c = obj_a.rx.f1c;
				}
				ref_checkSignalStability(&obj_a);
				checkSignalStability(&obj_b);
				diff_eq_int("f84 (%ld)", obj_b.rx.f84,
					    obj_a.rx.f84, k);
				diff_eq_int("f86 (%ld)", obj_b.rx.f86,
					    obj_a.rx.f86, k);
				diff_eq_int("f88 (%ld)", obj_b.rx.f88,
					    obj_a.rx.f88, k);
				diff_eq_int("stable (%ld)", obj_b.rx.f8a,
					    obj_a.rx.f8a, k);
				if (obj_a.rx.f8a)
					stable++;
				if (obj_a.rx.f88 == 0)
					reset++;
			}
			whole(k);
		}
		diff_eq_int("the line settled (%ld)", stable > 0, 1, stable);
		diff_eq_int("and was reset (%ld)", reset > 0, 1, reset);
	}
	rc |= diff_end();

	return rc;
}
