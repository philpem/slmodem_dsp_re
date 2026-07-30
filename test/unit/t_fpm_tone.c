/*
 * t_fpm_tone.c -- differential test of the tone generator.
 *
 * FPM_TONE_create is not reconstructed yet, so objects are built with the
 * *reference* implementation and then copied: both sides operate on
 * byte-identical starting state.  That isolates the functions under test from
 * the one that is still pending, and it is stronger than it sounds -- the
 * whole 0x108-byte object is compared afterwards, so a write to any field,
 * named or not, shows up.
 *
 * The reversal path needs care to reach: at the default 450-unit period and
 * 8 samples per unit, it takes 3600 samples to fire once.  Short bursts never
 * touch it, which is exactly how it would slip through a casual test.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/fpm_tone.h"

extern void *ref_FPM_TONE_create(void *state, void *cfg);
extern void ref_FPM_TONE_set_freq(void *state, short hz);
extern void ref_FPM_TONE_set_scale(void *state, short scale);
extern void ref_FPM_TONE_generate(void *state, short *out, short count);
extern short ref_FPM_TONE_CFG[];

/* Compare the whole object, so unnamed fields are covered too. */
static void
compare_state(const unsigned char *ours, const unsigned char *ref,
	      const char *what)
{
	int i;

	for (i = 0; i < FPM_TONE_STATE_SIZE; i++)
		diff_eq_int("state byte 0x%02lx", ours[i], ref[i], i);
	(void)what;
}

/*
 * Compare two created objects.  The four buffer pointers and the internal
 * self-pointer at +0xfc necessarily differ between builds, so those slots are
 * skipped and the buffers compared by content instead.
 */
static int
is_pointer_slot(int off)
{
	return off == 0x10 || off == 0x2c || off == 0x30
	       || off == 0xf4 || off == 0xf8 || off == 0xfc;
}

static void
compare_created(const unsigned char *ours, const unsigned char *ref, int len,
		int extra, int same_cfg)
{
	int i;

	for (i = 0; i < FPM_TONE_STATE_SIZE; i += 2) {
		if (is_pointer_slot(i) || is_pointer_slot(i - 2))
			continue;
		diff_eq_int("created byte 0x%02lx",
			    *(const short *)(ours + i),
			    *(const short *)(ref + i), i);
	}

	if (same_cfg) {
		/* Same config object, so the copied pointer must be identical. */
		diff_eq_int("source pointer copied (%ld)",
			    *(void *const *)(ours + 0x10)
			    == *(void *const *)(ref + 0x10), 1, 0);
	} else {
		/*
		 * Different config objects -- ours uses our extracted ToneLPF,
		 * the reference its own.  The addresses cannot match, so check
		 * the thing that actually matters: that our extraction of the
		 * prototype agrees with the blob's, tap for tap.
		 */
		const short *pa = *(const short *const *)(ref + 0x10);
		const short *pb = *(const short *const *)(ours + 0x10);

		for (i = 0; i < len; i++)
			diff_eq_int("ToneLPF[%ld]", pb[i], pa[i], i);
	}

	/* Buffer contents, not addresses. */
	{
		const short *ra = *(const short *const *)(ref + 0x2c);
		const short *rb = *(const short *const *)(ours + 0x2c);
		const short *za = *(const short *const *)(ref + 0x30);
		const short *zb = *(const short *const *)(ours + 0x30);
		const short *ka = *(const short *const *)(ref + 0xf4);
		const short *kb = *(const short *const *)(ours + 0xf4);
		const short *aa = *(const short *const *)(ref + 0xf8);
		const short *ab = *(const short *const *)(ours + 0xf8);

		for (i = 0; i < len; i++)
			diff_eq_int("reference[%ld]", rb[i], ra[i], i);
		for (i = 0; i < len + extra; i++)
			diff_eq_int("zeroed[%ld]", zb[i], za[i], i);
		for (i = 0; i < 5; i++)
			diff_eq_int("resonator coeff[%ld]", kb[i], ka[i], i);
		for (i = 0; i < 4; i++)
			diff_eq_int("resonator acc[%ld]", ab[i], aa[i], i);
	}
}

int
main(void)
{
	unsigned char a[FPM_TONE_STATE_SIZE], b[FPM_TONE_STATE_SIZE];
	static short oa[8192], ob[8192];
	void *built;
	int rc = 0;
	int k;

	/* One reference object, cloned for both sides. */
	built = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
	if (built == 0) {
		diff_begin("fpm_tone create");
		diff_eq_int("reference built an object (%ld)", 0, 1, 0);
		return diff_end();
	}

	/* create: the whole object, buffers included. */
	diff_begin("FPM_TONE_create default cfg");
	{
		unsigned char *ca = ref_FPM_TONE_create(0, ref_FPM_TONE_CFG);
		unsigned char *cb = FPM_TONE_create(0, ref_FPM_TONE_CFG);

		diff_eq_int("ours built an object (%ld)", cb != 0, 1, 0);
		if (cb != 0)
			compare_created(cb, ca,
					*(short *)(ca + FPM_TONE_CFG_LEN),
					*(short *)(ca + FPM_TONE_CFG_EXTRA), 1);
	}
	rc |= diff_end();

	/* create with NULL cfg must fall back to the built-in configuration. */
	diff_begin("FPM_TONE_create NULL cfg");
	{
		unsigned char *ca = ref_FPM_TONE_create(0, 0);
		unsigned char *cb = FPM_TONE_create(0, 0);

		diff_eq_int("ours built an object (%ld)", cb != 0, 1, 0);
		if (cb != 0)
			compare_created(cb, ca,
					*(short *)(ca + FPM_TONE_CFG_LEN),
					*(short *)(ca + FPM_TONE_CFG_EXTRA), 0);
	}
	rc |= diff_end();

	diff_begin("FPM_TONE_set_freq");
	for (k = 0; k < 4000; k += 7) {
		memcpy(a, built, sizeof(a));
		memcpy(b, built, sizeof(b));
		ref_FPM_TONE_set_freq(a, (short)k);
		FPM_TONE_set_freq(b, (short)k);
		compare_state(b, a, "set_freq");
	}
	rc |= diff_end();

	diff_begin("FPM_TONE_set_scale");
	for (k = -32768; k < 32768; k += 251) {
		memcpy(a, built, sizeof(a));
		memcpy(b, built, sizeof(b));
		ref_FPM_TONE_set_scale(a, (short)k);
		FPM_TONE_set_scale(b, (short)k);
		compare_state(b, a, "set_scale");
	}
	rc |= diff_end();

	/* Short bursts: exercises the sample loop, never the reversal. */
	diff_begin("FPM_TONE_generate short");
	memcpy(a, built, sizeof(a));
	memcpy(b, built, sizeof(b));
	ref_FPM_TONE_set_freq(a, 2100);
	FPM_TONE_set_freq(b, 2100);
	for (k = 0; k < 60; k++) {
		int n = (k % 13) + 1, i;

		ref_FPM_TONE_generate(a, oa, (short)n);
		FPM_TONE_generate(b, ob, (short)n);
		for (i = 0; i < n; i++)
			diff_eq_int("burst sample %ld", ob[i], oa[i], i);
		compare_state(b, a, "generate");
	}
	rc |= diff_end();

	/*
	 * Long run: 3600 samples per reversal at the default period, so this
	 * crosses several.  Chunked unevenly, because the counter advances by
	 * count>>3 per call -- so the *chunking* changes when reversals land,
	 * and a reconstruction that accumulated differently would diverge here
	 * and nowhere else.
	 */
	diff_begin("FPM_TONE_generate reversals");
	memcpy(a, built, sizeof(a));
	memcpy(b, built, sizeof(b));
	ref_FPM_TONE_set_freq(a, 2100);
	FPM_TONE_set_freq(b, 2100);
	{
		int resets = 0, prev = 0;

		for (k = 0; k < 400; k++) {
			int n = 40 + (k % 7) * 24, i, now;

			ref_FPM_TONE_generate(a, oa, (short)n);
			FPM_TONE_generate(b, ob, (short)n);
			for (i = 0; i < n; i++)
				diff_eq_int("long sample %ld", ob[i], oa[i], i);
			compare_state(b, a, "generate long");

			now = *(unsigned short *)(b + FPM_TONE_OFF_REV_COUNT);
			if (now < prev)
				resets++;
			prev = now;
		}

		/*
		 * Guard against this group going vacuous.  It takes 3600
		 * samples per reversal at the default period, so if the run
		 * length or the period ever changes, the reversal branch could
		 * stop being reached and every check above would still pass
		 * while testing nothing.  Assert it actually fired.
		 */
		diff_eq_int("reversals actually fired (%ld)", resets > 0, 1, 0);
	}
	rc |= diff_end();

	/* Counts that are not multiples of 8: the counter drops the remainder. */
	diff_begin("FPM_TONE_generate ragged counts");
	memcpy(a, built, sizeof(a));
	memcpy(b, built, sizeof(b));
	ref_FPM_TONE_set_freq(a, 1650);
	FPM_TONE_set_freq(b, 1650);
	for (k = 0; k < 900; k++) {
		int n = (k % 8) + 1, i;

		ref_FPM_TONE_generate(a, oa, (short)n);
		FPM_TONE_generate(b, ob, (short)n);
		for (i = 0; i < n; i++)
			diff_eq_int("ragged sample %ld", ob[i], oa[i], i);
		compare_state(b, a, "generate ragged");
	}
	rc |= diff_end();

	return rc;
}
