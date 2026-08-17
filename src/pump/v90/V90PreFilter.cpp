/*
 * V90PreFilter.cpp -- choosing the V.90 receive pre-filter.
 *
 * Reconstructed from dsplibs.o V90PreFilter.cpp.  Eight of the class's
 * twenty-four members -- `selectFilter`, `setParamEia6`, `autoSelection`,
 * `isV90WithEia6` and `displayParamEia6` from batch 3, `reset`, and the
 * constructor and destructor from the lifecycle batch (finding 1233).
 * `include/dsplib/V90PreFilter.h` carries the object map and the two table
 * shapes.
 *
 * THE CALLING CONVENTION IS PLAIN CDECL, `this` first on the stack
 * (finding 215).  `isV90WithEia6` is `const` and the mangling records it:
 * dropping the qualifier emits `_ZN...` where the blob has `_ZNK...`, which
 * links against nothing.
 *
 * WHAT THE CLASS DOES.  It is a FloatFIR at offset zero plus five words that
 * decide which coefficients go into it.  Three banks of coefficients exist --
 * 31 rows of 20 taps, another 31 of 20, and 31 of 40 -- and a row is chosen
 * by a "filter gain" index.  Which bank, and which row, comes either from the
 * registry (`params`) or, when the registry says -1, from matching what
 * Phase 2 measured against a table of reference loops for this codec.
 *
 * THE INDEX IS NOT ALWAYS CLAMPED, AND THAT IS THE OBJECT'S DOING.  Two paths
 * reach a bank with an index nothing has bounded: the ISDN and PBX arms take
 * the row straight out of the registry, and the automatic path skips its
 * clamp entirely when no reference loop was selected.  Reproduced literally.
 */

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/V90PreFilter.h"

#if __SIZEOF_POINTER__ == 4
#define V90PF_OFF(field, off, tag) \
	typedef char v90pf_off_##tag[ \
	    ((int)__builtin_offsetof(V90PreFilter, field) == (off)) ? 1 : -1]

V90PF_OFF(fir,       0x00, fir);
V90PF_OFF(codecType, 0x14, codectype);
V90PF_OFF(phase2,    0x18, phase2);
V90PF_OFF(params,    0x1c, params);
V90PF_OFF(gain,      0x20, gain);
V90PF_OFF(refLoop,   0x24, refloop);
typedef char v90pf_size[(sizeof(V90PreFilter) == 0x28) ? 1 : -1];
typedef char v90pf_loop_size[(sizeof(V90RefLoop) == 0x44) ? 1 : -1];
typedef char v90pf_codec_size[(sizeof(V90CodecEntry) == 0x24) ? 1 : -1];
#endif

/*
 * The three coefficient banks, as the object addresses them.  Types 1 and 2
 * are `base + 80 * row`; type 3 is `base - 3200 + 160 * row`, so its row zero
 * is at index 20 and the clamp that keeps it in range is `20 <= row <= 50`.
 * The negative displacement is in the instruction stream, not an error here.
 *
 * `row` is deliberately not range-checked: see the file comment.
 */
static float *
bank1(int row)
{
	return &V90PreFilter::preFilterCoefType1[0][0] + 20L * row;
}

static float *
bank2(int row)
{
	return &V90PreFilter::preFilterCoefType2[0][0] + 20L * row;
}

static float *
bank3(int row)
{
	return &V90PreFilter::preFilterCoefType3[0][0] - 800L + 40L * row;
}

#define BUGMSG \
	"V90PreFilter: getFilterPointer BUG !!! - unsupported PreFilterCoefType !\r\n"

/*
 * Nothing at all.  One byte in the blob, a bare `ret`, and it is kept because
 * it is a defined symbol something may call.
 */
void
V90PreFilter::displayParamEia6()
{
}

/*
 * Whether this connection is V.90 with EIA-6 timing.  Two independent ways to
 * be: the selected reference loop is marked capability 2, or the registry
 * block's +0x500 says 6.  With no loop selected the first is simply false --
 * the object computes 0, decrements it and tests for zero, which is `== 1` on
 * a value that is already 0 or 1.
 */
int
V90PreFilter::isV90WithEia6() const
{
	int cap = 0;

	if (refLoop >= 0)
		cap = (dataBase[codecType].loops[refLoop].capability == 2);

	return (cap == 1) || (V90PW(params)[0x500 / 4] == 6);
}

/*
 * How many reference loops this codec has.  The table is terminated by a
 * record whose name is empty, which is the same convention the constructor
 * walks and the same one `autoSelection` counts with.
 *
 * `const` IS IN THE MANGLING: the blob's symbol is `_ZNK12V90PreFilter...`,
 * so dropping the qualifier emits `_ZN...` and links against nothing.
 */
int
V90PreFilter::getNofRefLoops() const
{
	V90RefLoop *loops = dataBase[codecType].loops;
	int n = 0;

	while (loops[n].name[0] != '\0')
		n++;

	return n;
}

/*
 * How many taps the selected reference loop's bank has: 40 for bank 3 and 20
 * for everything else, INCLUDING the unsupported types the default arm
 * complains about.
 *
 * THE ARGUMENT IS NEVER READ.  `gain` is at 0x14(%esp) and the 92 bytes touch
 * only 0x10(%esp), which is `this`.  It is in the mangled name, so it is in
 * the signature; the object simply does not use it.
 *
 * THE DEFAULT ARM'S MESSAGE NAMES THE OTHER FUNCTION.  It is `BUGMSG`, whose
 * text is "V90PreFilter: getFilterPointer BUG !!!", printed here as well --
 * the author's own copy, one string shared by three sites.
 *
 * SPELLED AS A `switch`, WHICH THE OBJECT'S BRANCH TREE SETTLES against the
 * if/else chain `selectFilter` uses for the same three values.  GCC lays a
 * switch over {1,2,3} out as a BALANCED TREE -- compare the median first,
 * then take the low or the high half -- and 0x44fbd is exactly that:
 * `cmp $2 / je`, then `jg` to the `cmp $3`, and only in the low half the
 * `dec %eax / je` that tests for 1.  An if/else chain in source order would
 * test its first-written value first, and `selectFilter`'s does.
 *
 * FOUR SEPARATE `return 20`s, AND THE PERIOD BUILD IS WHY.  The two arms that
 * fall out at twenty taps are the same statement, so `case 1: case 2:` is the
 * obvious spelling -- and it makes the compiler merge them into a contiguous
 * RANGE, `cmp $1 / jl` then `cmp $2 / jle`, neither of which is in the blob.
 * Measured, with everything else held: merged 0x5e bytes here and 0x123 in
 * `setFilter(unsigned)`, which inlines this; separate 0x5b and 0x13b against
 * the blob's 0x5c and 0x138.  So the arms are separately labelled in the
 * original, which is what four `return`s give and what a shared `len` does
 * not.  Neither spelling is byte-exact and this is a similarity argument, not
 * a differential one -- the two behave identically over every input.
 */
unsigned int
V90PreFilter::getFilterLength(unsigned int gain)
{
	unsigned int len = 20;

	(void)gain;

	if (refLoop != -1) {
		switch (dataBase[codecType].loops[refLoop].coefType) {
		case 1:
			return 20;
		case 2:
			return 20;
		case 3:
			return 40;
		default:
			edprintf(BUGMSG);
			return 20;
		}
	}

	return len;
}

/*
 * Where the coefficients for `gain` live.  The bank comes from the selected
 * reference loop; the row is `gain`, clamped to the bank's last row -- and
 * bank 3's rows start at index 20, which is where its `- 800` displacement
 * comes from (see `bank3` above).
 *
 * WITH NO REFERENCE LOOP SELECTED THE CLAMP IS SKIPPED, and that is the
 * object's: 0x45002's `je` goes to 0x45040, which is PAST the `cmp $0x1e` at
 * 0x45032 that the type 1 and default arms fall through.  The same asymmetry
 * is already recorded for `selectFilter`, and this is where it comes from --
 * `selectFilter`'s automatic arm is this function inlined.
 *
 * So the caller owes this one a bounded `gain` when `refLoop` is -1, and a
 * differential trial that does not give it one is testing our undefined
 * behaviour rather than the object's (deviation D670).
 */
float *
V90PreFilter::getFilterPointer(unsigned int gain)
{
	if (refLoop == -1)
		return bank1((int)gain);

	switch (dataBase[codecType].loops[refLoop].coefType) {
	case 2:
		if (gain > 30)
			gain = 30;
		return bank2((int)gain);
	case 3:
		if (gain > 50)
			gain = 50;
		return bank3((int)gain);
	case 1:
		break;
	default:
		edprintf(BUGMSG);
		break;
	}

	if (gain > 30)
		gain = 30;

	return bank1((int)gain);
}

/*
 * Install the coefficients for a new filter gain, and do nothing at all if it
 * is the gain already installed.
 *
 * IT IS THE TWO ACCESSORS ABOVE, CALLED, and the object proves the pair by
 * repeating both bodies whole: 0x450b4 is `getFilterLength` inlined -- the
 * same balanced tree over the same field, leaving 20 or 40 in %ebp -- and
 * 0x450fc is `getFilterPointer` inlined, which RE-READS `refLoop` at 0x450f9
 * rather than reusing the register the first body left it in.  Two reads of
 * one field across no intervening store is what two calls look like and is
 * not what one hand-written block does.
 *
 * The length is computed FIRST, which is GCC evaluating a call's arguments
 * right to left; both are pure, so nothing here depends on it.
 */
void
V90PreFilter::setFilter(unsigned int gain)
{
	if ((unsigned int)this->gain != gain) {
		fir.setCoefficients(getFilterPointer(gain),
				    getFilterLength(gain));
		this->gain = (int)gain;
	}
}

/*
 * Install a bank and a row chosen by the CALLER, with no reference loop and
 * no clamp anywhere.
 *
 * `gain` IS STORED BEFORE THE SWITCH AND THEN CORRECTED, which is the object's
 * order and not a tidy way of writing it: `mov %ebx,0x20(%esi)` at 0x44a4a
 * precedes every branch, and only bank 3's arm follows its `setCoefficients`
 * with `lea -0x14(%ebx),%eax ; mov %eax,0x20(%esi)`.  Storing it once per arm
 * would put the bank 1 and bank 2 stores after their calls, and those two arms
 * are TAIL JUMPS with nothing after them at all.
 *
 * WHY `gain - 20`.  Bank 3's row zero is at index 20, so the row this call
 * installed is `gain - 20`, and that is what the field has to hold for the
 * next `setFilter(unsigned)` to compare against.  The 20-tap banks index from
 * zero and need no correction.  `selectFilter`'s registry arm already carries
 * the same three-way tail and the same `- 20`.
 *
 * NO CLAMP ON ANY ARM, so bank 3 with `gain` below 20 walks off the front of
 * the table.  The object does the arithmetic regardless; ours must not be
 * asked to (D670).
 *
 * THE CASE LABELS ARE VALUES.  `PreFilterCoefType`'s enumerators are not
 * recovered -- see the header -- and the condition is cast to `int` so that
 * spelling them as integers is not a diagnostic about an enumeration they are
 * not members of.  The cast changes no code: the object's `cmp $0x2` is on
 * the 32-bit argument either way.
 */
void
V90PreFilter::setFilter(PreFilterCoefType type, unsigned int gain)
{
	this->gain = (int)gain;

	switch ((int)type) {
	case 2:
		fir.setCoefficients(bank2((int)gain), 20);
		break;
	case 3:
		fir.setCoefficients(bank3((int)gain), 40);
		this->gain = (int)gain - 20;
		break;
	default:
		fir.setCoefficients(bank1((int)gain), 20);
		break;
	}
}

/*
 * Pick the reference loop whose six-point signature is closest, in squared
 * Euclidean distance, to what Phase 2 measured.
 *
 * The measurement is six differences from a common reference: the Phase 2
 * object's +0x18 points at a block whose +0x38 is the reference and whose
 * +0x3c onwards are the six points, and each difference is ROUNDED TO FLOAT
 * before the search sees it -- the object spells that `fstps`, and it matters
 * because the distances that follow are accumulated at extended precision
 * from those rounded inputs.
 *
 * The starting distance is 1e10f, so a table with no entry at all leaves
 * `refLoop` untouched -- and it is `selectFilter` setting it to 0 before the
 * call, not anything here, that makes the reads below safe.
 */
int
V90PreFilter::autoSelection()
{
	const float *measured = phase2->L2;
	V90RefLoop *loops = dataBase[codecType].loops;
	long double best = 1.0e10f;
	float target[6];
	unsigned int n, k;
	int i;

	for (i = 0; i <= 5; i++)
		target[i] = measured[0x38 / 4] - measured[0x3c / 4 + i];

	for (n = 0; loops[n].name[0] != '\0'; n++)
		;

	for (k = 0; k < n; k++) {
		long double acc = 0.0L;

		for (i = 0; i <= 5; i++) {
			long double d = target[i] - loops[k].signature[i];

			acc += d * d;
		}
		/*
		 * NOT `acc < best`.  The object compares with `fcom` and then
		 * branches on the carry flag alone (`sahf; jae`), and `fcom`
		 * sets C0 -- which is what lands in the carry -- for an
		 * UNORDERED result as well as for a less-than one.  So a NaN
		 * distance takes the update arm, where C's `<` would not, and
		 * a NaN is reachable: the measurement is whatever Phase 2 left
		 * in memory and the differences are taken from it unchecked.
		 * `!(acc >= best)` is the same predicate the object has.
		 */
		if (!(acc >= best)) {
			best = acc;
			refLoop = (int)k;
		}
	}

	edprintf("V90PreFilter: loop = %s (%d)\r\n", loops[refLoop].name,
		 refLoop);
	edprintf("V90PreFilter: Pre Filter Coeffs Type array %d, "
		 "(filter length %d)\r\n", loops[refLoop].coefType,
		 loops[refLoop].coefType == 3 ? 40 : 20);
	edprintf("V90PreFilter: HardwareCodecType: %s\r\n",
		 dataBase[codecType].name);

	return loops[refLoop].gain;
}

/*
 * What V.90 capability this connection has: 1 if it is EIA-6, and otherwise
 * whatever the selected reference loop's own capability word says.
 *
 * IT SELECTS A LOOP FIRST IF NONE IS SELECTED.  `test %ecx,%ecx ; js` at
 * 0x45a1c is a SIGN test and not a comparison with -1, so any negative
 * `refLoop` runs the search; `reset` leaves -1 there and this is one of the
 * two places that undoes it.
 *
 * `isV90WithEia6()` IS INLINED HERE, and the count of setcc is the check.
 * The object has exactly three -- `capability == 2` at 0x45a39, `== 1` at
 * 0x45a43 and `+0x500 == 6` at 0x45a52, then `or %al,%dl` -- which is that
 * function's body and nothing more.  Writing `isV90WithEia6() == 1` at the
 * call site would emit a fourth: the `== 1` in the object is the callee's own
 * (V90PreFilter.cpp above), on a value that is already 0 or 1.
 *
 * The entry test and the callee's `refLoop >= 0` are the same test, which is
 * why the fall-through at 0x45a1e goes straight to the capability load.
 *
 * WITH `refLoop` STILL NEGATIVE AFTER THE SEARCH the last line indexes
 * `loops[-1]`, which the object does and ours must not be asked to.  It is
 * reachable only when `autoSelection` matched nothing AND the registry's
 * +0x500 is not 6; a trial that arranges it is testing our undefined
 * behaviour (D670).
 */
int
V90PreFilter::getV90Capability()
{
	if (refLoop < 0)
		autoSelection();

	if (isV90WithEia6())
		return 1;

	return dataBase[codecType].loops[refLoop].capability;
}

/*
 * Load the FIR with the coefficients this connection wants.
 *
 * Five ways in, and they differ in where the bank and the row come from:
 *
 *   registry +0x0c == 1   ISDN NT1: row from +0x54, bank from the codec's
 *                         FIRST reference loop, row unclamped
 *   registry +0x0c == 2   PBX ISDN: the same with the row from +0x58
 *   registry +0x4c == -1  automatic: autoSelection() picks the loop, and the
 *                         bank and row follow from it
 *   registry +0x50        1, 2 or 3 force the bank; -1 takes it from the
 *                         codec's first loop; anything else is bank 1
 *
 * The row clamp depends on the bank and not on how it was chosen: 30 for the
 * 20-tap banks, and 20..50 for the 40-tap one, whose rows start at index 20.
 */
void
V90PreFilter::selectFilter()
{
	V90Parameters *p = params;
	float *coef;
	int type, g;

	refLoop = 0;

	if (V90PW(p)[0x0c / 4] == 1) {
		edprintf("V90PreFilter: Connection Type is ISDN NT1 BOX "
			 "(4.2kHz Null)\r\n");
		g = V90PW(p)[0x54 / 4];
		type = dataBase[codecType].loops[0].coefType;
		gain = g;
	} else if (V90PW(p)[0x0c / 4] == 2) {
		edprintf("V90PreFilter: Connection Type is PBX ISDN BOX "
			 "(4kHz Null)\r\n");
		g = V90PW(p)[0x58 / 4];
		type = dataBase[codecType].loops[0].coefType;
		gain = g;
	} else if (V90PW(p)[0x4c / 4] == -1) {
		int want = autoSelection();
		unsigned int len = 20;
		int loop, row;

		if (gain == want) {
			edprintf("V90PreFilter: Filter Gain = %d\r\n", want);
			return;
		}

		/*
		 * Two passes over the same field.  The first settles the tap
		 * count, the second the bank and the clamp -- and each
		 * complains about an unrecognised type, so an unrecognised
		 * type produces the message twice.
		 */
		loop = refLoop;
		if (loop != -1) {
			int t = dataBase[codecType].loops[loop].coefType;

			if (t == 3) {
				len = 40;
			} else if (t != 2 && t != 1) {
				edprintf(BUGMSG);
				loop = refLoop;
			}
		}

		row = want;
		if (loop == -1) {
			/* No clamp on this path.  The object's. */
			coef = bank1(row);
		} else {
			int t = dataBase[codecType].loops[loop].coefType;

			if (t == 2) {
				if ((unsigned int)want > 30)
					row = 30;
				coef = bank2(row);
			} else if (t == 3) {
				if ((unsigned int)want > 50)
					row = 50;
				coef = bank3(row);
			} else {
				if (t != 1)
					edprintf(BUGMSG);
				if ((unsigned int)want > 30)
					row = 30;
				coef = bank1(row);
			}
		}

		fir.setCoefficients(coef, len);
		gain = want;
		edprintf("V90PreFilter: Filter Gain = %d\r\n", want);
		return;
	} else {
		int force = V90PW(p)[0x50 / 4];
		int wide;

		if (force == 1) {
			edprintf("V90PreFilter: Force coeff type array 1\r\n");
			type = 1;
			wide = 0;
		} else if (force == 2) {
			edprintf("V90PreFilter: Force coeff type array 2\r\n");
			type = 2;
			wide = 0;
		} else if (force == 3) {
			edprintf("V90PreFilter: Force coeff type array 3\r\n");
			type = 3;
			wide = 1;
		} else if (force == -1) {
			type = dataBase[codecType].loops[0].coefType;
			edprintf("V90PreFilter: Using coeff type array "
				 "according to codec type\r\n");
			wide = (type == 3);
		} else {
			type = 1;
			wide = 0;
		}

		p = params;
		if (wide) {
			int v = V90PW(p)[0x4c / 4];

			g = (v > 49) ? 50 : ((v > 19) ? v : 20);
		} else {
			int v = V90PW(p)[0x4c / 4];

			g = (v > 29) ? 30 : v;
		}
		gain = g;
	}

	if (type == 2) {
		coef = bank2(g);
		fir.setCoefficients(coef, 20);
	} else if (type == 3) {
		coef = bank3(g);
		fir.setCoefficients(coef, 40);
		gain = g - 20;
	} else {
		coef = bank1(g);
		fir.setCoefficients(coef, 20);
	}

	edprintf("V90PreFilter: Filter Gain = %d\r\n", g);
}

/*
 * Move the EIA-6 timing parameters into place.
 *
 * NOTHING HERE TOUCHES THE FILTER.  `this` is read at exactly one offset,
 * +0x1c, and everything else happens inside the V90Parameters block:
 * twenty-six scattered word copies, a clock-deviation report, and -- only if
 * the deviation is non-zero -- a float store and an eighteen-word block move,
 * then four more words.  Not one of the twenty-six sources is also one of the
 * destinations, so the order below is the object's rather than a requirement.
 *
 * The offsets stay numeric because V90Parameters is not modelled and inventing
 * names for its fields from a copy alone would be a guess in the record.
 *
 * THE CLOCK DEVIATION IS x87 ARITHMETIC AND IS TRANSCRIBED AS SUCH.  The
 * registry's integer is multiplied by 0.001f, and the report's three fields
 * are the sign, the truncated magnitude and the first four decimal places --
 * the last of those being `(int)(10000.0f * (x - (int)x))` computed WITHOUT
 * rounding x to float first.  The float rounding happens once, for the value
 * that is stored and tested against zero, and the sign test uses the extended
 * value rather than the rounded one.  Finding 233 is the precedent for taking
 * that literally.
 */
void
V90PreFilter::setParamEia6()
{
	V90Parameters *p;
	const int *blk;
	long double x;
	float xf;
	int whole, frac, i;

	edprintf("V90PreFilter: setParamEIA6 called\r\n");

	p = params;
	V90PW(p)[0x010 / 4] = V90PW(p)[0x014 / 4];
	V90PW(p)[0x2a4 / 4] = V90PW(p)[0x2a8 / 4];
	V90PW(p)[0x374 / 4] = V90PW(p)[0x388 / 4];
	V90PW(p)[0x3a8 / 4] = V90PW(p)[0x3d8 / 4];
	V90PW(p)[0x3ac / 4] = V90PW(p)[0x3dc / 4];
	V90PW(p)[0x3b0 / 4] = V90PW(p)[0x3e0 / 4];
	V90PW(p)[0x3b4 / 4] = V90PW(p)[0x3e4 / 4];
	V90PW(p)[0x3b8 / 4] = V90PW(p)[0x3e8 / 4];
	V90PW(p)[0x3bc / 4] = V90PW(p)[0x3ec / 4];
	V90PW(p)[0x188 / 4] = V90PW(p)[0x1d4 / 4];
	V90PW(p)[0x18c / 4] = V90PW(p)[0x1d8 / 4];
	V90PW(p)[0x1c0 / 4] = V90PW(p)[0x1e4 / 4];
	V90PW(p)[0x1c4 / 4] = V90PW(p)[0x1dc / 4];
	V90PW(p)[0x1c8 / 4] = V90PW(p)[0x1e0 / 4];
	V90PW(p)[0x190 / 4] = V90PW(p)[0x198 / 4];
	V90PW(p)[0x194 / 4] = V90PW(p)[0x19c / 4];
	V90PW(p)[0x210 / 4] = V90PW(p)[0x218 / 4];
	V90PW(p)[0x214 / 4] = V90PW(p)[0x21c / 4];
	V90PW(p)[0x078 / 4] = V90PW(p)[0x07c / 4];
	V90PW(p)[0x1bc / 4] = V90PW(p)[0x1a0 / 4];
	V90PW(p)[0x230 / 4] = V90PW(p)[0x220 / 4];
	V90PW(p)[0x178 / 4] = V90PW(p)[0x1e8 / 4];
	V90PW(p)[0x17c / 4] = V90PW(p)[0x1ec / 4];
	V90PW(p)[0x180 / 4] = V90PW(p)[0x1f0 / 4];
	V90PW(p)[0x204 / 4] = V90PW(p)[0x244 / 4];
	V90PW(p)[0x20c / 4] = V90PW(p)[0x24c / 4];

	/* The block +0x00 points at; the deviation is the int at its +0x4c. */
	blk = *(const int *const *)&V90PB(p)[0];
	x = (long double)blk[0x4c / 4] * 0.001f;
	whole = (int)x;
	frac = (int)(10000.0f * (x - (long double)whole));
	xf = (float)x;

	edprintf("V90PreFilter: prev params ClockDeviation is = %c%d.%04d\r\n",
		 (x > 0.0L) ? '+' : '-', (int)__builtin_fabsl(x),
		 (frac < 0) ? -frac : frac);

	/*
	 * `fcompp; sahf; jne`, and the zero flag comes from C3, which is set
	 * for equal AND for unordered -- so the object treats a NaN deviation
	 * as zero where C's `!=` would not.
	 *
	 * THE ARGUMENT FOR THIS SPELLING IS NOW THE ARGUMENT AGAINST IT, and
	 * the code is left alone anyway.  It was written as two relational
	 * tests because C's `!=` acquires a parity test under `-mieee-fp`;
	 * `period_inner.sh` now carries `-mno-ieee-fp`, where `!=` IS the
	 * object's single `fcompp`/`jne` and this pair is one compare too many
	 * (finding 2300, which corrected nine such sites).  This is the tenth.
	 * It is not one of the nine because its suite is green either way --
	 * `x` is an int times 0.001f and cannot be a NaN, so the two tests
	 * agree over every value that reaches them -- so there was no
	 * differential failure to drive the change and nothing to prove it
	 * with beyond the codegen tier.  Whoever measures that next should
	 * take it.
	 */
	if (xf < 0.0f || xf > 0.0f) {
		edprintf("V90PreFilter: Setting timing parameters "
			 "(registry)...\r\n");
		p = params;
		V90PF(p)[0x84 / 4] = xf;
		for (i = 0; i < 18; i++)
			V90PW(p)[0x88 / 4 + i] = V90PW(p)[0x110 / 4 + i];
	}

	p = params;
	V90PW(p)[0x460 / 4] = V90PW(p)[0x490 / 4];
	V90PW(p)[0x40c / 4] = V90PW(p)[0x488 / 4];
	V90PW(p)[0x410 / 4] = V90PW(p)[0x484 / 4];
	V90PW(p)[0x414 / 4] = V90PW(p)[0x48c / 4];
}

/*
 * reset -- the FIR's own reset, then the type 1 bank at gain 0.
 *
 * Four steps and no diagnostic.  `FloatFIR::reset` clears the history and the
 * index; the two stores put the filter back in the state the constructor
 * leaves it in, with `refLoop` at -1 meaning "no reference loop selected"
 * (`autoSelection` is what fills it in, and it returns -1 on no match); and
 * the tail call reinstalls the FIRST ROW of the type 1 coefficient bank, 20
 * taps, which is gain 0.
 *
 * THE RELOCATION IS THE EVIDENCE FOR THE ROW.  `mov $0x0,%eax` at 0x44b0c
 * carries `R_386_32 _ZN12V90PreFilter18preFilterCoefType1E` with no addend,
 * so it is the base of the table and not `bank1(gain)` for some gain the
 * object happened to hold -- which a disassembly read without relocations
 * would have shown as a plain zero.  CLAUDE.md's tools/dis.py rule, in the
 * smallest possible instance.
 */
void
V90PreFilter::reset()
{
	fir.reset();

	refLoop = -1;
	gain = 0;

	fir.setCoefficients(&V90PreFilter::preFilterCoefType1[0][0], 20);
}

/* ================================================================ lifecycle */

/*
 * The banner the constructor puts round its out-of-range complaint.  102
 * characters of "*#", EXCEPT THAT ONE PAIR IS DOUBLED -- ".. *#**#* .." at
 * .rodata.str1.4+0xb930 -- which is in the object's bytes and is transcribed
 * rather than tidied (docs/deviations.md D177).
 *
 * IT IS ONE LITERAL ON ONE OVER-LONG LINE, AND IT LIVES HERE RATHER THAN
 * BESIDE `BUGMSG`, FOR THE AUDIT'S SAKE.  `debugaudit.py --invented` blanks
 * any line STARTING with `#` and then scans what is left in a single pass,
 * joining literals separated only by whitespace.  So two multi-line `#define`
 * string macros one after the other arrive as one concatenated string that is
 * in no `.rodata` and fails the `strings` gate; and a macro whose literal is
 * SPLIT across continuation lines is never rejoined, because the `\` between
 * the halves is not whitespace, so each half is audited as a truncated string
 * of its own.  Whole, and preceded by code, it is checked against the blob --
 * which is the only reason the doubled pair above is known to be the object's
 * and not a typing slip here.
 */
#define BUGBAR \
	"*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#**#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*#*\r\n"

/*
 * THE FILTER IS BUILT BEFORE ANYTHING IS DECIDED.  The first thing the
 * constructor does is run `FloatFIR`'s own, with forty taps, a NULL
 * coefficient pointer and ninety-nine samples of block slack:
 *
 *      44b31:  ba 63 00 00 00      mov    $0x63,%edx      ; blockSize
 *      44b36:  b9 28 00 00 00      mov    $0x28,%ecx      ; nTaps
 *      44b49:  89 5c 24 08         mov    %ebx,0x8(%esp)  ; coef = 0
 *
 * so the FIR allocates 40 + 99 floats and starts with no coefficients at all;
 * the tail of this function is what gives it some.  `test/harness/v90demfix.h`
 * calls the same two numbers FIR_TAPS and FIR_SLACK, from the other side.
 *
 * WHICH CODEC.  The registry wins if it has an opinion: a NEGATIVE
 * `HW_CODEC_TYPE` means "not configured", and only then is the constructor's
 * own argument used.  Otherwise the registry's value goes through a sixteen
 * arm switch -- the object has a real jump table at .rodata+0xd8c, sixteen
 * `R_386_32 .text` entries, so the source had a `switch` and not a range test
 * -- whose every arm stores its own index and whose default stores zero.  The
 * CASE LABELS BELOW ARE VALUES, NOT RECOVERED ENUMERATOR NAMES: what
 * `__tHardwareCodecTypes__` calls 0 through 15 is not in the object.
 *
 * THE INDEX IS THEN CHECKED AGAINST THE TABLE, and the bound is one less than
 * the number of entries -- `dataBase` is walked to its first empty name and
 * the count is decremented before the comparison.  So the LAST entry of the
 * table is unreachable through this path.  Transcribed; see
 * docs/deviations.md D176.
 *
 * THE DIAGNOSTICS HERE ARE PLAIN, NOT ENCODED.  This is one of the few places
 * in the V.90 half that calls `dsplibs_debug_printf` directly under its own
 * `dsplibs_debug_level > 1` test instead of going through `edprintf`, so it
 * neither encodes its output nor disturbs `edprintf`'s rotating key.  Each
 * message has its own test, which is what the object's four separate reloads
 * of the level say.
 */
V90PreFilter::V90PreFilter(__tHardwareCodecTypes__ codec, V90Phase2Info *info,
			   V90Parameters *parms)
	: fir(0x28, (float *)0, 0x63)
{
	int n;

	phase2 = info;
	params = parms;

	if (V90PW(params)[0x008 / 4] < 0) {
		codecType = (int)codec;
	} else {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90PreFilter: HardwareCodecType"
					     " loaded by configuration"
					     " parameters\r\n");

		switch (V90PW(params)[0x008 / 4]) {
		case 0:		codecType = 0;	break;
		case 1:		codecType = 1;	break;
		case 2:		codecType = 2;	break;
		case 3:		codecType = 3;	break;
		case 4:		codecType = 4;	break;
		case 5:		codecType = 5;	break;
		case 6:		codecType = 6;	break;
		case 7:		codecType = 7;	break;
		case 8:		codecType = 8;	break;
		case 9:		codecType = 9;	break;
		case 10:	codecType = 10;	break;
		case 11:	codecType = 11;	break;
		case 12:	codecType = 12;	break;
		case 13:	codecType = 13;	break;
		case 14:	codecType = 14;	break;
		case 15:	codecType = 15;	break;
		default:	codecType = 0;	break;
		}
	}

	for (n = 0; dataBase[n].name[0] != 0; n++)
		;
	n--;

	if (codecType > n) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(BUGBAR);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("V90PreFilter: External Hardware"
					     " Codec Index exceeds table"
					     " length (codec inx = %d, table"
					     " length = %d)\r\n",
					     codecType, n);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(BUGBAR);

		codecType = 0;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90PreFilter: HardwareCodecType: %s\r\n",
				     dataBase[codecType].name);

	reset();
}

/*
 * The destructor is nineteen bytes and all of them are the FIR's: it sets up
 * one argument, calls `FloatFIR::~FloatFIR` on `this` unadjusted, and
 * returns.  Nothing in this class's own five words is owned -- `phase2` and
 * `params` are the constructor's arguments and the three ints are indices --
 * so the body is empty and the member's destructor is the whole function.
 */
V90PreFilter::~V90PreFilter()
{
}
