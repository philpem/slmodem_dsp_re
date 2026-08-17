/*
 * t_vparser.c -- differential test of `Vparser_read_int` and
 * `Vparser_read_float`, the two three-byte stubs in src/core/Vparser.c.
 *
 * THREE BYTES EACH, so this test can be complete rather than representative:
 * `xor %eax,%eax; ret` has no input it can distinguish and no state it can
 * carry.  What there IS to check is exactly three things, and all three are
 * checked against the blob's own copy rather than against a literal:
 *
 *   - the return value is 0, for every argument shape;
 *   - nothing is written through the third argument, which is the whole
 *     reason `loadParams` has no observable behaviour;
 *   - nothing is written through the first two either, and nothing is written
 *     past the target -- a guard band either side of it says so.
 *
 * THE ARGUMENT SWEEP IS NOT DECORATION.  A stub that ignores its arguments
 * and a function that reads them and happens to find nothing look identical
 * on one call.  Null and non-null file pointers, an empty and a long name,
 * and a target holding poison rather than zero separate "did not write"
 * from "wrote what was already there" -- the mistake t_v90params.cpp's own
 * header records this tree making three times.
 *
 * IT IS ALSO THE BINARY WHERE THE BLOB'S OWN STUBS RUN.  `t_v90loadparams`
 * weakens both sides' copies out and supplies logging ones, so it proves the
 * call SEQUENCE and never executes either stub.  This one links them
 * untouched and executes both.  Neither test subsumes the other.
 *
 * Finding 6400.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/Vparser.h"

extern int ref_Vparser_read_int(char *paramFile, const char *name, int *value);
extern int ref_Vparser_read_float(char *paramFile, const char *name,
				  float *value);

/*
 * The target, with a guard word either side.  `slot[1]` is what is passed;
 * `slot[0]` and `slot[2]` catch a store that lands one word out, which is the
 * shape a wrong `lea` displacement would have.
 */
static unsigned int ours[3];
static unsigned int theirs[3];

#define POISON0	0xa5000000u
#define POISON1	0xa5000004u
#define POISON2	0xa5000008u

static void
fill(void)
{
	ours[0] = theirs[0] = POISON0;
	ours[1] = theirs[1] = POISON1;
	ours[2] = theirs[2] = POISON2;
}

/* The names are the object's own, taken from two of the 349 call sites. */
static char file_a[] = "v90.par";
static char file_b[] = "";
static const char *const names[] = {
	"PROBING_MODE",
	"",
	"MAX_TX_RATE_INDEX_FOR_SENSITIVE_ISP",
	"V92_MAX_NOF_COEFFS_IN_EACH_SECTION"
};

int
main(void)
{
	unsigned n, f;
	char *files[3];
	char namebuf[64];

	files[0] = (char *)0;
	files[1] = file_a;
	files[2] = file_b;

	diff_begin("vparser");

	for (f = 0; f < 3; f++) {
		for (n = 0; n < sizeof names / sizeof names[0]; n++) {
			long tag = (long)(f * 100 + n);
			int r_ours, r_theirs;

			memset(namebuf, 0, sizeof namebuf);
			memcpy(namebuf, names[n], strlen(names[n]) + 1);

			fill();
			r_ours = Vparser_read_int(files[f], namebuf,
						  (int *)&ours[1]);
			r_theirs = ref_Vparser_read_int(files[f], namebuf,
							(int *)&theirs[1]);
			diff_eq_int("read_int returns (case %ld)",
				    r_ours, r_theirs, tag);
			diff_eq_int("read_int leaves the target (case %ld)",
				    (int)ours[1], (int)theirs[1], tag);
			diff_eq_int("read_int target unchanged (case %ld)",
				    (int)ours[1], (int)POISON1, tag);
			diff_eq_int("read_int guard low (case %ld)",
				    (int)ours[0], (int)theirs[0], tag);
			diff_eq_int("read_int guard high (case %ld)",
				    (int)ours[2], (int)theirs[2], tag);

			fill();
			r_ours = Vparser_read_float(files[f], namebuf,
						    (float *)&ours[1]);
			r_theirs = ref_Vparser_read_float(files[f], namebuf,
							  (float *)&theirs[1]);
			diff_eq_int("read_float returns (case %ld)",
				    r_ours, r_theirs, tag);
			diff_eq_int("read_float leaves the target (case %ld)",
				    (int)ours[1], (int)theirs[1], tag);
			diff_eq_int("read_float target unchanged (case %ld)",
				    (int)ours[1], (int)POISON1, tag);
			diff_eq_int("read_float guard low (case %ld)",
				    (int)ours[0], (int)theirs[0], tag);
			diff_eq_int("read_float guard high (case %ld)",
				    (int)ours[2], (int)theirs[2], tag);

			/*
			 * The name is an INPUT.  Both sides were handed a
			 * MUTABLE copy of it, so a stub that wrote through
			 * its second argument shows up here and nowhere else.
			 */
			diff_eq_int("the name is not written (case %ld)",
				    memcmp(namebuf, names[n],
					   strlen(names[n]) + 1),
				    0, tag);
		}
	}

	return diff_end();
}
