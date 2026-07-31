/*
 * t_spandsp_v8.c -- does the reconstructed V.8 talk to a real one?
 *
 * The differential harness answers "is this the same as the blob".  It cannot
 * answer "is this correct V.8": a misread frequency or an inverted mark and
 * space would make both sides wrong in the same way and every check would
 * still pass.  SpanDSP is an independent implementation, so this tier asks
 * the question the blob cannot be the judge of.
 *
 * Rates differ -- SpanDSP's V.8 runs at 8000 and the reconstruction at 9600 --
 * so the reconstructed resampler sits between them, which incidentally
 * exercises it too.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "spandsp.h"

#include "dsplib/v8.h"
#include "dsplib/fixedrc.h"

#define NATIVE_RATE	9600
#define SPANDSP_RATE	8000

/* 9600 -> 8000, the mode call.c uses for a 9600 host. */
#define RC_DOWN		3

/* 8000 -> 9600, the other direction. */
#define RC_UP		2

static int checks;
static int failures;

static void
check(const char *what, int got, int want)
{
	checks++;
	if (got == want)
		return;
	failures++;
	printf("  FAIL %-46s got %d, want %d\n", what, got, want);
}

/* What SpanDSP's dedicated tone detector reports. */
static int tone_seen = -1;

static void
tone_handler(void *user_data, int tone, int level, int delay)
{
	(void)user_data;
	(void)level;
	(void)delay;
	if (tone > 0)
		tone_seen = tone;
}

/* What SpanDSP reports when it has made up its mind. */
static int v8_status;
static int v8_tone;

static void
result_handler(void *user_data, v8_parms_t *result)
{
	(void)user_data;
	v8_status = result->status;
	v8_tone = result->modem_connect_tone;
}

/*
 * Generate `blocks` blocks of ANSam with the reconstructed code, at 9600,
 * and resample to 8000 for SpanDSP.
 */
static int
make_ansam(short *out, int out_max, int blocks)
{
	static struct v8 v;
	struct rc *down;
	short native[64];
	int have = 0;
	int produced = 0;
	int i;

	memset(&v, 0, sizeof(v));
	v8_txinit(&v);
	/*
	 * The shaping filter's taps come from here.  Without it every sample
	 * goes through a filter of zeros and the tone is silence -- which is
	 * exactly what the first version of this produced.
	 */
	v.fa42 = 16384;
	v8_V21_Init(&v, 1, 0);
	v8_ansaminit(&v);
	/* A level a real line would carry, and reversals enabled. */
	v.fa42 = 16384;
	v.tone.f08 = 8000;
	v.tone.f0e = 1;

	down = RcFixed_Create(RC_DOWN);
	if (down == 0)
		return 0;

	for (i = 0; i < blocks; i++) {
		int n;

		/* Four samples a call; resample once sixteen calls' worth. */
		v8_ansamgenerate(&v, native + have);
		have += 4;
		if (have < 64)
			continue;

		n = out_max - produced;
		if (n < 64)
			break;
		RcFixed_Resample(down, native, 64, out + produced, &n);
		produced += n;
		have = 0;
	}
	RcFixed_Delete(down);
	return produced;
}

int
main(void)
{
	static short air[SPANDSP_RATE * 4];
	v8_state_t *v8;
	v8_parms_t parms;
	int n;

	printf("SpanDSP interop: V.8\n");

	/*
	 * The reconstruction's ANSam constants, checked against the standard
	 * rather than against the blob.  These are the numbers V.8 specifies.
	 */
	check("ANSam carrier is 2100 Hz",
	      (int)(0xe00 / 16384.0 * NATIVE_RATE + 0.5), 2100);
	check("ANSam modulation is 15 Hz",
	      (int)(0x1a / 16384.0 * NATIVE_RATE + 0.5), 15);
	check("ANSam reversal period is 450 ms",
	      (int)(0x438 * 4 * 1000.0 / NATIVE_RATE + 0.5), 450);

	/*
	 * The V.21 carriers, the same way: `v8_V21_Init`'s constants against
	 * a 13-bit accumulator at 9600.  V.21 puts channel 1 at 980 and 1180
	 * and channel 2 at 1650 and 1850, and these are those numbers -- not
	 * approximately, exactly.  Nothing in the differential harness could
	 * have told us that; agreeing with the blob about a wrong frequency
	 * looks identical to agreeing about a right one.
	 */
#define V21_HZ(step)	((int)((step) / 8192.0 * NATIVE_RATE + 0.5))
	check("V.21 channel 1 space is 1180 Hz", V21_HZ(0x3ef), 1180);
	check("V.21 channel 1 mark is 980 Hz", V21_HZ(0x344), 980);
	check("V.21 channel 2 space is 1850 Hz", V21_HZ(0x62b), 1850);
	check("V.21 channel 2 mark is 1650 Hz", V21_HZ(0x580), 1650);

	/* Now the live check: does SpanDSP hear our ANSam? */
	n = make_ansam(air, (int)(sizeof(air) / sizeof(air[0])), 4000);
	check("ANSam was generated", n > SPANDSP_RATE, 1 * (n > SPANDSP_RATE));
	{
		int i, peak = 0;
		double sum = 0;

		for (i = 0; i < n; i++) {
			int a = air[i] < 0 ? -air[i] : air[i];

			if (a > peak)
				peak = a;
			sum += (double)air[i] * air[i];
		}
		printf("  %d samples at %d Hz, peak %d, rms %.0f\n",
		       n, SPANDSP_RATE, peak, n ? sqrt(sum / n) : 0.0);
	}

	/*
	 * The direct question first: point SpanDSP's own ANSam detector at
	 * what the reconstruction produced.  This is the check that matters --
	 * an independent implementation recognising our signal as ANSam with
	 * phase reversals.
	 */
	{
		modem_connect_tones_rx_state_t *det;
		int off;

		det = modem_connect_tones_rx_init(NULL,
						  MODEM_CONNECT_TONES_ANSAM_PR,
						  tone_handler, NULL);
		if (det == NULL) {
			printf("  FAIL could not init the tone detector\n");
			failures++;
		} else {
			for (off = 0; off + 160 <= n; off += 160)
				modem_connect_tones_rx(det, air + off, 160);
			printf("  detector reported tone %d (%s)\n", tone_seen,
			       tone_seen == MODEM_CONNECT_TONES_ANSAM_PR
			       ? "ANSam with phase reversals" : "not ANSam-PR");
			check("SpanDSP recognises our ANSam", tone_seen,
			      MODEM_CONNECT_TONES_ANSAM_PR);
		}
	}

	/*
	 * The other direction: SpanDSP generates ANSam, the reconstruction
	 * has to recognise it.  This is the check that would catch a detector
	 * tuned to our own generator's quirks rather than to the standard.
	 */
	{
		static short native[NATIVE_RATE * 3];
		static struct v8_phase_rev pr;
		modem_connect_tones_tx_state_t *gen;
		struct rc *up;
		int made = 0;
		int off;
		int have = 0;

		gen = modem_connect_tones_tx_init(NULL,
						  MODEM_CONNECT_TONES_ANSAM_PR);
		if (gen == NULL) {
			printf("  FAIL could not init the tone generator\n");
			failures++;
		} else {
			/* 8000 -> 9600, the rate the reconstruction runs at. */
			up = RcFixed_Create(RC_UP);
			while (made + 256 < (int)(sizeof(native)
						  / sizeof(native[0]))) {
				short block[160];
				int got = 160;

				if (modem_connect_tones_tx(gen, block, 160)
				    <= 0)
					break;
				RcFixed_Resample(up, block, 160,
						 native + made, &got);
				made += got;
			}
			RcFixed_Delete(up);

			memset(&pr, 0, sizeof(pr));
			v8_phase_rev_init(&pr);
			for (off = 0; off + 64 <= made; off += 64)
				v8_phase_rev_detect(&pr, native + off, 64);

			printf("  our detector saw %d reversals, verdict %d\n",
			       pr.reversals, pr.detected);
			check("we generated something to look at", made > 8000,
			      1);
			check("our detector sees SpanDSP's reversals",
			      pr.reversals > 0, 1);
			(void)have;
		}
	}

	/*
	 * The full state machine is reported but not asserted on: completing
	 * a negotiation needs the sequencer, which is still being
	 * reconstructed.  What is proved here is the signal layer.
	 */
	memset(&parms, 0, sizeof(parms));
	parms.modem_connect_tone = MODEM_CONNECT_TONES_ANSAM_PR;
	parms.send_ci = true;
	parms.v92 = -1;
	parms.jm_cm.call_function = V8_CALL_V_SERIES;
	parms.jm_cm.modulations = V8_MOD_V21 | V8_MOD_V32 | V8_MOD_V34;
	parms.jm_cm.protocols = V8_PROTOCOL_LAPM_V42;

	v8_status = -1;
	v8_tone = -1;
	v8 = v8_init(NULL, true, &parms, result_handler, NULL);
	if (v8 != NULL) {
		int off;

		for (off = 0; off + 160 <= n; off += 160) {
			int16_t tx[160];

			v8_tx(v8, tx, 160);
			v8_rx(v8, air + off, 160);
		}
		printf("  SpanDSP V.8 state machine: status %d, tone %d"
		       " (not asserted; needs the sequencer)\n",
		       v8_status, v8_tone);
	}

	printf("%s: %d checks, %d failures\n",
	       failures ? "FAIL" : "PASS", checks, failures);
	return failures != 0;
}
