/*
 * detector.c -- the voice service's tone detector.
 *
 * Reconstructed from dsplibs.o:
 *
 *   detector_create    .text 0x0ad480    411 bytes
 *   detector_progress  .text 0x0ad6e0    814 bytes
 *
 * plus the seven file-local data symbols the two of them are the only
 * readers of: `status`, `tone_char`, `tone_integration_threshold`, `tone`
 * and `TONEamode_CFG` in `.data`, `enable` and `lookup_table` in `.rodata`.
 * All seven names below are the blob's own (`nm -S`), not inventions.
 *
 * THE SPAN IS `Beepgen.c` AND THAT IS A LAYOUT LABEL, NOT A MODULE NAME.
 * These two functions sit inside the blob span this tree calls `Beepgen.c`,
 * between `GetGain` (0xac960) and `zfFLTUTL_GetMaxAbsValue` (0xae850), and
 * the tree gives that span's leftovers to src/service/beepgen.c -- which is
 * where `detector_delete`, the three setters and `create_dtmf` already live.
 * This is VOICE-SERVICE code: `voice_create` is the only caller of
 * `detector_create` in the object, and `struct detector` is `struct voice`'s
 * +0x01c.  It gets its own file because its seven statics are read from
 * nowhere else, and a `static` belongs in the translation unit of its
 * reader (the argument findings F8772 and F8781 declined them on).
 *
 * WHY THESE TWO COULD NOT BE WRITTEN BEFORE.  Not the call graph -- every
 * callee has been written for some time -- but the DATA.  Seven LOCAL
 * symbols with one reader each, and that reader was these functions.  See
 * detector.h for what the object listens for and what it reports.
 */

#include "dsplib/cadence.h"
#include "dsplib/debug.h"
#include "dsplib/detector.h"
#include "dsplib/dtmf.h"
#include "dsplib/fdspkrnl.h"
#include "dsplib/silence.h"
#include "dsplib/sysdep.h"
#include "dsplib/vce.h"

/*
 * `.data` 0x8200.  The DETECTOR_OUTPUT_STATUS return code for each tone,
 * subscripted by the same index as `tone[]`.  1 and 2 are the cadence arm's
 * (busy and dial), so the six codes run 1..6 without a gap.
 */
static int status[DETECTOR_TONES] = { 3, 4, 5, 6 };

/*
 * `.data` 0x8210.  The DETECTOR_OUTPUT_IN_STREAM letter for each tone, read
 * `movsbl` at 0xad98e and 0xad9aa.  These are the IS-101 voice-mode shielded
 * event codes and the cadence arm's own 'b' and 'd' belong to the same set,
 * but nothing inside this object says so -- what the object does say is that
 * the letter and `status[]`'s code are alternatives for the same event.
 */
static char tone_char[DETECTOR_TONES] = { 'e', 'c', 'a', 'f' };

/*
 * `.data` 0x8214.  How many consecutive zero verdicts from `TONE_detect` a
 * tone must produce before it is reported.  `unsigned short`, and the
 * comparison against the counter is UNSIGNED -- `cmp %ax,0x8214(%ebx,%ebx,1)`
 * then `jae` at 0xad868.
 */
static unsigned short tone_integration_threshold[DETECTOR_TONES] = {
	25, 21, 12, 25
};

/*
 * `.data` 0x821c.  The four frequencies, in Hz.  1300 is the V.25 calling
 * tone, 1100 the fax calling tone, 2100 the answer tone and 2225 Bell 103's
 * answer -- but the object only says these are four floats that become
 * `fdsp_tone_cfg.freq`, so read the identifications as what the numbers are
 * and not as something derived here.
 */
static float tone[DETECTOR_TONES] = { 1300.0f, 1100.0f, 2100.0f, 2225.0f };

/*
 * `.data` 0x8240.  The template every one of the four resonators is built
 * from, with only `freq` changed per tone.  Twelve words, copied whole
 * (`rep movsl` with $0xc at 0xad4bb) -- which is what makes
 * `struct fdsp_tone_cfg` a real 48-byte type rather than the head of a tone
 * object.  Finding F8781 read these bytes out when it declined the symbol
 * for want of a reader; they are re-checked against the object here.
 *
 * `fir_proto` is NULL in `.data` and PATCHED at run time from `TONE_CFG`'s
 * (0xad4c0, a relocation against the global).  That is the answer to the
 * D995 worry F8781 raised: a 53-tap filter with a NULL prototype never
 * reaches `TONE_create`, because this assignment happens first.
 */
static struct fdsp_tone_cfg TONEamode_CFG = {
	980.0f,			/* freq, overwritten per tone below       */
	0.353599995f,		/* amp                                    */
	0.0f,			/* duration -- 0 is endless               */
	0.75f,			/* float_000c                             */
	0.01f,			/* float_0010                             */
	0.000199999995f,	/* float_0014                             */
	0.9375f,		/* pole_radius                            */
	0,			/* fir_proto -- see above                 */
	53,			/* fir_len                                */
	/* +0x22 was the `pad_22[2]` initializer slot; the two bytes are
	 * compiler-inserted alignment now (finding F10151). */
	0, 0, 0			/* int_0024, int_0028, int_002c           */
};

/*
 * `.rodata` 0xeecc.  Which `enable` bit gates which tone.  The object tests
 * it as a full 32-bit word (`test %ecx,0xeecc(,%ebx,4)` at 0xad723), so it
 * is `int` and not `short`.
 */
static const int enable[DETECTOR_TONES] = {
	DETECTOR_ENABLE_1300, DETECTOR_ENABLE_1100,
	DETECTOR_ENABLE_2100, DETECTOR_ENABLE_2225
};

/*
 * `.rodata` 0xeedc.  `dtmf_test` reports low + 4 * high, with the low group
 * 697/770/852/941 and the high 1209/1336/1477/1633; this turns that index
 * into the digit.  Read `movsbl` at 0xad8d0, and the sixteen characters are
 * the object's bytes.
 */
static const char lookup_table[16] = {
	'1', '4', '7', '*', '2', '5', '8', '0',
	'3', '6', '9', '#', 'A', 'B', 'C', 'D'
};

/*
 * Build the detector.
 *
 * The allocating path zeroes the seven pointers and falls into the common
 * one, so a caller supplying its own object keeps whatever dtmf, cadence and
 * tone objects it had -- every create below is handed the existing pointer
 * and re-initialises it in place.
 *
 * `s` is deliberately NOT zeroed: the object writes exactly five of
 * `cadence_setup`'s seven words (0xad55a, 0xad577, 0xad58e, 0xad582,
 * 0xad57e) and `cadence_create` reads `w3` -- so `cadence->int_27c` takes
 * whatever was on the stack.  Reproduced as written; see deviation D1000.
 */
struct detector *
detector_create(struct detector *d, void *modem, detector_sreg_fn get_sreg)
{
	struct fdsp_tone_cfg cfg;
	struct cadence_setup s;
	int i;

	if (d == 0) {
		d = (struct detector *)sysdep_malloc(sizeof(*d));
		if (d == 0)
			return 0;
		d->dtmf = 0;
		d->cadence_0008 = 0;
		d->cadence_busy = 0;
		d->cadence_dial = 0;
		/*
		 * A LOOP, not four stores: 0xad60a is three instructions
		 * indexed by %eax, which four separate assignments to four
		 * named fields could not produce.
		 */
		for (i = 0; i <= 3; i++)
			d->tone[i] = 0;
	}

	d->output_mode = DETECTOR_OUTPUT_IN_STREAM;
	d->dtmf = create_dtmf(d->dtmf);

	cfg = TONEamode_CFG;
	cfg.fir_proto = TONE_CFG.fir_proto;

	for (i = 0; i <= 3; i++) {
		cfg.freq = tone[i];
		d->tone[i] = TONE_create(d->tone[i], &cfg);
		d->tone_integration[i] = 0;
	}

	/*
	 * `r * 50 / 4`, and both halves are the object's: *50 is the two
	 * `lea`s and the `add` at 0xad529, /4 is `shr $0x2` -- unsigned, so
	 * the multiply is unsigned too.
	 */
	if (get_sreg != 0)
		d->dialtone_detect_delay =
		    get_sreg(modem, SREG_VOICE_DIALTONE_DETECT_DELAY) * 50 / 4;
	else
		d->dialtone_detect_delay = 0;

	d->int_002c = 0;
	d->enable = DETECTOR_ENABLE_ALL;

	s.w0 = 50;
	s.w1 = 50;
	s.w2 = 3;
	s.tone = CADENCE_TONE_BUSY;
	s.w6 = 0;
	d->cadence_busy = cadence_create(d->cadence_busy, &s, 2, modem);

	s.tone = CADENCE_TONE_DIAL;
	d->cadence_dial = cadence_create(d->cadence_dial, &s, 2, modem);

	return d;
}

/*
 * One block.  Three independent arms, in this order and each gated on a
 * freshly-read `enable`.
 */
int
detector_progress(struct detector *d, float *samples, short count,
		  unsigned char *out, unsigned short *outlen)
{
	int ret = 0;
	int i;

	/*
	 * DTMF.  Runs ONCE per block -- `dtmf_progress` already walks every
	 * sample and returns the last digit it settled, so there is nothing
	 * to drain.
	 */
	if (d->enable & DETECTOR_ENABLE_DTMF) {
		short r = dtmf_progress(d->dtmf, samples, count, 2);

		/*
		 * -1 and -2 are "no digit" and "block not finished"; the
		 * object merges them into one unsigned 16-bit range test
		 * (`lea 0x2(%edx),%eax; cmp $0x1,%ax; jbe` at 0xad8bc).
		 */
		if (r != DTMF_NOT_YET && r != DTMF_NO_DIGIT) {
			short c = (r < 0 || r > 15) ? -1 : lookup_table[r];

			if (c < 0) {
				/* Prints the INDEX, not the character. */
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "false dtmf detect %d\n", r);
			} else {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "detected dtmf %c\n",
					    (unsigned char)c);
				/*
				 * `_status`'s three stores, written out
				 * rather than called -- there is no "DLE %d"
				 * line here, so this is not that function
				 * inlined -- plus the cursor advance no
				 * other arm performs.  Finding F8805.
				 */
				*outlen += 2;
				out[0] = 0x10;
				out[1] = (unsigned char)c;
				out += 2;
			}
		}
	}

	/*
	 * The four resonators, held off entirely while the DTMF receiver is
	 * reporting a digit (`cmpw $0x0,0x90(%ebx)` at 0xad709).
	 */
	if (d->dtmf->held == 0) {
		for (i = 0; i <= 3; i++) {
			if (d->enable & enable[i]) {
				if (TONE_detect(d->tone[i], samples, count)
				    != 0) {
					d->tone_integration[i] = 0;
				} else if (++d->tone_integration[i]
				    > tone_integration_threshold[i]) {
					/*
					 * PASSES the threshold, not reaches
					 * it: the object's `jae` at 0xad870
					 * skips while `threshold >= counter`.
					 *
					 * The debug line comes BEFORE the
					 * output-mode test and is common to
					 * both of its arms -- the object's
					 * shape at 0xad872, not a tidied one.
					 */
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
						    "detected %c\n",
						    tone_char[i]);
					if (d->output_mode)
						_status(out, outlen,
							tone_char[i]);
					else
						ret = status[i];
				}
			}
		}
	}

	/*
	 * Busy and dial tone.  Both cadence detectors are fed EVERY sample,
	 * and each sample is converted separately for each of them -- the
	 * object recomputes the multiply and the cast at 0xad793 rather than
	 * reusing 0xad750's, because `cadence_progress` may have written
	 * through `samples`.
	 *
	 * 16000.0f is `.rodata.cst4` 0x504 read out of the object; it is not
	 * 32767, so a full-scale float lands at half of `short`'s range.
	 */
	if (d->enable & DETECTOR_ENABLE_CADENCE) {
		for (i = 0; i < count; i++) {
			if (cadence_progress(d->cadence_busy,
					     (short)(samples[i] * 16000.0f))
			    == CADENCE_DETECTED) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "busy detected by cadence\n");
				if (d->output_mode)
					_status(out, outlen, 'b');
				else
					ret = 1;
			}
			if (cadence_progress(d->cadence_dial,
					     (short)(samples[i] * 16000.0f))
			    == CADENCE_DETECTED) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
					    "dial detected by cadence\n");
				if (d->output_mode)
					_status(out, outlen, 'd');
				else
					ret = 2;
			}
		}
	}

	return ret;
}
