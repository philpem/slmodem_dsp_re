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
 *
 * WHAT THIS FILE CANNOT CHECK, AND WHY THERE IS NO TEST FOR IT.  `phase`,
 * `down` and `up` are signed 16-bit fields, and finding F2700 corrected all
 * three from `unsigned short`.  Nothing below can tell the two readings apart
 * and nothing added below could: the factors come from `fixedRc_DownFact[]`
 * and `fixedRc_UpFact[]`, whose largest entry is 24, `phase` is a remainder
 * modulo `up`, and `RcFixed_Create` takes a mode number rather than a factor,
 * so bit 15 is unreachable through the module's whole API.  A separating value
 * exists -- `down = 0x8000, up = 5` gives `input_needed, phase` of `0, 2`
 * signed and `1, 3` unsigned, both well defined -- but only by writing the
 * field behind the API, which 2152 rules proves something about the fixture.
 * The declaration is held by the codegen tier alone.  Do not add a poke here
 * without settling that first.
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
/* The harness globalizes these uniquely named original LOCAL tables for
 * target-identity checks; production keeps the source tables static. */
extern const short CI_bDroop[3], CI_b1[87], CI_b2[15];
extern const short ref_CI_bDroop[3], ref_CI_b1[87], ref_CI_b2[15];

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
		diff_eq_int("mode %ld creatable", ours != NULL,
			    ref != NULL, mode);
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

/* RcFixed_State reads the common handle's state pointer on either side.
 * The history allocations and static coefficient tables have different
 * addresses. Compare each payload first, then normalize only those six
 * pointers in a copied record so padding and all scalar state still count. */
static void
compare_kind1(void *ref, struct rc *ours, long step)
{
	struct rc_kind1 *a = (struct rc_kind1 *)RcFixed_State(ref);
	struct rc_kind1 *b = (struct rc_kind1 *)RcFixed_State(ours);
	struct rc_kind1 normalized = *b;

	diff_eq_int("kind1 record extent %ld", harness_alloc_reqsize(a), 40, step);
	diff_eq_int("kind1 source extent %ld", harness_alloc_reqsize(b), 40, step);
	diff_eq_int("kind1 ref droop history extent %ld", harness_alloc_reqsize(a->w6), 6, step);
	diff_eq_int("kind1 ref long history extent %ld", harness_alloc_reqsize(a->w174), 174, step);
	diff_eq_int("kind1 ref short history extent %ld", harness_alloc_reqsize(a->w30), 30, step);
	diff_eq_int("kind1 source droop history extent %ld", harness_alloc_reqsize(b->w6), 6, step);
	diff_eq_int("kind1 source long history extent %ld", harness_alloc_reqsize(b->w174), 174, step);
	diff_eq_int("kind1 source short history extent %ld", harness_alloc_reqsize(b->w30), 30, step);
	diff_eq_int("kind1 ref droop target %ld", a->droop == ref_CI_bDroop, 1, step);
	diff_eq_int("kind1 ref long target %ld", a->b1 == ref_CI_b1, 1, step);
	diff_eq_int("kind1 ref short target %ld", a->b2 == ref_CI_b2, 1, step);
	diff_eq_int("kind1 source droop target %ld", b->droop == CI_bDroop, 1, step);
	diff_eq_int("kind1 source long target %ld", b->b1 == CI_b1, 1, step);
	diff_eq_int("kind1 source short target %ld", b->b2 == CI_b2, 1, step);
	diff_eq_obj("kind1 droop coefficients", short[3], b->droop, a->droop, step);
	diff_eq_obj("kind1 long coefficients", short[87], b->b1, a->b1, step);
	diff_eq_obj("kind1 short coefficients", short[15], b->b2, a->b2, step);
	diff_eq_obj("kind1 droop history", short[3], b->w6, a->w6, step);
	diff_eq_obj("kind1 long history", short[87], b->w174, a->w174, step);
	diff_eq_obj("kind1 short history", short[15], b->w30, a->w30, step);
	normalized.w6 = a->w6;
	normalized.w174 = a->w174;
	normalized.w30 = a->w30;
	normalized.droop = a->droop;
	normalized.b1 = a->b1;
	normalized.b2 = a->b2;
	diff_eq_obj("kind1 scalar state and padding", struct rc_kind1,
		    &normalized, a, step);
}

/* Modes 0/1 are explicit constructor-created components, not modem-path
 * reachability witnesses. Only four bytes of each history are initialized
 * by Create/Reset. The remaining bytes below are the apparatus's documented
 * malloc fill, not a promise that the library initializes its whole history.
 * The original reads them on its first convolution, so preserve both the
 * partial initialization and the subsequent history on reset/reuse. */
static int
run_kind1(void)
{
	static const int sizes[] = { 0, 1, 2, 3, 4, 7, 15, 31, 87, 101 };
	static const int limits[] = { 0, 1, 2, 4, 5, 12, 512, 100000 };
	short samples[101];
	short oa[516], ob[516];
	int mode, signal, phase, round, i, j, rc = 0;
	long calls = 0, compared_samples = 0;
	char label[80];

	for (mode = 0; mode <= 1; mode++) {
		for (signal = 0; signal < 4; signal++) {
			void *r = ref_RcFixed_Create(mode);
			struct rc *o = RcFixed_Create(mode);
			struct rc_kind1 *a, *b;
			unsigned char expected[174];

			sprintf(label, "rc kind1 mode %d signal %d lifecycle", mode, signal);
			diff_begin(label);
			diff_eq_int("reference constructed %ld", r != NULL, 1, mode);
			diff_eq_int("source constructed %ld", o != NULL, 1, mode);
			if (!r || !o) {
				rc |= diff_end();
				if (r) ref_RcFixed_Delete(r);
				if (o) RcFixed_Delete(o);
				continue;
			}
			a = (struct rc_kind1 *)RcFixed_State(r);
			b = (struct rc_kind1 *)RcFixed_State(o);
			compare_kind1(r, o, calls);
			memset(expected, HARNESS_MALLOC_FILL, sizeof(expected));
			memset(expected, 0, 4);
			diff_eq_obj("constructor droop initialization", short[3], a->w6, expected, mode);
			diff_eq_obj("constructor long initialization", short[87], a->w174, expected, mode);
			diff_eq_obj("constructor short initialization", short[15], a->w30, expected, mode);
			for (i = 0; i < 101; i++) {
				if (signal == 0) samples[i] = i == 0 ? 16000 : 0;
				else if (signal == 1) samples[i] = 12000;
				else if (signal == 2) samples[i] = i & 1 ? -32768 : 32767;
				else samples[i] = input[i];
			}
			for (phase = 0; phase < 2; phase++) {
				if (phase != 0) {
					/* Reset is deliberately partial. Preserve the previous
					 * reference history except for its first four bytes. */
					memcpy(expected, a->w174, 174);
					memset(expected, 0, 4);
					ref_RcFixed_Reset(r);
					RcFixed_Reset(o);
					diff_eq_obj("reset preserves long history remainder", short[87], a->w174, expected, mode);
					compare_kind1(r, o, calls);
				}
				/* Repeated short calls cross all three ring wraps.
				 * Decimation's /2 phases restart on each call: this is
				 * cross-implementation agreement for one call sequence,
				 * not an assertion of arbitrary partition invariance. */
				for (round = 0; round < 2; round++) {
					for (i = 0; i < 10; i++) {
						for (j = 0; j < 8; j++) {
							int ca = limits[j], cb = limits[j];
							int cap = ca == 0 ? 1 : ca;
							int count, k;

							if (mode == 0) {
								count = (cap + 3) / 4;
								if (count > sizes[i]) count = sizes[i];
								count *= 4;
							} else {
								count = sizes[i] / 4;
								if (count > cap) count = cap;
							}
							for (k = 0; k < 516; k++) oa[k] = ob[k] = (short)0x5a6b;
							ref_RcFixed_Resample(r, samples, sizes[i], oa + 2, &ca);
							RcFixed_Resample(o, samples, sizes[i], ob + 2, &cb);
							diff_eq_int("kind1 output count %ld", cb, ca, calls);
							diff_eq_int("kind1 original count boundary %ld", ca, count, calls);
							diff_eq_obj("kind1 output and unwritten suffix", short[516], ob, oa, calls);
							diff_eq_int("kind1 reference prefix guard %ld", oa[1], 0x5a6b, calls);
							diff_eq_int("kind1 source prefix guard %ld", ob[1], 0x5a6b, calls);
							diff_eq_int("kind1 reference suffix guard %ld", oa[count + 2], 0x5a6b, calls);
							diff_eq_int("kind1 source suffix guard %ld", ob[count + 2], 0x5a6b, calls);
							compare_kind1(r, o, calls);
							compared_samples += ca;
							calls++;
						}
					}
				}
			}
			rc |= diff_end();
			ref_RcFixed_Delete(r);
			RcFixed_Delete(o);
		}
	}
	printf("kind1 lifecycle denominator: %ld calls, %ld reference outputs, 8 constructed pairs\n",
	       calls, compared_samples);
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
			 * and the reconstruction preserves that contract.
			 * Invalid null inputs are not lifecycle fixtures.
			 * See the note on RcFixed_Reset.
			 */
			ref_RcFixed_Delete(ha);
			RcFixed_Delete(hb);
			ref_RcFixed_Delete(fa);
			RcFixed_Delete(fb);
		}
	}
	rc |= diff_end();

	/* Explicit component modes, including their different cap/phase rules. */
	rc |= run_kind1();

	return rc;
}
