/*
 * t_callprog_create.c -- differential test of the call-progress supervisor's
 * construction.
 *
 * Most of what `CALLPROG_Create` does lands in eleven file-static tables that
 * have no global symbol, so there is nothing to link against.  They are
 * reachable all the same: `SMCv32_CFG` is a *global* in the same `.bss` at a
 * known offset, so its address pins the section base and every table can be
 * read at its fixed offset from there.
 *
 * That is a slightly unusual thing to do in a test, and worth the sentence:
 * without it the state machine -- which is the substance of the function --
 * could only be checked through `CALLPROG_Progress`, and the two would have to
 * be reconstructed together with no way to tell which was wrong.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "dsplib/callprog_state.h"
#include "dsplib/callprog.h"
#include "dsplib/modem_params.h"
#include "dsplib/debug.h"

extern void ref_CALLPROG_Create(struct callprog *cp, struct callprog_cfg *cfg);
extern void ref_CALLPROG_Delete(struct callprog *cp);
extern void ref_CALLPROG_Dial(struct callprog *cp, const char *s);

/* The reference side has its own level; the two must move together. */
extern unsigned int ref_dsplibs_debug_level;

/* The anchor.  .bss+0x188 in the object, whatever it lands at when linked. */
extern unsigned char ref_SMCv32_CFG[];

#define BSS_SMCV32_CFG_OFFSET	0x188

static unsigned char *
ref_bss(void)
{
	/*
	 * Laundered through a volatile so the compiler stops trying to
	 * bounds-check indices against the anchor's declared size.  It is a
	 * different object we are addressing, and only the linker knows that.
	 */
	unsigned char *volatile p = ref_SMCv32_CFG;

	return p - BSS_SMCV32_CFG_OFFSET;
}

/* Where each table sits, from the object's own symbol table. */
#define OFF_NEXT_CPTD		0x020
#define OFF_NEXT_LINE_CLEAR	0x070
#define OFF_ENABLE_LINE_CLEAR	0x080
#define OFF_MSG_CPTD		0x0c0
#define OFF_MSG_LINE_CLEAR	0x110
#define OFF_NEXT_TIMEOUT	0x11a
#define OFF_MSG_TIMEOUT		0x124
#define OFF_TIMEOUT_TABLE	0x140
#define OFF_AUTOMODE		0x168
#define OFF_DIALTONE		0x172
#define OFF_BUSY		0x17c

static long
sreg_stub(void *modem, unsigned short n)
{
	(void)modem;
	return (long)n;
}

/*
 * Compare every table, byte for byte, against the blob's.
 */
static void
compare_tables(long n)
{
	unsigned char *b = ref_bss();
	int s, e;

	for (s = 0; s < CALLPROG_STATES; s++) {
		for (e = 0; e < CALLPROG_CPTD_EVENTS; e++) {
			diff_eq_int("next_state_due_cptd",
				    next_state_due_cptd[s][e],
				    b[OFF_NEXT_CPTD + s * 8 + e], s * 8 + e);
			diff_eq_int("message_due_cptd",
				    message_due_cptd[s][e],
				    b[OFF_MSG_CPTD + s * 8 + e], s * 8 + e);
		}
		diff_eq_int("next_state_due_timeout",
			    next_state_due_timeout[s],
			    b[OFF_NEXT_TIMEOUT + s], s);
		diff_eq_int("message_due_timeout", message_due_timeout[s],
			    b[OFF_MSG_TIMEOUT + s], s);
		diff_eq_int("next_state_due_line_clear_timeout",
			    next_state_due_line_clear_timeout[s],
			    b[OFF_NEXT_LINE_CLEAR + s], s);
		diff_eq_int("message_due_line_clear_timeout",
			    message_due_line_clear_timeout[s],
			    b[OFF_MSG_LINE_CLEAR + s], s);
		diff_eq_int("automode_table", automode_table[s],
			    b[OFF_AUTOMODE + s], s);
		diff_eq_int("toneiir_dialtone_table", toneiir_dialtone_table[s],
			    b[OFF_DIALTONE + s], s);
		diff_eq_int("toneiir_busy_table", toneiir_busy_table[s],
			    b[OFF_BUSY + s], s);

		{
			int rt, re;

			memcpy(&rt, b + OFF_TIMEOUT_TABLE + s * 4, sizeof(rt));
			memcpy(&re, b + OFF_ENABLE_LINE_CLEAR + s * 4,
			       sizeof(re));
			diff_eq_int("timeout_table", timeout_table[s], rt, s);
			diff_eq_int("enable_line_clear_timeout",
				    enable_line_clear_timeout[s], re, s);
		}
	}
	(void)n;
}

static void
compare_object(const struct callprog *b, const struct callprog *a, long n)
{
	int i;

	for (i = 0; i < 7; i++)
		diff_eq_int("timeout[%ld]", b->timeout[i], a->timeout[i], i);
	diff_eq_int("f1c", b->f1c, a->f1c, n);
	diff_eq_int("same get_sreg", b->get_sreg == a->get_sreg, 1, n);
	diff_eq_int("same modem", b->modem == a->modem, 1, n);
	diff_eq_int("f28", b->f28, a->f28, n);
	diff_eq_int("state", b->state, a->state, n);
	diff_eq_int("countdown", b->countdown, a->countdown, n);
	diff_eq_int("line_clear_limit", b->line_clear_limit, a->line_clear_limit, n);
	diff_eq_int("line_clear_active", b->line_clear_active, a->line_clear_active, n);
	diff_eq_int("band_wanted", b->band_wanted, a->band_wanted, n);
	diff_eq_int("band allocated", b->band != 0, a->band != 0, n);
	diff_eq_int("dial allocated", b->dial != 0, a->dial != 0, n);
	diff_eq_int("busy allocated", b->busy != 0, a->busy != 0, n);
	diff_eq_int("dtmf allocated", b->dtmf != 0, a->dtmf != 0, n);
	diff_eq_int("dialtone_seen", b->dialtone_seen, a->dialtone_seen, n);
}

static int
run_create(const char *label, int band_wanted)
{
	struct callprog a, b;
	struct callprog_cfg ca, cb;

	diff_begin(label);

	harness_param_reset();
	harness_alloc_reset();
	harness_param_set(MustNoiseFilterBeApplied, band_wanted);
	harness_param_set(GetDialToneCallProgressFilterIndex, 1);
	harness_param_set(GetBusyToneCallProgressFilterIndex, 0);
	harness_param_set(GetDialToneFilterSubindex, 0);
	harness_param_set(GetCallProgressSamplesBufferLength, 666);
	harness_param_set(GetDialToneValidationTime, 50);
	harness_param_set(GetDialToneDetectionThreshold, 40);
	harness_param_set(GetMaxBusyCadenceOnTime, 55);
	harness_param_set(GetMinBusyCadenceOnTime, 20);
	harness_param_set(GetMinBusyCadenceOffTime, 20);
	harness_param_set(GetMaxBusyCadenceOffTime, 55);
	harness_param_set(GetBusyDetectionCyclesNumber, 3);
	harness_param_set(GetBusyToneLooseDetectionEnabled, 0);

	memset(&a, 0xA5, sizeof(a));
	memset(&b, 0xA5, sizeof(b));
	ca.w0 = 1;
	ca.get_sreg = sreg_stub;
	ca.modem = (void *)0xC0DEu;
	ca.w3 = 0;
	cb = ca;

	ref_CALLPROG_Create(&a, &ca);
	CALLPROG_Create(&b, &cb);

	compare_object(&b, &a, 0);
	compare_tables(0);

	/*
	 * The configuration is read, not written -- a create that modified its
	 * caller's block would be a surprise worth catching.
	 */
	diff_eq_int("config untouched", memcmp(&ca, &cb, sizeof(ca)) == 0, 1,
		    0);

	/* Both sides allocated the same things. */
	diff_eq_int("allocations balanced", harness_alloc.live > 0, 1, 0);

	ref_CALLPROG_Delete(&a);
	CALLPROG_Delete(&b);

	diff_eq_int("after delete: dial cleared", b.dial == 0, a.dial == 0, 0);
	diff_eq_int("after delete: f70", b.f70, a.f70, 0);
	/*
	 * And the three that are NOT cleared, which is the point of finding
	 * 55 -- asserted so a tidier reconstruction would fail here.
	 */
	diff_eq_int("busy left dangling", b.busy != 0, a.busy != 0, 0);
	diff_eq_int("dtmf left dangling", b.dtmf != 0, a.dtmf != 0, 0);

	diff_eq_int("everything freed", harness_alloc.live, 0, 0);
	diff_eq_int("no bad frees", harness_alloc.bad_free, 0, 0);

	return diff_end();
}

/*
 * CALLPROG_Dial, driven on top of a freshly created supervisor.  It rebuilds
 * the timeout half of the state machine from parameters the host may have
 * changed, so the tables are compared again afterwards -- and the blind-dial
 * branch is the one real decision in the function, so both arms are driven.
 */
static int
run_dial(const char *label, int blind, int calling_tone, int validate,
	 int state_in, const char *dialstr)
{
	struct callprog a, b;
	struct callprog_cfg ca, cb;

	diff_begin(label);

	harness_param_reset();
	harness_alloc_reset();
	harness_param_set(MustNoiseFilterBeApplied, 1);
	harness_param_set(GetDialToneCallProgressFilterIndex, 1);
	harness_param_set(GetBusyToneCallProgressFilterIndex, 0);
	harness_param_set(GetDialToneFilterSubindex, 0);
	harness_param_set(GetCallProgressSamplesBufferLength, 666);
	harness_param_set(GetDialToneDetectionThreshold, 40);
	harness_param_set(GetMaxBusyCadenceOnTime, 55);
	harness_param_set(GetMinBusyCadenceOnTime, 20);
	harness_param_set(GetMinBusyCadenceOffTime, 20);
	harness_param_set(GetMaxBusyCadenceOffTime, 55);
	harness_param_set(GetBusyDetectionCyclesNumber, 3);
	harness_param_set(GetBusyToneLooseDetectionEnabled, 0);
	harness_param_set(GetCallingToneFlag, calling_tone);
	harness_param_set(GetNoAnswerTimeOut, 45);
	harness_param_set(GetBlindDialPause, 6);
	harness_param_set(GetDialToneWaitTime, 9);
	harness_param_set(GetDialToneValidationTime, validate);
	harness_param_set(GetDialModifierValidation, 0);
	harness_param_set(GetPulseDialMakeTime, 33);
	harness_param_set(GetPulseDialBreakTime, 67);
	harness_param_set(GetDTMFHighToneLevel, 9);
	harness_param_set(GetDTMFHighAndLowToneLevelDifference, 2);
	/*
	 * No datapump.  CALLPROG_Dial reaches SetPulseMakeTime through
	 * DialerCreate, and that dereferences whatever this parameter returns
	 * -- so leaving it at the store's derived value is a wild pointer, not
	 * a harmless one.  Zero is the path both sides handle by doing
	 * nothing, which is what this test wants.
	 */
	harness_param_set(MDMPRM_DP_ADDR, 0);

	memset(&a, 0xA5, sizeof(a));
	memset(&b, 0xA5, sizeof(b));
	ca.w0 = blind;
	ca.get_sreg = sreg_stub;
	ca.modem = (void *)0xC0DEu;
	ca.w3 = 0;
	cb = ca;

	ref_CALLPROG_Create(&a, &ca);
	CALLPROG_Create(&b, &cb);
	a.state = b.state = state_in;

	ref_CALLPROG_Dial(&a, dialstr);
	CALLPROG_Dial(&b, dialstr);

	compare_object(&b, &a, 0);
	compare_tables(0);
	diff_eq_int("calling_tone_mode", b.calling_tone_mode, a.calling_tone_mode, 0);
	diff_eq_int("calling_tone_armed", b.calling_tone_armed, a.calling_tone_armed, 0);
	diff_eq_int("fatal", b.fatal, a.fatal, 0);
	diff_eq_int("state after dial", b.state, a.state, 0);
	diff_eq_int("calling tone armed", b.calling_tone.remaining,
		    a.calling_tone.remaining, 0);
	diff_eq_int("calling tone amplitude", b.calling_tone.amplitude,
		    a.calling_tone.amplitude, 0);
	diff_eq_int("dial string copied", strcmp(a.dialer.string,
						 b.dialer.string), 0, 0);
	diff_eq_int("dialler graded it", b.dialer.grade, a.dialer.grade, 0);

	ref_CALLPROG_Delete(&a);
	CALLPROG_Delete(&b);
	diff_eq_int("everything freed", harness_alloc.live, 0, 0);

	return diff_end();
}

/*
 * And the one path that does nothing: no S-register accessor.
 */
static int
run_dial_no_accessor(void)
{
	struct callprog a, b;
	struct callprog_cfg ca, cb;

	diff_begin("CALLPROG_Dial: no S-register accessor");

	harness_param_reset();
	harness_alloc_reset();
	harness_param_set(MustNoiseFilterBeApplied, 0);
	harness_param_set(MDMPRM_DP_ADDR, 0);

	memset(&a, 0xA5, sizeof(a));
	memset(&b, 0xA5, sizeof(b));
	ca.w0 = 0;
	ca.get_sreg = sreg_stub;
	ca.modem = (void *)0xC0DEu;
	ca.w3 = 0;
	cb = ca;
	ref_CALLPROG_Create(&a, &ca);
	CALLPROG_Create(&b, &cb);

	a.get_sreg = b.get_sreg = 0;
	a.state = b.state = 4;
	ref_CALLPROG_Dial(&a, "T5551234");
	CALLPROG_Dial(&b, "T5551234");

	diff_eq_int("state untouched", b.state, a.state, 0);
	diff_eq_int("both left state 4", a.state, 4, 0);
	compare_object(&b, &a, 0);

	ref_CALLPROG_Delete(&a);
	CALLPROG_Delete(&b);

	return diff_end();
}

/*
 * The transcript with the harness's own callback markers taken out.
 *
 * `runtime.c` writes a `<< get_param N >>` line into the capture for every
 * parameter read, which is useful when reading a trace by eye and is not the
 * object's output.  The two sides do not read parameters in lockstep -- the
 * reconstruction reads some through a local while the object re-reads them --
 * so comparing the raw text compares the harness's instrumentation as much as
 * the modem's diagnostics.  `dsplib_debug_capture_lines` already excludes
 * them, which is why the line counts agreed while the strings did not.
 */
static const char *
printed_only(int side, char *buf, size_t n)
{
	const char *p = dsplib_debug_capture_text(side);
	size_t out = 0;

	while (*p != '\0' && out + 1 < n) {
		const char *nl = strchr(p, '\n');
		size_t len = nl != 0 ? (size_t)(nl - p) + 1 : strlen(p);

		if (strncmp(p, "<< ", 3) != 0) {
			if (out + len + 1 >= n)
				break;
			memcpy(buf + out, p, len);
			out += len;
		}
		p += len;
	}
	buf[out] = '\0';
	return buf;
}

/*
 * The same three functions again, with the diagnostics turned on and the two
 * transcripts compared.
 *
 * THIS IS A DIFFERENT CHECK, not a repeat.  Everything above runs at level 0,
 * where a gated call site is unreachable: `dsplibs_debug_level` ships at zero
 * and every announcement in this file compiles to a branch nobody takes.  So
 * a wrong format string, a wrong argument, a site in the wrong arm and a site
 * that was never restored at all are the same program, and none of the checks
 * above can tell them apart.  Fourteen sites in `CALLPROG_Create`,
 * `CALLPROG_Delete` and `CALLPROG_Dial` were in exactly that position --
 * placed, and never once executed by anything (finding 192).
 *
 * Level 1 must be silent: the gates are `> 1`.  The sweep to 3 is what says
 * so rather than the reading.
 */
static int
run_trace(void)
{
	struct callprog a, b;
	struct callprog_cfg ca, cb;
	static char pa[65536], pb[65536];
	unsigned lines = 0;
	int lvl, blind, rc;

	diff_begin("CALLPROG create/dial/delete: the transcripts agree");
	for (lvl = 1; lvl <= 3; lvl++) {
		for (blind = 0; blind <= 1; blind++) {
			harness_param_reset();
			harness_alloc_reset();
			harness_param_set(MustNoiseFilterBeApplied, blind);
			harness_param_set(GetDialToneCallProgressFilterIndex, 1);
			harness_param_set(GetBusyToneCallProgressFilterIndex, 0);
			harness_param_set(GetDialToneFilterSubindex, 0);
			harness_param_set(GetCallProgressSamplesBufferLength,
					  666);
			harness_param_set(GetDialToneValidationTime, 50);
			harness_param_set(GetDialToneDetectionThreshold, 40);
			harness_param_set(GetMaxBusyCadenceOnTime, 55);
			harness_param_set(GetMinBusyCadenceOnTime, 20);
			harness_param_set(GetMinBusyCadenceOffTime, 20);
			harness_param_set(GetMaxBusyCadenceOffTime, 55);
			harness_param_set(GetBusyDetectionCyclesNumber, 3);
			harness_param_set(GetBusyToneLooseDetectionEnabled,
					  blind);
			harness_param_set(GetCallingToneFlag, 2);
			harness_param_set(GetNoAnswerTimeOut, 45);
			harness_param_set(GetBlindDialPause, blind ? 6 : 0);
			harness_param_set(GetDialToneWaitTime, 9);
			harness_param_set(GetDialModifierValidation, 0);
			harness_param_set(GetPulseDialMakeTime, 33);
			harness_param_set(GetPulseDialBreakTime, 67);
			harness_param_set(GetDTMFHighToneLevel, 9);
			harness_param_set(GetDTMFHighAndLowToneLevelDifference,
					  2);
			/*
			 * No datapump: CALLPROG_Dial reaches SetPulseMakeTime
			 * through DialerCreate and that dereferences whatever
			 * this returns, so the store's derived value is a wild
			 * pointer.  Same reason as run_dial above.
			 */
			harness_param_set(MDMPRM_DP_ADDR, 0);

			memset(&a, 0xA5, sizeof(a));
			memset(&b, 0xA5, sizeof(b));
			ca.w0 = 1;
			ca.get_sreg = sreg_stub;
			ca.modem = (void *)0xC0DEu;
			ca.w3 = 0;
			cb = ca;

			/*
			 * Both levels move together.  Raising only ours would
			 * compare a transcript against silence, which passes
			 * for the wrong reason the moment the reference is the
			 * side that stops printing.
			 */
			dsplibs_debug_level = ref_dsplibs_debug_level =
				(unsigned)lvl;
			dsplib_debug_capture_on = 1;
			dsplib_debug_capture_reset();

			ref_CALLPROG_Create(&a, &ca);
			CALLPROG_Create(&b, &cb);
			ref_CALLPROG_Dial(&a, "T5551234");
			CALLPROG_Dial(&b, "T5551234");
			ref_CALLPROG_Delete(&a);
			CALLPROG_Delete(&b);

			dsplibs_debug_level = ref_dsplibs_debug_level = 0;
			dsplib_debug_capture_on = 0;

			rc = strcmp(printed_only(0, pa, sizeof(pa)),
				    printed_only(1, pb, sizeof(pb)));
			if (rc != 0 && getenv("DBGDIFF") != NULL) {
				/* Side 0 is the reconstruction, 1 the blob. */
				fprintf(stderr, "--- ours (level %d) ---\n%s"
					"--- blob ---\n%s", lvl, pa, pb);
			}
			diff_eq_int("transcript (%ld)", rc == 0, 1,
				    (long)(lvl * 2 + blind));
			diff_eq_int("line count (%ld)",
				    (long)dsplib_debug_capture_lines(1),
				    (long)dsplib_debug_capture_lines(0),
				    (long)(lvl * 2 + blind));
			if (lvl == 1) {
				/* The gates are `> 1`, so nothing may fire. */
				diff_eq_int("level 1 is silent (%ld)",
					    (long)dsplib_debug_capture_lines(0),
					    0, (long)blind);
			}
			lines += dsplib_debug_capture_lines(0);
		}
	}
	/*
	 * Anti-vacuity.  Two identical empty strings compare equal, so without
	 * this the whole block passes on a tree where every site was deleted.
	 */
	diff_eq_int("the trace said something (%ld)", lines > 20, 1,
		    (long)lines);
	return diff_end();
}

int
main(void)
{
	int rc = 0;

	rc |= run_create("CALLPROG_Create: no band filter", 0);
	rc |= run_create("CALLPROG_Create: with band filter", 1);
	rc |= run_create("CALLPROG_Create: band flag 99", 99);

	rc |= run_dial("CALLPROG_Dial: wait for dial tone", 0, 2, 5, 1,
		       "T5551234");
	rc |= run_dial("CALLPROG_Dial: blind dial", 1, 2, 5, 1, "T5551234");
	rc |= run_dial("CALLPROG_Dial: long validation", 0, 2, 95, 1,
		       "T5551234");
	rc |= run_dial("CALLPROG_Dial: calling tone 0", 0, 0, 5, 1, "P123");
	rc |= run_dial("CALLPROG_Dial: calling tone 1", 0, 1, 5, 1, "P123");
	rc |= run_dial("CALLPROG_Dial: calling tone 7", 0, 7, 5, 1, "P123");
	rc |= run_dial("CALLPROG_Dial: from state 7", 0, 2, 5, 7, "T9");
	rc |= run_dial("CALLPROG_Dial: bad dial string", 0, 2, 5, 1, "5551234");
	rc |= run_dial_no_accessor();

	/*
	 * Anti-vacuity: the tables must not be all zeros, which is what they
	 * would be if neither side had built anything.
	 */
	diff_begin("guards");
	{
		int s, e, nonzero = 0;

		for (s = 0; s < CALLPROG_STATES; s++)
			for (e = 0; e < CALLPROG_CPTD_EVENTS; e++)
				if (next_state_due_cptd[s][e] != 0)
					nonzero++;
		diff_eq_int("the machine has transitions (%ld)", nonzero >= 30,
			    1, nonzero);
		diff_eq_int("busy is listened for everywhere",
			    toneiir_busy_table[0] && toneiir_busy_table[9], 1,
			    0);
		diff_eq_int("dial tone only where it should be",
			    toneiir_dialtone_table[1]
			    && !toneiir_dialtone_table[5], 1, 0);
	}
	rc |= diff_end();

	/*
	 * Last, because it rebuilds the tables from its own parameters and the
	 * guards above read whatever the previous run left.
	 */
	rc |= run_trace();

	return rc;
}
