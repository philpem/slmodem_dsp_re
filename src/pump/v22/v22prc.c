/*
 * v22prc.c -- V.22 / V.22bis: the small state and status helpers.
 *
 * Reconstructed from dsplibs.o.  The nine live in two of the author's
 * translation units, split by address:
 *
 *   TxNOP           .text 0x08c340   44   \
 *   RxClampV22      .text 0x08c370   44    |  the 0x8bd50..0x8c5a0 block,
 *   ReadGTimer      .text 0x08c3a0   15    |  with MakeTxData and the
 *   RxTrained1200   .text 0x08be20   63    |  Detect_* family
 *   RxTrained2400   .text 0x08be60  136   /
 *
 *   TxClockSync     .text 0x08e610   22   \
 *   CarrierDetect   .text 0x08e630   14    |  the 0x8e120..0x8e669 block,
 *   SignalDetect    .text 0x08e640   14    |  after DemodDataV22
 *   GetSignalQuality .text 0x08e650  25   /
 *
 * plus, from the 2026-08-30 no-entry-point leaf batch (finding F8320's
 * bucket -- exported API nothing in the object calls), three more of the
 * same shape from the same two neighbourhoods:
 *
 *   V22FP_control   .text 0x08c3b0  145      after ReadGTimer
 *   ScramblerOn     .text 0x08e670   11   \  after GetSignalQuality
 *   DescramblerOn   .text 0x08e680   11   /
 *
 * `tools/tumap.py` puts thirteen V.22 translation units in one shared
 * bracket, so it cannot say which of `V22.c`, `v22prc.c` and `v22stc.c` each
 * block belongs to; they are together here because none of them calls
 * anything, which is what made all nine writable before `V22FP_create`.
 *
 * ---------------------------------------------------------------------------
 * THE TWO SIXTEEN-BIT LOOP IDIOMS, because both look like bugs and neither is
 *
 * TxNOP and RxClampV22 count with a 16-bit register and test the value BEFORE
 * the decrement:
 *
 *     lea -0x1(%ecx),%eax      eax = n - 1
 *     movswl %ax,%ecx          n   = (short)(n - 1)
 *     inc %ax                  ax  = (short)n_old
 *     jne                      loop while n_old != 0
 *
 * so a count of 0x9f produces 0xa0 = 160 writes, not 159.  Written below as
 * `for (i = 0; i <= COUNT - 1; i++)`, which is the same trip count and the
 * shape the rest of this tree uses.
 *
 * RxTrained1200 and RxTrained2400 both hold their index as a `short` and
 * compare it against an `unsigned short` count widened to 32 bits.  With a
 * count above 0x7fff that comparison behaves differently from an `int` loop,
 * so the widths are kept: the counts here are small, but the object's shape
 * is the object's shape and the differential test covers the edge.
 */

#include "dsplib/v22prc.h"

/* The object is not modelled; see v22prc.h.  These are the only accessors. */
#define FIELD(obj, off)		((unsigned char *)(obj) + (off))
#define FIELD_INT(obj, off)	(*(int *)(void *)FIELD((obj), (off)))
#define FIELD_PTR(obj, off)	(*(void **)(void *)FIELD((obj), (off)))
#define FIELD_SHORT(obj, off)	(*(short *)(void *)FIELD((obj), (off)))
#define FIELD_USHORT(obj, off)	(*(unsigned short *)(void *)FIELD((obj), (off)))
#define FIELD_BYTE(obj, off)	(*(unsigned char *)FIELD((obj), (off)))

/*
 * One block of the datapump is 20 ms, so the shared clock is in
 * milliseconds.  The new value is returned, not the old one.
 */
int
ReadGTimer(void *modem)
{
	int *timer = (int *)FIELD_PTR(modem, V22_OBJ_GTIMER);

	*timer += 20;
	return *timer;
}

void
TxNOP(void *modem, void *arg1, short *out, short *count)
{
	short i;

	(void)modem;
	(void)arg1;

	for (i = 0; i <= V22_TX_BLOCK - 1; i++)
		out[i] = 0;

	*count = V22_TX_BLOCK;
}

void
RxClampV22(void *modem, void *arg1, short *out, short *count)
{
	short i;

	(void)modem;
	(void)arg1;

	for (i = 0; i <= V22_CLAMP_BLOCK - 1; i++)
		out[i] = V22_CLAMP_VALUE;

	*count = V22_CLAMP_BLOCK;
}

/*
 * True when every one of `*count` symbols is 3.
 *
 * An empty array is trained: the object's first comparison is `0 >= n`, which
 * jumps straight to the `i == n` test with i still zero.  Preserved, and
 * covered by the test, because "no symbols yet" reading as "training
 * complete" is exactly the kind of thing a rewrite quietly changes.
 */
int
RxTrained1200(const short *symbols, const unsigned short *count)
{
	unsigned short n = *count;
	short i = 0;

	if (i < (int)n && symbols[0] == V22_TRAINED_1200_SYMBOL) {
		do {
			i = (short)(i + 1);
			if (i >= (int)n)
				break;
		} while (symbols[i] == V22_TRAINED_1200_SYMBOL);
	}

	return i == (int)n;
}

/*
 * True when MORE THAN seven symbols at the END of the array are 15.
 *
 * Backwards from `*count - 1`, and the run has to reach the end of the array
 * or eight entries, whichever comes first -- the loop stops on a mismatch OR
 * on running out, and both exits land on the same `> 7` test.  So a
 * nine-entry array of 15s passes, and so does an eight-entry one.
 */
int
RxTrained2400(const short *symbols, const unsigned short *count)
{
	unsigned short n = *count;
	short run = 0;
	short i;

	if ((int)run >= (int)n)
		return 0;

	i = (short)(n - 1);
	if (symbols[i] != V22_TRAINED_2400_SYMBOL)
		return 0;

	i = (short)(i - 1);
	for (;;) {
		run = (short)(run + 1);
		if ((int)run >= (int)n)
			break;
		if (symbols[i] != V22_TRAINED_2400_SYMBOL)
			break;
		i = (short)(i - 1);
	}

	return run > V22_TRAINED_2400_RUN;
}

int
CarrierDetect(void *modem)
{
	void *fp = FIELD_PTR(modem, V22_OBJ_FP);

	return FIELD_INT(fp, V22FP_CARRIER);
}

int
SignalDetect(void *modem)
{
	void *fp = FIELD_PTR(modem, V22_OBJ_FP);

	return FIELD_INT(fp, V22FP_SIGNAL);
}

/*
 * Quality as a distance from the rail: 0x8000 minus the stored figure,
 * truncated to sixteen bits and returned unsigned.
 *
 * The object loads the constant as 0xffff8000 -- that is, -32768 in a 32-bit
 * register -- subtracts, and then zero-extends the low half.  So a stored
 * figure of 0 gives 32768 and one of 0x8000 gives 0.  The wraparound is real
 * and reachable: any stored figure above 0x8000 gives a LARGE answer, not a
 * negative one.  Preserved, and the differential test sweeps the whole
 * sixteen-bit domain rather than sampling it.
 */
unsigned short
GetSignalQuality(void *modem)
{
	void *fp = FIELD_PTR(modem, V22_OBJ_FP);

	return (unsigned short)(-32768 - (int)FIELD_USHORT(fp, V22FP_QUALITY));
}

/*
 * Three modes, a `switch` in the object (compare, jg, dec, je -- GCC's shape
 * for a dense switch of three), and no default action.  Written as a switch
 * for the same reason.
 */
void
SetAdaptEqV22(void *modem, unsigned short mode)
{
	void *fp;

	switch (mode) {
	case 1:
		fp = FIELD_PTR(modem, V22_OBJ_FP);
		FIELD_INT(fp, V22FP_EQ_ADAPT) = 0;
		break;
	case 2:
		fp = FIELD_PTR(modem, V22_OBJ_FP);
		FIELD_INT(fp, V22FP_EQ_ADAPT) = 1;
		FIELD_SHORT(fp, V22FP_EQ_MODE) = 0;
		break;
	case 3:
		fp = FIELD_PTR(modem, V22_OBJ_FP);
		FIELD_INT(fp, V22FP_EQ_ADAPT) = 1;
		FIELD_SHORT(fp, V22FP_EQ_MODE) = 1;
		/* Set here and cleared by nothing -- mode 2 leaves it alone. */
		FIELD_INT(fp, V22FP_EQ_EXTRA) = 1;
		break;
	default:
		break;
	}
}

void
TxClockSync(void *modem)
{
	void *fp = FIELD_PTR(modem, V22_OBJ_FP);
	short baud = FIELD_SHORT(fp, V22FP_BAUD);

	FIELD_SHORT(fp, V22FP_TX_CLOCK) = (short)(baud * 3);
}

/*
 * V22FP_control, ScramblerOn and DescramblerOn were reconstructed here first,
 * against `void *modem`, and again in `v22ctl.c` against the modelled
 * `struct v22fp`.  Both passed their differential tests -- they are the same
 * three functions -- so the typed pair is what the tree keeps, and this file
 * declares nothing about them.  `v22ctl.c` is their one home.
 */
