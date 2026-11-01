/*
 * The host diagnostic import observes a promoted int before printf's %c
 * conversion hides its sign. These are constructor-created parser components,
 * not negotiated modem histories. The Digit call's exact format and argument
 * and the initial mode announcement are forwarded to the ordinary harness.
 */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/dialer.h"
#include "dsplib/pulse.h"
#include "dsplib/modem_params.h"

extern int ref_DialerCreate(struct dialer *, const char *, void *);
extern unsigned int ref_dsplibs_debug_level;
extern int ref_GetNextDigitAndReturnNextState(struct dialer *)
	__attribute__((regparm(2)));
#if __GNUC__ >= 4
extern int GetNextDigitAndReturnNextState(struct dialer *)
	__attribute__((weak, regparm(3)));
#else
extern int GetNextDigitAndReturnNextState(struct dialer *)
	__attribute__((weak, regparm(2)));
#endif
extern int __real_dsplibs_debug_printf(const char *, ...);
extern int __real_ref_dsplibs_debug_printf(const char *, ...);

static int arguments[2][3], calls[2], mode_calls[2];

static int
observe(int side, const char *fmt, va_list ap)
{
	int value;
	if (strcmp(fmt, "Dialer.c: GetNextDigit... TONE_OR_PULSE_FLAG became "
			"TONE_DIALING\n") == 0) {
		mode_calls[side]++;
		return side ? __real_ref_dsplibs_debug_printf(fmt)
			    : __real_dsplibs_debug_printf(fmt);
	}
	if (strcmp(fmt, "Digit is %c\n") != 0) {
		fprintf(stderr, "t_dialerdigitargs: unexpected diagnostic format\n");
		abort();
	}
	value = va_arg(ap, int);
	if (calls[side] < 3)
		arguments[side][calls[side]] = value;
	calls[side]++;
	return side ? __real_ref_dsplibs_debug_printf(fmt, value)
		    : __real_dsplibs_debug_printf(fmt, value);
}

int __wrap_dsplibs_debug_printf(const char *fmt, ...)
{
	va_list ap;
	int result;
	va_start(ap, fmt);
	result = observe(0, fmt, ap);
	va_end(ap);
	return result;
}

int __wrap_ref_dsplibs_debug_printf(const char *fmt, ...)
{
	va_list ap;
	int result;
	va_start(ap, fmt);
	result = observe(1, fmt, ap);
	va_end(ap);
	return result;
}

static void
params(struct call *call)
{
	harness_param_reset();
	harness_param_set(GetPulseDialDigitPattern, 1);
	harness_param_set(GetPulseDialingFlag, 1);
	harness_param_set(GetDialModifierValidation, 1);
	harness_param_set(GetPulseDialMakeTime, 33);
	harness_param_set(GetPulseDialBreakTime, 67);
	harness_param_set(GetDTMFDialSpeed, 70);
	harness_param_set(MDMPRM_DP_ADDR, (long)(intptr_t)call);
}

int main(void)
{
	unsigned byte;
	int configurations = 0;
	if (GetNextDigitAndReturnNextState == 0) {
#if __GNUC__ < 4
		fprintf(stderr, "t_dialerdigitargs: required period parser absent\n");
		return 1;
#else
		fprintf(stderr, "t_dialerdigitargs: parser inlined in this compiler "
			"tier; 0/128 configurations exercised\n");
		return 0;
#endif
	}
	diff_begin("Dialer diagnostic promoted digit arguments");
	for (byte = 128; byte <= 255; byte++) {
		struct dialer a, b;
		struct call ca, cb;
		char text[4];
		int ra, rb, j;
		int expected[3];
		text[0] = 'T'; text[1] = (char)byte;
		text[2] = '5'; text[3] = '\0';
		expected[0] = 'T'; expected[1] = (int)byte - 256;
		expected[2] = '5';
		memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
		memset(&ca, 0, sizeof ca); memset(&cb, 0, sizeof cb);
		ca.self = &ca; cb.self = &cb;
		ca.modem = cb.modem = (void *)0xD1A1u;
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		params(&ca); ra = ref_DialerCreate(&a, text, ca.modem);
		params(&cb); rb = DialerCreate(&b, text, cb.modem);
		diff_eq_int("reference constructor accepted", ra, 0, byte);
		diff_eq_int("constructor accepted", rb, 0, byte);
		diff_eq_int("reference tolerated input", a.grade, DIALER_TOLERABLE, byte);
		diff_eq_int("tolerated input", b.grade, DIALER_TOLERABLE, byte);
		diff_eq_int("reference owns input copy", memcmp(a.string, text, 4), 0, byte);
		diff_eq_int("owns input copy", memcmp(b.string, text, 4), 0, byte);
		memset(arguments, 0, sizeof arguments); memset(calls, 0, sizeof calls);
		memset(mode_calls, 0, sizeof mode_calls);
		dsplib_debug_capture_reset(); dsplib_debug_capture_on = 1;
		dsplibs_debug_level = ref_dsplibs_debug_level = 2;
		ra = ref_GetNextDigitAndReturnNextState(&a);
		rb = GetNextDigitAndReturnNextState(&b);
		dsplibs_debug_level = ref_dsplibs_debug_level = 0;
		dsplib_debug_capture_on = 0;
		diff_eq_int("reference digit state", ra, 1, byte);
		diff_eq_int("digit state", rb, 1, byte);
		diff_eq_int("reference consumed input", a.pos, 2, byte);
		diff_eq_int("consumed input", b.pos, 2, byte);
		diff_eq_int("reference row", a.row, 1, byte);
		diff_eq_int("row", b.row, 1, byte);
		diff_eq_int("reference column", a.col, 1, byte);
		diff_eq_int("column", b.col, 1, byte);
		diff_eq_obj("parser after input", struct dialer, &b, &a, byte);
		diff_eq_int("reference diagnostic calls", calls[1], 3, byte);
		diff_eq_int("diagnostic calls", calls[0], 3, byte);
		diff_eq_int("reference mode announcement", mode_calls[1], 1, byte);
		diff_eq_int("mode announcement", mode_calls[0], 1, byte);
		for (j = 0; j < 3; j++) {
			diff_eq_int("reference promoted argument", arguments[1][j],
				    expected[j], (long)byte * 3 + j);
			diff_eq_int("promoted argument", arguments[0][j],
				    expected[j], (long)byte * 3 + j);
		}
		diff_eq_int("rendered text agrees",
			strcmp(dsplib_debug_capture_text(0), dsplib_debug_capture_text(1)),
			0, byte);
		diff_eq_int("rendered call boundaries agree",
			dsplib_debug_capture_boundaries_equal(), 1, byte);
		diff_eq_int("reference capture complete", dsplib_debug_capture_complete(1), 1, byte);
		diff_eq_int("capture complete", dsplib_debug_capture_complete(0), 1, byte);
		configurations++;
	}
	diff_eq_int("constructed configurations", configurations, 128, 0);
	fprintf(stderr, "t_dialerdigitargs: %d constructed inputs, "
		"384 promoted arguments per side\n", configurations);
	return diff_end();
}
