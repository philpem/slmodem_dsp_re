/**
 * @file V90PreFilter.h
 * @brief The V.90 receive pre-filter: a `FloatFIR` base plus the logic that
 *        chooses its coefficients from what Phase 2 measured about the line
 *        (or from the V.90 parameter registry, when it has an opinion).
 *
 * Reconstructed from dsplibs.o. All twenty-four members (fourteen methods
 * and ten static coefficient/table members, 23,860 bytes of the latter) are
 * now written, across a lifecycle batch (finding F1233), a filter-accessor
 * batch and the original batch 3 (`selectFilter`, `setParamEia6`,
 * `autoSelection`, `isV90WithEia6`, `displayParamEia6`).
 *
 * Not polymorphic: the destructor has the two ordinary variants and not the
 * deleting `D0`, so there is no vptr and offset 0 is a real member (finding
 * F228). The object is 40 bytes (0x28), not the 1,280 an earlier bound
 * mistakenly took from the largest displacement any method uses -- most of
 * that reach is `setParamEia6` and `isV90WithEia6` indexing into the
 * `V90Parameters` block pointed at by `params`, not into `this` (finding
 * F234). The largest true `this` displacement is +0x24.
 *
 * `FloatFIR` is a public base class, not a member -- the object says so
 * outright rather than leaving it ambiguous: GCC selects the base-object
 * constructor/destructor variants (C2/D2) for a base subobject and the
 * complete-object ones (C1/D1) for a member, and every site in the blob
 * that builds or tears down the embedded FIR uses the base variants, which
 * are distinct symbols at distinct addresses from the complete-object ones.
 * Modelling `fir` as a member instead cost both destructors their byte
 * identity (finding F8080). The cost of modelling it as a base is that the
 * class is no longer standard-layout, so the `.cpp`'s `__builtin_offsetof`
 * assertions are conditionally supported rather than well defined; GCC
 * accepts them on both compilers, and the assertion on the FIR itself is
 * replaced by one on `sizeof(FloatFIR)`, which pins the same fact.
 */

#ifndef DSPLIB_V90PREFILTER_H
#define DSPLIB_V90PREFILTER_H

#include "dsplib/FloatFIR.h"

/**
 * @brief One reference loop: a name, the six-point signature Phase 2's
 * measurement is matched against, and what to do when it wins.
 *
 * The shape is read off the code rather than a document: `autoSelection()`
 * steps the array 0x44 at a time to a zero-length name, matches the
 * measurement against `signature`, and returns `gain` for the closest;
 * `selectFilter`/`setFilter`/`getFilterPointer` switch on `coefType` to
 * choose the coefficient bank (`edprintf`'s "Pre Filter Coeffs Type array
 * %d" is where the name comes from); `isV90WithEia6`/`getV90Capability` test
 * `capability` against 2.
 */
struct V90RefLoop {
	char name[32];		/* +0x00 zero-length ends the array         */
	float signature[6];	/* +0x20 what autoSelection matches against */
	int coefType;		/* +0x38 1, 2 or 3: which bank              */
	int gain;		/* +0x3c the row within it                  */
	int capability;		/* +0x40 2 means EIA-6                      */
};

/**
 * @brief One hardware codec: its name, and the reference loops measured for
 * it. The constructor prints the name ("HardwareCodecType: %s") and counts
 * the table by walking until a name's first byte is zero.
 */
struct V90CodecEntry {
	char name[32];		/* +0x00 */
	V90RefLoop *loops;	/* +0x20 */
};

/*
 * V90Phase2Info was stubbed here too, as an opaque 0x1c-byte block; finding
 * F255 modelled it properly and this include replaces that stub. The 0x1c
 * was only the furthest `autoSelection` reaches -- the real object is 0x24,
 * reached by the constructor and `setToDefault`, neither a member of this
 * class (finding F215's bound-from-what-you-happen-to-write mistake).
 */
#include "dsplib/V90Phase2Info.h"

/*
 * V90Parameters is modelled, and this header used to carry a second, smaller
 * definition of it (0x504 against the real 0x558, from how far five methods
 * happened to reach) -- undefined behaviour wherever both were visible, and
 * what V90ModemCtor.cpp's long comment about which header it may not include
 * is about. One definition now, in V90Parameters.h; the raw word and float
 * views the block form provided live there as V90PW()/V90PF()/V90PB().
 * Finding F1112.
 */
#include "dsplib/V90Parameters.h"

/*
 * Not a size, and never was: the furthest V90PreFilter's methods reach into
 * V90Parameters (`isV90WithEia6` reads +0x500). `sizeof(V90Parameters)` is
 * 0x558. The tests use this as the boundary of the region they exercise --
 * a guard that everything above it stays untouched -- a different job from
 * a size, which is why it survives alongside the real one.
 */
#define V90PARAMETERS_BOUND 0x504

/*
 * `__tHardwareCodecTypes__` has its own header because V90ModemCtor.cpp needs
 * the type and must not have this one -- see V90CodecType.h.
 */
#include "dsplib/V90CodecType.h"

/**
 * @brief Which coefficient bank a filter gain is drawn from. Named by
 * `setFilter`'s mangling; defined rather than opaquely declared because
 * C++98 (the author's own compiler) has no opaque enum declaration.
 *
 * Three values are recovered and their names are not: every dispatching
 * site (`setFilter(PreFilterCoefType, unsigned)`, and the `V90RefLoop::
 * coefType` switches in `setFilter(unsigned)`, `getFilterPointer`,
 * `getFilterLength` and `selectFilter`) tests against exactly 2 and 3,
 * treats 1 as its own arm, and falls to a complaining default -- so the
 * values are 1, 2 and 3, each selecting the coefficient bank of the same
 * number, and every site's default arm leaves room for more. No enumerators
 * are added for them: what each value selects is known, what the author
 * called it is not, and an invented name would be believed by every later
 * reader with no test able to catch it wrong (CLAUDE.md's naming rule). The
 * `.cpp`'s case labels are therefore plain integers.
 *
 * `_BASE_PIN` fixes only the underlying type, and is load-bearing for the
 * differential test as well: it is what makes `(PreFilterCoefType)7` a
 * value of the enumeration rather than undefined, so a sweep can drive the
 * default arm. docs/method/compilers.md, V2.
 */
enum PreFilterCoefType { PreFilterCoefType_BASE_PIN = -0x7fffffff - 1 };

class V90PreFilter : public FloatFIR {
public:
	/**
	 * @brief Load the FIR with the coefficients this connection wants.
	 *
	 * Five paths, differing in where the bank and row come from: the
	 * registry's ISDN NT1 or PBX ISDN settings (row from a fixed offset,
	 * bank from the codec's first reference loop, row unclamped), the
	 * automatic path (autoSelection() picks the loop), or the registry
	 * forcing a bank directly. The row clamp then depends only on the
	 * resulting bank: 30 for the 20-tap banks, 20..50 for the 40-tap one.
	 */
	void selectFilter();

	/**
	 * @brief Move the EIA-6 timing parameters into place inside the
	 * V90Parameters registry.
	 *
	 * Touches `this` at exactly one offset (`params`) and does the rest
	 * of its work -- twenty-six word copies, a clock-deviation report,
	 * and (only if the deviation is non-zero) a float store and an
	 * eighteen-word block move plus four more words -- entirely inside
	 * the registry block. The offsets stay numeric because
	 * `V90Parameters` is not modelled at this granularity.
	 */
	void setParamEia6();

	/**
	 * @brief Pick the reference loop whose six-point signature is
	 * closest, in squared Euclidean distance, to what Phase 2 measured.
	 *
	 * Leaves `refLoop` unchanged if the table has no entries (the
	 * starting distance is deliberately unreachable); a NaN measurement
	 * is reachable and is treated as closer than anything, following the
	 * object's unordered-compare branch.
	 * @return The row (`gain`) of the selected loop.
	 */
	int autoSelection();

	/**
	 * @brief Whether this connection is V.90 with EIA-6 timing.
	 *
	 * True if the selected reference loop (if any) is marked capability
	 * 2, or if the registry's own EIA-6 flag is set.
	 */
	int isV90WithEia6() const;

	/**
	 * @brief No-op, kept as a defined symbol something may call. One
	 * byte (`ret`) in the object.
	 */
	void displayParamEia6();

	/**
	 * @brief Construct: build the embedded FIR (40 taps, no coefficients
	 * yet, 99 samples of slack), resolve which hardware codec applies,
	 * and store @p info and @p params.
	 *
	 * The registry's own codec setting wins over @p codec whenever it is
	 * non-negative; a negative registry codec is deliberately transcribed
	 * as reachable (docs/deviations.md D176: the resolved index can walk
	 * one past the last table entry).
	 * @param codec   The caller's requested hardware codec, used only
	 *                when the registry has no opinion.
	 * @param info    The Phase 2 measurement this filter will be
	 *                selected from.
	 * @param params  The V.90 parameter registry.
	 */
	V90PreFilter(__tHardwareCodecTypes__ codec, V90Phase2Info *info,
		     V90Parameters *params);

	/**
	 * @brief Destructor.
	 */
	~V90PreFilter();

	/**
	 * @brief Reset the filter to its post-construction state: clear the
	 * FIR's history, deselect any reference loop, and reinstall
	 * coefficient bank 1's first row (gain 0).
	 */
	void reset();

	/**
	 * @brief Install the coefficients for a new filter gain, doing
	 * nothing if it is already the installed gain.
	 * @param gain  The row within the selected bank to install.
	 */
	void setFilter(unsigned int gain);

	/**
	 * @brief Install a bank and row chosen directly by the caller, with
	 * no reference loop involved and no clamp on the row.
	 * @param type  Which coefficient bank (1, 2 or 3; see
	 *              ::PreFilterCoefType).
	 * @param gain  The row within that bank.
	 */
	void setFilter(PreFilterCoefType type, unsigned int gain);

	/**
	 * @brief The coefficient pointer for a filter gain.
	 *
	 * With no reference loop selected the row is unclamped; otherwise
	 * the bank comes from the loop's `coefType` and the row is clamped
	 * to that bank's last row (30 for the 20-tap banks, bank 3's own
	 * offset row for the 40-tap one).
	 * @param gain  The row to look up.
	 * @return Pointer to the first coefficient of that row.
	 */
	float *getFilterPointer(unsigned int gain);

	/**
	 * @brief The tap count for the currently selected bank: 40 for bank
	 * 3, 20 for bank 1, bank 2, or no reference loop selected. @p gain
	 * is part of the mangled signature but is not read.
	 * @param gain  Unused.
	 * @return 20 or 40.
	 */
	unsigned int getFilterLength(unsigned int gain);

	/**
	 * @brief This connection's V.90 capability: 1 if EIA-6 (see
	 * isV90WithEia6()), otherwise the selected reference loop's own
	 * capability word. Selects a reference loop first if none is
	 * selected yet.
	 */
	int getV90Capability();

	/**
	 * @brief How many reference loops this codec's table has, counted to
	 * the first zero-length name.
	 */
	int getNofRefLoops() const;

	/*
	 * Data members are public because the original's access specifiers are
	 * not recoverable, and because one access section keeps the class
	 * standard-layout.  The names are invented; the mangling never carries
	 * a data member's name.
	 */
	/*
	 * +0x00 to +0x13 is the FloatFIR base subobject, twenty bytes; see
	 * the note at the top of this file for why it is a base and not a
	 * member.  Its own fields are inherited, so `setCoefficients`,
	 * `process` and `FloatFIR::reset` are called unqualified here.
	 */
	/** @brief Index into #dataBase for this connection's hardware codec. */
	int codecType;			/* +0x14 */

	/** @brief The constructor's Phase 2 measurement argument. */
	V90Phase2Info *phase2;		/* +0x18 */

	/** @brief The constructor's V90Parameters registry argument. */
	V90Parameters *params;		/* +0x1c */

	/** @brief "Filter Gain": the row within the selected bank. */
	int gain;			/* +0x20 */

	/** @brief Index into #dataBase's loop array; -1 for none selected. */
	int refLoop;			/* +0x24 */

	/*
	 * The ten static members. All are `D` in the blob, so none is const,
	 * and `float *` is what setCoefficients takes anyway. The six
	 * refLoopsType* tables are declared here rather than with the only
	 * function that names them in .text, because `dataBase`'s definition
	 * carries sixteen relocations into these six, so the batch that
	 * defines `dataBase` defines them too (finding F234). There is no
	 * refLoopsType3.
	 */
	static float preFilterCoefType1[31][20];
	static float preFilterCoefType2[31][20];
	static float preFilterCoefType3[31][40];
	static V90CodecEntry dataBase[17];
	static V90RefLoop refLoopsType1[23];
	static V90RefLoop refLoopsType2[34];
	static V90RefLoop refLoopsType4[36];
	static V90RefLoop refLoopsType5[36];
	static V90RefLoop refLoopsType6[34];
	static V90RefLoop refLoopsType7[34];
};

#endif /* DSPLIB_V90PREFILTER_H */
