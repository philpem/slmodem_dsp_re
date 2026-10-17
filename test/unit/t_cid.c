/*
 * t_cid.c -- differential test of the Caller ID wrapper, src/service/cid.c
 * against the blob:
 *
 *   CID_create    0x340   195 bytes
 *   CID_delete    0x410    84 bytes
 *   CID_process   0x470   386 bytes
 *
 * The wrapper and receiver are reconstructed; both sides run their own
 * complete CID chain. This fixture covers the rate gate, allocation,
 * 192-sample chunking, verdict mapping and four-string TTY delivery.
 *
 * To reach the success path the test synthesises a Bell 202 caller ID burst
 * -- channel seizure, mark, then an SDMF message with a valid checksum --
 * and one check DEMANDS that both sides returned 1 for it and put bytes on
 * the TTY: a CID test where no CID is ever delivered is the vacuous run
 * findings F134/F2400 warn about.  A corrupted burst (checksum broken)
 * exercises the failure verdict the same way.
 *
 * The TTY transcripts are captured per side (harness_tty_*) and compared as
 * byte strings AND as call patterns -- CID_process sends each string and
 * its CRLF separately, and a wrapper that coalesced them would hand equal
 * bytes over a different call count.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "harness.h"
#include "dsplib/cid.h"
#include "dsplib/cid_modem.h"
#include "dsplib/debug.h"

extern void *ref_CID_create(void *m, unsigned rate, unsigned cid_val);
extern void ref_CID_delete(void *cid);
extern int ref_CID_process(void *cid, void *in, int count);
extern unsigned int ref_dsplibs_debug_level;

/* ------------------------------------------------------------------ */
/* A Bell 202 caller-ID burst: 1200 baud FSK, mark 1200 Hz, space     */
/* 2200 Hz, at the 8000 Hz line rate.                                 */
/* ------------------------------------------------------------------ */

#define MAXBITS		4096
#define MAXSAMP		40000

static unsigned char bits[MAXBITS];
static int nbits;
static short burst[MAXSAMP];
static int nsamp;

static void
put_bit(int b)
{
	if (nbits < MAXBITS)
		bits[nbits++] = (unsigned char)(b != 0);
}

/* Async framing: start 0, eight data bits LSB first, stop 1. */
static void
put_byte(unsigned v)
{
	int i;

	put_bit(0);
	for (i = 0; i < 8; i++)
		put_bit((int)(v >> i) & 1);
	put_bit(1);
}

static const short sintab[64] = {
	     0,    784,   1560,   2320,   3056,   3760,   4426,   5045,
	  5611,   6120,   6565,   6942,   7246,   7475,   7626,   7697,
	  7688,   7597,   7427,   7180,   6857,   6463,   6002,   5479,
	  4899,   4269,   3596,   2887,   2149,   1391,    620,   -156,
	  -929,  -1691,  -2435,  -3153,  -3837,  -4481,  -5077,  -5619,
	 -6102,  -6520,  -6868,  -7143,  -7341,  -7460,  -7498,  -7455,
	 -7331,  -7127,  -6845,  -6488,  -6059,  -5562,  -5003,  -4387,
	 -3720,  -3009,  -2260,  -1481,   -680,    135,    955,   1772
};

/*
 * Render the bit string to samples.  The bit clock is exact -- bit index
 * = i*1200/8000 = 3i/20 -- and the carrier runs on a 26.6 fixed-point
 * accumulator through the table above, phase-continuous across the mark/
 * space switch the way a real transmitter is.
 */
static void
render(void)
{
	/* freq * 64 slots * 2^26 / 8000, for 1200 and 2200 Hz. */
	const unsigned int step_mark = 644245094ul;
	const unsigned int step_space = 1181116006ul;
	unsigned int phase = 0;
	long i, total = (long)nbits * 20 / 3;

	nsamp = 0;
	for (i = 0; i < total && nsamp < MAXSAMP; i++) {
		long bi = i * 3 / 20;

		phase += bits[bi] ? step_mark : step_space;	/* wraps at 2^32 */
		burst[nsamp++] = sintab[(phase >> 26) & 63];
	}
}

/* An SDMF message: 0x04, length, MMDDHHMM + number, checksum. */
static void
build_burst(int wreck_checksum)
{
	static const char body[] = "08300945" "5551234";
	unsigned sum;
	int i;

	nbits = 0;

	/* Channel seizure: 300 alternating bits, then 180 of mark. */
	for (i = 0; i < 300; i++)
		put_bit(i & 1);
	for (i = 0; i < 180; i++)
		put_bit(1);

	sum = 0x04 + (unsigned)(sizeof(body) - 1);
	put_byte(0x04);
	put_byte(sizeof(body) - 1);
	for (i = 0; body[i]; i++) {
		put_byte((unsigned char)body[i]);
		sum += (unsigned char)body[i];
	}
	put_byte((0x100 - (sum & 0xff) + (wreck_checksum ? 0x55 : 0))
		 & 0xff);

	/* A tail of mark so the last stop bit is not the last sample. */
	for (i = 0; i < 60; i++)
		put_bit(1);

	render();
}

/* ------------------------------------------------------------------ */

static long
count_substr(const char *hay, const char *needle)
{
	long n = 0;
	size_t len = strlen(needle);

	while ((hay = strstr(hay, needle)) != 0) {
		n++;
		hay += len;
	}
	return n;
}

static int dummy_modem_ours, dummy_modem_ref;

/* Read-only host observations during the real Bell-202 delivery. CID's
 * private wrapper is two pointers (modem, receiver); memcpy accesses its
 * receiver slot without defining a second copy of that type. */
#define HOST_OBSERVATIONS 32
struct cid_host_observation {
	int modem_matches;
	int length;
	int string_offset;
	int crlf;
	int mode, cid_val, samples_fill;
	char rendered[0x258];
	unsigned char payload[0x258];
};
static struct cid_host_observation host_observations[2][HOST_OBSERVATIONS];
static void *active_cid[2];
static int host_calls[2], host_overflow;
static long observed_host_calls;
static int observation_negative;

extern int __real_modem_send_to_tty(void *, const void *, int);
extern int __real_ref_modem_send_to_tty(void *, const void *, int);

static void observe_host(int side, void *modem, const void *buf, int n)
{
	struct cid_modem *ctx;
	struct cid_host_observation *o;
	uintptr_t address, first;
	if (!active_cid[side])
		return;
	if (host_calls[side] >= HOST_OBSERVATIONS || n < 0 || n > 0x258) {
		host_overflow = 1;
		return;
	}
	memcpy(&ctx, (const char *)active_cid[side] + sizeof(void *), sizeof ctx);
	o = &host_observations[side][host_calls[side]++];
	o->modem_matches = modem == (side ? (void *)&dummy_modem_ref
					 : (void *)&dummy_modem_ours);
	o->length = n;
	address = (uintptr_t)buf;
	first = (uintptr_t)ctx->strings;
	o->string_offset = address >= first && address < first + sizeof ctx->strings
		? (int)(address - first) : -1;
	o->crlf = n == 2 && memcmp(buf, "\r\n", 2) == 0;
	o->mode = ctx->mode;
	o->cid_val = ctx->cid_val;
	o->samples_fill = ctx->samples_fill;
	memcpy(o->rendered, ctx->strings, sizeof o->rendered);
	memcpy(o->payload, buf, (unsigned)n);
}

int __wrap_modem_send_to_tty(void *m, const void *buf, int n)
{
	observe_host(0, m, buf, n);
	return __real_modem_send_to_tty(m, buf, n);
}
int __wrap_ref_modem_send_to_tty(void *m, const void *buf, int n)
{
	observe_host(1, m, buf, n);
	return __real_ref_modem_send_to_tty(m, buf, n);
}

/*
 * Feed one buffer to both sides in `chunk`-sample slices, stopping when a
 * side settles, and compare every verdict.  Returns the settled verdict
 * (they are compared equal before this returns).
 */
static int
feed(void *ours, void *ref, short *samples, int count, int chunk, long tag)
{
	int off = 0, r1 = 0, r2 = 0, i;

	memset(host_observations, 0, sizeof host_observations);
	host_calls[0] = host_calls[1] = host_overflow = 0;
	active_cid[0] = ours;
	active_cid[1] = ref;

	while (off < count) {
		int n = chunk < count - off ? chunk : count - off;

		r1 = CID_process(ours, samples + off, n);
		r2 = ref_CID_process(ref, samples + off, n);
		diff_eq_int("CID_process verdict (offset %ld)", r1, r2,
			    tag * 100000 + off);
		if (r1 != 0 || r2 != 0)
			break;
		off += n;
	}
	active_cid[0] = active_cid[1] = 0;
	diff_eq_int("host callback observations fit (%ld)", host_overflow, 0, tag);
	diff_eq_int("host callback observation counts (%ld)", host_calls[0], host_calls[1], tag);
	for (i = 0; i < host_calls[0] && i < host_calls[1]; i++) {
		struct cid_host_observation *a = &host_observations[0][i];
		struct cid_host_observation *b = &host_observations[1][i];
		diff_eq_obj("state and arguments visible at TTY callback",
			struct cid_host_observation, a, b, tag * 100 + i);
		diff_eq_int("TTY callback retains modem identity (%ld)", a->modem_matches, 1, tag * 100 + i);
		diff_eq_int("TTY callback payload provenance (%ld)",
			a->crlf ? a->string_offset == -1 : a->string_offset >= 0,
			1, tag * 100 + i);
		observed_host_calls++;
		if (!observation_negative) {
			/* A changed callback-visible field must change the record
			 * even when the forwarded TTY bytes are identical. */
			struct cid_host_observation wrong = *a;
			wrong.samples_fill++;
			diff_eq_int("state-only callback negative rejected",
				memcmp(&wrong, b, sizeof wrong) != 0, 1, tag);
			observation_negative = 1;
		}
	}
	return r1 == r2 ? r1 : -99;
}

int
main(void)
{
	void *ours, *ref;
	int live0, verdict;

	diff_begin("caller ID wrapper");
	harness_tty_reset();
	live0 = harness_alloc.live;

	/* The rate gate: only 8000 and 9600 exist. */
	diff_eq_int("rate 7200 refused (ours %ld)",
		    CID_create(&dummy_modem_ours, 7200, 1) == 0, 1, 7200);
	diff_eq_int("rate 7200 refused (ref %ld)",
		    ref_CID_create(&dummy_modem_ref, 7200, 1) == 0, 1, 7200);
	diff_eq_int("rate 0 refused (ours %ld)",
		    CID_create(&dummy_modem_ours, 0, 1) == 0, 1, 0);
	diff_eq_int("rate 0 refused (ref %ld)",
		    ref_CID_create(&dummy_modem_ref, 0, 1) == 0, 1, 0);
	diff_eq_int("refusals leak nothing (%ld live)",
		    harness_alloc.live, live0, 0);

	/* 9600 exists too and takes the cid_freq_sampl branch. */
	ours = CID_create(&dummy_modem_ours, 9600, 1);
	ref = ref_CID_create(&dummy_modem_ref, 9600, 1);
	diff_eq_int("9600 create agrees (%ld)",
		    (ours != 0), (ref != 0), 9600);
	if (ours)
		CID_delete(ours);
	if (ref)
		ref_CID_delete(ref);
	diff_eq_int("9600 create/delete balances (%ld live)",
		    harness_alloc.live, live0, 9600);

	/* The real run, at 8000. */
	ours = CID_create(&dummy_modem_ours, 8000, 1);
	ref = ref_CID_create(&dummy_modem_ref, 8000, 1);
	diff_eq_int("8000 create agrees (%ld)", (ours != 0), (ref != 0),
		    8000);
	diff_eq_int("8000 create succeeded (%ld)", ours != 0 && ref != 0,
		    1, 8000);

	/* Silence first: nothing may be delivered or decided. */
	memset(burst, 0, 2000 * sizeof(short));
	verdict = feed(ours, ref, burst, 2000, 500, 1);
	diff_eq_int("silence decides nothing (%ld)", verdict, 0, 1);
	diff_eq_int("silence puts nothing on the TTY (%ld)",
		    harness_tty_ours.len, harness_tty_ref.len, 0);

	/*
	 * The good burst.  Chunked at 500 samples -- not a multiple of the
	 * wrapper's 192 -- so the internal chunking is exercised too.
	 */
	build_burst(0);
	verdict = feed(ours, ref, burst, nsamp, 500, 2);
	diff_eq_int("the good burst DELIVERS on both sides (%ld)",
		    verdict, 1, 2);
	diff_eq_int("TTY byte counts (%ld)",
		    harness_tty_ours.len, harness_tty_ref.len, 2);
	diff_eq_int("TTY call counts (%ld)",
		    harness_tty_ours.calls, harness_tty_ref.calls, 2);
	diff_eq_int("TTY transcripts byte-identical (%ld)",
		    harness_tty_ours.len == harness_tty_ref.len
		    && memcmp(harness_tty_ours.data, harness_tty_ref.data,
			      (unsigned)(harness_tty_ours.len
					 < HARNESS_TTY_MAX
					 ? harness_tty_ours.len
					 : HARNESS_TTY_MAX)) == 0, 1, 2);
	diff_eq_int("something was actually delivered (%ld bytes)",
		    harness_tty_ours.len > 0, 1, harness_tty_ours.len);

	CID_delete(ours);
	ref_CID_delete(ref);

	/*
	 * The whole burst in ONE CID_process call, so the early exit on a
	 * settled verdict is observable: the wrapper must stop at the chunk
	 * that delivered and leave the tail of the buffer unoffered.  A
	 * wrapper that kept going would either re-deliver (TTY grows) or let
	 * a later chunk overwrite the verdict.
	 */
	harness_tty_reset();
	ours = CID_create(&dummy_modem_ours, 8000, 1);
	ref = ref_CID_create(&dummy_modem_ref, 8000, 1);
	build_burst(0);
	/*
	 * Trailing silence, so the chunk that settles is nowhere near the
	 * last: a wrapper that failed to stop would run the receiver on
	 * into the silence and come home with the silence's verdict.
	 */
	memset(burst + nsamp, 0, 2048 * sizeof(short));
	verdict = feed(ours, ref, burst, nsamp + 2048, nsamp + 2048, 5);
	diff_eq_int("one-call burst delivers on both sides (%ld)",
		    verdict, 1, 5);
	diff_eq_int("one-call TTY byte counts (%ld)",
		    harness_tty_ours.len, harness_tty_ref.len, 5);
	diff_eq_int("one-call TTY call counts (%ld)",
		    harness_tty_ours.calls, harness_tty_ref.calls, 5);
	CID_delete(ours);
	ref_CID_delete(ref);

	/* The wrecked burst: same signal, checksum broken. */
	harness_tty_reset();
	ours = CID_create(&dummy_modem_ours, 8000, 1);
	ref = ref_CID_create(&dummy_modem_ref, 8000, 1);
	build_burst(1);
	verdict = feed(ours, ref, burst, nsamp, 192, 3);
	diff_eq_int("the wrecked burst FAILS on both sides (%ld)",
		    verdict, -1, 3);
	diff_eq_int("failure delivers nothing (%ld)",
		    harness_tty_ours.len, harness_tty_ref.len, 3);
	CID_delete(ours);
	ref_CID_delete(ref);
	diff_eq_int("everything released (%ld live)",
		    harness_alloc.live, live0, 3);

	/*
	 * Once more with the debug tier open and captured, over the whole
	 * lifecycle, so the four gated printfs compare as text.
	 */
	dsplibs_debug_level = ref_dsplibs_debug_level = 2;
	dsplib_debug_capture_on = 1;
	dsplib_debug_capture_reset();
	harness_tty_reset();
	ours = CID_create(&dummy_modem_ours, 8000, 1);
	ref = ref_CID_create(&dummy_modem_ref, 8000, 1);
	build_burst(0);
	verdict = feed(ours, ref, burst, nsamp, 192, 4);
	diff_eq_int("debug run delivers too (%ld)", verdict, 1, 4);
	CID_delete(ours);
	ref_CID_delete(ref);

	/* The failure diagnostic needs its own real checksum-failure history. */
	harness_tty_reset();
	ours = CID_create(&dummy_modem_ours, 8000, 1);
	ref = ref_CID_create(&dummy_modem_ref, 8000, 1);
	build_burst(1);
	verdict = feed(ours, ref, burst, nsamp, 192, 6);
	diff_eq_int("debug wrecked burst fails (%ld)", verdict, -1, 6);
	diff_eq_int("debug failure delivers no TTY bytes (%ld)",
		    harness_tty_ours.len + harness_tty_ref.len, 0, 6);
	CID_delete(ours);
	ref_CID_delete(ref);
	diff_eq_int("debug lifecycles released (%ld live)",
		    harness_alloc.live, live0, 6);
	dsplib_debug_capture_on = 0;
	dsplibs_debug_level = ref_dsplibs_debug_level = 0;

	/*
	 * Both receivers now run independently. Keep this wrapper fixture's
	 * assertions scoped to its own five diagnostic prefixes; receiver
	 * diagnostics are outside this boundary. The native callsite observer
	 * additionally checks the wrapper's exact diagnostic arguments and
	 * their order relative to progress, strings and TTY callbacks.
	 */
	{
		static const char *own[] = {
			"cid: create...", "cid: CID Success",
			"cid: STR: '", "cid: delete...", "cid: CID Failure"
		};
		unsigned k;

		for (k = 0; k < sizeof(own) / sizeof(own[0]); k++)
			diff_eq_int("wrapper debug line count (%ld)",
				    count_substr(dsplib_debug_capture_text(0),
						 own[k]),
				    count_substr(dsplib_debug_capture_text(1),
						 own[k]), k);
	}
	diff_eq_int("our wrapper actually printed (%ld lines)",
		    dsplib_debug_capture_lines(0) > 0, 1,
		    (long)dsplib_debug_capture_lines(0));
	diff_eq_int("the success line is among them (%ld)",
		    count_substr(dsplib_debug_capture_text(0),
				 "cid: CID Success") > 0, 1, 4);
	diff_eq_int("the failure line is among them (%ld)",
		    count_substr(dsplib_debug_capture_text(0),
				 "cid: CID Failure") > 0, 1, 6);

	diff_eq_int("read-only host observations executed (%ld)", observed_host_calls > 0, 1, observed_host_calls);
	diff_eq_int("callback state negative executed", observation_negative, 1, 0);
	printf("CID host boundary observations: %ld paired calls; state-only negative %d\n",
	       observed_host_calls, observation_negative);
	return diff_end();
}
