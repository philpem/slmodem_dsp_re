/*
 * FixedRC.c -- Fixed Rate Converter: fixed rational-factor resampling.
 *
 * Reconstructed from dsplibs.o FixedRC.c, .text 0x0b0d90-0x0b1cf0 and the
 * factor tables at .data 0x94e0 / 0x9540.
 *
 * This is the module that lets the datapumps run at 8 kHz while the host runs
 * at 9600.  RcFixed_Resample() is called from exactly four places in the
 * original: dp_wrapper_run(), call_run(), FAX_process() and VOICE_process().
 *
 * Modes 0 and 1 use a different state layout in the original (a 40-byte block
 * with three sub-allocations) and are not implemented here.  They are the
 * explicit-only x4 and /4 entries, unreachable from
 * RcFixed_Check_Combination(), so nothing in the modem can request them.
 */

#include <stdlib.h>
#include <string.h>

#include "dsplib/fixedrc.h"

/*
 * Supported conversion ratios, as (down, up) pairs indexed by mode.
 *
 * The conversion applied is out = in * up / down, so a mode's *down* factor is
 * matched against the input rate and its *up* factor against the output rate,
 * both reduced by their GCD.
 *
 * Reading the interesting entries:
 *
 *   mode 2   5:6    8000 -> 9600   pump to host
 *   mode 3   6:5    9600 -> 8000   host to pump      <- the common case
 *   mode 14  2:3    e.g. 8000 -> 12000
 *   mode 15  3:2
 *   mode 18  9:10   e.g. 7200 -> 8000
 *   mode 19  10:9
 *
 * Modes 8 and 9 duplicate the ratios of modes 0 and 1 (4:1 and 1:4).  They are
 * distinct entries because each mode selects its own filter design in
 * RcFixed_Create()'s jump table, so the same ratio can be offered at two
 * different filter lengths or bandwidths.
 *
 * The trailing {0, 0} is a terminator, and its index doubles as the
 * "unsupported" return value from RcFixed_Check_Combination().
 */
/*
 * `static int`, NOT `const int`, AND UP BEFORE DOWN -- all three read off the
 * relocations in `RcFixed_Check_Combination`, which reach both tables:
 *
 *   blob:  mov 0x9540(,%eax,4),%edx     R_386_32  .data     (up)
 *          cmp %ebx,0x94e0(,%eax,4)     R_386_32  .data     (down)
 *
 * A reference resolved against the SECTION symbol with the offset as an inline
 * addend is a reference to a file-local object: had these been `extern`, the
 * relocation would name them (CLAUDE.md's rule about what a relocation's
 * ABSENCE proves, in its data form).  The section is `.data` and not
 * `.rodata`, so they were not `const` either.  And `down` sits 0x60 BELOW `up`
 * in the blob while GCC 3.4 emits file-scope objects in reverse definition
 * order -- verified on this object -- so `up` was defined first.  Nothing
 * outside this file names either table and no header declares them.
 */
static int fixedRc_UpFact[RCFIXED_NMODES + 1] = {
	4, 1, 6, 5, 6, 1, 5, 1, 4, 1,
	24, 5, 5, 4, 3, 2, 10, 3, 10, 9,
	0,
};

static int fixedRc_DownFact[RCFIXED_NMODES + 1] = {
	1, 4, 5, 6, 1, 6, 1, 5, 1, 4,
	5, 24, 4, 5, 2, 3, 3, 10, 9, 10,
	0,
};

int
RcFixed_UpFactor(int mode)
{
	return (mode >= 0 && mode <= RCFIXED_NMODES) ? fixedRc_UpFact[mode] : 0;
}

int
RcFixed_DownFactor(int mode)
{
	return (mode >= 0 && mode <= RCFIXED_NMODES) ? fixedRc_DownFact[mode] : 0;
}

/*
 * Greatest common divisor, by Euclid.
 *
 * Matches the original's loop exactly, including its behaviour on zero and
 * negative inputs: it uses the C remainder operator, so gcd(x, 0) == x and
 * signs propagate rather than being normalised.  Callers pass positive rates,
 * but the differential test covers the edges anyway.
 */
static int
gcd(int a, int b)
{
	while (b != 0) {
		int t = a % b;
		a = b;
		b = t;
	}
	return a;
}

int
RcFixed_Check_Combination(int in_rate, int out_rate)
{
	int g, down, up, mode;

	g = gcd(in_rate, out_rate);

	up = out_rate / g;
	down = in_rate / g;

	/*
	 * Start at 2, skipping the explicit-only x4 and /4 entries, and stop
	 * at the {0,0} terminator.  Returning the terminator's index on no
	 * match is deliberate: RcFixed_Create() treats any mode above the
	 * table as "no conversion required".
	 */
	for (mode = 2; fixedRc_UpFact[mode] != 0; mode++) {
		if (fixedRc_UpFact[mode] == up && fixedRc_DownFact[mode] == down)
			break;
	}

	return mode;
}

/*
 * ---------------------------------------------------------------------------
 * Converter
 * ---------------------------------------------------------------------------
 */

struct rc {
	int kind;               /* +0x00 0 = polyphase, 1 = modes 0/1 */
	struct rc_state *state; /* +0x04 */
};

struct rc_state *
RcFixed_State(struct rc *h)
{
	return h ? h->state : NULL;
}

/*
 * Set the running state to its initial condition.
 *
 * `pos` starts at `taps`, not zero: the first `taps` history entries are left
 * as zero padding so the very first output has a full window to convolve
 * against without a special case.
 *
 * `input_needed` starts at 1 when up <= down and 0 otherwise.  Interpolating
 * ratios can emit an output before consuming anything; decimating ones cannot.
 */
static void
rc_reset_state(struct rc_state *s)
{
	memset(s->history, 0, sizeof(s->history));
	s->pos = s->taps;
	s->input_needed = (s->up <= s->down) ? 1 : 0;

	/*
	 * The original uses the C remainder, then corrects a negative result by
	 * adding `up`.  Rates are positive so the correction is unreachable in
	 * practice, but it is reproduced to keep behaviour identical.
	 *
	 * The correction tests the FIELD, not an `int` temporary: the object's
	 * test is sixteen bits wide -- `test %dx,%dx; js` at RcFixed_Reset+0x72
	 * -- which is the sign of the value after truncation to `phase`, and a
	 * 32-bit remainder in a local would give `test %edx,%edx`.  It is also
	 * a branch the compiler deletes outright if `phase` is unsigned, so its
	 * presence is a second proof of the declaration.  Finding F2700.
	 */
	s->phase = (short)(s->down % s->up);
	if (s->phase < 0)
		s->phase = (short)(s->phase + s->up);
}

struct rc *
RcFixed_Create(int mode)
{
	struct rc *h;
	struct rc_state *s;

	/* The original rejects absurd mode numbers before touching the tables. */
	if (mode < 0 || mode > 999)
		return NULL;

	/* Modes 0 and 1, and anything past the table, have no polyphase bank. */
	if (mode >= RCFIXED_NMODES || rc_banks[mode].coeff == NULL)
		return NULL;

	h = calloc(1, sizeof(*h));
	if (h == NULL)
		return NULL;

	s = calloc(1, sizeof(*s));
	if (s == NULL) {
		free(h);
		return NULL;
	}

	h->kind = 0;
	h->state = s;

	s->coeff = rc_banks[mode].coeff;
	s->taps = (short)rc_banks[mode].taps;
	s->up = (short)fixedRc_UpFact[mode];
	s->down = (short)fixedRc_DownFact[mode];

	rc_reset_state(s);
	return h;
}

/*
 * Put a converter back to the state Create left it in.
 *
 * The null check is ours and the original has none -- `RcFixed_Delete`
 * checks, this one dereferences its argument on the first instruction.  Kept
 * because a caller error should not be a fault here, and because it cannot
 * be differentially tested either way: the only input that would tell the two
 * apart crashes the reference.  Noted so it is not mistaken for something the
 * object does.
 */
void
RcFixed_Reset(struct rc *h)
{
	if (h != NULL && h->kind == 0 && h->state != NULL)
		rc_reset_state(h->state);
}

void
RcFixed_Delete(struct rc *h)
{
	if (h == NULL)
		return;
	free(h->state);
	free(h);
}

/*
 * Append one sample to the sliding window.
 *
 * The original does not use a circular buffer.  It writes forward through a
 * flat 200-entry array and, on reaching the end, copies the most recent `taps`
 * samples back to the start and resumes from there.  Compaction costs a short
 * memmove once every (200 - taps) samples, and in exchange the convolution
 * inner loop is a straight walk with no index wrapping -- worth it on a 2003
 * CPU, and reproduced here because it also determines exactly which samples
 * survive across the boundary.
 */
static void
rc_push(struct rc_state *s, short sample)
{
	s->history[s->pos++] = sample;

	if (s->pos == RCFIXED_HISTORY) {
		memmove(s->history, s->history + RCFIXED_HISTORY - s->taps,
			(size_t)s->taps * sizeof(s->history[0]));
		s->pos = s->taps;
	}
}

/*
 * One output sample: the inner product of the current polyphase branch with
 * the most recent `taps` inputs.
 *
 * Coefficients are Q14 and the accumulator is shifted right by 14.  The branch
 * base is `phase * taps`, and both pointers walk forward -- so coeff[0]
 * multiplies the *oldest* sample of the window, which is the reverse of
 * textbook convolution order.  See docs/coefficients.md.
 */
static short
rc_output(struct rc_state *s)
{
	const short *coeff = s->coeff + (int)s->phase * s->taps;
	const short *hist = s->history + (s->pos - s->taps);
	int acc = 0;
	int k;

	for (k = 0; k < s->taps; k++)
		acc += (int)coeff[k] * (int)hist[k];

	return (short)(acc >> 14);
}

/*
 * Advance the phase accumulator by `down`; every time it passes `up` another
 * input sample is owed.  This is the standard rational-rate bookkeeping:
 *
 *     input_needed = (phase + down) / up
 *     phase        = (phase + down) % up
 *
 * THE ORIGINAL SPELLS IT AS A SUBTRACT-AND-COUNT LOOP, not as a division, and
 * that is transcribed rather than paraphrased here.  Inlined into
 * RcFixed_Resample the object reads:
 *
 *     lea    (%ecx,%esi,1),%eax     ; phase + down
 *     mov    %ebp,0x1a0(%ebx)       ; input_needed = 0
 *     mov    %ax,0x194(%ebx)        ; phase = that, SIXTEEN bits
 *     cmp    %ax,%cx                ; against `up`, sixteen bits
 *     jg     ...                    ; SIGNED: skip if up > phase
 *     xor    %ebx,%ebx              ; n = 0
 *   1:mov    %eax,%edx
 *     inc    %ebx
 *     sub    %ecx,%edx              ; phase -= up
 *     cmp    %cx,%dx
 *     mov    %edx,%eax
 *     jge    1b                     ; SIGNED again
 *     mov    %dx,0x194(%edi)        ; the sunk stores
 *     mov    %ebx,0x1a0(%edi)
 *
 * Nothing in it is wider than sixteen bits, which is why writing it as a
 * division was visible to the codegen tier: `acc / (int)s->up` forces a
 * `cltd; idiv` and with it a 32-bit sign extension of both `down` and `up`,
 * and `extcheck` reported exactly that at mem 0x196 and mem 0x198 once the
 * declarations were corrected.  The two forms agree over every reachable
 * value (0 <= phase < up <= 24, 1 <= down <= 24).  Finding F2700.
 */
static void
rc_advance(struct rc_state *s)
{
	s->phase = (short)(s->phase + s->down);
	s->input_needed = 0;

	while (s->phase >= s->up) {
		s->phase = (short)(s->phase - s->up);
		s->input_needed++;
	}
}

void
RcFixed_Resample(struct rc *h, const short *in, int in_count,
		 short *out, int *out_count)
{
	struct rc_state *s;
	int produced = 0;
	int limit;

	/*
	 * The incoming value of *out_count is an output limit, read before it
	 * is zeroed.  Conversion stops when either the input runs out or that
	 * many samples have been produced.
	 *
	 * Passing 0 therefore means "no limit", not "produce nothing": the
	 * original compares for inequality, so once the first sample is
	 * emitted the count can never equal 0 again and the loop runs until
	 * the input is exhausted.  dp_wrapper relies on the limit; callers
	 * that pass 0 rely on the other reading.  Both are reproduced.
	 */
	limit = (out_count != NULL) ? *out_count : 0;

	if (out_count != NULL)
		*out_count = 0;

	if (h == NULL || h->kind != 0 || h->state == NULL)
		return;

	s = h->state;

	while (in_count > 0) {
		int need = s->input_needed;
		int take = (need < in_count) ? need : in_count;
		int i;

		for (i = 0; i < take; i++)
			rc_push(s, *in++);

		/*
		 * Ran out part-way through the samples this output needs.  The
		 * consumed ones stay in the history and `input_needed` carries
		 * the shortfall into the next call.
		 */
		if (need > in_count) {
			s->input_needed = need - take;
			break;
		}

		in_count -= need;

		out[produced++] = rc_output(s);
		rc_advance(s);

		/*
		 * Limit test at the bottom, matching the original: one output
		 * is always produced before it applies.  That is what makes a
		 * limit of 0 behave as "no limit" -- see the note above.
		 */
		if (produced == limit)
			break;
	}

	if (out_count != NULL)
		*out_count = produced;
}
