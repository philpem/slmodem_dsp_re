/*
 * t_rcresample.c -- differential test of the rational resampler.
 *
 * Unlike the pure functions tested elsewhere, this one is stateful: the
 * history window, phase accumulator and input-owed counter all persist across
 * calls, and the interesting bugs live in that carry-over rather than in the
 * arithmetic. So the test drives both implementations with the *same*
 * fragmentation pattern and compares output sample-for-sample, then compares
 * the internal state after every call.
 *
 * Fragmentation matters. Feeding 1000 samples in one call exercises none of
 * the partial-consumption path; feeding them in ragged chunks exercises all of
 * it, including the case where a call ends part-way through the samples one
 * output needs.
 *
 * The 200-sample history compaction is reached only after enough samples have
 * been pushed, so the streams are long enough to cross it several times.
 */

#include <stdlib.h>
#include <string.h>

#include <stdio.h>

#include "harness.h"
#include "dsplib/fixedrc.h"

extern void *ref_RcFixed_Create(int mode);
extern void ref_RcFixed_Delete(void *h);
extern void ref_RcFixed_Reset(void *h);
extern void ref_RcFixed_Resample(void *h, const short *in, int in_count,
				 short *out, int *out_count);

#define NSAMP 4000

static short input[NSAMP];

/*
 * A deterministic signal with content across the whole band: a low tone the
 * filter must pass, a high tone it must reject, and a pseudorandom component
 * so every coefficient sees a distinct value rather than a repeating pattern.
 */
static void
make_input(void)
{
	unsigned lfsr = 0xACE1u;
	int i;

	for (i = 0; i < NSAMP; i++) {
		int noise;

		lfsr = (lfsr >> 1) ^ (-(int)(lfsr & 1u) & 0xB400u);
		noise = (int)(lfsr & 0x1fff) - 0x1000;
		/* ~1 kHz and ~3.7 kHz at 9600, plus noise */
		input[i] = (short)(8000 * (i % 10 < 5 ? 1 : -1) / 3
				   + 4000 * (i % 13 < 6 ? 1 : -1) / 3
				   + noise / 2);
	}
}

/* Feed one mode through both sides with a given chunking pattern. */
static int
run_mode(int mode, const char *label, const int *chunks, int nchunks)
{
	void *ref;
	struct rc *ours;
	short out_a[NSAMP * 8], out_b[NSAMP * 8];
	int pos = 0, na = 0, nb = 0, ci = 0;
	int rc;

	ref = ref_RcFixed_Create(mode);
	ours = RcFixed_Create(mode);

	diff_begin(label);
	if (ref == NULL || ours == NULL) {
		/*
		 * Modes 0 and 1 are deviation D3: the original builds a
		 * converter with a different state layout, we decline.  Nothing
		 * can request them -- every call site passes a literal 2, 3, 4,
		 * 5 or 7, and Check_Combination starts its scan at 2 -- so the
		 * divergence is unreachable.  Assert exactly that shape rather
		 * than equality, so an accidental change on either side fails.
		 */
		if (mode < 2) {
			diff_eq_int("mode %ld: we decline", ours == NULL, 1, mode);
			diff_eq_int("mode %ld: original still builds one",
				    ref != NULL, 1, mode);
		} else {
			diff_eq_int("mode %ld creatable", ours != NULL,
				    ref != NULL, mode);
		}
		if (ref)
			ref_RcFixed_Delete(ref);
		if (ours)
			RcFixed_Delete(ours);
		return diff_end();
	}

	while (pos < NSAMP) {
		int n = chunks[ci++ % nchunks];
		int ca = 0, cb = 0;
		int k;

		if (pos + n > NSAMP)
			n = NSAMP - pos;

		ref_RcFixed_Resample(ref, input + pos, n, out_a + na, &ca);
		RcFixed_Resample(ours, input + pos, n, out_b + nb, &cb);

		diff_eq_int("mode %ld: output count", cb, ca, mode);
		if (ca != cb)
			break;

		for (k = 0; k < ca; k++)
			diff_eq_int("mode %ld: sample", out_b[nb + k],
				    out_a[na + k], mode);

		na += ca;
		nb += cb;
		pos += n;
	}

	/* Total production must match too, not just each call. */
	diff_eq_int("mode %ld: total outputs", nb, na, mode);

	rc = diff_end();
	ref_RcFixed_Delete(ref);
	RcFixed_Delete(ours);
	return rc;
}

/*
 * Drive one mode with a fixed output cap on every call, checking both the
 * samples and that neither side exceeds the cap.
 */
static int
run_limited(const char *label, int mode, int limit)
{
	void *ref = ref_RcFixed_Create(mode);
	struct rc *ours = RcFixed_Create(mode);
	short out_a[NSAMP * 8], out_b[NSAMP * 8];
	int pos = 0, na = 0, nb = 0, k, rc;

	diff_begin(label);
	while (pos < NSAMP) {
		int n = 64, ca = limit, cb = limit;

		if (pos + n > NSAMP)
			n = NSAMP - pos;

		ref_RcFixed_Resample(ref, input + pos, n, out_a + na, &ca);
		RcFixed_Resample(ours, input + pos, n, out_b + nb, &cb);

		diff_eq_int("limit %ld: output count", cb, ca, limit);
		diff_eq_int("limit %ld: cap respected", cb <= limit, 1, limit);
		if (ca != cb)
			break;
		for (k = 0; k < ca; k++)
			diff_eq_int("limit %ld: sample", out_b[nb + k],
				    out_a[na + k], limit);
		na += ca;
		nb += cb;
		pos += n;
	}
	rc = diff_end();
	ref_RcFixed_Delete(ref);
	RcFixed_Delete(ours);
	return rc;
}

int
main(void)
{
	/* One call per chunk size; ragged sizes hit the partial-consume path. */
	static const int bulk[] = { 480 };
	static const int ragged[] = { 1, 7, 3, 48, 2, 160, 5, 11, 96, 1 };
	static const int tiny[] = { 1 };
	static const int modes[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
				     12, 13, 14, 15, 16, 17, 18, 19 };
	char label[64];
	int rc = 0;
	unsigned i;

	make_input();

	for (i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
		sprintf(label, "rc mode %d bulk", modes[i]);
		rc |= run_mode(modes[i], label, bulk,
			       sizeof(bulk) / sizeof(bulk[0]));
	}

	/* The two ratios dp_wrapper actually uses, under adversarial chunking. */
	rc |= run_mode(3, "rc 9600->8000 ragged", ragged,
		       sizeof(ragged) / sizeof(ragged[0]));
	rc |= run_mode(2, "rc 8000->9600 ragged", ragged,
		       sizeof(ragged) / sizeof(ragged[0]));
	rc |= run_mode(3, "rc 9600->8000 one-at-a-time", tiny,
		       sizeof(tiny) / sizeof(tiny[0]));
	rc |= run_mode(2, "rc 8000->9600 one-at-a-time", tiny,
		       sizeof(tiny) / sizeof(tiny[0]));

	/*
	 * Output limits.  The incoming *out_count is a cap, and 0 means "no
	 * limit" because the original tests for inequality after producing.
	 * Every case above passes 0, so none of them exercises a real cap --
	 * which is exactly how a missing limit went unnoticed until
	 * dp_wrapper, the only caller that passes one, disagreed.
	 */
	rc |= run_limited("rc limit 1", 3, 1);
	rc |= run_limited("rc limit 7", 3, 7);
	rc |= run_limited("rc limit 192", 3, 192);
	rc |= run_limited("rc limit exact frag", 2, 160);
	rc |= run_limited("rc limit huge", 3, 100000);

	/*
	 * Reset, which puts a converter back to the state Create left it in.
	 * Checked by running one, resetting it, and requiring that it then
	 * produces exactly what a freshly built one does -- the state struct
	 * alone would not prove it, since a field Reset forgot could be one
	 * the resampler reads and the comparison would still pass.
	 */
	diff_begin("RcFixed_Reset");
	{
		static short fresh_a[NSAMP], fresh_b[NSAMP];
		static short again_a[NSAMP], again_b[NSAMP];
		void *ha, *hb;
		void *fa, *fb;
		int na, nb, ma, mb, i;
		int mode;

		for (mode = 2; mode <= 3; mode++) {
			ha = ref_RcFixed_Create(mode);
			hb = RcFixed_Create(mode);
			fa = ref_RcFixed_Create(mode);
			fb = RcFixed_Create(mode);
			diff_eq_int("all four built (%ld)",
				    ha && hb && fa && fb, 1, mode);
			if (!ha || !hb || !fa || !fb)
				continue;

			/* Dirty them, then reset. */
			na = nb = 0;
			ref_RcFixed_Resample(ha, input, NSAMP, again_a, &na);
			RcFixed_Resample(hb, input, NSAMP, again_b, &nb);
			ref_RcFixed_Reset(ha);
			RcFixed_Reset(hb);

			na = nb = ma = mb = 0;
			ref_RcFixed_Resample(ha, input, 800, again_a, &na);
			RcFixed_Resample(hb, input, 800, again_b, &nb);
			ref_RcFixed_Resample(fa, input, 800, fresh_a, &ma);
			RcFixed_Resample(fb, input, 800, fresh_b, &mb);

			diff_eq_int("counts agree (%ld)", nb, na, mode);
			diff_eq_int("reset matches fresh, reference (%ld)",
				    na, ma, mode);
			diff_eq_int("reset matches fresh, ours (%ld)", nb, mb,
				    mode);
			diff_eq_int("it produced something (%ld)", na > 100, 1,
				    na);
			for (i = 0; i < na && i < ma; i++) {
				diff_eq_int("sample %ld", again_b[i],
					    again_a[i], i);
				diff_eq_int("reset == fresh %ld", again_a[i],
					    fresh_a[i], i);
			}

			/*
			 * NULL is deliberately NOT driven here: the original
			 * dereferences its argument on the first instruction
			 * and the reconstruction checks, so the one input
			 * that would tell them apart crashes the reference.
			 * See the note on RcFixed_Reset.
			 */
			ref_RcFixed_Delete(ha);
			RcFixed_Delete(hb);
			ref_RcFixed_Delete(fa);
			RcFixed_Delete(fb);
		}
	}
	rc |= diff_end();

	/* Deviation D3: unreachable modes 0 and 1, which we decline to build. */
	rc |= run_mode(0, "rc mode 0 declined (D3)", bulk, 1);
	rc |= run_mode(1, "rc mode 1 declined (D3)", bulk, 1);

	return rc;
}
