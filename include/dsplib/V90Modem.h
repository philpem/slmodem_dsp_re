/*
 * V90Modem.h -- the V.90 modem proper: the object VPcmFloModem embeds at
 * +0x1758, and the thing `V90Modem::V90Modem` builds.
 *
 * THIS FILE USED TO SAY "NOT AN OBJECT MAP AND MUST NOT BE READ AS ONE".
 * It carried a class with no data members, safe for `printTitle` and for
 * nothing else, and it ended by saying "whoever reconstructs the rest
 * replaces this file wholesale".  That is what has happened.  The class body
 * that used to live in `include/dsplib/V90SessionFlag.h` -- six fields, the
 * two the VPcmFloModem batch carved out of `pad_08`, and `setSessionFlag` --
 * has moved here unchanged in content, and `V90SessionFlag.h` now includes
 * this file.  Nothing recorded in either place was dropped in the move.
 *
 * NOT POLYMORPHIC.  Two destructors, `D1` at 0x192b0 and `D2` at 0x19160,
 * and no `D0`; a deleting destructor is what GCC emits for a virtual one
 * (finding 228), so there is no vptr and +0x00 is a real member.
 *
 * ===========================================================================
 * WHERE THE LAYOUT COMES FROM
 * ===========================================================================
 *
 * `sizeof(V90Modem) == 0x49c0` is asserted in src/pump/v90/VPcmFloModem.cpp
 * and is not asserted again here; it was settled by the VPcmFloModem batch
 * before any of the fields below were read.  Every field between +0x0c and
 * +0x49b4 -- the span that header called `pad_0c[0x49a8]` -- is named by the
 * CONSTRUCTOR at 0x194e0, and each one is proved twice: once by what the
 * constructor stores there, and once by the position it occupies in the
 * twelve- and fourteen-argument calls to `V90Modulator` and `V90Demodulator`,
 * whose MANGLINGS spell out the type of every parameter.  That second reading
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
 *            So the V90CP runs exactly up to `ptr_49b4` and there is no
 *            unmodelled span left anywhere in this object.
 *
 * THE ONE SIZE THAT RESTS ON ADJACENCY ALONE is `tagV90AdditionalCPinfo`'s
 * 0x18, which is 0xcd0 - 0xcb8 and nothing else -- finding 1320's bound with
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
 * V92Modem.h records the same shape for the same reason (finding 1323).
 */

#ifndef DSPLIB_V90MODEM_H
#define DSPLIB_V90MODEM_H

#include "dsplib/V90CP.h"
#include "dsplib/V90Equalizer.h"	/* V90ComputationalMode lives there */
#include "dsplib/V90MappingParams.h"
#include "dsplib/V90MP.h"

/*
 * POINTERS ONLY, SO FORWARD DECLARATIONS ONLY, and that is load-bearing
 * rather than tidy.  `V90Demodulator.h` drags in `V90PreFilter.h` and with it
 * the OTHER definition of `V90Parameters` (finding 1112), which cannot sit
 * in a translation unit beside the named one in `V90Parameters.h`.  This
 * header is included by `VPcmFloModem.h`, so pulling either in here would
 * decide that question for every one of its users.  A translation unit that
 * needs to DEREFERENCE one of these includes it itself (finding 1325).
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
 * The mangling of the constructor spells both of these
 * (`12V90ModemSide`, `20V90ComputationalMode`) and says nothing about their
 * enumerators, so they are declared and not defined -- finding 226, and the
 * spelling V90Equalizer.h and V92Modem.h already use.
 *
 * `V90ModemSide`'s underlying type is `unsigned int` because the DESTRUCTOR's
 * range test is `cmpl $0x1,0x49bc(%esi); jbe` at 0x192c2 -- an UNSIGNED
 * comparison, where a signed `side > 1` would be `jle`.  That is V92Modem.h's
 * argument for `V92ModemSide` applied to the same shape in the same place.
 * `V90ComputationalMode` is `int` because nothing here constrains it and
 * V90Equalizer.h chose `int` first.  It is REDECLARED here rather than
 * guarded: repeating an opaque-enum declaration is legal exactly as long as
 * the base agrees, so the duplicate is a check that the two headers still
 * agree and not a hazard.  V90Equalizer.h does reach the same translation
 * unit as this file, through V90SessionFlag.h, so the check runs.
 *
 * THAT LAST PARAGRAPH NO LONGER DESCRIBES THE CODE.  C++98 has no opaque
 * enum, so the author cannot have written `enum X : int;`, and a DEFINITION
 * may not be repeated where a declaration could be.  So the duplicate is
 * gone: `V90ComputationalMode` lives in V90Equalizer.h, which this file now
 * includes, and `V90ModemSide` lives here.  One type, one home.
 * docs/method/compilers.md, V2.
 *
 * `V90ModemSide` MUST be unsigned: the destructor's range test is
 * `cmpl $0x1,0x49bc(%esi); jbe` at 0x192c2, and a signed `side > 1` compiles
 * to `jle`/`jg` instead -- confirmed under both compilers, which is what
 * makes the assertion below one about the object rather than about a
 * compiler version.  `_BASE_PIN` is ours; the object names no enumerator.
 */
enum V90ModemSide { V90ModemSide_BASE_PIN = 0xffffffffu };

typedef char v90modem_side_is_unsigned[
    ((enum V90ModemSide)-1 > (enum V90ModemSide)0) ? 1 : -1];

/*
 * The two values the code distinguishes, spelled as macros for V92Modem.h's
 * reason: an enumerator would be a NAME, and the mangling carries none.  0
 * builds the modulator and prints "Digital"; 1 builds the demodulator and
 * prints "Analog".  Which of these `vpcm_create` reaches is settled
 * elsewhere -- it passes a literal 0 to `VPCMXF_Create`, which inverts it, so
 * the shipped side is always ANALOG (findings 701, 702).
 */
#define V90_MODEM_SIDE_DIGITAL	0
#define V90_MODEM_SIDE_ANALOG	1

/*
 * +0xcb8, 0x18 bytes.  The mangling of both modulator constructors names the
 * type (`P22tagV90AdditionalCPinfo`) and this file used to be the one place
 * that needed the definition, because the block is EMBEDDED and a member
 * cannot be incomplete.
 *
 * THE DEFINITION HAS MOVED TO ITS OWN HEADER and this is now an include, for
 * the reason `V90ConnectionEvaluator`'s move had: a second file --
 * V90Demodulator.cpp, whose `enterRRN` writes the record's +0x10 -- needs the
 * complete type, and "one type, one home" means the home moves rather than
 * the definition being spelled twice.  Nothing about the embedded member at
 * +0xcb8 changed.  The size is 0xcd0 - 0xcb8 and is adjacency alone, so it is
 * not asserted -- see the file comment.
 */
#include "dsplib/tagV90AdditionalCPinfo.h"

class V90Modem {
public:
	/*
	 * 0x194e0 (C1) and 0x19740 (C2), 0x255 = 597 bytes each.  The sixth
	 * argument is stored at +0x49b8 and handed on as the LAST argument of
	 * whichever of the two halves gets built.
	 */
	V90Modem(V90ModemSide side, _tagModemParameters *modemParams,
		 tagV90DILdescriptor *dilDescriptor, unsigned int nofSymbols,
		 V90ComputationalMode compMode, unsigned int flag);

	/*
	 * 0x192b0 (D1) and 0x19160 (D2), 0x141 = 321 bytes each.  The two
	 * differ in ONE BYTE, the epilogue's scratch pop -- D1 has `pop %edx`
	 * where D2 has `pop %eax` -- which is register allocation and is the
	 * compiler's free choice.  V92Modem's pair differs the same way and
	 * for the same reason (finding 1324).
	 */
	~V90Modem();

	/*
	 * Nine messages: three ungated banner lines through `edprintf`, three
	 * gated ones through `dsplibs_debug_printf`, then three more ungated
	 * and a gated banner to close.  See src/pump/v90/V90Modem.cpp for the
	 * order, which is not the order the disassembly is laid out in.
	 */
	void printTitle();

	/*
	 * `progress` -- .text+0x19ad0, 188 bytes.  The whole body is the
	 * `side` switch: it forwards its four arguments unchanged to
	 * `V90Modulator::progress` on the digital arm and to
	 * `V90Demodulator::progress` on the analogue one, and prints
	 * "Illegal modemSide" on anything else.
	 *
	 * BOTH LIVE ARMS ARE TAIL JUMPS -- `jmp`, not `call`, at 0x19b3b and
	 * 0x19b65 -- so this function's return type IS the two callees', and
	 * both of those are `void`.  The default arm tail-jumps to
	 * `dsplibs_debug_printf`, whose `int` is therefore returned by
	 * accident on that path alone; a function that really returned a value
	 * would have to agree with itself across the three and this one does
	 * not.
	 *
	 * IT DEREFERENCES A POINTER THE OTHER ARM'S CONSTRUCTOR SET TO NULL,
	 * with no guard.  `side` is the only thing that keeps the two apart,
	 * and the constructor writes the two consistently -- but any value
	 * outside {0, 1} leaves BOTH untouched (see the note above) and this
	 * function's default arm is then the only thing standing between a
	 * seeded object and a wild call.
	 */
	void progress(int *bits, unsigned int &nofBits, float *samples,
		      unsigned int nofSymbols);

	void setSessionFlag(unsigned int flag);

	/* --- data members; the mangling never carries one (finding 226) --- */

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
	 * `ptr_49b4` KEEPS ITS OFFSET NAME.  Two batches' worth of offset
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
	 * so nothing but the slot distinguishes them.  Findings 1301 and 1307
	 * are two batches that shipped exactly this pair in the wrong order;
	 * the test drives them through the callee, which is the only place a
	 * swap becomes visible.
	 */
	V90MappingParams mappingParams;		/* +0x0018 argument 6     */
	V90MappingParams mappingParamsAlt;	/* +0x0668 argument 7     */

	tagV90AdditionalCPinfo additionalCPinfo;/* +0x0cb8 argument 8     */
	V90MP mp;				/* +0x0cd0 argument 10    */
	V90CP cp;				/* +0x0df4 argument 9     */

	V90Parameters *ptr_49b4;		/* +0x49b4 OWNED          */

	unsigned int sessionFlag;		/* +0x49b8                */
	V90ModemSide side;			/* +0x49bc                */
};

#endif /* DSPLIB_V90MODEM_H */
