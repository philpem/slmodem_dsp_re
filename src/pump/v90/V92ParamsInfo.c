/*
 * V92ParamsInfo.c -- allocate, release and fill the V.92 modem's mapping
 * parameter block.
 *
 * Five functions, 3,106 bytes.  Four of them are the allocators and the
 * deleters, 411 bytes, with exactly ten `sysdep_malloc` calls and ten
 * `sysdep_free` calls between them; the fifth is
 * `V92setParamsInfoFromCPUnPck` (.text+0x12f00, 2,695 bytes), which is what
 * fills the block and therefore what NAMED it -- almost every field name in
 * `include/dsplib/V92ParamsInfo.h` and all of
 * `include/dsplib/V92CPUnPck.h` came out of the format strings below.
 * The header carries the block's identification and the derivation of each
 * name; this file is the code.
 *
 * WHAT THE CREATORS DO NOT DO, which is the part worth reading twice: neither
 * checks a return.  Six mallocs in a row are stored straight into the block
 * with no `test %eax,%eax` between them, so a failing allocator leaves nulls
 * in the block and the constructor carries on.  `vpcm_create` DOES check the
 * one it makes for `K56FLEX_Create` -- .text+0x3b31 -- so this is a property
 * of these four functions and not a house style.  docs/deviations.md D171.
 *
 * WHAT THE DELETERS DO NOT DO is in the header: no slot is nulled after being
 * freed, which is D170.  Both facts are the object's and both are reproduced.
 *
 * THE BLOCK PLACEMENT IN THE OBJECT IS GCC'S AND NOT THE SOURCE'S.  Each
 * `if (p) sysdep_free(p)` compiles to a forward `jne` into an out-of-line
 * block that calls and jumps back, so the six calls in `V92deleteConstellations`
 * appear in reverse order at the tail of the function.  That is basic-block
 * placement, which CLAUDE.md's rule puts in the "free, so ignore it" column;
 * the source below is the six statements in the order the tests run them.
 */

#include "dsplib/V92ParamsInfo.h"

#include "dsplib/V92CPUnPck.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"

/*
 * The scale factors, every one of them read off a `.rodata.cst4` operand
 * rather than derived.  Five of the six are exact powers of two, which is
 * what a fixed-point field being brought into floats looks like; the sixth is
 * not and is written as the literal the object holds.
 *
 *   +0xa0  1333.3333740234375   the "Upstream rate" print, and nothing else
 *   +0xa4  2^-18                prefilterGain -> gain, first multiplication
 *   +0xac  4000                 gain, second multiplication ("Lu")
 *   +0xb0  2^-15                the two z arrays
 *   +0xb4  2^-14                the two p arrays
 *   +0xa8  1000000              the %06d fraction, in frac_of below
 */
#define V92PI_RATE_PER_DRN	1333.3333740234375f
#define V92PI_GAIN_SCALE	3.814697265625e-06f
#define V92PI_GAIN_LU		4000.0f
#define V92PI_ZERO_SCALE	3.0517578125e-05f
#define V92PI_POLE_SCALE	6.103515625e-05f

/*
 * The float-as-%c%d.%06d idiom, the same three helpers
 * src/pump/v90/V92EchoCanceller.cpp carries and for the same reasons.  The
 * subtraction inside `frac_of` is `v - (int)v` here, which is the order the
 * object uses -- `dc e1`, which objdump prints as `fsub %st,%st(1)` and which
 * IS `FSUBR st(1),st(0)`, so st(1) becomes st(0) - st(1) and st(0) is the
 * value (finding F245, and tools/dis.py flags the line).  The abs() on the
 * result makes the order unobservable either way (finding F256).
 *
 * `sign_of` is `0.0f < v` because the object is `fldz; fcompp` and selects on
 * CF alone: zero prints as '-'.
 */
static char
sign_of(float v)
{
	return (0.0f < v) ? '+' : '-';
}

static int
whole_of(float v)
{
	return (int)__builtin_fabsf(v);
}

static int
frac_of(float v)
{
	return __builtin_abs((int)(((long double)v - (long double)(int)v)
				   * 1.0e6f));
}

void
V92createConstellations(struct V92ParamsInfo *p)
{
	/*
	 * Rolled, this is:
	 *     for (i = 0; i < 6; i++)
	 *             p->constellations[i] =
	 *                     sysdep_malloc(V92_PARAMSINFO_CONSTELLATION_SZ);
	 */
	p->constellations[0] = sysdep_malloc(V92_PARAMSINFO_CONSTELLATION_SZ);
	p->constellations[1] = sysdep_malloc(V92_PARAMSINFO_CONSTELLATION_SZ);
	p->constellations[2] = sysdep_malloc(V92_PARAMSINFO_CONSTELLATION_SZ);
	p->constellations[3] = sysdep_malloc(V92_PARAMSINFO_CONSTELLATION_SZ);
	p->constellations[4] = sysdep_malloc(V92_PARAMSINFO_CONSTELLATION_SZ);
	p->constellations[5] = sysdep_malloc(V92_PARAMSINFO_CONSTELLATION_SZ);
}

void
V92createFilterCoefficients(struct V92ParamsInfo *p)
{
	p->z1 = sysdep_malloc(V92_PARAMSINFO_FILTERCOEF_SZ);
	p->p1 = sysdep_malloc(V92_PARAMSINFO_FILTERCOEF_SZ);
	p->z2 = sysdep_malloc(V92_PARAMSINFO_FILTERCOEF_SZ);
	p->p2 = sysdep_malloc(V92_PARAMSINFO_FILTERCOEF_SZ);
}

void
V92deleteConstellations(struct V92ParamsInfo *p)
{
	if (p->constellations[0] != 0)
		sysdep_free(p->constellations[0]);
	if (p->constellations[1] != 0)
		sysdep_free(p->constellations[1]);
	if (p->constellations[2] != 0)
		sysdep_free(p->constellations[2]);
	if (p->constellations[3] != 0)
		sysdep_free(p->constellations[3]);
	if (p->constellations[4] != 0)
		sysdep_free(p->constellations[4]);
	if (p->constellations[5] != 0)
		sysdep_free(p->constellations[5]);
}

void
V92deleteFilterCoefficients(struct V92ParamsInfo *p)
{
	if (p->z1 != 0)
		sysdep_free(p->z1);
	if (p->p1 != 0)
		sysdep_free(p->p1);
	if (p->z2 != 0)
		sysdep_free(p->z2);
	if (p->p2 != 0)
		sysdep_free(p->p2);
}

/*
 * ===========================================================================
 * V92setParamsInfoFromCPUnPck (.text+0x12f00, 2,695 bytes)
 *
 * Refill the parameter block from an unpacked CP message.  Three independent
 * halves, each gated on its own `*Present` word, and NOTHING outside those
 * three is conditional: the eight scalars at the top are copied whatever the
 * flags say, and the three flags are copied before they are tested, so what
 * gates each half is the copy in the DESTINATION and not the source.  That is
 * the object's -- `mov 0x4(%esi),%ebx; test %ebx,%ebx` at .text+0x13090 reads
 * back what .text+0x12f1c has just written -- and it matters, because a
 * caller that hands the same block twice sees the second call gated on what
 * the first left behind only if the flags are copied first.  They are.
 *
 * HALF THE FUNCTION IS DIAGNOSTICS AND EVERY GATE RELOADS THE LEVEL.  There
 * are twenty-six separate `cmpl $0x1,dsplibs_debug_level` in it and not one
 * of them is shared between two prints, which is v34pcmcreate.cpp's "FIVE
 * SEPARATE GATES AND NOT ONE" over again.  Each print below therefore gets
 * its own `if (DSPLIB_DEBUG_ON())` and they are NOT collapsed into blocks;
 * the level is a global that a callee could change, so collapsing them is a
 * behaviour change and not a tidy-up.  test/unit/t_v92unpck.c drives both
 * levels and compares the two transcripts, which is what checks the format
 * strings and the argument lists rather than just the control flow.
 *
 * THE GAIN IS ROUNDED TO FLOAT TWICE AND THAT IS LOAD-BEARING.  The object
 * stores the first product through a 4-byte slot (`fstps; flds`) before
 * multiplying by 4000, so the second multiplication sees a single-precision
 * value and not the x87 register's 80 bits.  Assigning to `p->gain` and then
 * reading it back is what reproduces that; computing `pfg * 2^-18 * 4000` in
 * one expression would not.
 *
 * One shape here is the compiler's and not reproduced literally: the object
 * materialises `p->gain` before the FIRST print only on the printing path,
 * because on the other path the store is dead.  The source below assigns
 * unconditionally.  The two agree at every level -- the value stored is the
 * same and the second assignment overwrites it either way.
 * ===========================================================================
 */
void
V92setParamsInfoFromCPUnPck(struct V92ParamsInfo *p, struct V92CPUnPck *cp)
{
	unsigned int i;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92 mapping parameters:\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CPObj->modulosEncoderPresent = %d\r\n",
				     cp->modulosEncoderPresent);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CPObj->prefilterPrecoderPresent = "
				     "%d\r\n", cp->prefilterPrecoderPresent);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CPObj->constellationPresent = %d\r\n",
				     cp->constellationPresent);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CPObj->trellisState = %d\r\n",
				     cp->trellisState);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CPObj->prefilterGain = %d\r\n",
				     cp->prefilterGain);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CPObj->extendEu = %d\r\n",
				     cp->extendEu);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("CPObj->drn = %d\r\n", cp->drn);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Upstream rate = %d\r\n",
				     (int)((cp->drn + 17)
					   * V92PI_RATE_PER_DRN));

	p->modulosEncoderPresent = cp->modulosEncoderPresent;
	p->prefilterPrecoderPresent = cp->prefilterPrecoderPresent;
	p->constellationPresent = cp->constellationPresent;

	/* `lea 0x22(%edi,%edi,1),%ebx` -- twice (drn + 17), and the same
	 * (drn + 17) the rate print above is built from. */
	p->K = 2 * (cp->drn + 17);

	p->trellisType = cp->trellisState;
	p->extendEu = cp->extendEu;

	/*
	 * THE NARROWING IS STATED, NOT HOPED FOR, and the `double` is not a
	 * widening of the arithmetic.  The object rounds this product to 32
	 * bits through a stack slot -- `fstps 0x28(%esp); flds 0x28(%esp)` at
	 * .text+0x12f61 -- BEFORE multiplying by 4000, so the second
	 * multiplication sees a single-precision value.  Written as
	 * `p->gain = cp->prefilterGain * SCALE;` that rounding is the
	 * compiler's to make: GCC 3.4.2 makes it, and GCC 13 under
	 * `-fexcess-precision=fast` keeps the 80-bit register across the
	 * debug branch and rounds once at the end instead, which is a
	 * different number for any gain with more than 24 significant bits.
	 * It cost six of 1,530 checks in t_v92unpck.
	 *
	 * `(double)pfg * (double)2^-18` is EXACT -- a 32-bit integer scaled by
	 * a power of two needs 32 of double's 53 bits -- so the `(float)` is
	 * the object's one rounding and not a second one.  GCC still emits
	 * `fmuls` off a 4-byte constant, and the sequence it gives is the
	 * blob's instruction for instruction.  This is the spelling
	 * docs/method/compilers.md records for `Resampler.cpp`: a `double`
	 * narrowed by explicit conversion satisfies both compilers, where an
	 * assignment or a `(float)` cast satisfies neither.
	 */
	p->gain = (float)((double)cp->prefilterGain
			  * (double)V92PI_GAIN_SCALE);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator: constellation gain "
				     "(before Lu multiplication) = "
				     "%c%d.%06d\r\n",
				     sign_of(p->gain), whole_of(p->gain),
				     frac_of(p->gain));
	p->gain = p->gain * V92PI_GAIN_LU;
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V92Modulator: constellation gain "
				     "(after Lu multiplication) = "
				     "%c%d.%06d\r\n",
				     sign_of(p->gain), whole_of(p->gain),
				     frac_of(p->gain));

	/* --------------------------------------------------- the moduli */
	if (p->modulosEncoderPresent != 0) {
		for (i = 0; i < V92_CPUNPCK_MODULI; i++)
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("CPObj->M[%d] = %d\r\n",
						     i, cp->M[i]);

		p->m[0] = cp->M[0];
		p->m[1] = cp->M[1];
		p->m[2] = cp->M[2];
		p->m[3] = cp->M[3];
		p->m[4] = cp->M[4];
		p->m[5] = cp->M[5];
		p->m[6] = cp->M[6];
		p->m[7] = cp->M[7];
		p->m[8] = cp->M[8];
		p->m[9] = cp->M[9];
		p->m[10] = cp->M[10];
		p->m[11] = cp->M[11];
	}

	/* ------------------------------- the prefilter and the precoder */
	if (p->prefilterPrecoderPresent != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("CPObj->lz1 = %d\r\n", cp->lz1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("CPObj->lz2 = %d\r\n", cp->lz2);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("CPObj->lp1 = %d\r\n", cp->lp1);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("CPObj->lp2 = %d\r\n", cp->lp2);

		p->lz1 = cp->lz1;
		p->lp1 = cp->lp1;
		p->lz2 = cp->lz2;
		p->lp2 = cp->lp2;

		if (p->lz1 > V92_PARAMSINFO_MAX_FILTER_LEN)
			p->lz1 = V92_PARAMSINFO_MAX_FILTER_LEN;
		if (p->lz2 > V92_PARAMSINFO_MAX_FILTER_LEN)
			p->lz2 = V92_PARAMSINFO_MAX_FILTER_LEN;
		if (p->lp1 > V92_PARAMSINFO_MAX_FILTER_LEN)
			p->lp1 = V92_PARAMSINFO_MAX_FILTER_LEN;
		if (p->lp2 > V92_PARAMSINFO_MAX_FILTER_LEN)
			p->lp2 = V92_PARAMSINFO_MAX_FILTER_LEN;

		/*
		 * The zeros come out at 2^-15 and the poles at 2^-14, each
		 * loop bounded by its own length and reading its own 16-bit
		 * source array.  The constants are loaded once and left on
		 * the stack across the loop, which is the compiler hoisting
		 * them.
		 */
		for (i = 0; i < p->lz1; i++)
			p->z1[i] = cp->z1[i] * V92PI_ZERO_SCALE;
		for (i = 0; i < p->lp1; i++)
			p->p1[i] = cp->p1[i] * V92PI_POLE_SCALE;
		for (i = 0; i < p->lz2; i++)
			p->z2[i] = cp->z2[i] * V92PI_ZERO_SCALE;
		for (i = 0; i < p->lp2; i++)
			p->p2[i] = cp->p2[i] * V92PI_POLE_SCALE;
	}

	/* ------------------------------------------- the constellations */
	if (p->constellationPresent != 0) {
		for (i = 0; i < V92_CPUNPCK_CONSTELS; i++)
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("CPObj->LC[%d] = %d\r\n",
						     i, cp->LC[i]);
		for (i = 0; i < V92_CPUNPCK_CONSTELS; i++)
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("CPObj->indexConstel[%d]"
						     " = %d\r\n",
						     i, cp->indexConstel[i]);

		p->LC[0] = cp->LC[0];
		p->LC[1] = cp->LC[1];
		p->LC[2] = cp->LC[2];
		p->LC[3] = cp->LC[3];
		p->LC[4] = cp->LC[4];
		p->LC[5] = cp->LC[5];

		if (p->LC[0] > V92_PARAMSINFO_MAX_LC)
			p->LC[0] = V92_PARAMSINFO_MAX_LC;
		if (p->LC[1] > V92_PARAMSINFO_MAX_LC)
			p->LC[1] = V92_PARAMSINFO_MAX_LC;
		if (p->LC[2] > V92_PARAMSINFO_MAX_LC)
			p->LC[2] = V92_PARAMSINFO_MAX_LC;
		if (p->LC[3] > V92_PARAMSINFO_MAX_LC)
			p->LC[3] = V92_PARAMSINFO_MAX_LC;
		if (p->LC[4] > V92_PARAMSINFO_MAX_LC)
			p->LC[4] = V92_PARAMSINFO_MAX_LC;
		if (p->LC[5] > V92_PARAMSINFO_MAX_LC)
			p->LC[5] = V92_PARAMSINFO_MAX_LC;

		/*
		 * Six banner-and-dump blocks, one per constellation, each
		 * skipped entirely when its size is zero -- and the size is
		 * re-read from the block on every iteration, because the
		 * printf could have changed it as far as the compiler knows.
		 */
		if (p->LC[0] != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("======== Constellation"
						     " LC 1 ============="
						     "\r\n");
			for (i = 0; i < p->LC[0]; i++)
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("\tconst1[%d] ="
							     " %d\r\n", i,
							     cp->const1[i]);
		}
		if (p->LC[1] != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("======== Constellation"
						     " LC 2 ============="
						     "\r\n");
			for (i = 0; i < p->LC[1]; i++)
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("\tconst2[%d] ="
							     " %d\r\n", i,
							     cp->const2[i]);
		}
		if (p->LC[2] != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("======== Constellation"
						     " LC 3 ============="
						     "\r\n");
			for (i = 0; i < p->LC[2]; i++)
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("\tconst3[%d] ="
							     " %d\r\n", i,
							     cp->const3[i]);
		}
		if (p->LC[3] != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("======== Constellation"
						     " LC 4 ============="
						     "\r\n");
			for (i = 0; i < p->LC[3]; i++)
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("\tconst4[%d] ="
							     " %d\r\n", i,
							     cp->const4[i]);
		}
		if (p->LC[4] != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("======== Constellation"
						     " LC 5 ============="
						     "\r\n");
			for (i = 0; i < p->LC[4]; i++)
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("\tconst5[%d] ="
							     " %d\r\n", i,
							     cp->const5[i]);
		}
		if (p->LC[5] != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("======== Constellation"
						     " LC 6 ============="
						     "\r\n");
			for (i = 0; i < p->LC[5]; i++)
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf("\tconst6[%d] ="
							     " %d\r\n", i,
							     cp->const6[i]);
		}

		/*
		 * The closing rule, printed once if ANY constellation is
		 * non-empty.  Six loads and six `jne` to one block, which is
		 * a short-circuiting `||` chain and not six statements.
		 */
		if (p->LC[0] != 0 || p->LC[1] != 0 || p->LC[2] != 0
		    || p->LC[3] != 0 || p->LC[4] != 0 || p->LC[5] != 0)
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("========================="
						     "=========\r\n");

		/*
		 * UNCONDITIONAL, and a real indexed loop in the object --
		 * which is what makes indexConstel an array where the six
		 * constellation pointers beside it are six fields.
		 */
		for (i = 0; i < V92_CPUNPCK_CONSTELS; i++)
			p->indexConstel[i] = cp->indexConstel[i];

		for (i = 0; i < p->LC[0]; i++)
			p->constellations[0][i] = cp->const1[i];
		for (i = 0; i < p->LC[1]; i++)
			p->constellations[1][i] = cp->const2[i];
		for (i = 0; i < p->LC[2]; i++)
			p->constellations[2][i] = cp->const3[i];
		for (i = 0; i < p->LC[3]; i++)
			p->constellations[3][i] = cp->const4[i];
		for (i = 0; i < p->LC[4]; i++)
			p->constellations[4][i] = cp->const5[i];
		for (i = 0; i < p->LC[5]; i++)
			p->constellations[5][i] = cp->const6[i];
	}
}
