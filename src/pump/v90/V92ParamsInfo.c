/*
 * V92ParamsInfo.c -- allocate and release the arrays inside the V.92 modem's
 * parameter-info block.
 *
 * Four functions, 411 bytes, and between them exactly ten `sysdep_malloc`
 * calls and ten `sysdep_free` calls.  `include/dsplib/V92ParamsInfo.h` carries
 * the block's identification and the argument to it; this file is the code.
 *
 * WHAT THE CREATORS DO NOT DO, which is the part worth reading twice: neither
 * checks a return.  Six mallocs in a row are stored straight into the block
 * with no `test %eax,%eax` between them, so a failing allocator leaves nulls
 * in the block and the constructor carries on.  `vpcm_create` DOES check the
 * one it makes for `K56FLEX_Create` -- .text+0x3b31 -- so this is a property
 * of these four functions and not a house style.  docs/deviations.md D181.
 *
 * WHAT THE DELETERS DO NOT DO is in the header: no slot is nulled after being
 * freed, which is D180.  Both facts are the object's and both are reproduced.
 *
 * THE BLOCK PLACEMENT IN THE OBJECT IS GCC'S AND NOT THE SOURCE'S.  Each
 * `if (p) sysdep_free(p)` compiles to a forward `jne` into an out-of-line
 * block that calls and jumps back, so the six calls in `V92deleteConstellations`
 * appear in reverse order at the tail of the function.  That is basic-block
 * placement, which CLAUDE.md's rule puts in the "free, so ignore it" column;
 * the source below is the six statements in the order the tests run them.
 */

#include "dsplib/V92ParamsInfo.h"

#include "dsplib/sysdep.h"

void
V92createConstellations(struct V92ParamsInfo *p)
{
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
	p->filterCoefficients[0] = sysdep_malloc(V92_PARAMSINFO_FILTERCOEF_SZ);
	p->filterCoefficients[1] = sysdep_malloc(V92_PARAMSINFO_FILTERCOEF_SZ);
	p->filterCoefficients[2] = sysdep_malloc(V92_PARAMSINFO_FILTERCOEF_SZ);
	p->filterCoefficients[3] = sysdep_malloc(V92_PARAMSINFO_FILTERCOEF_SZ);
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
	if (p->filterCoefficients[0] != 0)
		sysdep_free(p->filterCoefficients[0]);
	if (p->filterCoefficients[1] != 0)
		sysdep_free(p->filterCoefficients[1]);
	if (p->filterCoefficients[2] != 0)
		sysdep_free(p->filterCoefficients[2]);
	if (p->filterCoefficients[3] != 0)
		sysdep_free(p->filterCoefficients[3]);
}
