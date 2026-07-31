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

#include "dsplib/pulse.h"
#include "dsplib/modem_params.h"

extern int modem_get_param(void *modem, int name);
extern void modem_set_param(void *modem, int name, int value);

/*
 * The call object, or NULL when the modem has no datapump.  Every function
 * here begins with this and every one tolerates the NULL.
 */
static struct call *
call_of(void *modem)
{
	return (struct call *)modem_get_param(modem, MDMPRM_DP_ADDR);
}

void
SetPulseMakeTime(void *modem, int ms)
{
	struct call *c = call_of(modem);

	if (c == 0)
		return;
	c->self->pulse_make = ms;
}

void
SetPulseBreakTime(void *modem, int ms)
{
	struct call *c = call_of(modem);

	if (c == 0)
		return;
	c->self->pulse_break = ms;
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
	modem_set_param(modem, MDMPRM_PULSE_DIAL, 0);
}

void
PulseDialDigit(void *modem, int digit)
{
	struct call *c = call_of(modem);

	if (c == 0)
		return;

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
	remaining = c->pulse_remaining;
	if (remaining == 0)
		return 1;

	elapsed = c->pulse_elapsed;

	if (elapsed < c->pulse_break) {
		if (c->pulse_off_hook == 0) {
			/* Start of a pulse: interrupt the line. */
			c->pulse_off_hook = 1;
			modem_set_param(dp->modem, MDMPRM_HOOK_ON, 1);
			c->pulse_elapsed = elapsed + PULSE_TICK_MS;
			return c->pulse_remaining == 0;
		}
	} else if (c->pulse_off_hook != 0) {
		/* Break time is up: restore the line. */
		c->pulse_off_hook = 0;
		modem_set_param(dp->modem, MDMPRM_HOOK_ON, 0);
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
