/*
 * t_vcedle.c -- differential test of `voice_dle_command`.
 *
 * THE WHOLE CONTEXT IS COMPARED, NOT THE TWO FLAGS.  `struct voice_ctx` is
 * 0x74c bytes and only two of them are modelled, so a store anywhere else --
 * which is exactly what a wrong offset would be -- has to be visible.  Both
 * sides get their own copy, both are prefilled with 0xa5 (never zero: zero is
 * the one filler that makes "never written" look like "written correctly"),
 * and `diff_eq_obj` compares all 1,868 bytes after every call.
 *
 * THE SWEEP IS EVERY BYTE VALUE, and it is done twice -- once over
 * `signed char` -128..127 and once over the 0..255 an `unsigned char` caller
 * would hand across -- because the object loads the command with `movsbl`.
 * The two sweeps disagree about nothing here (the two live cases are 0x03 and
 * 0x18, both positive) and that AGREEMENT is the result: it bounds the
 * signedness question to the printed text, which the transcript check below
 * then settles.
 *
 * THE TRANSCRIPT IS THE ONLY PLACE THE SIGN SHOWS.  At level 2 the default
 * arm prints the command with `%2x`, so 0x83 renders as `ffffff83` if the
 * value is sign-extended and `83` if it is not.  Levels 0, 1 and 2 are all
 * run, because every gated site here is `> 1`.
 *
 * COVERAGE IS ASSERTED FROM THE RUN (F134): `main` fails unless all three
 * arms were taken and unless the sweep made the number of calls it claims.
 */

#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "dsplib/debug.h"
#include "dsplib/voicecmd.h"

extern unsigned int ref_dsplibs_debug_level;
extern int ref_voice_dle_command(struct voice_ctx *v, signed char cmd);

extern int dsplib_debug_capture_on;
void dsplib_debug_capture_reset(void);
unsigned dsplib_debug_capture_lines(int side);
const char *dsplib_debug_capture_text(int side);

static long seen_etx, seen_can, seen_other, calls;

static void
set_level(unsigned int lvl)
{
	dsplibs_debug_level = lvl;
	ref_dsplibs_debug_level = lvl;
}

static void
one(int cmd, long tag)
{
	struct voice_ctx a, b;
	int ra, rb;

	memset(&a, HARNESS_MALLOC_FILL, sizeof a);
	memset(&b, HARNESS_MALLOC_FILL, sizeof b);

	ra = ref_voice_dle_command(&a, (signed char)cmd);
	rb = voice_dle_command(&b, (signed char)cmd);

	diff_eq_int("voice_dle_command(%ld) return", rb, ra, tag);
	diff_eq_obj("context after voice_dle_command", struct voice_ctx, &b,
		    &a, tag);
	calls++;

	if ((signed char)cmd == VOICE_DLE_ETX)
		seen_etx++;
	else if ((signed char)cmd == VOICE_DLE_CAN)
		seen_can++;
	else
		seen_other++;
}

static int
t_sweep(void)
{
	int c;

	diff_begin("voice_dle_command over every byte value, both signednesses");
	for (c = -128; c <= 127; c++)
		one(c, (long)c);
	for (c = 0; c <= 255; c++)
		one(c, (long)(1000 + c));
	return diff_end();
}

/*
 * The two live arms, spelled out against the header's constants rather than
 * only against the blob -- so a reconstruction that agreed with the blob
 * because BOTH had been changed still fails here.
 */
static int
t_arms(void)
{
	struct voice_ctx v;
	int r;

	diff_begin("what each arm leaves behind, stated absolutely");

	memset(&v, HARNESS_MALLOC_FILL, sizeof v);
	r = voice_dle_command(&v, VOICE_DLE_ETX);
	diff_eq_int("ETX returns 0", r, 0, VOICE_DLE_ETX);
	diff_eq_int("ETX sets dle_etx", v.dle_etx, 1, VOICE_DLE_ETX);
	diff_eq_int("ETX leaves dle_can alone", v.dle_can, (int)0xa5a5a5a5,
		    VOICE_DLE_ETX);

	memset(&v, HARNESS_MALLOC_FILL, sizeof v);
	r = voice_dle_command(&v, VOICE_DLE_CAN);
	diff_eq_int("CAN returns 9", r, VOICE_DLE_CAN_STATUS, VOICE_DLE_CAN);
	diff_eq_int("CAN sets dle_can", v.dle_can, 1, VOICE_DLE_CAN);
	diff_eq_int("CAN leaves dle_etx alone", v.dle_etx, (int)0xa5a5a5a5,
		    VOICE_DLE_CAN);

	memset(&v, HARNESS_MALLOC_FILL, sizeof v);
	r = voice_dle_command(&v, 0x41);
	diff_eq_int("an unknown command returns 0", r, 0, 0x41);
	diff_eq_int("...and sets neither flag",
		    v.dle_etx == (int)0xa5a5a5a5
		    && v.dle_can == (int)0xa5a5a5a5, 1, 0x41);

	return diff_end();
}

static int
t_debug(void)
{
	static const int cmds[] = { VOICE_DLE_ETX, VOICE_DLE_CAN, 0x41,
				    (int)0x83, 0x00, 0x7f };
	static const unsigned int levels[] = { 0, 1, 2 };
	unsigned int l;
	unsigned int i;

	diff_begin("the diagnostic transcript at levels 0, 1 and 2");

	for (l = 0; l < 3; l++) {
		unsigned int lvl = levels[l];
		unsigned lines_ours, lines_ref;

		set_level(lvl);
		dsplib_debug_capture_reset();
		dsplib_debug_capture_on = 1;
		for (i = 0; i < sizeof cmds / sizeof cmds[0]; i++) {
			struct voice_ctx a, b;

			memset(&a, HARNESS_MALLOC_FILL, sizeof a);
			memset(&b, HARNESS_MALLOC_FILL, sizeof b);
			(void)ref_voice_dle_command(&a, (signed char)cmds[i]);
			(void)voice_dle_command(&b, (signed char)cmds[i]);
		}
		dsplib_debug_capture_on = 0;

		lines_ours = dsplib_debug_capture_lines(0);
		lines_ref = dsplib_debug_capture_lines(1);
		diff_eq_int("line count at level %ld", lines_ours, lines_ref,
			    (long)lvl);
		/* One line per command at level 2, none below it. */
		diff_eq_int("line count is 0 or 6 at level %ld", lines_ours,
			    lvl > 1 ? 6 : 0, (long)lvl);
		diff_eq_int("transcript text at level %ld",
			    strcmp(dsplib_debug_capture_text(0),
				   dsplib_debug_capture_text(1)) == 0, 1,
			    (long)lvl);
		/*
		 * And the sign, read straight out of the text: 0x83 is
		 * printed sign-extended, so `ffffff83` must appear and a bare
		 * ` 83` must not.  This is the one observable difference
		 * between `signed char` and `unsigned char` here.
		 */
		if (lvl > 1)
			diff_eq_int("0x83 prints sign-extended at level %ld",
				    strstr(dsplib_debug_capture_text(1),
					   "ffffff83") != 0, 1, (long)lvl);
	}
	set_level(0);
	return diff_end();
}

static int
t_coverage(void)
{
	diff_begin("the sweep reached all three arms");
	diff_eq_int("ETX was seen", seen_etx, 2, 0);
	diff_eq_int("CAN was seen", seen_can, 2, 0);
	diff_eq_int("other commands were seen", seen_other, 508, 0);
	diff_eq_int("the sweep was not empty", calls, 512, 0);
	fprintf(stderr, "t_vcedle: %ld calls, etx=%ld can=%ld other=%ld\n",
		calls, seen_etx, seen_can, seen_other);
	return diff_end();
}

int
main(void)
{
	int failed = 0;

	failed |= t_sweep();
	failed |= t_arms();
	failed |= t_debug();
	failed |= t_coverage();
	return failed;
}
