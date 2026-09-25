/*
 * v22prc.c -- V.22 / V.22bis: the small state and status helpers.
 *
 * Reconstructed from dsplibs.o.  What is left here is the 0x8bd50..0x8c5a0
 * block, one of the author's translation units:
 *
 *   TxNOP           .text 0x08c340   44   \
 *   RxClampV22      .text 0x08c370   44    |  the 0x8bd50..0x8c5a0 block,
 *   ReadGTimer      .text 0x08c3a0   15    |  with MakeTxData and the
 *   RxTrained1200   .text 0x08be20   63    |  Detect_* family
 *   RxTrained2400   .text 0x08be60  136   /
 *
 * SetAdaptEqV22, TxClockSync, CarrierDetect, SignalDetect, GetSignalQuality,
 * ScramblerOn and DescramblerOn are NOT here: the object puts them in
 * V22int.c, and they moved there in the TU reconciliation.  V22FP_control
 * has its one home in v22ctl.c.
 *
 * `tools/tumap.py` puts thirteen V.22 translation units in one shared
 * bracket, so it cannot say which of `V22.c`, `v22prc.c` and `v22stc.c` this
 * block belongs to; it is here because none of the five calls anything, which
 * is what made them writable before `V22FP_create`.
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
#include "dsplib/v22fp.h"

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
 * One block of the datapump is 20 ms, so the shared clock is in
 * milliseconds.  The new value is returned, not the old one.
 */
int
ReadGTimer(void *modem)
{
	struct v22fp *v22 = (struct v22fp *)modem;
	int *timer = &v22->hdx->gtimer;

	*timer += 20;
	return *timer;
}
/*
 * SetAdaptEqV22, TxClockSync, CarrierDetect, SignalDetect and
 * GetSignalQuality are V22int.c's and now live in src/pump/v22/V22int.c.
 * What remains here is the 0x8bd50..0x8c5a0 block, which the object puts in
 * another V.22 unit.
 */
