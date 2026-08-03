/*
 * t_v34rx.c -- differential test of the V.34 receiver/transmitter cores.
 */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "dsplib/debug.h"

extern unsigned int ref_dsplibs_debug_level;
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"
#include "dsplib/v34shell.h"
#include "dsplib/v34pcmif.h"

static void shell_sink_a(void *s, int v, int n) { (void)s;(void)v;(void)n; }
static void shell_sink_b(void *s, int v, int n) { (void)s;(void)v;(void)n; }

extern void ref_rxreadqueue(void *q);
extern void ref_txwritequeue(void *q, const short *src);
extern int ref_bitreverse(unsigned short v, short nbits);
extern void ref_decision(void *d, const int *pts, short npts);
extern void ref_V34nlencoder(const short *in, short *out);
extern void ref_updateAlpha(short *a, int e, int d, int g, int dec,
			    const char *t);
extern int ref_V34descrambler(void *s, short bits, short nbits);
extern void ref_txinit(void *obj);
extern int ref_agcadapt(void *a);
extern void ref_rxtiminginit(void *obj);
extern void ref_rxinit(void *obj);
extern void ref_txmit(void *obj);
extern void ref_V34agc(void *rx);
extern void ref_rxtiming(void *obj);
extern void ref_txrxdmainit(short *dst, const short *src);
extern void ref_V34SetupDemodulator(void *obj, short baud, short carrier);
extern void ref_v34FreezeEcho(void *obj);
extern int ref_adaptecho(void *obj);
extern int ref_modem_serrint(void *obj);
extern void ref_decoderv34(void *obj);
extern int ref_polyValue(short k);
extern void ref_setInitialPhase(void *obj);
extern void ref_setTimingStateParameters(void *obj);
extern void ref_TimingV34(void *obj);
extern void ref_receiver(void *obj);
extern void ref_VPcmV34LogTimingOffset(void *obj, short v);
extern int ref_V34scrambler(unsigned *sr, short mode, short bits, short nbits);
extern void ref_V34SetupModulator(void *m, short b, short c, short p, int a,
				  int r);

/* Big enough for the larger of the two rings, plus the output slot. */
union qbuf { struct v34_queue q; unsigned char raw[0x400]; };
static union qbuf qa, qb;

static void
compare(const char *what, int tag)
{
	unsigned i;

	for (i = 0; i < sizeof(qa); i++) {
		/* The two cursors are addresses; compare as offsets. */
		if (i >= __builtin_offsetof(struct v34_queue, rd)
		    && i < __builtin_offsetof(struct v34_queue, ring))
			continue;
		diff_eq_int(what, qa.raw[i], qb.raw[i], (long)i * 100 + tag);
	}
	diff_eq_int("rd offset", (char *)qa.q.rd - (char *)&qa,
		    (char *)qb.q.rd - (char *)&qb, tag);
	diff_eq_int("wr offset", (char *)qa.q.wr - (char *)&qa,
		    (char *)qb.q.wr - (char *)&qb, tag);
}

int
main(void)
{
	int rc = 0, i, start;

	diff_begin("v34 bitreverse");
	for (i = 0; i <= 20; i++) {
		int v;

		for (v = 0; v < 0x10000; v += 37)
			diff_eq_int("bitreverse", bitreverse((unsigned short)v,
							     (short)i),
				    ref_bitreverse((unsigned short)v,
						   (short)i),
				    (long)i * 100000 + v);
	}
	rc |= diff_end();

	diff_begin("v34 rxreadqueue");
	/* Every start position, so the ring wrap is crossed from each side. */
	for (start = 0; start < 64; start++) {
		unsigned k;

		memset(&qa, HARNESS_MALLOC_FILL, sizeof(qa));
		memset(&qb, HARNESS_MALLOC_FILL, sizeof(qb));
		for (k = 0; k < 64; k++)
			qa.q.ring[k] = qb.q.ring[k] = (int)(k * 7919 + 13);
		qa.q.count = qb.q.count = 200;
		qa.q.rd = qa.q.ring + start;
		qb.q.rd = qb.q.ring + start;
		qa.q.wr = qa.q.ring;
		qb.q.wr = qb.q.ring;
		for (i = 0; i < 40; i++) {
			rxreadqueue(&qa.q);
			ref_rxreadqueue(&qb.q);
			compare("after rxreadqueue", start * 100 + i);
		}
	}
	rc |= diff_end();

	diff_begin("v34 txwritequeue");
	for (start = 0; start < 158; start += 7) {
		static short src[4];
		unsigned k;

		memset(&qa, HARNESS_MALLOC_FILL, sizeof(qa));
		memset(&qb, HARNESS_MALLOC_FILL, sizeof(qb));
		for (k = 0; k < 158; k++)
			qa.q.ring[k] = qb.q.ring[k] = (int)(k * 4409 + 7);
		qa.q.count = qb.q.count = 0;
		qa.q.wr = qa.q.ring + start;
		qb.q.wr = qb.q.ring + start;
		qa.q.rd = qa.q.ring;
		qb.q.rd = qb.q.ring;
		for (i = 0; i < 60; i++) {
			src[0] = (short)(i * 311 - 4000);
			src[1] = (short)(i * 733 - 9000);
			src[2] = (short)(-i * 101);
			src[3] = (short)(i * 17 + 5);
			txwritequeue(&qa.q, src);
			ref_txwritequeue(&qb.q, src);
			compare("after txwritequeue", start * 100 + i);
		}
	}
	rc |= diff_end();

	diff_begin("v34 nlencoder");
	{
		int re, im;

		for (re = -32768; re < 32768; re += 331)
		for (im = -20000; im < 20000; im += 4441) {
			short in[2], oa[2], ob[2];

			in[0] = (short)re; in[1] = (short)im;
			oa[0] = oa[1] = 0x5a5a;
			ob[0] = ob[1] = 0x5a5a;
			V34nlencoder(in, oa);
			ref_V34nlencoder(in, ob);
			diff_eq_int("nlenc re", oa[0], ob[0],
				    (long)re * 100000 + im);
			diff_eq_int("nlenc im", oa[1], ob[1],
				    (long)re * 100000 + im);
		}
	}
	rc |= diff_end();

	diff_begin("v34 decision");
	{
		static struct v34_receiver da, db;
		static int pts[64];
		int n, t;

		for (i = 0; i < 64; i++)
			pts[i] = (int)(((unsigned)(short)(i * 719 - 12000)
					<< 16)
				       | (unsigned short)(short)(i * 431
								 - 9000));

		for (n = 1; n <= 64; n += 3)
		for (t = -30000; t < 30000; t += 2711) {
			memset(&da, HARNESS_MALLOC_FILL, sizeof(da));
			memset(&db, HARNESS_MALLOC_FILL, sizeof(db));
			da.target_re = db.target_re = (short)t;
			da.target_im = db.target_im = (short)(-t / 3);
			decision(&da, pts, (short)n);
			ref_decision(&db, pts, (short)n);
			diff_eq_int("best index", da.best_index,
				    db.best_index, (long)n * 100000 + t);
			diff_eq_int("best point", da.dp.point,
				    db.dp.point, (long)n * 100000 + t);
			for (i = 0; i < (int)sizeof(da); i++)
				diff_eq_int("decoder state",
					    ((unsigned char *)&da)[i],
					    ((unsigned char *)&db)[i], i);
		}
		/* npts of zero: the loop must not run and best must be pts[0]. */
		memset(&da, HARNESS_MALLOC_FILL, sizeof(da));
		memset(&db, HARNESS_MALLOC_FILL, sizeof(db));
		da.target_re = db.target_re = 1234;
		da.target_im = db.target_im = -567;
		decision(&da, pts, 0);
		ref_decision(&db, pts, 0);
		diff_eq_int("zero points, index", da.best_index,
			    db.best_index, 0);
		diff_eq_int("zero points, point", da.dp.point,
			    db.dp.point, 0);
	}
	rc |= diff_end();

	diff_begin("v34 updateAlpha");
	{
		/*
		 * Non-negative energies only, plus the two negatives that are
		 * safe.  `energy = -1` is NOT in the domain: it already has
		 * bit 30 set so the normalising loop does not run, and
		 * (-1 + 0x8000) >> 16 is zero, so the divide faults -- in the
		 * blob exactly as in the reconstruction.  Driving it proves
		 * nothing except that both sides use `idiv`.
		 */
		static const int energies[] = { 0, 1, 2, 3, 255, 256, 32767,
						65536, 0x100000, 0x3fffffff,
						0x40000000, 0x7fffffff,
						-65536, -0x40000000 };
		unsigned e;
		int g, dc, ap;

		for (e = 0; e < sizeof(energies) / sizeof(energies[0]); e++)
		for (g = -32768; g <= 32767; g += 9973)
		for (dc = 0; dc <= 32767; dc += 11311)
		for (ap = 0; ap <= 1; ap++) {
			short aa = 1234, ab = 1234;

			updateAlpha(&aa, energies[e], ap, g, dc, "NE");
			ref_updateAlpha(&ab, energies[e], ap, g, dc, "NE");
			diff_eq_int("alpha", aa, ab,
				    (long)energies[e] * 31 + g);
		}
	}
	rc |= diff_end();

	/*
	 * The DIAGNOSTIC path, which nothing had ever exercised.  Both sides
	 * gate on their own debug level and print through their own hook, so
	 * with capture on the two transcripts can be compared directly.
	 *
	 * This is the test that was missing: updateAlpha's message had the
	 * wrong format string, the wrong argument count and the wrong types,
	 * and no differential run could see it because dsplibs_debug_level is
	 * zero everywhere.  See finding 126.
	 */
	diff_begin("v34 updateAlpha debug transcript");
	{
		/*
		 * No small negative energies here: they divide by zero in
		 * the original as well as in the reconstruction, and the
		 * quantity is a sum of squares, so reaching one means the
		 * estimate has already overflowed.  Finding 127.
		 */
		static const int energies[] = { 0, 1, 0x4000, 0x1000000,
						0x40000000, 0x7fffffff,
						-65536 };
		short aa, ab;
		unsigned e;
		int ap, g;

		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		for (e = 0; e < sizeof(energies) / sizeof(energies[0]); e++)
		for (ap = 0; ap <= 1; ap++)
		for (g = 0x1000; g <= 0x7000; g += 0x3000) {
			dsplib_debug_capture_reset();
			aa = ab = (short)(0x1234 + (int)e * 977);
			updateAlpha(&aa, energies[e], ap, g, 0x4000, "NE");
			ref_updateAlpha(&ab, energies[e], ap, g, 0x4000, "NE");
			diff_eq_int("debug transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, (long)e * 100 + ap * 10 + g / 0x1000);
			diff_eq_int("debug alpha", aa, ab,
				    (long)e * 100 + ap * 10 + g / 0x1000);
		}

		/* And it must actually have printed something. */
		diff_eq_int("transcript non-empty",
			    dsplib_debug_capture_lines(1) > 0, 1, 0);

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	diff_begin("v34 descrambler");
	{
		static struct v34_receiver sa, sb;
		int ans, n, v;

		for (ans = 0; ans <= 1; ans++)
		for (n = 0; n <= 16; n++) {
			memset(&sa, HARNESS_MALLOC_FILL, sizeof(sa));
			memset(&sb, HARNESS_MALLOC_FILL, sizeof(sb));
			sa.scrambler_sr = sb.scrambler_sr = 0;
			sa.flags = sb.flags =
				(unsigned short)(ans ? V34_SCR_ANSWERER : 0);
			for (v = 0; v < 4000; v += 7) {
				diff_eq_int("descrambled",
					    V34descrambler(&sa, (short)v,
							   (short)n),
					    ref_V34descrambler(&sb, (short)v,
							       (short)n),
					    (long)ans * 1000000 + n * 10000
					    + v);
				diff_eq_int("scrambler sr", (long)sa.scrambler_sr,
					    (long)sb.scrambler_sr, v);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 txinit");
	{
		static struct v34_object oa, ob;
		unsigned b;

		memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
		memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
		/* The cancellers must be wired or CleanUp walks nowhere. */
		V34InitializeImplementationSpecific(&oa);
		ref_V34InitializeImplementationSpecific(&ob);
		txinit(&oa);
		ref_txinit(&ob);

		for (b = 0; b < sizeof(oa); b++) {
			/* Every pointer: each side holds its own addresses. */
			unsigned skip[][2] = {
			  { __builtin_offsetof(struct v34_object, rxq)
			    + __builtin_offsetof(struct v34_queue, rd), 8 },
			  { __builtin_offsetof(struct v34_object, txq)
			    + __builtin_offsetof(struct v34_queue, rd), 8 },
			  { __builtin_offsetof(struct v34_object, p_2074), 4 },
			  /*
			   * The six POINTERS only.  dlen and taps at +0x18
			   * are plain counts and get compared -- skipping
			   * the whole 0x20 hid them for no reason.
			   */
			  { __builtin_offsetof(struct v34_object, echo0), 0x18 },
			  { __builtin_offsetof(struct v34_object, echo1), 0x18 },
			  { __builtin_offsetof(struct v34_object, prefilter)
			    + __builtin_offsetof(struct v34_echo_prefilter,
						 coeff), 4 },
			};
			unsigned s2, hit = 0;

			for (s2 = 0; s2 < sizeof(skip) / sizeof(skip[0]); s2++)
				if (b >= skip[s2][0]
				    && b < skip[s2][0] + skip[s2][1])
					hit = 1;
			if (hit)
				continue;
			diff_eq_int("txinit at %ld",
				    ((unsigned char *)&oa)[b],
				    ((unsigned char *)&ob)[b], b);
		}
		/* And the cursors, as offsets. */
		diff_eq_int("txq rd", oa.txq.rd - oa.txq.ring,
			    ob.txq.rd - ob.txq.ring, 0);
		diff_eq_int("txq wr", oa.txq.wr - oa.txq.ring,
			    ob.txq.wr - ob.txq.ring, 0);
		diff_eq_int("rxq rd", oa.rxq.rd - oa.rxq.ring,
			    ob.rxq.rd - ob.rxq.ring, 0);
		diff_eq_int("rxq wr", oa.rxq.wr - oa.rxq.ring,
			    ob.rxq.wr - ob.rxq.ring, 0);
		diff_eq_int("txq primed with 32", oa.txq.count, 0x20, 0);
	}
	rc |= diff_end();

	diff_begin("v34 agcadapt");
	{
		static struct v34_receiver aa, ab;
		int lv, in, st, gn, fl;

		for (fl = 0; fl <= 2; fl += 2)
		for (lv = -32768; lv < 32768; lv += 4093)
		for (in = 0; in < 65536; in += 12289)
		for (st = -32768; st < 32768; st += 21851)
		for (gn = -32768; gn < 32768; gn += 13107) {
			memset(&aa, HARNESS_MALLOC_FILL, sizeof(aa));
			memset(&ab, HARNESS_MALLOC_FILL, sizeof(ab));
			aa.flags = ab.flags = (unsigned short)(fl ? V34_RX_FLAG_AGC_FREEZE : 0);
			aa.agc_level = ab.agc_level = (short)lv;
			aa.energy.h.agc_input = ab.energy.h.agc_input = (unsigned short)in;
			aa.agc_step = ab.agc_step = (short)st;
			aa.agc_gain = ab.agc_gain = (short)gn;
			aa.agc_accum = ab.agc_accum = (short)(lv / 3);

			diff_eq_int("agcadapt ret", agcadapt(&aa),
				    ref_agcadapt(&ab), lv);
			for (i = 0; i < (int)sizeof(aa); i++)
				diff_eq_int("agc state at %ld",
					    ((unsigned char *)&aa)[i],
					    ((unsigned char *)&ab)[i], i);
		}
	}
	rc |= diff_end();

	diff_begin("v34 rxtiminginit");
	{
		static struct v34_object oa, ob;
		unsigned b;

		memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
		memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
		rxtiminginit(&oa);
		ref_rxtiminginit(&ob);

		for (b = 0; b < sizeof(oa); b++) {
			/* The two installed pointers differ by construction. */
			unsigned rs = 0x264 + __builtin_offsetof(
				struct v34_receiver, rx_samples);
			unsigned tp = 0x50c + __builtin_offsetof(
				struct v34_timing, prefilter_coeff);

			if ((b >= rs && b < rs + 4) || (b >= tp && b < tp + 8))
				continue;
			diff_eq_int("rxtiminginit at %ld",
				    ((unsigned char *)&oa)[b],
				    ((unsigned char *)&ob)[b], b);
		}
		diff_eq_int("baud starts at 2400",
			    ((struct v34_receiver *)((char *)&oa + 0x264))->f1d2,
			    2400, 0);
	}
	rc |= diff_end();

	diff_begin("v34 rxinit");
	{
		static struct v34_object oa, ob;
		int fl;

		for (fl = 0; fl <= 8; fl += 8) {
			unsigned b;

			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			((struct v34_receiver *)((char *)&oa + 0x264))->flags =
				(unsigned short)fl;
			((struct v34_receiver *)((char *)&ob + 0x264))->flags =
				(unsigned short)fl;
			rxinit(&oa);
			ref_rxinit(&ob);

			for (b = 0; b < sizeof(oa); b++) {
				unsigned rs = 0x264 + __builtin_offsetof(
					struct v34_receiver, rx_samples);

				/*
				 * agc_accum used to be skipped here, on the
				 * theory that rxinit seeded it from the
				 * object's own address (D34, now retracted).
				 * It is compared like everything else: the
				 * blob zeroes it.  A skip plus an assertion
				 * of one's own reading is not a differential
				 * test -- see finding 122.
				 */
				if (b >= rs && b < rs + 4)
					continue;
				diff_eq_int("rxinit at %ld",
					    ((unsigned char *)&oa)[b],
					    ((unsigned char *)&ob)[b],
					    (long)fl * 100000 + b);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 txmit");
	{
		static struct v34_object oa, ob;
		/*
		 * shaped, the bulk ring and the pre-filter coefficients all
		 * live OUTSIDE the object.  Pointing any of them into it
		 * collides with the arrays
		 * V34InitializeImplementationSpecific set up -- identically
		 * on both sides, so a comparison cannot see it.  Finding 116b
		 * is what that cost.
		 */
		static short shp_a[512], shp_b[512];
		static short bra[64], brb[64];
		int gate, it;

		for (gate = 0; gate <= 1; gate++) {
			memset(&oa, 0, sizeof(oa)); memset(&ob, 0, sizeof(ob));
			memset(shp_a, 0, sizeof(shp_a));
			memset(shp_b, 0, sizeof(shp_b));
			memset(bra, 0, sizeof(bra)); memset(brb, 0, sizeof(brb));
			V34InitializeImplementationSpecific(&oa);
			ref_V34InitializeImplementationSpecific(&ob);
			txinit(&oa); ref_txinit(&ob);
			((struct v34_modulator *)((char *)&oa + 0x1450))->shaped
				= shp_a;
			((struct v34_modulator *)((char *)&ob + 0x1450))->shaped
				= shp_b;
			V34SetupModulator((struct v34_modulator *)
					  ((char *)&oa + 0x1450), 2400, 1600,
					  0, 0, 1);
			ref_V34SetupModulator((char *)&ob + 0x1450, 2400,
					      1600, 0, 0, 1);
			oa.prefilter.coeff = ob.prefilter.coeff =
				V34TimingPrefilterCoeff;
			oa.prefilter.shift = ob.prefilter.shift = 14;
			oa.f25d4 = ob.f25d4 = 0x4000;
			oa.f25c2 = ob.f25c2 = (short)(gate ? 0x200 : 0);
			oa.bulk_ring = bra;  ob.bulk_ring = brb;
			oa.bulk_len  = ob.bulk_len = 64;

			for (it = 0; it < 60; it++) {
				unsigned b;

				oa.f25d0 = ob.f25d0 = (short)(it * 811 - 9000);
				oa.f25d2 = ob.f25d2 = (short)(it * 337 - 5000);
				txmit(&oa); ref_txmit(&ob);

				for (b = 0; b < sizeof(oa); b++) {
					/* Every pointer field: two objects. */
					if ((b >= 0x268 && b < 0x270)
					    || (b >= 0x2074 && b < 0x2078)
					    || (b >= 0x20cc && b < 0x20d0)
					    || (b >= 0x2220 && b < 0x2228)
					    || (b >= 0x35b0 && b < 0x35b4)
					    || (b >= 0x80b8 && b < 0x80d8)
					    || (b >= 0x9138 && b < 0x9158)
					    || (b >= 0x1450 + 0x10
						&& b < 0x1450 + 0x18)
					    || (b >= 0x1450 + 0xc24
						&& b < 0x1450 + 0xc28)
					    || (b >= 0x1450 + 0xc7c
						&& b < 0x1450 + 0xc80)
					    || (b >= 0x1450 + 0xcb0
						&& b < 0x1450 + 0xcb4))
						continue;
					/*
					 * Stride larger than the object, so
					 * (iteration, offset) is unambiguous
					 * -- finding 116a.
					 */
					diff_eq_int("txmit at %ld",
						    ((unsigned char *)&oa)[b],
						    ((unsigned char *)&ob)[b],
						    (long)it * 100000 + b);
				}
				for (b = 0; b < 64; b++)
					diff_eq_int("bulk ring", bra[b],
						    brb[b], b);
				for (b = 0; b < 512; b++)
					diff_eq_int("shaped", shp_a[b],
						    shp_b[b], b);
			}
		}
	}
	rc |= diff_end();

	/*
	 * V34agc: the AGC chain with no timing loop around it.
	 *
	 * Driven directly rather than through rxtiming, which is the whole
	 * point -- V34demodulate is a local symbol and can only be reached
	 * through the interpolator, so every AGC defect it had presented as
	 * a loop-shape failure.  This one is global and isolates them.
	 *
	 * The sweep has to cross the three gates that decide anything: the
	 * freeze flag, the RMS floor at 31, and the |err| > 1200 deadband.
	 * A sweep that stays below the floor exercises two `return`s and
	 * proves nothing, so the amplitudes run from silence to clipping.
	 */
	diff_begin("v34 V34agc");
	{
		static struct v34_receiver ra, rb;
		/* The receive queue IS the receiver's first 0x10c bytes. */
		struct v34_queue *qa2 = (struct v34_queue *)&ra;
		struct v34_queue *qb2 = (struct v34_queue *)&rb;
		int amp, gn, fl, it;
		unsigned b;

		for (fl = 0; fl <= 2; fl += 2)
		for (amp = 0; amp < 32767; amp += 3037)
		for (gn = 0x100; gn < 0x7000; gn += 0x1a01) {
			memset(&ra, HARNESS_MALLOC_FILL, sizeof(ra));
			memset(&rb, HARNESS_MALLOC_FILL, sizeof(rb));

			ra.flags = rb.flags =
			    (unsigned short)(fl ? V34_RX_FLAG_AGC_FREEZE : 0);
			ra.agc_gain = rb.agc_gain = (short)gn;
			ra.agc_level = rb.agc_level = 0;
			ra.agc_accum = rb.agc_accum = 0;
			ra.agc_step = rb.agc_step = 0x3333;
			ra.f19c = rb.f19c = 0;
			ra.energy.sum = rb.energy.sum = 0;
			for (b = 0; b < V34_AGC_RMS_TAPS; b++)
				ra.rms_buf[b] = rb.rms_buf[b] = 0;

			/* Prime the ring with a ramp, and both cursors. */
			qa2->count = qb2->count = V34_RXQ_RING;
			for (b = 0; b < V34_RXQ_RING; b++)
				qa2->ring[b] = qb2->ring[b] =
				    (int)(short)((b * amp) % 32768
						 - (int)(b & 1) * amp);
			qa2->rd = qa2->ring;
			qb2->rd = qb2->ring;
			qa2->wr = qa2->ring;
			qb2->wr = qb2->ring;

			/*
			 * Sixteen bursts: enough for the 36-entry RMS window
			 * to fill and wrap, so the adapt path runs on a full
			 * window rather than on the memset fill.
			 */
			for (it = 0; it < 16; it++) {
				long tag = ((long)fl * 1000 + amp / 3037) * 100000
					 + (long)it * 1000;

				V34agc(&ra);
				ref_V34agc(&rb);

				for (b = 0; b < sizeof(ra); b++) {
					/* rx_samples: each side's own address. */
					if (b >= __builtin_offsetof(
						    struct v34_receiver,
						    rx_samples)
					    && b < __builtin_offsetof(
						    struct v34_receiver,
						    rx_samples) + 4)
						continue;
					/* And the queue cursors. */
					if (b >= __builtin_offsetof(
						    struct v34_queue, rd)
					    && b < __builtin_offsetof(
						    struct v34_queue, wr) + 4)
						continue;
					diff_eq_int("V34agc at %ld",
						    ((unsigned char *)&ra)[b],
						    ((unsigned char *)&rb)[b],
						    tag + b);
				}
				diff_eq_int("V34agc rxq rd",
					    (long)(qa2->rd - qa2->ring),
					    (long)(qb2->rd - qb2->ring),
					    tag);
				diff_eq_int("V34agc samples",
					    (long)((char *)ra.rx_samples
						   - (char *)&ra),
					    (long)((char *)rb.rx_samples
						   - (char *)&rb),
					    tag);
			}
		}
	}
	rc |= diff_end();

	/*
	 * rxtiming + V34demodulate: the interpolator and everything under it.
	 *
	 * V34demodulate is a file-static in the object, so it has no ref_
	 * alias and can only be reached through this caller.  That is why the
	 * AGC inside it was proven separately via V34agc first -- otherwise
	 * every AGC defect arrives here disguised as a loop-shape failure.
	 *
	 * The step sweep exists to reach all three paths.  Against a wrap of
	 * 1024: 300 and 700 can only ever cross once (phase < 1024, so
	 * phase+step < 2048); 1500 crosses twice for larger phases; and 2600
	 * crosses twice on EVERY output and still leaves the phase above the
	 * wrap, which is the case the object does not handle.  That last one
	 * is included deliberately -- reproducing the unhandled case is the
	 * only way to know we reproduce it.
	 */
	diff_begin("v34 rxtiming + V34demodulate");
	{
		static struct v34_object oa, ob;
		static short carrier[256];
		static const short steps[] = { 300, 700, 1023, 1500, 2600 };
		unsigned sw, b;
		int k, it;

		for (b = 0; b < 256; b++)
			carrier[b] = (short)(((int)b * 517) % 32768 - 16384);

		for (sw = 0; sw < sizeof(steps) / sizeof(steps[0]); sw++) {
			struct v34_receiver *ra, *rb;

			/*
			 * The fill, not zero: a zeroed object would hide
			 * anything rxinit and rxtiminginit fail to set, which
			 * is the question task #23 added it to ask.
			 */
			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			V34InitializeImplementationSpecific(&oa);
			ref_V34InitializeImplementationSpecific(&ob);
			/* txinit sets the RECEIVE queue's cursors. */
			txinit(&oa); ref_txinit(&ob);
			rxinit(&oa); ref_rxinit(&ob);
			rxtiminginit(&oa); ref_rxtiminginit(&ob);

			ra = (struct v34_receiver *)((char *)&oa + 0x264);
			rb = (struct v34_receiver *)((char *)&ob + 0x264);

			/* Fill the ring, and say it is full. */
			for (b = 0; b < V34_RXQ_RING; b++)
				((struct v34_queue *)ra)->ring[b] =
				((struct v34_queue *)rb)->ring[b] =
				    (int)((unsigned)(unsigned short)
					  (short)(b * 2731 - 12000)
					  | ((unsigned)(unsigned short)
					     (short)(b * 991 - 8000) << 16));
			((struct v34_queue *)ra)->count =
			((struct v34_queue *)rb)->count = V34_RXQ_RING;

			ra->carrier = rb->carrier = carrier;
			ra->f1b8 = rb->f1b8 = 3;
			ra->f1ba = rb->f1ba = 64;
			ra->f1bc = rb->f1bc = 5;

			/*
			 * The RMS index is NOT set by rxinit or
			 * rxtiminginit -- dpskinit and v34modeminit zero it,
			 * further up the call chain than anything here.  So
			 * the fixture stands in for them; left at the fill it
			 * would be -23131 and index 46 KB below rms_buf.
			 */
			ra->f19c = rb->f19c = 0;

			ra->f1ac = rb->f1ac = 17;
			ra->f1ae = rb->f1ae = steps[sw];
			ra->f1b0 = rb->f1b0 = 1024;
			/*
			 * THE BOUND, and it is far tighter than timing_out[]
			 * suggests.  V34demodulate appends one gained sample
			 * per pull starting at +0x10c, and the very next
			 * fields are the receiver's own bookkeeping: f128 at
			 * +0x128, f12a at +0x12a, the energy sum at +0x12c
			 * and rx_samples itself at +0x130.  So there are
			 * fourteen shorts of headroom, and the fifteenth pull
			 * overwrites THIS LOOP'S OWN BOUND -- see finding 123.
			 *
			 * Six outputs at up to two pulls each is twelve,
			 * which stays inside.  This is a bound on the caller,
			 * not something to test past: driving it further
			 * measures the overrun, not the interpolator.
			 */
			ra->f128 = rb->f128 = 6;
			ra->agc_gain = rb->agc_gain = 0x400;
			ra->agc_step = rb->agc_step = 0x3333;

			for (it = 0; it < 4; it++) {
				long tag = (long)sw * 10000000
					 + (long)it * 100000;

				rxtiming(&oa);
				ref_rxtiming(&ob);

				/* It must not have reached f128. */
				if ((char *)ra->rx_samples
				    > (char *)&ra->f128) {
					printf("FIXTURE: the burst reached "
					       "+0x128 -- lower f128\n");
					return 1;
				}

				for (b = 0; b < sizeof(oa); b++) {
					/*
					 * Every pointer in the object: each
					 * side holds its own addresses.  The
					 * timing filter's two coefficient
					 * pointers and the transmit queue's
					 * cursors are here because leaving
					 * them out is what this fixture got
					 * wrong first -- twice now, counting
					 * V34TimingFilter.
					 */
					static const unsigned skip[][2] = {
					  { 0x264 + 0x04, 8 },   /* rxq rd/wr */
					  { 0x264 + 0x130, 4 },  /* samples   */
					  { 0x264 + 0x1b4, 4 },  /* carrier   */
					  { 0x50c + __builtin_offsetof(
					      struct v34_timing,
					      prefilter_coeff), 8 },
					  { 0x221c + __builtin_offsetof(
					      struct v34_queue, rd), 8 },
					  { 0x2074, 4 },
					  { 0x2078 + __builtin_offsetof(
					      struct v34_echo_prefilter,
					      coeff), 4 },
					  { 0x80b8, 0x18 },  /* echo0 ptrs */
					  { 0x9138, 0x18 },  /* echo1 ptrs */
					};
					unsigned s2, hit = 0;

					for (s2 = 0; s2 < sizeof(skip)
						     / sizeof(skip[0]); s2++)
						if (b >= skip[s2][0]
						    && b < skip[s2][0]
							   + skip[s2][1])
							hit = 1;
					if (hit)
						continue;
					diff_eq_int("rxtiming at %ld",
						    ((unsigned char *)&oa)[b],
						    ((unsigned char *)&ob)[b],
						    tag + b);
				}
				diff_eq_int("rxtiming rd",
				    (long)(((struct v34_queue *)ra)->rd
					   - ((struct v34_queue *)ra)->ring),
				    (long)(((struct v34_queue *)rb)->rd
					   - ((struct v34_queue *)rb)->ring),
				    tag);
				diff_eq_int("rxtiming samples",
				    (long)((char *)ra->rx_samples - (char *)ra),
				    (long)((char *)rb->rx_samples - (char *)rb),
				    tag);
				for (k = 0; k < 6; k++)
					diff_eq_int("timing out",
						    ra->timing_out[k],
						    rb->timing_out[k],
						    tag + 90000 + k);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 txrxdmainit");
	{
		short src[8], da[12], db[12];
		int n, k;

		for (n = 0; n < 400; n++) {
			for (k = 0; k < 8; k++)
				src[k] = (short)(n * 7919 + k * 4093 - 32768);
			/* -32768 negates to itself; make sure that is hit. */
			if (n % 37 == 0)
				src[3] = src[5] = (short)-32768;
			memset(da, 0x5a, sizeof(da));
			memset(db, 0x5a, sizeof(db));
			txrxdmainit(da, src);
			ref_txrxdmainit(db, src);
			for (k = 0; k < 12; k++)
				diff_eq_int("dma coeff", da[k], db[k],
					    (long)n * 100 + k);
		}
	}
	rc |= diff_end();

	diff_begin("v34 V34scrambler");
	{
		unsigned s0;
		int md, nb, bv, ra2, rb2;
		unsigned sa, sb;
		int n;

		for (n = 0; n < 40; n++)
		for (md = 0; md <= 1; md++)
		for (nb = -2; nb <= 17; nb++)
		for (bv = 0; bv < 65536; bv += 2731) {
			s0 = (unsigned)(n * 0x9e3779b9u + 0x0f0f0f0fu);
			sa = sb = s0;
			ra2 = V34scrambler(&sa, (short)md, (short)bv,
					   (short)nb);
			rb2 = ref_V34scrambler(&sb, (short)md, (short)bv,
					       (short)nb);
			diff_eq_int("scr out", ra2, rb2,
				    ((long)n * 100 + md * 50 + (nb + 2)) * 100000
				    + bv);
			diff_eq_int("scr sr", (long)sa, (long)sb,
				    ((long)n * 100 + md * 50 + (nb + 2)) * 100000
				    + bv);
		}
	}
	rc |= diff_end();

	/*
	 * V34scrambler and V34descrambler must be inverses.  Not a
	 * differential check -- both sides are ours -- but it is the property
	 * the pair exists for, and it would catch a tap that matched the blob
	 * in one direction only.
	 */
	diff_begin("v34 scrambler round trip");
	{
		int md, n;

		for (md = 0; md <= 1; md++) {
			static struct v34_receiver d;
			unsigned tx = 0x2a2a2a2au;

			memset(&d, 0, sizeof(d));
			d.scrambler_sr = 0;
			d.flags = (unsigned short)(md ? V34_SCR_ANSWERER : 0);

			/*
			 * The descrambler is self-synchronising, so the first
			 * 23 bits come out wrong by construction; compare
			 * once the register has filled.
			 */
			for (n = 0; n < 64; n++) {
				int plain = (n * 13) & 0xff;
				int coded = V34scrambler(&tx, (short)md,
							 (short)plain, 8);
				int back = V34descrambler(&d, (short)coded, 8);

				if (n >= 4)
					diff_eq_int("round trip", back, plain,
						    (long)md * 1000 + n);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 v34FreezeEcho");
	{
		static struct v34_object oa, ob;
		unsigned b;

		memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
		memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
		V34InitializeImplementationSpecific(&oa);
		ref_V34InitializeImplementationSpecific(&ob);
		txinit(&oa); ref_txinit(&ob);
		oa.f25c2 = ob.f25c2 = 0x0102;

		v34FreezeEcho(&oa);
		ref_v34FreezeEcho(&ob);

		for (b = 0; b < sizeof(oa); b++) {
			static const unsigned skip[][2] = {
			  { 0x264 + 0x04, 8 },
			  { 0x221c + 0x04, 8 },
			  { 0x2074, 4 },
			  { 0x80b8, 0x18 },
			  { 0x9138, 0x18 },
			  { 0x2078, 4 },
			};
			unsigned s2, hit = 0;

			for (s2 = 0; s2 < sizeof(skip) / sizeof(skip[0]); s2++)
				if (b >= skip[s2][0]
				    && b < skip[s2][0] + skip[s2][1])
					hit = 1;
			if (!hit)
				diff_eq_int("freeze at %ld",
					    ((unsigned char *)&oa)[b],
					    ((unsigned char *)&ob)[b], b);
		}
		diff_eq_int("freeze set bit 2", oa.f25c2 & V34_EC_FROZEN,
			    V34_EC_FROZEN, 0);
		diff_eq_int("freeze kept the rest", oa.f25c2 & ~V34_EC_FROZEN,
			    0x0102, 0);
	}
	rc |= diff_end();

	/*
	 * V34SetupDemodulator: the six rates and eight carriers, plus values
	 * that match neither.  Both lookups fall through silently, so the
	 * unrecognised cases are the ones worth driving -- they must leave
	 * the previous setup in place, not clear it.
	 */
	diff_begin("v34 V34SetupDemodulator");
	{
		static struct v34_object oa, ob;
		static const short rates[] = { 2400, 2743, 2800, 3000, 3200,
					       3429, 0, 1, 2401, -1, 32767 };
		static const short carrs[] = { 1600, 1680, 1800, 1829, 1867,
					       1920, 1959, 2000, 0, 1801, -5 };
		unsigned r, c, b;

		for (r = 0; r < sizeof(rates) / sizeof(rates[0]); r++)
		for (c = 0; c < sizeof(carrs) / sizeof(carrs[0]); c++) {
			struct v34_receiver *ra, *rb;

			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			ra = (struct v34_receiver *)((char *)&oa + 0x264);
			rb = (struct v34_receiver *)((char *)&ob + 0x264);

			/* A known previous setup, so fall-through shows. */
			V34SetupDemodulator(&oa, 2400, 1800);
			ref_V34SetupDemodulator(&ob, 2400, 1800);

			V34SetupDemodulator(&oa, rates[r], carrs[c]);
			ref_V34SetupDemodulator(&ob, rates[r], carrs[c]);

			for (b = 0; b < sizeof(oa); b++) {
				/* carrier is a pointer; compared below. */
				unsigned cp = 0x264 + __builtin_offsetof(
					struct v34_receiver, carrier);

				if (b >= cp && b < cp + 4)
					continue;
				diff_eq_int("setup at %ld",
					    ((unsigned char *)&oa)[b],
					    ((unsigned char *)&ob)[b],
					    ((long)r * 20 + c) * 100000 + b);
			}
			/*
			 * The two sides point at their own copies of the
			 * table, so the pointers cannot be compared -- but
			 * the CONTENTS can, and that is the check that
			 * matters: it says both picked the same table.
			 * Comparing the pointers would only have measured
			 * that the blob and the reconstruction live at
			 * different addresses.
			 */
			{
				int k;

				for (k = 0; k < 2 * (int)ra->f1ba; k++)
					diff_eq_int("carrier entry",
						    ra->carrier[k],
						    rb->carrier[k],
						    ((long)r * 20 + c) * 1000
						    + k);
			}
		}
	}
	rc |= diff_end();

	/*
	 * The quarter-cycle relation the demodulator depends on: every
	 * carrier table is exactly 2 * f1ba shorts, so carrier[i + f1ba] is
	 * carrier[i] shifted a quarter period.  A property check, not a
	 * differential one -- but it is the invariant that makes
	 * V34demodulate's two reads a cosine and a sine, and it would catch a
	 * table paired with the wrong length.
	 */
	diff_begin("v34 carrier tables are 2 x f1ba");
	{
		static struct v34_object o;
		static const struct { short c; unsigned len; } tab[] = {
			{ 1600, 12 }, { 1680, 80 }, { 1800, 32 }, { 1829, 42 },
			{ 1867, 72 }, { 1920, 10 }, { 1959, 98 }, { 2000, 48 },
		};
		unsigned i;

		for (i = 0; i < sizeof(tab) / sizeof(tab[0]); i++) {
			struct v34_receiver *r;

			memset(&o, 0, sizeof(o));
			V34SetupDemodulator(&o, 2400, tab[i].c);
			r = (struct v34_receiver *)((char *)&o + 0x264);
			diff_eq_int("2 x half length",
				    2 * (unsigned)r->f1ba, tab[i].len,
				    tab[i].c);
		}
	}
	rc |= diff_end();

	/*
	 * adaptecho: 200 calls, which crosses every schedule boundary --
	 * the 0x8f energy-gathering burst, the one-shot measurement at 0x90,
	 * and the every-tenth-call step recompute past f3554.  A shorter run
	 * would exercise only the first and prove almost nothing.
	 *
	 * Run with the debug transcript captured too, since adaptecho reaches
	 * three separate messages (updateAlpha's, the echo-energy report and
	 * the negative-lag error) and finding 126 is what those cost when
	 * nothing compares them.
	 */
	diff_begin("v34 adaptecho");
	{
		static struct v34_object oa, ob;
		int lagbase, it;
		unsigned b;

		for (lagbase = 0; lagbase <= 3; lagbase++) {
			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			V34InitializeImplementationSpecific(&oa);
			ref_V34InitializeImplementationSpecific(&ob);
			txinit(&oa); ref_txinit(&ob);

			oa.f25c2 = ob.f25c2 = 0;
			oa.f260  = ob.f260  = 0;
			oa.fa23e = ob.fa23e = 0;
			oa.f354c = ob.f354c = 0;
			oa.f3550 = ob.f3550 = -0x2000;
			oa.f3554 = ob.f3554 = 0x95;
			oa.f3558 = ob.f3558 = 0x7000;
			oa.f355c = ob.f355c = 6;
			oa.f3560 = ob.f3560 = 0;
			oa.echo0.adapt_count = ob.echo0.adapt_count = 0;

			/*
			 * lagbase 3 makes the lag go negative part-way, which
			 * is the path that declines to adapt and prints.
			 */
			oa.f25c = ob.f25c = (short)(lagbase == 3 ? 8 : 0x40);

			for (b = 0; b < V34_TXQ_RING; b++)
				oa.txq.ring[b] = ob.txq.ring[b] =
				    (int)(short)(b * 3571 - 15000);
			oa.txq.count = ob.txq.count = 0x30;

			dsplibs_debug_level = 2;
			ref_dsplibs_debug_level = 2;
			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			for (it = 0; it < 200; it++) {
				oa.fa23e = ob.fa23e = (short)(it * 37 - 900);
				adaptecho(&oa);
				ref_adaptecho(&ob);
			}

			dsplib_debug_capture_on = 0;
			dsplibs_debug_level = 0;
			ref_dsplibs_debug_level = 0;

			diff_eq_int("adaptecho transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, lagbase);

			for (b = 0; b < sizeof(oa); b++) {
				static const unsigned skip[][2] = {
				  { 0x264 + 0x04, 8 },
				  { 0x221c + 0x04, 8 },
				  { 0x2074, 4 },
				  { 0x2078, 4 },
				  { 0x80b8, 0x14 },   /* ptrs, NOT +0x14 */
				  { 0x80b8 + 0x16, 2 },
				  { 0x9138, 0x14 },
				  { 0x9138 + 0x16, 2 },
				};
				unsigned s2, hit = 0;

				for (s2 = 0; s2 < sizeof(skip)
					     / sizeof(skip[0]); s2++)
					if (b >= skip[s2][0]
					    && b < skip[s2][0] + skip[s2][1])
						hit = 1;
				if (!hit)
					diff_eq_int("adaptecho at %ld",
						    ((unsigned char *)&oa)[b],
						    ((unsigned char *)&ob)[b],
						    (long)lagbase * 100000 + b);
			}
			diff_eq_int("adaptecho txq rd",
				    (long)(oa.txq.rd - oa.txq.ring),
				    (long)(ob.txq.rd - ob.txq.ring), lagbase);
			diff_eq_int("adapt counter stepped",
				    oa.echo0.adapt_count != 0, 1, lagbase);
		}
	}
	rc |= diff_end();

	/*
	 * modem_serrint: 300 calls per configuration, which crosses the near
	 * canceller's 0x90 measurement and the far one's 0x2bb offset, and
	 * the flag sweep covers all three ways it builds the complex sample.
	 *
	 * `fir` needs coefficients at f2a4 and uses ECHO1's fractional array
	 * as its delay line -- the finding-100 overlay, third reader.  The
	 * coefficient block must live OUTSIDE the object: pointing it inside
	 * collides with what V34InitializeImplementationSpecific set up, and
	 * does so identically on both sides, which is finding 116b.
	 */
	diff_begin("v34 modem_serrint");
	{
		static struct v34_object oa, ob;
		static short coeff[64];
		int mode, feed, far, it;
		unsigned b;

		for (b = 0; b < 64; b++)
			coeff[b] = (short)(b * 617 - 9000);

		for (mode = 0; mode < 3; mode++)
		for (feed = 0; feed <= 1; feed++)
		for (far = 0; far <= 1; far++) {
			struct v34_receiver *ra, *rb;

			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			V34InitializeImplementationSpecific(&oa);
			ref_V34InitializeImplementationSpecific(&ob);
			txinit(&oa); ref_txinit(&ob);
			rxinit(&oa); ref_rxinit(&ob);
			rxtiminginit(&oa); ref_rxtiminginit(&ob);
			ra = (struct v34_receiver *)((char *)&oa + 0x264);
			rb = (struct v34_receiver *)((char *)&ob + 0x264);

			ra->flags = rb->flags = (unsigned short)
			    (mode == 0 ? 0x8000 : mode == 1 ? 0x0800 : 0);
			ra->f2a4 = rb->f2a4 = coeff;

			oa.f25c  = ob.f25c  = 0x40;
			oa.f25c2 = ob.f25c2 = (short)(feed ? V34_EC_FEED : 0);
			oa.f260  = ob.f260  = 1234;
			oa.fa23c = ob.fa23c = (short)(far ? 1 : 0);
			oa.fa23e = ob.fa23e = 0;
			oa.fa240 = ob.fa240 = 0;
			oa.f2aa4 = ob.f2aa4 = 0;
			oa.f2aa6 = ob.f2aa6 = 0;
			oa.f354c = ob.f354c = 0;
			oa.f3550 = ob.f3550 = -0x1800;
			oa.f3552 = ob.f3552 = -0x1400;
			oa.echo0.adapt_count = ob.echo0.adapt_count = 0;
			oa.echo1.adapt_count = ob.echo1.adapt_count = 0;
			for (b = 0; b < 0x12c; b++)
				oa.hist_2aa8[b] = ob.hist_2aa8[b] = 0;
			for (b = 0; b < 0x258; b++)
				oa.hist_2f58[b] = ob.hist_2f58[b] = 0;

			for (b = 0; b < V34_TXQ_RING; b++)
				oa.txq.ring[b] = ob.txq.ring[b] =
				    (int)(short)(b * 2777 - 12000);
			oa.txq.count = ob.txq.count = 0x30;
			oa.rxq.count = ob.rxq.count = 0;
			for (b = 0; b < V34_RXQ_RING; b++)
				oa.rxq.ring[b] = ob.rxq.ring[b] = 0;

			dsplibs_debug_level = 2;
			ref_dsplibs_debug_level = 2;
			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			for (it = 0; it < 300; it++) {
				modem_serrint(&oa);
				ref_modem_serrint(&ob);
			}

			dsplib_debug_capture_on = 0;
			dsplibs_debug_level = 0;
			ref_dsplibs_debug_level = 0;

			diff_eq_int("serrint transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, (long)mode * 100 + feed * 10 + far);

			for (b = 0; b < sizeof(oa); b++) {
				static const unsigned skip[][2] = {
				  { 0x264 + 0x04, 8 },
				  { 0x264 + 0x130, 4 },
				  { 0x264 + 0x1b4, 4 },
				  { 0x264 + 0x2a4, 4 },
				  /* rxtiminginit installs these two. */
				  { 0x50c + __builtin_offsetof(
				      struct v34_timing,
				      prefilter_coeff), 8 },
				  { 0x221c + 0x04, 8 },
				  { 0x2074, 4 },
				  { 0x2078, 4 },
				  { 0x80b8, 0x14 },
				  { 0x80b8 + 0x16, 2 },
				  { 0x9138, 0x14 },
				  { 0x9138 + 0x16, 2 },
				};
				unsigned s2, hit = 0;

				for (s2 = 0; s2 < sizeof(skip)
					     / sizeof(skip[0]); s2++)
					if (b >= skip[s2][0]
					    && b < skip[s2][0] + skip[s2][1])
						hit = 1;
				if (!hit)
					diff_eq_int("serrint at %ld",
						    ((unsigned char *)&oa)[b],
						    ((unsigned char *)&ob)[b],
						    ((long)mode * 100
						     + feed * 10 + far)
						    * 100000 + b);
			}
			diff_eq_int("serrint rxq wr",
				    (long)(oa.rxq.wr - oa.rxq.ring),
				    (long)(ob.rxq.wr - ob.rxq.ring),
				    (long)mode * 100 + feed * 10 + far);
			diff_eq_int("serrint txq rd",
				    (long)(oa.txq.rd - oa.txq.ring),
				    (long)(ob.txq.rd - ob.txq.ring),
				    (long)mode * 100 + feed * 10 + far);
		}
	}
	rc |= diff_end();

	diff_begin("v34 VPcmV34LogTimingOffset");
	{
		static struct v34_object oa, ob;
		int v;

		for (v = -32768; v < 32768; v += 4093) {
			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			VPcmV34LogTimingOffset(&oa, (short)v);
			ref_VPcmV34LogTimingOffset(&ob, (short)v);
			diff_eq_int("timing offset", oa.fac0c, ob.fac0c, v);
		}
	}
	rc |= diff_end();

	/*
	 * decoderv34: both decoders.  The 0x98 flag combination selects the
	 * trellis path, and ALL THREE bits must be set -- so the sweep walks
	 * every subset of them, not just on and off, to prove the others
	 * fall through to the four-point slice.
	 */
	diff_begin("v34 decoderv34");
	{
		static struct v34_object oa, ob;
		static const short coeffs[12] = {
			  30,  -21,   15,   -9,    6,   -3,
			 -28,   19,  -14,    8,   -5,    2
		};
		int fl, re, im, it;
		unsigned b;

		for (fl = 0; fl < 8; fl++)
		for (re = -12000; re < 12000; re += 5100) {
			struct v34_receiver *ra, *rb;
			unsigned short flags = (unsigned short)
			    (((fl & 1) ? 0x08 : 0) | ((fl & 2) ? 0x10 : 0)
			     | ((fl & 4) ? 0x80 : 0));

			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			ra = (struct v34_receiver *)((char *)&oa + 0x264);
			rb = (struct v34_receiver *)((char *)&ob + 0x264);

			/* Enough shell state for the trellis path to run. */
			{
				struct v34_shell *sa = (struct v34_shell *)&oa;
				struct v34_shell *sb = (struct v34_shell *)&ob;
				int k;

				for (k = 0; k < 32 * 16; k++)
					sa->trellis[k] = sb->trellis[k] = 0;
				for (k = 0; k < 32; k++) {
					sa->state[k].seed =
					sb->state[k].seed = 0;
					sa->state[k].a = sb->state[k].a =
					    (short)((k % 9) * 4 - 16);
					sa->state[k].b = sb->state[k].b =
					    (short)((k % 7) * 4 - 12);
					sa->state[k].c = sb->state[k].c =
					    (short)((k % 11) * 4 - 20);
					sa->state[k].d = sb->state[k].d =
					    (short)((k % 5) * 4 - 8);
				}
				for (k = 0; k < 16; k++)
					sa->cost[k] = sb->cost[k] = (short)k;
				for (k = 0; k < 18; k++)
					sa->frame[k] = sb->frame[k] = 0;
				for (k = 0; k < 8; k++)
					sa->sub[k] = sb->sub[k] = 0;
				for (k = 0; k < 6; k++)
					sa->hist[k] = sb->hist[k] = 0;
				for (k = 0; k < 0x80; k++) {
					sa->t1[k] = sb->t1[k] = (short)(k + 1);
					sa->t2[k] = sb->t2[k] = (short)(k + 2);
					sa->t3[k] = sb->t3[k] = k;
				}
				sa->coeff = coeffs; sb->coeff = coeffs;
				sa->put_bits = shell_sink_a;
				sb->put_bits = shell_sink_b;
				sa->state_idx = sb->state_idx = 0;
				sa->divisor = sb->divisor = 1;
				sa->wrap = sb->wrap = 1;
				sa->fa14 = sb->fa14 = 2;
				sa->fa44 = sb->fa44 = 2;
				sa->invert = sb->invert = 0;
				sa->fa00 = sb->fa00 = 3;
				sa->fa02 = sb->fa02 = 4;
				sa->fa3c = sb->fa3c = 0;
				sa->fa3e = sb->fa3e = 0;
				sa->fa40 = sb->fa40 = 3;
				sa->latched = sb->latched = 1;
				sa->count = sb->count = 9;
				sa->fa06 = sb->fa06 = 3;
				sa->fa08 = sb->fa08 = 0;
				sa->fa0e = sb->fa0e = 8;
				sa->fa10 = sb->fa10 = 8;
				sa->fa04 = sb->fa04 = 7;
				sa->prev_k = sb->prev_k = 0;
			}

			ra->flags = rb->flags = flags;
			ra->f266 = rb->f266 = 0;
			ra->f1aa = rb->f1aa = 0;
			ra->f124 = rb->f124 = 20;
			ra->f798 = rb->f798 = (short)(-60 - (re / 3000));
			ra->scrambler_sr = rb->scrambler_sr = 0x2a2a2a2a;
			oa.faa96 = ob.faa96 = 40;

			for (it = 0; it < 10; it++) {
				long tag = ((long)fl * 100
					    + (re + 12000) / 5100) * 100 + it;

				im = re / 2 + it * 700 - 3000;
				ra->target_re = rb->target_re = (short)re;
				ra->target_im = rb->target_im = (short)im;

				decoderv34(&oa);
				ref_decoderv34(&ob);

				diff_eq_int("dec best", ra->best_index,
					    rb->best_index, tag);
				diff_eq_int("dec f218", ra->f218, rb->f218,
					    tag);
				diff_eq_int("dec flags", ra->flags, rb->flags,
					    tag);
			}

			for (b = 0; b < sizeof(oa); b++) {
				static const unsigned skip[][2] = {
				  { 0x264 + 0x04, 8 },
				  { 0x264 + 0x130, 4 },
				  { 0x264 + 0x1b4, 4 },
				  { 0x264 + 0x2a4, 4 },
				  { 0x221c + 0x04, 8 },
				  { 0x2074, 4 },
				  { 0x2078, 4 },
				  { 0x80b8, 0x14 }, { 0x80b8 + 0x16, 2 },
				  { 0x9138, 0x14 }, { 0x9138 + 0x16, 2 },
				  { 0xa24, 4 },     /* shell coeff  */
				  { 0xe48, 4 },     /* shell sink   */
				};
				unsigned s2, hit = 0;

				for (s2 = 0; s2 < sizeof(skip)
					     / sizeof(skip[0]); s2++)
					if (b >= skip[s2][0]
					    && b < skip[s2][0] + skip[s2][1])
						hit = 1;
				if (!hit)
					diff_eq_int("decoderv34 at %ld",
						    ((unsigned char *)&oa)[b],
						    ((unsigned char *)&ob)[b],
						    ((long)fl * 100
						     + (re + 12000) / 5100)
						    * 100000 + b);
			}
		}
	}
	rc |= diff_end();

	diff_begin("v34 polyValue");
	{
		int k;

		for (k = -32768; k < 32768; k += 1)
			diff_eq_int("polyValue", polyValue((short)k),
				    ref_polyValue((short)k), k);
	}
	rc |= diff_end();

	/*
	 * setInitialPhase: the sign-crossing search, both interpolation
	 * error paths, and the twenty-candidate fit.  The metric patterns
	 * are chosen so the crossing lands at each possible index and so
	 * the give-up-at-5 path is reached.
	 */
	diff_begin("v34 setInitialPhase");
	{
		static struct v34_object oa, ob;
		int pat, ph, wrap, k;

		for (pat = 0; pat < 12; pat++)
		for (ph = 0; ph < 4; ph++)
		for (wrap = 1; wrap <= 2; wrap++) {
			struct v34_receiver *ra, *rb;

			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			ra = (struct v34_receiver *)((char *)&oa + 0x264);
			rb = (struct v34_receiver *)((char *)&ob + 0x264);

			for (k = 0; k < 21; k++) {
				short v;

				/*
				 * pat 0..5   crossing at index pat+1
				 * pat 6      never crosses (all positive)
				 * pat 7      crosses everywhere -> the k==5
				 *            give-up path
				 * pat 8..11  equal-and-opposite pairs, which
				 *            is the a+b == 0 divide guard
				 */
				if (pat < 6)
					v = (short)(k <= pat ? 900 - k * 40
							     : -700 + k * 30);
				else if (pat == 6)
					v = (short)(500 + k * 11);
				else if (pat == 7)
					v = (short)((k & 1) ? 800 : -800);
				else
					v = (short)((k & 1) ? 1000 : -1000);
				ra->timing_out[k] = rb->timing_out[k] = v;
			}

			ra->f1ac = rb->f1ac = (short)(ph * 700);
			ra->f1b0 = rb->f1b0 = (short)(wrap * 0x1f40);
			ra->f1ec = rb->f1ec = 0;
			ra->f1ee = rb->f1ee = 0;

			setInitialPhase(&oa);
			ref_setInitialPhase(&ob);

			{
				long tag = ((long)pat * 100 + ph * 10 + wrap);

				diff_eq_int("phase", ra->f1ac, rb->f1ac, tag);
				diff_eq_int("idx lo", ra->f1ec, rb->f1ec, tag);
				diff_eq_int("idx hi", ra->f1ee, rb->f1ee, tag);
			}
		}
	}
	rc |= diff_end();

	/*
	 * setTimingStateParameters: all nine states, both parameter tables,
	 * and states outside the table -- including negative ones, which the
	 * unsigned compare is what keeps from indexing behind it.
	 */
	diff_begin("v34 setTimingStateParameters");
	{
		static struct v34_object oa, ob;
		static const short states[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8,
						9, 100, -1, -8, 32767, -32768 };
		unsigned si;
		int var, d0;

		for (si = 0; si < sizeof(states) / sizeof(states[0]); si++)
		for (var = 0; var <= 1; var++)
		for (d0 = -3000; d0 <= 3000; d0 += 1500) {
			struct v34_receiver *ra, *rb;
			long tag = (long)si * 1000 + var * 100 + (d0 + 3000) / 1500;

			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			ra = (struct v34_receiver *)((char *)&oa + 0x264);
			rb = (struct v34_receiver *)((char *)&ob + 0x264);

			oa.f359c = ob.f359c = (short)(var ? 0x65 : 0x11);
			oa.faa96 = ob.faa96 = 400;
			ra->f1c0 = rb->f1c0 = states[si];
			ra->f1d0 = rb->f1d0 = (short)d0;
			ra->f232 = rb->f232 = 0;
			ra->f234 = rb->f234 = 0;
			ra->f236 = rb->f236 = 0;
			ra->f1d2 = rb->f1d2 = 0;
			oa.fac0c = ob.fac0c = 0;

			setTimingStateParameters(&oa);
			ref_setTimingStateParameters(&ob);

			diff_eq_int("sts f232", ra->f232, rb->f232, tag);
			diff_eq_int("sts f234", ra->f234, rb->f234, tag);
			diff_eq_int("sts f236", ra->f236, rb->f236, tag);
			diff_eq_int("sts f1d2", ra->f1d2, rb->f1d2, tag);
			diff_eq_int("sts offset", oa.fac0c, ob.fac0c, tag);
		}
	}
	rc |= diff_end();

	/*
	 * TimingV34: the state machine, the detector and the integrator, run
	 * long enough for the dwell counters to advance states and for the
	 * ppm report to fire.  f1c0 == -1 (done) and the f1c8 branch out of
	 * state 1 are both driven.
	 */
	diff_begin("v34 TimingV34");
	{
		static struct v34_object oa, ob;
		int st, skip, var, it, k;

		for (st = -1; st <= 8; st++)
		for (skip = 0; skip <= 1; skip++)
		for (var = 0; var <= 1; var++) {
			struct v34_receiver *ra, *rb;

			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			ra = (struct v34_receiver *)((char *)&oa + 0x264);
			rb = (struct v34_receiver *)((char *)&ob + 0x264);

			for (k = 0; k < 21; k++)
				ra->timing_out[k] = rb->timing_out[k] =
				    (short)(k <= 2 ? 900 - k * 40
						   : -700 + k * 30);

			oa.f359c = ob.f359c = (short)(var ? 0x65 : 0x11);
			oa.faa96 = ob.faa96 = 400;
			oa.fac0c = ob.fac0c = 0;
			ra->f1c0 = rb->f1c0 = (short)st;
			ra->f1c8 = rb->f1c8 = skip;
			ra->f1ec = rb->f1ec = 1;
			ra->f1ee = rb->f1ee = 2;
			ra->f1ac = rb->f1ac = 700;
			ra->f1ae = rb->f1ae = 0x3e80;
			ra->f1b0 = rb->f1b0 = 0x3e80;
			ra->f1be = rb->f1be = 0x3e80;
			ra->f1cc = rb->f1cc = 0;
			ra->f1ce = rb->f1ce = 0;
			ra->f1d0 = rb->f1d0 = 0;
			ra->f1d2 = rb->f1d2 = 40;
			ra->f1d8 = rb->f1d8 = 0;
			ra->f1e0 = rb->f1e0 = 0;
			ra->f230 = rb->f230 = 0;
			ra->f232 = rb->f232 = 0;
			ra->f234 = rb->f234 = 0;
			ra->f236 = rb->f236 = 0;

			for (it = 0; it < 120; it++) {
				long tag = (((long)(st + 1) * 10 + skip) * 10
					    + var) * 1000 + it;

				/* Perturb the metric so the loop has work. */
				ra->timing_out[1] = rb->timing_out[1] =
				    (short)(800 - it * 7);
				ra->timing_out[2] = rb->timing_out[2] =
				    (short)(-600 + it * 5);

				TimingV34(&oa);
				ref_TimingV34(&ob);

				diff_eq_int("tv state", ra->f1c0, rb->f1c0, tag);
				diff_eq_int("tv step",  ra->f1ae, rb->f1ae, tag);
				diff_eq_int("tv acc",  (long)ra->f1d8,
					    (long)rb->f1d8, tag);
				diff_eq_int("tv int",  (long)ra->f1e0,
					    (long)rb->f1e0, tag);
				diff_eq_int("tv ppm",   ra->f1d0, rb->f1d0, tag);
				diff_eq_int("tv dwell", ra->f230, rb->f230, tag);
				diff_eq_int("tv phase", ra->f1ac, rb->f1ac, tag);
			}
		}
	}
	rc |= diff_end();

	/*
	 * receiver: the whole per-symbol chain, staged by its three flag
	 * gates.  Stage one leaves 0x400, 0x1000 and 0x2000 clear, so the
	 * spine runs alone -- interpolate, equalise, derotate, slice against
	 * rxvect4, drive the carrier NCO, adapt the centre taps.
	 *
	 * DRIVEN THROUGH V34SetupDemodulator, not by hand.  `receiver` calls
	 * TimingV34, which recomputes f1ae from f1be every symbol, so a
	 * hand-set step survives exactly one call: with f1be left at the fill
	 * the step becomes garbage, every output wraps twice, and twelve
	 * pulls run the receive burst over +0x120..+0x126 -- f120, `flags`,
	 * f124 and best_index, which are the fields `receiver` then reads.
	 * That is finding 123's overrun, and it makes both sides agree on
	 * nonsense.  The real rates keep f1ae below f1b0, which is what
	 * bounds the pulls at one per output.
	 *
	 * TWO CONSTRAINTS ON f128, AND THEY AGREE.  timing_out[] holds seven
	 * entries before the predictor's coefficients begin at +0x288, and
	 * finding 123 measured fourteen shorts of receive-burst headroom.
	 * Seven outputs at up to two pulls each is fourteen samples, so one
	 * bound implies the other; both are asserted below.
	 */
	/*
	 * receiver: the whole per-symbol chain.  Built and brought up one
	 * flag gate at a time -- 0x400 (data mode and the retrain detector),
	 * 0x2000 (the precoder) and 0x1000 (the predictor and its LMS) -- and
	 * then swept together, because the two predictor gates share one set
	 * of coefficients and one history and a shared-state fault hides if
	 * only ever one is set.
	 *
	 * DRIVEN THROUGH V34SetupDemodulator, not by hand.  `receiver` calls
	 * TimingV34, which recomputes f1ae from f1be every symbol, so a
	 * hand-set step survives exactly one call: with f1be left at the fill
	 * the step becomes garbage, every output wraps twice, and twelve
	 * pulls run the receive burst over +0x120..+0x126 -- f120, `flags`,
	 * f124 and best_index, which are the fields `receiver` then reads.
	 * Both sides then agree on nonsense.  The real rates keep f1ae below
	 * f1b0, which is what bounds the pulls at one per output.
	 *
	 * TWO CONSTRAINTS ON f128, AND THEY AGREE.  timing_out[] holds seven
	 * entries before the predictor's coefficients begin at +0x288, and
	 * finding 123 measured fourteen shorts of receive-burst headroom.
	 * Seven outputs at up to two pulls each is fourteen samples, so one
	 * bound implies the other; both are asserted below.
	 *
	 * f124 IS DRIVEN, NOT OBSERVED.  It is `rxsymcnt`, and every
	 * threshold in the function is a comparison against it -- 0x11, 0x40,
	 * 0x68, 0x132, 0x143, 0x152, 0x153, 0x332, 0x7530.  Nothing inside
	 * `receiver` advances it past 2, so the sweep sets it outright.
	 */
	/*
	 * receiver: the whole per-symbol chain.  Built and brought up one
	 * flag gate at a time -- 0x400 (data mode and the retrain detector),
	 * 0x2000 (the precoder) and 0x1000 (the predictor and its LMS) -- and
	 * then swept together, because the two predictor gates share one set
	 * of coefficients and one history and a shared-state fault hides if
	 * only ever one is set.
	 *
	 * DRIVEN THROUGH V34SetupDemodulator, not by hand.  `receiver` calls
	 * TimingV34, which recomputes f1ae from f1be every symbol, so a
	 * hand-set step survives exactly one call: with f1be left at the fill
	 * the step becomes garbage, every output wraps twice, and twelve
	 * pulls run the receive burst over +0x120..+0x126 -- f120, `flags`,
	 * f124 and best_index, which are the fields `receiver` then reads.
	 * Both sides then agree on nonsense.  The real rates keep f1ae below
	 * f1b0, which is what bounds the pulls at one per output.
	 *
	 * TWO CONSTRAINTS ON f128, AND THEY AGREE.  timing_out[] holds seven
	 * entries before the predictor's coefficients begin at +0x288, and
	 * finding 123 measured fourteen shorts of receive-burst headroom.
	 * Seven outputs at up to two pulls each is fourteen samples, so one
	 * bound implies the other; both are asserted below.
	 *
	 * f124 IS DRIVEN, NOT OBSERVED.  It is `rxsymcnt`, and every
	 * threshold in the function is a comparison against it -- 0x11, 0x40,
	 * 0x68, 0x132, 0x143, 0x152, 0x153, 0x332, 0x7530.  Nothing inside
	 * `receiver` advances it past 2, so the sweep sets it outright.
	 *
	 * THE TWEAKS EXIST BECAUSE SIX BRANCHES ARE OTHERWISE UNREACHABLE
	 * from any starting state a caller could produce in 72 symbols: the
	 * two-pull path needs a step wider than the wrap, the retrain and
	 * renegotiation flags need a counter 140 symbols deep, the equaliser
	 * error report needs 1024, its saturation needs an accumulator
	 * already overflowed, and the shifted-TRN2 branch needs an equaliser
	 * whose energy has settled late.  Each is seeded rather than waited
	 * for, and a temporary counter per branch was used to confirm every
	 * one of them is reached.
	 */
	diff_begin("v34 receiver");
	{
		static struct v34_object oa, ob;
		static const struct { short baud, carrier; } rates[] = {
			{ 2400, 1800 }, { 3200, 1920 },
		};
		/*
		 * The symbol counts either side of every threshold, plus the
		 * sixteen-symbol window 0x143..0x152 where TRN runs a second
		 * scrambler, and 0x153 where the shifted-TRN2 check fires.
		 */
		static const short syms[] = {
			0, 1, 2, 0x11, 0x12, 0x40, 0x41, 0x68, 0x69,
			0x132, 0x133, 0x142, 0x143, 0x14a, 0x152, 0x153,
			0x154, 0x212, 0x213, 0x332, 0x333, 0x400,
			0x7530, 0x7531,
		};
#define RXT_WIDE_STEP	0x01	/* f1ae > f1b0: the two-pull path        */
#define RXT_DEAD_EQ	0x02	/* zero taps: the point stops moving     */
#define RXT_RTN_UP	0x04	/* seed rtncount just under its trip     */
#define RXT_RTN_DOWN	0x08	/* and inside the renegotiation band     */
#define RXT_REPORT	0x10	/* seed the 1024-symbol error report     */
#define RXT_SATURATE	0x20	/* with both accumulators overflowed     */
#define RXT_LATE_EQ	0x40	/* energy in taps 60..75, not 32..47     */
#define RXT_NO_SIGNAL	0x80	/* a floor the burst cannot clear        */
		static const struct {
			unsigned short flags;
			short gain;
			unsigned char tweak;
		} cases[] = {
		  { 0,                                        0x0400, 0 },
		  { V34_RX_FLAG_PRECODE,                      0x0400, 0 },
		  { V34_RX_FLAG_PREDICT,                      0x0400, 0 },
		  { V34_RX_FLAG_PRECODE | V34_RX_FLAG_PREDICT, 0x0400, 0 },
		  { V34_RX_FLAG_DATA,                         0x0400, 0 },
		  { V34_RX_FLAG_DATA | V34_RX_FLAG_TRN_WATCH, 0x0400, 0 },
		  { V34_RX_FLAG_DATA | V34_RX_FLAG_LATE_TRN,  0x0400, 0 },
		  { V34_RX_FLAG_DATA | V34_RX_FLAG_TRN_WATCH
		    | V34_SCR_ANSWERER,                       0x0400, 0 },
		  { V34_RX_FLAG_DATA | V34_RX_FLAG_PRECODE
		    | V34_RX_FLAG_PREDICT,                    0x0400, 0 },
		  /* Frozen AGC and 16x gain: the only way the slicer error
		   * clears 0x600 and the S-S1 reset fires. */
		  { V34_RX_FLAG_DET_PENDING,                  0x4000, 0 },
		  { V34_RX_FLAG_DET_PENDING | V34_RX_FLAG_PRECODE
		    | V34_RX_FLAG_PREDICT,                    0x4000, 0 },
		  { V34_RX_FLAG_DET_PENDING,                  0x4000,
		    RXT_NO_SIGNAL },
		  { 0,                                        0x0400,
		    RXT_WIDE_STEP },
		  { V34_RX_FLAG_DATA | V34_RX_FLAG_PRECODE,   0x0400,
		    RXT_WIDE_STEP },
		  { V34_RX_FLAG_DATA,                         0x0400,
		    RXT_DEAD_EQ | RXT_RTN_UP },
		  { V34_RX_FLAG_DATA,                         0x0400,
		    RXT_RTN_DOWN },
		  { V34_RX_FLAG_PREDICT,                      0x0400,
		    RXT_REPORT },
		  { V34_RX_FLAG_DATA | V34_RX_FLAG_PREDICT,   0x0400,
		    RXT_REPORT | RXT_SATURATE },
		  { V34_RX_FLAG_DATA | V34_RX_FLAG_TRN_WATCH, 0x0400,
		    RXT_LATE_EQ },
		};
		unsigned sw, cs, b;
		int k, it;

		for (sw = 0; sw < sizeof(rates) / sizeof(rates[0]); sw++)
		for (cs = 0; cs < sizeof(cases) / sizeof(cases[0]); cs++) {
			struct v34_receiver *ra, *rb;
			struct v34_equalizer *qa2, *qb2;
			unsigned tweak = cases[cs].tweak;

			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			V34InitializeImplementationSpecific(&oa);
			ref_V34InitializeImplementationSpecific(&ob);
			txinit(&oa); ref_txinit(&ob);
			rxinit(&oa); ref_rxinit(&ob);
			rxtiminginit(&oa); ref_rxtiminginit(&ob);
			V34SetupDemodulator(&oa, rates[sw].baud,
					    rates[sw].carrier);
			ref_V34SetupDemodulator(&ob, rates[sw].baud,
						rates[sw].carrier);

			ra = (struct v34_receiver *)((char *)&oa + 0x264);
			rb = (struct v34_receiver *)((char *)&ob + 0x264);
			qa2 = (struct v34_equalizer *)((char *)&oa + 0x630);
			qb2 = (struct v34_equalizer *)((char *)&ob + 0x630);

			for (b = 0; b < V34_RXQ_RING; b++)
				((struct v34_queue *)ra)->ring[b] =
				((struct v34_queue *)rb)->ring[b] =
				    (int)((unsigned)(unsigned short)
					  (short)(b * 2731 - 12000)
					  | ((unsigned)(unsigned short)
					     (short)(b * 991 - 8000) << 16));
			((struct v34_queue *)ra)->count =
			((struct v34_queue *)rb)->count = V34_RXQ_RING;

			/*
			 * The RMS index, as in the rxtiming fixture: neither
			 * init writes it, dpskinit and v34modeminit do.
			 */
			ra->f19c = rb->f19c = 0;
			ra->agc_gain = rb->agc_gain = cases[cs].gain;
			ra->agc_step = rb->agc_step = 0x3333;

			/*
			 * Past state 1, so TimingV34 neither re-seeds the
			 * phase nor advances a state; f232 == -1 disables the
			 * dwell.  At 1 or below `receiver` returns before the
			 * slicer, which would leave two thirds of it untested.
			 */
			ra->f1c0 = rb->f1c0 = 4;
			ra->f232 = rb->f232 = -1;
			ra->f234 = rb->f234 = 0x1000;
			ra->f236 = rb->f236 = 0x0800;
			ra->f1ec = rb->f1ec = 1;
			ra->f1ee = rb->f1ee = 2;

			/*
			 * The predictor's own state.  Nothing in either init
			 * touches +0x288..+0x2a3, so at the fill the taps are
			 * -23131 and the prediction saturates on the first
			 * symbol -- identically on both sides, which is a
			 * pass that measures nothing.
			 */
			for (k = 0; k < 3; k++) {
				ra->pred_b[k] = rb->pred_b[k] =
				    (short)(0x0c00 >> k);
				ra->pred_a[k] = rb->pred_a[k] =
				    (short)(-0x0300 >> k);
			}
			for (k = 0; k < 4; k++)
				ra->pred_i[k] = rb->pred_i[k] =
				ra->pred_q[k] = rb->pred_q[k] = 0;

			ra->f268 = rb->f268 = 0;
			ra->f26a = rb->f26a = 0;
			ra->f26c = rb->f26c = 0;
			ra->f26e = rb->f26e = 0;
			ra->f798 = rb->f798 = 0;

			oa.rx_energy_floor = ob.rx_energy_floor =
			    (tweak & RXT_NO_SIGNAL) ? 0x40000000 : 900;
			oa.status = ob.status = 0;

			if (tweak & RXT_WIDE_STEP) {
				/*
				 * A step a quarter wider than the wrap, which
				 * TimingV34 will keep reproducing.  Eight
				 * pulls at four outputs still fits the burst.
				 */
				ra->f1be = rb->f1be = 20000;
				ra->f1ae = rb->f1ae = 20000;
			}
			if (tweak & RXT_DEAD_EQ) {
				memset(qa2->re, 0, sizeof qa2->re);
				memset(qb2->re, 0, sizeof qb2->re);
				memset(qa2->im, 0, sizeof qa2->im);
				memset(qb2->im, 0, sizeof qb2->im);
			}
			if (tweak & RXT_LATE_EQ)
				for (k = 60; k < 76; k++) {
					qa2->re[k] = qb2->re[k] = 0x2000;
					qa2->im[k] = qb2->im[k] = -0x1800;
				}
			if (tweak & RXT_REPORT)
				ra->f21c = rb->f21c = 0x3fd;
			if (tweak & RXT_SATURATE) {
				ra->f220 = rb->f220 = 0x7ffffff0;
				ra->f228 = rb->f228 = 0x7ffffff0;
			}

			ra->flags = rb->flags = cases[cs].flags;

			for (it = 0; it < (int)(sizeof(syms) / sizeof(syms[0]))
					   * 3; it++) {
				long tag = ((long)sw * 100 + cs) * 10000000
					 + (long)it * 100000;

				/*
				 * The count is re-imposed every symbol: the
				 * handshake path writes 1 or 2 into it and
				 * the S-S1 reset writes 0, so leaving it
				 * alone would collapse the sweep to those.
				 */
				ra->f124 = rb->f124 =
				    syms[it % (sizeof(syms) / sizeof(syms[0]))];
				if (tweak & RXT_RTN_UP)
					ra->f798 = rb->f798 = 0x8c;
				if (tweak & RXT_RTN_DOWN)
					ra->f798 = rb->f798 =
					    (short)((it & 1) ? -0x7c : -0x85);

				receiver(&oa);
				ref_receiver(&ob);

				if ((char *)ra->rx_samples
				    > (char *)&ra->f120) {
					printf("FIXTURE: the burst reached "
					       "+0x120 -- it is eating "
					       "flags, not spare buffer\n");
					return 1;
				}
				if (ra->f128 > 7) {
					printf("FIXTURE: f128 > 7 runs "
					       "timing_out into the "
					       "predictor\n");
					return 1;
				}

				for (b = 0; b < sizeof(oa); b++) {
					static const unsigned skip[][2] = {
					  { 0x264 + 0x04, 8 },   /* rxq rd/wr */
					  { 0x264 + 0x130, 4 },  /* samples   */
					  { 0x264 + 0x1b4, 4 },  /* carrier   */
					  { 0x264 + 0x2a4, 4 },  /* f2a4      */
					  { 0x50c + __builtin_offsetof(
					      struct v34_timing,
					      prefilter_coeff), 8 },
					  { 0x221c + __builtin_offsetof(
					      struct v34_queue, rd), 8 },
					  { 0x2074, 4 },
					  { 0x2078 + __builtin_offsetof(
					      struct v34_echo_prefilter,
					      coeff), 4 },
					  { 0x80b8, 0x18 },  /* echo0 ptrs */
					  { 0x9138, 0x18 },  /* echo1 ptrs */
					};
					unsigned s2, hit = 0;

					for (s2 = 0; s2 < sizeof(skip)
						     / sizeof(skip[0]); s2++)
						if (b >= skip[s2][0]
						    && b < skip[s2][0]
							   + skip[s2][1])
							hit = 1;
					if (hit)
						continue;
					diff_eq_int("receiver at %ld",
						    ((unsigned char *)&oa)[b],
						    ((unsigned char *)&ob)[b],
						    tag + b);
				}
				diff_eq_int("receiver rxq rd",
				    (long)(((struct v34_queue *)ra)->rd
					   - ((struct v34_queue *)ra)->ring),
				    (long)(((struct v34_queue *)rb)->rd
					   - ((struct v34_queue *)rb)->ring),
				    tag);
				diff_eq_int("receiver samples",
				    (long)((char *)ra->rx_samples - (char *)ra),
				    (long)((char *)rb->rx_samples - (char *)rb),
				    tag);
				for (k = 0; k < 7; k++)
					diff_eq_int("receiver timing out",
						    ra->timing_out[k],
						    rb->timing_out[k],
						    tag + 90000 + k);
			}
		}
	}
	rc |= diff_end();

	/*
	 * And the seven diagnostic paths, which are seven of this function's
	 * own annotations and are dead code at level 1.  Finding 134: a
	 * dropped call site is a dropped annotation, and an untested one is
	 * finding 126 -- the only reconstruction so far whose debug string
	 * was wrong was the one nothing drove.
	 */
	diff_begin("v34 receiver debug transcript");
	{
		static struct v34_object oa, ob;
		static const short syms[] = { 0x11, 0x40, 0x69, 0x143, 0x153,
					      0x333, 0x7531 };
		unsigned cs, b;
		int it, saw;

		dsplibs_debug_level = 2;
		ref_dsplibs_debug_level = 2;
		dsplib_debug_capture_on = 1;

		for (cs = 0; cs < 6; cs++) {
			struct v34_receiver *ra, *rb;
			struct v34_equalizer *qa2, *qb2;

			memset(&oa, HARNESS_MALLOC_FILL, sizeof(oa));
			memset(&ob, HARNESS_MALLOC_FILL, sizeof(ob));
			V34InitializeImplementationSpecific(&oa);
			ref_V34InitializeImplementationSpecific(&ob);
			txinit(&oa); ref_txinit(&ob);
			rxinit(&oa); ref_rxinit(&ob);
			rxtiminginit(&oa); ref_rxtiminginit(&ob);
			V34SetupDemodulator(&oa, 3000, 1800);
			ref_V34SetupDemodulator(&ob, 3000, 1800);

			ra = (struct v34_receiver *)((char *)&oa + 0x264);
			rb = (struct v34_receiver *)((char *)&ob + 0x264);
			qa2 = (struct v34_equalizer *)((char *)&oa + 0x630);
			qb2 = (struct v34_equalizer *)((char *)&ob + 0x630);

			for (b = 0; b < V34_RXQ_RING; b++)
				((struct v34_queue *)ra)->ring[b] =
				((struct v34_queue *)rb)->ring[b] =
				    (int)((unsigned)(unsigned short)
					  (short)(b * 2731 - 12000)
					  | ((unsigned)(unsigned short)
					     (short)(b * 991 - 8000) << 16));
			((struct v34_queue *)ra)->count =
			((struct v34_queue *)rb)->count = V34_RXQ_RING;

			ra->f19c = rb->f19c = 0;
			ra->agc_gain = rb->agc_gain = 0x4000;
			ra->agc_step = rb->agc_step = 0x3333;
			ra->f1c0 = rb->f1c0 = 4;
			ra->f232 = rb->f232 = -1;
			ra->f234 = rb->f234 = 0x1000;
			ra->f236 = rb->f236 = 0x0800;
			ra->f1ec = rb->f1ec = 1;
			ra->f1ee = rb->f1ee = 2;
			memset(ra->pred_b, 0, 12); memset(rb->pred_b, 0, 12);
			memset(ra->pred_i, 0, 16); memset(rb->pred_i, 0, 16);
			ra->f268 = rb->f268 = 0; ra->f26a = rb->f26a = 0;
			ra->f26c = rb->f26c = 0; ra->f26e = rb->f26e = 0;
			ra->f798 = rb->f798 = 0;
			ra->f21c = rb->f21c = 0x3fd;
			oa.status = ob.status = 0;

			/*
			 * cs picks which annotation is reachable: the retrain
			 * pair needs a dead equaliser and a primed counter,
			 * the shifted-TRN2 report needs energy late in the
			 * taps, and the disconnection needs a floor nothing
			 * clears.
			 */
			oa.rx_energy_floor = ob.rx_energy_floor =
			    (cs == 3) ? 0x40000000 : 900;
			if (cs == 1 || cs == 2) {
				memset(qa2->re, 0, sizeof qa2->re);
				memset(qb2->re, 0, sizeof qb2->re);
				memset(qa2->im, 0, sizeof qa2->im);
				memset(qb2->im, 0, sizeof qb2->im);
			}
			if (cs == 4)
				for (b = 60; b < 76; b++) {
					qa2->re[b] = qb2->re[b] = 0x2000;
					qa2->im[b] = qb2->im[b] = -0x1800;
				}
			ra->flags = rb->flags = (unsigned short)
			    ((cs == 0 || cs == 3) ? V34_RX_FLAG_DET_PENDING
			     : cs == 4 ? (V34_RX_FLAG_DATA
					  | V34_RX_FLAG_TRN_WATCH)
			     : cs == 5 ? V34_RX_FLAG_PREDICT
			     : V34_RX_FLAG_DATA);

			saw = 0;
			for (it = 0; it < 28; it++) {
				long tag = (long)cs * 1000 + it;

				ra->f124 = rb->f124 =
				    syms[it % (sizeof(syms)/sizeof(syms[0]))];
				if (cs == 1)
					ra->f798 = rb->f798 = 0x8c;
				if (cs == 2)
					ra->f798 = rb->f798 =
					    (short)((it & 1) ? -0x7c : -0x85);

				dsplib_debug_capture_reset();
				receiver(&oa);
				ref_receiver(&ob);
				diff_eq_int("receiver transcript",
				    strcmp(dsplib_debug_capture_text(0),
					   dsplib_debug_capture_text(1)) == 0,
				    1, tag);
				if (dsplib_debug_capture_lines(1) > 0)
					saw = 1;
				for (b = 0; b < sizeof(oa); b++) {
					if (b >= 0x264 + 0x04 && b < 0x264 + 0x0c)
						continue;
					if (b >= 0x264 + 0x130 && b < 0x264 + 0x134)
						continue;
					if (b >= 0x264 + 0x1b4 && b < 0x264 + 0x1b8)
						continue;
					if (b >= 0x264 + 0x2a4 && b < 0x264 + 0x2a8)
						continue;
					if (b >= 0x620 && b < 0x628)
						continue;
					if (b >= 0x221c + 4 && b < 0x221c + 0xc)
						continue;
					if (b >= 0x2074 && b < 0x2078)
						continue;
					if (b >= 0x2078 + __builtin_offsetof(
						  struct v34_echo_prefilter,
						  coeff)
					    && b < 0x2078 + __builtin_offsetof(
						  struct v34_echo_prefilter,
						  coeff) + 4)
						continue;
					if (b >= 0x80b8 && b < 0x80b8 + 0x18)
						continue;
					if (b >= 0x9138 && b < 0x9138 + 0x18)
						continue;
					diff_eq_int("receiver debug state %ld",
						    ((unsigned char *)&oa)[b],
						    ((unsigned char *)&ob)[b],
						    tag * 100000 + b);
				}
			}

			/*
			 * Each case must actually have said something --
			 * checked across the whole case, not on whatever the
			 * last symbol happened to print.  An empty transcript
			 * compares equal to an empty transcript, which is
			 * finding 122's failure mode in another costume.
			 */
			diff_eq_int("receiver transcript non-empty",
				    saw, 1, (long)cs);
		}

		dsplib_debug_capture_on = 0;
		dsplibs_debug_level = 0;
		ref_dsplibs_debug_level = 0;
	}
	rc |= diff_end();

	return rc;
}
