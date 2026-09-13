/*
 * vpcm_tables.h -- the coefficient tables of the V.PCM construction path.
 *
 * The reference distributes these definitions across the translation units
 * that use them, and each is FILE-LOCAL there.  The reconstructed consumers
 * are written, so each table is now `static` in its own file and has no
 * declaration here:
 *
 *     entFiltNum, entFiltDen       src/pump/v90/VPcmFloModemCtor.cpp
 *     IIR2100_Coef_{A,B}_{8000,9600} src/dsp/AnsamToneDetector.cpp
 *     v92TxPreFilter               src/pump/v90/V92Modulator.cpp
 *
 * Two tables still have no reconstructed consumer and remain defined in
 * src/pump/v90/vpcm_tables.c, so their declarations stay below:
 * `v92echoPreFilter_{a,b}` (V92EchoCanceller) and `v34initialbauds`
 * (VpcmFloModem).  See that file's comment for why neither can move yet.
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

/*
 * v92echoPreFilter_a/b are FILE-LOCAL in the object (`d`), but a `static`
 * copy in vpcm_tables.c is dropped by -O3: nothing in this tree references
 * them yet (V92EchoCanceller's constructor, their only consumer, is not
 * reconstructed).  The record stays global so the differential test can
 * reach it; their reference home is V92EchoCanceller.cpp.
 */
extern float v92echoPreFilter_a[V92_ECHO_PREFILTER_TAPS];
extern float v92echoPreFilter_b[V92_ECHO_PREFILTER_TAPS];

/*
 * `v34initialbauds` has moved: its reference consumer is VpcmFloModem's
 * constructor, which now reads it rather than the duplicate
 * `vpcm_ctor_flags_0217`, so it is `static` in
 * `src/pump/v90/VPcmFloModemCtor.cpp`.  A test names it through the test
 * tier's globalized copies (tools/testvisible.py).
 */

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_VPCM_TABLES_H */
