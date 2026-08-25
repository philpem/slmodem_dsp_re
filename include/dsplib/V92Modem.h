/*
 * V92Modem.h -- the top of the V.92 half of the V.PCM construction chain.
 *
 * THIS IS NOW AN OBJECT MAP, which it was not: the file used to open by
 * saying that no field of the class had been read and that any offset through
 * it was meaningless.  `V92Modem::V92Modem` (.text+0x13d30, C1; +0x13ec0, C2)
 * and `V92Modem::~V92Modem` (+0x13a80, D1; +0x13990, D2) between them name
 * seven of the eight members below, and the two members they do not write are
 * bounded rather than guessed.  See src/pump/v90/V92Modem.cpp.
 *
 * THE SIZE IS 0xaac AND IT IS DERIVED TWICE, not assumed.  The constructor
 * stores a four-byte `modemSide` at +0xaa8, so `sizeof` is at least 0xaac.
 * And `V92Modem` is not heap-allocated anywhere in the object -- it is an
 * EMBEDDED member of `VPcmFloModem`, built in place by
 * `_ZN12VPcmFloModemC1EPv12V90ModemSide...` at .text+0xfae0 with
 * `lea 0x6124(%ebx),%eax`, and the member VPcmFloModem constructs next lives
 * at `lea 0x6bd0(%ebx),%edx`.  0x6bd0 - 0x6124 = 0xaac, so `sizeof` is at
 * most 0xaac.  The two bounds meet.  (Alignment corroborates rather than
 * contradicts: +0x6124 is 4-aligned and not 8-aligned, so `alignof` is at
 * most 4 and 0xaac needs no tail padding.)  Finding F1320.
 *
 * WHAT THE ALLOCATION ORACLE GAVE.  Four of the members are built with
 * `sysdep_malloc(sizeof(X))` immediately before their constructor, so the
 * immediate IS the original compiler's own `sizeof` (finding F1246).  All
 * five of the sizes it hands over were already pinned here by their own
 * classes' allocation sites, and all five agree: 0xdc `V92Parameters`,
 * 0x2c `V92Phase2Info`, 0x918 `V92CP`, 0xb4 `struct V92ParamsInfo`, and 0x90
 * `V92Modulator`.  This constructor confirmed ten of them and invented none.
 *
 * `printTitle` is unchanged and still safe to call through a pointer to
 * nothing: 0x13bf0-0x13c9c never reads the incoming argument slot, and its
 * tail call at 0x13c46 overwrites that slot before jumping.  That is why
 * test/unit/t_printtitle.cpp still drives it over a 64-byte array although
 * the class is now 2,732 bytes -- the array is deliberately smaller than the
 * object and the test says so.
 */

#ifndef DSPLIB_V92MODEM_H
#define DSPLIB_V92MODEM_H

class V92CP;
class V92MappingParams;
class V92Modulator;
class V92Parameters;
class V92Phase2Info;
struct _tagModemParameters;
struct tagV90DILdescriptor;

/*
 * THE TWO ENUMERATED ARGUMENT TYPES, opaque, with a fixed underlying type --
 * the spelling include/dsplib/V90Equalizer.h uses for `V90ComputationalMode`
 * and for the same reason.  The mangling records the enum's NAME
 * (`12V92ModemSide`, `20V92ComputationalMode`) and nothing about its
 * enumerators, so naming them would be putting a guess into the record.
 *
 * The underlying type is `unsigned int` for `V92ModemSide` because the
 * destructor's range test is `cmpl $0x1,0xaa8(%esi); jbe` at .text+0x13a92 --
 * an UNSIGNED compare where a signed `> 1` would have emitted `jle`.  GCC
 * picks `unsigned int` for an enum with no negative enumerator, which is what
 * the two values below are.  Nothing anywhere fixes `V92ComputationalMode`'s,
 * because nothing in the object reads it; `int` there follows the V.90 file.
 *
 * Fixing the underlying type has a second effect the test depends on: every
 * value of that type is then a value of the enum, so `(V92ModemSide)2` is
 * well defined and the "illegal modemSide" arm both functions carry can
 * actually be driven.
 *
 * THE OPAQUE DECLARATION THAT USED TO DO THAT IS C++11, so the author cannot
 * have written it.  A C++98 definition keeps the property, but only with an
 * enumerator to pin the base -- an EMPTY enum has the range 0..0, which would
 * make `(V92ModemSide)2` exactly the undefined behaviour this comment is
 * about.  `_BASE_PIN` is ours; the object names no enumerator.
 * docs/method/compilers.md, V2.
 */
enum V92ModemSide { V92ModemSide_BASE_PIN = 0xffffffffu };
enum V92ComputationalMode { V92ComputationalMode_BASE_PIN = -0x7fffffff - 1 };

typedef char v92modem_side_is_unsigned[
    ((enum V92ModemSide)-1 > (enum V92ModemSide)0) ? 1 : -1];

/*
 * The two the object names, from the string the constructor selects at
 * .text+0x13d64: `test %ebx,%ebx` then "Digital" when zero and "Analog"
 * otherwise, into "V92Modem Construction (as %s Modem)".  The switch that
 * follows gives the same pair their meaning -- the ANALOG side is the one
 * that builds a `V92Modulator`, which is V.92 upstream PCM: the analog client
 * is the transmitter.  Anything else is the "Illegal modemSide" arm.
 */
#define V92_MODEM_SIDE_DIGITAL	0
#define V92_MODEM_SIDE_ANALOG	1

/*
 * The span between +0x00c and +0xa9c, which is the class's `V92Ja` and is
 * modelled as bytes because `V92Ja`'s size is not established -- see
 * include/dsplib/V92Ja.h, which says so in full.  The layout is identical
 * either way, so this is a modelling choice and not a deviation; what it
 * costs is that the span's INTERNAL boundaries stay unclaimed.
 *
 * That the object at +0x00c IS the `V92Ja` is not adjacency: the constructor
 * passes `lea 0xc(%esi),%edx` as the THIRD argument of
 * `_ZN12V92ModulatorC1EjP13V92Phase2InfoP5V92Ja...`, whose third parameter is
 * `V92Ja *`.  `V92Modem::reset` at .text+0x13cf5 then hands +0x00c and +0x010
 * to `_Z22V92DILdescriptorPackerP19tagV90DILdescriptorPhPi` as its `int *`
 * and its `unsigned char *`, which is V92Ja.h's `bitCount` at +0x000 and its
 * byte vector at +0x004 exactly.  How far past +0x010 the vector runs, and
 * whether anything else shares the span, this batch cannot say.
 */
#define V92_MODEM_JA_BYTES	0xa90

class V92Modem {
public:
	/*
	 * .text+0x13d30 (C1) and +0x13ec0 (C2), 0x189 = 393 bytes each and
	 * BYTE-IDENTICAL -- no virtual base, so GCC emitted one body twice.
	 * The fifth argument is never read: no instruction in either copy
	 * touches 0x54(%esp).
	 */
	V92Modem(V92ModemSide side, _tagModemParameters *modemParams,
		 unsigned int nSamples, tagV90DILdescriptor *dilDescriptor,
		 V92ComputationalMode mode);

	/*
	 * .text+0x13a80 (D1) and +0x13990 (D2), 0xe5 = 229 bytes each.  The
	 * two differ in TWO BYTES, both the epilogue's scratch pop -- D1 has
	 * `pop %edx` where D2 has `pop %eax` -- which is register allocation
	 * and is the compiler's free choice.  Finding F1324.
	 */
	~V92Modem();

	/*
	 * Eight messages, and the shape differs from `V90Modem::printTitle`
	 * in two ways that are easy to miss: there is no "Components:" line,
	 * and the closing banner is UNGATED here and gated there.  See
	 * src/pump/v90/V92Modem.cpp.
	 */
	void printTitle();

	/*
	 * .text+0x13ca0, 0x8a = 138 bytes, and .text+0x13b70, 0x79 = 121.
	 *
	 * BOTH ARE THE SAME THREE-ARM SWITCH ON `modemSide` AND BOTH ARMS OF
	 * INTEREST ARE THE ANALOG ONE, which is what makes this pair live code
	 * on the shipped configuration where the V.90 modulator chain is dead:
	 * V.92's upstream PCM is the analogue client transmitting, so the side
	 * that owns a `V92Modulator` is the side that has one to drive.
	 *
	 * The digital arm of each returns having done nothing at all -- no
	 * message, no store -- and the illegal arm prints and returns.  `reset`
	 * calls `printTitle` on EVERY side, before the switch.
	 *
	 * `progress`'s four argument types are the mangling's; its return type
	 * is measured `void`, and so is `reset`'s -- the analog arm of each is a
	 * TAIL CALL into the modulator (`jmp _ZN12V92Modulator11enterPhase3Ev`
	 * at +0x13d25, `jmp _ZN12V92Modulator8progressEPiRjPfj` at +0x13be4),
	 * which would forward a return value if there were one, and the two
	 * callees have none.
	 */
	void reset();
	void progress(int *bits, unsigned int &nbits, float *out,
		      unsigned int nSamples);

	/* Public for `offsetof`; one access section, as everywhere here. */

	/*
	 * +0x000  The `V92Modulator`, and it is OWNED: the destructor calls
	 * `_ZN12V92ModulatorD1Ev` on it and frees it.  Built only on the
	 * analog side; set to NULL on the digital side; and on any other
	 * value of `modemSide` LEFT ALONE -- the constructor's default arm
	 * prints and stores nothing, so this word keeps whatever the storage
	 * held.  Finding F1323.
	 */
	V92Modulator *modulator;

	/*
	 * +0x004  `sysdep_malloc(0xdc)` then
	 * `_ZN13V92ParametersC1EP19_tagModemParameters` on the constructor's
	 * SECOND argument.  Owned: destroyed and freed.
	 */
	V92Parameters *parameters;

	/*
	 * +0x008  `sysdep_malloc(0x2c)` then
	 * `_ZN13V92Phase2InfoC1EP13V92Parameters` on +0x004, which the
	 * constructor RE-READS out of the object rather than reusing the
	 * register.  Freed by the destructor with a bare `sysdep_free` and no
	 * destructor call -- the blob carries no `_ZN13V92Phase2InfoD*`
	 * symbol at all -- and it is the ONLY member the destructor nulls.
	 */
	V92Phase2Info *phase2Info;

	/*
	 * +0x00c  The `V92Ja`, by value and 0xa90 bytes.  Neither constructed
	 * nor destroyed: no `_ZN5V92Ja` symbol is called anywhere near
	 * either function, so the class is trivial in both directions.  Not
	 * written by the constructor either -- the seed survives it.
	 */
	unsigned char ja[V92_MODEM_JA_BYTES];

	/*
	 * +0xa9c  The constructor's FOURTH argument, stored and not owned.
	 * Handed on to `V92Modulator` as its fourth, and to
	 * `V92DILdescriptorPacker` by `reset`.
	 */
	tagV90DILdescriptor *dil;

	/*
	 * +0xaa0  `sysdep_malloc(0xb4)` with NO constructor after it, then
	 * `V92createConstellations` and `V92createFilterCoefficients`.
	 *
	 * THIS IS `struct V92ParamsInfo`.  The identification is not the size
	 * alone: the same pointer is the SIXTH argument of
	 * `_ZN12V92ModulatorC1E...P16V92MappingParamsP13V92Parameters`, whose
	 * sixth parameter the mangling spells `V92MappingParams *`, and the
	 * two C functions that fill it take `struct V92ParamsInfo *`.  One
	 * 180-byte block, two names, and include/dsplib/V92ParamsInfo.h had
	 * already reached it from the other end.  Finding F1321.
	 *
	 * The type here is the MANGLING'S, so that the argument the modulator
	 * receives needs no explanation; the .cpp casts at the four C call
	 * sites and says why each time.
	 */
	V92MappingParams *mappingParams;

	/*
	 * +0xaa4  `sysdep_malloc(0x918)` then `_ZN5V92CPC1Ev`, no arguments.
	 * Owned: destroyed and freed.
	 */
	V92CP *cp;

	/*
	 * +0xaa8  The constructor's FIRST argument, stored before anything is
	 * allocated, and RE-READ out of the object by the switch that
	 * follows -- so the switch is on the member and not on the parameter.
	 * The destructor reads it too.
	 */
	V92ModemSide modemSide;
};

#endif /* DSPLIB_V92MODEM_H */
