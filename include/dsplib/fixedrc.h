/*
 * fixedrc.h -- fixed rational-factor sample rate conversion.
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

#endif /* DSPLIB_FIXEDRC_H */
