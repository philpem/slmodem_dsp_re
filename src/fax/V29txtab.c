/*
 * V29txtab.c -- ITU-T V.29 (fax): the transmitter's pulse-shaping, scrambler
 *               and constellation tables.
 *
 * The blob names `V29txtab.c` between `V29tx.c` and `Vmi_v17.c`, and every
 * one of these tables is reached only from the transmit side (`V29tx.c`,
 * `V29t_prc.c`, `V29t_stc.c`, `V29t_int.c`).  The bodies are the text that
 * was in `src/fax/v29.c`, moved verbatim, in the object's address order.
 */

#include "dsplib/v29data.h"
#include "dsplib/v29fax.h"
#include "dsplib/smc.h"

/*
 * The transmit configuration and the tables `V29TX_create` builds its DSP
 * sub-objects from.  `protocol`/`bitrate`/`flags` are typed by
 * `V29TX_status`'s own reads (`V29TXS_PROTOCOL`/`_BITRATE`/`_FLAGS_10`,
 * v29fax.h); `int_0008` is 60000, `v21tx_cfg`'s own value at the identical
 * offset.  The rest are usage inference; see v29data.h.
 */
struct v29tx_cfg V29TX_CFG = {
	0,			/* protocol                                  */
	9600,			/* bitrate                                   */
	0,			/* short_0004                                */
	0,			/* short_0006                                */
	60000,			/* int_0008                                  */
	1,			/* int_000c                                  */
	0,			/* flags                                     */
	0,			/* short_0012                                */
	1,			/* fifo_size_factor -- V29TX_create's own
					transmit FIFO size is this * 48,
					`v17tx_cfg`'s own field, same name    */
	0,			/* int_0018 -- V29TX_create's own FPM_PPS_CFG
					aux, across the (void *)(long) idiom */
};

/* Indexed by V29TXP_RATE.  Fed to FPM_PPS_init's `scale` at 0x9bd25. */
int V29TX_PPS_SCALE[2] = { 45016, 25480 };

/* Indexed by V29TXP_RATE, read inside TxNextStateV29 itself at 0xa4937. */
short V29TX_PATTERN_SCR1[2] = { 7, 15 };

/* The transmit pulse shaper's I and Q coefficient tables, 120 shorts each,
 * fed to FPM_PPS_init at 0x9bd57. */
const short V29TX_PPS_IFILT[120] = {
	   -22,   -192,   -455,   -670,   -707,   -522,   -190,    125,
	   258,    137,   -162,   -453,   -529,   -287,    207,    738,
	  1051,    997,    622,    164,    -75,    110,    690,   1406,
	  1887,   1859,   1304,    499,   -123,   -201,    333,   1191,
	  1856,   1858,   1046,   -291,  -1545,  -2104,  -1699,   -598,
	   487,    759,   -222,  -2234,  -4436,  -5760,  -5483,  -3691,
	 -1359,     51,   -675,  -3781,  -8197, -11812, -12306,  -8208,
	   298,  11207,  21310,  27371,  27371,  21310,  11207,    298,
	 -8208, -12306, -11812,  -8197,  -3781,   -675,     51,  -1359,
	 -3691,  -5483,  -5760,  -4436,  -2234,   -222,    759,    487,
	  -598,  -1699,  -2104,  -1545,   -291,   1046,   1858,   1856,
	  1191,    333,   -201,   -123,    499,   1304,   1859,   1887,
	  1406,    690,    110,    -75,    164,    622,    997,   1051,
	   738,    207,   -287,   -529,   -453,   -162,    137,    258,
	   125,   -190,   -522,   -707,   -670,   -455,   -192,    -22,
};

const short V29TX_PPS_QFILT[120] = {
	    99,    244,    224,      9,   -326,   -628,   -747,   -628,
	  -345,    -72,      6,   -194,   -603,  -1017,  -1206,  -1044,
	  -589,    -65,    248,    177,   -240,   -756,  -1033,   -836,
	  -173,    686,   1339,   1469,   1036,    318,   -210,   -141,
	   630,   1810,   2836,   3167,   2597,   1406,    246,   -186,
	   451,   1901,   3389,   3989,   3135,    990,  -1546,  -3237,
	 -3164,  -1305,   1283,   2823,   1630,  -3000, -10235, -17804,
	-22775, -22725, -16800,  -6193,   6193,  16800,  22725,  22775,
	 17804,  10235,   3000,  -1630,  -2823,  -1283,   1305,   3164,
	  3237,   1546,   -990,  -3135,  -3989,  -3389,  -1901,   -451,
	   186,   -246,  -1406,  -2597,  -3167,  -2836,  -1810,   -630,
	   141,    210,   -318,  -1036,  -1469,  -1339,   -686,    173,
	   836,   1033,    756,    240,   -177,   -248,     65,    589,
	  1044,   1206,   1017,    603,    194,     -6,     72,    345,
	   628,    747,    628,    326,     -9,   -224,   -244,    -99,
};

/*
 * The symbol coder's carrier phasor (24 steps, 1700 Hz at 2400 baud with
 * `rot_step` = 0x11) and its constellation maps, fed to SMC_init at
 * 0x9bcba.  smc.h names cosine/sine's slots from this function's own
 * relocations.
 */
const short V29TX_SMC_COSINE[24] = {
	32767,  31651,  28378,  23170,  16384,   8481,      0,  -8481,
       -16384, -23170, -28378, -31651, -32767, -31651, -28378, -23170,
       -16384,  -8481,      0,   8481,  16384,  23170,  28378,  31651,
};

const short V29TX_SMC_SINE[24] = {
	    0,   8481,  16384,  23170,  28378,  31651,  32767,  31651,
	28378,  23170,  16384,   8481,      0,  -8481, -16384, -23170,
       -28378, -31651, -32767, -31651, -28378, -23170, -16384,  -8481,
};

const short V29TX_SMC_IMAP[16] = {
	 6144,  2048,     0, -2048, -6144, -2048,     0,  2048,
	10240,  6144,     0, -6144,-10240, -6144,     0,  6144,
};

const short V29TX_SMC_QMAP[16] = {
	    0,  2048,  6144,  2048,     0, -2048, -6144, -2048,
	    0,  6144, 10240,  6144,     0, -6144,-10240, -6144,
};

/* dibit -> quadrant increment; struct fpm_smc_cfg::pmap's own type. */
const unsigned short V29TX_SMC_PMAP[8] = { 1, 0, 2, 3, 6, 7, 5, 4 };
