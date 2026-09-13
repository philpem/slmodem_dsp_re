/*
 * vpcm_tables.c -- the coefficient tables of the V.PCM construction path that
 * have no reconstructed consumer of their own yet.
 *
 * WHERE EACH TABLE REALLY LIVES, AND HOW THAT IS KNOWN.  Every table below is
 * a LOCAL symbol in the object, so each was `static` and can only be named
 * from inside the file that defines it.  That makes attribution exact, with no
 * bracket and no contiguity fill: a file-local table is defined in the
 * translation unit of the function that references it, and each has exactly
 * one referencing function.  `tools/relocscan.py --at .data:0xNN` gives the
 * reference and it is always a constructor:
 *
 *     v92echoPreFilter_{a,b}                   V92EchoCanceller's constructor
 *     v34initialbauds                          VPcmFloModem's constructor
 *
 * SEVEN OF THE ORIGINAL TEN HAVE MOVED TO THEIR CONSUMER.  The reference
 * distributes these definitions across the translation units that use them,
 * and the reconstructed consumers are written now, so each table is `static`
 * in its own file:
 *
 *     entFiltNum, entFiltDen       -> src/pump/v90/VPcmFloModemCtor.cpp
 *     IIR2100_Coef_{A,B}_{8000,9600} -> src/dsp/AnsamToneDetector.cpp
 *     v92TxPreFilter               -> src/pump/v90/V92Modulator.cpp
 *
 * The reference's FILE records own them the same way: IIR2100_* under
 * `AnsamToneDetector.cpp`, entFilt* under `VpcmFloModem.cpp`, and
 * v92TxPreFilter under `V92Modulator.cpp`.
 *
 * THE ONE THAT REMAINS CANNOT MOVE YET.
 *
 *     `v92echoPreFilter_{a,b}` belong to V92EchoCanceller.cpp, but that
 *     constructor is not reconstructed, so a `static` copy here has no
 *     referrer and -O3 DROPS it.  The record stays global so the differential
 *     test can reach it.
 *
 * `v34initialbauds` has moved: its consumer is the V.PCM constructor, which
 * now reads it instead of the duplicate `vpcm_ctor_flags_0217`, so it is
 * `static` in `VPcmFloModemCtor.cpp`.
 *
 * WHEN THE REMAINING CONSUMER LANDS, MOVE THAT TABLE IN AS `static` too.
 * A new `static` beside this one links perfectly, the two copies diverge
 * silently, and the test here keeps passing against the copy nobody uses.
 *
 * THEY ARE NOT `const`, AND THAT IS DELIBERATE.  The nine coefficient tables
 * are in the blob's `.data` (section 143), not its `.rodata` (129), so the
 * original declared them writable; `v34initialbauds` is the one in `.rodata`
 * and is the one declared `const` here.  Section placement is forced by the
 * declaration rather than chosen by the compiler, so it is evidence and is
 * reproduced.  Nothing writes to any of them.
 *
 * AND THAT CLAIM HAS BEEN LOOKED AT RATHER THAN ASSERTED, because no test can
 * see it -- `t_vpcmtabs.c` compares bytes and a byte in `.rodata` is the same
 * byte.  `readelf -sW` on the object puts the coefficient tables in `.data`
 * (`WA`) and `v34initialbauds` alone in `.rodata` (`A`), which is the blob's
 * 143/129 split reproduced.  If somebody later adds `const` to one of the
 * coefficient tables for tidiness, nothing in `make phase` will notice; a
 * `readelf` on the object is the check.
 *
 * THE VALUES ARE REFERENCE BYTES, NOT A GENERATOR.  Deriving a filter design
 * back out of its coefficients is `docs/fastpass.md`'s deferred work; until
 * the retarget wants them at another rate, byte-exact is byte-exact and
 * `test/unit/t_vpcmtabs.c` proves it against the blob's own copy.
 */

#include "dsplib/vpcm_tables.h"

/*
 * ---------------------------------------------------------------------------
 * V92EchoCanceller's constructor: the echo pre-filter, an ARMA pair.
 * ---------------------------------------------------------------------------
 */

/*
 * `v92echoPreFilter_a`, .data+0x360 -- the AR (recursive) half, third argument
 * of `FloatARMA(n, 12, a, b, 99)` at .text+0x1119b.  Twelve floats indexed by
 * tap, a[0] exactly 1.0.
 *
 * LOCAL (`d`) in the object, but a `static` copy here has no referrer yet
 * (V92EchoCanceller's constructor is unwritten) and -O3 DROPS it, so the
 * record stays global.  When that constructor lands, move it back in as
 * `static`.
 */
float v92echoPreFilter_a[V92_ECHO_PREFILTER_TAPS] = {
	1.0f, 0.708999991f, -2.30019999f, -2.67490005f, 1.53729999f,
	3.62899995f, 0.467999995f, -2.13840008f, -1.00730002f, 0.423099995f,
	0.344900012f, 0.0285f,
};

/*
 * `v92echoPreFilter_b`, .data+0x320 -- the MA (feed-forward) half, fourth
 * argument of the same call.  Twelve floats, and b[0] is 1.4986 rather than 1,
 * so this pair is not normalised the way the two IIR pairs above are.
 *
 * LOCAL (`d`) in the object; global here for the same reason as the `_a`
 * half above -- a `static` copy is dropped by -O3 with no referrer.
 */
float v92echoPreFilter_b[V92_ECHO_PREFILTER_TAPS] = {
	1.49860001f, 0.621999979f, -2.73329997f, -2.60159993f, 1.71739995f,
	3.37879992f, 0.135000005f, -1.75409997f, -0.682799995f, 0.350800008f,
	0.221100003f, 0.00579999993f,
};

/*
 * `v34initialbauds`, .rodata+0x3e0 -- six bytes, all 1, copied wholesale into
 * the modem object at +0x217 by VPcmFloModem's constructor (.text+0xfbf5).
 *
 * MOVED.  Its reference consumer is that constructor, which now reads this
 * array rather than a duplicate (`vpcm_ctor_flags_0217`), and it is `static`
 * in `VPcmFloModemCtor.cpp` with the rest of the constructor.  The derivation
 * that was here is in that file now.
 */