/*
 * V90TRN2Designer.cpp -- the constructor and destructor.
 *
 * `include/dsplib/V90TRN2Designer.h` carries the object map, the evidence for
 * the two pointer types and the scope of the size bound.
 *
 * PLAIN CDECL with `this` as the first STACK argument -- `mov 0x4(%esp),%eax`
 * with no frame at all -- so nothing here needs a calling-convention
 * attribute (finding F215).
 */

#include <stddef.h>

#include "dsplib/V90TRN2Designer.h"

/*
 * THE 0x558 `V90Parameters`, NEVER `V90PreFilter.h`'s 0x504 one.  The three
 * members below reach +0x074, +0x078 and +0x080, all of which are inside
 * both definitions, but the choice is not a matter of which fields are
 * reached: mixing the two under-allocates by 84 bytes and passes every test
 * not run under a checking allocator.  `V90ModemCtor.cpp` carries the long
 * version of this comment; `tools/onedef.py` gates it.
 */
#include "dsplib/V90ConstellationPower.h"
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90Parameters.h"
#include "dsplib/V90Phase3Modulator.h"
#include "dsplib/V90SpectralConditions.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"

/*
 * `pcm.h` is a C header with no linkage guard of its own, so it takes the
 * wrapper here -- exactly as `V90ConstellationDesigner.cpp` does.  Without it
 * the two companding calls mangle and match nothing the object defines.
 */
extern "C" {
#include "dsplib/pcm.h"
}

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts
 * its own.
 *
 * GUARDED ON THE POINTER WIDTH, because both members are pointers and
 * `make check64` compiles this file for the native target, where they are
 * eight bytes and +0x04 moves.  The claim is about the 32-bit layout the blob
 * has, so it is asserted only where the compiler lays that layout out.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define TRN2_OFF(field, off, tag) \
	typedef char trn2_off_##tag[ \
	    ((int)__builtin_offsetof(V90TRN2Designer, field) == (off)) ? 1 : -1]

TRN2_OFF(params, 0x00, params);
TRN2_OFF(power,  0x04, power);
typedef char trn2_size[(sizeof(V90TRN2Designer) == 0x08) ? 1 : -1];
#endif

/*
 * Two stores, in the object's order.  The argument names are the mangling's
 * types spelled out; nothing here reads either pointer.
 */
V90TRN2Designer::V90TRN2Designer(V90Parameters *p, V90ConstellationPower *cp)
{
	params = p;
	power = cp;
}

/*
 * One byte, `ret`.  See the header: the symbol exists only because the
 * original declared the destructor, so declaring and emptying it here is the
 * reconstruction and not a placeholder.
 */
V90TRN2Designer::~V90TRN2Designer()
{
}

/*
 * Adopt the configured TRN2 length as the working one.
 *
 * Twenty-four bytes, and every one of them is a decision:
 *
 *     cmpw $0x0,0x8(%esp)      the argument is compared AS A SHORT, in
 *                              memory, before `this` is even loaded
 *     je   +0x17               a zero argument does nothing at all
 *     mov  0x4(%esp),%ecx      this
 *     mov  (%ecx),%edx         this->params
 *     mov  0x80(%edx),%eax
 *     mov  %eax,0x78(%edx)
 *
 * So the argument is a FLAG and not a length: nothing here stores it, and the
 * value that lands in `nofUcodesInTrn2` comes from +0x080.  That asymmetry is
 * what finding F3527 used to call +0x080 the configured value and +0x078 the
 * working one, and it is why this member is named for what it selects rather
 * than for what it is passed.
 */
void
V90TRN2Designer::setNofUcodesInTrn2(short on)
{
	if (on != 0)
		params->nofUcodesInTrn2 = params->unnamed_080;
}

/*
 * Fill all six constellations with a descending run from 78, `nofUcodesInTrn2`
 * entries long.
 *
 * BOTH COUNTERS ARE `short` AND THAT IS FORCED, not a reading of the loop
 * bounds: the inner one is `inc %eax ; cwtl` and the outer is
 * `lea 0x1(%edi),%eax ; movswl %ax,%edi ; cmp $0x5,%di`.  An `int` needs
 * neither sign-extension, and the upper half is USED -- it is the index and
 * the compare operand -- so this is the forced case of the codegen rule and
 * not 614's discarded half.
 *
 * THE BOUND IS RE-READ EVERY ITERATION.  `mov 0x0(%ebp),%ecx` appears at
 * 0x3cb0e before the loop and AGAIN at 0x3cb3a inside it, because the byte
 * stores may alias `this->params`; hoisting the count into a local deletes
 * that reload.  The condition is therefore written through the member.
 *
 * THERE IS NO BOUND AGAINST 128.  The rows are 128 bytes each and the loop
 * runs to `params->nofUcodesInTrn2`, which nothing here checks, so a count
 * above 128 walks into the next row.  The object is written that way; the
 * test drives 128 exactly and no further.
 *
 * The compare is `cmp 0x78(%ecx),%eax ; jge` -- SIGNED, against the `int`
 * field, with the `short` counter promoted.
 */
void
V90TRN2Designer::setTrn2DummyConstel(V90MappingParams *mappingParams)
{
	short k;

	for (k = 0; k <= 5; k++) {
		unsigned char value = 78;
		short i;

		for (i = 0; i < params->nofUcodesInTrn2; i++) {
			mappingParams->constellation[k][i] = value;
			mappingParams->codecConstellation[k][i] = value;
			value--;
		}
	}
}

/*
 * log10() on the coprocessor, as the object computes it: `fldlg2` pushes
 * log10(2) at the register's full 64-bit mantissa and `fyl2x` computes
 * st(1) * log2(st(0)) and pops, so the pair takes one value and leaves one.
 *
 * GCC DOES NOT EMIT THAT SEQUENCE FOR `log10()` AT THIS TREE'S FLAGS, and the
 * flag that would is not the one finding F876 names.  Measured on the period
 * compiler in `tools/toolchain/`, at `build.sh`'s exact flag list plus one:
 *
 *     (nothing)                                       call log10
 *     -funsafe-math-optimizations                     call log10
 *     -funsafe-math-optimizations -fno-math-errno     call log10
 *     -funsafe-math-optimizations -fno-trapping-math  call log10
 *     -ffast-math                                     fldlg2 / fxch / fyl2x
 *
 * so `-funsafe-math-optimizations` is necessary and NOT sufficient for a
 * `double` argument, and nothing narrower than `-ffast-math` reproduces it.
 * That flag is not in this tree's derived set and must not be: it withdraws
 * NaN semantics from the whole translation unit, which CLAUDE.md records
 * breaking eleven other sites.  A call to libm's `log10` is not the same
 * function either -- it is correctly rounded where `fyl2x` is not -- so the
 * sequence is written out.
 *
 * THE COPY IS DELIBERATE AND IT IS THE FOURTH.  `Psd.cpp`, `V90Equalizer.cpp`
 * and `VPcmFloModem.cpp` each carry the same eight lines, and Psd.cpp says why
 * a shared header is a separate concern: a new C++ header has to be added to
 * `offcheck.py`'s SKIP_HEADERS or the `offsets` gate breaks files nobody
 * touched.  Finding F876.
 */
static inline long double
trn2_x87_log10(long double x)
{
	long double r;

	__asm__ ("fldlg2\n\tfxch %%st(1)\n\tfyl2x" : "=t" (r) : "0" (x));
	return r;
}

/*
 * How many bits one frame of the six constellations carries: log2 of the
 * product of their lengths, truncated.
 *
 * `this` IS NOT TOUCHED.  The whole body works through the argument, which is
 * why the class header can bound the object at eight bytes across five of its
 * six members.
 *
 * THE PRODUCT IS A `float`, and that is measured rather than chosen: the zero
 * test is `fcoms 0x2ac` against a FLOAT zero, and a `double` product compiles
 * to `fcoml` against `.rodata.cst8`.  Nothing rounds it to float in between,
 * because -mfpmath=387 keeps it in the register at extended precision until
 * something stores it -- so the six `fildll`/`fmulp` are what a `float`
 * declaration produces here, and the declared type shows up only in the
 * compare.
 *
 * THE CONVERSION IS 64-BIT: `fistpll` with the low word taken, not `fistpl`.
 * That is forced, and the reading is `unsigned int` -- the i386 back end has
 * no direct float-to-unsigned and converts through DImode, which is exactly
 * `fistpll` plus the low word.  `V90ConstellationDesigner::maxK` is the same
 * function in the sibling class and this tree already spells it
 * `(int)(unsigned int)`; the two agree instruction for instruction and the
 * consistency is worth more than a second opinion.  No caller types the
 * result: `maxK` has no relocation anywhere in the object and is reached only
 * by being inlined into `V90TRN2Design`.
 *
 * THE EPSILON IS THE POINT OF THE FUNCTION, not noise: with truncation
 * towards zero (`or $0xc00`), an exact power of two whose logarithm lands a
 * few ULP low would otherwise come back one bit short.  1e-6f is the object's
 * constant at .rodata.cst4+0x2b4.
 */
int
V90TRN2Designer::maxK(V90MappingParams *mappingParams)
{
	float product = (float)mappingParams->constellationSize[0] *
			mappingParams->constellationSize[1] *
			mappingParams->constellationSize[2] *
			mappingParams->constellationSize[3] *
			mappingParams->constellationSize[4] *
			mappingParams->constellationSize[5];
	int k = 0;

	if (product != 0.0f) {
		long double logProduct = trn2_x87_log10((long double)product);
		float logTwo = (float)trn2_x87_log10((long double)2.0f);

		k = (int)(unsigned int)(logProduct / logTwo + 1e-6f);
	}
	return k;
}

/*
 * FSQRT on the value already in st(0), for the "sqrt(power)" diagnostic.
 *
 * The object's is a bare `fsqrt` with no branch and no call.  `sqrt()` at
 * this tree's flags is a libm call with an inline `fsqrt`/`fcom`/`jp` fast
 * path in front of it -- measured on the period compiler -- so it is neither
 * the same instructions nor the same value on a negative input, where
 * `fsqrt` returns the indefinite NaN and libm sets errno.
 *
 * SAME ASM, SAME REASON, as `x87_fsqrt` in V90ConstellationDesigner.cpp,
 * `v92mapper_fsqrt` in V92Mapper.cpp and `agc_fsqrt` in dsplib/Agc.h.  Fourth
 * copy, deliberate, for the reason `trn2_x87_log10`'s fourth copy is.
 */
static inline long double
trn2_x87_fsqrt(long double x)
{
	long double r;

	__asm__ ("fsqrt" : "=t" (r) : "0" (x));
	return r;
}

/*
 * The companding, and it is a macro for the reason
 * `V90ConstellationDesigner.cpp`'s `FORCERATE_ENCODE` is one: the object has
 * SIX pairs of `linear2alaw`/`linear2ulaw` call sites in this function alone,
 * and a `static` helper would not inline at these flags (finding F2163).
 *
 * `linear2alaw`'s result is XORed with 0xd5 and `linear2ulaw`'s is
 * complemented, which is the pair this object always uses and which
 * `V90ConstellationDesigner.h` derives.  A-law is the NON-ZERO `PcmType`.
 */
#define TRN2_COMPAND(v)							\
	((unsigned char)(pcmType != PCM_TYPE_MU_LAW			\
			 ? (linear2alaw(__builtin_abs((int)(v))) ^ 0xd5)	\
			 : (unsigned char)~linear2ulaw(			\
				   __builtin_abs((int)(v)))))

/*
 * V90TRN2Design -- lay out the six TRN2 constellations.
 *
 * 3,767 bytes at 0x3cb60, the largest single function in this class and the
 * only one that writes `mappingParams`.  Returns 1 when all six were designed
 * and 0 when one of them could not be; the caller's test is `test %ax,%ax`,
 * which is what makes the return SIXTEEN BITS and not `int`.
 *
 * THE ARGUMENT NAMES ARE THE SIBLING'S, not invented here.
 * `V90ConstellationDesigner::setConstellationToNoise_forceRate` takes the same
 * six tables in the same shapes and this tree already names them `ucode`,
 * `alt`, `dmin`, `topUcode` and `allow`; using anything else for the same
 * arrays would be a second vocabulary for one set of objects.  What the ONE
 * caller passes corroborates the shapes rather than the names --
 * `V90Demodulator::exitPhase3` hands over `V90AutoDigitalImpDetector::linMapp`
 * (+0x0000), `::linMappAlt` (+0x0600), `::byte_0d00` (+0x0d00) and
 * `::short_2800` (+0x2800), and `topUcode` is the return of
 * `V90Phase3Demodulator::getMaxUcode` and `maxLookahead` of
 * `V90Jd::getMaxLookahead`, both of which ARE the author's own words because
 * they come out of a mangled name.  `maxTxIndex` is the author's word too:
 * the first diagnostic prints it as "max TX index".
 *
 * `unused` IS NEVER READ.  There is no reference to +0x120 anywhere in the
 * 3,767 bytes; the test drives it varying and asserts nothing changes, so
 * that is measured rather than assumed.
 *
 * THE TWO DIVIDES ARE SPELLED DIFFERENTLY AND THAT IS FORCED.  The iterative
 * arm computes `x * (1.0f / y)` -- `fld1` hoisted out of the loop, then
 * `fdivr`/`fmulp` at both of its uses -- and the `dmin[k] == 0` arm computes
 * `x / y`, a single `fdivp`.  Measured on the period compiler: a `/` gives
 * `fdivp` and a reciprocal-multiply gives `fdivr`+`fmulp`, and this tree's
 * flags forbid the compiler from turning one into the other (that is what
 * `-funsafe-math-optimizations` licenses and it is not set).  The two spell
 * different NUMBERS -- the results go straight through a `(short)`
 * truncation, so a last-bit difference is a different constellation.
 * Finding F4403.
 */
short
V90TRN2Designer::V90TRN2Design(V90MappingParams *mappingParams,
			       short (*ucode)[V90_CONSTELLATION_MAX],
			       short (*alt)[V90_CONSTELLATION_MAX],
			       unsigned char (*allow)[V90_CONSTELLATION_MAX],
			       short *dmin,
			       PcmType codecPcmType, PcmType pcmType,
			       short unused,
			       unsigned char *topUcode,
			       unsigned int maxLookahead,
			       unsigned char maxTxIndex,
			       V90SpecialSpectralConditions cond)
{
	unsigned char top[V90_CONSTELLATIONS];
	unsigned char maxLevel;
	unsigned int id;
	unsigned int root;
	unsigned int index;
	int level;
	int i, k;
	float p;
	float dBm0;

	/*
	 * +0x61c, the same slot `V90ConstellationDesigner::process` stores 1
	 * into and which nothing in the object reads.  This is its second
	 * writer and it stores the same constant.
	 */
	mappingParams->word_61c = 1;

	for (i = 0; i <= 5; i++)
		mappingParams->constellationSize[i] = params->nofUcodesInTrn2;

	/*
	 * The spectral shaper, written out again rather than shared: this is
	 * a different class from `V90ConstellationDesigner`, whose
	 * `spectralDesign` does the same six copies, and neither can call the
	 * other.  The LIMIT IS OUTSIDE BOTH ARMS here and inside them there,
	 * which is the one structural difference and is what the object says:
	 * each arm loads its own identifier into the same register and the
	 * `cmp`/`jbe` sits in the shared tail.
	 *
	 * The comparison is UNSIGNED (`jbe`), which the `unsigned int`
	 * argument forces whatever `SPECTRAL_SHAPER_ID`'s own `int` says.
	 */
	if (cond == V90_SPECTRAL_GERMAN_PBX) {
		mappingParams->shaperA1 = params->GERMAN_PBX_SPECTRAL_SHAPER_A1;
		mappingParams->shaperSR = params->GERMAN_PBX_SPECTRAL_SHAPER_SR;
		mappingParams->shaperA2 = params->GERMAN_PBX_SPECTRAL_SHAPER_A2;
		mappingParams->shaperB1 = params->GERMAN_PBX_SPECTRAL_SHAPER_B1;
		mappingParams->shaperB2 = params->GERMAN_PBX_SPECTRAL_SHAPER_B2;
		id = (unsigned int)params->GERMAN_PBX_SPECTRAL_SHAPER_ID;
	} else {
		mappingParams->shaperA1 = params->SPECTRAL_SHAPER_A1;
		mappingParams->shaperSR = params->SPECTRAL_SHAPER_SR;
		mappingParams->shaperA2 = params->SPECTRAL_SHAPER_A2;
		mappingParams->shaperB1 = params->SPECTRAL_SHAPER_B1;
		mappingParams->shaperB2 = params->SPECTRAL_SHAPER_B2;
		id = (unsigned int)params->SPECTRAL_SHAPER_ID;
	}
	if (id > maxLookahead)
		id = maxLookahead;
	mappingParams->shaperId = id;

	/*
	 * The same line `V90ConstellationDesigner::adjustConstellationsPower`
	 * carries, and it is what NAMES +0x000: the bit count of one frame.
	 * `V90ConstellationPower` reads it back as
	 * `1LL << (shaperSR + word_0 - 6)`, so the two are inverses.
	 */
	mappingParams->word_0 = maxK(mappingParams)
				- mappingParams->shaperSR + 6;

	for (i = 0; i <= 5; i++)
		mappingParams->distinctIndex[i] = 0;

	/*
	 * The ceiling on a transmitted level, from the power ladder.
	 *
	 * THE INDEX IS `maxTxIndex + 4` AND THE 4 IS IN THE OBJECT, not in the
	 * reading: the relocation against `averagePowerLimits` carries an
	 * inline addend of 0x10 and the scale is 4, so the effective address
	 * is `&averagePowerLimits[maxTxIndex + 4]`.  A REL addend is easy to
	 * lose -- see the top of `tools/dis.py` -- so the test sweeps
	 * `maxTxIndex` over a range where the ladder's entries differ, which
	 * separates `+ 4` from `+ 0` in behaviour and not only in bytes.
	 *
	 * The root is ONE `unsigned int`, not a `long long`: it comes out
	 * through `fistpll` with the low word taken and goes straight back in
	 * through `push` of a ZEROED REGISTER and `fildll`.  A `long long`
	 * would have pushed its own high dword; a hard zero is the unsigned-32
	 * idiom running the other way, and one declaration explains both ends.
	 */
	root = (unsigned int)trn2_x87_fsqrt((long double)
		V90ConstellationPower::averagePowerLimits[maxTxIndex + 4]);
	level = (int)(root * 1.6270744800567627f);
	maxLevel = (unsigned char)(TRN2_COMPAND(level) - 2);

	edprintf("V90TRN2Design: max TX index = %d, givin power limit = %d, "
		 "implying max level =%d\r\n", maxTxIndex, root, level);
	edprintf("V90TRN2Design: ...hence max ucode (after factor) = %d, "
		 "by params maxUcode = %d\r\n", maxLevel, params->maxUcode);

	/*
	 * THE LARGER OF THE TWO WINS, and that is what the object does rather
	 * than an interpretation of it: `cmp %al,0x73(%esp) ; jae` keeps the
	 * computed byte only when it is already at or above the parameter.
	 * A companded codeword RISES with amplitude -- measured, 0 for silence
	 * and 127 for full scale, both laws -- so `params->maxUcode` acts as a
	 * FLOOR under the ceiling: it stops the power ladder from setting a
	 * limit quieter than the parameter allows.  The name is the object's
	 * (finding F3527) and this comment says only what the instructions say.
	 *
	 * The field is `int` and the comparison is one byte, so the cast is at
	 * the use -- `V90Parameters.h`'s layout is frozen and its punned sites
	 * are their own batch (docs/plan.md section 3).
	 */
	if (maxLevel < (unsigned char)params->maxUcode)
		maxLevel = (unsigned char)params->maxUcode;

	/*
	 * Walk each phase's starting ucode down until its level fits under
	 * the ceiling.  Nothing stops this at zero; the byte wraps.
	 */
	for (k = 0; k <= 5; k++) {
		top[k] = topUcode[k];
		while (TRN2_COMPAND(ucode[k][top[k]]) > maxLevel)
			top[k]--;
	}

	for (k = 0; k <= 5; k++) {
		short slot;

		if (dmin[k] != 0) {
			short d;
			short iter;
			short bound;

			/*
			 * `dmin[k]` is READ ONLY AS A FLAG.  Its value never
			 * reaches the arithmetic -- the target this arm aims
			 * at is derived from the starting level instead -- and
			 * the name is the sibling's for the same array rather
			 * than a claim about this use.
			 */
			d = (short)(ucode[k][top[k]]
				    * (1.0f
				       / (params->nofUcodesInTrn2 - 0.5f)));
			slot = 0;
			iter = 0;
			while (slot < params->nofUcodesInTrn2 && iter <= 199) {
				int a = alt[k][top[k]] - d;
				int b = ucode[k][top[k]] - d;
				short half = (short)(d / 2);
				short lo, hi;

				bound = (short)(a <= b ? a : b);
				mappingParams->constellation[k][0] = top[k];
				slot = 1;

				for (i = top[k] - 1;
				     i >= params->unnamed_360; i--) {
					if (ucode[k][i] < half)
						break;
					if (alt[k][i] < half)
						break;
					if (ucode[k][i] == 0)
						break;
					if (alt[k][i] == 0)
						break;
					if (ucode[k][i] <= bound
					    && alt[k][i] <= bound
					    && allow[k][i] != 0) {
						mappingParams
						    ->constellation[k][slot] =
							(unsigned char)i;
						slot++;
						a = alt[k][i] - d;
						b = ucode[k][i] - d;
						bound = (short)(a <= b ? a : b);
						if (slot == params
							    ->nofUcodesInTrn2)
							break;
					}
				}

				/*
				 * Both tables at wherever the scan stopped,
				 * whichever way it stopped.  The object reads
				 * them on every exit edge, which is what says
				 * they are read AFTER the loop and not inside
				 * it.
				 */
				hi = ucode[k][i];
				lo = alt[k][i];
				iter++;
				d = (short)(d - (d * 0.5f - (lo <= hi
							    ? lo : hi))
					        * (1.0f
						   / (params->nofUcodesInTrn2
						      + 0.5f)));
			}
			if (slot == params->nofUcodesInTrn2)
				continue;
		} else {
			short d;
			short threshold;
			unsigned char code;

			d = (short)(0.85f * ucode[k][top[k]]
				    / (params->nofUcodesInTrn2 - 0.5f));
			edprintf("Trn2 Design: phase %d, dMin = %d...\r\n",
				 k, d);

			/*
			 * `unnamed_360` is read here as ONE UNSIGNED BYTE
			 * (`movzbl`) and in the arm above as a SIGNED INT
			 * (`cmp %esi,0x360(%ebp)`).  Both casts are at the
			 * use for the reason `maxUcode`'s is.
			 */
			threshold = (short)(d / 2);
			code = (unsigned char)params->unnamed_360;
			for (slot = (short)(params->nofUcodesInTrn2 - 1);
			     slot >= 0; slot--) {
				while (ucode[k][code] < threshold
				       && code <= top[k])
					code++;
				mappingParams->constellation[k][slot] = code;
				threshold = (short)(d + ucode[k][code]);
				if (top[k] <= code) {
					slot--;
					break;
				}
			}
		}
		/*
		 * ONE SLOT COUNTER, TWO MEANINGS, and the object shares it:
		 * the arm above counts upwards and this one counts down, and
		 * both converge on `slot >= 0` deciding whether the phase was
		 * designed.  A count that reached the target has already
		 * `continue`d; a descending index that ran out is -1.
		 */
		if (slot >= 0)
			goto failed;
	}

	/*
	 * The codec constellation.  When the two `PcmType`s agree the entry is
	 * the SMALLER of the companded level and the ucode itself, and the
	 * object computes the companding TWICE to do it -- once for the test
	 * and once for the store -- because `linear2alaw` is an external call
	 * it may not fold.  The macro reproduces that; a temporary would not.
	 */
	for (k = 0; k <= 5; k++)
		for (i = 0; i < params->nofUcodesInTrn2; i++) {
			unsigned char u = mappingParams->constellation[k][i];

			if (codecPcmType == pcmType)
				mappingParams->codecConstellation[k][i] =
				    TRN2_COMPAND(ucode[k][u]) < u
					? TRN2_COMPAND(ucode[k][u])
					: mappingParams->constellation[k][i];
			else
				mappingParams->codecConstellation[k][i] =
				    TRN2_COMPAND(ucode[k][u]);
		}

	p = power->getPower(mappingParams,
			    V90_TX_POWER_OVER_CODEC_CONSTELLATION, pcmType);
	index = power->getPowerIndexForPower(p);

	edprintf("--------------------------------------\r\n");
	edprintf("V90TRN2Design:\r\n");
	edprintf("V90TRN2Design: sqrt(power)  = %c%d.%03d\r\n",
		 !(0.0f >= p) ? '+' : '-',
		 (int)__builtin_fabsl(trn2_x87_fsqrt((long double)p)),
		 __builtin_abs((int)((trn2_x87_fsqrt((long double)p)
				      - (long double)(int)trn2_x87_fsqrt(
						(long double)p))
				     * 1000.0f)));

	dBm0 = (index + 1) * -0.5f;
	edprintf("V90TRN2Design: index %d, power in dBm0  = %c%d.%01d\r\n",
		 index,
		 !(0.0f >= dBm0) ? '+' : '-',
		 (int)__builtin_fabsf(dBm0),
		 __builtin_abs((int)((dBm0 - (float)(int)dBm0) * 10.0f)));

	/*
	 * Four separately gated prints, each re-reading `dsplibs_debug_level`
	 * -- which is forced, because `dsplibs_debug_printf` is an external
	 * call and may have changed it.  A single `if` around all four would
	 * read it once.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "\n--------------------------------------\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90TRN2Designer : ADI design report : \r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("trn2 size  :  %d\r\n",
				     params->nofUcodesInTrn2);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("\n" "------------------------------"
				     "------------------------------"
				     "------------------------------"
				     "------------" "\r\n");

	for (i = 0; i < params->nofUcodesInTrn2; i++)
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "trn2 constel[%d]  :  %d %d %d %d %d %d  :  "
			    "%d %d %d %d %d %d  :  %d %d %d %d %d %d\r\n", i,
			    mappingParams->constellation[0][i],
			    mappingParams->constellation[1][i],
			    mappingParams->constellation[2][i],
			    mappingParams->constellation[3][i],
			    mappingParams->constellation[4][i],
			    mappingParams->constellation[5][i],
			    mappingParams->codecConstellation[0][i],
			    mappingParams->codecConstellation[1][i],
			    mappingParams->codecConstellation[2][i],
			    mappingParams->codecConstellation[3][i],
			    mappingParams->codecConstellation[4][i],
			    mappingParams->codecConstellation[5][i],
			    ucode[0][mappingParams->constellation[0][i]],
			    ucode[1][mappingParams->constellation[1][i]],
			    ucode[2][mappingParams->constellation[2][i]],
			    ucode[3][mappingParams->constellation[3][i]],
			    ucode[4][mappingParams->constellation[4][i]],
			    ucode[5][mappingParams->constellation[5][i]]);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("\n" "------------------------------"
				     "------------------------------"
				     "------------------------------"
				     "------------" "\r\n");
	return 1;

failed:
	setTrn2DummyConstel(mappingParams);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90TRN2Designer::V90TRN2Design()  "
				     "constelation design failed !!!\r\n");
	return 0;
}
