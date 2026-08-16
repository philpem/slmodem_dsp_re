/*
 * V90ConstellationDesigner.cpp -- the V.90 constellation designer's rate
 * limits, lifecycle and eleven small members.
 *
 * Reconstructed from dsplibs.o.  Fifteen of the class's twenty-four symbols
 * live here: the two constructor variants and the two destructor variants,
 * `reset`, `setMinMaxRates` -- the only one `v34handshak` reaches -- and the
 * eleven leaves below.  `include/dsplib/V90ConstellationDesigner.h` carries
 * the object map and the evidence for it.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding 215):
 * `mov 0x10(%esp),%ebx` after one push and an eight-byte frame.  Nothing here
 * needs a calling-convention attribute.
 *
 * THE GATE IS TESTED TWICE.  The object compares `dsplibs_debug_level`
 * against 1 before the first diagnostic and AGAIN before the second, which is
 * what two separate `DSPLIB_DEBUG_ON()` sites compile to and what a single
 * `if` around both would not: `dsplibs_debug_printf` is an external call, so
 * the compiler must assume it can change the level.  For the same reason the
 * second diagnostic RELOADS `maxRate` from the object -- the argument
 * register did not survive the first call -- while the first uses the
 * argument still live in `%edx`.  Both spellings of the first are the same
 * code; `minRate` is written here because it is what the second must be.
 */

#include <stddef.h>

#include "dsplib/debug.h"
/*
 * `pcm.h` is a C header with no linkage guard of its own, so it takes the
 * same wrapper every other C++ consumer of it uses (V90Phase3Modulator.cpp
 * and V90AutoDigitalImpDetector.cpp): without it `linear2alaw` mangles and
 * the reference resolves to nothing.
 */
extern "C" {
#include "dsplib/pcm.h"
}
#include "dsplib/V90MappingParams.h"
/*
 * For `V90Parameters` -- the NAMED 0x558 map, not `V90PreFilter.h`'s 0x504
 * word block.  Two definitions of that class exist in this tree and no
 * translation unit may include both (finding 1112); this one takes the named
 * map, because the slot `reset` copies has a name in it and a numeric index
 * would throw that away.
 */
#include "dsplib/V90Parameters.h"
#include "dsplib/V90ConstellationDesigner.h"

/*
 * Hold the compiler to the map in the header.  `tools/offcheck.py` does this
 * for the C structs but parses only `struct name {`, so a C++ class asserts
 * its own -- and this is the check that catches an object right in size and
 * wrong in its offsets.  Skipped on the 64-bit `check64` pass, where a
 * 32-bit layout is not what the compiler lays out.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V90CD_OFF(field, off, tag) \
	typedef char v90cd_off_##tag[ \
	    ((int)__builtin_offsetof(V90ConstellationDesigner, field) \
	     == (off)) ? 1 : -1]

V90CD_OFF(params,    0x00, params);
V90CD_OFF(mappingParams, 0x04, mappingparams);
V90CD_OFF(byte_08,   0x08, byte08);
V90CD_OFF(constelTable, 0x14, consteltable);
V90CD_OFF(word_2c,   0x2c, word2c);
V90CD_OFF(short_0a,  0x0a, short0a);
V90CD_OFF(short_0c,  0x0c, short0c);
V90CD_OFF(short_0e,  0x0e, short0e);
V90CD_OFF(short_10,  0x10, short10);
V90CD_OFF(word_24,   0x24, word24);
V90CD_OFF(power,     0x30, power);
V90CD_OFF(byte_38,   0x38, byte38);
V90CD_OFF(preFilter, 0x44, prefilter);
V90CD_OFF(word_48,   0x48, word48);
V90CD_OFF(maxRate, 0x4c, maxrate);
V90CD_OFF(minRate, 0x50, minrate);
typedef char v90cd_size[(sizeof(V90ConstellationDesigner) == 0x54) ? 1 : -1];
#endif

/*
 * The constructor -- eight stores, no branch, no call, and it reads none of
 * the three pointers it is handed.  The statement order below is the object's
 * store order; only the two rate defaults are constants a reader could have
 * predicted, and the header says why 28000 lands at +0x50 and 56000 at +0x4c
 * rather than the other way round.
 *
 * The 0x16 at +0x38 is written as `0x16` and not `22` because the object's
 * immediate is the measurement and its decimal reading is not: nothing here
 * knows what the field counts.
 */
V90ConstellationDesigner::V90ConstellationDesigner(V90Parameters *p,
						   V90PreFilter *pf,
						   V90ConstellationPower *cp)
{
	byte_08 = 0;
	word_48 = 0;
	byte_38 = 0x16;
	power = cp;
	params = p;
	minRate = 28000;
	maxRate = 56000;
	preFilter = pf;
}

/*
 * The destructor -- one byte, `ret`.  See the header: the symbol exists only
 * because the original declared the destructor, so declaring and emptying it
 * here is the reconstruction, not a placeholder.
 */
V90ConstellationDesigner::~V90ConstellationDesigner()
{
}

/*
 * reset -- seven stores, no branch, no call, no diagnostic.
 *
 * The whole body is `movl $0x0,0x48(%eax)`, four `movw $0x0` and two copies,
 * so the only thing that is not obvious from the disassembly is the widths,
 * and those are the store encodings: 0x48 and 0x24 are `movl`, the four at
 * +0x0a..+0x10 are `movw` with a `66` prefix.
 *
 * The parameter read is the last thing the object does and the FIRST thing
 * the compiler scheduled -- `mov (%eax),%ecx` is the second instruction --
 * which is register pressure and not statement order (CLAUDE.md's "free, so
 * ignore it").  The order below is the store order.
 */
void
V90ConstellationDesigner::reset()
{
	word_48 = 0;
	short_0a = 0;
	short_0c = 0;
	short_0e = 0;
	short_10 = 0;
	word_24 = params->unnamed_39c;
}

void
V90ConstellationDesigner::setMinMaxRates(unsigned int min, unsigned int max)
{
	minRate = min;
	maxRate = max;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner: set min rate to %d\r\n",
		    minRate);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner: set max rate to %d\r\n",
		    maxRate);
}

/*
 * ===========================================================================
 * The eleven small members
 * ===========================================================================
 *
 * NOTHING IN THE OBJECT CALLS ANY OF THEM.  A sweep of every `R_386_PC32`
 * relocation in `.text` finds no caller for any of the eleven, so each is
 * reached only through its own symbol and each is driven directly by
 * `test/unit/t_v90cdesign.cpp`.  That also means no call site types an
 * argument or a return for us: what is written below is what the bodies
 * force and nothing more.
 */

/*
 * log10() on the coprocessor, as the object computes it.
 *
 * `fldlg2` pushes log10(2) at the register's full 64-bit mantissa and `fyl2x`
 * computes st(1) * log2(st(0)) and pops, so the sequence takes one value and
 * leaves one -- net stack effect zero, which is what makes the "=t"/"0" tie
 * legal.  GCC emits it for `log10()` only under -funsafe-math-optimizations,
 * which this tree does not build with, and glibc's `log10()` is a polynomial
 * that differs from it in the last place.
 *
 * THIS IS THE THIRD COPY -- `src/pump/v90/VPcmFloModem.cpp` and
 * `src/pump/v90/V90Equalizer.cpp` carry the same eight lines, and that one is
 * deliberate for the same reason theirs is: hoisting it into a shared header
 * from this worktree would touch a file another batch owns for no
 * behavioural gain.  Recorded so that a later cleanup can collapse the three.
 */
static inline long double
x87_log10(long double x)
{
	long double r;

	__asm__ ("fldlg2\n\tfxch %%st(1)\n\tfyl2x" : "=t" (r) : "0" (x));
	return r;
}

/*
 * pow6 -- the sixth power of a short, as a float.
 *
 * `filds` converts the argument ONCE and the value stays in a register, so
 * `p *= x` below reloads nothing; the object's single conversion is what
 * makes that the shape and not six.  The counter is a `short`: the object
 * widens it with `cwtl` and compares `%ax` every turn, which a plain `int`
 * would not need.
 *
 * FIVE MULTIPLIES, not six.  The accumulator is seeded with x rather than
 * with 1, so `i` runs 1..5 and the result is x^6.
 */
float
V90ConstellationDesigner::pow6(short x)
{
	float p = x;
	short i;

	for (i = 1; i < 6; i++)
		p *= x;
	return p;
}

/*
 * calcK -- log2 of the product of six scaled constellation sizes.
 *
 * THE UNSIGNED ARGUMENT IS WHAT THE 64-BIT CONVERSION MEASURES: the object
 * pushes a zero high dword and uses `fildll`, which is the unsigned-to-float
 * idiom.  A signed `int` converts with a 32-bit `fildl` and no push at all.
 *
 * THE RECIPROCAL IS THE OBJECT'S, not a rewrite.  It divides ONE by
 * log10(2) and multiplies, where `maxK`, `realK` and `calcMtoMatchKtarget`
 * all divide directly -- and the two spellings are not the same in the last
 * place.  What shows it is the pair `fld1; fld %st(0)` at the top: the
 * compiler loaded the constant 1 once and duplicated it because the source
 * uses 1.0f twice, once to seed the accumulator and once as this numerator.
 */
float
V90ConstellationDesigner::calcK(unsigned int m, float *f)
{
	float k = 1.0f;
	float l2;
	short i;

	for (i = 0; i < 6; i++)
		k *= m * f[i];

	l2 = (float)x87_log10((long double)2.0f);
	return (float)x87_log10((long double)k) * (1.0f / l2);
}

/*
 * The product of the six constellation sizes, as a float.  Shared by `realK`
 * and `maxK`, which the object spells out separately -- both are six
 * `fildll`s and five `fmulp`s, left to right, with every size converted as
 * UNSIGNED.
 */
static float
constellation_product(V90MappingParams *p)
{
	return (float)p->constellationSize[0] * p->constellationSize[1]
	     * p->constellationSize[2] * p->constellationSize[3]
	     * p->constellationSize[4] * p->constellationSize[5];
}

/*
 * realK -- the same K as a float, with a zero product answered by zero.
 *
 * The compare is `fcom`/`fnstsw`/`sahf` against a literal 0.0f, which is an
 * ORDERED compare and therefore what -mno-ieee-fp emits for `==` (finding
 * 1990).  Both logarithms reach memory as floats before the divide.
 */
float
V90ConstellationDesigner::realK(V90MappingParams *p)
{
	float prod = constellation_product(p);
	float lp;
	float l2;

	if (prod == 0.0f)
		return 0.0f;

	lp = (float)x87_log10((long double)prod);
	l2 = (float)x87_log10((long double)2.0f);
	return lp / l2 + 1e-9f;
}

/*
 * maxK -- the same quantity truncated to an integer.
 *
 * THE 1e-6f IS THE OBJECT'S and it is not cosmetic: the truncation below
 * rounds toward zero (the control word is or'd with 0xc00 first), so a K that
 * lands a hair under an integer would truncate to the integer below without
 * it.
 *
 * THE CAST IS TO `unsigned int` AND THAT IS MEASURED.  The object converts
 * with `fistpll`, a 64-bit store, and then reads the low dword -- which is
 * how GCC converts a float to `unsigned int` on this target.  A cast to
 * `int` is a 32-bit `fistpl` and would have been one instruction shorter.
 */
int
V90ConstellationDesigner::maxK(V90MappingParams *p)
{
	float prod = constellation_product(p);
	float l2;

	if (prod == 0.0f)
		return 0;

	l2 = (float)x87_log10((long double)2.0f);
	return (int)(unsigned int)(x87_log10((long double)prod) / l2 + 1e-6f);
}

/*
 * calcMtoMatchKtarget -- the constellation size that reaches a target K.
 *
 * 2^((kTarget - log2(m)) / 6), and every constant in it is measured:
 * 0.16666667f is 1/6 rounded to a float, 100.0f is the fractional part's
 * scale and 1.0069555f is 2^(1/100) -- so the power is split into an integer
 * part done by doubling and a hundredth part done by repeated multiplication.
 *
 * BOTH LOOPS ARE THE SOURCE'S, not the compiler's.  `1 << n` does not compile
 * to a doubling loop and neither does `powf`; the object counts down through
 * a guarded `do`/`while` in each case, which is what a `for` over an UNSIGNED
 * bound becomes.
 *
 * THE HAZARD, and it is recorded in docs/deviations.md rather than guarded
 * here: nothing bounds `n`.  A `kTarget` below log2(m) makes the truncation
 * negative, the doubling loop then runs about 2^32 times, and the conversion
 * back to a float reads the negative low dword as unsigned.  The object does
 * exactly that.
 */
int
V90ConstellationDesigner::calcMtoMatchKtarget(float kTarget, float m)
{
	float lm = (float)x87_log10((long double)m);
	float l2 = (float)x87_log10((long double)2.0f);
	long double x = ((long double)kTarget - lm / l2) * (1.0f / 6.0f);
	unsigned int n = (unsigned int)x;
	unsigned int frac = (unsigned int)((x - n) * 100.0f);
	unsigned int shift = 1;
	float p = 1.0f;
	unsigned int i;

	for (i = 0; i < n; i++)
		shift += shift;
	for (i = 0; i < frac; i++)
		p *= 1.0069555f;

	return (int)(unsigned int)((long double)p * shift);
}

/*
 * findMinValueIndex -- the constellation whose first byte is smallest.
 *
 * THE COMPARISONS ARE UNSIGNED AND THAT IS FORCED: `jae`/`jbe` throughout,
 * where an `int` holding a `movzbl`-loaded byte would have compared signed.
 * So the value and the length are both unsigned here.
 *
 * THE TIE-BREAK IS THE ASYMMETRY WORTH SEEING.  On an equal first byte the
 * function takes the constellation with the LARGER size, in the minimum and
 * the maximum alike -- the two bodies differ in exactly one condition code.
 */
int
V90ConstellationDesigner::findMinValueIndex(V90MappingParams *p)
{
	unsigned int bestLen = p->constellationSize[0];
	unsigned int bestVal = p->constellation[0][0];
	int best = 0;
	int i;

	for (i = 1; i <= 5; i++) {
		unsigned int v = p->constellation[i][0];

		if (v < bestVal) {
			bestLen = p->constellationSize[i];
			bestVal = v;
			best = i;
		} else if (v == bestVal) {
			unsigned int len = p->constellationSize[i];

			if (len > bestLen) {
				bestLen = len;
				best = i;
			}
		}
	}
	return best;
}

/* The maximum, and see findMinValueIndex for the tie-break. */
int
V90ConstellationDesigner::findConstelMaxValueIndex(V90MappingParams *p)
{
	unsigned int bestLen = p->constellationSize[0];
	unsigned int bestVal = p->constellation[0][0];
	int best = 0;
	int i;

	for (i = 1; i <= 5; i++) {
		unsigned int v = p->constellation[i][0];

		if (v > bestVal) {
			bestLen = p->constellationSize[i];
			bestVal = v;
			best = i;
		} else if (v == bestVal) {
			unsigned int len = p->constellationSize[i];

			if (len > bestLen) {
				bestLen = len;
				best = i;
			}
		}
	}
	return best;
}

/*
 * spectralDesign -- copy one of two spectral shaper descriptions into the
 * constellation table, with the identifier limited by the rate.
 *
 * THE TWO ARMS ARE THE SAME SIX STORES from two disjoint runs of
 * `V90Parameters`, and `tools/vparse.py` names both runs, so which arm is
 * which is a measurement and not a reading of the enumerator: the `== 2` arm
 * copies `GERMAN_PBX_SPECTRAL_SHAPER_*`.
 *
 * THE LIMIT IS UNSIGNED because the rate argument is, whatever
 * `SPECTRAL_SHAPER_ID`'s own `int` says -- `ja` in one arm and `jbe` in the
 * other, from the same source expression.
 */
void
V90ConstellationDesigner::spectralDesign(unsigned int rate,
					 V90SpecialSpectralConditions cond)
{
	V90Parameters *p = params;
	V90MappingParams *mp = mappingParams;
	unsigned int id;

	if (cond == V90_SPECTRAL_GERMAN_PBX) {
		id = (unsigned int)p->GERMAN_PBX_SPECTRAL_SHAPER_ID;
		if (id > rate)
			id = rate;
		mp->shaperId = id;
		mp->shaperA1 = p->GERMAN_PBX_SPECTRAL_SHAPER_A1;
		mp->shaperSR = p->GERMAN_PBX_SPECTRAL_SHAPER_SR;
		mp->shaperA2 = p->GERMAN_PBX_SPECTRAL_SHAPER_A2;
		mp->shaperB1 = p->GERMAN_PBX_SPECTRAL_SHAPER_B1;
		mp->shaperB2 = p->GERMAN_PBX_SPECTRAL_SHAPER_B2;
	} else {
		id = (unsigned int)p->SPECTRAL_SHAPER_ID;
		if (id > rate)
			id = rate;
		mp->shaperId = id;
		mp->shaperA1 = p->SPECTRAL_SHAPER_A1;
		mp->shaperSR = p->SPECTRAL_SHAPER_SR;
		mp->shaperA2 = p->SPECTRAL_SHAPER_A2;
		mp->shaperB1 = p->SPECTRAL_SHAPER_B1;
		mp->shaperB2 = p->SPECTRAL_SHAPER_B2;
	}
}

/*
 * reconstructInitialConditions -- drop every entry of each constellation that
 * precedes the one the ucode names.
 *
 * THE SEARCH IS UNBOUNDED.  `while (constellation[k][d] != ucode[k]) d++;` is
 * a bare `jne` with nothing stopping it at the row's end or at the row's
 * length, so a ucode value that is not in the row walks off it.  The object
 * is written that way and it is reproduced; docs/deviations.md carries the
 * entry.
 *
 * THE SHIFT IS DONE ONE PLACE AT A TIME, `d` times, rather than by `d`
 * places once -- so it is O(d * n) and it re-reads the length every round,
 * because the length is decremented between rounds.  The inner index is an
 * `unsigned char` (`movzbl %bl` every turn), which is why the length is
 * hoisted into an `unsigned int`: the compare is `jb`.
 */
void
V90ConstellationDesigner::reconstructInitialConditions(V90MappingParams *p,
						       unsigned char *ucode)
{
	unsigned char k;

	for (k = 0; k <= 5; k++) {
		unsigned char target = ucode[k];
		unsigned char drop = 0;

		while (p->constellation[k][drop] != target)
			drop++;

		while (drop != 0) {
			unsigned int n = p->constellationSize[k];
			unsigned char i;

			for (i = 0; i < n; i++) {
				p->constellation[k][i] =
				    p->constellation[k][i + 1];
				p->codecConstellation[k][i] =
				    p->codecConstellation[k][i + 1];
			}
			p->constellationSize[k]--;
			drop--;
		}
	}
}

/*
 * constelBuild -- count the entries of one constellation row that clear a
 * threshold which rises as they are counted.
 *
 * THE TWO TABLES SHARE ONE BASE REGISTER.  `constelTable` is loaded once and
 * the 8-bit table is addressed at a fixed +0xd00 from it, so the displacement
 * is measured and the pointed-at object's shape is not; see the header.
 *
 * THE LOOP BOUND IS THE ROW'S FIRST BYTE and the start is a parameter, so
 * `i` can exceed 127 and read into the next row.  The object indexes flat
 * (`(k << 7) + i` with no masking) and that is reproduced.
 */
unsigned char
V90ConstellationDesigner::constelBuild(short step, short which)
{
	V90MappingParams *mp = mappingParams;
	short (*tbl)[128] = constelTable;
	const unsigned char *mark = (const unsigned char *)tbl + 0xd00;
	unsigned char count = 0;
	short thresh = step >> 1;
	unsigned int i;

	for (i = params->unnamed_360;
	     i <= mp->constellation[which][0];
	     i++) {
		short v = tbl[which][i];

		if (v > thresh && mark[which * 128 + i] != 0) {
			count++;
			thresh = (short)(step + v);
		}
	}
	return count;
}

/*
 * findNextUcodeToAdd -- walk a constellation forward from its current length
 * until the spacing to the entry it started from is wide enough, and encode
 * the entry it stopped on.
 *
 * TWO WALKS, AND THE BOUND IS DIFFERENT IN EACH.  Which one runs is decided
 * by `dmin[which] != 0`, and the difference is not only the test:
 *
 *   dmin non-zero   bound `i <= 0x71`, an UNSIGNED compare (`ja`/`jbe`), and
 *                   the entry must clear BOTH `ucode[start] + short_10` and
 *                   `alt[start] + short_10`
 *   dmin zero       bound `(signed char)i >= 0`, a SIGNED test (`js`/`jns`),
 *                   and one threshold, `ucode[start] + short_0a`
 *
 * so the same byte is compared unsigned in one arm and signed in the other,
 * which is what the two spellings below say.
 *
 * `__builtin_abs`, NOT the ternary.  The object's `cltd; xor %edx,%eax; sub
 * %edx,%eax` is what GCC 3.4.2 emits for the builtin; `x < 0 ? -x : x`
 * compiles to a branch (findings 2116-2117).
 *
 * THE SIXTH ARGUMENT IS UNUSED.  Nothing in the body touches 0x48(%esp).  It
 * is in the mangling, so it is in the signature.
 */
int
V90ConstellationDesigner::findNextUcodeToAdd(unsigned char *out,
					     unsigned char which,
					     short (*ucode)[128],
					     short (*alt)[128],
					     short *dmin,
					     unsigned char (*unused)[128])
{
	V90MappingParams *mp = mappingParams;
	unsigned char start = mp->constellation[which][0];
	unsigned char i = start;
	int sample;

	(void)unused;

	if (dmin[which] != 0) {
		int lo = ucode[which][start] + short_10;

		while (i <= 0x71) {
			short v = ucode[which][i];

			if (v >= lo && v >= alt[which][start] + short_10)
				break;
			i++;
		}
	} else {
		int lo = ucode[which][start] + short_0a;

		while ((signed char)i >= 0) {
			short v = ucode[which][i];

			if (v >= lo)
				break;
			i++;
		}
	}

	out[0] = i;
	sample = __builtin_abs((int)ucode[which][i]);
	if (word_2c != 0)
		out[1] = (unsigned char)(linear2alaw(sample) ^ 0xd5);
	else
		out[1] = (unsigned char)~linear2ulaw(sample);

	return (signed char)i >= 0;
}
