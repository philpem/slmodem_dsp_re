/*
 * t_v34rx.c -- differential test of the V.34 receiver/transmitter cores.
 */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "dsplib/v34filt.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34recv.h"
#include "dsplib/v34rx.h"

extern void ref_rxreadqueue(void *q);
extern void ref_txwritequeue(void *q, const short *src);
extern int ref_bitreverse(unsigned short v, short nbits);
extern void ref_decision(void *d, const int *pts, short npts);
extern void ref_V34nlencoder(const short *in, short *out);
extern void ref_updateAlpha(short *a, int e, int d, int g, int dec, int t);
extern int ref_V34descrambler(void *s, short bits, short nbits);
extern void ref_txinit(void *obj);
extern int ref_agcadapt(void *a);
extern void ref_rxtiminginit(void *obj);
extern void ref_rxinit(void *obj);
extern void ref_txmit(void *obj);
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
			diff_eq_int("best point", da.decision_point,
				    db.decision_point, (long)n * 100000 + t);
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
		diff_eq_int("zero points, point", da.decision_point,
			    db.decision_point, 0);
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

			updateAlpha(&aa, energies[e], ap, g, dc, 7);
			ref_updateAlpha(&ab, energies[e], ap, g, dc, 7);
			diff_eq_int("alpha", aa, ab,
				    (long)energies[e] * 31 + g);
		}
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
			  { __builtin_offsetof(struct v34_object, echo0), 0x20 },
			  { __builtin_offsetof(struct v34_object, echo1), 0x20 },
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
			aa.agc_input = ab.agc_input = (unsigned short)in;
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
			    ((struct v34_receiver *)((char *)&oa + 0x264))->baud,
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
				 * D34: seeded from the object's own address,
				 * so the two sides differ legitimately.
				 */
				unsigned ac = 0x264 + __builtin_offsetof(
					struct v34_receiver, agc_accum);

				if ((b >= rs && b < rs + 4)
				    || (b >= ac && b < ac + 2))
					continue;
				diff_eq_int("rxinit at %ld",
					    ((unsigned char *)&oa)[b],
					    ((unsigned char *)&ob)[b],
					    (long)fl * 100000 + b);
			}
			diff_eq_int("agc_accum is the hilbert address",
				    ((struct v34_receiver *)
				     ((char *)&oa + 0x264))->agc_accum,
				    (short)(unsigned long)
				    ((char *)&oa + 0xa1b8), fl);
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

	return rc;
}
