/*
 * V90AutoDigitalImpDetector.cpp -- the V.90 digital-impairment detector's two
 * reset methods.
 *
 * Two of the class's thirty-two members: `reset(unsigned char, PcmType,
 * short)` and `resetLinearMapping()`, which are the two that are leaves --
 * between them they call only `alaw2linear` and `ulaw2linear`.
 * include/dsplib/V90AutoDigitalImpDetector.h carries the object map and the
 * measurement the 43,440-byte size comes from.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL.  `this` is the first *stack*
 * argument -- `mov 0x20(%esp),%ebx` after four pushes and a twelve-byte frame
 * -- not %ecx, so these are not thiscall and nothing here needs an attribute
 * (finding F215).
 *
 * Built -fno-exceptions -fno-rtti -nostdinc++ like the rest of the C++ here;
 * see the Makefile.  No virtuals, no allocation, no static data members, so
 * the test binaries still link with $(CC).
 */

#include <stddef.h>

extern "C" {
#include "dsplib/pcm.h"
#include "dsplib/debug.h"
#include "dsplib/encode.h"
}

#include "dsplib/V90AutoDigitalImpDetector.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but only parses `struct name {` out of include/dsplib, so
 * a C++ class has to assert its own -- and it is exactly the check that
 * catches an object right in size and wrong by four in every offset, which is
 * the failure docs/v90cpp.md warns about for the classes that DO have a vptr.
 *
 * GUARDED ON THE POINTER WIDTH, because the class holds one -- `params` at
 * +0x2814 -- and `make check64` compiles this file `-fsyntax-only` for the
 * native target, where that pointer is eight bytes and every offset after it
 * moves.  The claim is about the 32-bit layout the blob has, so it is only
 * asserted where the compiler is laying that layout out.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define ADID_OFF(field, off, tag) \
	typedef char adid_off_##tag[ \
	    ((int)__builtin_offsetof(V90AutoDigitalImpDetector, field) \
	     == (off)) ? 1 : -1]

ADID_OFF(linMapp,      0x0000, linmapp);
ADID_OFF(linMappAlt,   0x0600, linmappalt);
ADID_OFF(byte_0d00,    0x0d00, byte0d00);
ADID_OFF(float_1000,   0x1000, float1000);
ADID_OFF(uint_1c00,    0x1c00, uint1c00);
ADID_OFF(short_2800,   0x2800, short2800);
ADID_OFF(byte_280c,    0x280c, byte280c);
ADID_OFF(params,       0x2814, params);
ADID_OFF(short_8b00,   0x8b00, short8b00);
ADID_OFF(int_9100,     0x9100, int9100);
ADID_OFF(float_9118,   0x9118, float9118);
ADID_OFF(float_9d18,   0x9d18, float9d18);
ADID_OFF(uint_9d30,    0x9d30, uint9d30);
ADID_OFF(float_9d48,   0x9d48, float9d48);
ADID_OFF(short_a948,   0xa948, shorta948);
ADID_OFF(pcmType,      0xa95c, pcmtype);

/*
 * THE FIVE THIS BATCH ADDS, and what proved each.  These are the fields the
 * header used to spell `pad_*`; an offset here is the whole claim, so each
 * one names the instruction it was read from.
 *
 * +0x0c00  `mov %ax,0xc00(%ebx,%edx,2)` in setPrevSessionLinearMapping,
 *          `edx` running 0..0x7f -- 128 shorts, the region's whole extent.
 * +0x2818  `mov %ax,0x2818(%edi,%esi,2)` in addReceivedSampleToStorage with
 *          `esi = 0x83e * phase + count`; 6 * 0x83e shorts is 0x62e8 bytes,
 *          which reaches +0x8b00 exactly.
 * +0xa94c  `fdivs 0xa94c(%ecx)` in applyPadGainToLinMapp -- a float, and the
 *          divisor the method's name calls a pad gain.
 * +0xa956  `mov %al,0xa956(%edx,%ebx,1)` in setMaxUcodeArray, `edx` 0..5.
 * +0xa9a6  `movswl 0xa9a6(%ebx),%edx` in isAltRbs, compared against a
 *          non-negative distance with a SIGNED `jle`.
 * +0xa9a4  `movswl 0xa9a4(%ebp),%edx` in unitePhasesInfoOfUref, compared
 *          against a non-negative distance with a SIGNED `jge`.
 */
ADID_OFF(prevLinMapp,  0x0c00, prevlinmapp);
ADID_OFF(sampleStore,  0x2818, samplestore);
ADID_OFF(padGain,      0xa94c, padgain);
ADID_OFF(maxUcode,     0xa956, maxucode);
ADID_OFF(short_a9a4,   0xa9a4, shorta9a4);
ADID_OFF(short_a9a6,   0xa9a6, shorta9a6);
ADID_OFF(ucode,        0xa96b, ucode);
ADID_OFF(ucodeLevel,   0xa96c, ucodelevel);
ADID_OFF(short_a96e,   0xa96e, shorta96e);
ADID_OFF(float_a970,   0xa970, floata970);
ADID_OFF(float_a974,   0xa974, floata974);
ADID_OFF(short_a978,   0xa978, shorta978);
ADID_OFF(short_a97a,   0xa97a, shorta97a);
ADID_OFF(float_a97c,   0xa97c, floata97c);
ADID_OFF(float_a980,   0xa980, floata980);

/*
 * THE ELEVEN THE STUDY BATCH ADDS, which between them retire every `pad_`
 * region in the object outside the sample store.  Each names the instruction
 * that fixed it; a `pad_` split is a claim about memory nothing tests, so the
 * disassembly is the whole of the evidence.
 *
 * +0xa950  `flds 0xa950(%ebx)` at 0x408b4 in resetStudyUrefHandler -- a float,
 *          and the divisor every threshold that method installs is scaled by.
 * +0xa964  `flds 0xa964(%esi)` at 0x4124b in porcessFirstStudy, printed under
 *          the name "2.5*trn1Sigma"; `fsts 0xa964(%ebx)` in studyUrefHandler.
 * +0xa96a  `movb $0x0,0xa96a(%esi)` and `incb 0xa96a(%esi)` in
 *          porcessFirstStudy -- one byte, counting the phases it flags.
 * +0xa984  `mov %ecx,0xa984(%ebx)` (zero) in resetStudyUrefHandler and
 *          `mov 0xa984(%ebx),%eax` in studyUrefHandler: 32 bits, integer.
 * +0xa988  `incl 0xa988(%ebx)` in studyUrefHandler -- a 32-bit counter, so
 *          not a float whatever the zero store alone would allow.
 * +0xa98c..+0xa9a0  six 32-bit `mov` copies out of the parameter block, at
 *          0x408cc..0x40913 and again at 0x40a36..0x40a94; five of the six are
 *          then compared against +0xa988 with `cmp`/`je` in studyUrefHandler.
 * +0xa9a8  `fstps 0xa9a8(%ebx)` at 0x40954 and `flds 0xa9a8(%edx)` at 0x40875
 *          in getAltVarThresh -- a float, printed as "altMinVarThresh".
 * +0xa9ac  `mov %ax,0xa9ac(%ebx)` at 0x4099c and `filds 0xa9ac(%esi)` at
 *          0x41251 -- a short, printed as "neighborUcodeMinDistance".
 * +0xa9ae  `mov %cx,0xa9ae(%ebx)` and `filds 0xa9ae(%esi)` at 0x4126c -- a
 *          short, and the displacement the object's size is measured from.
 */
ADID_OFF(float_a950,   0xa950, floata950);
ADID_OFF(trn1Sigma,    0xa964, trn1sigma);
ADID_OFF(byte_a96a,    0xa96a, bytea96a);
ADID_OFF(int_a984,     0xa984, inta984);
ADID_OFF(int_a988,     0xa988, inta988);
ADID_OFF(int_a98c,     0xa98c, inta98c);
ADID_OFF(int_a990,     0xa990, inta990);
ADID_OFF(int_a994,     0xa994, inta994);
ADID_OFF(int_a998,     0xa998, inta998);
ADID_OFF(int_a99c,     0xa99c, inta99c);
ADID_OFF(int_a9a0,     0xa9a0, inta9a0);
ADID_OFF(altMinVarThresh,          0xa9a8, altminvarthresh);
ADID_OFF(neighborUcodeMinDistance, 0xa9ac, neighborucodemin);
ADID_OFF(neighborUcodeMaxDistance, 0xa9ae, neighborucodemax);

/*
 * THE ONE THE DIL BATCH ADDS -- one of the three gaps finding F1424 left, and
 * the only one of them any member of the class reads.
 *
 * +0xa968  `mov %ax,0xa968(%esi)` at 0x41cd0 and `mov %bx,0xa968(%esi)` at
 *          0x41cec in porcessSecondStudy, and the same pair in
 *          setQcLinearMapping -- sixteen-bit stores.  The three readers
 *          sign-extend: `movswl 0xa968(%ebp),%esi` at 0x418ff in
 *          updateAltRbsPhaseInDil, and the same instruction four times in
 *          findPadGain and five times in determineMaxUcode.  Two bytes, and
 *          `byte_a96a` at +0xa96a is what bounds it above.
 */
ADID_OFF(unSuspectedPhase, 0xa968, unsuspectedphase);

/*
 * THE TWO THE PAD-GAIN BATCH ADDS, and they are the last two gaps outside the
 * sample store.  No test in this tree can see either -- the first is a byte in
 * a two-byte `pad_` region and the second is four bytes nothing else reads --
 * so the disassembly is the whole of the evidence, which is finding F1360's
 * situation exactly.
 *
 * +0xa954  `movzbl 0xa954(%esi)` at 0x4363e and `cmpb $0x3f,0xa954(%esi)` at
 *          0x43801 in findPadGain, `mov %bl,0xa954(%ebp)` at 0x4449f in
 *          determineMaxUcode -- seven accesses, all eight bits and all
 *          unsigned, so one byte and not the two the region held.
 * +0xa960  `mov %ecx,0xa960(%esi)` at 0x43e93 and `mov %edi,0xa960(%esi)` at
 *          0x440c4 in findPadGain -- 32-bit stores of 0 and 1, which is what
 *          makes the whole four-byte region one `int`.
 */
ADID_OFF(byte_a954,    0xa954, bytea954);
ADID_OFF(int_a960,     0xa960, inta960);

typedef char adid_size[(sizeof(V90AutoDigitalImpDetector) == 0xa9b0) ? 1 : -1];

#endif /* 32-bit */

/*
 * V90Parameters is not modelled -- include/dsplib/V90PreFilter.h declares it
 * as an unnamed word block for exactly this reason -- and `reset` reads
 * exactly one thing out of it: the `short` at +0x0c, which it compares
 * against 2.  Reading it through a byte pointer keeps the offset numeric,
 * which is the honest spelling while the block's own layout is unknown.
 */
#define V90PARAMETERS_CONNECTION_TYPE 0x0c

/*
 * The two six-word blocks `resetStudyUrefHandler` copies into +0xa98c..+0xa9a0.
 * Which one it takes is its argument: nonzero picks the first.  Both are read
 * as 32-bit words and nothing here interprets them, so they stay numeric for
 * the same reason the connection type does.
 */
#define V90PARAMETERS_STUDY_QC		0x4a8
#define V90PARAMETERS_STUDY_PLAIN	0x348

/* Ungrouped.  The object writes and compares 0xffff as a `short`. */
#define ADID_NO_GROUP	((short)-1)

/*
 * THE SIGN TEST IS BACKWARDS FROM THE OBVIOUS SPELLING, and a NaN is what
 * separates them.  The object compares with `fcom` against a zero in %st(0)
 * and takes '+' when CF is set -- and an UNORDERED compare sets CF, so a NaN
 * prints '+' where `v > 0.0f ? '+' : '-'` would print '-'.  Written as the
 * negation of the ordered test, which is what the branch encodes.  A seeded
 * object reaches this: one 32-bit pattern in 128 is a NaN.
 *
 * THE ZERO IS ON THE LEFT AND THE NEGATION IS ON THE OUTSIDE, and both halves
 * of that are forced.  The object's three sites in `getAltVarThresh` are
 *
 *	fldz / fcompp (or fcoms) / fnstsw %ax / sahf
 *	sbb %eax,%eax / and $0xfffffffe,%eax / add $0x2d,%eax
 *
 * -- 0x4073c, 0x407c4 and 0x40859 -- so there is no branch at all: the
 * character is 0x2d - 2*CF, which is '+' exactly when the compare set CF.
 * That is only encodable when the TRUE arm is the CF one, and CF is "below or
 * unordered" with the ZERO in %st(0).  Spelling it `(0.0f >= v) ? '-' : '+'`
 * puts the true arm on the other side: GCC 3.4.2 then commutes the compare to
 * `v <= 0.0f`, emits `flds v`/`fcomps 0.0f`/`ja`, and a NaN -- which sets CF
 * and ZF both -- fails `ja` and prints '-' where the object prints '+'.
 * Measured: of ten spellings compiled with the object's flags, this is the
 * only one that reproduces the `sbb`/`and`/`add` triple.
 *
 * The two spellings are the SAME under IEEE rules -- `0.0f >= NaN` is false
 * either way -- so this is an operand-order fix and not a change of meaning;
 * the modern build is unaffected.  The old comment argued the `-mieee-fp`
 * case, which no longer applies.  Findings F1990 and F2300.
 */
#define ADID_PRINT_SIGN(v)	(!(0.0f >= (v)) ? '+' : '-')

/* `fabs` then a truncating `fistpl`. */
#define ADID_PRINT_WHOLE(v)	((int)__builtin_fabsf(v))

/*
 * The hundredths: the fractional part scaled and truncated, then made
 * positive with the integer `cltd; xor; sub` the class uses everywhere.  The
 * scale is passed in because it is NOT the same constant at the two sites --
 * `getAltVarThresh` multiplies by the double at `.rodata.cst8+0xe0` and
 * `resetStudyUrefHandler` by the float at `.rodata.cst4+0x320`, both 100.
 */
#define ADID_PRINT_FRAC(v, scale) \
	adid_abs((int)((scale) * ((v) - (float)(int)(v))))

/*
 * BOTH ARE `inline`, AND IT IS NOT A STYLE PREFERENCE -- IT IS THE ONLY
 * SPELLING THAT SURVIVES THE PERIOD COMPILER.
 *
 * The object has no such helper: every read of the parameter block is a plain
 * `mov 0x4a8(%edx),%eax` in the caller's own instruction stream.  Modern GCC
 * inlines a `static` function used twelve times without being asked, so the
 * modern build looks the same.  **GCC 3.4.2 does not**: `-finline-functions`
 * is an -O3 flag in 3.x, so at -O2 it inlines only what is DECLARED `inline`,
 * and it emitted an out-of-line copy plus twelve calls.
 *
 * That is not a cosmetic difference. `resetStudyUrefHandler` holds
 * `1.0f / float_a950` in an x87 register ACROSS these reads -- the object does
 * the divide at 0x408ca and the first use at 0x40929, with the six parameter
 * loads in between -- and a call forces the register to be spilled, which on
 * x87 means ROUNDED FROM 64 BITS OF SIGNIFICAND TO 24.  GCC 3.4.2 duly emitted
 * `fstps 0x24(%esp)`, the reciprocal came back four bits short, and
 * `(short)(inv * 50.0f + 0.5f)` fell from 1 to 0 at +0xa9a6.  `make phase` was
 * green throughout, because the modern build had no call to spill across.
 * Finding F1448, and it is the first defect the period tier caught that the
 * modern tier could not see.
 */
static inline short
paramShort(const V90Parameters *p, unsigned int off)
{
	return *(const short *)((const unsigned char *)p + off);
}

static inline int
paramWord(const V90Parameters *p, unsigned int off)
{
	return *(const int *)((const unsigned char *)p + off);
}

/*
 * The magnitude of a signed 32-bit value.  The object spells it `cltd; xor
 * %edx,%eax; sub %edx,%eax` in nine places; this is the same thing, and GCC
 * emits that sequence (or `mov`/`sar`/`xor`/`sub`, which is the same
 * arithmetic) for it.  Written here rather than calling `abs` because this
 * translation unit is built `-nostdinc++`.
 *
 * `inline`, FOR THE SAME REASON `paramWord` IS -- see the note there.  GCC
 * 3.4.2 does not inline a plain `static` function at -O2, and the object has
 * no such call anywhere; the one in `findPadGain`'s report sits between the
 * computation of `err` and the `(int)err` that prints it, so the call spilled
 * `err` from an x87 register to a four-byte slot and the printed error came
 * back one too high.  Finding F1448.
 */
static inline int
adid_abs(int v)
{
	return v < 0 ? -v : v;
}

/*
 * `trn1Sigma`: the mean of the unsuspected phases' variances for the reference
 * code.  The object has this twice as well, at 0x42c10 and 0x42ea9, differing
 * only in the format string -- which is why the string is the parameter.
 *
 * THE FIELD AND THE PRINT GET TWO DIFFERENT ROUNDINGS OF ONE VALUE, and that
 * is the whole reason this is written as it is.  The sum is accumulated in
 * %st(0) and never spilled, so it and the quotient are at the x87's 64-bit
 * significand; `fsts 0xa964` rounds to a float on the way into the field, and
 * the print's `fstpl 0x4(%esp)` rounds the SAME register to a double on the way
 * into the argument slot.  A float widened to a double has twenty-nine zero
 * mantissa bits and the true value does not, so the two differ in exactly the
 * half `%d` goes on to read.  Keeping the value in one C variable across both
 * uses is what reproduces it; a `float` return or a `float` parameter would
 * force a 32-bit slot and rounds it once for both.  Finding F1437's situation
 * and finding F1443.
 *
 * NOTHING BOUNDS THE COUNT.  Six flagged phases give 0.0f/0, a NaN, which is
 * then stored and printed -- the same shape as D282 and reachable the same way.
 *
 * THE FORMAT HAS NO CONVERSION FOR WHAT IT IS HANDED.  `%d` against a `float`
 * promoted to a `double` is four bytes of mantissa read as an integer.  That is
 * the object's call and it is reproduced; docs/deviations.md D291.
 */
static void
adid_updateTrn1Sigma(V90AutoDigitalImpDetector *o, const char *fmt)
{
	float sum = 0.0f;
	short n = 0;
	short phase;

	for (phase = 0; phase < V90ADID_PHASES; phase++)
		if (o->short_2800[phase] == 0) {
			sum += o->float_9d48[phase][o->ucode];
			n = (short)(n + 1);
		}

	sum = sum / n;
	o->trn1Sigma = sum;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(fmt, sum);
}

/*
 * Clear the per-phase measurement state and install the session's reference
 * code, its companding law and its alternate-RBS flag.
 *
 * The five arrays cleared per (phase, code) and the five scalars cleared per
 * phase are the object's whole measurement state; `linMapp` and `linMappAlt`
 * are NOT among them -- `resetLinearMapping` below is a separate call, and
 * `reset` does not make it.
 *
 * The code-to-level conversion is the blob's, and it is the same one
 * V90Phase3Modulator's DIL expansion uses: the argument is a seven-bit
 * magnitude, and the sign/company bits are supplied here -- `(code & 0x7f) ^
 * 0xd5` for A-law and `(code & 0x7f) ^ 0xff` for mu-law, which is `~` of the
 * masked code.  Both produce a code in 0x80..0xff.
 *
 * The four thresholds at the end come from the parameter block's +0x0c: two
 * sets of (short, short, float, float) selected by whether it is 2.  The
 * float constants are the object's own bit patterns -- 0x40a00000, 0x3fc00000,
 * 0x3e800000, 0x3eb33333, 0x3fe00000 -- read out of the immediate operands,
 * not out of a decompilation.
 */
/*
 * The constructor -- one store, and the header says why that is the whole of
 * it.  Nothing is cleared here, so a detector is unusable until `reset` and
 * `resetLinearMapping` have run; the test's whole-object comparison is what
 * makes "leaves the other 43,425 bytes alone" a measurement.
 */
V90AutoDigitalImpDetector::V90AutoDigitalImpDetector(V90Parameters *p)
{
	params = p;
}

/*
 * One byte, `ret`.  Declared because the blob has the symbol; see the header.
 */
V90AutoDigitalImpDetector::~V90AutoDigitalImpDetector()
{
}

/*
 * ==========================================================================
 * The processing methods.
 *
 * WHAT THE CLASS MEASURES.  Six RBS phases, and for each a running count and
 * a running sum of |sample| and of |sample|^2, kept per PCM code in
 * (+0x1c00, +0x1000, +0x9118) and per phase in (+0x9d30, +0x9d18).  A
 * `*MeanAndVar` method divides the sums by the count to get a mean and a
 * variance, and rounds the mean into `linMapp` -- so `linMapp[phase][code]`
 * is "the linear level this phase actually delivers for this code", and
 * `linMappAlt` is the same under the alternate-RBS hypothesis.  The `Alt`
 * half is always per phase where the plain half is per phase per code, which
 * is the difference to look for when reading a pair.
 *
 * ROUNDING IS AN EXPLICIT +0.5f EVERYWHERE.  Every `fistp` in the class is
 * preceded by `fnstcw` / `or $0xc00` / `fldcw`, which is round-toward-zero --
 * a C cast to an integer type, not a rounding one.  So where the object adds
 * a constant 0.5f before the store, that addition IS the rounding and is part
 * of the arithmetic; it is not the compiler's doing and it does not
 * disappear.
 *
 * THE DIVISIONS ARE NOT INTERCHANGEABLE.  `updateLinMappMeanAndVar` and
 * `updateUrefAlt` compute `1.0f / count` and multiply by it;
 * `updateLinMappMeanAndVarAlt` divides directly.  All three are in the object
 * and GCC will not turn one into the other without `-ffast-math`, so each is
 * written the way the object has it.
 *
 * That they differ AT ALL had to be earned rather than asserted.  Swapping
 * the two forms survives 64 seeded trials and 10,700 comparisons over forty
 * blocks of samples: random floats do not land close enough to a rounding
 * boundary for one ulp in the mean to survive `+ 0.5f` and a truncating
 * `fistp`.  `t_v90adid` seeds a constructed witness instead -- count 41, sum
 * 143.5, where the division is exactly 3.5 and the reciprocal is a hair under
 * -- and with it all three mutations are caught.  Finding F1366, which is also
 * why the search for that witness had to mirror this whole function body and
 * not just the expression.
 * ==========================================================================
 */

/*
 * Clear one (phase, code) cell of the three cumulative registers.  The second
 * argument is the code; there is no third register to clear per phase, so the
 * variance at +0x9d48 is deliberately left alone -- it is an output, not an
 * accumulator.
 */
void
V90AutoDigitalImpDetector::clearCamulativeVal(short phase, short code)
{
	uint_1c00[phase][code] = 0;
	float_1000[phase][code] = 0.0f;
	float_9118[phase][code] = 0.0f;
}

/*
 * The alternate-RBS pair of the above -- and it takes two arguments of which
 * IT USES ONLY THE FIRST.  The second is loaded into no register and named in
 * no displacement anywhere in the thirty-one bytes; the mangling says it is
 * there, so it is declared, and it is left unnamed here to say that reading
 * it is not an omission.  docs/deviations.md D257.
 */
void
V90AutoDigitalImpDetector::clearCamulativeAltVal(short phase, short)
{
	uint_9d30[phase] = 0;
	float_9d18[phase] = 0.0f;
}

/*
 * Install the four thresholds that depend on the connection type, which is
 * the same pair of value sets `reset` selects between on the parameter
 * block's +0x0c.
 *
 * THE ELSE ARM WRITES THREE FIELDS WHERE THE IF ARM WRITES FOUR: +0xa978 is
 * set to 1 when the type is 2 and is left at whatever it held otherwise,
 * where `reset` clears it on the same test.  That asymmetry is the object's;
 * there are three stores in the else arm and there is no fourth anywhere in
 * the ninety-seven bytes.  docs/deviations.md D255.
 */
void
V90AutoDigitalImpDetector::setConnectionType(short type)
{
	if (type == 2) {
		short_a978 = 1;
		short_a97a = 88;
		float_a97c = 0.35f;
		float_a980 = 1.75f;
	} else {
		short_a97a = 80;
		float_a97c = 0.25f;
		float_a980 = 1.5f;
	}
}

void
V90AutoDigitalImpDetector::reset(unsigned char code, PcmType law, short altRbs)
{
	short phase;

	pcmType = law;
	ucode = code;

	if (law != PCM_TYPE_MU_LAW)
		ucodeLevel = (short)alaw2linear(
		    (unsigned char)((code & 0x7f) ^ 0xd5));
	else
		ucodeLevel = (short)ulaw2linear(
		    (unsigned char)((code & 0x7f) ^ 0xff));

	short_a948 = 0;
	padGain = 1.0f;
	short_a96e = altRbs;

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		short ci;

		for (ci = 0; ci < V90ADID_CODES; ci++) {
			uint_1c00[phase][ci] = 0;
			float_1000[phase][ci] = 0.0f;
			float_9118[phase][ci] = 0.0f;
			float_9d48[phase][ci] = 0.0f;
			short_8b00[phase][ci] = 0;
			byte_0d00[phase][ci] = 1;
		}

		int_9100[phase] = 0;
		short_2800[phase] = 0;
		uint_9d30[phase] = 0;
		byte_280c[phase] = 0;
		float_9d18[phase] = 0.0f;
	}

	float_a970 = short_a96e != 0 ? 5.0f : 1.5f;
	float_a974 = 5.0f;

	if (paramShort(params, V90PARAMETERS_CONNECTION_TYPE) == 2) {
		short_a978 = 1;
		short_a97a = 88;
		float_a97c = 0.35f;
		float_a980 = 1.75f;
	} else {
		short_a978 = 0;
		short_a97a = 80;
		float_a97c = 0.25f;
		float_a980 = 1.5f;
	}
}

/*
 * Clear both linear-mapping tables and seed each phase's entry for the
 * reference code with the reference level.
 *
 * The two tables are written identically at every step -- the same 128 zeros
 * per phase, then the same `ucodeLevel` at the same `ucode` -- which is what
 * makes the pair legible as a value and its alternate-RBS hypothesis rather
 * than as two unrelated arrays.
 *
 * `ucode` is a byte and the tables are 128 wide, so a reference code of 128
 * or more would write past the end of a row.  The object does not mask it
 * here, and neither does this: `reset` is what puts a value there and it
 * stores its argument unmasked.
 *
 * `ucodeLevel` IS READ STRAIGHT INTO THE STORES, WITH NO LOCAL, and that is
 * the whole of the one byte this function used to differ by.  The object
 * hoists the load out of the phase loop itself and spells it `movzwl
 * 0xa96c(%ebx),%edi`; a `short level = ucodeLevel;` local makes GCC 3.4.2
 * emit `movswl` for the same load.  Seven spellings were compiled --
 * `(short)` cast, plain `short`, `unsigned short`, `int`, `unsigned int`, no
 * local, and no local for `at` either -- differing bytes of 111:
 *
 *     (short) cast  1     unsigned short  0 <--     no local       0 <--
 *     short         1     int             1         `at` inlined   1
 *                         unsigned int    1
 *
 * **THE PREIMAGE IS NOT UNIQUE and this is therefore NOT a decoding.**  Two
 * cells reach zero and the object cannot tell them apart, so 7771's rule
 * binds: a hit against a non-injective map is not an inference about the
 * author's text.  What IS established is negative and is the useful half --
 * the signed 16-bit local this file used to declare is excluded, because
 * every cell carrying one is off by that byte.  The no-local spelling is
 * taken as the smaller claim; `unsigned short level` would be equally exact
 * and equally unevidenced.
 *
 * AND THE FIELD ITSELF IS STILL `short`.  The `movzwl` here is finding F614's
 * free case -- the 32-bit result is discarded by a 16-bit store -- so it says
 * nothing about +0xa96c's signedness, and the rest of the object settles that
 * the other way: one `filds 0xa96c(%ebx)`, which is a SIGNED integer load,
 * and five `movswl` of the same field elsewhere.  Retyping the member would
 * have been 613's family read backwards.
 */
void
V90AutoDigitalImpDetector::resetLinearMapping()
{
	unsigned char at = ucode;
	short phase;

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		short ci;

		for (ci = 0; ci < V90ADID_CODES; ci++) {
			linMapp[phase][ci] = 0;
			linMappAlt[phase][ci] = 0;
		}

		linMapp[phase][at] = ucodeLevel;
		linMappAlt[phase][at] = ucodeLevel;
	}
}

/*
 * Is any phase flagged at +0x2800?
 *
 * THE ACCUMULATOR IS A SHORT, not an int, and that is visible: the object
 * truncates the running total back to sixteen bits with `movswl %ax,%ecx`
 * after every add.  Six flags cannot overflow it, so nothing observable turns
 * on the width -- but the test at the end is a signed `jle`, so the total is
 * signed, and six entries of -32768 would wrap where an `int` would not.
 */
int
V90AutoDigitalImpDetector::isThereAnyAltRbsPhase()
{
	short sum = 0;
	short phase;

	for (phase = 0; phase < V90ADID_PHASES; phase++)
		sum = (short)(sum + short_2800[phase]);

	return sum > 0 ? 1 : 0;
}

/*
 * Does this sample look like alternate RBS on this phase?
 *
 * Yes when the phase is flagged AND the sample's magnitude is further from
 * the phase's established level for this code than the threshold at +0xa9a6
 * allows.  Both magnitudes are taken before the subtraction, so a level and
 * a sample of opposite sign compare as though both were positive -- which is
 * the point, since the class works in code magnitudes throughout.
 *
 * The conversion of the float argument is a TRUNCATION: the object sets the
 * control word to round-toward-zero around the `fistpl`.
 */
int
V90AutoDigitalImpDetector::isAltRbs(short phase, short code, float v)
{
	int d;

	if (short_2800[phase] == 0)
		return 0;

	d = adid_abs(adid_abs((int)v) - linMapp[phase][code]);

	return d > short_a9a6 ? 1 : 0;
}

/*
 * The variance threshold the alternate-RBS test runs against, from the six
 * per-phase variances the caller hands in.
 *
 * IT IS THE MEAN OF THE BELOW-AVERAGE HALF, not the mean.  The six are summed,
 * the sum is multiplied by 1/6 to get the average, and only the entries
 * strictly below the average are averaged again -- so a single wild phase is
 * excluded from the threshold it would otherwise set.  That answer is scaled
 * by the caller's factor and then floored at `altMinVarThresh`.
 *
 * THE TWO COMPARISONS ARE WRITTEN AS THE OBJECT'S BRANCHES AND NOT AS THEIR
 * READABLE OPPOSITES.  `jae` over the accumulate means the entry joins the
 * average when it is NOT ordered-greater-or-equal, so a NaN variance joins
 * where `var[i] < lim` would exclude it.  The floor at the end is an ordered
 * `>` and needs no such care.
 *
 * NOTHING BOUNDS THE COUNT.  If every entry equals the average -- six equal
 * variances, which a cleared object has -- none is below it, the count stays
 * zero and the mean is 0.0f/0, a NaN that propagates through the factor and
 * past the floor into the return value.  docs/deviations.md D282.
 */
float
V90AutoDigitalImpDetector::getAltVarThresh(float *var, float factor)
{
	float sum = 0.0f;
	float below = 0.0f;
	float lim, mean, thresh;
	short count = 0;
	short i;

	for (i = 0; i < V90ADID_PHASES; i++)
		sum += var[i];

	lim = sum * 0.16666667f;

	for (i = 0; i < V90ADID_PHASES; i++) {
		if (var[i] >= lim)
			continue;
		below += var[i];
		count = (short)(count + 1);
	}

	mean = below / count;
	thresh = factor * mean;

	edprintf("V90AutoDigitalImpDetector: getAltVarThresh: meanVar = "
		 "%c%d.%02d\r\n", ADID_PRINT_SIGN(mean), ADID_PRINT_WHOLE(mean),
		 ADID_PRINT_FRAC(mean, 100.0));
	edprintf("V90AutoDigitalImpDetector: getAltVarThresh: factor = "
		 "%c%d.%02d\r\n", ADID_PRINT_SIGN(factor),
		 ADID_PRINT_WHOLE(factor), ADID_PRINT_FRAC(factor, 100.0));
	edprintf("V90AutoDigitalImpDetector: getAltVarThresh: initial varThresh "
		 "= %c%d.%02d\r\n", ADID_PRINT_SIGN(thresh),
		 ADID_PRINT_WHOLE(thresh), ADID_PRINT_FRAC(thresh, 100.0));

	if (altMinVarThresh > thresh)
		thresh = altMinVarThresh;

	return thresh;
}

/*
 * Install the study's thresholds, either from the parameter block's QC set or
 * from the fixed defaults, and report what was installed.
 *
 * THE ARGUMENT IS A FLAG AND NOT A COUNT.  Zero takes the second branch
 * entirely: six words out of the parameter block at +0x348, five constants,
 * and a return -- no arithmetic, no scaling and NOT ONE `edprintf`.  Nonzero
 * takes the first: a different six words at +0x4a8, and every threshold
 * derived from `1.0f / float_a950` and then clamped at exactly the value the
 * zero branch would have used.  So the two branches agree when the gain is 1
 * for three of the five and differ for the other two, which is how the
 * constants were read as a scale rather than as replacements.
 *
 * THE SEEDING OF THE MAPPING IS GUARDED ON THE GAIN BEING NONZERO, and the
 * DIVISION IS NOT.  `1.0f / float_a950` is formed at the top of the branch
 * whatever the gain is, so a gain of zero still produces infinities and
 * stores 0x8000 through every `fistp` on the way; only the write of
 * `ucodeLevel / gain` into the two mapping tables is skipped.  That ordering
 * is the object's -- the compare against 0.0f is at 0x409d2, five stores
 * later -- and it is why a zero gain is a state this method leaves behind
 * rather than one it refuses.
 */
void
V90AutoDigitalImpDetector::resetStudyUrefHandler(unsigned int qc)
{
	float inv;
	short v;
	short phase;

	if (qc == 0) {
		int_a98c = paramWord(params, V90PARAMETERS_STUDY_PLAIN + 0x00);
		int_a990 = paramWord(params, V90PARAMETERS_STUDY_PLAIN + 0x04);
		int_a994 = paramWord(params, V90PARAMETERS_STUDY_PLAIN + 0x08);
		int_a998 = paramWord(params, V90PARAMETERS_STUDY_PLAIN + 0x0c);
		int_a99c = paramWord(params, V90PARAMETERS_STUDY_PLAIN + 0x10);
		int_a9a0 = paramWord(params, V90PARAMETERS_STUDY_PLAIN + 0x14);

		short_a9a4 = 25;
		short_a9a6 = 50;
		altMinVarThresh = 4000.0f;
		neighborUcodeMinDistance = 2500;
		neighborUcodeMaxDistance = 6000;

		int_a984 = 0;
		int_a988 = 0;
		return;
	}

	inv = 1.0f / float_a950;

	int_a98c = paramWord(params, V90PARAMETERS_STUDY_QC + 0x00);
	int_a990 = paramWord(params, V90PARAMETERS_STUDY_QC + 0x04);
	int_a994 = paramWord(params, V90PARAMETERS_STUDY_QC + 0x08);
	int_a998 = paramWord(params, V90PARAMETERS_STUDY_QC + 0x0c);
	int_a99c = paramWord(params, V90PARAMETERS_STUDY_QC + 0x10);
	int_a9a0 = paramWord(params, V90PARAMETERS_STUDY_QC + 0x14);

	short_a9a4 = (short)(inv * 25.0f + 0.5f);

	/*
	 * Clamped BEFORE the store, not after it: the object compares the
	 * product while it is still in %st(0) at extended precision and stores
	 * once.  A local keeps the comparison on the same value.
	 */
	{
		float f = inv * 4444.4443f;

		if (f > 4000.0f)
			f = 4000.0f;
		altMinVarThresh = f;
	}

	short_a9a6 = (short)(inv * 50.0f + 0.5f);

	v = (short)(inv * 2777.7778f + 0.5f);
	if (v > 2500)
		v = 2500;
	neighborUcodeMinDistance = v;

	v = (short)(inv * 6666.667f + 0.5f);
	if (v > 6000)
		v = 6000;
	neighborUcodeMaxDistance = v;

	if (float_a950 != 0.0f) {
		short seed = (short)(ucodeLevel * inv + 0.5f);

		for (phase = 0; phase < V90ADID_PHASES; phase++) {
			linMapp[phase][ucode] = seed;
			linMappAlt[phase][ucode] = seed;
		}
	}

	edprintf("--------------------------------------------------------"
		 "-------\r\n");
	edprintf("V90AutoDigitalImpDetector::resetStudyUrefHandler QC report "
		 ":\r\n");
	edprintf("prevSession uinfo : %d %d %d %d %d %d\r\n",
		 linMapp[0][ucode], linMapp[1][ucode], linMapp[2][ucode],
		 linMapp[3][ucode], linMapp[4][ucode], linMapp[5][ucode]);
	edprintf("uniteUrefDistanceThresh = %d\r\n", short_a9a4);
	edprintf("altRbsDistanceThresh = %d\r\n", short_a9a6);
	edprintf("neighborUcodeMinDistance=%d neighborUcodeMaxDistance=%d\r\n",
		 neighborUcodeMinDistance, neighborUcodeMaxDistance);
	edprintf("altMinVarThresh = %c%d.%02d\r\n",
		 ADID_PRINT_SIGN(altMinVarThresh),
		 ADID_PRINT_WHOLE(altMinVarThresh),
		 ADID_PRINT_FRAC(altMinVarThresh, 100.0f));
	edprintf("--------------------------------------------------------"
		 "-------\r\n");

	int_a984 = 0;
	int_a988 = 0;
}

/*
 * Add one sample's magnitude to the phase's alternate-RBS accumulators.  The
 * count is incremented unconditionally, so a phase's mean is over every
 * sample offered to it and not over some accepted subset.
 */
void
V90AutoDigitalImpDetector::calculateLinearMeanAndVarAlt(short v,
							unsigned int phase)
{
	float_9d18[phase] += adid_abs(v);
	uint_9d30[phase]++;
}

void
V90AutoDigitalImpDetector::unitePhasesInfoOfUref(short at)
{
	short group[V90ADID_PHASES];
	short sum = 0;
	short prev;
	short i = 0;

	/*
	 * `bestValue` is deliberately left uninitialised, because the object
	 * leaves it uninitialised: the only path that reads it without having
	 * written it is D281's, and giving it a value here would be inventing
	 * behaviour the blob does not have rather than reproducing it.
	 *
	 * THE DECLARATION ORDER HERE IS A CONSTANT MAP, AND THE +1 IS ONE
	 * EXTRA `flds` OF THE NaN.  All 6! = 720 orderings of these six
	 * declarations were compiled on the period compiler and give **one**
	 * distinct emission -- GCC 3.4.2 lays this frame out by size and
	 * alignment, not by source order, so unlike `V92Transmitter::process`
	 * (7847) there is nothing to recover here.  What the 280 differing
	 * bytes of 748 actually are: a systematic `%ebx`/`%esi` exchange
	 * through the whole body, plus one instruction.  The blob loads three
	 * x87 constants in its prologue (NaN, 1.0, 0.5) and we load FOUR --
	 * the NaN twice, from two identical `.rodata.cst4` slots.  The blob
	 * spills `bestVar` to `0xc(%esp)` with `fstps` and reloads it after
	 * the loop; we keep it on the x87 stack and pay a second
	 * materialisation of the constant for it.  That is x87 stack
	 * allocation, and no spelling of this declaration reaches it.
	 * Finding F7848.
	 */
	float bestVar = __builtin_nanf("");
	float bestValue;

	do {
		short best = 0;
		short p;

		prev = sum;

		for (p = 0; p < V90ADID_PHASES; p++)
			group[p] = ADID_NO_GROUP;

		/*
		 * i stops at 4, not 5: the inner loops only ever look
		 * FORWARD, so the last phase can never start a group.  The
		 * value it is left with is what the checksum below reads.
		 */
		for (i = 0; i < V90ADID_PHASES - 1; i++) {
			short total;
			short count;
			short j;
			float fsum, fsq;

			if (group[i] != ADID_NO_GROUP || short_2800[i] != 0)
				continue;

			/*
			 * THE POOLED COUNT IS A `short`, and the count it is
			 * pooled from is an `unsigned int`.  The object loads
			 * it with `movswl`, which is the low half of the
			 * 32-bit field sign-extended -- so a phase with more
			 * than 32,767 samples in a cell pools as a negative
			 * number.  That is the object's arithmetic.
			 */
			total = (short)uint_1c00[i][at];
			fsum = float_1000[i][at];
			fsq = float_9118[i][at];

			for (j = (short)(i + 1); j < V90ADID_PHASES; j++) {
				if (group[j] != ADID_NO_GROUP
				    || short_2800[j] != 0)
					continue;
				if (adid_abs(linMapp[i][at] - linMapp[j][at])
				    >= short_a9a4)
					continue;

				group[j] = i;
				fsum += float_1000[j][at];
				fsq += float_9118[j][at];
				total = (short)(total + uint_1c00[j][at]);
			}

			/*
			 * An empty group leaves both tables alone -- not even
			 * a store of zero -- so its members keep whatever
			 * `resetLinearMapping` gave them.
			 */
			if (total != 0) {
				float inv = 1.0f / total;
				float mean = fsum * inv;

				linMapp[i][at] = (short)(mean + 0.5f);
				float_9d48[i][at] = fsq * inv - mean * mean;
			}

			group[i] = i;
			count = 1;

			for (j = (short)(i + 1); j < V90ADID_PHASES; j++) {
				if (group[j] != i)
					continue;
				linMapp[j][at] = linMapp[i][at];
				float_9d48[j][at] = float_9d48[i][at];
				count = (short)(count + 1);
			}

			if (count > best) {
				best = count;
				bestValue = linMapp[i][at];
				bestVar = float_9d48[i][at];
			}
		}

		/*
		 * The convergence checksum.  `i` is 5 here and it is the
		 * index -- D280.
		 */
		sum = 0;
		for (p = 0; p < V90ADID_PHASES; p++)
			sum = (short)(sum + linMapp[i][at]);
	} while (sum != prev);

	{
		short v = (short)bestValue;
		short p;

		for (p = 0; p < V90ADID_PHASES; p++)
			if (short_2800[p] != 0) {
				linMapp[p][at] = v;
				float_9d48[p][at] = bestVar;
			}
	}
}

/*
 * Turn one cell's accumulators into a mean and a variance.
 *
 * The variance is E[x^2] - E[x]^2, and reading that out of the object needs
 * finding F245: the `de e9` here prints as `fsubrp` and IS `FSUBP`, so the
 * subtraction is (mean of squares) - (square of mean) and not the other way
 * round.  A negative variance would be the tell if it were reversed.
 *
 * Nothing happens when the count is zero -- not even a store of zero -- so a
 * cell that never saw a sample keeps whatever `linMapp` entry
 * `resetLinearMapping` gave it.
 */
void
V90AutoDigitalImpDetector::updateLinMappMeanAndVar(short phase, short code)
{
	float inv, mean;

	if (uint_1c00[phase][code] == 0)
		return;

	inv = 1.0f / uint_1c00[phase][code];
	mean = float_1000[phase][code] * inv;

	float_9d48[phase][code] = float_9118[phase][code] * inv - mean * mean;
	linMapp[phase][code] = (short)(mean + 0.5f);
}

/*
 * Turn each phase's accumulators for the reference code into a mean and a
 * variance, unite the phases, and then clear the accumulators.
 *
 * The three steps are in that order and the last one is unconditional, so a
 * cell with no samples keeps its mapping entry and loses nothing, and every
 * cell starts the next study empty.
 *
 * `ucode` IS RE-READ AFTER THE CALL.  The object loads it once at the head,
 * keeps a copy on the stack for the call's argument, and then loads it AGAIN
 * from +0xa96b at 0x410d2 for the clearing loop -- so `unitePhasesInfoOfUref`
 * is allowed to change it and the clear follows the new value.  It does not
 * change it today; the reload is reproduced because it is what the object
 * does, not because the difference is reachable.
 */
void
V90AutoDigitalImpDetector::updateUref()
{
	short phase;

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		if (uint_1c00[phase][ucode] != 0) {
			/*
			 * THE TWO EXTRA INSTRUCTIONS HERE ARE `fxch`, NOT A
			 * STATEMENT, so lever 2's reading of this symbol's
			 * +2 is wrong and the loop body is not short of
			 * anything.  Instructions 25 and 26 -- `fld %st(1)`
			 * and `fmul %st(2),%st` -- are the blob's exactly;
			 * from there the blob subtracts, stores the variance
			 * and only then forms `mean + 0.5f`, while ours
			 * computes the rounding first and pays two `fxch` to
			 * the x87 stack shuffler for it.  The formula itself
			 * is confirmed by the object (`fmuls 0x9118` then
			 * `fld mean; fmul` then the subtract), so it is the
			 * SCHEDULE that differs.  Twelve spellings were
			 * compiled on the period compiler -- the two
			 * statements in both orders, a variance temp, a
			 * rounded temp, both, `mean * mean` in a temp,
			 * `0.5f + mean`, a named `half`, both multiply
			 * operand orders and the negated form -- giving THREE
			 * distinct emissions and NO cell at byte identity.
			 * Swapping the two statements alone takes the
			 * instruction count to the blob's exactly and the
			 * bytes only from 81 to 56, which is hill-climbing
			 * and is declined under 7782; it also emits the two
			 * stores in the opposite order to the object's.
			 * `nm -n` puts this file 34 of 34 in the blob's own
			 * emission order already, so lever 3 has nothing
			 * positional to offer either.  Finding F7846.
			 */
			float inv = 1.0f / uint_1c00[phase][ucode];
			float mean = float_1000[phase][ucode] * inv;

			float_9d48[phase][ucode] =
			    float_9118[phase][ucode] * inv - mean * mean;
			linMapp[phase][ucode] = (short)(mean + 0.5f);
		}
	}

	unitePhasesInfoOfUref(ucode);

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		float_1000[phase][ucode] = 0.0f;
		uint_1c00[phase][ucode] = 0;
		float_9118[phase][ucode] = 0.0f;
	}
}

/*
 * The alternate-RBS pair: the phase's own sum over the phase's own count,
 * rounded into `linMappAlt`.  No variance -- the class keeps no per-phase
 * sum of squares to form one from.
 */
void
V90AutoDigitalImpDetector::updateLinMappMeanAndVarAlt(short phase, short code)
{
	if (uint_9d30[phase] == 0)
		return;

	linMappAlt[phase][code] =
	    (short)(float_9d18[phase] / uint_9d30[phase] + 0.5f);
}

/*
 * Round each phase's alternate-RBS mean into the reference code's column of
 * `linMappAlt`, then clear the accumulators.
 *
 * THE CLEARING IS UNCONDITIONAL and the update is not: a phase that is not
 * flagged, or that has no samples, still has its sum and count reset.  So
 * this is a per-block boundary and not a per-block update, and a phase that
 * misses the flag loses its samples rather than carrying them forward.
 *
 * The mean here is `sum * (1.0f / count)` where `updateLinMappMeanAndVarAlt`
 * writes `sum / count`; both are in the object and they are not the same
 * arithmetic.
 */
void
V90AutoDigitalImpDetector::updateUrefAlt()
{
	short phase;

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		if (short_2800[phase] != 0 && uint_9d30[phase] != 0)
			linMappAlt[phase][ucode] =
			    (short)(float_9d18[phase]
				    * (1.0f / uint_9d30[phase]) + 0.5f);

		float_9d18[phase] = 0.0f;
		uint_9d30[phase] = 0;
	}
}

/*
 * The first study: turn the middle of each phase's accumulators into a
 * mapping, decide which phases are too rough to trust, and empty everything.
 *
 * THE MEAN LOOP ONLY COVERS CODES 0x40..0x4f.  Sixteen codes out of the 128,
 * and the reference code is skipped inside them -- so this is a study of the
 * middle of the companding law and not of the whole of it.  The rough-phase
 * test that follows runs over the fifteen adjacent differences inside the
 * same window.
 *
 * A PHASE IS SUSPECTED UNLESS MORE THAN NINE OF ITS FIFTEEN NEIGHBOURING
 * DIFFERENCES ARE LARGE.  The counter starts at 1 rather than 0, so "more
 * than nine" needs nine differences over the threshold and not ten; a phase
 * already flagged at +0x2800 skips the count entirely and is suspected on
 * that alone.  Every suspected phase also bumps the tally at +0xa96a.
 *
 * THE CLEARING PASS SPARES ONE ENTRY.  `linMapp[phase][ucode]` is the only
 * mapping entry not zeroed, because the loop is split around `ucode` and the
 * middle step clears the three accumulators without touching the mapping --
 * so the seed `resetLinearMapping` put there survives the study.  `ucode` is
 * a byte and the row is 128 wide, and the object masks it nowhere: a
 * reference code of 128 or more walks the split into the next phase's row.
 */
void
V90AutoDigitalImpDetector::porcessFirstStudy()
{
	float sqrDiffThresh;
	float t;
	unsigned char phase;
	unsigned char code;

	/*
	 * 2.5 sigma, clamped between the two neighbour distances.  Both
	 * comparisons are the object's `fcom`/`jae` and keep the RIGHT operand
	 * when the compare is unordered, which is what the ternaries spell.
	 */
	t = trn1Sigma * 2.5f;
	t = (t >= (float)neighborUcodeMinDistance)
		? t : (float)neighborUcodeMinDistance;
	sqrDiffThresh = ((float)neighborUcodeMaxDistance >= t)
		? t : (float)neighborUcodeMaxDistance;

	edprintf("V90AutoDigitalImpDetector::porcessFirstStudy()  "
		 "2.5*trn1Sigma=%d , sqrDiffThresh=%d\n",
		 (short)(trn1Sigma * 2.5f + 0.5), (short)(sqrDiffThresh + 0.5));

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		for (code = 0x40; code <= 0x4f; code++) {
			float inv, mean;

			if (ucode == code)
				continue;
			if (uint_1c00[phase][code] == 0)
				continue;

			inv = 1.0f / uint_1c00[phase][code];
			mean = float_1000[phase][code] * inv;

			float_9d48[phase][code] =
			    inv * float_9118[phase][code] - mean * mean;
			linMapp[phase][code] = (short)(mean + 0.5f);
		}
	}

	byte_a96a = 0;

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		short n = 1;

		if (short_2800[phase] == 0) {
			for (code = 0x40; code <= 0x4e; code++) {
				float d = (float)(linMapp[phase][code + 1]
						  - linMapp[phase][code]);

				if (d * d > sqrDiffThresh)
					n = (short)(n + 1);
			}

			if (n > 9) {
				byte_280c[phase] = 0;
				continue;
			}
		}

		byte_a96a++;
		byte_280c[phase] = 1;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90AutoDigitalImpDetector: suspected rbs "
				     "patern  %d%d%d%d%d%d\r\n", byte_280c[0],
				     byte_280c[1], byte_280c[2], byte_280c[3],
				     byte_280c[4], byte_280c[5]);

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		unsigned char k;

		for (k = 0; k < ucode; k++) {
			float_1000[phase][k] = 0.0f;
			uint_1c00[phase][k] = 0;
			float_9118[phase][k] = 0.0f;
			linMapp[phase][k] = 0;
		}

		uint_1c00[phase][ucode] = 0;
		float_1000[phase][ucode] = 0.0f;
		float_9118[phase][ucode] = 0.0f;

		/*
		 * The counter is a byte and the object tests its SIGN bit, so
		 * this stops at 127 and a `ucode` of 0xff wraps the start to 0
		 * and clears the whole row.  Both are what `unsigned char k =
		 * ucode + 1; k < 128` does in C.
		 */
		for (k = (unsigned char)(ucode + 1); k < V90ADID_CODES; k++) {
			uint_1c00[phase][k] = 0;
			float_1000[phase][k] = 0.0f;
			float_9118[phase][k] = 0.0f;
			linMapp[phase][k] = 0;
		}
	}
}

/*
 * Pool the unsuspected phases' accumulators for one code and give every
 * unsuspected phase the pooled answer.
 *
 * The shape is `unitePhasesInfoOfUref`'s -- group the phases that are not
 * flagged, pick the biggest group, average it -- with three differences that
 * are the object's and are each written down:
 *
 * THE FLAG IS +0x280c, NOT +0x2800.  `unitePhasesInfoOfUref` groups on the
 * per-phase short at +0x2800; this groups on the byte at +0x280c, which is
 * what `porcessFirstStudy` writes.  So the two methods answer to different
 * flags and the pair is not a copy.
 *
 * THE MERGE TEST IS AGAINST A VARIANCE, NOT A CONSTANT DISTANCE.  Two phases
 * join when the square of the difference between their entries is less than
 * a quarter of the LEADER's variance, so the tolerance is per phase and per
 * code rather than the single `short_a9a4` the other method uses.  An
 * unordered compare MERGES -- `jae` skips and CF is set by an unordered
 * `fcompp` -- so a NaN variance at +0x9d48 pools everything behind it, which
 * `updateLinMappMeanAndVar` can produce and a seeded object reaches directly.
 *
 * AND THE TOLERANCE IS FORMED BEFORE THE DISTANCE, which is the only reason
 * that last sentence is true.  `fcompp` compares %st(0) against %st(1), and
 * on an unordered compare CF is set whichever way round they are -- so which
 * of the two is on top decides whether a NaN merges or skips, and it is
 * decided by which subexpression the compiler pushed LAST.  The object at
 * 0x41739 is
 *
 *	flds 0x9d48(%edi,%eax,4) / fmul %st(1),%st	the tolerance
 *	push %eax / fildl (%esp) / fmul %st(0),%st	the squared distance
 *	fcompp / fnstsw %ax / sahf / jae		skip
 *
 * -- tolerance first, distance second, so the DISTANCE is %st(0) and `jae`
 * means "skip when d*d is ordered-not-below", leaving the unordered case to
 * fall through and merge.  Writing the tolerance inline on the right of the
 * `>=` reverses that: GCC evaluates the comparison's left operand first, so
 * `d*d` is pushed first, and it then emits two `fxch` and `jbe` -- which
 * SKIPS on an unordered compare, the opposite answer for a NaN variance.
 * Hoisting it into `lim` above the distance is what puts the object's operand
 * in %st(0); the resulting thirteen instructions are the object's, register
 * allocation included.  The `>=` itself is unchanged and so is its meaning
 * under IEEE rules, so this is an evaluation-order fix and the modern build
 * does not see it.  Finding F1990 for why the flag exposes it at all.
 *
 * THERE IS NO CONVERGENCE LOOP.  One pass, not `unitePhasesInfoOfUref`'s
 * do/while.
 *
 * `count` IS NEVER RESET PER GROUP (D283) and `bestGroup` is read
 * uninitialised when no group is ever formed (D284); both are below.
 */
void
V90AutoDigitalImpDetector::uniteLinMappInfoOfUnsuspectedPhases(unsigned char at)
{
	short group[V90ADID_PHASES];
	short best = 0;
	short total;
	float fsum, fsq, inv, mean, var;
	short r;
	unsigned char i, j;

	/*
	 * THE RUNNING SIZE IS SET ONCE, OUTSIDE THE LOOP THAT USES IT.  The
	 * object stores 1 into it at 0x4159a, before the scan begins, and the
	 * only other write is the increment inside the inner loop -- so a
	 * second group starts counting from wherever the first one finished
	 * and "the biggest group" is really "the last group that merged
	 * anything".  docs/deviations.md D283.
	 */
	short count = 1;

	/*
	 * Left uninitialised because the object leaves it uninitialised: the
	 * only path that reads it unwritten needs all five of phases 0..4
	 * flagged at +0x280c, and there the object's answer is not a function
	 * of its inputs at all.  docs/deviations.md D284, and the test keeps
	 * off it for D281's reason.
	 */
	short bestGroup;

	for (i = 0; i < V90ADID_PHASES; i++)
		group[i] = ADID_NO_GROUP;

	/*
	 * i stops at 4: the inner loop only ever looks forward, so the last
	 * phase can never lead a group.
	 */
	for (i = 0; i < V90ADID_PHASES - 1; i++) {
		if (byte_280c[i] != 0 || group[i] != ADID_NO_GROUP)
			continue;

		group[i] = i;

		for (j = (unsigned char)(i + 1); j < V90ADID_PHASES; j++) {
			float lim;
			float d;

			if (byte_280c[j] != 0 || group[j] != ADID_NO_GROUP)
				continue;

			/*
			 * The tolerance is formed FIRST so that the squared
			 * distance is the one in %st(0) at the `fcompp`, which
			 * is what makes the object's `jae` merge an unordered
			 * compare rather than skip it.  Head comment.
			 */
			lim = float_9d48[i][at] * 0.25f;
			d = (float)(linMapp[i][at] - linMapp[j][at]);

			if (d * d >= lim)
				continue;

			group[j] = i;
			count = (short)(count + 1);
		}

		if (count > best) {
			best = count;
			bestGroup = i;
		}
	}

	total = 0;
	fsum = 0.0f;
	fsq = 0.0f;

	for (j = 0; j < V90ADID_PHASES; j++) {
		if (byte_280c[j] != 0 || group[j] != bestGroup)
			continue;

		fsum += float_1000[j][at];
		fsq += float_9118[j][at];

		/*
		 * The pooled count is a `short` and the counts it pools are
		 * `unsigned int`: the object truncates with `cwtl` after every
		 * add, the same as `unitePhasesInfoOfUref`.
		 */
		total = (short)(total + uint_1c00[j][at]);
	}

	/*
	 * AN EMPTY POOL LEAVES EVERYTHING ALONE -- not the mapping, not the
	 * variance, and NOT the accumulators, which every other exit clears.
	 * The object returns straight out at 0x41618 with two values still on
	 * the FPU stack and nothing written.
	 */
	if (total == 0)
		return;

	inv = 1.0f / total;
	mean = fsum * inv;
	var = fsq * inv - mean * mean;
	r = (short)(mean + 0.5f);

	/*
	 * EVERY unsuspected phase gets the answer, not just the members of the
	 * chosen group -- there is no `group[j] == bestGroup` test here, only
	 * the flag.  The clearing below it is unconditional and covers the
	 * suspected phases too.
	 */
	for (j = 0; j < V90ADID_PHASES; j++) {
		if (byte_280c[j] == 0) {
			linMapp[j][at] = r;
			float_9d48[j][at] = var;
		}

		float_1000[j][at] = 0.0f;
		uint_1c00[j][at] = 0;
		float_9118[j][at] = 0.0f;
	}
}

/*
 * ==========================================================================
 * `unitePhasesInfoOfUref`, and the `updateUref` that was waiting on it.
 *
 * WHAT IT IS FOR.  Six RBS phases each measure the same reference code, and
 * some of them are the same phase as far as the line is concerned -- the same
 * digital impairment, the same level.  This method finds those groups: it
 * takes the phases NOT flagged at +0x2800, merges any two whose `linMapp`
 * entries for the reference code differ by less than the threshold at
 * +0xa9a4, recomputes each group's mean and variance from the group's pooled
 * accumulators, and writes the pooled answer back to every member.  Then it
 * takes the LARGEST group's answer and gives it to every phase that IS
 * flagged -- the ones whose own measurement is not trusted.
 *
 * IT REPEATS UNTIL IT STOPS CHANGING.  The whole of the above sits inside a
 * do/while whose test is a checksum of `linMapp`, because merging rewrites
 * the entries the next round's merging decisions are made from.
 *
 * THE CHECKSUM IS NOT WHAT IT LOOKS LIKE -- see D280.  It adds up SIX COPIES
 * OF ONE ENTRY, `linMapp[5][at]`, because the index is the outer loop's
 * variable left at its terminal value rather than the inner loop's.  That is
 * visible in the object as a load hoisted clean out of the loop, which is
 * only possible if the address does not vary.  It is written that way here.
 *
 * THE SENTINEL IS A NaN.  The best-group variance starts as the constant at
 * `.rodata.cst4+0x328`, which is 0x7fc00000.  It is only observable if no
 * group is ever formed, which needs all five of phases 0..4 flagged -- and in
 * that case the paired `bestValue` is read uninitialised, so the object's
 * behaviour there is not a function of its inputs at all.  D281.
 * ==========================================================================
 */


/*
 * The entry of `linMapp[phase]` nearest in magnitude to the value offered.
 *
 * THE SEARCH IS OVER CODES 5 TO 116 INCLUSIVE, not over the whole row: the
 * counter starts at 5 and the loop test is an unsigned `cmp $0x74; jbe`.  The
 * ends of the row are excluded, which is what "unsuspected" is about -- the
 * smallest and largest codes are the ones a digital impairment reaches first.
 *
 * The initial best distance is 1,000,000, which no distance between two
 * shorts can reach, so the first iteration always wins and the returned entry
 * is never the value of an uninitialised index.
 */
short
V90AutoDigitalImpDetector::unSuspectedPhaseNearestLinMapp(short v, short phase)
{
	int mag = adid_abs(v);
	int best = 1000000;
	unsigned short at = 5;
	unsigned short ci;

	for (ci = 5; ci <= 0x74; ci++) {
		int d = adid_abs(mag - linMapp[phase][ci]);

		if (d < best) {
			at = ci;
			best = d;
		}
	}

	return linMapp[phase][at];
}

/*
 * Rebuild the suspected phases' mapping from the samples they actually
 * received, using the unsuspected phase's mapping as the reference.
 *
 * FOR EACH SUSPECTED PHASE, AND EACH CODE IN A FIXED SCAN ORDER: quantise
 * every sample this phase stored for that code to the nearest entry of the
 * reference phase's mapping, find the most popular value among them, and give
 * the phase the reference's entry for the mapping and the popular value for
 * the alternate mapping.
 *
 * THE SCAN ORDER IS A TABLE AND NOT A RANGE.  115 codes: the odd ones
 * descending from 63 to 3, then 2, then the even ones ascending from 4 to 64,
 * then 65 to 116 in order.  Codes 0 and 1 are not in it and neither is
 * anything above 116.  The object holds it as an automatic array `memcpy`d
 * from `.rodata+0xcfc`, which is what a local array with an initialiser
 * compiles to, and it is written that way here.
 *
 * THE SAMPLE-STORE CURSOR IS UNBOUNDED, and it is a different unbounded index
 * from D256's: `base` walks forward by the histogram count of every code it
 * visits, and nothing compares it against 0x83e.  The store is laid out as
 * "phase p's samples for the codes in scan order, end to end", so the cursor
 * only stays inside the row while the histogram agrees with what
 * `addReceivedSampleToStorage` actually stored.  docs/deviations.md D287.
 *
 * THE POPULAR VALUE IS FOUND DESTRUCTIVELY.  Each run of equal samples is
 * counted and then overwritten with -1 so that the next pass does not count it
 * again, which is why -1 is skipped on the way in and why the sample store is
 * left full of them.  A sample equal to the reference entry is skipped too, so
 * "popular" means "popular among the samples that disagree with the
 * reference".
 */
void
V90AutoDigitalImpDetector::updateAltRbsPhaseInDil()
{
	/*
	 * 115 bytes, `memcpy`d from `.rodata+0xcfc` at 0x4186d.  Not `static
	 * const`: the object copies it to the stack on every call, which is
	 * what an automatic array with an initialiser does.
	 */
	unsigned char order[115] = {
		63, 61, 59, 57, 55, 53, 51, 49, 47, 45, 43, 41, 39, 37, 35, 33,
		31, 29, 27, 25, 23, 21, 19, 17, 15, 13, 11,  9,  7,  5,  3,  2,
		 4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34,
		36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 65,
		66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
		82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97,
		98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110,
		111, 112, 113, 114, 115, 116
	};
	unsigned short phase;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("--------------------------------------"
				     "----------------------------\n");

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		unsigned short i;
		int base = 0;

		/* Only the phases flagged at +0x2800, and they print too. */
		if (short_2800[phase] == 0)
			continue;

		for (i = 0; i < 115; i++) {
			unsigned char at = order[i];
			unsigned short maxCount = 0;
			short maxValue = 0;
			unsigned short m;
			unsigned short j;

			/*
			 * Quantise this code's samples onto the reference
			 * phase's mapping.  The object inlines
			 * `unSuspectedPhaseNearestLinMapp` here -- the same
			 * 1,000,000 sentinel and the same 5..0x74 window -- so
			 * calling it is a factoring difference and not a
			 * behavioural one.
			 */
			for (j = 0; j < short_8b00[phase][at]; j++)
				sampleStore[phase][base + j] =
				    unSuspectedPhaseNearestLinMapp(
					    sampleStore[phase][base + j],
					    unSuspectedPhase);

			for (m = 0; m < short_8b00[phase][at] - 1; m++) {
				short v = sampleStore[phase][base + m];
				unsigned short count;
				unsigned short q;

				if (v == linMapp[unSuspectedPhase][at])
					continue;
				if (v == -1)
					continue;

				count = 1;

				for (q = (unsigned short)(m + 1);
				     q < short_8b00[phase][at]; q++) {
					short w = sampleStore[phase][base + q];

					if (w != v)
						continue;
					/*
					 * Both of these are already known
					 * false -- `w` equals `v` and `v`
					 * passed the same two tests -- and the
					 * object tests them anyway, which is
					 * why they are here.
					 */
					if (w == linMapp[unSuspectedPhase][at])
						continue;
					if (w == -1)
						continue;

					sampleStore[phase][base + q] = -1;
					count = (unsigned short)(count + 1);
				}

				/* An UNSIGNED sixteen-bit compare. */
				if (count > maxCount) {
					maxCount = count;
					maxValue = sampleStore[phase][base + m];
				}
			}

			if (short_8b00[phase][at] != 0) {
				linMapp[phase][at] =
				    linMapp[unSuspectedPhase][at];
				linMappAlt[phase][at] = maxCount != 0
				    ? maxValue : linMapp[unSuspectedPhase][at];
				base += short_8b00[phase][at];
			}
		}

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("\n----  AltPhase %d : "
					     "linearMapping  linearMappingAlt"
					     "  ----\n", phase);

		{
			unsigned short ci;

			for (ci = 0; ci < V90ADID_CODES; ci++)
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "AltUcode[%d]  :  %d  %d\n", ci,
					    linMapp[phase][ci],
					    linMappAlt[phase][ci]);
		}
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("--------------------------------------"
				     "----------------------------\n");
}

/*
 * ==========================================================================
 * The study methods -- the four that set the thresholds up, run the first
 * study over them, and pool what it found.
 *
 * ALL FOUR TALK, AND THAT IS HALF OF WHY THEY ARE WORTH HAVING.  Between them
 * they make twelve `edprintf` calls and one `dsplibs_debug_printf`, and the
 * format strings are the original author's names for six fields this batch
 * would otherwise have had to call `short_a9ac` and `float_a9a8`.
 * `trn1Sigma`, `neighborUcodeMinDistance`, `neighborUcodeMaxDistance`,
 * `altMinVarThresh`, `uniteUrefDistanceThresh` and `altRbsDistanceThresh` are
 * all read straight off the wire this way.
 *
 * A FLOAT IS PRINTED AS `%c%d.%02d`, never with `%f`.  The object has no
 * floating-point formatting at all: every one of these sites takes the sign,
 * the truncated magnitude and a hundredths digit as three integers.  The
 * three macros below are that idiom, and each is written to the branch the
 * object actually encodes rather than to the one that reads naturally --
 * see ADID_PRINT_SIGN.
 * ==========================================================================
 */




/*
 * The second study: repair each suspected phase's mapping entry from the
 * unsuspected phase's, one code at a time, where the repair is unambiguous.
 *
 * FOR EACH CODE AND EACH SUSPECTED PHASE, three entries of the reference
 * phase's mapping are considered -- the code itself and its two neighbours, or
 * the code and the two BELOW it, depending on which side of the reference this
 * phase's own entry falls -- and the nearest of the three is taken.  It is
 * taken only if it is unambiguously the nearest: either it is exact, or the
 * second nearest is more than twice as far away.
 *
 * THE SECOND-NEAREST DISTANCE IS INITIALISED ONCE FOR THE WHOLE CALL, and the
 * nearest is initialised per (code, phase).  So the ratio test compares this
 * code's best against the best runner-up seen since the method started, not
 * against this code's runner-up, and it gets harder to satisfy as the call
 * proceeds.  The object loads the sentinel with `flds` at 0x41cb0, before the
 * prologue's pushes, and reuses that x87 slot for the running value -- which
 * it could only do if the value were dead after one use.  docs/deviations.md
 * D286.
 *
 * THE SENTINEL IS A NaN, and that is what makes the first comparison against
 * it succeed: the update is guarded by a `jae` that an unordered compare does
 * not take, so the first distance that is not an improvement becomes the
 * second-nearest whatever its size.  Written as the negation of the ordered
 * test, which is what the branch encodes -- `!(d >= second)`, not `d < second`.
 *
 * THE NEIGHBOUR WINDOW REACHES BEFORE THE TABLE.  At codes 0 and 1 the "two
 * below" case indexes `linMapp[unSuspectedPhase][-2]`, which for an
 * unsuspected phase of 0 is four bytes in front of the object.  The object
 * does not guard it and neither does this.  docs/deviations.md D287.
 */
void
V90AutoDigitalImpDetector::porcessSecondStudy()
{
	/*
	 * Both of these are initialised once, before the scan, and the object
	 * initialises them there: the NaN at 0x41cb0 and the zero at 0x41cc5.
	 * A stale `bestAt` is reachable -- three distances of 32,256 or more
	 * leave the nearest at its sentinel -- and it is a deterministic zero
	 * rather than an uninitialised read.
	 */
	float second = __builtin_nanf("");
	short bestAt = 0;
	unsigned char at;

	for (unSuspectedPhase = 0;
	     byte_280c[unSuspectedPhase] != 0 && unSuspectedPhase <= 4;
	     unSuspectedPhase++)
		;

	/*
	 * D285: the scan above cannot leave a value above 5, so this arm is
	 * unreachable.  It is the object's and it is written.
	 */
	if (unSuspectedPhase > 5) {
		unSuspectedPhase = 0;
	} else {
		for (at = 0; at <= 0x74; at++) {
			unsigned char phase;

			for (phase = 0; phase < V90ADID_PHASES; phase++) {
				float nearest = 32256.0f;
				short from;
				short v;
				unsigned char k;

				if (byte_280c[phase] == 0)
					continue;
				if (short_2800[phase] != 0)
					continue;

				from = linMapp[phase][at]
				       > linMapp[unSuspectedPhase][at]
				    ? (short)at : (short)(at - 2);
				v = linMapp[phase][at];

				for (k = 0; k <= 2; k++) {
					float d = (float)adid_abs(v
					    - linMapp[unSuspectedPhase][from + k]);

					if (!(d >= nearest)) {
						second = nearest;
						nearest = d;
						bestAt = (short)(from + k);
					} else if (!(d >= second)) {
						second = d;
					}
				}

				if (nearest == 0.0f
				    || (1.0f / nearest) * second > 2.0f)
					linMapp[phase][at] =
					    linMapp[unSuspectedPhase][bestAt];
			}
		}
	}

	edprintf("V90AutoDigitalImpDetector: unSuspectedPhase = %d\r\n",
		 unSuspectedPhase);

	updateAltRbsPhaseInDil();

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("------------------------------------"
				     "------------------\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("-----------------  Linear Mapping "
				     "report --------------\r\n");

	{
		/*
		 * A BYTE, and the loop ends on its SIGN BIT -- so this covers
		 * the whole 128-wide row where `setQcLinearMapping`'s
		 * otherwise identical loop stops at 0x74.
		 */
		unsigned char ci;

		for (ci = 0; ci < V90ADID_CODES; ci++)
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("linearMapping[%d]  :  "
						     "%d  %d  %d  %d  %d  "
						     "%d\r\n", ci,
						     linMapp[0][ci],
						     linMapp[1][ci],
						     linMapp[2][ci],
						     linMapp[3][ci],
						     linMapp[4][ci],
						     linMapp[5][ci]);
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("------------------------------------"
				     "------------------\r\n");
}

/*
 * File one received sample under its phase, and count its code.
 *
 * THE STORE INDEX IS UNBOUNDED.  `int_9100[phase]` is incremented once per
 * call and nothing in these 149 bytes compares it against 0x83e, so a caller
 * that offers more than 2,110 samples for one phase writes past that phase's
 * row and eventually past the object.  That is the object's behaviour and it
 * is reproduced; docs/deviations.md D256 records it and the test keeps the
 * count inside the row rather than pretending the check exists.
 *
 * The +0.5f is the rounding, as everywhere else here; the magnitude is taken
 * AFTER the conversion, so a sample of -0.4 stores 0 and one of -0.6 stores
 * 0 as well -- (short)(-0.6 + 0.5) truncates toward zero.
 */
void
V90AutoDigitalImpDetector::addReceivedSampleToStorage(short phase,
						      unsigned char code,
						      float v)
{
	int n = int_9100[phase];
	short q = (short)(v + 0.5f);

	int_9100[phase] = n + 1;
	sampleStore[phase][n] = (short)adid_abs(q);
	short_8b00[phase][code]++;
}

/*
 * Add one sample to the cumulative registers of the cell its LINEAR level
 * companded to.
 *
 * The first argument is the magnitude accumulated; the second is the linear
 * level whose PCM code selects the cell.  They are separate arguments and the
 * object treats them separately -- the second goes through `linear2alaw` or
 * `linear2ulaw` and never reaches the accumulator.
 *
 * THE COMPANDED CODE IS INVERTED BACK TO A MAGNITUDE, with the same masks
 * `reset` uses in the other direction: `^ 0xd5` for A-law, and for mu-law
 * `0xff - code`, which is `^ 0xff` on a byte.  The result indexes a 128-wide
 * row and can be up to 255, so a code in the upper half indexes into the next
 * phase's row.  The object does not mask it; neither does this.
 */
void
V90AutoDigitalImpDetector::calculateLinearMeanAndVar(short v, short level,
						     unsigned int phase)
{
	short at;
	int mag;

	if (pcmType != PCM_TYPE_MU_LAW)
		at = (unsigned char)(linear2alaw(adid_abs(level)) ^ 0xd5);
	else
		at = (short)(0xff
			     - (unsigned char)linear2ulaw(adid_abs(level)));

	mag = adid_abs(v);

	float_1000[phase][at] += mag;
	float_9118[phase][at] += mag * mag;
	uint_1c00[phase][at]++;
}

/*
 * The re-test that closes the second and third updates: a phase that was
 * flagged as carrying alternate RBS keeps the flag only if its alternate level
 * is STILL further from its plain level than `short_a9a6` allows.
 *
 * The object writes this block out twice, at 0x42867 and 0x42b79, with
 * `isAltRbs` inlined and its leading "is the phase flagged" test dropped --
 * the caller has just tested it.  Calling the method reinstates a test whose
 * answer is already known, which is why this is one function here and two
 * copies there.
 */
static void
adid_recheckAltRbs(V90AutoDigitalImpDetector *o)
{
	short phase;

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		if (o->short_2800[phase] == 0)
			continue;

		o->short_2800[phase] = (short)o->isAltRbs(phase, o->ucode,
		    (float)o->linMappAlt[phase][o->ucode]);

		if (o->short_2800[phase] == 0)
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("V90AutoDigitalImpDetector"
						     ": alternate rbs false "
						     "detection on phase %d "
						     "!!!\n", phase);
	}
}

/*
 * ==========================================================================
 * THE DIL BATCH: `updateAltRbsPhaseInDil` and the two members that call it.
 *
 * WHAT THE THREE ARE FOR.  By the time these run, `porcessFirstStudy` has
 * decided which of the six RBS phases are too rough to trust and written that
 * verdict into `byte_280c`.  These three take the FIRST phase that is clear --
 * `unSuspectedPhase` at +0xa968, which both callers scan for with the same
 * loop -- and use its mapping as the reference every other phase is repaired
 * against.  `updateAltRbsPhaseInDil` does the repair; `setQcLinearMapping`
 * builds the mapping to repair from the accumulators (or, for a phase with no
 * verdict, from the previous session's), and `porcessSecondStudy` tries to
 * repair the suspected phases' entries one code at a time first.
 *
 * THE TWO CALLERS SHARE A TAIL, AND IT IS NOT QUITE THE SAME TAIL.  Both scan
 * for `unSuspectedPhase`, call `updateAltRbsPhaseInDil`, and then print the
 * whole mapping table under the same three format strings -- but
 * `porcessSecondStudy` prints codes 0..0x7f and `setQcLinearMapping` prints
 * 0..0x74, and the counters are not even the same type: a byte whose sign bit
 * ends the loop in the first, a `short` compared against 0x74 in the second.
 * The two are written out rather than factored into a helper because that
 * difference is the only thing separating them and a helper would hide it.
 * ==========================================================================
 */

int
V90AutoDigitalImpDetector::studyUrefHandler(float v, unsigned int phase)
{
	/*
	 * Formed once, before the dispatch, and truncating: the object's
	 * `fists 0x46(%esp)` at 0x4217f sits between the control-word switch it
	 * sets up for the whole function and the switch on the state.  The
	 * float itself stays live -- only `isAltRbs` uses it, and only in three
	 * of the seven arms.
	 */
	short sample = (short)v;
	int ret = 1;

	switch (int_a984) {
	case 0: {
		/*
		 * The initial pattern.  Accumulate until +0xa98c samples have
		 * arrived, then form each phase's variance for the reference
		 * code, ask `getAltVarThresh` what counts as large, and flag
		 * every phase over it as carrying alternate RBS.
		 */
		float var[V90ADID_PHASES];
		float thresh;
		short n = 0;
		short p;

		int_a988++;
		calculateLinearMeanAndVar(sample, ucodeLevel, phase);

		if (int_a988 != int_a98c)
			break;

		/*
		 * NO ZERO-COUNT GUARD, where every other mean in the class has
		 * one: a phase with no samples divides 1.0f by 0 here and its
		 * variance comes out a NaN or an infinity, which then goes
		 * straight into `getAltVarThresh`.  docs/deviations.md D292.
		 */
		for (p = 0; p < V90ADID_PHASES; p++) {
			float inv = 1.0f / uint_1c00[p][ucode];
			float mean = float_1000[p][ucode] * inv;

			var[p] = inv * float_9118[p][ucode] - mean * mean;
		}

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90AutoDigitalImpDetector:  "
					     "initail var (Trn1):  %d  %d  %d  "
					     "%d  %d  %d\n", (int)var[0],
					     (int)var[1], (int)var[2],
					     (int)var[3], (int)var[4],
					     (int)var[5]);

		thresh = getAltVarThresh(var, float_a970);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90AutoDigitalImpDetector:  "
					     "initail AltRbsVarThresh (Trn1) = "
					     "%d\n", (int)thresh);

		/*
		 * An ORDERED `>`: the object's `fcomps`/`jbe` skips on an
		 * unordered compare, so a NaN variance -- which the missing
		 * guard above can produce -- does NOT flag its phase.
		 */
		for (p = 0; p < V90ADID_PHASES; p++)
			if (var[p] > thresh) {
				n = (short)(n + 1);
				short_2800[p] = 1;
			}

		/*
		 * And the accumulators are emptied only if something was
		 * flagged.  A study that finds no alternate RBS at all carries
		 * this pass's samples forward into the next state.
		 */
		if (n > 0)
			for (p = 0; p < V90ADID_PHASES; p++) {
				float_1000[p][ucode] = 0.0f;
				uint_1c00[p][ucode] = 0;
				float_9118[p][ucode] = 0.0f;
			}

		edprintf("V90AutoDigitalImpDetector: Trn1 alternate rbs initial "
			 "patern  %d%d%d%d%d%d\n", short_2800[0], short_2800[1],
			 short_2800[2], short_2800[3], short_2800[4],
			 short_2800[5]);

		int_a988 = 0;
		int_a984 = 1;
		break;
	}

	case 1:
		/*
		 * The first update.  Same accumulation, and the answer to the
		 * caller is now "was this phase already flagged" -- the index
		 * is the RAW `unsigned int` argument here where every other arm
		 * narrows it to a `short` first, because those arms reach the
		 * flag through `isAltRbs` and this one does not.
		 */
		int_a988++;
		calculateLinearMeanAndVar(sample, ucodeLevel, phase);
		ret = short_2800[phase] == 0 ? 1 : 0;

		if (int_a988 != int_a990)
			break;

		updateUref();

		/*
		 * TWO LOCAL ARRAYS ARE FILLED HERE AND NEVER READ.  The object
		 * runs two six-iteration loops copying `linMapp[i][ucode]` and
		 * then `linMappAlt[i][ucode]` into one twelve-byte stack slot
		 * at 0x50(%esp) -- GCC gave both the same slot because the
		 * first is dead before the second starts -- and the print below
		 * reads the tables directly.  All six stores to that slot in
		 * the whole function are writes and there is not one read, so
		 * they are omitted; cases 2 and 3 have the same pair and case 5
		 * does not.  docs/deviations.md D293.
		 */
		edprintf("V90AutoDigitalImpDetector trn1 first update  :  %d  "
			 "%d  %d  %d  %d  %d\n", linMapp[0][ucode],
			 linMapp[1][ucode], linMapp[2][ucode], linMapp[3][ucode],
			 linMapp[4][ucode], linMapp[5][ucode]);

		int_a988 = 0;
		int_a984 = 2;
		break;

	case 2:
		/*
		 * The second update, and the first that splits the sample two
		 * ways: a sample too far from this phase's established level
		 * goes to the ALTERNATE accumulators and the caller is told 0,
		 * anything else goes to the plain ones.
		 */
		int_a988++;

		if (isAltRbs((short)phase, ucode, v)) {
			calculateLinearMeanAndVarAlt(sample, phase);
			ret = 0;
		} else {
			calculateLinearMeanAndVar(sample, ucodeLevel, phase);
			ret = 1;
		}

		if (int_a988 != int_a994)
			break;

		updateUref();
		updateUrefAlt();

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90AutoDigitalImpDetector trn1 "
					     "second update  :  %d  %d  %d  %d "
					     " %d  %d\n", linMapp[0][ucode],
					     linMapp[1][ucode],
					     linMapp[2][ucode],
					     linMapp[3][ucode],
					     linMapp[4][ucode],
					     linMapp[5][ucode]);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90AutoDigitalImpDetector trn1 Alt"
					     " first update  :  %d  %d  %d  %d "
					     " %d  %d\n", linMappAlt[0][ucode],
					     linMappAlt[1][ucode],
					     linMappAlt[2][ucode],
					     linMappAlt[3][ucode],
					     linMappAlt[4][ucode],
					     linMappAlt[5][ucode]);

		adid_recheckAltRbs(this);

		short_a948 = 1;
		int_a988 = 0;
		int_a984 = 4;
		break;

	case 3:
		/*
		 * The third update.  The same split as state 2 -- but the
		 * caller is told 1 either way, so an alternate-RBS sample is
		 * no longer withheld from whatever is upstream.
		 */
		int_a988++;

		if (isAltRbs((short)phase, ucode, v))
			calculateLinearMeanAndVarAlt(sample, phase);
		else
			calculateLinearMeanAndVar(sample, ucodeLevel, phase);

		if (int_a988 != int_a998)
			break;

		updateUref();
		updateUrefAlt();

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90AutoDigitalImpDetector trn1 "
					     "third update  :  %d  %d  %d  %d  "
					     "%d  %d\n", linMapp[0][ucode],
					     linMapp[1][ucode],
					     linMapp[2][ucode],
					     linMapp[3][ucode],
					     linMapp[4][ucode],
					     linMapp[5][ucode]);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90AutoDigitalImpDetector trn1 Alt"
					     " second update  :  %d  %d  %d  %d"
					     "  %d  %d\n", linMappAlt[0][ucode],
					     linMappAlt[1][ucode],
					     linMappAlt[2][ucode],
					     linMappAlt[3][ucode],
					     linMappAlt[4][ucode],
					     linMappAlt[5][ucode]);

		adid_recheckAltRbs(this);
		adid_updateTrn1Sigma(this, "V90AutoDigitalImpDetector first "
					   "update : trn1Sigma = %d\n");

		int_a988 = 0;
		int_a984 = 5;
		break;

	case 4: {
		/*
		 * A pure delay, and the only arm that does not look at the
		 * sample at all: +0xa99c calls apart, it hands the machine on
		 * to state 3.  The count that fires it is the incremented one
		 * and what is stored on the firing pass is zero, not it.
		 */
		int next = int_a988 + 1;

		if (next == int_a99c) {
			int_a988 = 0;
			int_a984 = 3;
		} else {
			int_a988 = next;
		}
		break;
	}

	case 5:
		/*
		 * The final update.  State 3's arm again, against the same
		 * duration at +0xa998 -- and the tail has no re-test of the
		 * flags, because there is nothing left to correct them for.
		 */
		int_a988++;

		if (isAltRbs((short)phase, ucode, v))
			calculateLinearMeanAndVarAlt(sample, phase);
		else
			calculateLinearMeanAndVar(sample, ucodeLevel, phase);

		if (int_a988 != int_a998)
			break;

		updateUref();
		updateUrefAlt();

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90AutoDigitalImpDetector trn1 "
					     "final update  :  %d  %d  %d  %d  "
					     "%d  %d\n", linMapp[0][ucode],
					     linMapp[1][ucode],
					     linMapp[2][ucode],
					     linMapp[3][ucode],
					     linMapp[4][ucode],
					     linMapp[5][ucode]);

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90AutoDigitalImpDetector trn1 Alt"
					     " final update  :  %d  %d  %d  %d "
					     " %d  %d\n", linMappAlt[0][ucode],
					     linMappAlt[1][ucode],
					     linMappAlt[2][ucode],
					     linMappAlt[3][ucode],
					     linMappAlt[4][ucode],
					     linMappAlt[5][ucode]);

		adid_updateTrn1Sigma(this, "V90AutoDigitalImpDetector second "
					   "update : trn1Sigma = %d\n");

		int_a988 = 0;
		int_a984 = 6;
		ret = 2;
		break;

	case 6:
		/* Terminal: the study is over and says so on every call. */
		ret = 2;
		break;

	default:
		/*
		 * The dispatch is `cmp $0x6; ja`, an UNSIGNED compare, so a
		 * negative state word lands here rather than below the table.
		 */
		break;
	}

	return ret;
}

/*
 * Find the gain of the pad in the line, and which companding law is on the
 * other end of it.
 *
 * THE IDEA.  If the network puts a pad -- a fixed attenuation -- between the
 * digital source and this modem, every level the modem measures is the
 * transmitted level divided by that gain.  So: pick a code, ASSUME its
 * measured level is really the level of some OTHER code sent at full scale,
 * and the ratio of the two is a candidate gain.  Divide the reference phase's
 * whole mapping by that candidate, push each entry through the companding law
 * and straight back again, and add up the squares of what does not survive
 * the round trip.  The right gain is the one whose codes land on codec levels,
 * so it is the one with the smallest error.  Do the whole thing twice, once
 * per law, and the smaller of the two errors names the law as well.
 *
 * FOUR THINGS ABOUT IT ARE WORTH KNOWING BEFORE READING IT.
 *
 * THE SEARCH FOR THE STARTING CODE IS FIVE ENTRIES WIDE AND ITS RESULT IS
 * THEN CLAMPED INTO SIXTEEN.  `projectionBaseUcode` is the code with the
 * smallest variance among the five ending at `byte_a954 - 3`, and it is then
 * forced into [0x50, 0x5f] -- so the search only shows through the clamp, and
 * only when it lands inside that window.  Both names are the object's own:
 * it prints them as "projectionBaseUcode" and "minUcodeForCodecProjection".
 *
 * THE ERRORS ARE BUCKETED BY GAIN, THREE WAYS, AND THE BUCKETS ARE NOT
 * SYMMETRIC.  A candidate above 2.7 goes in one bucket, one below 1.2 in
 * another -- but only for A-law -- and everything else in a third.  The
 * best-of-three then prefers the middle bucket unless the high bucket beats
 * it by a fifth (0.8f), and A-law additionally falls back to a gain of
 * exactly 1.0f if the low bucket beats the answer by 0.15 (0.85f).  So
 * "no pad at all" is a hypothesis the method holds separately and only for
 * one of the two laws.
 *
 * THE LOW BUCKET KEEPS NO GAIN.  There are five slots and not six: the
 * fallback it feeds is the constant 1.0f, so the candidate that filled it is
 * never needed.
 *
 * `bestAt` IS READ UNINITIALISED IF NOTHING EVER WINS.  The running minimum
 * starts at 1e8 and the five entries are compared against it with the
 * object's `jb` -- so five variances of 1e8 or more, with no NaN among them,
 * leave the object taking `projectionBaseUcode` off a stack slot it never
 * wrote.  It is left uninitialised here for D281's reason and the test is
 * constructed to keep off it.  docs/deviations.md D290, which is also where
 * the two ways this method fails to terminate are written down.
 */
void
V90AutoDigitalImpDetector::findPadGain()
{
	unsigned char base;		/* the object's projectionBaseUcode */
	unsigned char minU;		/* minUcodeForCodecProjection */
	unsigned char aCode, uCode;
	unsigned char codec;
	float minErrorMuLaw, gainValueMuLaw;
	float minErrorALaw, gainValueALaw;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("------------------------------------"
				     "------------------------------------"
				     "-\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90AutoDigitalImpDetector::findPadGain()"
				     " Report :\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("------------------------------------"
				     "------------------------------------"
				     "-\r\n");

	{
		/*
		 * Five codes ending at `byte_a954 - 3`, and the one with the
		 * smallest variance wins.  Every index is a BYTE, recomputed
		 * from the counter rather than walked as a pointer -- 0x43674
		 * forms the address afresh each pass -- so a counter that
		 * wraps at zero reads entry 255 of the row and not the entry
		 * in front of it.
		 *
		 * The bound is `(int)start - 5` and the counter is compared
		 * against it zero-extended with a SIGNED `jg`, so a `byte_a954`
		 * of 3 to 7 inclusive makes the bound negative and the loop
		 * never ends.  D290.
		 */
		float bestVar = 100000000.0f;
		unsigned char bestAt;
		unsigned char d = (unsigned char)(byte_a954 - 3);
		int lim = (int)d - 5;

		do {
			float v = float_9d48[unSuspectedPhase][d];

			/* `jb` takes an unordered compare -- finding F1436. */
			if (!(v >= bestVar)) {
				bestVar = v;
				bestAt = d;
			}
			d--;
		} while ((int)d > lim);

		base = bestAt;
	}

	if (base > 0x5f)
		base = 0x5f;
	else if (base < 0x50)
		base = 0x50;

	/*
	 * The two companded spellings of the base code, formed once: the
	 * sign/company bits are supplied exactly as `reset` supplies them,
	 * `^ 0xd5` for A-law and `~` -- which is `^ 0xff` on a byte -- for
	 * mu-law.
	 */
	aCode = (unsigned char)((base & 0x7f) ^ 0xd5);
	uCode = (unsigned char)~(base & 0x7f);

	for (codec = 0; codec <= 1; codec++) {
		/*
		 * The three buckets and their two gains.  Five slots, reset
		 * for each law: the low bucket's fallback is a constant, so it
		 * needs no gain of its own.
		 */
		float errHigh = 1000000.0f, gainHigh = 1.0f;
		float errMid = 1000000.0f, gainMid = 1.0f;
		float errLow = 1000000.0f;
		float scale = float_a97c;
		unsigned char cur;

		/*
		 * The bottom of the candidate range: the base code's level cut
		 * by `float_a97c`, companded back, and floored at 0x28.  The
		 * object loads the scale before the call and spills it, which
		 * is a spill and not an ordering the arithmetic depends on.
		 */
		if (codec != 0)
			minU = (unsigned char)(
			    linear2alaw(adid_abs((int)(alaw2linear(aCode)
						       * scale))) ^ 0xd5);
		else
			minU = (unsigned char)~linear2ulaw(
			    adid_abs((int)(ulaw2linear(uCode) * scale)));

		if (minU <= 0x27)
			minU = 0x28;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("Codec type %d\r\n", codec);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("projectionBaseUcode = %d "
					     "minUcodeForCodecProjection = "
					     "%d\r\n", base, minU);

		for (cur = minU; cur <= base; cur++) {
			/*
			 * `fstps 0x9c(%esp)` on every pass of the inner loop,
			 * so the running total is rounded to a float 93 times
			 * over and not accumulated at 64 bits.
			 */
			volatile float errSum = 0.0f;
			float ref = (float)linMapp[unSuspectedPhase][base];
			/*
			 * `fstps 0x94(%esp)` at 0x437fa and `flds`/`fdivs`/
			 * `fmuls` of that slot at every one of its six uses --
			 * including the reciprocal, which the object forms
			 * afresh inside the loop where a plain local lets the
			 * compiler hoist it and round it once.
			 */
			volatile float gain;
			float err;
			int level;
			unsigned char k;

			if (codec != 0)
				level = alaw2linear(
				    (unsigned char)((cur & 0x7f) ^ 0xd5));
			else
				level = ulaw2linear(
				    (unsigned char)((cur & 0x7f) ^ 0xff));

			/*
			 * The candidate.  `ulaw2linear(0xff)` is 0, so an
			 * infinite or NaN gain is routine here and not exotic;
			 * every comparison below is written for it.
			 */
			gain = ref / (float)level;

			/*
			 * The round trip, over codes 0x40 up to `byte_a954`.
			 * The reciprocal is formed INSIDE the loop -- the
			 * object issues `flds 1.0f` and `fdivs` on every pass
			 * at 0x4382b -- and the projected level is stored
			 * through a SIXTEEN-bit `fistps`, so a projection out
			 * of a `short`'s range comes back as 0x8000 rather
			 * than as the low half of a 32-bit conversion.
			 */
			for (k = 0x40; k <= byte_a954; k++) {
				short proj = (short)(
				    linMapp[unSuspectedPhase][k]
				    * (1.0f / gain) + 0.5f);
				short back;
				float d;

				if (codec != 0)
					back = (short)alaw2linear(
					    linear2alaw(proj));
				else
					back = (short)ulaw2linear(
					    linear2ulaw(proj));

				d = (float)(proj - back);
				errSum += d * d;
			}

			err = gain * gain * errSum;

			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("error = %d, gain = "
						     "%c%d.%02d\r\n", (int)err,
						     ADID_PRINT_SIGN(gain),
						     ADID_PRINT_WHOLE(gain),
						     ADID_PRINT_FRAC(gain,
								     100.0f));

			/*
			 * The three buckets.  Each keeper is the negation of
			 * an ordered `>=`, because the object guards it with
			 * `jae` and an unordered compare does not take that
			 * branch.  NOTHING SEPARATES THE TWO READINGS HERE and
			 * the negation is written anyway: `err` is a product
			 * of a finite gain and a sum of at most 93 squared
			 * `short` differences, so it is never a NaN and never
			 * an infinity, and the four `equivalent` verdicts in
			 * test/mutations/v90adid.json carry the arithmetic.
			 * The two bucket TESTS are ordered `>=` as they stand:
			 * a NaN gain -- which needs a mapping this class
			 * cannot produce -- would fall to the last arm for
			 * A-law and to the middle one for mu-law, which is
			 * where the object's `jb` and `jae` send it.
			 */
			if (gain >= 2.7f) {
				if (!(err >= errHigh)) {
					errHigh = err;
					gainHigh = gain;
				}
			} else if (codec == 1 && !(gain >= 1.2f)) {
				if (!(err >= errLow))
					errLow = err;
			} else {
				if (!(err >= errMid)) {
					errMid = err;
					gainMid = gain;
				}
			}
		}

		/*
		 * Best of the three.  The A-law pair doubles as the working
		 * pair -- the object writes +0x90 and +0x98 on both passes and
		 * copies them out to the mu-law pair only on the first, which
		 * is why the A-law values are the ones still standing at the
		 * print below.
		 */
		gainValueALaw = gainMid;
		minErrorALaw = errMid;

		if (errMid * 0.8f > errHigh) {
			gainValueALaw = gainHigh;
			minErrorALaw = errHigh;
		}

		if (codec == 0) {
			gainValueMuLaw = gainValueALaw;
			minErrorMuLaw = minErrorALaw;
		} else if (!(errLow * 0.85f >= minErrorALaw)) {
			gainValueALaw = 1.0f;
			minErrorALaw = errLow;
		}
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("------ Final conclusions -------\r\n");

	/*
	 * Four reports, and the scale is a DOUBLE at all four -- `.rodata.cst8`
	 * +0xf0 and +0xf8, where `determineMaxUcode` above uses the float at
	 * `.rodata.cst4+0x390`.  Six fractional digits for a gain and two for
	 * an error.
	 */
	edprintf("minErrorMuLaw = %c%d.%02d\r\n",
		 ADID_PRINT_SIGN(minErrorMuLaw),
		 ADID_PRINT_WHOLE(minErrorMuLaw),
		 ADID_PRINT_FRAC(minErrorMuLaw, 100.0));
	edprintf("gainValueMuLaw = %c%d.%06d\r\n",
		 ADID_PRINT_SIGN(gainValueMuLaw),
		 ADID_PRINT_WHOLE(gainValueMuLaw),
		 ADID_PRINT_FRAC(gainValueMuLaw, 1000000.0));
	edprintf("minErrorALaw = %c%d.%02d\r\n",
		 ADID_PRINT_SIGN(minErrorALaw),
		 ADID_PRINT_WHOLE(minErrorALaw),
		 ADID_PRINT_FRAC(minErrorALaw, 100.0));
	edprintf("gainValueALaw = %c%d.%06d\r\n",
		 ADID_PRINT_SIGN(gainValueALaw),
		 ADID_PRINT_WHOLE(gainValueALaw),
		 ADID_PRINT_FRAC(gainValueALaw, 1000000.0));

	/*
	 * The verdict.  An ordered `>` -- a NaN mu-law error names mu-law,
	 * which is what the object's `ja` does.
	 */
	if (minErrorMuLaw > minErrorALaw) {
		int_a960 = 1;
		edprintf("Final codec identified is ALaw\r\n");
	} else {
		int_a960 = 0;
		edprintf("Final codec identified is MuLaw\r\n");
		gainValueALaw = gainValueMuLaw;
	}

	padGain = gainValueALaw;

	edprintf("Final padGain identified is = %c%d.%06d\r\n",
		 ADID_PRINT_SIGN(gainValueALaw),
		 ADID_PRINT_WHOLE(gainValueALaw),
		 ADID_PRINT_FRAC(gainValueALaw, 1000000.0));

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("------------------------------------"
				     "------------------------------------"
				     "-\r\n");
}

/*
 * Divide both mapping tables through by the pad gain.
 *
 * The reciprocal is formed once, outside both loops -- the object hoists
 * `fld1` and the 0.5f above the loop and issues a single `fdivs` -- so a pad
 * gain of zero produces an infinity that then multiplies every entry, rather
 * than 1,536 separate divisions.  `reset` seeds the gain with 1.0f, which
 * makes this method a no-op until `findPadGain` has run.
 */
void
V90AutoDigitalImpDetector::applyPadGainToLinMapp()
{
	float inv = 1.0f / padGain;
	short phase;

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		short ci;

		for (ci = 0; ci < V90ADID_CODES; ci++) {
			linMapp[phase][ci] =
			    (short)(linMapp[phase][ci] * inv + 0.5f);
			linMappAlt[phase][ci] =
			    (short)(linMappAlt[phase][ci] * inv + 0.5f);
		}
	}
}

/*
 * ==========================================================================
 * THE PAD-GAIN BATCH: `determineMaxUcode` and `findPadGain`.
 *
 * WHAT THE TWO ARE FOR, AND WHY THEY ARE ONE READING.  By this point the
 * class knows, for every RBS phase and every seven-bit code, how noisy that
 * cell's measured mapping is -- `float_9d48`, which the object's own format
 * string calls `linearMappingVar`.  `determineMaxUcode` turns that into two
 * answers: a per-cell "is this code usable" mask in `byte_0d00`, and the
 * highest usable code for each phase in `maxUcode[6]`, both hung off one
 * scalar at +0xa954 that it also leaves behind.  `findPadGain` then starts
 * from that scalar, assumes a pad in the line, and searches for the gain that
 * makes the reference phase's mapping come back through a companding law
 * unchanged -- storing the winner in `padGain`, which is the only thing that
 * ever makes `applyPadGainToLinMapp`'s division do anything, because `reset`
 * seeds the gain with 1.0f.
 *
 * SO +0x0d00 IS DECIDABLE HERE and nowhere else: `determineMaxUcode` is the
 * only member besides `reset` that touches it, and it both fills it and reads
 * it back in the same call.  Finding F1435.
 *
 * THE UNORDERED COMPARE IS THE WHOLE OF THE CARE THESE TWO NEED.  Every
 * floating-point branch in both methods is an `fcom`, and an unordered
 * compare sets CF and ZF -- so wherever the object skips on `jae` a NaN does
 * NOT skip, wherever it takes on `jb` a NaN DOES take, and wherever it tests
 * with `je` a NaN compares EQUAL.  The first two are written as the negation
 * of an ordered test, which is what the branch encodes.  The THIRD is a plain
 * `v == 0.0f`: under the object's own `-mno-ieee-fp` that is one `fcom` with
 * no parity test, so the NaN takes it exactly as the object's `fcomp`/`je`
 * does.  The two-compare "not less and not greater" spelling that used to
 * stand here was a workaround for the flag `make period` was missing, not the
 * object's code (findings F1447, F1990 and F2300).
 *
 * AND A RIGHT SPELLING IS NOT YET A RIGHT COMPARE.  `fcom` sets CF on an
 * unordered compare whichever way round its operands are, so which one is in
 * %st(0) is what decides where a NaN goes -- and with the flag set GCC is
 * free to commute a compare and invert its predicate, which is identical for
 * ordered values and exactly opposite for a NaN.  `ADID_PRINT_SIGN` above is
 * spelled the way it is for that reason alone, and both of these methods
 * print through it.
 *
 * WHAT IS *NOT* REPRODUCED IS THE SIGNALLING.  The object compares with
 * `fcomp`, which raises #IA on a quiet NaN; both of this tree's compilers
 * emit `fucomp`, which does not, for every spelling tried.  Nothing here
 * reads the x87 status word, so no test can see it -- docs/deviations.md
 * D295.
 * A seeded `float_9d48` reaches all three: one 32-bit pattern in 128 is a
 * NaN.  Finding F1436.
 *
 * THREE OF THE LOCALS HERE ARE `volatile`, AND IT IS NOT A HINT.  The object
 * is `-mfpmath=387` and this tree is deliberately not `-ffloat-store`, so a
 * `float` local lives in an x87 register at 64 bits of significand until
 * something makes it spill -- and the value the ORIGINAL goes on to use is
 * whichever of the two its own compiler happened to leave in reach.  Where
 * the object stored a float and read it back, reproducing the ROUNDING is not
 * optional: `determineMaxUcode`'s threshold at twenty variances of 1e9 is
 * 1000000014.9 unrounded and 1000000000 rounded, and it prints the number.
 * Measured, not assumed -- `volatile` was added to exactly the three locals
 * whose transcripts disagreed and to no others, and each names the object's
 * store below.  Finding F1437.
 * ==========================================================================
 */

/*
 * Decide how far up the companding law each phase may be driven.
 *
 * FIVE STEPS, and the third is the one the method is named for:
 *
 *   1. a variance threshold, from the sum of |`linearMappingVar`| over codes
 *      40..59 of the reference phase, scaled by `float_a980` and by 0.05f,
 *      which is the 1/20 of the twenty entries written out;
 *   2. two reports of that threshold, either side of a clamp into
 *      [500, 100000];
 *   3. a scan DOWNWARDS from the argument for the first window of five
 *      adjacent codes containing more than two "small but not zero"
 *      variances, whose top becomes `byte_a954`, floored at `short_a97a`;
 *   4. `byte_0d00[phase][code]` set to 1 exactly where the code is within the
 *      argument's reach and the variance is under twice the threshold;
 *   5. `maxUcode[phase]` read back out of that mask.
 *
 * THE SECOND REPORT'S SIGN TEST IS THE ORDERED ONE.  The first prints through
 * the class's usual `fldz`/`fcomps`/`sbb` idiom, where an unordered compare
 * yields '+' (ADID_PRINT_SIGN); the second is `flds`/`fcomps 0.0f`/`ja` at
 * 0x44347, which yields '-'.  The clamp between them leaves the threshold in
 * [500, 100000], so no reachable value separates the two spellings -- but
 * they are different instructions and this is written as the one the object
 * has.
 *
 * THE REPORT HEADER IS HANDED THREE ARGUMENTS IT HAS NO CONVERSIONS FOR.
 * "linearMappingVar report:" contains no `%`, and the object still stores
 * `unSuspectedPhase`, `short_a97a + 1` and the argument into the outgoing
 * argument slots at 0x44373, 0x4437f and 0x44391 before the call.  GCC does
 * not emit dead stores into an argument area, so they are the call's and they
 * are written; no transcript can see the difference, so the disassembly is
 * the whole of the evidence for this one.
 *
 * THREE OF THE INDICES ARE UNBOUNDED -- docs/deviations.md D288 for the two
 * loops the argument drives off the end of the row, D289 for the backwards
 * walk that can never terminate.
 */
void
V90AutoDigitalImpDetector::determineMaxUcode(short maxCode)
{
	/* `fsts 0x1c(%esp)` at 0x4424b, reloaded with `flds` eight times. */
	volatile float varThresh;
	int origWhole;
	unsigned char minU = (unsigned char)short_a97a;
	unsigned char mu;
	unsigned char at;
	unsigned char phase;
	unsigned char k;
	unsigned char ci;

	{
		/*
		 * Twenty entries, codes 40..59 of the reference phase: the
		 * object counts a byte down from 0x13 and walks a pointer up
		 * from +0x9de8, which is entry 40 of the row.  The running
		 * total stays in %st(1) for the whole loop, so it accumulates
		 * at extended precision and rounds once -- which is why the
		 * Makefile is deliberately not -ffloat-store.
		 */
		float sum = 0.0f;
		float product;
		short i;

		for (i = 40; i <= 59; i++)
			sum += __builtin_fabsf(
			    float_9d48[unSuspectedPhase][i]);

		/*
		 * AND THE FIRST REPORT PRINTS THE PRODUCT AT BOTH PRECISIONS
		 * AT ONCE.  The object stores the float at 0x4424b and then
		 * takes `fabs` of the value STILL IN %st(0) -- the unrounded
		 * one -- while the sign, the truncation and the fractional
		 * part all come from `flds 0x1c(%esp)`, the rounded one.  At a
		 * threshold of 1000000014.9 that is "1000000014.00" against
		 * "1000000000.00", so the split is printed and has to be
		 * written.  Everything from the clamp onwards uses the rounded
		 * value alone.  Finding F1437.
		 */
		product = sum * float_a980 * 0.05f;
		origWhole = ADID_PRINT_WHOLE(product);
		varThresh = product;
	}

	edprintf("V90AutoDigitalImpDetector: original varThresh = %c%d.%02d\r\n",
		 ADID_PRINT_SIGN(varThresh), origWhole,
		 ADID_PRINT_FRAC(varThresh, 100.0f));

	/*
	 * Both tests are the object's `jae` and `jbe`, so a NaN threshold
	 * fails the first and comes out as 500 -- which is what leaves the
	 * ordered sign test below unreachable on anything but a real number.
	 */
	if (!(varThresh >= 500.0f))
		varThresh = 500.0f;
	else if (!(varThresh <= 100000.0f))
		varThresh = 100000.0f;

	edprintf("V90AutoDigitalImpDetector: final varThresh = %c%d.%02d\r\n",
		 varThresh > 0.0f ? '+' : '-', ADID_PRINT_WHOLE(varThresh),
		 ADID_PRINT_FRAC(varThresh, 100.0f));

	edprintf("--------------------------------------------------------\r\n");
	edprintf("V90AutoDigitalImpDetector: linearMappingVar report:\r\n",
		 unSuspectedPhase, short_a97a + 1, maxCode);

	/*
	 * A BYTE COUNTER AGAINST A SIGNED INT BOUND.  `k` is zero-extended for
	 * the compare, so a `maxCode` of 255 or more never ends the loop --
	 * the counter wraps to 0 and 0 is still under the bound -- and a
	 * negative one skips it entirely.  D288.
	 */
	for (k = (unsigned char)(short_a97a + 1); (int)k <= maxCode; k++)
		edprintf("linearMappingVar[%d][%d] = %d\r\n", unSuspectedPhase,
			 k, adid_abs((int)float_9d48[unSuspectedPhase][k]));

	edprintf("--------------------------------------------------------\r\n");

	/*
	 * The scan.  Five adjacent codes ending at `ci`, and the answer is the
	 * top of the first window in which more than two of them have a
	 * variance that is small and not zero.  What is stored is the top
	 * MINUS the offset of the first qualifying entry, so it is the highest
	 * code that actually qualified and not the window's top.
	 *
	 * `ci` and the window offsets are bytes and the row is 128 wide, so
	 * the window reads outside the reference phase's row at both ends --
	 * D288.
	 */
	at = minU;

	for (ci = (unsigned char)maxCode; ci > minU; ci--) {
		unsigned char first = 0;
		unsigned char n = 0;
		unsigned char d;

		for (d = 0; d <= 4; d++) {
			float v = float_9d48[unSuspectedPhase][ci - d];

			if (v >= varThresh)
				continue;
			/*
			 * THE OBJECT'S ZERO TEST TAKES A NaN.  `fcomp %st(1)`
			 * against the zero it has kept on the stack since
			 * 0x4443c, then `je` -- and an unordered compare sets
			 * C3, so ZF is set and the entry is skipped.  ONE
			 * compare, and under -mno-ieee-fp `v == 0.0f` is
			 * exactly that: GCC emits `fcom`/`fnstsw`/`sahf`/`je`
			 * with no parity test, so a NaN is skipped too.
			 *
			 * This used to be spelled "not less and not greater",
			 * two compares, because under -mieee-fp `==` acquires
			 * a parity test and a NaN then falls through.  That
			 * was a workaround for `make period`'s flag set, and
			 * with the object's own flag it is neither needed nor
			 * the object's code.  Findings F1436, F1447 and F2300.
			 */
			if (v == 0.0f)
				continue;

			if (n == 0)
				first = d;
			n++;
		}

		if (n > 2) {
			at = (unsigned char)(ci - first);
			break;
		}
	}

	byte_a954 = at;

	/*
	 * The floor.  The compare is signed and against the whole `short`, so
	 * a `short_a97a` above 255 forces the byte every time.
	 */
	mu = byte_a954;

	if ((int)mu < (int)short_a97a) {
		edprintf("V90AutoDigitalImpDetector: original maxUcode = %d  "
			 "forced minimum maxUcode = %d\n", mu, short_a97a);
		byte_a954 = (unsigned char)short_a97a;
		mu = (unsigned char)short_a97a;
	}

	/*
	 * The usability mask.  The per-phase test is loop-invariant and GCC
	 * unswitched it -- which is why a flagged phase still spends 128
	 * iterations doing nothing at 0x44560, an empty `inc`/`jns` spin -- so
	 * it is written where the object's source had it and not where the
	 * object's code has it.
	 *
	 * THE REFERENCE CODE IS FORCED BACK TO 1 AFTERWARDS, for every phase
	 * and whatever the variance said, and `ucode` is a byte the object
	 * never masks: a reference code of 128 or more marks a cell in the
	 * next phase's row.  docs/deviations.md D289.
	 */
	{
		float lim = varThresh + varThresh;
		unsigned char ref = ucode;

		for (phase = 0; phase < V90ADID_PHASES; phase++) {
			unsigned char code;

			for (code = 0; code < V90ADID_CODES; code++) {
				if (short_2800[phase] != 0)
					continue;

				if ((int)code > (int)maxCode)
					byte_0d00[phase][code] = 0;
				else if (__builtin_fabsf(float_9d48[phase][code])
					 >= lim)
					byte_0d00[phase][code] = 0;
				else
					byte_0d00[phase][code] = 1;
			}

			byte_0d00[phase][ref] = 1;
		}
	}

	/*
	 * And the read-back.  An unflagged phase takes the scan's answer and
	 * then walks DOWNWARDS until it finds a usable code; a flagged one
	 * takes the answer clamped to two below the argument, and never looks
	 * at the mask at all.
	 *
	 * THE WALK HAS NO FLOOR.  It decrements a byte, so it wraps at 0 into
	 * the previous 128 indices -- which for phases 0..4 is the next
	 * phase's row and for phase 5 is the start of `float_1000`, both
	 * inside the object -- and if all 256 of those bytes are zero it never
	 * ends.  docs/deviations.md D289.
	 */
	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		unsigned char m = byte_a954;

		if (short_2800[phase] == 0) {
			maxUcode[phase] = m;

			if (byte_0d00[phase][m] == 0) {
				unsigned char d = m;

				do {
					d--;
				} while (byte_0d00[phase][d] == 0);

				maxUcode[phase] = d;
			}
		} else if ((int)m <= (int)maxCode - 2) {
			maxUcode[phase] = m;
		} else {
			maxUcode[phase] = (unsigned char)(maxCode - 2);
		}

		edprintf("V90AutoDigitalImpDetector: phase %d maxUcode is %d\r\n",
			 phase, maxUcode[phase]);
	}
}

/* Copy the caller's six per-phase maximum codes in. */
void
V90AutoDigitalImpDetector::setMaxUcodeArray(unsigned char *from)
{
	short phase;

	for (phase = 0; phase < V90ADID_PHASES; phase++)
		maxUcode[phase] = from[phase];
}

/*
 * ==========================================================================
 * `studyUrefHandler` -- the per-sample entry point of the whole study, and the
 * last member of the class.
 *
 * WHAT THE STUDY DOES.  The far end sends the TRN1 segment: a long run of one
 * PCM code, the "reference code" the class calls `ucode`.  Every sample of it
 * arrives here with the RBS phase it belongs to, and the method's job is to
 * decide, for each of the six phases, what linear level that one code actually
 * comes back at -- and whether the phase is delivering a SECOND level as well,
 * which is what "alternate RBS" means.  It does that in six timed passes, each
 * ending in an update of `linMapp` and `linMappAlt` for the reference code, and
 * it leaves behind the two things the rest of the class runs on: the per-phase
 * alternate-RBS flags at +0x2800, and `trn1Sigma`, the mean variance of the
 * unsuspected phases that `porcessFirstStudy` turns into its smoothness
 * threshold.
 *
 * IT IS A STATE MACHINE ON +0xa984, AND THE STATES ARE NOT IN ORDER.  The
 * object dispatches through a seven-entry jump table at `.rodata+0xd70` and
 * each arm names its successor, so the chain is measured and not inferred:
 *
 *     0 --a98c--> 1 --a990--> 2 --a994--> 4 --a99c--> 3 --a998--> 5 --a998--> 6
 *
 * Six of the seven arms count samples in +0xa988 and fire their tail when the
 * count reaches the duration named above the arrow; state 4 counts and does
 * nothing else, and state 6 is terminal.  `resetStudyUrefHandler` copies SIX
 * durations out of the parameter block and only five of them are ever read:
 * states 3 and 5 share +0xa998 and +0xa9a0 is read by no member of the class.
 * docs/deviations.md D294.
 *
 * IT RETURNS AN int, AND THE MANGLING DOES NOT SAY SO.  Every arm leaves
 * through `mov 0x3c(%esp),%eax` at 0x421a3, and that slot holds 1 on entry.
 * Three values are reachable and each means something: 2 is "the study is
 * over" (state 6, and the call that enters it), 0 is "do not use this sample"
 * -- state 1 when the phase is already flagged, state 2 when the alternate-RBS
 * test fires -- and 1 is everything else.
 *
 * WHAT IT CALLS, AND WHAT THE OBJECT INLINED.  Five of the class's own members
 * do the work -- `calculateLinearMeanAndVar`, `calculateLinearMeanAndVarAlt`,
 * `isAltRbs`, `updateUref` and `updateUrefAlt` -- and the object inlines all
 * five, which is most of why 5,335 bytes decode into this much source.  They
 * are called here; the shapes match instruction for instruction and calling
 * them is a factoring difference (finding F1440).  The two it really does call
 * are `getAltVarThresh` and, through `updateUref`, `unitePhasesInfoOfUref`.
 *
 * NO LOCAL IS EVER READ BEFORE IT IS WRITTEN.  Unlike D281, D284 and D290,
 * every one of this method's locals -- the return slot, the three alternate-RBS
 * flags, `var[6]`, the threshold, the saved reference codes and both control
 * words -- is written on every path that reads it, so every arm is fully
 * comparable and the test needs no carve-out.
 * ==========================================================================
 */

/*
 * Copy the previous session's 128-entry linear mapping in.  One entry per
 * code and none per phase: the loop index is compared against 0x7f, not
 * against 6 * 128.
 */
void
V90AutoDigitalImpDetector::setPrevSessionLinearMapping(short *from)
{
	short ci;

	for (ci = 0; ci < V90ADID_CODES; ci++)
		prevLinMapp[ci] = from[ci];
}

/*
 * Build the mapping the QC session starts from.
 *
 * EVERY PHASE THAT IS NOT FLAGGED AT +0x2800 GETS A MAPPING, from one of two
 * places: its own accumulators if it has a verdict at +0x280c, and the
 * PREVIOUS SESSION's mapping if it has not.  So +0x280c is read here as "this
 * phase was studied" rather than as "this phase is suspected", which is the
 * opposite of how `uniteLinMappInfoOfUnsuspectedPhases` reads it -- the object
 * uses the same byte both ways and the two methods are not a pair.
 *
 * The per-code work is `updateLinMappMeanAndVar` and the object inlines it:
 * the same `1.0f / count`, the same `E[x^2] - E[x]^2`, the same `+ 0.5f` and
 * the same truncating `fistp`.  Calling it is a factoring difference.
 *
 * `prevLinMapp` IS ONE ROW FOR ALL SIX PHASES.  The copy loop reads
 * `prevLinMapp[code]` with no phase term at all, so every unstudied phase gets
 * the same 128 entries -- which is what makes +0x0c00 128 shorts rather than
 * 6 * 128 (finding F1361).
 */
void
V90AutoDigitalImpDetector::setQcLinearMapping()
{
	short phase;
	short ci;

	for (phase = 0; phase < V90ADID_PHASES; phase++) {
		if (short_2800[phase] != 0)
			continue;

		if (byte_280c[phase] != 0) {
			for (ci = 0; ci < V90ADID_CODES; ci++)
				updateLinMappMeanAndVar(phase, ci);
		} else {
			for (ci = 0; ci < V90ADID_CODES; ci++)
				linMapp[phase][ci] = prevLinMapp[ci];
		}
	}

	for (unSuspectedPhase = 0;
	     byte_280c[unSuspectedPhase] != 0 && unSuspectedPhase <= 4;
	     unSuspectedPhase++)
		;

	updateAltRbsPhaseInDil();

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("------------------------------------"
				     "------------------\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("-----------------  Linear Mapping "
				     "report --------------\r\n");

	/*
	 * 0x74, not 0x7f: this loop stops seventeen codes short of the row
	 * where `porcessSecondStudy`'s prints the whole of it.
	 */
	for (ci = 0; ci <= 0x74; ci++)
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("linearMapping[%d]  :  %d  %d  "
					     "%d  %d  %d  %d\r\n", ci,
					     linMapp[0][ci], linMapp[1][ci],
					     linMapp[2][ci], linMapp[3][ci],
					     linMapp[4][ci], linMapp[5][ci]);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("------------------------------------"
				     "------------------\r\n");
}

/*
 * Rotate the reference code's column of `linMapp` so that it starts at the
 * given phase.
 *
 * Six entries are read into a local array walking phases from `offset` and
 * wrapping at 6, then written back to phases 0..5 in order.  The whole column
 * is copied out before any of it is written back, which is what makes this a
 * rotation rather than a shift that overwrites its own source.
 *
 * The wrap is `phase + 1 == 6 ? 0 : phase + 1` and not a modulus, so an
 * `offset` outside 0..5 walks straight out of the table rather than folding
 * back into it.  docs/deviations.md D258.
 */
void
V90AutoDigitalImpDetector::adjustUinfoToPhaseOffset(short offset)
{
	short saved[V90ADID_PHASES];
	short phase = offset;
	short i;

	for (i = 0; i < V90ADID_PHASES; i++) {
		saved[i] = linMapp[phase][ucode];
		phase = (short)(phase + 1) == V90ADID_PHASES
			    ? 0 : (short)(phase + 1);
	}

	for (i = 0; i < V90ADID_PHASES; i++)
		linMapp[i][ucode] = saved[i];
}
