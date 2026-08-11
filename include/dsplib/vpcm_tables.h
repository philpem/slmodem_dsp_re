/*
 * vpcm_tables.h -- the coefficient tables of the V.PCM construction path.
 *
 * Ten tables, 710 bytes, and every one of them is a FILE-LOCAL symbol in the
 * object.  src/pump/v90/vpcm_tables.c says where each one really lives and why
 * they are gathered here anyway.
 *
 * The element types are not read off the bytes.  Every table is handed to a
 * constructor whose mangled name states the pointer type, so `double` and
 * `float` below are the object's own declaration and not an inference from
 * the stride:
 *
 *     GenericIIR<float,double>(j, j, double *, double *, j)   entFilt*
 *     GenericToneDetector(j, j, double *, double *, ...)      IIR2100_*
 *     FloatARMA(j, j, float *, float *, j)                    v92echoPreFilter_*
 *     FloatFIR(j, float *, j)                                 v92TxPreFilter
 *
 * and in all three of the pair-taking cases the DENOMINATOR is the third
 * argument and the numerator the fourth, which is what pairs `entFiltDen` with
 * `IIR2100_Coef_A_*` and `v92echoPreFilter_a` rather than with the `_b`.
 */

#ifndef DSPLIB_VPCM_TABLES_H
#define DSPLIB_VPCM_TABLES_H

/*
 * Five taps each, and the same five-and-five is passed twice to the
 * constructor -- `mov $0x5,%edi` / `mov $0x5,%ecx` at .text+0xfb8a and
 * +0xfba0.  A fourth-order section, then, not two of different orders.
 */
#define VPCM_ENTFILT_TAPS		5

/*
 * The 2100 Hz detector's two rate variants.  The count is not a constant in
 * the object: `lea 0xb(%eax,%eax,1)` at .text+0x1092a with %eax the result of
 * `sete` on `cmp $0x1f40` gives 13 at 8000 Hz and 11 otherwise, and the same
 * count is passed for both the A and the B array.
 */
#define IIR2100_TAPS_8000		13
#define IIR2100_TAPS_9600		11

/* Twelve each; `mov $0xc,%ebp` at .text+0x11151 is the count argument. */
#define V92_ECHO_PREFILTER_TAPS		12

/* `mov $0x24,%eax` at .text+0x15330 is the tap count argument to FloatFIR. */
#define V92_TXPREFILTER_TAPS		36

/* Six, which is how many symbol rates V.34 has. */
#define V34_INITIAL_BAUDS		6

#ifdef __cplusplus
extern "C" {
#endif

extern double entFiltNum[VPCM_ENTFILT_TAPS];
extern double entFiltDen[VPCM_ENTFILT_TAPS];

extern double IIR2100_Coef_A_8000[IIR2100_TAPS_8000];
extern double IIR2100_Coef_B_8000[IIR2100_TAPS_8000];
extern double IIR2100_Coef_A_9600[IIR2100_TAPS_9600];
extern double IIR2100_Coef_B_9600[IIR2100_TAPS_9600];

extern float v92echoPreFilter_a[V92_ECHO_PREFILTER_TAPS];
extern float v92echoPreFilter_b[V92_ECHO_PREFILTER_TAPS];

extern float v92TxPreFilter[V92_TXPREFILTER_TAPS];

extern const unsigned char v34initialbauds[V34_INITIAL_BAUDS];

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_VPCM_TABLES_H */
