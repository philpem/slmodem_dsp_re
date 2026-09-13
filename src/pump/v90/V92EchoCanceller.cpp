/*
 * V92EchoCanceller.cpp -- moving the echo canceller's delay, clearing it, and
 * tearing it down.
 *
 * Reconstructed from dsplibs.o.  Seven of the class's twelve members: the
 * constructor, `setEchoDelay`, which is the one `v34handshak` reaches,
 * `reset`, the destructor, and -- below the second banner -- `setState`,
 * `updateEchoHistory` and `process`, which are the state machine and the two
 * signal paths.  `include/dsplib/V92EchoCanceller.h` carries the object map,
 * the four states, and the argument that +0x2c is a tap count rather than a
 * pointer.
 *
 * `reset` AND `~V92EchoCanceller` ARE BOTH 195 BYTES AND ARE NOT THE SAME
 * CODE.  Two functions of one class with one size is worth checking rather
 * than assuming, and the check says coincidence: `reset` opens `push %ebx;
 * xor %eax,%eax; sub $0x18,%esp` and ends in `jmp FloatARMA::reset`, while
 * the destructor opens `sub $0xc,%esp; cmpl $0x1,dsplibs_debug_level` and is
 * three conditional frees.  What IS duplicated is `D1` and `D2`, which are
 * byte-identical to each other 0xd0 apart -- and that pair is what a single
 * C++ destructor body produces for a class with no virtual bases, so one
 * definition here is right and no second spelling is needed.  Finding F1270.
 *
 * PLAIN CDECL, `this` as the first STACK argument (finding F215).  This one
 * has no frame at all: `mov 0x4(%esp),%eax` is the whole prologue.
 *
 * THE OLD DELAY IS READ BEFORE IT IS OVERWRITTEN, which is the only ordering
 * constraint in the function.  The object computes `newDelay - oldDelay` into
 * `%ecx`, stores the new delay, and only then folds `%ecx` into the tap
 * count; the two spellings that produce that -- taking a copy of the old
 * value first, or writing the `+=` before the assignment -- are the same
 * code, because +0x2c and +0x38 are distinct members of one object and GCC
 * knows they cannot alias.  Written in the order that needs no temporary.
 *
 * THE DIAGNOSTIC IS A TAIL CALL.  `jmp edprintf`, not `call`, with the
 * arguments written into the caller's own outgoing slots -- which is what
 * `edprintf` being the last statement of a `void` function compiles to.  It
 * prints the argument, not the field, but the two are equal by then.
 */

#include <stddef.h>
#include <math.h>

extern "C" {
#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/sysdep.h"
}

#include "dsplib/FloatARMA.h"
#include "dsplib/V92EchoCanceller.h"
#include "dsplib/V92Parameters.h"
#include "dsplib/vpcm_tables.h"

/* See V90ConstellationDesigner.cpp for why these are here and why guarded. */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define V92EC_OFF(field, off, tag) \
	typedef char v92ec_off_##tag[ \
	    ((int)__builtin_offsetof(V92EchoCanceller, field) == (off)) \
	    ? 1 : -1]

V92EC_OFF(params,       0x00, params);
V92EC_OFF(arma,         0x04, arma);
V92EC_OFF(state,        0x08, state);
V92EC_OFF(updateDuration, 0x0c, updateduration);
V92EC_OFF(word_10,      0x10, word10);
V92EC_OFF(filterLength, 0x14, filterlength);
V92EC_OFF(word_18,      0x18, word18);
V92EC_OFF(historyAlloc, 0x1c, historyalloc);
V92EC_OFF(echoCoeff,    0x20, echocoeff);
V92EC_OFF(echoHistory,  0x24, echohistory);
V92EC_OFF(historyIndex, 0x28, historyindex);
V92EC_OFF(echoLength,   0x2c, echolength);
V92EC_OFF(echoBeta,     0x30, echobeta);
V92EC_OFF(echoBetaDecay, 0x34, echobetadecay);
V92EC_OFF(echoDelay,    0x38, echodelay);
typedef char v92ec_size[(sizeof(V92EchoCanceller) == 0x3c) ? 1 : -1];

/* The three fields of the parameter block this file reads. */
typedef char v92ec_off_delayoffset[
    ((int)__builtin_offsetof(V92Parameters, V92_ECHO_DELAY_OFFSET) == 0x74)
    ? 1 : -1];
typedef char v92ec_off_filterlength[
    ((int)__builtin_offsetof(V92Parameters, V92_ECHO_FILTER_LENGTH) == 0x6c)
    ? 1 : -1];
typedef char v92ec_off_initialdelay[
    ((int)__builtin_offsetof(V92Parameters, V92_ECHO_INITIAL_DELAY) == 0x70)
    ? 1 : -1];

/* And the six `setState` reads: three fast, three slow. */
typedef char v92ec_off_fastbeta[
    ((int)__builtin_offsetof(V92Parameters, V92_ECHO_FAST_BETA_FACTOR) == 0x78)
    ? 1 : -1];
typedef char v92ec_off_fastdecay[
    ((int)__builtin_offsetof(V92Parameters, V92_ECHO_FAST_DECAY_FACTOR) == 0x7c)
    ? 1 : -1];
typedef char v92ec_off_slowbeta[
    ((int)__builtin_offsetof(V92Parameters, V92_ECHO_SLOW_BETA_FACTOR) == 0x80)
    ? 1 : -1];
typedef char v92ec_off_slowdecay[
    ((int)__builtin_offsetof(V92Parameters, V92_ECHO_SLOW_DECAY_FACTOR) == 0x84)
    ? 1 : -1];
typedef char v92ec_off_fastdur[
    ((int)__builtin_offsetof(V92Parameters, V92_ECHO_FAST_UPDATE_DURATION)
     == 0x88) ? 1 : -1];
typedef char v92ec_off_slowdur[
    ((int)__builtin_offsetof(V92Parameters, V92_ECHO_SLOW_UPDATE_DURATION)
     == 0x8c) ? 1 : -1];
#endif

/*
 * The ARMA behind the canceller, and its two coefficient arrays.  These ARE
 * the reference's `v92echoPreFilter_b` (.data+0x320) and `v92echoPreFilter_a`
 * (.data+0x360), LOCAL in the object and moved in here from the invented
 * `vpcm_tables.c`; this constructor is their only consumer, so they are
 * `static`.
 *
 * .data and NOT .rodata: the constructor hands both to `FloatARMA::FloatARMA`,
 * whose coefficient parameters are `float *`, so a `const` here would be
 * neither the object's type nor a thing that compiles.  Twelve entries each,
 * and the relocations are against the .data SECTION symbol with the offset as
 * an inline addend, which is what makes them file-local (finding F604).
 *
 * `den[0]` IS EXACTLY 1.0f, which decides an arm inside `FloatARMA`:
 * its constructor rescales both arrays by `1.0f / den[0]` only when
 * `den[0] != 1.0f`, so on this path the coefficients are copied and not
 * scaled, and `m_a[0]` is then cleared.  Worth knowing before reading the
 * test's expectations for `m_a`.
 *
 * Every literal is the shortest decimal that reproduces the blob's four
 * bytes, and the test compares the arrays `FloatARMA` copied them into
 * against the blob's, in full, on every trial.
 */
static float v92echoPreFilter_b[V92_ECHO_PREFILTER_TAPS] = {
	1.49860001f, 0.621999979f, -2.73329997f, -2.60159993f, 1.71739995f,
	3.37879992f, 0.135000005f, -1.75409997f, -0.682799995f, 0.350800008f,
	0.221100003f, 0.00579999993f,
};

static float v92echoPreFilter_a[V92_ECHO_PREFILTER_TAPS] = {
	1.0f, 0.708999991f, -2.30019999f, -2.67490005f, 1.53729999f,
	3.62899995f, 0.467999995f, -2.13840008f, -1.00730002f, 0.423099995f,
	0.344900012f, 0.0285f,
};

/*
 * `FloatARMA` IS BUILT WITH ORDINARY PLACEMENT `new`.  It calls
 * `sysdep_malloc(0x34)` and then the C1 constructor on the result, with NO
 * null test between them -- which is what `new` compiles to over an allocator
 * hooked to `sysdep_malloc`, and this build is `-nostdinc++` with no <new>, so
 * `include/dsplib/sysdep.h` declares the shared non-throw placement `operator
 * new`/`operator delete` pair every such site in this tree uses.  This file
 * used to reach the constructor through a hand-mangled
 * `asm("_ZN9FloatARMAC1EjjPfS0_j")` label, on the belief (finding F1340) that
 * a user-declared placement `operator new` would force a null test the blob
 * does not have; finding F10155 retracts that empirically and F10157 proves
 * the mechanism end-to-end.  The destructor's existing
 * `arma->~FloatARMA(); sysdep_free(arma)` needed no trick at all and is
 * unchanged.
 */

/*
 * The canceller's whole construction: one diagnostic, the initial delay, the
 * filter length, three allocations and a reset.
 *
 * THE ALLOCATION SIZES ARE THE WHOLE POINT, and D72 is about this function.
 * `historyAlloc` is computed here, stored at +0x1c, and multiplied by four to
 * size `echoHistory`; NOTHING reallocates it afterwards, and `setEchoDelay`
 * moves `echoLength` -- the bound every consumer clears and reads to --
 * without reference to it.  D72 is the entry, finding F1188 the numbers and
 * the CANNOT FIRE verdict; this file neither clamps nor re-argues it.
 *
 * `setEchoDelay` IS CALLED, NOT INLINED, and the object inlined it.  The
 * instruction stream at +0x32..+0x4d is that method's body verbatim, down to
 * the diagnostic, and GCC at -O2 would not inline a non-`inline` external
 * function -- so the original's source and ours differ in factoring here and
 * agree in behaviour, which is CLAUDE.md's sanctioned case.  Writing it as a
 * call rather than a copy is also what keeps `v92ec`'s anchors unique
 * (finding F1264): the same three statements twice in one file would put half
 * the suite's `find` strings on two occurrences each.
 *
 * IT READS TWO MEMBERS BEFORE ANYTHING HAS WRITTEN THEM.  `echoLength +=
 * delay - echoDelay` runs on whatever the allocation held, and the object
 * does the same -- `mov 0x38(%esi),%ecx` at +0x35 is a load of uninitialised
 * storage.  The result is DEAD: `reset()`, the last thing this constructor
 * does, rebuilds `echoLength` from `filterLength`, `echoDelay` and the
 * parameter block, and `historyAlloc` is built from +0x18 and +0x38 and never
 * from +0x2c.  It is reproduced because it is in the object, and recorded
 * here because it is the kind of thing a later reader "cleans up".  D225.
 * Reproducing it also depends on `-fno-lifetime-dse`, which is in CXXFLAGS
 * for finding F1224's reason: without it the compiler is entitled to treat the
 * pre-constructor contents of `*this` as unreachable.
 *
 * THE FILTER LENGTH IS A SIGNED DIVIDE, not a mask.  `test %eax,%eax; js;
 * add $0x3; and $0xfffffffc` is `x / 4 * 4` on an `int`, which rounds toward
 * zero; the mask D72 and finding F1188 write rounds toward minus infinity.
 * They agree at the shipped 180 and differ for every negative value, and the
 * test cannot tell them apart because a negative length makes the very next
 * `sysdep_malloc` a request for 16 GB.  Recorded, not driven -- finding F1312.
 */
V92EchoCanceller::V92EchoCanceller(V92Parameters *parameters,
				   unsigned int blockLen, unsigned int extra)
{
	int length;
	unsigned int span;
	FloatARMA *m;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92EchoCanceller: constraction\r\n");

	params = parameters;
	setEchoDelay((unsigned int)params->V92_ECHO_INITIAL_DELAY);

	length = params->V92_ECHO_FILTER_LENGTH / 4 * 4;
	word_18 = (unsigned int)length - 1u;
	filterLength = (unsigned int)length;
	edprintf("V92EchoCanceller: echoFilterLen = %d\r\n", filterLength);

	echoCoeff = (float *)sysdep_malloc(filterLength * sizeof(float));

	span = word_18 + echoDelay;
	historyAlloc = span + 2u * blockLen + span / blockLen * blockLen
		       + extra;
	echoHistory = (float *)sysdep_malloc(historyAlloc * sizeof(float));

	m = (FloatARMA *)sysdep_malloc(sizeof(FloatARMA));
	new (m) FloatARMA(12u, 12u, v92echoPreFilter_a, v92echoPreFilter_b, 99u);
	arma = m;

	reset();
}

void
V92EchoCanceller::setEchoDelay(unsigned int delay)
{
	echoLength += delay - echoDelay;
	echoDelay = delay;

	edprintf("V92EchoCanceller: echoDelay updated to: %d\n", delay);
}

/*
 * zeroEchoCoeff -- 0x10f60, 45 bytes: the coefficient bank to zero, bound
 * cached in a register (strict aliasing lets a float store not clobber an
 * `unsigned int` member, so `i < filterLength` compiles to the object's
 * single load).  The identical loop opens `reset` below; this is the
 * out-of-line copy the blob also emits, claimed at last.
 */
void
V92EchoCanceller::zeroEchoCoeff()
{
	unsigned int n;

	for (n = 0; n < filterLength; n++)
		echoCoeff[n] = 0.0f;
}

/*
 * resetEchoHistory -- 0x10f90, 62 bytes, immediately after `zeroEchoCoeff`
 * and immediately before `reset` in the blob as here: the write cursor to
 * zero, the length rebuilt from the three terms (V92EchoCanceller.h walks
 * the arithmetic), and the history cleared to that length.  D72's unclamped
 * bound, exactly as in `reset` below.
 *
 * THE IDENTIFIERS DIFFER FROM `reset`'s ON PURPOSE (`n` for `i`, `blk` for a
 * bare `params->`), in both members above: the mutation suite anchors on
 * `reset`'s exact text and an anchor must match exactly once (`make refs`).
 * An identifier is not a codegen carrier, so nothing else moves.
 */
void
V92EchoCanceller::resetEchoHistory()
{
	V92Parameters *blk = params;
	unsigned int n;

	historyIndex = 0;
	/* the three-term rebuild; the header walks the arithmetic */
	echoLength = echoDelay + (filterLength >> 1)
		     + (unsigned int)blk->V92_ECHO_DELAY_OFFSET;
	for (n = 0; n < echoLength; n++)
		echoHistory[n] = 0.0f;
}

/*
 * Clear the canceller: both buffers, the write cursor, the tap count, the two
 * betas and the ARMA behind it.
 *
 * FOUR OF THIS CLASS'S OWN MEMBERS ARE INLINED HERE, AND THE FACTORING IS
 * RECORDED RATHER THAN REPRODUCED.  The first loop below is byte-for-byte the
 * body of `zeroEchoCoeff` (blob 0x10f60, 45 B); the block after it is
 * byte-for-byte the body of `resetEchoHistory` (0x10f90, 62 B), down to the
 * order of the `historyIndex` store and the `echoLength` recomputation; and
 * the two store-and-print pairs are `setEchoBeta(0.0f)` (0x10cc0) and
 * `setDecayFactor(0.0f)` (0x10d60) with their argument constant-folded --
 * `setEchoBeta` prints the sign of the FIELD it has just stored, so a zero
 * argument folds `fldz; fcomps 0x30(%ecx)` to false and the character to
 * `'-'`, and its `(int)fabs` and fractional terms to zero.  That is exactly
 * the `$0x2d, $0, $0` the object passes.
 *
 * TWO OF THE FOUR ARE NOW WRITTEN, just above -- `zeroEchoCoeff` and
 * `resetEchoHistory` have their own out-of-line definitions, claimed at
 * last -- and the inlined spelling HERE still stands: GCC inlined them in
 * the original too, so the emitted code is the same either way, and
 * CLAUDE.md's rule that a different factoring may differ for ever while
 * behaving identically is what makes that a choice rather than a compromise.
 * Finding F1271.  (`setEchoBeta(0.0f)` and `setDecayFactor(0.0f)` remain
 * unwritten and constant-folded, as the paragraph above describes.)
 *
 * THE SECOND LOOP IS D72's, AND THIS FILE DOES NOT CLAMP IT.  `echoLength` is
 * rebuilt from `filterLength`, `echoDelay` and the parameter block and used
 * as the bound on `echoHistory` with no reference to what that buffer was
 * allocated with.  D72 is the entry; it is CONFIRMED present and CANNOT FIRE
 * at any real `V92_ECHO_INITIAL_DELAY`, and the test that covers this
 * function puts a compared guard past the end of both buffers so that a
 * spelling which overran would fail rather than pass quietly.
 */
void
V92EchoCanceller::reset()
{
	unsigned int i;

	for (i = 0; i < filterLength; i++)
		echoCoeff[i] = 0.0f;

	historyIndex = 0;
	echoLength = (filterLength >> 1) + echoDelay
		     + (unsigned int)params->V92_ECHO_DELAY_OFFSET;
	for (i = 0; i < echoLength; i++)
		echoHistory[i] = 0.0f;

	state = V92_ECHO_FILTER_ONLY;

	echoBeta = 0.0f;
	edprintf("V92EchoCanceller: echoBeta = %c%d.%06d\r\n", '-', 0, 0);

	echoBetaDecay = 0.0f;
	edprintf("V92EchoCanceller: echoBetaDecay = %c%d.%06d\r\n", '-', 0, 0);

	arma->reset();
}

/*
 * Three owned blocks, each freed only if non-null and each nulled afterwards.
 *
 * THE GATE IS THE CALL SITE'S, NOT `edprintf`'s.  This is the one diagnostic
 * in the class that goes through `dsplibs_debug_printf` directly, so the
 * object tests `dsplibs_debug_level` itself -- `cmpl $0x1,...; ja` -- and the
 * message is plain text rather than encoded.  Every other diagnostic here is
 * an ungated `edprintf`, which self-gates one level down.
 *
 * THE ARMA IS DESTROYED AND THEN FREED SEPARATELY, which is what an explicit
 * destructor call followed by `sysdep_free` compiles to and is NOT what
 * `delete` compiles to.  Two calls, the same pointer in `%ebx` for both.
 *
 * THE REASON USED TO BE "`delete` WOULD CALL `operator delete` AND THE OBJECT
 * CALLS `sysdep_free`", AND THAT INFERENCE IS WITHDRAWN -- finding F7786.  This
 * codebase REPLACES global `operator delete`, and the replacement inlines to
 * `sysdep_free`, so a `delete` here would also have reached `sysdep_free` and
 * the callee's name settles nothing.  The blob's own compiler-generated `D0Ev`
 * destructors prove it: they tail-call `sysdep_free` where a library
 * `operator delete` would have made them call `_ZdlPv`, and the object defines
 * and references no `_Znwj`, `_Znaj`, `_ZdlPv` or `_ZdaPv` anywhere.
 *
 * WHAT STILL CARRIES THE READING IS THE SHAPE, not the name: `delete p` emits
 * ONE call after a front-end null test, and the object emits TWO -- the
 * destructor and then the free -- which no spelling of `delete` produces.
 */
V92EchoCanceller::~V92EchoCanceller()
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92EchoCanceller Destruction\r\n");

	if (echoCoeff != NULL) {
		sysdep_free(echoCoeff);
		echoCoeff = NULL;
	}
	if (echoHistory != NULL) {
		sysdep_free(echoHistory);
		echoHistory = NULL;
	}
	if (arma != NULL) {
		arma->~FloatARMA();
		sysdep_free(arma);
		arma = NULL;
	}
}

/*
 * ==========================================================================
 * The state machine and the two signal paths.
 * ==========================================================================
 */

/*
 * The three arguments every float this class prints is broken into.
 *
 * The object formats its own fixed-point decimal and hands `edprintf` a
 * character and two ints; the same shape as `V90Phase2Info`'s, and written
 * the same way for the reason finding F256 gives -- the products are computed
 * on the x87 stack at 64 significand bits and `long double` is the spelling
 * that does not depend on the excess precision being there.
 *
 * THE SIGN IS `!(v <= 0.0f)` AND NOT `0.0f < v`, and the difference is a NaN.
 * `fldz; fcomps v; sahf; sbb %edx,%edx; and $0xfffffffe,%edx; add $0x2d,%edx`
 * selects on CF alone, CF is C0, and FCOM sets C0 for LESS-THAN and for
 * UNORDERED both -- so the object prints '+' for a NaN coefficient where
 * `0.0f < v` prints '-'.  Zero prints as '-'.  Same reading as
 * `V90Equalizer::setLinearEquBeta`, and the coefficient dump below is fed
 * whatever the adaptation left behind, so it is reachable.
 */
static char
sign_of(float v)
{
	return !(v <= 0.0f) ? '+' : '-';
}

/* `fld %st(0); fabs; fistpl` with the control word set to truncate. */
static int
whole_of(float v)
{
	return (int)fabsf(v);
}

/*
 * `fildl` the truncation back, subtract it from the value, scale by 1e6,
 * truncate again, and `cltd; xor %edx,%eax; sub %edx,%eax` -- which is abs().
 * The subtraction is `v - (int)v` here, the way round that keeps the value's
 * sign; the abs() makes the order untestable either way (finding F256).
 */
static int
frac_of(float v)
{
	return __builtin_abs((int)(((long double)v - (long double)(int)v)
				   * 1.0e6f));
}

/*
 * print_echo_coeffs -- 0x10a30, 179 bytes, the FIRST symbol of this TU in
 * the blob (it precedes `setEchoDelay` at 0x10af0).  A free function with a
 * mangled name (`_Z17print_echo_coeffsPfj`), not a member: the banner, then
 * one fixed-point line per coefficient through the same char-and-two-ints
 * split as every float this class prints -- `%06d`, so the 1e6 scale the
 * three helpers above already carry.  `unsigned int` for the count is the
 * mangling's `j`, and the loop compares `jb`, unsigned, to match.
 *
 * It sits here AFTER the helpers only because they are file-static; the
 * emission-order cost of not leading the TU with it is one function's
 * register allocation, and the differential tier cannot see it.
 */
void
print_echo_coeffs(float *coeffs, unsigned int len)
{
	unsigned int i;

	edprintf("**************** NEAR ECHO FILTER *************** \n");
	for (i = 0; i < len; i++)
		edprintf("   = %c%d.%06d\r\n", sign_of(coeffs[i]),
			 whole_of(coeffs[i]), frac_of(coeffs[i]));
}

/*
 * `V92EchoCanceller::setEchoBeta(float)` and
 * `V92EchoCanceller::setDecayFactor(float)`, INLINED -- the same two bodies
 * `reset` carries with their argument constant-folded (finding F1271).
 *
 * `setState` reaches them three times over, at 0x113ab, 0x1145b and 0x11598,
 * and the object has the whole body at each site: store the field, then print
 * the field's sign and the argument's magnitude.
 *
 * THEY WERE FILE-STATICS UNTIL NOW, AND THE REASON HAS EXPIRED.  Neither
 * member was written, so a call to one would have been a call to nothing, and
 * the shared body was spelled once as a file-static that GCC inlined back
 * into the three sites.  Both are now the real members the object defines --
 * 0x10cc0 and 0x10d60, 147 bytes between them -- `setState` calls them, and
 * GCC inlines them back exactly as before.  So the factoring note above still
 * describes the object and the source now names what the object names.
 * Writing the body out three times is still wrong for the other reason it
 * always was: an anchor must be unique (finding F1264).
 *
 * The sign comes from the FIELD and the two integers from the ARGUMENT, which
 * is what the object does -- `fsts 0x30(%ebx)` leaves the value live in the
 * register for the magnitude and `fcomps 0x30(%ebx)` reads the store back for
 * the sign.  They are the same float; the spelling is the object's.
 *
 * TWO OF THE SIX INLINED COPIES COME OUT AS A CONSTANT `'-'` HERE AND AS A
 * COMPARE IN THE OBJECT, and it is a fold rather than a spelling.  The
 * FILTER_ONLY arm calls both with a literal `0.0f`; the object folds the
 * magnitude and the fraction (`xor %ecx,%ecx; xor %esi,%esi` at 0x113ad) and
 * still RELOADS the field for the sign (`fcoms 0x30(%ebx)` at 0x113bc,
 * `sbb %edx,%edx` at 0x113c6), where GCC 3.4.2 given this source forwards the
 * store and folds the compare.  The reload's answer is `'-'`, which is what
 * the fold produces, so no input can tell them apart and there is nothing for
 * a test to pin.  `setState` is therefore 5 of the object's 7 selects and
 * stays that way; finding F2411.
 */
void
V92EchoCanceller::setEchoBeta(float beta)
{
	echoBeta = beta;
	edprintf("V92EchoCanceller: echoBeta = %c%d.%06d\r\n",
		 sign_of(echoBeta), whole_of(beta), frac_of(beta));
}

void
V92EchoCanceller::setDecayFactor(float decay)
{
	echoBetaDecay = decay;
	edprintf("V92EchoCanceller: echoBetaDecay = %c%d.%06d\r\n",
		 sign_of(echoBetaDecay), whole_of(decay), frac_of(decay));
}

/*
 * setEchoParams (.text+0x10e00, 339 B) -- all three at once.
 *
 * THE WHOLE BODY IS THREE CALLS, and every one of them is inlined in the
 * object: the two float stores and their two messages in order, then
 * `echoLength += delay - echoDelay; echoDelay = delay;` and the third
 * message as a TAIL CALL -- `jmp edprintf` at +0x14e, with the arguments
 * written into this function's own outgoing slots, which is what
 * `setEchoDelay` being the last statement of a `void` function compiles to.
 *
 * THE ORDER IS FORCED BY THE TWO DIAGNOSTICS rather than chosen: echoBeta's
 * message (`.rodata.str1.4:0x3028`) is emitted at +0xa2 and echoBetaDecay's
 * (`:0x3054`) at +0x12a, with the store to +0x34 at +0xb1 between them.
 * Only the delay arithmetic has an ordering constraint of its own, and
 * `setEchoDelay` already carries it.
 */
void
V92EchoCanceller::setEchoParams(float beta, float decay,
				 unsigned int delay)
{
	setEchoBeta(beta);
	setDecayFactor(decay);
	setEchoDelay(delay);
}

/*
 * Enter a state: announce it, load the state's parameters, and restart the
 * sample count.
 *
 * A REPEATED STATE IS A NO-OP AND DOES NOT RESTART THE COUNT.  `cmp
 * %eax,0x8(%ebx); je` jumps past the `movl $0x0,0x10(%ebx)` that every other
 * arm falls into, so `word_10` survives a redundant call and is cleared by a
 * real change -- including a change to the ILLEGAL arm, which prints and
 * clears the count without touching the state.
 *
 * THE DISPATCH IS SIGNED.  `cmp $0x1; je; jle; cmp $0x2; je; cmp $0x3; je`,
 * and `jle` is the signed branch -- see the enum's comment in the header for
 * why that decides the underlying type.
 *
 * THE COEFFICIENT DUMP IS THE EXIT FROM TRAINING.  `mov 0x8(%ebx),%eax; sub
 * $0x2,%eax; cmp $0x1,%eax; jbe` is GCC's range test for `old == 2 || old ==
 * 3`, so the filter is printed on the way from either training state to
 * FILTER_ONLY and on no other transition.  It is `filterLength` lines of
 * `edprintf` and it runs whatever the debug level is -- `edprintf` self-gates
 * one level down, so at level 0 the formatting and the encoding still happen
 * and only the printing is skipped (see encode.h).
 */
void
V92EchoCanceller::setState(V92EchoCancellerState newState)
{
	unsigned int i;

	if (state == newState)
		return;

	switch (newState) {
	case V92_ECHO_FILTER_ONLY:
		edprintf("V92EchoCanceller: echo state set to filter only\r\n");

		if (state == V92_ECHO_FAST_TRAINING
		    || state == V92_ECHO_SLOW_TRAINING) {
			edprintf("**************** NEAR ECHO FILTER " "*************** \n");
			for (i = 0; i < filterLength; i++)
				edprintf("   = %c%d.%06d\r\n",
					 sign_of(echoCoeff[i]),
					 whole_of(echoCoeff[i]),
					 frac_of(echoCoeff[i]));
		}

		state = V92_ECHO_FILTER_ONLY;
		setEchoBeta(0.0f);
		setDecayFactor(0.0f);
		break;

	case V92_ECHO_COUNT_DELAY:
		edprintf("V92EchoCanceller: echo state set to count delay "
			 "before training\r\n");
		state = V92_ECHO_COUNT_DELAY;
		/*
		 * `mov 0x38(%ebx),%eax; add $0x190,%eax` -- the only place the
		 * duration is built rather than read, and the only use of
		 * `echoDelay` outside the tap-count arithmetic.
		 */
		updateDuration = echoDelay + 400u;
		break;

	case V92_ECHO_FAST_TRAINING:
		edprintf("V92EchoCanceller: echo state set to fast echo " "training\r\n");
		state = V92_ECHO_FAST_TRAINING;
		setEchoBeta(params->V92_ECHO_FAST_BETA_FACTOR);
		setDecayFactor(params->V92_ECHO_FAST_DECAY_FACTOR);
		updateDuration =
			(unsigned int)params->V92_ECHO_FAST_UPDATE_DURATION;
		break;

	case V92_ECHO_SLOW_TRAINING:
		edprintf("V92EchoCanceller: echo state set to slow echo " "training\r\n");
		state = V92_ECHO_SLOW_TRAINING;
		setEchoBeta(params->V92_ECHO_SLOW_BETA_FACTOR);
		setDecayFactor(params->V92_ECHO_SLOW_DECAY_FACTOR);
		updateDuration =
			(unsigned int)params->V92_ECHO_SLOW_UPDATE_DURATION;
		break;

	default:
		/*
		 * THE ONE GATED DIAGNOSTIC IN THE CLASS BESIDES THE
		 * DESTRUCTOR'S: `cmpl $0x1,dsplibs_debug_level; ja` around a
		 * plain `dsplibs_debug_printf`, so this message is readable
		 * text where every other one here is encoded.
		 */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V92EchoCanceller: setState "
					     "ERROR: illegal state\r\n");
		break;
	}

	word_10 = 0;
}

/*
 * Fill the history with the ARMA's view of the transmitted signal.
 *
 * THIS IS THE WRITER AND `process` IS THE READER, and they do not share a
 * cursor: this one appends at `echoHistory[echoLength]` and moves +0x2c,
 * `process` reads a `filterLength`-long window at `echoHistory[historyIndex]`
 * and moves +0x28.  Nothing here touches +0x28.
 *
 * THE BUFFER IS BOUNDED BY `historyAlloc` HERE, WHICH IS D72's OTHER SIDE --
 * AND THE BOUND IS CONDITIONAL, NOT A SAFETY NET.  `reset` clears
 * `echoLength` entries of a buffer allocated for `historyAlloc`; this
 * function, entered with `echoLength <= historyAlloc - 1`, never writes past
 * `historyAlloc - 1`, because the fast path is guarded by `echoLength + count
 * < historyAlloc` and the slow one compacts as the next index reaches it.
 *
 * Entered with `echoLength >= historyAlloc` -- which is exactly what D72 says
 * `setEchoDelay` can leave behind -- neither holds.  The fast guard is false,
 * so the slow path runs; its first store is already past the end; and the
 * compaction trigger is an EQUALITY, `cmp 0x1c(%ecx),%eax; je` at 0x117c7 and
 * not `jae`, against a cursor that only ever grows.  So it never matches
 * again and the writer runs `count` words off the end with nothing to stop
 * it.  That is a D72 DATAPOINT and not a second deviation: the disagreement
 * is still the one D72 records, CONFIRMED and CANNOT FIRE at any real
 * `V92_ECHO_INITIAL_DELAY` (finding F1188).  Nothing here clamps anything, the
 * test never asks the object to cross the line, and it carries a compared
 * guard at BOTH ends of the buffer so a spelling that overran would fail
 * rather than pass.
 *
 * THE COMPACTION KEEPS `filterLength - 1` SAMPLES, which is exactly the
 * overlap `process` needs: its window runs from `historyIndex` to
 * `historyIndex + filterLength - 1` and its cursor wraps at `historyAlloc -
 * word_18`, so the last `filterLength - 1` written samples are the ones a
 * wrapped reader is still looking at.  The copy runs DOWNWARD, from the
 * sample just written to the front of the buffer, and the loop is
 * BOTTOM-TESTED with no guard -- `lea -0x1(%esi),%edx` then `dec %edx; jne`
 * -- so it always runs at least once.  With `filterLength < 2` the count
 * underflows and it runs 2**32 - 1 times; the shipped 180 cannot reach that.
 * D273.
 *
 * `FloatARMA::process(float)` IS CALLED PER SAMPLE, on both arms, with the
 * pointer at +0x04 -- two call sites, which is why the object has the loop
 * body twice.
 */
void
V92EchoCanceller::updateEchoHistory(float *in, unsigned int count)
{
	float *w = &echoHistory[echoLength];
	unsigned int i;

	if (echoLength + count < historyAlloc) {
		for (i = 0; i < count; i++)
			*w++ = arma->process(in[i]);
		echoLength += count;
		return;
	}

	while (count != 0) {
		*w++ = arma->process(*in++);

		if (echoLength + 1 == historyAlloc) {
			float *src = &echoHistory[echoLength];
			float *dst = &echoHistory[filterLength - 2];
			unsigned int n = filterLength - 1;

			do {
				*dst-- = *src--;
			} while (--n != 0);

			echoLength = filterLength - 1;
			w = &echoHistory[filterLength - 1];
		} else {
			echoLength++;
		}
		count--;
	}
}

/*
 * The dot product `process` runs once per sample, and the reason it is a
 * function.
 *
 * THE OBJECT'S SUMMATION ORDER IS NOT A COMPILER'S CHOICE.  It steps four
 * taps at a time into TWO alternating accumulators -- `faddp %st,%st(1)` for
 * the even terms, `faddp %st,%st(2)` for the odd ones -- and adds the two
 * together at the end.  Float addition does not associate, so no flag this
 * object was built with permits GCC to invent that from a one-tap loop: the
 * unrolling is the original's source, and it is reproduced.  The tail loop
 * adds into the even accumulator.
 *
 * AND OVER ORDINARY DATA IT MAKES NO DIFFERENCE, which was measured rather
 * than assumed.  A product of two floats needs 48 significand bits and these
 * accumulators have 64, so a filter of taps within a factor of 2**16 of each
 * other sums EXACTLY however it is grouped -- and a mutation that merges the
 * two accumulators passed the whole differential suite until the test was
 * given a filter with a cancellation in it and a dynamic range of 2**100.
 * What the grouping decides is where a cancellation lands, so that is the
 * shape the test now carries; the last bits agree because the arithmetic is
 * exact, not because the order was copied.
 *
 * `long double` FOR THE ACCUMULATORS, because `flds; fmuls; faddp` keeps the
 * product and the running sum at 64 significand bits and never rounds to
 * float.  A `float` accumulator says the same thing only for as long as the
 * compiler's excess precision is `fast`; this spelling does not depend on it
 * (V90Phase2Info.cpp's finding F256 is the same argument).
 *
 * Written once and called from both loops: GCC inlines it, and a `long
 * double` return is passed in st(0) with no rounding even when it does not.
 */
static long double
ec_filter_sum(const float *h, const float *c, unsigned int n)
{
	long double s0 = 0.0L, s1 = 0.0L;

	while (n > 3) {
		s0 += (long double)h[0] * c[0];
		s1 += (long double)h[1] * c[1];
		s0 += (long double)h[2] * c[2];
		s1 += (long double)h[3] * c[3];
		h += 4;
		c += 4;
		n -= 4;
	}
	while (n != 0) {
		s0 += (long double)*h++ * *c++;
		n--;
	}
	return s1 + s0;
}

/*
 * Cancel the echo from one block, adapt if the state says to, and hand the
 * state machine on when the state's duration is up.
 *
 * IT READS `out[0]` BEFORE IT READS ANYTHING ELSE, INCLUDING `count`.  `flds
 * (%edx); fcomps <177.0f>` is the first thing the function does, so a call
 * with `count == 0` still dereferences the second buffer.  177.0f is
 * referenced from this one instruction and from nowhere else in the 1.2 MB
 * object (`relocscan.py --at .rodata.cst4:0x8c`), so nothing else in the blob
 * says what it means; when it matches, the block is copied through unfiltered
 * and the read cursor is stepped as if it had been filtered, and none of the
 * state machine runs.  Reproduced as measured.  D272.
 *
 * THE COMPARISON IS SPELLED `<` OR `>` FOR THE SAME REASON
 * `V90Equalizer::setLinearEquBeta`'s is: the object branches on ZF alone and
 * FCOM sets C3 for EQUAL and for UNORDERED both, so a NaN in `out[0]` takes
 * the copy path where C's `!=` would take the filter path.  The negated
 * `<`/`>` pair is the predicate the object has (finding F236's shape).
 *
 * FOUR PATHS, AND ONLY TWO OF THEM ADAPT.  State 0 filters and returns
 * without touching `word_10`, so a canceller that has finished training never
 * changes state again.  State 1 copies the block through, advances the cursor
 * by the whole block at once, and counts.  States 2 and 3 -- and, because the
 * dispatch is `if (state == 0) ... else if (state == 1) ... else`, every
 * ILLEGAL state too -- filter, adapt, decay the beta, and count.
 *
 * THE ERROR THAT DRIVES THE ADAPTATION IS NOT THE ONE THAT IS WRITTEN OUT.
 * `fld %st(0); fstps (%edx,%eax,4)` stores a float copy of the error to
 * `out[i]` and keeps the 64-bit-significand original in the register for
 * `err * echoBeta`, so the update uses more precision than the caller sees.
 * `long double err` is that, spelled so it does not depend on excess
 * precision.
 *
 * THE CURSOR WRAPS AT `historyAlloc - word_18` AND +0x18 IS WHAT IT READS --
 * not `filterLength - 1` recomputed, which is what `updateEchoHistory`'s
 * compaction does with the same quantity.  Two fields holding one number, and
 * each function picks a different one; that is why +0x18 is a real member and
 * not a value the constructor could have folded away.
 */
void
V92EchoCanceller::process(float *in, float *out, unsigned int count)
{
	unsigned int i, j, mod;
	long double sum, err, mu;

	/*
	 * `fcoms` against the 177.0f constant and `je` -- ONE compare, taken
	 * for equal and for unordered both, which under -mno-ieee-fp is what
	 * `out[0] == 177.0f` emits.  The negated pair of relational tests this
	 * used to carry was the -mieee-fp workaround and is two compares.
	 * Finding F2300.
	 */
	if (out[0] == 177.0f) {
		mod = historyAlloc - word_18;
		for (i = 0; i < count; i++) {
			out[i] = in[i];
			if (historyIndex + 1 == mod)
				historyIndex = 0;
			else
				historyIndex = historyIndex + 1;
		}
		return;
	}

	if (state == V92_ECHO_FILTER_ONLY) {
		for (i = 0; i < count; i++) {
			sum = ec_filter_sum(&echoHistory[historyIndex],
					    echoCoeff, filterLength);
			out[i] = in[i] - sum;

			if (historyIndex + 1 == historyAlloc - word_18)
				historyIndex = 0;
			else
				historyIndex = historyIndex + 1;
		}
		return;
	}

	if (state == V92_ECHO_COUNT_DELAY) {
		for (i = 0; i < count; i++)
			out[i] = in[i];

		/*
		 * ONE CONDITIONAL SUBTRACTION, NOT A MODULO.  `cmp %eax,%edx;
		 * jae; sub %eax,%edx` -- a block longer than the modulus
		 * leaves the cursor past the end of the buffer and the object
		 * does not loop.  D274, and it cannot fire at any block
		 * length the modem uses.
		 */
		mod = historyAlloc - word_18;
		historyIndex += count;
		if (historyIndex >= mod)
			historyIndex -= mod;

		word_10 += count;
		if (word_10 >= updateDuration)
			setState(V92_ECHO_FAST_TRAINING);
		return;
	}

	for (i = 0; i < count; i++) {
		sum = ec_filter_sum(&echoHistory[historyIndex], echoCoeff,
				    filterLength);
		err = in[i] - sum;
		out[i] = err;

		mu = err * echoBeta;
		for (j = 0; j < filterLength; j++)
			echoCoeff[j] += echoHistory[historyIndex + j] * mu;

		echoBeta = echoBeta * echoBetaDecay;

		if (historyIndex + 1 == historyAlloc - word_18)
			historyIndex = 0;
		else
			historyIndex = historyIndex + 1;
	}

	word_10 += count;
	if (word_10 >= updateDuration)
		setState(state == V92_ECHO_FAST_TRAINING
			 ? V92_ECHO_SLOW_TRAINING : V92_ECHO_FILTER_ONLY);
}
