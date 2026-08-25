/*
 * pulse.c -- Pulse dialling: dialling by interrupting the line.
 *
 * Reconstructed from dsplibs.o call.c:
 *
 *   PulseDialDigit        .text 0x003200
 *   IsPulseDialerReady    .text 0x0032a0
 *   LastPulseDigitDialed  .text 0x003420
 *   SetPulseBreakTime     .text 0x003480
 *   SetPulseMakeTime      .text 0x0034e0
 *
 * See pulse.h for the shape of a digit.  The rest of call.c is not
 * reconstructed yet; these five are separated out because Dialer.c calls them
 * and nothing else in call.c does.
 */

#include <stdint.h>

#include "dsplib/debug.h"
#include "dsplib/pulse.h"
#include "dsplib/modem_params.h"


/*
 * The call object, or NULL when the modem has no datapump.  Every function
 * here begins with this and every one tolerates the NULL.
 */
static struct call *
call_of(void *modem)
{
	return (struct call *)(intptr_t)modem_get_param(modem, MDMPRM_DP_ADDR);
}

void
PulseDialDigit(void *modem, int digit)
{
	struct call *c = call_of(modem);

	if (c == 0)
		return;

	/* Reported BEFORE the zero-dials-ten fix-up, so a '0' prints as 0. */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("call: PulseDialDigit %lu...\n",
				     (unsigned long)digit);

	/* Zero dials ten, which is how loop disconnect has always spelled it. */
	if (digit == 0)
		digit = 10;

	c->self->pulse_remaining = digit;
	c->self->pulse_elapsed = 0;

	modem_set_param(modem, MDMPRM_PULSE_DIAL, digit);
}

int
IsPulseDialerReady(void *modem)
{
	struct call *dp = call_of(modem);
	struct call *c;
	int remaining, elapsed;

	/*
	 * No datapump, so nothing can be pulsing.  Answering "ready" rather
	 * than "busy" is what stops a caller polling this from spinning
	 * forever on a modem that has been torn down.
	 */
	if (dp == 0)
		return 1;

	c = dp->self;

	/*
	 * Reported on entry, before the remaining==0 early out, so a caller
	 * polling an idle dialler gets one line per tick.  Both arguments are
	 * read straight from the object -- the locals below do not exist yet.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("call: IsPulseDialerReady !(%u) (count %d)\n",
				     (unsigned)c->pulse_remaining,
				     c->pulse_elapsed);

	remaining = c->pulse_remaining;
	if (remaining == 0)
		return 1;

	elapsed = c->pulse_elapsed;

	if (elapsed < c->pulse_break) {
		if (c->pulse_off_hook == 0) {
			/* Start of a pulse: interrupt the line. */
			c->pulse_off_hook = 1;

			/*
			 * Reported after the flag is set but BEFORE the line
			 * moves; the "hook off" case below is the other way
			 * round.  Not symmetry for its own sake: the store to
			 * pulse_off_hook is ahead of the call in the object
			 * (0x3308, gate at 0x32fc), and a store cannot be
			 * hoisted over an external call, so this order is the
			 * source order.
			 */
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf("call: %d: hook on...\n",
						     remaining);

			modem_set_param(dp->modem, MDMPRM_HOOK_ON, 1);
			c->pulse_elapsed = elapsed + PULSE_TICK_MS;
			return c->pulse_remaining == 0;
		}
	} else if (c->pulse_off_hook != 0) {
		/* Break time is up: restore the line. */
		c->pulse_off_hook = 0;
		modem_set_param(dp->modem, MDMPRM_HOOK_ON, 0);

		/* After the line moves, unlike "hook on" above (0x33d8). */
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("call: %d: hook off...\n",
					     remaining);
		c->pulse_elapsed = elapsed + PULSE_TICK_MS;
		return c->pulse_remaining == 0;
	}

	/*
	 * Mid-pulse, in the make half.  Both the "line already interrupted and
	 * still within break" case and the "line already restored" case land
	 * here, which is why the two tests above only act on a transition.
	 */
	if (elapsed < c->pulse_break + c->pulse_make) {
		c->pulse_elapsed = elapsed + PULSE_TICK_MS;
	} else {
		/* One whole pulse sent. */
		c->pulse_elapsed = 0;
		remaining--;
		c->pulse_remaining = remaining;
		c->pulse_elapsed = PULSE_TICK_MS;
	}

	return remaining == 0;
}

/*
 * Note this one does not look at the call object at all -- it just tells the
 * host the pulse dialler is done, by clearing the parameter PulseDialDigit
 * set.  So the host, not the library, owns the "a digit is being pulsed"
 * state; the library only owns the timing.
 */
void
LastPulseDigitDialed(void *modem)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("call: LastPulseDigitDialed...\n");

	modem_set_param(modem, MDMPRM_PULSE_DIAL, 0);
}

void
SetPulseBreakTime(void *modem, int ms)
{
	struct call *c = call_of(modem);

	if (c == 0)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("call: SetPulseBreakTime %lu\n",
				     (unsigned long)ms);

	c->self->pulse_break = ms;
}

void
SetPulseMakeTime(void *modem, int ms)
{
	struct call *c = call_of(modem);

	if (c == 0)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("call: SetPulseMakeTime %lu\n",
				     (unsigned long)ms);

	c->self->pulse_make = ms;
}
