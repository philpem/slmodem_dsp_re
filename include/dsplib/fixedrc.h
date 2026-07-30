/*
 * fixedrc.h -- Fixed Rate Converter: fixed rational-factor resampling.
 *
 * The datapumps run at their own native rates (8 kHz for B103/V.21, V.22,
 * V.23 and V.32) while the host runs at 9600.  This module is the bridge, and
 * it is why the pumps never had to care about the host rate.
 *
 * A conversion is identified by a small integer `mode`, which indexes a table
 * of supported up/down factor pairs.  Callers obtain one from
 * RcFixed_Check_Combination() and hand it to RcFixed_Create().
 */

#ifndef DSPLIB_FIXEDRC_H
#define DSPLIB_FIXEDRC_H

/*
 * Number of entries in the factor tables, excluding the {0,0} terminator.
 * RcFixed_Check_Combination() returns RCFIXED_NMODES for an unsupported
 * combination, and RcFixed_Create() maps that to an identity converter.
 */
#define RCFIXED_NMODES 20

/*
 * Look up the conversion mode for a given rate pair.
 *
 * Reduces in_rate:out_rate by their GCD and searches the factor tables for a
 * matching down:up pair.  Returns an index in [2, RCFIXED_NMODES), or
 * RCFIXED_NMODES if the ratio is not supported.
 *
 * The search deliberately starts at index 2: entries 0 and 1 are the plain
 * x4 and /4 cases, reachable only by asking for them explicitly.
 *
 * Note that equal rates reduce to 1:1, which is *not* in the tables, so
 * 8000 -> 8000 returns RCFIXED_NMODES and yields an identity converter.  That
 * is the mechanism by which moving the host to 8 kHz turns this whole module
 * into a pass-through.
 */
int RcFixed_Check_Combination(int in_rate, int out_rate);

/* Up and down factors per mode; exposed for tests and for rate planning. */
int RcFixed_UpFactor(int mode);
int RcFixed_DownFactor(int mode);

/*
 * History depth.  The original allocates a flat 200-sample int16 window and
 * compacts it when the write position reaches the end, rather than using a
 * circular buffer -- see rc_resample() in src/core/fixedrc.c.
 */
#define RCFIXED_HISTORY 200

/* One polyphase coefficient bank: `up` branches of `taps` int16 Q14. */
struct rc_bank {
	const short *coeff;
	int taps;
};

extern const struct rc_bank rc_banks[RCFIXED_NMODES];

/*
 * Converter state.  Field order follows the original's 420-byte layout so the
 * two can be compared field by field during differential testing; see the
 * offsets in the comments.
 */
struct rc_state {
	const short *coeff;                 /* +0x000 selected bank         */
	short history[RCFIXED_HISTORY];     /* +0x004 sliding input window  */
	unsigned short phase;               /* +0x194 accumulator, 0..up-1  */
	unsigned short down;                /* +0x196 input rate factor     */
	unsigned short up;                  /* +0x198 output rate factor    */
	short taps;                         /* +0x19a per-branch length     */
	int pos;                            /* +0x19c history write index   */
	int input_needed;                   /* +0x1a0 samples before next out */
};

/* Opaque handle. */
struct rc;

/*
 * Create a converter for a mode from RcFixed_Check_Combination().
 *
 * Returns NULL for modes 0 and 1 (which use a different state layout in the
 * original and are not implemented here) and for modes at or above
 * RCFIXED_NMODES, i.e. unsupported ratios.
 */
struct rc *RcFixed_Create(int mode);
void RcFixed_Delete(struct rc *h);
void RcFixed_Reset(struct rc *h);

/*
 * Convert `in_count` samples.  Writes at most as many outputs as the ratio
 * allows and stores the count through `out_count`.  Consumes all of `in`
 * unless it runs out mid-way through the samples needed for one more output,
 * in which case the remainder is held in the history for the next call.
 */
void RcFixed_Resample(struct rc *h, const short *in, int in_count,
		      short *out, int *out_count);

/* Test accessor: the live state, for field-by-field comparison. */
struct rc_state *RcFixed_State(struct rc *h);

#endif /* DSPLIB_FIXEDRC_H */
