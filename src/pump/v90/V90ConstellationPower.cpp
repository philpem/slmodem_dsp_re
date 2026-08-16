/*
 * V90ConstellationPower.cpp -- the V.90 downstream constellation power model:
 * the empty constructor/destructor pair, five members and the power ladder.
 *
 * `include/dsplib/V90ConstellationPower.h` carries the object map, the
 * 144-byte measurement and the argument for declaring two empty bodies.
 *
 * PLAIN CDECL with `this` as the first STACK argument, like the rest of the
 * C++ here (finding 215): `mov 0x30(%esp),%edi` in `calcModulusParameters`
 * after four pushes and a 0x1c frame, and `mov 0x60(%esp),%esi` in `getPower`,
 * which then hands that same %esi straight on as `calcModulusParameters`'
 * first stack argument.
 *
 * THE ORDER OF THE DEFINITIONS IS THE OBJECT'S, and for `getConstellationInfo`
 * it is load bearing: `getPower` contains that function INLINED -- the same
 * `cmpl $0x1` and the same twelve `lea` displacements, without the `index <= 5`
 * bound the out-of-line copy has -- so it must already have been seen when
 * `getPower` is compiled for -O3 to have the same opportunity.
 *
 * WHAT IS NOT SPELLED THE OBVIOUS WAY, and why:
 *
 *   `modulus[5]` is a TRUNCATION, not a remainder.  See finding 3052 and the
 *   comment at the tail of `calcModulusParameters`.
 *
 *   `getPower` reads `constellation`, `constellationSize` and `power` back out
 *   of memory on every iteration, and calls `alaw2linear` TWICE with the same
 *   argument rather than squaring a temporary.  Both are the object's, and
 *   both are forced: the companding routines are external calls, so nothing
 *   the compiler could see lets it cache a member across one or fold the pair.
 *
 *   the tail is `* (1.0 / 6.0)` and not `/ 6.0`.  The object's constant is the
 *   double 0.16666666666666666 at `.rodata.cst8:0xc0` reached by `fmull`; a
 *   divide would be `fdivl` against 6.0 and would not round the same way.
 */

#include <stddef.h>

/*
 * `pcm.h` is a C header with no linkage guard of its own, so it takes the
 * same wrapper every other C++ consumer of it uses (V90Phase3Modulator.cpp,
 * V90AutoDigitalImpDetector.cpp and V90ConstellationDesigner.cpp): without it
 * `alaw2linear` mangles and the reference resolves to nothing.
 */
extern "C" {
#include "dsplib/pcm.h"
}
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90ConstellationPower.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts
 * its own -- and this is the check that catches an object right in size and
 * wrong in its offsets.  Skipped on the 64-bit `check64` pass, where a 32-bit
 * layout is not what the compiler lays out; `build.sh` and `period_inner.sh`
 * both pass `-D__SIZEOF_POINTER__=4` so the period build still runs them
 * (docs/method/compilers.md, V3).
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90CP_OFF(field, off, tag) \
	typedef char v90cp_off_##tag[ \
	    ((int)__builtin_offsetof(V90ConstellationPower, field) \
	     == (off)) ? 1 : -1]

V90CP_OFF(constellation,     0x00, constellation);
V90CP_OFF(constellationSize, 0x04, constellationsize);
V90CP_OFF(codewordCount,     0x08, codewordcount);
V90CP_OFF(remaining,         0x10, remaining);
V90CP_OFF(modulus,           0x48, modulus);
V90CP_OFF(placeValue,        0x60, placevalue);
typedef char v90cp_size[(sizeof(V90ConstellationPower) == 0x90) ? 1 : -1];
#endif

/*
 * THE POWER LADDER, 35 entries, `.data` at 0xb60.  Reference bytes, and the
 * shape of them is measured rather than derived: every one of the 35 is a
 * PERFECT SQUARE, of 15124 down to 2133, and consecutive roots stand
 * 0.5006 +/- 0.005 dB apart -- so the table is an amplitude ladder in 0.5 dB
 * steps, stored squared, spanning 17 dB.  Finding 3054.  What the amplitudes
 * are referred TO is a derivation and is deferred with the rest of them
 * (docs/fastpass.md); a byte-exact copy is byte-exact and the differential
 * test proves it with no derivation at all.
 *
 * NOT `const`: the symbol is `D` in `.data`, and a `const` static member of
 * this shape would be emitted into `.rodata`.
 */
unsigned int V90ConstellationPower::averagePowerLimits[V90CP_POWER_INDICES] = {
	228735376, 203804176, 181710400, 161900176, 144288144,
	128595600, 114661264, 102171664,  91087936,  81144064,
	 72318016,  64448784,  57456400,  51208336,  45643536,
	 40704400,  36240400,  32307856,  28815424,  25684624,
	 22886656,  20394256,  18181696,  16192576,  14440000,
	 12873744,  11478544,  10214416,   9120400,   8133904,
	  7246864,   6451600,   5740816,   5112121,   4549689
};

V90ConstellationPower::V90ConstellationPower()
{
}

V90ConstellationPower::~V90ConstellationPower()
{
}

/*
 * Lay out the mixed-radix odometer for one mapping.
 *
 * The bit count is `mappingParams->shaperSR + mappingParams->word_0 - 6`,
 * `mov 0x620(%ebp),%ecx ; mov 0x0(%ebp),%eax ; add %eax,%ecx ; sub $0x6,%ecx`,
 * and the 1 it shifts is a 64-bit one: `shld %cl,%ebx,%esi ; shl %cl,%ebx ;
 * test $0x20,%cl` is the i386 long-long left shift and nothing else.  There is
 * no guard on the count -- see docs/deviations.md D349.
 *
 * Then five rounds of divide-and-remainder against the six constellation
 * sizes, and a sixth round that is NOT one.
 */
void
V90ConstellationPower::calcModulusParameters(V90MappingParams *mappingParams)
{
	unsigned int i;
	double product;

	codewordCount = 1LL << (mappingParams->shaperSR
				+ mappingParams->word_0 - 6);
	remaining[0] = codewordCount - 1;

	/*
	 * `remaining[i] % constellationSize[i]` is a SIGNED 64-bit remainder --
	 * the object calls `__moddi3` and `__divdi3`, never the `__u*` pair --
	 * and the size enters it zero extended (`xor %eax,%eax` into the high
	 * half of the argument pair), which is the `unsigned int` that field
	 * is.  The subtraction reads `modulus[i]` BACK out of the object, ahead
	 * of a `xor %ecx,%ecx`, so it is the truncated 32-bit value that is
	 * subtracted and not the 64-bit remainder.
	 */
	for (i = 0; i < V90CP_CONSTELLATIONS - 1; i++) {
		modulus[i] = (unsigned int)(remaining[i]
			     % mappingParams->constellationSize[i]);
		remaining[i + 1] = (remaining[i] - modulus[i])
				   / mappingParams->constellationSize[i];
	}

	/*
	 * AND THE SIXTH IS A TRUNCATION.  Where the five above are a `__moddi3`
	 * call, this one is `mov 0x38(%edi),%esi ; mov %esi,0x5c(%edi)` -- the
	 * low half of `remaining[5]`, copied.  The two readings agree for every
	 * `remaining[5]` below `constellationSize[5]`, which is the ordinary
	 * case and is why the test has to force the other one.  Finding 3052.
	 */
	modulus[V90CP_CONSTELLATIONS - 1] =
	    (unsigned int)remaining[V90CP_CONSTELLATIONS - 1];
	remaining[V90CP_CONSTELLATIONS] =
	    (remaining[V90CP_CONSTELLATIONS - 1]
	     - modulus[V90CP_CONSTELLATIONS - 1])
	    / mappingParams->constellationSize[V90CP_CONSTELLATIONS - 1];

	/*
	 * The place values.  The running product is a LOCAL and never read back
	 * out of `placeValue`: the object keeps it in %st across all six stores
	 * (`fstl` five times and `fstpl` once, no reload), so a version that
	 * read the previous element back would be rounding to double once per
	 * step where the object rounds only on the store.  Every value here is
	 * a product of at most six constellation sizes and so is exact either
	 * way, but the shape is the object's and costs nothing.
	 */
	product = 1.0;
	placeValue[0] = product;
	for (i = 1; i < V90CP_CONSTELLATIONS; i++) {
		product *= mappingParams->constellationSize[i - 1];
		placeValue[i] = product;
	}
}

/*
 * Select one of the twelve byte tables and its length.
 *
 * A six-way switch and not an index calculation: the object dispatches through
 * a jump table at `.rodata:0xccc` whose six entries are in index order, so the
 * original had a `switch` here.  An index above 5 falls out of the switch with
 * nothing written -- there is no default arm and no assignment before it.
 *
 * The `point` test is a single equality against 1 (`dec %edx ; je`), so every
 * value other than 1 takes `constellation`.
 */
void
V90ConstellationPower::getConstellationInfo(V90MappingParams *mappingParams,
					    V90TxPowerMeasurementPoint point,
					    unsigned int index)
{
	switch (index) {
	case 0:
		constellation = (point == V90_TX_POWER_OVER_CODEC_CONSTELLATION)
				? mappingParams->codecConstellation[0]
				: mappingParams->constellation[0];
		constellationSize = mappingParams->constellationSize[0];
		break;
	case 1:
		constellation = (point == V90_TX_POWER_OVER_CODEC_CONSTELLATION)
				? mappingParams->codecConstellation[1]
				: mappingParams->constellation[1];
		constellationSize = mappingParams->constellationSize[1];
		break;
	case 2:
		constellation = (point == V90_TX_POWER_OVER_CODEC_CONSTELLATION)
				? mappingParams->codecConstellation[2]
				: mappingParams->constellation[2];
		constellationSize = mappingParams->constellationSize[2];
		break;
	case 3:
		constellation = (point == V90_TX_POWER_OVER_CODEC_CONSTELLATION)
				? mappingParams->codecConstellation[3]
				: mappingParams->constellation[3];
		constellationSize = mappingParams->constellationSize[3];
		break;
	case 4:
		constellation = (point == V90_TX_POWER_OVER_CODEC_CONSTELLATION)
				? mappingParams->codecConstellation[4]
				: mappingParams->constellation[4];
		constellationSize = mappingParams->constellationSize[4];
		break;
	case 5:
		constellation = (point == V90_TX_POWER_OVER_CODEC_CONSTELLATION)
				? mappingParams->codecConstellation[5]
				: mappingParams->constellation[5];
		constellationSize = mappingParams->constellationSize[5];
		break;
	}
}

/*
 * The mean square companded level over a whole frame.
 *
 * For each of the six symbol positions, `fraction` is the share of the
 * `codewordCount` codewords that put constellation point `j` in that position.
 * The odometer makes that a three-way question against `modulus[i]`, and the
 * object's three arms are `ja`, `je` and fall-through in that order:
 *
 *   j below the digit boundary   the position carries `remaining[i+1] + 1`
 *                                codewords for each of the `placeValue[i]`
 *                                combinations of the positions below it
 *   j at the boundary            one minus everything the other points take
 *   j above it                   `remaining[i+1]` combinations
 *
 * The middle arm is spelled `1.0 - 1.0 / n * p * d` and not `1.0 - d * p / n`
 * because the object computes the reciprocal FIRST: `fildll` the count,
 * `fld1`, then `de f1`, which is FDIVRP and leaves `1.0 / count` -- objdump
 * prints that mnemonic as its own opposite and `tools/dis.py` says so on the
 * line (finding 245).  The outer arms divide the other way round, `fdivrl`
 * against the place value, and are spelled to match.
 */
float
V90ConstellationPower::getPower(V90MappingParams *mappingParams,
				V90TxPowerMeasurementPoint point,
				PcmType pcmType)
{
	double power;
	unsigned int i;

	power = 0.0;
	calcModulusParameters(mappingParams);

	for (i = 0; i < V90CP_CONSTELLATIONS; i++) {
		unsigned int j;

		getConstellationInfo(mappingParams, point, i);

		for (j = 0; j < constellationSize; j++) {
			double fraction;

			if (modulus[i] > j)
				fraction = placeValue[i] / (double)codewordCount
					   * (double)(remaining[i + 1] + 1);
			else if (modulus[i] == j)
				fraction = 1.0 - 1.0 / (double)codewordCount
					   * placeValue[i]
					   * (double)(remaining[i]
						      - remaining[i + 1]);
			else
				fraction = placeValue[i] / (double)codewordCount
					   * (double)remaining[i + 1];

			/*
			 * The companding law, `test %edx,%edx ; je` with the
			 * fall-through arm A-law -- the same convention
			 * `PcmType` records and the same two masks
			 * `V90Phase3Modulator` and `V90Phase3Demodulator` use.
			 */
			if (pcmType != PCM_TYPE_MU_LAW)
				power += (double)alaw2linear(
					     (unsigned char)((constellation[j]
							      & 0x7f) ^ 0xd5))
					 * (double)alaw2linear(
					     (unsigned char)((constellation[j]
							      & 0x7f) ^ 0xd5))
					 * fraction;
			else
				power += (double)ulaw2linear(
					     (unsigned char)((constellation[j]
							      & 0x7f) ^ 0xff))
					 * (double)ulaw2linear(
					     (unsigned char)((constellation[j]
							      & 0x7f) ^ 0xff))
					 * fraction;
		}
	}

	power = power * (1.0 / 6.0);
	return (float)power;
}

/*
 * The largest ladder index whose limit still reaches `power`.
 *
 * The object enters the loop on `averagePowerLimits[34] < power` alone and
 * leaves it on `limits[i] < power && i != 0`, which is the rotated form of the
 * loop below with `34 != 0` folded away.  Both conditions are computed with
 * `setb`/`setne` and ANDed rather than short circuited, which is free.
 *
 * The comparison is `unsigned int` against `float`, and the object converts
 * with `fildll` off a zero-extended pair -- so the ladder is unsigned, and the
 * conversion happens at the x87's own width exactly as the language's
 * conversion does under `-mfpmath=387`.
 */
unsigned int
V90ConstellationPower::getPowerIndexForPower(float power)
{
	unsigned int index;

	index = V90CP_POWER_INDICES - 1;
	while (index != 0 && averagePowerLimits[index] < power)
		index--;

	return index;
}

/* Eleven bytes: `xor %eax,%eax ; cmpl $0x22,0x8(%esp) ; setbe %al ; ret`. */
bool
V90ConstellationPower::isLegalPowerIndex(unsigned int index)
{
	return index <= V90CP_POWER_INDICES - 1;
}
