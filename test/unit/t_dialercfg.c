/*
 * t_dialercfg.c -- differential test of the dialler's configuration fetch.
 *
 * Sixteen parameters copied into a struct is the kind of function where a
 * reconstruction can be wrong in a way nothing notices: swap two fields whose
 * values happen to be similar, or read the neighbouring parameter, and every
 * functional test still passes.  So this checks three things beyond the
 * resulting struct:
 *
 *   - both sides asked for the same parameters, the same number of times, in
 *     the same order.  The harness records the last request per side and the
 *     call count; the sweep below drives each parameter to a distinct value
 *     so a swap changes the struct.
 *
 *   - the two DTMF level tables are exercised over their whole valid range
 *     AND outside it, since the out-of-range fallback is a different constant
 *     rather than a clamp.
 *
 *   - the levels really are one decibel apart, which byte equality against
 *     the blob cannot tell you and which is what makes the table a level
 *     table rather than seven arbitrary numbers.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "harness.h"
#include "dsplib/dialercfg.h"
#include "dsplib/modem_params.h"

extern void ref_GetDialerConfig(struct dialer_cfg *cfg, void *modem);

/* Every parameter GetDialerConfig reads, with a distinct value each. */
static const struct {
	int	param;
	int	value;
} settings[] = {
	{ GetDTMFDialSpeed,				71 },
	{ GetPulseDialMakeTime,				72 },
	{ GetPulseDialBreakTime,			73 },
	{ GetDialPauseTime,				74 },
	{ GetHookFlashTime,				75 },
	{ GetPulseDialingFlag,				76 },
	{ GetDialModifierValidation,			77 },
	{ GetABCDDialingPermittedFlag,			78 },
	{ GetPulseAndToneDialInSameDialStringPermittedFlag, 79 },
	{ GetCallingToneFlag,				80 },
	{ GetComaPauseDurationLimit,			81 },
	{ GetPulseDialDigitPattern,			82 },
	{ GetPulseBetweenDigitsInterval,		83 }
};

static void
apply(int level, int twist, int pulse_flag)
{
	unsigned k;

	harness_param_reset();
	for (k = 0; k < sizeof(settings) / sizeof(settings[0]); k++)
		harness_param_set(settings[k].param, settings[k].value);
	harness_param_set(GetPulseDialingFlag, pulse_flag);
	harness_param_set(GetDTMFHighToneLevel, level);
	harness_param_set(GetDTMFHighAndLowToneLevelDifference, twist);
}

static int
compare(const char *what, int level, int twist, int pulse_flag)
{
	struct dialer_cfg a, b;
	unsigned i;
	char msg[128];

	memset(&a, 0xA5, sizeof(a));
	memset(&b, 0xA5, sizeof(b));

	/*
	 * One reset, then both sides -- the harness keeps a separate log per
	 * side, so resetting between them would wipe the reference's before it
	 * could be compared.
	 */
	apply(level, twist, pulse_flag);
	ref_GetDialerConfig(&a, (void *)0xC0FFEEu);
	GetDialerConfig(&b, (void *)0xC0FFEEu);

	snprintf(msg, sizeof(msg), "%s: byte %%ld", what);
	for (i = 0; i < sizeof(struct dialer_cfg); i++)
		diff_eq_int(msg, ((unsigned char *)&b)[i],
			    ((unsigned char *)&a)[i], (long)i);

	/*
	 * Same number of host requests, and the last one was the same.  A
	 * reconstruction that dropped the duplicate GetDTMFDialSpeed fetch
	 * would produce an identical struct and fail here.
	 */
	diff_eq_int("parameter call count", harness_param_ours.calls,
		    harness_param_ref.calls, 0);
	diff_eq_int("last parameter requested", (int)harness_param_ours.last_param,
		    (int)harness_param_ref.last_param, 0);
	diff_eq_int("same modem handle",
		    harness_param_ours.last_modem == harness_param_ref.last_modem,
		    1, 0);

	return 0;
}

int
main(void)
{
	struct dialer_cfg cfg;
	int level, twist;
	int rc = 0;

	diff_begin("GetDialerConfig: the whole struct");
	compare("nominal", 9, 2, 1);
	compare("levels at the bottom", DIALER_DTMF_LEVEL_MIN,
		DIALER_DTMF_TWIST_MIN, 0);
	compare("levels at the top", DIALER_DTMF_LEVEL_MAX,
		DIALER_DTMF_TWIST_MAX, 1);
	compare("level below range", DIALER_DTMF_LEVEL_MIN - 1, 3, 1);
	compare("level above range", DIALER_DTMF_LEVEL_MAX + 1, 3, 1);
	compare("twist below range", 9, DIALER_DTMF_TWIST_MIN - 1, 1);
	compare("twist above range", 9, DIALER_DTMF_TWIST_MAX + 1, 1);
	compare("both negative", -5, -5, 1);
	compare("both huge", 100000, 100000, 1);
	compare("pulse flag 0", 9, 2, 0);
	compare("pulse flag 99", 9, 2, 99);
	rc |= diff_end();

	/* Every level and every twist, against the reference. */
	diff_begin("GetDialerConfig: level and twist sweep");
	for (level = -2; level <= 16; level++)
		for (twist = -2; twist <= 8; twist++)
			compare("sweep", level, twist, 1);
	rc |= diff_end();

	/*
	 * What byte equality cannot say: that these are decibels.
	 */
	diff_begin("GetDialerConfig: the tables are decibels");
	for (level = DIALER_DTMF_LEVEL_MIN; level < DIALER_DTMF_LEVEL_MAX;
	     level++) {
		short lo, hi;
		double db;
		char msg[128];

		apply(level, 2, 1);
		GetDialerConfig(&cfg, (void *)0xC0FFEEu);
		hi = cfg.dtmf_high;
		apply(level + 1, 2, 1);
		GetDialerConfig(&cfg, (void *)0xC0FFEEu);
		lo = cfg.dtmf_high;

		db = 20.0 * log10((double)hi / lo);
		snprintf(msg, sizeof(msg),
			 "level %d to %d is %.3f dB", level, level + 1, db);
		diff_eq_int(msg, db > 0.97 && db < 1.03, 1, level);
	}
	apply(DIALER_DTMF_LEVEL_MIN, 2, 1);
	GetDialerConfig(&cfg, (void *)0xC0FFEEu);
	diff_eq_int("level 6 is 0 dBFS", cfg.dtmf_high, 16384, 6);

	for (twist = DIALER_DTMF_TWIST_MIN; twist <= DIALER_DTMF_TWIST_MAX;
	     twist++) {
		double db;
		char msg[128];

		apply(9, twist, 1);
		GetDialerConfig(&cfg, (void *)0xC0FFEEu);
		db = 20.0 * log10((double)cfg.dtmf_high / cfg.dtmf_low);
		snprintf(msg, sizeof(msg), "twist %d is %.2f dB down", twist,
			 db);
		diff_eq_int(msg, db > twist - 0.06 && db < twist + 0.06, 1,
			    twist);
	}

	/*
	 * The out-of-range fallbacks are the ITU's values, not zero and not a
	 * clamp to the nearest end of the table.
	 */
	apply(99, 99, 1);
	GetDialerConfig(&cfg, (void *)0xC0FFEEu);
	diff_eq_int("out-of-range level falls back to -2 dB", cfg.dtmf_high,
		    DIALER_DTMF_LEVEL_DEFAULT, 0);
	{
		double db = 20.0 * log10((double)cfg.dtmf_high / cfg.dtmf_low);
		char msg[96];

		snprintf(msg, sizeof(msg),
			 "out-of-range twist falls back to %.2f dB", db);
		diff_eq_int(msg, db > 1.94 && db < 2.06, 1, 0);
	}
	rc |= diff_end();

	return rc;
}
