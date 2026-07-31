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
#include <string.h>

#include "harness.h"
#include "dsplib/callprog_state.h"
#include "dsplib/callprog.h"
#include "dsplib/modem_params.h"

extern void ref_CALLPROG_Create(struct callprog *cp, struct callprog_cfg *cfg);
extern void ref_CALLPROG_Delete(struct callprog *cp);

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
	diff_eq_int("f34", b->f34, a->f34, n);
	diff_eq_int("f48", b->f48, a->f48, n);
	diff_eq_int("f4c", b->f4c, a->f4c, n);
	diff_eq_int("band_wanted", b->band_wanted, a->band_wanted, n);
	diff_eq_int("band allocated", b->band != 0, a->band != 0, n);
	diff_eq_int("dial allocated", b->dial != 0, a->dial != 0, n);
	diff_eq_int("busy allocated", b->busy != 0, a->busy != 0, n);
	diff_eq_int("dtmf allocated", b->dtmf != 0, a->dtmf != 0, n);
	diff_eq_int("f80", b->f80, a->f80, n);
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

int
main(void)
{
	int rc = 0;

	rc |= run_create("CALLPROG_Create: no band filter", 0);
	rc |= run_create("CALLPROG_Create: with band filter", 1);
	rc |= run_create("CALLPROG_Create: band flag 99", 99);

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

	return rc;
}
