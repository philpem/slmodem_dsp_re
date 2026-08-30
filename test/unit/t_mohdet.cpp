/*
 * t_mohdet.cpp -- differential test of the V.92 MOH retrain-request
 * detector, src/pump/v90/mohdet.cpp against the blob:
 *
 *   retrainDetector        0x5f80   390 bytes
 *   resetRetrainDetector   0x6110   118 bytes
 *   interpretMohTimeout    0x6190   102 bytes
 *
 * A `.cpp` because all three names are mangled; the `asm` aliases below are
 * the same arrangement t_v90equ.cpp uses for `V90Equalizer`'s methods.
 *
 * The detector's state is 32 self-contained bytes with no pointers, so the
 * two sides run their own copies and `diff_eq_obj` compares them whole
 * after every call.  The scenarios walk the decision surface the
 * disassembly shows: a tone in the notch (detects, eventually), the same
 * tone split across ragged call boundaries, a tone outside the notch, a
 * tone too quiet to pass the 150000 floor, noise, and the OTHER coefficient
 * set with the tones swapped.  One check demands that some scenario
 * actually returned 1 on both sides -- a detector test where nothing
 * detects is the vacuous run findings F134/F2400 warn about.
 */

#include <string.h>

#include "harness.h"
#include "dsplib/mohdet.h"

int ref_retrainDetector(tag_retrainReqDet *det, short *in, int nSamples)
	asm("ref__Z15retrainDetectorP17tag_retrainReqDetPsi");
void ref_resetRetrainDetector(tag_retrainReqDet *det, short which)
	asm("ref__Z20resetRetrainDetectorP17tag_retrainReqDets");
int ref_interpretMohTimeout(short code)
	asm("ref__Z19interpretMohTimeouts");

extern "C" {
extern unsigned int dsplibs_debug_level;
extern unsigned int ref_dsplibs_debug_level;
}

#define NSAMP 1024

static tag_retrainReqDet ours, ref;
static short sig[NSAMP];
static int some_detection;

/* Fs/8 sine, the 0x66 notch's own frequency. */
static void
fill_fs8(int amp)
{
	static const int q[8] = { 0, 707, 1000, 707, 0, -707, -1000, -707 };
	int i;

	for (i = 0; i < NSAMP; i++)
		sig[i] = (short)(amp * q[i & 7] / 1000);
}

/* Fs/4 sine, the 0x65 notch's. */
static void
fill_fs4(int amp)
{
	static const int q[4] = { 0, 1000, 0, -1000 };
	int i;

	for (i = 0; i < NSAMP; i++)
		sig[i] = (short)(amp * q[i & 3] / 1000);
}

static void
fill_noise(void)
{
	unsigned long seed = 0x1234567;
	int i;

	for (i = 0; i < NSAMP; i++) {
		seed = seed * 1103515245ul + 12345ul;
		sig[i] = (short)((seed >> 8) & 0x3fff) - 0x2000;
	}
}

/*
 * Reset both sides with `which`, feed the signal in `chunk`-sample calls,
 * and compare the verdict and the state at every step.
 */
static void
run(short which, int chunk, long tag)
{
	int off, r1, r2;

	memset(&ours, 0xaa, sizeof(ours));
	memset(&ref, 0xaa, sizeof(ref));
	resetRetrainDetector(&ours, which);
	ref_resetRetrainDetector(&ref, which);
	diff_eq_obj("after reset", struct tag_retrainReqDet,
		    &ours, &ref, tag);

	for (off = 0; off < NSAMP; off += chunk) {
		int n = chunk < NSAMP - off ? chunk : NSAMP - off;

		r1 = retrainDetector(&ours, sig + off, n);
		r2 = ref_retrainDetector(&ref, sig + off, n);
		diff_eq_int("retrainDetector verdict (call at %ld)",
			    r1, r2, tag * 10000 + off);
		diff_eq_obj("after retrainDetector",
			    struct tag_retrainReqDet, &ours, &ref,
			    tag * 10000 + off);
		if (r1 == 1 && r2 == 1) {
			some_detection = 1;
			break;
		}
	}
}

int
main(void)
{
	int code;

	diff_begin("V.92 MOH retrain-request detector");

	/* The reset's two coefficient sets, and what it leaves alone. */
	{
		static const short whichs[] = { 0x65, 0x66, 0, 0x64, 0x67,
						-1, 0x165 };
		unsigned i;

		for (i = 0; i < sizeof(whichs) / sizeof(whichs[0]); i++) {
			memset(&ours, 0xaa, sizeof(ours));
			memset(&ref, 0xaa, sizeof(ref));
			resetRetrainDetector(&ours, whichs[i]);
			ref_resetRetrainDetector(&ref, whichs[i]);
			diff_eq_obj("resetRetrainDetector",
				    struct tag_retrainReqDet, &ours, &ref, i);
		}
	}

	/* Zero samples: no state may move. */
	memset(&ours, 0x33, sizeof(ours));
	memset(&ref, 0x33, sizeof(ref));
	diff_eq_int("zero samples returns alike (%ld)",
		    retrainDetector(&ours, sig, 0),
		    ref_retrainDetector(&ref, sig, 0), 0);
	diff_eq_obj("zero samples leaves state", struct tag_retrainReqDet,
		    &ours, &ref, 0);

	/* The notch tone, whole and ragged; the wrong tone; the floors. */
	fill_fs8(5000);
	run(0x66, NSAMP, 1);	/* one call: should detect               */
	run(0x66, 37, 2);	/* ragged chunks across block boundaries */
	run(0x65, NSAMP, 3);	/* other notch: tone not removed         */
	fill_fs8(300);
	run(0x66, NSAMP, 4);	/* under the 150000 input-energy floor   */
	fill_fs4(5000);
	run(0x65, 64, 5);	/* the other selector's own tone         */
	run(0x66, NSAMP, 6);	/* ...seen by the wrong notch            */
	fill_noise();
	run(0x66, 128, 7);	/* noise: notch removes almost nothing   */

	/*
	 * A LOUD notch tone over a QUIET noise floor: the notch removes the
	 * tone, the residual noise still clears the 2250000 absolute ceiling
	 * (about 5.6M per block at +-4096) while the 4:1 ratio holds -- the
	 * one region that discriminates the ceiling from the ratio test.
	 * Noisier floors drift past ten times the ceiling and stop holding
	 * anything.  Built by summing, clipped to short.
	 */
	{
		unsigned long seed = 0xbeef;
		int i;

		fill_fs8(20000);
		for (i = 0; i < NSAMP; i++) {
			long v;

			seed = seed * 1103515245ul + 12345ul;
			v = (long)sig[i]
			    + (long)((seed >> 8) & 0x1fff) - 0x1000;
			if (v > 32767)
				v = 32767;
			if (v < -32768)
				v = -32768;
			sig[i] = (short)v;
		}
		run(0x66, NSAMP, 9);
	}

	/*
	 * TONE BURSTS WITH GAPS: three detected blocks, two silent ones,
	 * repeated.  A failed block must CLEAR the detection count -- if it
	 * merely paused, the bursts would sum to more than 5 and return 1.
	 * Chunked at one block so the count is compared at every boundary.
	 */
	{
		int i;

		fill_fs8(5000);
		for (i = 0; i < NSAMP; i++)
			if ((i >> 6) % 5 >= 3)
				sig[i] = 0;
		run(0x66, 64, 10);
	}

	diff_eq_int("some scenario detected on BOTH sides (%ld)",
		    some_detection, 1, 0);

	/*
	 * The detection debug line, compared as text.  Level 2 opens the
	 * gate the disassembly shows at 0x60e0 (`> 1`).
	 */
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;
	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();
	fill_fs8(5000);
	run(0x66, NSAMP, 8);
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;
	diff_eq_int("debug transcripts match (%ld)",
		    strcmp(dsplib_debug_capture_text(0),
			   dsplib_debug_capture_text(1)) == 0, 1, 0);
	diff_eq_int("the detection line actually printed (%ld lines)",
		    dsplib_debug_capture_lines(1) > 0, 1,
		    (long)dsplib_debug_capture_lines(1));
	diff_eq_int("both sides printed alike (%ld)",
		    (long)dsplib_debug_capture_lines(0),
		    (long)dsplib_debug_capture_lines(1), 0);

	/* The timeout ladder, over and past both ends of the table. */
	for (code = -40; code <= 40; code++)
		diff_eq_int("interpretMohTimeout(%ld)",
			    interpretMohTimeout((short)code),
			    ref_interpretMohTimeout((short)code), code);
	diff_eq_int("interpretMohTimeout(0x7fff) (%ld)",
		    interpretMohTimeout(0x7fff),
		    ref_interpretMohTimeout(0x7fff), 0x7fff);
	diff_eq_int("interpretMohTimeout(-0x8000) (%ld)",
		    interpretMohTimeout((short)-0x8000),
		    ref_interpretMohTimeout((short)-0x8000), -0x8000);
	/* Two absolute anchors, so both sides being wrong alike would show. */
	diff_eq_int("code 13 means no limit (%ld)",
		    interpretMohTimeout(13), -1, 13);
	diff_eq_int("code 12 is 960 seconds (%ld)",
		    interpretMohTimeout(12), 960, 12);

	return diff_end();
}
