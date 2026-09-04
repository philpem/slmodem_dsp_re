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

/**
 * @brief Look up the conversion mode for a given rate pair.
 *
 * Reduces @p in_rate : @p out_rate by their GCD and searches the factor
 * tables for a matching down:up pair. The search deliberately starts at
 * index 2: entries 0 and 1 are the plain x4 and /4 cases, reachable only
 * by asking for them explicitly. Equal rates reduce to 1:1, which is
 * *not* in the tables, so 8000 -> 8000 returns #RCFIXED_NMODES and yields
 * an identity converter -- the mechanism by which moving the host to
 * 8 kHz turns this whole module into a pass-through.
 *
 * @param in_rate   Input sample rate.
 * @param out_rate  Output sample rate.
 * @return An index in `[2, RCFIXED_NMODES)`, or #RCFIXED_NMODES if the
 *         ratio is not supported.
 */
int RcFixed_Check_Combination(int in_rate, int out_rate);

/**
 * @brief The upsampling factor for a mode, for tests and rate planning.
 * @param mode  A mode from RcFixed_Check_Combination().
 * @return The up factor.
 */
int RcFixed_UpFactor(int mode);

/**
 * @brief The downsampling factor for a mode, for tests and rate planning.
 * @param mode  A mode from RcFixed_Check_Combination().
 * @return The down factor.
 */
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
 * Converter state, 420 bytes -- the size the original hands to malloc,
 * `movl $0x1a4,(%esp)` at RcFixed_Create+0x4a.
 *
 * Every 16-bit field here is SIGNED, and the object says so at each of them:
 *
 *   +0x194 phase  RcFixed_Resample b13ee `movswl 0x194(%esi),%eax` feeding
 *                 `imul %ecx,%eax` -- a live 32-bit use, so the extension is
 *                 forced.  `RcFixed_Reset` b0e82 also tests the freshly
 *                 computed value with a SIXTEEN-bit `test %dx,%dx; js` and
 *                 corrects it, a branch the compiler deletes outright if the
 *                 field cannot be negative.
 *   +0x196 down   b0e5a / b1033 `movswl`, then `cltd; idiv` -- a signed divide.
 *   +0x198 up     loaded `movzwl` (the first use is a 16-bit compare, where
 *                 the extension is free) and then RE-EXTENDED `movswl %cx,%esi`
 *                 at b0e71 for that same divide.  619's rule: the zero-
 *                 extending load was the expression, the re-extension is the
 *                 declaration.
 *   +0x19a taps   b0e51 / b13e7 `movswl`, used at 32 bits by `imul` and `sub`.
 *
 * `movzwl 0x194(%ebx),%ecx` at b1448 is not a contradiction: every use there is
 * 16 bits wide (a 16-bit store and `cmp %ax,%cx`), which finding F614 puts in
 * the compiler's free column.  It is also why `extcheck` cannot see +0x194 --
 * 618's "loaded both ways proves nothing" filter suppresses the operand.
 *
 * Findings F2403 and F2700.  Field order follows the original's 420-byte layout
 * so the two can be compared field by field during differential testing.
 */
struct rc_state {
	const short *coeff;                 /* +0x000 selected bank         */
	short history[RCFIXED_HISTORY];     /* +0x004 sliding input window  */
	short phase;                        /* +0x194 accumulator, 0..up-1  */
	short down;                         /* +0x196 input rate factor     */
	short up;                           /* +0x198 output rate factor    */
	short taps;                         /* +0x19a per-branch length     */
	int pos;                            /* +0x19c history write index   */
	int input_needed;                   /* +0x1a0 samples before next out */
};

/* Opaque handle. */
struct rc;

/**
 * @brief Create a converter for a mode from RcFixed_Check_Combination().
 * @param mode  A mode index.
 * @return A new converter, or NULL for modes 0 and 1 (which use a
 *         different state layout in the original and are not implemented
 *         here) and for modes at or above #RCFIXED_NMODES, i.e.
 *         unsupported ratios.
 */
struct rc *RcFixed_Create(int mode);

/**
 * @brief Free a converter built by RcFixed_Create().
 * @param h  The converter to free.
 */
void RcFixed_Delete(struct rc *h);

/** @brief Reset a converter's history and phase to their initial state. */
void RcFixed_Reset(struct rc *h);

/**
 * @brief Convert a run of samples.
 *
 * Consumes all of @p in unless it runs out mid-way through the samples
 * needed for one more output, in which case the remainder is held in the
 * history for the next call.
 *
 * @param h          The converter.
 * @param in         Input samples.
 * @param in_count   Number of input samples.
 * @param out        Output buffer.
 * @param out_count  Set to the number of output samples written -- at
 *                    most as many as the ratio allows.
 */
void RcFixed_Resample(struct rc *h, const short *in, int in_count,
		      short *out, int *out_count);

/**
 * @brief Test accessor: the converter's live state, for field-by-field
 * comparison.
 * @param h  The converter.
 * @return Pointer to its internal `struct rc_state`.
 */
struct rc_state *RcFixed_State(struct rc *h);

#endif /* DSPLIB_FIXEDRC_H */
