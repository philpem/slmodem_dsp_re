/* Zero-budget transcript collector controls, run by period and modern tiers. */
#include "harness.h"
#include "transcript_evidence.h"

extern int dsplibs_debug_printf(const char *, ...);
extern int ref_dsplibs_debug_printf(const char *, ...);

static void pair(const char *a, const char *b)
{
	dsplib_debug_capture_reset();
	dsplibs_debug_printf("%s", a);
	ref_dsplibs_debug_printf("%s", b);
}

int main(void)
{
	static char huge[262145];
	static const char *bad[] = {
		"totally wrong text\r\n", "power = -12.125 dBm0\r\n",
		"power = +121.250 dBm0\r\n", "power = +12.125 watts\r\n",
		"power = +12.126 dBm0\r\n", "power = +12.125 dBm0\n",
		"power = +12.125 dBm0\r\nextra\r\n", ""
	};
	unsigned i;
	diff_begin("transcript lossless controls");
	dsplib_debug_capture_on = 1;
	pair("power = +12.125 dBm0\r\n", "power = +12.125 dBm0\r\n");
	diff_eq_int("identical transcript", transcript_exact("control.same", 7), 1, 7);
	for (i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
		pair(bad[i], "power = +12.125 dBm0\r\n");
		/* Deliberately the SAME site/input for every corruption. The
		 * Boolean diagnostic collision must not lose the actual text. */
		diff_eq_int("same-tag corruption rejected (%ld)",
			transcript_exact("control.same", 7), 0, i);
	}
	pair("sum = -1\r\nabs sum = +2\r\n", "sum = +2\r\nabs sum = -1\r\n");
	diff_eq_int("swapped fields", transcript_exact("control.swap", 8), 0, 8);
	pair("a\nb\n", "b\na\n");
	diff_eq_int("order", transcript_exact("control.order", 9), 0, 9);
	dsplib_debug_capture_reset();
	dsplibs_debug_printf("%cA", 0);
	ref_dsplibs_debug_printf("%cB", 0);
	diff_eq_int("embedded NUL does not hide suffix",
		transcript_exact("control.nul", 10), 0, 10);
	diff_eq_int("embedded NUL length", dsplib_debug_capture_size(0), 2, 10);
	dsplib_debug_capture_reset();
	dsplibs_debug_printf("ab");
	ref_dsplibs_debug_printf("a");
	ref_dsplibs_debug_printf("b");
	diff_eq_int("callback count", transcript_exact("control.count", 11), 0, 11);
	memset(huge, 'x', sizeof(huge) - 1);
	huge[sizeof(huge) - 1] = 0;
	pair(huge, huge);
	diff_eq_int("equal truncation rejected", transcript_exact("control.cap", 12), 0, 12);
	diff_eq_int("overflow marked side 0", dsplib_debug_capture_complete(0), 0, 12);
	diff_eq_int("overflow marked side 1", dsplib_debug_capture_complete(1), 0, 12);
	pair("reset", "reset");
	diff_eq_int("reset clears overflow", transcript_exact("control.reset", 13), 1, 13);
	dsplib_debug_capture_on = 0;
	return diff_end();
}
