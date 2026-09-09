/*
 * t_vce.c -- differential test of the four `vce_*` / `STRM_VCE_*` functions.
 *
 * The first three functions are `t` in the object.  This test reaches both
 * sides through the callback tables made by `VOICE_create`, rather than
 * publishing an external declaration that would change the reconstructed
 * object's binding.  Their stack-based prologues establish ordinary cdecl;
 * no `regparm(2)` declaration is involved.  Finding F8770.
 *
 * WHAT COUNTS AS AN OBSERVABLE RESULT:
 *
 *   - `vce_get_sreg`'s return value, over EVERY register number 0..300 and
 *     not just the seven it knows, because "returns 0" is as much a claim as
 *     any other answer and a switch that grew an arm would still pass a test
 *     that only asked about seven;
 *   - the two `short`s `STRM_VCE_GetFDSPEnvironmentalParams` writes, AND the
 *     values it was handed, so that "writes 51 and 369" is separated from
 *     "leaves whatever was there";
 *   - the debug transcript at levels 0, 1 and 2 for all four, since every one
 *     of the six gated sites in this group is `> 1` and a run at one level
 *     cannot tell `> 1` from `> 0` (debug.h, finding F150);
 *   - the parameter log, because `vce_get_sreg` fetches MDMPRM_VOICEINFO
 *     UNCONDITIONALLY -- including on the three arms that answer a constant
 *     and on the default arm that answers nothing -- and a reconstruction
 *     that hoisted the fetch inside the switch would be invisible otherwise.
 *
 * THE FIXTURE PLANTS EVERY FIELD THE CALLEE SUBSCRIPTS, NOT ONLY THE THREE IT
 * DEREFERENCES (D955 / F8587).  `struct voice_info` is filled with a distinct
 * value per member before every call, so a copy that read the wrong offset
 * gets a wrong ANSWER rather than the same wrong bytes on both sides.
 *
 * COVERAGE IS ASSERTED FROM THE RUN (F134).  The `seen_*` counters below
 * count arms taken as judged by the REFERENCE side's answer, and `main` fails
 * if any is zero -- including the four sensitivity bands, which is the part
 * of this group a "sweep the register numbers" test would never reach.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/cadence.h"
#include "dsplib/detector.h"
#include "dsplib/vce.h"
#include "dsplib/modem_params.h"
#include "dsplib/voice.h"

extern unsigned int ref_dsplibs_debug_level;
extern void ref_STRM_VCE_GetFDSPEnvironmentalParams(short *psFarEchoDelay,
						    short *psNearEchoDelay);
extern void *ref_VOICE_create(void *modem, unsigned int rate);

extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

static long seen_arm[8];	/* one per known register, [7] is the default */
static long seen_band[4];	/* the four sensitivity levels 0..3         */
static long sregs_compared;
static struct vce *ours_vce;
static struct vce *ref_vce;
static int callback_modem;
static struct voice_info callback_info;
static void fill_info(struct voice_info *vi, unsigned int sens,
		      unsigned int period);

/* VOICE_create needs this same country fixture as t_voiceapi. */
static void
set_voice_params(void)
{
	unsigned k;
	static const int on_off[4] = {
		GetMaxBusyCadenceOnTime, GetMinBusyCadenceOnTime,
		GetMinBusyCadenceOffTime, GetMaxBusyCadenceOffTime
	};

	harness_param_reset();
	harness_param_set(MDMPRM_VOICEINFO, (long)(size_t)&callback_info);
	harness_param_set(GetDialToneCallProgressFilterIndex, 1);
	harness_param_set(GetBusyToneCallProgressFilterIndex, 1);
	harness_param_set(GetCongestionToneCallProgressFilterIndex, 1);
	harness_param_set(GetRingbackToneCallProgressFilterIndex, 1);
	harness_param_set(GetDialToneFilterSubindex, 0);
	harness_param_set(GetCallProgressSamplesBufferLength, 666);
	harness_param_set(GetDialToneValidationTime, 50);
	harness_param_set(GetDialToneDetectionThreshold, 40);
	harness_param_set(GetBusyToneLooseDetectionEnabled, 0);
	harness_param_set(GetBusyDetectionCyclesNumber, 3);
	harness_param_set(GetCongestionDetectionCyclesNumber, 3);
	harness_param_set(GetRingbackDetectionCyclesNumber, 3);
	harness_param_set(GetBusyToneDiffTime, 3);
	for (k = 0; k < 4; k++)
		harness_param_set(on_off[k], (int)(60 + k * 13));
}

static void
callbacks_init(void)
{
	fill_info(&callback_info, 110, 111);
	set_voice_params();
	ours_vce = (struct vce *)VOICE_create(&callback_modem, VCE_RATE_8000);
	ref_vce = (struct vce *)ref_VOICE_create(&callback_modem, VCE_RATE_8000);
}

static int
ours_sreg(void *modem, unsigned int num)
{
	return (int)ours_vce->voice->cfg.fn_04(modem, (int)num);
}

static void
ours_hook_on(void *modem)
{
	ours_vce->voice->cfg.fn_08(modem);
}

static void
ours_hook_off(void *modem)
{
	ours_vce->voice->cfg.fn_0c(modem);
}

static int
ref_sreg(void *modem, unsigned int num)
{
	return (int)ref_vce->voice->cfg.fn_04(modem, (int)num);
}

static void
ref_hook_on(void *modem)
{
	ref_vce->voice->cfg.fn_08(modem);
}

static void
ref_hook_off(void *modem)
{
	ref_vce->voice->cfg.fn_0c(modem);
}

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

/*
 * A voice_info whose eleven members are all distinct and none of them zero,
 * so that reading the wrong one is a wrong answer and not a coincidence.
 * `sens` and `period` are the two the caller varies.
 */
static void
fill_info(struct voice_info *vi, unsigned int sens, unsigned int period)
{
	vi->comp_method = 0x1101;
	vi->sample_rate = 0x1102;
	vi->rx_gain = 0x1103;
	vi->tx_gain = 0x1104;
	vi->dtmf_symbol = 0x1105;
	vi->tone1_freq = 0x1106;
	vi->tone2_freq = 0x1107;
	vi->tone_duration = 0x1108;
	vi->inactivity_timer = 0x1109;
	vi->silence_detect_sensitivity = sens;
	vi->silence_detect_period = period;
}

static int
arm_of(unsigned int num)
{
	switch (num) {
	case SREG_FLASH_TIMER:			return 0;
	case SREG_HANDSET_GANE:			return 1;
	case SREG_VOICE_DIALTONE_DETECT_DELAY:	return 2;
	case SREG_SILENCE_DETECT_SENSITIVITY:	return 3;
	case SREG_SILENCE_DETECT_DURATION:	return 4;
	case SREG_MIC_GAIN:			return 5;
	case SREG_LINE_RECORD_GAIN:		return 6;
	}
	return 7;
}

/*
 * The register sweep.  Every number 0..300 against several voice_info
 * contents, so that the six answers that come out of the block are exercised
 * with several different blocks and the 294 that do not are exercised at all.
 */
static int
t_sreg(void)
{
	static const unsigned int sens[] = {
		0, 1, 63, 64, 65, 127, 128, 191, 192, 193, 255,
		256, 1023, 0xffffffffu
	};
	static const unsigned int period[] = { 0, 1, 7, 4000, 0x80000000u };
	unsigned int num;
	unsigned int s, p;
	struct voice_info vi;
	void *modem = (void *)0x4321;

	diff_begin("vce_get_sreg over every register number and many blocks");

	for (s = 0; s < sizeof sens / sizeof sens[0]; s++) {
		for (p = 0; p < sizeof period / sizeof period[0]; p++) {
			harness_param_reset();
			fill_info(&vi, sens[s], period[p]);
			harness_param_set(MDMPRM_VOICEINFO, (long)(size_t)&vi);

			for (num = 0; num <= 300; num++) {
				long tag = (long)(num * 1000 + s * 10 + p);
				int a, b;

				a = ref_sreg(modem, num);
				b = ours_sreg(modem, num);
				diff_eq_int("vce_get_sreg(%ld)", b, a, tag);
				sregs_compared++;
				seen_arm[arm_of(num)]++;
				if (num == SREG_SILENCE_DETECT_SENSITIVITY
				    && a >= 0 && a <= 3)
					seen_band[a]++;
			}
		}
	}

	/*
	 * The fetch is unconditional.  One call on the default arm, with the
	 * log cleared first, and both sides must have asked for exactly
	 * MDMPRM_VOICEINFO exactly once.
	 */
	harness_param_reset();
	fill_info(&vi, 100, 200);
	harness_param_set(MDMPRM_VOICEINFO, (long)(size_t)&vi);
	(void)ref_sreg(modem, 7);
	(void)ours_sreg(modem, 7);
	diff_eq_int("an unknown register still fetches VOICEINFO",
		    harness_param_ours.calls, harness_param_ref.calls, 7);
	diff_eq_int("...exactly once", harness_param_ours.calls, 1, 7);
	diff_eq_int("...and it is MDMPRM_VOICEINFO",
		    (long)harness_param_ours.last_param, MDMPRM_VOICEINFO, 7);
	diff_eq_int("...for the modem it was handed",
		    harness_param_ours.last_modem == modem, 1, 7);

	return diff_end();
}

/*
 * The echo delays.  Both out-parameters are seeded with a distinct value
 * before every call, and both are compared afterwards -- so "wrote 51" is
 * separated from "left what was there", and a swap of the two pointers is a
 * failure rather than a coincidence.
 */
static int
t_env_params(void)
{
	static const short seeds[] = { 0, 1, -1, 51, 369, 32767, -32768, 300 };
	unsigned int i, j;

	diff_begin("STRM_VCE_GetFDSPEnvironmentalParams writes both delays");

	for (i = 0; i < sizeof seeds / sizeof seeds[0]; i++) {
		for (j = 0; j < sizeof seeds / sizeof seeds[0]; j++) {
			short fa = seeds[i], na = seeds[j];
			short fb = seeds[i], nb = seeds[j];
			long tag = (long)(i * 10 + j);

			ref_STRM_VCE_GetFDSPEnvironmentalParams(&fa, &na);
			STRM_VCE_GetFDSPEnvironmentalParams(&fb, &nb);
			diff_eq_int("far echo delay [%ld]", fb, fa, tag);
			diff_eq_int("near echo delay [%ld]", nb, na, tag);
			diff_eq_int("far is 51 [%ld]", fb,
				    STRM_VCE_FAR_ECHO_DELAY, tag);
			diff_eq_int("near is 369 [%ld]", nb,
				    STRM_VCE_NEAR_ECHO_DELAY, tag);
		}
	}
	return diff_end();
}

/*
 * The transcripts.  Six gated sites across the four functions, all `> 1`, so
 * levels 0 and 1 must print nothing and level 2 must print the same text on
 * both sides.  The hooks print a POINTER, which differs run to run but not
 * side to side, so the texts are compared and not matched against a literal.
 */
static int
t_debug(void)
{
	static const unsigned int levels[] = { 0, 1, 2 };
	unsigned int l;
	struct voice_info vi;
	void *modem = (void *)0x4321;

	diff_begin("the diagnostic transcript at levels 0, 1 and 2");

	harness_param_reset();
	fill_info(&vi, 100, 200);
	harness_param_set(MDMPRM_VOICEINFO, (long)(size_t)&vi);

	for (l = 0; l < 3; l++) {
		short fa = 7, na = 9, fb = 7, nb = 9;
		unsigned lines_ours, lines_ref;
		unsigned int lvl = levels[l];

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;

		ref_hook_on(modem);
		ours_hook_on(modem);
		ref_hook_off(modem);
		ours_hook_off(modem);
		(void)ref_sreg(modem, SREG_MIC_GAIN);
		(void)ours_sreg(modem, SREG_MIC_GAIN);
		ref_STRM_VCE_GetFDSPEnvironmentalParams(&fa, &na);
		STRM_VCE_GetFDSPEnvironmentalParams(&fb, &nb);

		dsplib_debug_capture_on = 0;
		lines_ours = dsplib_debug_capture_lines(0);
		lines_ref = dsplib_debug_capture_lines(1);
		diff_eq_int("line count at level %ld", lines_ours, lines_ref,
			    (long)lvl);
		/*
		 * Four printfs at level 2 -- hook_on, hook_off and the two
		 * "StrmVCE" lines; vce_get_sreg has no gated site at all.
		 */
		diff_eq_int("line count is 0 or 4 at level %ld", lines_ours,
			    lvl > 1 ? 4 : 0, (long)lvl);
		diff_eq_int("transcript text at level %ld",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
	}
	set_level(0);
	return diff_end();
}

static int
t_coverage(void)
{
	int i;

	diff_begin("the sweep above reached every arm it claims to");
	for (i = 0; i < 8; i++)
		diff_eq_int("register arm %ld was taken", seen_arm[i] > 0, 1,
			    (long)i);
	for (i = 0; i < 4; i++)
		diff_eq_int("sensitivity band %ld was reported",
			    seen_band[i] > 0, 1, (long)i);
	diff_eq_int("the sweep was not empty", sregs_compared, 301 * 14 * 5, 0);
	fprintf(stderr, "t_vce: %ld sreg comparisons, arms "
			"%ld/%ld/%ld/%ld/%ld/%ld/%ld/%ld, bands "
			"%ld/%ld/%ld/%ld\n",
		sregs_compared, seen_arm[0], seen_arm[1], seen_arm[2],
		seen_arm[3], seen_arm[4], seen_arm[5], seen_arm[6],
		seen_arm[7], seen_band[0], seen_band[1], seen_band[2],
		seen_band[3]);
	return diff_end();
}

int
main(void)
{
	int failed = 0;

	callbacks_init();
	if (ours_vce == 0 || ref_vce == 0) {
		fprintf(stderr, "t_vce: VOICE_create fixture failed\n");
		return 1;
	}
	failed |= t_sreg();
	failed |= t_env_params();
	failed |= t_debug();
	failed |= t_coverage();
	return failed;
}
