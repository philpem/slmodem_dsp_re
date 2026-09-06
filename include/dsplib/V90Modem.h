/**
 * @file V90Modem.h
 * @brief `V90Modem`, the V.90 modem proper: the object `VPcmFloModem` embeds
 *        at +0x1758, and the thing `V90Modem::V90Modem` builds.
 *
 * This file used to say "not an object map and must not be read as one" and
 * carried a class with no data members, safe for `printTitle` and nothing
 * else. That has since been replaced wholesale: the class body that used to
 * live in `include/dsplib/V90SessionFlag.h` -- six fields, the two the
 * VPcmFloModem batch carved out of `pad_08`, and `setSessionFlag` -- moved
 * here unchanged in content, and `V90SessionFlag.h` now includes this file.
 *
 * Not polymorphic: two destructors, `D1` at 0x192b0 and `D2` at 0x19160, and
 * no `D0`; a deleting destructor is what GCC emits for a virtual one
 * (finding F228), so there is no vptr and +0x00 is a real member.
 *
 * ===========================================================================
 * WHERE THE LAYOUT COMES FROM
 * ===========================================================================
 *
 * `sizeof(V90Modem) == 0x49c0` is asserted in src/pump/v90/VPcmFloModem.cpp
 * and is not asserted again here; it was settled by the VPcmFloModem batch
 * before any of the fields below were read. Every field between +0x0c and
 * +0x49b4 -- the span that header called `pad_0c[0x49a8]` -- is named by the
 * constructor at 0x194e0, and each one is proved twice: once by what the
 * constructor stores there, and once by the position it occupies in the
 * twelve- and fourteen-argument calls to `V90Modulator` and `V90Demodulator`,
 * whose manglings spell out the type of every parameter. That second reading
 * is what types them; adjacency only bounds them.
 *
 *     +0x0c  malloc(0x90), `V90Jd::V90Jd(V90Parameters *)`, stored; and it is
 *            argument 3 of both callees, spelled `P5V90Jd`.
 *     +0x10  malloc(0xdc), `V92Jd::V92Jd(V90Parameters *)`, stored; argument
 *            4, spelled `P5V92Jd`.
 *     +0x14  the constructor's THIRD argument, stored and not owned;
 *            argument 5, spelled `P19tagV90DILdescriptor`.
 *     +0x18, +0x668  `lea 0x18(%esi)` and `lea 0x668(%esi)` -- ADDS off
 *            `this`, not loads, so both blocks are IN the object -- passed as
 *            arguments 6 and 7, which the mangling spells
 *            `P16V90MappingParams` and `S9_`, the same type again.  Their
 *            SIZE is not adjacency: `sizeof(V90MappingParams)` is 0x650 from
 *            its own header's field map, and 0x668 - 0x18 is 0x650.  Two
 *            measurements, and they agree.
 *     +0xcb8 `lea 0xcb8(%esi)`, argument 8, `P22tagV90AdditionalCPinfo`.
 *     +0xcd0 `V90MP::V90MP()` is called on it before anything else happens,
 *            and it is argument 10 (`P5V90MP`).  `sizeof(V90MP)` is 0x124
 *            (src/pump/v90/V90MP.cpp) and 0xdf4 - 0xcd0 is 0x124.
 *     +0xdf4 `V90CP::V90CP()`, argument 9 (`P5V90CP`).  `sizeof(V90CP)` is
 *            0x3bc0 (src/pump/v90/V90CP.cpp) and 0x49b4 - 0xdf4 is 0x3bc0.
 *            So the V90CP runs exactly up to `params` and there is no
 *            unmodelled span left anywhere in this object.
 *
 * THE ONE SIZE THAT RESTS ON ADJACENCY ALONE is `tagV90AdditionalCPinfo`'s
 * 0x18, which is 0xcd0 - 0xcb8 and nothing else -- finding F1320's bound with
 * no independent measurement beside it.  Every other size in this file has a
 * `sysdep_malloc` or an existing assertion behind it.  That is why no
 * `sizeof` is asserted for that struct and why its one member is a `pad_`.
 *
 * ===========================================================================
 * THE TWO POINTERS THE CONSTRUCTOR MAY LEAVE ALONE
 * ===========================================================================
 *
 * `modulator` and `demodulator` are written on the side == 0 and side == 1
 * arms respectively, each arm writing the other as NULL.  ANY OTHER VALUE of
 * `side` prints "Illegal modemSide" and stores NEITHER -- `test`/`je`,
 * `dec`/`je`, then fall through -- so both words keep whatever the storage
 * held.  The destructor then tests both and frees what it finds, which on a
 * seeded fixture means it frees garbage.  That is the object's behaviour and
 * it is reproduced; see test/unit/t_v90modemctor.cpp, which asserts our
 * allocator counters EQUAL the blob's rather than asserting they are zero.
 * V92Modem.h records the same shape for the same reason (finding F1323).
 */

#ifndef DSPLIB_V90MODEM_H
#define DSPLIB_V90MODEM_H

#include "dsplib/V90CP.h"
#include "dsplib/V90Equalizer.h"	/* V90ComputationalMode lives there */
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90MP.h"

/*
 * Pointers only, so forward declarations only. This used to be load-bearing
 * rather than tidy: `V90Demodulator.h` drags in `V90PreFilter.h`, which once
 * carried a second, incompatible definition of `V90Parameters` that could
 * not sit in a translation unit beside the named one in `V90Parameters.h`
 * (finding F1112) -- retired at task #116 (finding F6402), and
 * `V90ModemCtor.cpp` includes both today without conflict. The forward
 * declarations are kept regardless: this header is included by
 * `VPcmFloModem.h`, so pulling either full definition in here would decide
 * that question for every one of its users, and a translation unit that
 * needs to dereference one of these includes it itself (finding F1325).
 */
class V90Demodulator;
class V90Jd;
class V90Modulator;
class V90Parameters;
class V90Phase2Info;
class V92Jd;
struct _tagModemParameters;
struct tagV90DILdescriptor;

/*
 * The mangling of the constructor spells both of these (`12V90ModemSide`,
 * `20V90ComputationalMode`) but says nothing about their enumerators, so
 * they are declared and not defined (finding F226), the same spelling
 * V90Equalizer.h and V92Modem.h already use.
 *
 * `V90ModemSide`'s underlying type is `unsigned int`: the destructor's
 * range test on it is an unsigned comparison, where a signed `side > 1`
 * would compile differently -- the same argument V92Modem.h makes for
 * `V92ModemSide` at the same shape in the sibling class (finding F1332).
 * `_BASE_PIN` is ours; the object names no enumerator.
 *
 * `V90_MODEM_SIDE_DIGITAL`/`V90_MODEM_SIDE_ANALOG` ARE REAL ENUMERATORS,
 * not macros outside the enum, unlike `V90ComputationalMode` below and the
 * other opaque enums this tree declines to fill in (`PreFilterCoefType`,
 * `__tHardwareCodecTypes__`): those are genuinely unnamed because the
 * mangling is the ONLY evidence and it carries no enumerator, so a name
 * there would be invented. Here the mangling is not the evidence -- the
 * constructor's own diagnostic string is: `modemSide == 0 ? "Digital" :
 * "Analog"` at .text+0x19514, corroborated independently by the two
 * strings' own `.rodata.str1.1` order (V90ModemCtor.cpp), CLAUDE.md's
 * strongest evidence tier. Naming a real value the object itself prints is
 * not the same act as naming one it doesn't, so the two enums are handled
 * differently even though both started from the same C++98-vs-mangling
 * constraint.
 *
 * `V90ComputationalMode` has one home only, in V90Equalizer.h, which this
 * file now includes: C++98 has no opaque enum declaration, so a definition
 * may not be repeated in a second header the way an earlier version of
 * this file did (docs/method/compilers.md, V2).
 */
enum V90ModemSide {
	V90_MODEM_SIDE_DIGITAL = 0,
	V90_MODEM_SIDE_ANALOG = 1,
	V90ModemSide_BASE_PIN = 0xffffffffu
};

typedef char v90modem_side_is_unsigned[
    ((enum V90ModemSide)-1 > (enum V90ModemSide)0) ? 1 : -1];

/*
 * `tagV90AdditionalCPinfo`, embedded at +0xcb8, 0x18 bytes -- the mangling
 * of both modulator constructors names the type. Its definition lives in
 * its own header rather than here, because V90Demodulator.cpp's `enterRRN`
 * also needs the complete type ("one type, one home"; the same reason
 * `V90ConnectionEvaluator`'s definition moved). The size is 0xcd0 - 0xcb8,
 * adjacency alone, so it is not asserted -- see the file comment.
 */
#include "dsplib/tagV90AdditionalCPinfo.h"

class V90Modem {
public:
	/**
	 * @brief Construct one side of a V.90 modem and everything it embeds.
	 *
	 * Builds `phase2Info`, `jd`, `jd92`, both `V90MappingParams`, `mp` and
	 * `cp`, stores @p dilDescriptor and @p flag, and then builds either
	 * `modulator` (side == digital) or `demodulator` (side == analog),
	 * leaving the other side's pointer NULL. @p compMode and @p flag are
	 * forwarded on as the last two of the twelve/fourteen arguments to
	 * whichever half gets built. Any @p side outside the two legal values
	 * leaves both `modulator` and `demodulator` untouched -- see the file
	 * comment.
	 *
	 * @param side           Which half to build, ::V90_MODEM_SIDE_DIGITAL
	 *                       or ::V90_MODEM_SIDE_ANALOG.
	 * @param modemParams    The caller's raw parameter block.
	 * @param dilDescriptor  The DIL descriptor; stored, not owned.
	 * @param nofSymbols     Symbol-buffer capacity, forwarded to the built half.
	 * @param compMode       Computational mode, forwarded to the built half.
	 * @param flag           Session flag, stored at `sessionFlag` and forwarded.
	 */
	V90Modem(V90ModemSide side, _tagModemParameters *modemParams,
		 tagV90DILdescriptor *dilDescriptor, unsigned int nofSymbols,
		 V90ComputationalMode compMode, unsigned int flag);

	/**
	 * @brief Destroy whichever side was built and its embedded sub-objects.
	 *
	 * The two duplicated destructor symbols (`D1`, `D2`) differ by one
	 * byte, the epilogue's scratch register (finding F1324) -- a free
	 * compiler choice, not a behavioral difference.
	 */
	~V90Modem();

	/**
	 * @brief Print the nine-line V90 modem version banner.
	 *
	 * Never touches `this`. Three ungated banner/description lines
	 * through edprintf(), three gated ones (banner, version string, date)
	 * through dsplibs_debug_printf(), then three more ungated and a
	 * gated closing banner -- in that order, which is NOT the order the
	 * object's disassembly lays the blocks out in (see V90Modem.cpp).
	 */
	void printTitle();

	/**
	 * @brief Forward one progress step to whichever side was built.
	 *
	 * The whole body is the `side` switch: forwards its four arguments
	 * unchanged to V90Modulator::progress() on the digital side or
	 * V90Demodulator::progress() on the analog side (both tail calls),
	 * or prints "Illegal modemSide" on any other value. An illegal
	 * `side` dereferences whichever pointer the constructor left NULL,
	 * with no guard.
	 *
	 * @param bits        Data bits, direction depending on side.
	 * @param nofBits     In/out bit count, per the callee's own contract.
	 * @param samples     Sample buffer, direction depending on side.
	 * @param nofSymbols  Symbol count for this call.
	 */
	void progress(int *bits, unsigned int &nofBits, float *samples,
		      unsigned int nofSymbols);

	/**
	 * @brief Print a banner and reset whichever side was built.
	 *
	 * Digital side: calls V90Modulator::reset() and nothing else.
	 * Analog side: if `params->PROBING_MODE` is set, forces @p qcFlag
	 * to 0 and prints a warning (the masked value, not the argument, is
	 * what both the descriptor selection below and
	 * V90Demodulator::reset() see); selects and installs the DIL
	 * descriptor via setDilDescriptor() -- the object's only call of
	 * that function; then calls V90Demodulator::reset(qcFlag). Any
	 * other `side` only prints "Illegal modemSide".
	 *
	 * @param qcFlag  Nonzero to request quick connect (tested with
	 *                `test`, not compared against 1).
	 */
	void reset(unsigned int qcFlag);

	/**
	 * @brief Propagate a new session flag to `this` and whichever side was built.
	 * @param flag  The new session flag value.
	 */
	void setSessionFlag(unsigned int flag);

	/* --- data members; the mangling never carries one (finding F226) --- */

	V90Modulator *modulator;		/* +0x0000 side == 0      */
	V90Demodulator *demodulator;		/* +0x0004 side == 1      */

	/*
	 * +0x0008 and +0x49b4 were carved out of `pad_08` by the
	 * VPcmFloModem batch, which reaches both through the V90Modem
	 * EMBEDDED in a VPcmFloModem at +0x1758 -- so what that batch reads
	 * as `this + 0x1760` and `this + 0x610c` is this object's +0x08 and
	 * +0x49b4.  The constructor confirms both independently:
	 * `sysdep_malloc(0x24)` then `V90Phase2Info::V90Phase2Info` writes
	 * +0x08, and `sysdep_malloc(0x558)` then
	 * `V90Parameters::V90Parameters` writes +0x49b4, and both sizes match
	 * the `sizeof` those two classes already assert.
	 *
	 * `params` KEEPS ITS OFFSET NAME.  Two batches' worth of offset
	 * assertions and one `+ 0x20` cast name it that; only its type was
	 * ever new, and the constructor adds ownership to the record and not
	 * a name.
	 */
	V90Phase2Info *phase2Info;		/* +0x0008 OWNED          */
	V90Jd *jd;				/* +0x000c OWNED          */
	V92Jd *jd92;				/* +0x0010 OWNED          */
	tagV90DILdescriptor *dil;		/* +0x0014 not owned      */

	/*
	 * +0x0018 and +0x0668.  Arguments 6 and 7 of both halves, in that
	 * order -- the constructor loads `this + 0x18` into the sixth slot
	 * and `this + 0x668` into the seventh, and the two are the SAME TYPE,
	 * so nothing but the slot distinguishes them.  Findings F1301 and F1307
	 * are two batches that shipped exactly this pair in the wrong order;
	 * the test drives them through the callee, which is the only place a
	 * swap becomes visible.
	 */
	V90MappingParams mappingParams;		/* +0x0018 argument 6     */
	V90MappingParams mappingParamsAlt;	/* +0x0668 argument 7     */

	tagV90AdditionalCPinfo additionalCPinfo;/* +0x0cb8 argument 8     */
	V90MP mp;				/* +0x0cd0 argument 10    */
	V90CP cp;				/* +0x0df4 argument 9     */

	/*
	 * Named on the unanimous sibling convention: every other class
	 * holding a `V90Parameters *` (V90Phase2Info, V90Jd, V92Jd,
	 * V90Demodulator, V90Modulator and more) already calls it `params`,
	 * and the constructor confirms it directly -- this field is loaded
	 * from the `params` argument and immediately re-passed as `params`
	 * to V90Phase2Info's, V90Jd's and V92Jd's own constructors.
	 */
	V90Parameters *params;		/* +0x49b4 OWNED          */

	unsigned int sessionFlag;		/* +0x49b8                */
	V90ModemSide side;			/* +0x49bc                */
};

#endif /* DSPLIB_V90MODEM_H */
