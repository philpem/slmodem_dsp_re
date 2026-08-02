/*
 * t_v34rx.c -- differential test of the V.34 receiver/transmitter cores.
 */
#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "dsplib/v34rx.h"

extern void ref_rxreadqueue(void *q);
extern void ref_txwritequeue(void *q, const short *src);
extern int ref_bitreverse(unsigned short v, short nbits);
extern void ref_decision(void *d, const int *pts, short npts);
extern void ref_V34nlencoder(const short *in, short *out);
extern void ref_updateAlpha(short *a, int e, int d, int g, int dec, int t);

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
		static struct v34_decoder da, db;
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
			diff_eq_int("best point", da.best_point,
				    db.best_point, (long)n * 100000 + t);
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
		diff_eq_int("zero points, point", da.best_point,
			    db.best_point, 0);
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

	return rc;
}
