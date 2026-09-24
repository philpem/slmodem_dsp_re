/*
 * V8Fsk.c -- bringing up the V.21 modem that V.8 signals over.
 *
 * V.8's CM and JM ride on a 300 baud V.21 link, and this is what configures
 * and drives it from this side: V8_V21_Init picks the channel and role and
 * v8_fskmodulate turns bits into shaped samples.  Two independent choices are
 * made here: which of V.21's two channels this modem transmits on, and
 * whether it answered the call.  They pick different things -- the channel
 * picks the carrier constants and the 61-tap filter, the role picks the four
 * filter designs and two more constants -- and the four combinations are all
 * reachable.  The delay line, filter selection and receive correlator are
 * `V8Dpsk.c`'s, which is where the object keeps them.
 */

#include "dsplib/v8.h"

static const short temp_v21_hibnd[V8_V21_TAPS] = {
	   -14,      9,     33,     20,    -28,    -48,    -10,     25,
	     5,      3,     82,    105,    -80,   -283,   -153,    223,
	   333,     50,   -126,     31,    -33,   -572,   -645,    536,
	  1745,    910,  -1598,  -2645,   -433,   2566,   2566,   -433,
	 -2645,  -1598,    910,   1745,    536,   -645,   -572,    -33,
	    31,   -126,     50,    333,    223,   -153,   -283,    -80,
	   105,     82,      3,      5,     25,    -10,    -48,    -28,
	    20,     33,      9,    -14,      0
};

static const short temp_v21_lobnd[V8_V21_TAPS] = {
	   -12,      8,     23,     25,     13,      0,      6,     35,
	    61,     38,    -54,   -176,   -236,   -164,     11,    164,
	   176,     59,    -22,    118,    475,    771,    614,   -167,
	 -1273,  -2027,  -1796,   -487,   1291,   2550,   2550,   1291,
	  -487,  -1796,  -2027,  -1273,   -167,    614,    771,    475,
	   118,    -22,     59,    176,    164,     11,   -164,   -236,
	  -176,    -54,     38,     61,     35,      6,      0,     13,
	    25,     23,      8,    -12,      0
};

/* The blob's eight local objects are 82 bytes each, separated by 14 bytes
 * of alignment padding.  v8_fskdemodulate reads indices 0..39; retain the
 * final coefficient at index 40 because it belongs to the stored design. */
static const short HM_filter1_coef[V8_V21_FILTER_TAPS] = {
	   220,    112,   -160,   -367,   -184,    394,    770,    282,
	  -832,  -1333,   -315,   1424,   1906,    223,  -2053,  -2333,
	     0,   2572,   2499,   -300,  -2844,  -2365,    586,   2791,
	  1977,   -768,  -2422,  -1442,    789,   1833,    898,   -657,
	 -1176,   -458,    436,    617,    184,   -234,   -282,    -69,
	   155
};

static const short HM_filter2_coef[V8_V21_FILTER_TAPS] = {
	     0,    210,    239,    -36,   -444,   -480,    153,    930,
	   832,   -404,  -1585,  -1168,    789,   2263,   1372,  -1247,
	 -2795,  -1375,   1670,   3045,   1178,  -1941,  -2948,   -847,
	  1977,   2531,    482,  -1758,  -1906,   -181,   1344,   1228,
	     0,   -857,   -653,     61,    444,    285,    -56,   -228,
	  -155
};

static const short HS_filter1_coef[V8_V21_FILTER_TAPS] = {
	   220,     84,   -216,   -326,     63,    604,    436,   -566,
	 -1136,   -137,   1450,   1344,   -789,  -2273,   -794,   2045,
	  2421,   -475,  -2948,  -1616,   1874,   2928,    197,  -2657,
	 -1977,   1092,   2464,    660,  -1636,  -1564,    315,   1374,
	   588,   -617,   -744,    -20,    444,    252,   -127,   -237,
	   -57
};

static const short HS_filter2_coef[V8_V21_FILTER_TAPS] = {
	     0,    223,    190,   -174,   -476,   -141,    653,    790,
	  -304,  -1386,   -715,   1259,   1906,    -74,  -2339,  -1678,
	  1398,   2878,    586,  -2599,  -2442,    888,   2999,   1204,
	 -1977,  -2409,    162,   2176,   1256,   -972,  -1585,   -227,
	  1019,    751,   -252,   -620,   -184,    269,    258,    -23,
	  -212
};

static const short LM_filter1_coef[V8_V21_FILTER_TAPS] = {
	   220,    191,     82,   -128,   -403,   -619,   -597,   -215,
	   478,   1215,   1603,   1319,    323,  -1059,  -2229,  -2594,
	 -1871,   -267,   1570,   2842,   2973,   1894,     79,  -1683,
	 -2659,  -2505,  -1399,     89,   1298,   1785,   1493,    712,
	  -123,   -660,   -772,   -556,   -218,     63,    209,    236,
	   190
};

static const short LM_filter2_coef[V8_V21_FILTER_TAPS] = {
	     0,    142,    276,    346,    262,    -41,   -510,   -948,
	 -1074,   -680,    211,   1285,   2037,   2012,   1063,   -516,
	 -2077,  -2904,  -2563,  -1134,    797,   2403,   3005,   2382,
	   864,   -850,  -2035,  -2272,  -1603,   -453,    619,   1197,
	  1170,    714,    143,   -274,   -428,   -364,   -198,    -28,
	   110
};

static const short LS_filter1_coef[V8_V21_FILTER_TAPS] = {
	   220,    170,      8,   -251,   -480,   -466,    -62,    622,
	  1170,   1094,    211,  -1102,  -2037,  -1857,   -450,   1470,
	  2734,   2467,    702,  -1565,  -2973,  -2670,   -854,   1358,
	  2659,   2372,    824,   -952,  -1926,  -1692,   -619,    516,
	  1074,    912,    338,   -199,   -428,   -352,   -137,     65,
	   190
};

static const short LS_filter2_coef[V8_V21_FILTER_TAPS] = {
	     0,    166,    288,    271,     25,   -409,   -783,   -747,
	  -123,    862,   1603,   1476,    323,  -1312,  -2428,  -2199,
	  -581,   1556,   2923,   2630,    797,  -1495,  -2882,  -2581,
	  -864,   1170,   2328,   2065,    739,   -727,  -1493,  -1294,
	  -478,    336,    709,    587,    218,   -109,   -253,   -229,
	  -110
};

void
v8_V21_Init(struct v8 *v, short channel, short answerer)
{
	const short *a, *b, *c, *d;

	if (channel != 0) {
		v8_copycoeff(v->v21_taps, temp_v21_hibnd, V8_V21_TAPS);
		v->v21_params.carrier_a = 0x62b;
		v->v21_params.carrier_b = 0x580;
	} else {
		v8_copycoeff(v->v21_taps, temp_v21_lobnd, V8_V21_TAPS);
		v->v21_params.carrier_a = 0x3ef;
		v->v21_params.carrier_b = 0x344;
	}

	v->v21_params.tx_level = v8_mpyint(0x3224, v->tx_gain);

	if (answerer != 0) {
		v->v21_params.f0c = 4;
		v->v21_params.f0e = -100;
		c = HM_filter1_coef;
		d = HM_filter2_coef;
		a = HS_filter1_coef;
		b = HS_filter2_coef;
	} else {
		v->v21_params.f0c = 7;
		v->v21_params.f0e = 0;
		c = LM_filter1_coef;
		d = LM_filter2_coef;
		a = LS_filter1_coef;
		b = LS_filter2_coef;
	}

	v->v21_params.carrier_phase = 0;
	v->v21_params.samples_per_bit = 0x20;
	v->v21_params.sample_count = 0;
	v->v21_params.inbuf_pos = 0;
	v->v21_params.bits = 0;
	v->v21_params.bitcount = 0;
	v->v21_params.mark_bit = 0;
	v->v21_params.space_bit = 1;
	v->v21_params.f14 = 0x18;

	v->rx.flags |= V8_RX_V21_ARMED;

	V8_setFilters(v, a, b, c, d);
	V8_V21_reset(v);

	v->v21_params.zero_run = 0;
	v->v21_params.ones_run = 0;
	v->v21_params.ones_run_len = 0;
	v->v21_params.gap_count = 0;
	v->v21_params.gap_seen = 0;
}

/*
 * Four samples of FSK.  The two carriers differ only in which increment is
 * added to the shared phase, so the branch is one field apart; everything
 * after -- table lookup, amplitude, shaping filter -- is common.
 */
int
v8_fskmodulate(struct v8 *v, short which)
{
	struct v8_v21_params *p = &v->v21_params;
	short step = (short)(which != 0 ? p->carrier_b : p->carrier_a);
	int i;

	for (i = 0; i < V8_QUEUE_BLOCK; i++) {
		unsigned phase;
		short c;

		phase = ((unsigned)(unsigned short)p->carrier_phase
			 + (unsigned short)step) & 0x1fff;
		p->carrier_phase = (short)phase;

		c = v8_cosread((unsigned char)(phase >> 5));
		v->tx_stage[i] = v8_fsktxfilter(v, v8_mpyint(c, p->tx_level));
	}

	return v8_txwritequeue(v);
}
