/*
 * V90PreFilter.cpp -- choosing the V.90 receive pre-filter.
 *
 * Reconstructed from dsplibs.o V90PreFilter.cpp.  Five of the class's
 * twenty-four members -- `selectFilter`, `setParamEia6`, `autoSelection`,
 * `isV90WithEia6` and `displayParamEia6`.  `include/dsplib/V90PreFilter.h`
 * carries the object map and the two table shapes.
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

	return (cap == 1) || (params->w[0x500 / 4] == 6);
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
	const float *measured = *(const float *const *)&phase2->b[0x18];
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

	if (p->w[0x0c / 4] == 1) {
		edprintf("V90PreFilter: Connection Type is ISDN NT1 BOX "
			 "(4.2kHz Null)\r\n");
		g = p->w[0x54 / 4];
		type = dataBase[codecType].loops[0].coefType;
		gain = g;
	} else if (p->w[0x0c / 4] == 2) {
		edprintf("V90PreFilter: Connection Type is PBX ISDN BOX "
			 "(4kHz Null)\r\n");
		g = p->w[0x58 / 4];
		type = dataBase[codecType].loops[0].coefType;
		gain = g;
	} else if (p->w[0x4c / 4] == -1) {
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
		int force = p->w[0x50 / 4];
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
			int v = p->w[0x4c / 4];

			g = (v > 49) ? 50 : ((v > 19) ? v : 20);
		} else {
			int v = p->w[0x4c / 4];

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
	p->w[0x010 / 4] = p->w[0x014 / 4];
	p->w[0x2a4 / 4] = p->w[0x2a8 / 4];
	p->w[0x374 / 4] = p->w[0x388 / 4];
	p->w[0x3a8 / 4] = p->w[0x3d8 / 4];
	p->w[0x3ac / 4] = p->w[0x3dc / 4];
	p->w[0x3b0 / 4] = p->w[0x3e0 / 4];
	p->w[0x3b4 / 4] = p->w[0x3e4 / 4];
	p->w[0x3b8 / 4] = p->w[0x3e8 / 4];
	p->w[0x3bc / 4] = p->w[0x3ec / 4];
	p->w[0x188 / 4] = p->w[0x1d4 / 4];
	p->w[0x18c / 4] = p->w[0x1d8 / 4];
	p->w[0x1c0 / 4] = p->w[0x1e4 / 4];
	p->w[0x1c4 / 4] = p->w[0x1dc / 4];
	p->w[0x1c8 / 4] = p->w[0x1e0 / 4];
	p->w[0x190 / 4] = p->w[0x198 / 4];
	p->w[0x194 / 4] = p->w[0x19c / 4];
	p->w[0x210 / 4] = p->w[0x218 / 4];
	p->w[0x214 / 4] = p->w[0x21c / 4];
	p->w[0x078 / 4] = p->w[0x07c / 4];
	p->w[0x1bc / 4] = p->w[0x1a0 / 4];
	p->w[0x230 / 4] = p->w[0x220 / 4];
	p->w[0x178 / 4] = p->w[0x1e8 / 4];
	p->w[0x17c / 4] = p->w[0x1ec / 4];
	p->w[0x180 / 4] = p->w[0x1f0 / 4];
	p->w[0x204 / 4] = p->w[0x244 / 4];
	p->w[0x20c / 4] = p->w[0x24c / 4];

	/* The block +0x00 points at; the deviation is the int at its +0x4c. */
	blk = *(const int *const *)&p->b[0];
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
	 * as zero where C's `!=` would not.  Spelled as the object's predicate
	 * rather than as `!=`; the difference is unreachable, because `x` is
	 * an int times 0.001f and cannot be a NaN, but the two are not the
	 * same test and only one of them is the object's.
	 */
	if (xf < 0.0f || xf > 0.0f) {
		edprintf("V90PreFilter: Setting timing parameters "
			 "(registry)...\r\n");
		p = params;
		p->f[0x84 / 4] = xf;
		for (i = 0; i < 18; i++)
			p->w[0x88 / 4 + i] = p->w[0x110 / 4 + i];
	}

	p = params;
	p->w[0x460 / 4] = p->w[0x490 / 4];
	p->w[0x40c / 4] = p->w[0x488 / 4];
	p->w[0x410 / 4] = p->w[0x484 / 4];
	p->w[0x414 / 4] = p->w[0x48c / 4];
}
