/**
 * @file V92Modem.h
 * @brief The top of the V.92 half of the V.PCM construction chain: owns the
 *        V.92 parameter/CP/phase-2-info sub-objects and, on the analog side,
 *        the `V92Modulator` that does the actual work.
 *
 * `V92Modem::V92Modem` and `V92Modem::~V92Modem` between them name seven of
 * the eight members below; the two they do not write are bounded rather than
 * guessed. `reset()` and `progress()` are the class's whole run-time surface,
 * both three-arm switches on `modemSide` that tail-call into the modulator
 * on the analog arm and do nothing on the digital one.
 *
 * `sizeof(V92Modem)` is 0xaac, derived twice rather than assumed: the
 * constructor's `modemSide` store at +0xaa8 gives a floor of 0xaac, and
 * `V92Modem` is never heap-allocated -- it is an embedded member of
 * `VPcmFloModem`, built in place at a fixed displacement from the next
 * member `VPcmFloModem` constructs, and the gap between the two is exactly
 * 0xaac (finding F1320). Four of the five owned sub-objects are built with
 * `sysdep_malloc(sizeof(X))` immediately before their constructor, so the
 * immediate is the original compiler's own `sizeof` (finding F1246): 0xdc
 * `V92Parameters`, 0x2c `V92Phase2Info`, 0x918 `V92CP`, 0xb4
 * `struct V92ParamsInfo`, 0x90 `V92Modulator` -- all independently confirmed
 * by those classes' own headers.
 *
 * `printTitle()` never reads its `this` pointer's tail (it tail-calls into
 * `printTitle`-adjacent code that overwrites the slot first), which is why
 * `test/unit/t_printtitle.cpp` can drive it over an array much smaller than
 * the real 2,732-byte class.
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
 * The two enumerated argument types, opaque, with a fixed underlying type --
 * the same spelling include/dsplib/V90Equalizer.h uses for
 * `V90ComputationalMode` and for the same reason. The mangling records the
 * enum's name and nothing about its enumerators, so naming them would put a
 * guess into the record.
 *
 * `V92ModemSide`'s underlying type is `unsigned int`, measured rather than
 * assumed: the destructor's range test (`cmpl $0x1,0xaa8(%esi); jbe`) is
 * unsigned, where a signed `> 1` would emit `jle`. `V92ComputationalMode`'s
 * is not fixed by any evidence -- nothing in the object reads it -- and
 * follows the V.90 file's `int` for consistency.
 *
 * Fixing the underlying type also makes every value of that type a value of
 * the enum, so `(V92ModemSide)2` is well defined and the "illegal modemSide"
 * arm both `reset()` and `progress()` carry can actually be driven by a test.
 * Spelled as a `_BASE_PIN` enumerator rather than an opaque `enum : unsigned`
 * declaration, since a fixed base is C++11 and the author's compiler was
 * C++98 (docs/method/compilers.md, V2); the pin is ours; the object names no
 * enumerator.
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
 * The span between +0x00c and +0xa9c is the class's embedded `V92Ja`,
 * modelled as bytes because `V92Ja`'s own size is not established (see
 * V92Ja.h) -- a modelling choice, not a deviation, since the layout is
 * identical either way. What it costs is that the span's internal
 * boundaries stay unclaimed.
 *
 * That +0x00c is a `V92Ja` and not merely adjacent to one: the constructor
 * passes it as `V92Modulator`'s third argument, mangled `V92Ja *`, and
 * `reset()` hands +0x00c and +0x010 to `V92DILdescriptorPacker` as exactly
 * V92Ja.h's `bitCount` and byte-vector fields. How far past +0x010 the
 * vector runs, and whether anything else shares the span, is not settled.
 */
#define V92_MODEM_JA_BYTES	0xa90

class V92Modem {
public:
	/**
	 * @brief Construct the modem: allocate and build `parameters`,
	 *        `phase2Info`, `mappingParams` and `cp` unconditionally, plus
	 *        `modulator` on the analog side only.
	 * @param side           Which side of the link this instance is;
	 *                       decides whether `modulator` is built.
	 * @param modemParams    Forwarded to `parameters`'s constructor.
	 * @param nSamples       Forwarded to `modulator`'s constructor
	 *                       (analog side only).
	 * @param dilDescriptor  Stored as `dil` and forwarded to `modulator`.
	 * @param mode           Forwarded to `modulator`'s constructor;
	 *                       otherwise unused (the fifth argument is never
	 *                       read on the digital/illegal arms).
	 */
	V92Modem(V92ModemSide side, _tagModemParameters *modemParams,
		 unsigned int nSamples, tagV90DILdescriptor *dilDescriptor,
		 V92ComputationalMode mode);

	/**
	 * @brief Destroy and free the four unconditionally-owned sub-objects
	 *        plus `modulator`, if built.
	 */
	~V92Modem();

	/**
	 * @brief Log an eight-message construction banner. Differs from
	 *        `V90Modem::printTitle` in having no "Components:" line and
	 *        in leaving its closing banner ungated.
	 */
	void printTitle();

	/**
	 * @brief Reset the modem for a new connection: call printTitle(),
	 *        then on the analog side tail-call into
	 *        `V92Modulator::enterPhase3`. A no-op on the digital side;
	 *        logs an "illegal modemSide" message on any other value.
	 */
	void reset();
	/**
	 * @brief Run one block through the modem. On the analog side,
	 *        tail-calls into `V92Modulator::progress` with the same
	 *        arguments; a no-op on the digital side, an "illegal
	 *        modemSide" log on any other value.
	 * @param bits      Input bits, forwarded to the modulator.
	 * @param nbits     In/out bit count, forwarded to the modulator.
	 * @param out       Output samples, forwarded to the modulator.
	 * @param nSamples  Samples requested, forwarded to the modulator.
	 */
	void progress(int *bits, unsigned int &nbits, float *out,
		      unsigned int nSamples);

	/* Public for `offsetof`; one access section, as everywhere here. */

	/* +0x000  Owned; built only on the analog side, NULL on the digital
	 * side, and left holding whatever the allocation gave on any other
	 * `modemSide` (the constructor's illegal-side arm never touches it),
	 * finding F1323. */
	V92Modulator *modulator;

	/* +0x004  Owned; built from the constructor's second argument. */
	V92Parameters *parameters;

	/* +0x008  Owned, but freed with a bare `sysdep_free` and no
	 * destructor call -- the blob has no `V92Phase2Info` destructor at
	 * all -- and the only member the destructor nulls afterwards. */
	V92Phase2Info *phase2Info;

	/* +0x00c  The `V92Ja`, embedded by value (0xa90 bytes); neither
	 * constructed nor destroyed, and not written by the constructor --
	 * see V92Ja.h. */
	unsigned char ja[V92_MODEM_JA_BYTES];

	/* +0xa9c  The constructor's fourth argument, stored and not owned;
	 * handed to `V92Modulator` and to `V92DILdescriptorPacker` (by
	 * `reset`). */
	tagV90DILdescriptor *dil;

	/*
	 * +0xaa0  Allocated with no constructor call, then filled by
	 * `V92createConstellations`/`V92createFilterCoefficients` -- this is
	 * `struct V92ParamsInfo`, identified by more than size: the same
	 * pointer is `V92Modulator`'s sixth constructor argument, whose
	 * mangling spells it `V92MappingParams *` (finding F1321; see
	 * V92ParamsInfo.h). The type here is the mangling's; the .cpp casts
	 * at each C call site.
	 */
	V92MappingParams *mappingParams;

	/* +0xaa4  Owned; built with no constructor arguments. */
	V92CP *cp;

	/* +0xaa8  The constructor's first argument, stored before any
	 * allocation and re-read out of the object (not the parameter) by
	 * every switch on it, including the destructor's. */
	V92ModemSide modemSide;
};

#endif /* DSPLIB_V92MODEM_H */
