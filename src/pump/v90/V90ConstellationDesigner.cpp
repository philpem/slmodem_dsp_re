/*
 * V90ConstellationDesigner.cpp -- the V.90 constellation designer's rate
 * limits, lifecycle and eleven small members.
 *
 * Reconstructed from dsplibs.o.  The two constructor variants and the two
 * destructor variants live here, with `reset`, `setMinMaxRates` -- the only
 * one `v34handshak` reaches -- the eleven leaves below, and the two large
 * members that follow them: `determineDminForRrn` and
 * `setConstellationToNoise`.  `include/dsplib/V90ConstellationDesigner.h`
 * carries the object map and the evidence for it.
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
 * `setConstellationToNoise`'s six ungated diagnostics.  `encode.h` carries
 * its own `extern "C"`, so it needs no wrapper.
 */
#include "dsplib/encode.h"
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
V90CD_OFF(float_18,  0x18, float18);
V90CD_OFF(float_1c,  0x1c, float1c);
V90CD_OFF(float_20,  0x20, float20);
V90CD_OFF(word_28,   0x28, word28);
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
 * The product of the six constellation sizes, as a float: six `fildll`s and
 * five `fmulp`s, left to right, with every size converted as UNSIGNED.
 *
 * IT IS A MACRO AND NOT A FUNCTION, and that is measured rather than a style
 * choice.  `realK` and `maxK` each spell the product out in full in the
 * object, and there is no helper symbol anywhere in the blob.  Written as a
 * `static` function here it did NOT inline under GCC 3.4.2 -- the two members
 * came out 82 and 106 bytes against the object's 178 and 204, with the
 * missing ~96 sitting in a symbol the blob has no counterpart for, which is
 * CLAUDE.md's inlining-boundary trap exactly.
 */
#define CONSTELLATION_PRODUCT(p) \
	((float)(p)->constellationSize[0] * (p)->constellationSize[1] \
	 * (p)->constellationSize[2] * (p)->constellationSize[3] \
	 * (p)->constellationSize[4] * (p)->constellationSize[5])

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
	float prod = CONSTELLATION_PRODUCT(p);
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
	float prod = CONSTELLATION_PRODUCT(p);
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
	unsigned int i;

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
	unsigned int i;

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
	unsigned int id;

	if (cond == V90_SPECTRAL_GERMAN_PBX) {
		id = (unsigned int)params->GERMAN_PBX_SPECTRAL_SHAPER_ID;
		if (id > rate)
			id = rate;
		mappingParams->shaperId = id;
		mappingParams->shaperA1 = params->GERMAN_PBX_SPECTRAL_SHAPER_A1;
		mappingParams->shaperSR = params->GERMAN_PBX_SPECTRAL_SHAPER_SR;
		mappingParams->shaperA2 = params->GERMAN_PBX_SPECTRAL_SHAPER_A2;
		mappingParams->shaperB1 = params->GERMAN_PBX_SPECTRAL_SHAPER_B1;
		mappingParams->shaperB2 = params->GERMAN_PBX_SPECTRAL_SHAPER_B2;
	} else {
		id = (unsigned int)params->SPECTRAL_SHAPER_ID;
		if (id > rate)
			id = rate;
		mappingParams->shaperId = id;
		mappingParams->shaperA1 = params->SPECTRAL_SHAPER_A1;
		mappingParams->shaperSR = params->SPECTRAL_SHAPER_SR;
		mappingParams->shaperA2 = params->SPECTRAL_SHAPER_A2;
		mappingParams->shaperB1 = params->SPECTRAL_SHAPER_B1;
		mappingParams->shaperB2 = params->SPECTRAL_SHAPER_B2;
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
		/*
		 * THE OBJECT LOADS THIS WITH `movzwl` AND WE EMIT `movswl`,
		 * and that is the FREE kind of extension (finding 614), not
		 * finding 613's forced kind: the 32-bit result never survives.
		 * It feeds a 16-bit compare (`cmp %bp,%bx`) and then an add
		 * whose result is immediately truncated by `movswl %dx,%ebp`,
		 * so the upper half is dead both times.  `short` is also what
		 * the comparison needs -- it is SIGNED 16-bit (`jle`), which an
		 * `unsigned short` promoted to `int` would not give.
		 */
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

/*
 * ===========================================================================
 * determineDminForRrn -- the two minimum distances the rate-renegotiation
 * thresholds need, one for a rate down and one for a rate up.
 * ===========================================================================
 *
 * 3,760 bytes, and about 44% of them are diagnostics: seventeen
 * `dsplibs_debug_printf` sites, each its own `DSPLIB_DEBUG_ON()`.  The level
 * is re-read from memory after every call (0x48561, 0x4857c, 0x485c1 and
 * more), which is what separate gates compile to when the call between them
 * can change the level -- the same reasoning `setMinMaxRates` carries.
 *
 * THE FORMAT STRINGS ARE THE AUTHOR'S NAMES FOR THE VARIABLES and they are
 * where every name below comes from: `pParams->m[1..6]` for
 * `V90MappingParams::constellationSize`, `phase` for the chosen constellation,
 * `maxM`, `nofUcodes`, `prevNofUcodes`, `tempDmin`, `prevDmin`, `dMin`,
 * `rrnDownDmin` and `rrnUpDmin`.  The last three are `short_0a`, `short_0c`
 * and `short_0e`, and THIS IS THE FIRST READER OR WRITER OF `short_0c` AND
 * `short_0e` anywhere in the reconstruction: `reset` zeroes both and nothing
 * else had touched them.  `short_0a` is `dMin` -- read here, written by
 * nothing reconstructed yet.
 *
 * WHAT IT RETURNS IS NOTHING, and the argument is stronger than "no path sets
 * %eax": the two `ret` paths leave UNRELATED values there.  0x484fa arrives
 * with the coprocessor status word `fnstsw` deposited at 0x484c0, and 0x4881f
 * arrives with `dsplibs_debug_printf`'s return.  No int-returning source could
 * converge on those, so the return type is `void`.
 *
 * THE 0x280c DISPLACEMENT OFF `constelTable`.  Block one below tests
 * `((unsigned char *)constelTable)[0x280c + k]` where `constelBuild` tests
 * `[0xd00 + 128*k + i]`.  Two independent displacements off one pointer, from
 * two members, is what the header now records; neither says what the
 * pointed-at object IS and no struct is invented for it.
 *
 * THE TWO HALVES SPELL THE SAME FORMULA TWO DIFFERENT WAYS, and that is
 * measured rather than tidied.  Both compute 2^((kTarget - log2 m) / 6) the
 * way `calcMtoMatchKtarget` does, but the RRN-DOWN half divides ONE by
 * log10(2) and multiplies (0x47def is `d8 fc`, FDIVR ST(0),ST(4) with ST(4)
 * the CSEd 1.0) while the RRN-UP half divides directly (0x48302 is `de fa`,
 * FDIVP ST(2),ST(0) with ST(2) the logarithm).  Those are not the same value
 * in the last place, the differential tier cannot tell them apart (finding
 * 2150's shape), and factoring the two into one helper would be wrong in a
 * way no test could catch.  So they stay apart.
 *
 * THE PRODUCT OF THE SIX SCALED SIZES IS A `double`.  0x47d9b spills it with
 * `fstpl` and 0x47da5 and 0x482b6 reload it with `fldl` -- 64 bits, so the
 * variable is a `double` and not a `float`, and the rounding at that spill is
 * observable: it feeds a truncation two steps later.
 *
 * THE UNINITIALISED READ IS THE OBJECT'S.  When the rrn-down loop does not
 * run once, `prevNofUcodes` and `prevDmin` are read at 0x4815b and 0x48280
 * having never been written.  Reproduced, and D330 records it.
 */
void
V90ConstellationDesigner::determineDminForRrn(unsigned int rrn)
{
	float mm[6];
	double m;
	unsigned int maxSize;
	unsigned int rrnDownMaxM;
	unsigned int rrnUpMaxM;
	short phase;
	unsigned int k;
	int failed;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner::determineDminForRrn :"
		    " pParams->m[1..6] = %d %d %d %d %d %d\r\n",
		    mappingParams->constellationSize[0],
		    mappingParams->constellationSize[1],
		    mappingParams->constellationSize[2],
		    mappingParams->constellationSize[3],
		    mappingParams->constellationSize[4],
		    mappingParams->constellationSize[5]);

	/*
	 * The largest constellation among those the table's 0x280c byte
	 * allows.  `phase` is a `short` and the object proves it twice over:
	 * the assignment is `movswl %cx` into a 32-bit spill slot, and the
	 * same shape appears at 0x84/0x88(%esp) where a 32-bit slot is written
	 * by a 16-bit `fistps` and read by a 16-bit `filds`.  The counter is
	 * UNSIGNED -- `cmp $0x5,%ecx; jbe`.
	 */
	maxSize = 0;
	phase = 0;
	for (k = 0; k <= 5; k++)
		if (mappingParams->constellationSize[k] > maxSize
		    && ((unsigned char *)constelTable)[0x280c + k] == 0) {
			maxSize = mappingParams->constellationSize[k];
			phase = k;
		}

	/*
	 * The six sizes scaled by the largest, and their product.  NOTHING
	 * BOUNDS `maxSize`: if no constellation passes the test above it stays
	 * zero and the reciprocal is an infinity.  The object divides anyway;
	 * D331.
	 */
	m = 1.0;
	for (k = 0; k <= 5; k++) {
		mm[k] = mappingParams->constellationSize[k] * (1.0f / maxSize);
		m *= mm[k];
	}

	/*
	 * The rate-down target.  See the header comment for why this one
	 * multiplies by the reciprocal and its twin below divides.
	 */
	{
		float lm = (float)x87_log10((long double)m);
		float l2 = (float)x87_log10((long double)2.0f);
		long double x = ((long double)(rrn - 0.75f)
				 - lm * (1.0f / l2)) * (1.0f / 6.0f);
		unsigned int n = (unsigned int)x;
		unsigned int frac = (unsigned int)((x - n) * 100.0f);
		unsigned int shift = 1;
		float p = 1.0f;
		unsigned int i;

		for (i = 0; i < n; i++)
			shift += shift;
		for (i = 0; i < frac; i++)
			p *= 1.0069555f;
		rrnDownMaxM = (unsigned int)((long double)p * shift);
	}

	if (calcK(rrnDownMaxM, mm) < rrn - 0.8f) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner:: rrn down maxM too low"
			    " - probably will cause more then one rate down"
			    "\r\n");
		if (calcK(rrnDownMaxM + 1, mm) < rrn - 0.3f) {
			rrnDownMaxM++;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V90ConstellationDesigner:: increment maxM"
				    " for one rate down\r\n");
		}
	}
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner:: current maxM = %d "
		    " rrn down maxM = %d\r\n",
		    mappingParams->constellationSize[phase], rrnDownMaxM);

	if (mappingParams->constellationSize[phase] > rrnDownMaxM) {
		unsigned char nofUcodes;
		unsigned char prevNofUcodes;
		short tempDmin;
		short prevDmin;
		unsigned int nofIterations;

		/*
		 * `nofUcodes` is a BYTE and the truncation is the object's:
		 * `mov %al,%bl` off a 32-bit `constellationSize`.  So a size
		 * above 255 enters the loop as its low byte.  D332.
		 */
		nofUcodes = (unsigned char)
			    mappingParams->constellationSize[phase];
		tempDmin = short_0a;
		nofIterations = 0;
		while (nofUcodes >= rrnDownMaxM && nofIterations <= 99) {
			prevNofUcodes = nofUcodes;
			prevDmin = tempDmin;
			tempDmin = (short)(tempDmin * 1.02f + 1.0f);
			nofUcodes = constelBuild(tempDmin, phase);
			nofIterations++;
		}

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner:: rrn down - nofUcodes"
			    " = %d  prevNofUcodes = %d\r\n",
			    nofUcodes, prevNofUcodes);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner:: rrn down - tempDmin "
			    " = %d  prevDmin      = %d\r\n",
			    tempDmin, prevDmin);

		if (prevNofUcodes == rrnDownMaxM)
			short_0c = prevDmin;
		else if (calcK(nofUcodes, mm) >= rrn - 0.8f
			 || calcK(prevNofUcodes, mm) >= rrn - 0.3f)
			short_0c = tempDmin;
		else
			short_0c = prevDmin;
	} else {
		short_0c = (short)(short_0a * 1.25f + 0.5f);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner:: maxM>pParams->m[phase]"
			    " seting rrnDownDmin arbitrary !!!\r\n");
	}
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner:: rrnDownDmin = %d\r\n",
		    short_0c);

	/* And the rate-up target, which divides where its twin multiplied. */
	{
		float lm = (float)x87_log10((long double)m);
		float l2 = (float)x87_log10((long double)2.0f);
		long double x = ((long double)(rrn + 1.05f) - lm / l2)
			      * (1.0f / 6.0f);
		unsigned int n = (unsigned int)x;
		unsigned int frac = (unsigned int)((x - n) * 100.0f);
		unsigned int shift = 1;
		float p = 1.0f;
		unsigned int i;

		for (i = 0; i < n; i++)
			shift += shift;
		for (i = 0; i < frac; i++)
			p *= 1.0069555f;
		rrnUpMaxM = (unsigned int)((long double)p * shift);
	}

	failed = 0;
	if (calcK(rrnUpMaxM, mm) < rrn + 1.0f) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner:: maxM too high -"
			    " probably will not cause rate up\r\n");
		if (calcK(rrnUpMaxM + 1, mm) < rrn + 1.7f) {
			rrnUpMaxM++;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V90ConstellationDesigner:: increment maxM"
				    " for one rate up\r\n");
		} else {
			failed = 1;
		}
	}
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner:: current maxM = %d "
		    " rrn up maxM = %d\r\n",
		    mappingParams->constellationSize[phase], rrnUpMaxM);

	if (failed) {
		short_0e = short_0a;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner:: failed to find"
			    " rrnUpDmin => seting rrnUpDmin = dMin !!!\r\n");
	} else if (mappingParams->constellationSize[phase] < rrnUpMaxM) {
		unsigned char nofUcodes;
		short tempDmin;
		unsigned int nofIterations;

		nofUcodes = (unsigned char)
			    mappingParams->constellationSize[phase];
		tempDmin = short_0a;
		nofIterations = 0;
		while (nofUcodes < rrnUpMaxM && nofIterations <= 99) {
			tempDmin = (short)(tempDmin * 0.95f);
			nofUcodes = constelBuild(tempDmin, phase);
			nofIterations++;
		}

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner:: rrn up nofUcodes ="
			    " %d \r\n", nofUcodes);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner:: rrn up tempDmin  ="
			    " %d \r\n", tempDmin);

		/*
		 * THE FIRST STORE IS NOT DEAD, and that is what proves it is
		 * in the source: 0x48b4a writes `tempDmin` into `short_0e`,
		 * calls the diagnostic, and 0x487e0 then overwrites it with
		 * `dMin`.  A store to a member cannot be removed across an
		 * external call, so the compiler kept it on the arm that has
		 * one and sank it into 0x48ab9 on the arm that does not.
		 */
		short_0e = tempDmin;
		if (calcK(nofUcodes, mm) > rrn + 1.7f) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V90ConstellationDesigner:: tempDmin might"
				    " cause 2 rates up => rrnUpDmin = dMin"
				    "\r\n");
			short_0e = short_0a;
		}
	} else {
		short_0e = (short)(short_0a * 0.9f + 0.5f);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner:: maxM<pParams->m[phase]"
			    " seting rrnUpDmin arbitrary !!!\r\n");
	}
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner:: rrnUpDmin = %d\r\n", short_0e);
}

/*
 * ===========================================================================
 * setConstellationToNoise -- rebuild all six constellations for a measured
 * noise level, and set the three thresholds the connection evaluator judges
 * the line by.
 * ===========================================================================
 *
 * 3,641 bytes, and rather more than half of them are diagnostics: sixteen
 * `dsplibs_debug_printf` sites, each its own `DSPLIB_DEBUG_ON()` -- sixteen
 * reads of `dsplibs_debug_level` for sixteen calls -- and SIX
 * `edprintf` sites that are not gated at all.  Which of the two a site uses
 * is not decoration -- `edprintf` encodes and then applies the gate itself,
 * so its six run their encoding at every level including zero -- and the
 * transcript test sees the difference, so the split below is measured
 * against the relocations rather than chosen.
 *
 * WHAT IT RETURNS IS NOTHING, and the header carries the argument: the two
 * `ret` paths leave `dsplibs_debug_level` and `dsplibs_debug_printf`'s return
 * in %eax, which are not one quantity.
 *
 * THE TWO NaNs ARE THE FIRST TWO INSTRUCTIONS, before the prologue: `flds`
 * twice of `.rodata.cst4`'s 0x7fc00000.  They are the two float locals the
 * "dMinHighRates / dMinLowRates" diagnostic prints, and the restricted arm
 * discards both with `fstp; fstp` because it overwrites them.  On the
 * unrestricted arm they survive, and the diagnostic prints (short)NaN, which
 * is -32768 on both sides.  Any quiet NaN behaves identically here, so this
 * is the encoding's evidence and not the test's.
 *
 * `USE_RESTRICED_DMIN` IS SPELLED THE PARAMETER'S WAY, missing its T.  The
 * name is `tools/vparse.py`'s, out of the configuration file's own table, and
 * the format string at 0xc534 misspells it identically -- so the typo is the
 * author's and correcting it here would lose the correspondence.
 *
 * THE THRESHOLD SEED IS DIVIDED, NOT SHIFTED, and its sibling is shifted:
 * `constelBuild` opens with a bare `sar $1` on a promoted `short` and this
 * function opens both of its inner loops with `shr $0x1f; lea; sar $1`, which
 * is the round-toward-zero sequence GCC emits for `/ 2` and not for `>> 1`.
 * Two loops of the same shape, spelled two ways by the same author.  The two
 * readings differ only on a NEGATIVE ODD seed, so the test sweeps one.
 *
 * THE STAGING BUFFER IS 128 BYTES AND NOTHING BOUNDS THE COUNT.  The accepted
 * indices go into a local array and the loop runs from `params->unnamed_360`
 * to `arg5[k]`, a byte, so a bound of 255 against a start of 0 can accept 256
 * entries into 128 bytes.  The object smashes its own frame; D333 records it
 * and the test stays inside the array.
 */
void
V90ConstellationDesigner::setConstellationToNoise(float noiseEnergy,
						  short (*ucode)[128],
						  short (*alt)[128],
						  short *dmin,
						  unsigned char *lastUcode,
						  unsigned char (*allow)[128])
{
	float dMinHighRates = __builtin_nanf("");
	float dMinLowRates = __builtin_nanf("");
	float retrainFactor;
	short keptDmin = short_0a;
	unsigned char picked[128];
	unsigned int nofPicked;
	unsigned short maxM;
	unsigned int k;
	unsigned int i;

	/*
	 * `keptDmin` above is read BEFORE either arm writes `short_0a`, and
	 * that is what the KeepRate case restores.  The object keeps it in
	 * %ebx from 0x48b96 all the way to 0x4946c.
	 */
	if (params->USE_RESTRICED_DMIN) {
		dMinHighRates = noiseEnergy * 3.3330500f + 80.0f;
		dMinLowRates = noiseEnergy * 8.2135878f + 12.0f;
		short_0a = (short)(dMinLowRates >= dMinHighRates
				   ? dMinLowRates : dMinHighRates);
		retrainFactor = 2.0f;
	} else {
		short_0a = (short)(noiseEnergy * 6.7762098f + 9.9f);
		retrainFactor = 4.0f;
	}

	/*
	 * The fixed-point printer, four times over and inline each time.  The
	 * sign is `sbb %ecx,%ecx; and $-2,%ecx; add $0x2d,%ecx` off an
	 * ORDERED compare of 0.0f against the value, so it is '+' only when
	 * the value is strictly positive and '-' at zero.  The magnitude is
	 * `(int)fabs`, and the hundredths come off the SIGNED remainder and
	 * are made positive with `__builtin_abs` (findings 2116-2117) rather
	 * than with a ternary.
	 *
	 * AND THE SCALE IS A FLOAT HERE AND A DOUBLE IN THE OTHER THREE:
	 * `flds .rodata.cst4+0x448` at 0x48c5c against `fldl
	 * .rodata.cst8+0x150` at 0x48dfb and 0x48eab.  100 is exact in both,
	 * so the two spellings cannot differ in any digit -- this is measured
	 * and kept, not tidied.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner: noiseEnergy = %c%d.%02d\r\n",
		    (0.0f < noiseEnergy) ? '+' : '-',
		    (int)__builtin_fabsf(noiseEnergy),
		    __builtin_abs((int)((noiseEnergy
					 - (float)(int)noiseEnergy) * 100.0f)));
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner: dMinHighRates = %d "
		    " dMinLowRates = %d\r\n",
		    (short)dMinHighRates, (short)dMinLowRates);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "V90ConstellationDesigner: current dMin = %d\r\n",
		    short_0a);

	/*
	 * FOUR CASES AND NO DEFAULT, and the object's decision tree is what
	 * says so: `cmp $1; je / jle -> test for 0 / cmp $2; je / cmp $3; je`
	 * is a balanced tree over {0,1,2,3}, and a `word_48` of 4 or more --
	 * or a negative one -- falls out of the switch having written
	 * nothing.  Three of the four end by writing the three thresholds and
	 * KeepRate is the one that does not, which is exactly what its own
	 * diagnostic claims: "keep dMin and pdsnr thresh".
	 *
	 * The three threshold stores are written out three times rather than
	 * factored, because the object has three copies and GCC 3.4.2 at
	 * these flags does not inline an extern member (finding 2163) -- a
	 * helper would leave three calls and a symbol the blob has no
	 * counterpart for.
	 */
	switch (word_48) {
	case 1:
		short_0a = keptDmin;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner: KeepRate => keep dMin"
			    " and pdsnr thresh\r\n");
		break;

	case 2: {
		/*
		 * One rate up wants the SMALLER of the two minimum distances,
		 * and a zero `rrnUpDmin` means there is nothing to take.  The
		 * condition code is what fixes the sense: `cmp %dx,%ax; jle`
		 * keeps `short_0e` when it is less than OR EQUAL, so the
		 * source's test is on the other one being greater.
		 */
		short d;

		if (short_0e != 0)
			d = (short_0e > short_0a) ? short_0a : short_0e;
		else
			d = short_0a;
		short_0a = d;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner: dMin calc OneRateUp\r\n");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner: rrnUpDmin = %d\r\n",
			    short_0e);
		float_18 = noiseEnergy * 0.45f;
		float_1c = noiseEnergy * 1.4125f;
		float_20 = retrainFactor * noiseEnergy;
		break;
	}

	case 3: {
		/* And one rate down wants the larger; `jge` where the other
		 * arm has `jle`, and that one condition code is the whole
		 * difference between the two bodies. */
		short d;

		if (short_0c != 0)
			d = (short_0c < short_0a) ? short_0a : short_0c;
		else
			d = short_0a;
		short_0a = d;
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner: dMin calc"
			    " OneRateDown\r\n");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner: rrnDownDmin = %d\r\n",
			    short_0c);
		float_18 = noiseEnergy * 0.45f;
		float_1c = noiseEnergy * 1.4125f;
		float_20 = retrainFactor * noiseEnergy;
		break;
	}

	case 0:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "V90ConstellationDesigner: dMin calc"
			    " NoRestriction\r\n");
		float_18 = noiseEnergy * 0.45f;
		float_1c = noiseEnergy * 1.4125f;
		float_20 = retrainFactor * noiseEnergy;
		/*
		 * THE ONLY CLAMP IN THE FUNCTION, and its window is open at
		 * the bottom and closed at the top: `cmp $0x43; jg` first and
		 * `cmp $0x3e; jle` second, both SIGNED 16-bit, so the source
		 * tests the upper bound first.
		 */
		if (word_24 == 0 && short_0a <= 0x43 && short_0a > 0x3e) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
				    "V90ConstellationDesigner: adjusting dMin"
				    " for rate>=53k. Orig dMin=%d Modified"
				    " dMin=%d\r\n", short_0a, 0x3e);
			short_0a = 0x3e;
		}
		break;
	}

	/*
	 * Whatever the arm decided, the next call is a KeepRate one.  The
	 * 1.25f is applied to the member and not to a local: 0x49492 reloads
	 * `movzwl 0xa(%ecx)` after the KeepRate diagnostic's call, which a
	 * local would have spilled to the stack instead.
	 */
	word_48 = 1;
	short_10 = (short)(short_0a * 1.25f);

	/*
	 * The configuration file can pin dMin, and -1 is how it says it does
	 * not: `cmp $0xffffffff,%eax; jle`.  The store is 16 bits wide off a
	 * 32-bit parameter, so a forced value above 0x7fff arrives truncated.
	 */
	if (params->FORCED_DMIN > -1) {
		short_0a = (short)params->FORCED_DMIN;
		edprintf("V90ConstellationDesigner: dMin Forced to: %d\r\n",
			 short_0a);
	}
	edprintf("V90ConstellationDesigner: final dMin = %d\r\n", short_0a);
	edprintf("V90ConstellationDesigner: USE_RESTRICED_DMIN = %d\r\n",
		 params->USE_RESTRICED_DMIN);
	edprintf("V90ConstellationDesigner: pdSnrThreshForRateUp ="
		 " %c%d.%02d\r\n",
		 (0.0f < float_18) ? '+' : '-', (int)__builtin_fabsf(float_18),
		 __builtin_abs((int)((float_18 - (float)(int)float_18)
				     * 100.0)));
	edprintf("V90ConstellationDesigner: pdSnrThreshForRateDown ="
		 " %c%d.%02d\r\n",
		 (0.0f < float_1c) ? '+' : '-', (int)__builtin_fabsf(float_1c),
		 __builtin_abs((int)((float_1c - (float)(int)float_1c)
				     * 100.0)));
	edprintf("V90ConstellationDesigner: pdSnrThreshForRetrain ="
		 " %c%d.%02d\r\n",
		 (0.0f < float_20) ? '+' : '-', (int)__builtin_fabsf(float_20),
		 __builtin_abs((int)((float_20 - (float)(int)float_20)
				     * 100.0)));

	/*
	 * ------------------------------------------------------------------
	 * The build itself: six constellations, one pass each.
	 * ------------------------------------------------------------------
	 *
	 * TWO INNER LOOPS OF THE SAME SHAPE, chosen by `dmin[k]`, which is
	 * the same discriminator and the same argument `findNextUcodeToAdd`
	 * takes.  They differ in two things and not one: the non-zero arm
	 * seeds its threshold from `short_10` and requires BOTH tables to
	 * clear it, the zero arm seeds from `short_0a` and looks at the first
	 * table only.
	 *
	 * THE INDICES COME OUT BACKWARDS.  The staging buffer is filled
	 * forwards and read `0xbf(%esp,%edi,1)` with %edi = count - i, so
	 * `constellation[k][0]` is the LAST index accepted.
	 */
	for (k = 0; k <= 5; k++) {
		nofPicked = 0;
		if (dmin[k] != 0) {
			short thresh = (short)(short_10 / 2);

			for (i = params->unnamed_360; i <= lastUcode[k]; i++) {
				short a = ucode[k][i];

				if (a > thresh) {
					short b = alt[k][i];

					if (b > thresh && allow[k][i] != 0) {
						picked[nofPicked++] =
						    (unsigned char)i;
						thresh = (short)
						    ((b >= a ? b : a)
						     + short_10);
					}
				}
			}
		} else {
			short thresh = (short)(short_0a / 2);

			for (i = params->unnamed_360; i <= lastUcode[k]; i++) {
				short a = ucode[k][i];

				if (a > thresh && allow[k][i] != 0) {
					picked[nofPicked++] = (unsigned char)i;
					thresh = (short)(a + short_0a);
				}
			}
		}

		mappingParams->constellationSize[k] = nofPicked;
		for (i = 0; i < nofPicked; i++) {
			mappingParams->constellation[k][i] =
			    picked[nofPicked - 1 - i];
			/*
			 * THE `+0x28` COMPARISON IS THE LEAST OBVIOUS THING
			 * HERE.  When the two slots agree the codec byte is
			 * computed and then compared against the constellation
			 * byte it came from, and the SMALLER of the two is
			 * stored -- `cmp %esi,%edx; jge` on two `unsigned
			 * char`s, which promote to `int` and therefore compare
			 * signed.  When they disagree the codec byte is stored
			 * whatever it is.
			 *
			 * The winning expression is spelled out a second time
			 * rather than kept in the local: the object recomputes
			 * it from `mappingParams->constellation[k][i]` at
			 * 0x49275 where the test used the local at 0x4921f,
			 * which is what a copy of the simple arm below looks
			 * like once the compiler has it.  Nothing writes the
			 * byte in between, so the two readings cannot differ.
			 */
			if (word_2c == word_28) {
				unsigned char c =
				    mappingParams->constellation[k][i];
				unsigned char e;

				if (word_2c != 0)
					e = (unsigned char)
					    (linear2alaw(__builtin_abs(
						(int)ucode[k][c])) ^ 0xd5);
				else
					e = (unsigned char)
					    ~linear2ulaw(__builtin_abs(
						(int)ucode[k][c]));

				if (e < c) {
					unsigned char d =
					    mappingParams->constellation[k][i];

					if (word_2c != 0)
						mappingParams
						  ->codecConstellation[k][i] =
						    (unsigned char)
						    (linear2alaw(
							__builtin_abs(
							 (int)ucode[k][d]))
						     ^ 0xd5);
					else
						mappingParams
						  ->codecConstellation[k][i] =
						    (unsigned char)
						    ~linear2ulaw(
							__builtin_abs(
							 (int)ucode[k][d]));
				} else {
					mappingParams
					  ->codecConstellation[k][i] =
					    mappingParams
					      ->constellation[k][i];
				}
			} else {
				unsigned char c =
				    mappingParams->constellation[k][i];

				if (word_2c != 0)
					mappingParams
					  ->codecConstellation[k][i] =
					    (unsigned char)
					    (linear2alaw(__builtin_abs(
						(int)ucode[k][c])) ^ 0xd5);
				else
					mappingParams
					  ->codecConstellation[k][i] =
					    (unsigned char)
					    ~linear2ulaw(__builtin_abs(
						(int)ucode[k][c]));
			}
		}
	}

	/*
	 * ------------------------------------------------------------------
	 * And the report, which is the rest of the function.
	 * ------------------------------------------------------------------
	 *
	 * `maxM` IS SIXTEEN BITS WIDE off a 32-bit field, and the object says
	 * so twice: `movzwl 0x604(%edi),%esi` for the seed and `movzwl
	 * %ax,%esi` for the update, either side of a 32-bit UNSIGNED compare.
	 * Nothing here can reach a size above 256 -- the count is bounded by
	 * the staging buffer -- so the truncation is unreachable from this
	 * function and is recorded rather than driven.
	 */
	maxM = mappingParams->constellationSize[0];
	for (k = 1; k <= 5; k++)
		if (mappingParams->constellationSize[k] > maxM)
			maxM = mappingParams->constellationSize[k];

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "\n------------------------------------------------------------"
		    "-----------------------------------------------------\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90 Constellation Designer report:\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "constelation size phase[0..5]  :  %d  %d  %d  %d  %d"
		    "  %d\r\n",
		    mappingParams->constellationSize[0],
		    mappingParams->constellationSize[1],
		    mappingParams->constellationSize[2],
		    mappingParams->constellationSize[3],
		    mappingParams->constellationSize[4],
		    mappingParams->constellationSize[5]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "----------- constel phase 0,1,2,3,4,5\t"
		    " codec constel phase 0,1,2,3,4,5"
		    "\tlinearMapping of constel --------------\r\n");

	/*
	 * THE GATE IS INSIDE THE LOOP and the loop is not inside the gate:
	 * 0x49310 re-reads `dsplibs_debug_level` every turn, which is what a
	 * `DSPLIB_DEBUG_ON()` in the body compiles to once the call between
	 * turns can change it.  Eighteen `%d` from three tables, the third of
	 * them the linear sample each constellation entry names.
	 */
	for (i = 0; i < maxM; i++)
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
			    "ucode[%d]  :  %d  %d  %d  %d  %d  %d  :  %d  %d"
			    "  %d  %d  %d  %d  :  %d  %d  %d  %d  %d  %d\r\n",
			    i,
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
		dsplibs_debug_printf(
		    "------------------------------------------------------------"
		    "---------------------------------------------------------\r\n");
}
