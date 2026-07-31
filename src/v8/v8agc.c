/*
 * v8agc.c -- the receive front end.
 *
 * One pass over each block of four samples: take them out of the symbol
 * buffer, run them through a 40-tap filter chosen by which end of the call
 * this is, apply the current gain with saturation, and adapt that gain from
 * the energy just measured.
 *
 * The adaptation at the end is v8_agcadapt with one substitution -- the level
 * comes from the energy of these four samples rather than from the field that
 * function reads.  They are the two halves of one int, which is why that
 * field is a short of its own and not part of a wider one.
 */

#include "dsplib/v8.h"

static const short agc_taps_answer[40] = {
	   -15,      5,     27,     33,      0,    -62,    -93,    -26,
	   122,    218,    108,   -197,   -445,   -310,    270,    886,
	   842,   -323,  -2358,  -4317,  11279,  -4317,  -2358,   -323,
	   842,    886,    270,   -310,   -445,   -197,    108,    218,
	   122,    -26,    -93,    -62,      0,     33,     27,      5
};

static const short agc_taps_caller[40] = {
	    25,      6,    -11,    -12,      5,     13,    -37,   -156,
	  -257,   -181,    165,    654,    932,    642,   -245,  -1285,
	 -1776,  -1250,    122,   1560,   2170,   1560,    122,  -1250,
	 -1776,  -1285,   -245,    642,    932,    654,    165,   -181,
	  -257,   -156,    -37,     13,      5,    -12,    -11,      6
};

static const short agc_gain[192] = {
	 16384,  16511,  16638,  16763,  16888,  17011,  17134,  17256,
	 17377,  17498,  17617,  17736,  17854,  17971,  18087,  18203,
	 18317,  18432,  18545,  18658,  18770,  18881,  18992,  19102,
	 19211,  19320,  19429,  19536,  19643,  19750,  19856,  19961,
	 20066,  20170,  20274,  20377,  20480,  20582,  20683,  20784,
	 20885,  20985,  21085,  21184,  21283,  21381,  21479,  21577,
	 21673,  21770,  21866,  21962,  22057,  22152,  22246,  22341,
	 22434,  22528,  22620,  22713,  22805,  22897,  22988,  23079,
	 23170,  23260,  23350,  23440,  23529,  23618,  23707,  23795,
	 23883,  23971,  24058,  24145,  24232,  24318,  24404,  24490,
	 24576,  24661,  24746,  24830,  24914,  24999,  25082,  25166,
	 25249,  25332,  25415,  25497,  25579,  25661,  25742,  25824,
	 25905,  25986,  26066,  26147,  26227,  26307,  26386,  26465,
	 26545,  26624,  26702,  26781,  26859,  26937,  27014,  27092,
	 27169,  27246,  27323,  27400,  27476,  27553,  27629,  27704,
	 27780,  27855,  27930,  28005,  28080,  28155,  28229,  28303,
	 28377,  28451,  28525,  28598,  28672,  28745,  28817,  28890,
	 28963,  29035,  29107,  29179,  29251,  29322,  29394,  29465,
	 29536,  29607,  29678,  29748,  29819,  29889,  29959,  30029,
	 30099,  30168,  30238,  30307,  30376,  30445,  30514,  30583,
	 30651,  30720,  30788,  30856,  30924,  30991,  31059,  31126,
	 31194,  31261,  31328,  31395,  31461,  31528,  31595,  31661,
	 31727,  31793,  31859,  31925,  31990,  32056,  32121,  32186,
	 32251,  32316,  32381,  32446,  32510,  32575,  32639,  32703
};

/* Where the gain table runs out. */
#define V8_AGC_GAIN_MAX		0xbf

/* Below this the block is too quiet to adapt from. */
#define V8_AGC_FLOOR		0x1f

/* How many saturating blocks before the gain is forced back down. */
#define V8_AGC_CLIP_LIMIT	0xa

int
V8agc(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	const short *taps = v->mode != 0 ? agc_taps_caller : agc_taps_answer;
	int energy = 0;
	int gain = 0;
	int i;

	v8_rxreadqueue(v);
	r->buf = v->rx_stage + V8_QUEUE_BLOCK;

	/* The band filter, a sample at a time, shifting its own line. */
	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		int acc = 0;
		int j;

		v->agc_line[0] = v->rx_stage[i];
		for (j = 0; j < V8_AGC_TAPS; j++)
			acc += v->agc_line[j] * taps[j];
		v->rx_stage[i] = (short)(acc >> 14);

		for (j = V8_AGC_TAPS - 1; j > 0; j--)
			v->agc_line[j] = v->agc_line[j - 1];
	}

	/* Into the running history, which wraps at 36. */
	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		short at = r->f82;

		r->f82 = (short)(at + 1);
		r->hist[at] = v->rx_stage[i];
		if (at + 1 > V8_AGC_HIST)
			r->f82 = 0;
	}

	/* Its energy, each term pre-scaled so the sum cannot overflow. */
	for (i = 0; i < V8_AGC_HIST; i++) {
		int x = r->hist[i];

		energy += ((x * 0x38e) >> 15) * x;
	}

	if (energy != 0) {
		unsigned acc = (unsigned)energy;
		int shift = 0;
		int e;
		int idx;

		/* Normalise, remembering by how much. */
		while (acc <= 0x1fffffff) {
			acc += acc;
			shift++;
		}
		e = (int)(acc >> 15);
		if (shift & 1)
			e = (unsigned short)e >> 1;

		idx = (((unsigned short)e + 0x40) >> 7) - 0x40;
		if ((unsigned short)idx > V8_AGC_GAIN_MAX)
			idx = V8_AGC_GAIN_MAX;

		gain = (unsigned short)agc_gain[(unsigned short)idx]
		       >> (shift >> 1);
	}

	/*
	 * Apply the gain, saturating rather than wrapping.  A run of
	 * saturating blocks forces the gain back to a known value, and if the
	 * handshake was waiting on the tone detector it is restarted -- the
	 * clip means whatever it thought it heard was an artefact.
	 */
	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		int scaled = r->f1c * v->rx_stage[i];
		unsigned top = (unsigned)scaled >> 25;

		if (top == 0 || top == 0x7f) {
			v->rx_stage[i] = (short)(scaled >> 10);
			continue;
		}
		v->rx_stage[i] = (short)((scaled >> 16) > 0 ? 0x7f00 : 0x8100);

		r->fac = (short)(r->fac + 1);
		if (r->fac != V8_AGC_CLIP_LIMIT)
			continue;
		r->f1c = 0x400;
		if (v->f9d8 != 0x24)
			continue;
		v->f9d8 = 0x19;
		v->detector.f08 = 0;
		v->detector.f30 = 0;
		r->fac = 0;
	}

	if ((short)gain <= V8_AGC_FLOOR)
		return 0;

	/* Adapt, from the energy of the four samples just scaled. */
	energy = 0;
	for (i = 0; i < V8_QUEUE_BLOCK; i++)
		energy += v->rx_stage[i] * v->rx_stage[i];
	r->f14 = (short)energy;
	r->f16 = (short)(energy >> 16);

	return v8_agcadapt(v);
}

void
checkSignalStability(struct v8 *v)
{
	struct v8_rx *r = &v->rx;
	int elapsed = (unsigned short)r->f84 + 4;
	int settled;
	int delta;

	if ((short)elapsed > V8_STABLE_PERIOD) {
		/*
		 * Time to refresh the reference.  Note that the comparison
		 * below then measures the gain against the value just taken
		 * from it, so it is always zero on this pass -- the original
		 * does the store first and the arithmetic afterwards.
		 */
		r->f84 = 0;
		r->f86 = r->f1c;
	} else {
		r->f84 = (short)elapsed;
	}

	/*
	 * The relative change, in Q14.  A zero reference would divide by zero
	 * in the original; nothing reaches it, because the gain is only ever
	 * this function's reference after having been non-zero.
	 */
	if (r->f86 == 0)
		delta = 0;
	else
		delta = (short)((((int)r->f1c - r->f86) << 14) / r->f86);

	if (delta < 0)
		delta = -(short)delta;

	if ((short)delta > V8_STABLE_TOLERANCE) {
		r->f88 = 0;
		r->f8a = 0;
		return;
	}

	settled = (unsigned short)r->f88 + 4;
	r->f88 = (short)settled;
	if ((short)settled > V8_STABLE_PERIOD)
		r->f8a = 1;
}
