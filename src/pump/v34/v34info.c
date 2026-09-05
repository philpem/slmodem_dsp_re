/*
 * v34info.c -- ITU-T V.34: the INFO0, INFO1a and Modem-on-Hold codecs.
 *
 * THE SAME TRANSLATION UNIT AS v34pcmif.c, split by role rather than by
 * address: both files hold `extern "C"` exports of `VPcmV34Main.cpp`, which
 * is C++ and is not otherwise reconstructed.  See the header of v34pcmif.c
 * for the evidence that the TU is that one.  Two files, one TU -- do not
 * read them as two.
 *
 * `v34pcmif.c` has the accessors and the phase-3 indications; this has the
 * four functions that assemble or take apart a phase-2 INFO message, and
 * `V34GiveProbeResults`, which is the same shape: a message arriving from
 * the C++ side and being copied into the V.34 object.
 *
 * THE SPLIT BETWEEN `Set` AND `Give` IS THE DIRECTION.  `V34SetINFO0aBits`
 * and `V34SetINFO0dBits` WRITE the caller's message buffer -- they are the
 * encoders, called before an INFO0 is transmitted.  `V34GiveINFO0dBits` and
 * `V34GiveINFO1aBits` READ it: something has been received and is being
 * handed over.  The names read the other way round at first glance and do
 * not mean that.
 *
 * ONE MESSAGE BUFFER, THIRTEEN SHORTS.  Every function here takes the
 * caller's INFO message as a `short *`, and between them they touch indices
 * 0..3 (SetINFO0aBits), 0..9 (both debug prints) and 12 (SetINFO0dBits).
 * So a caller that sized the buffer from the ten-value print would be
 * written past by the encoder.  There is no bound in the object; the number
 * is stated here because a differential test with a buffer that is too small
 * reads adjacent state on both sides and can agree by accident (finding F129).
 */

#include "dsplib/debug.h"
#include "dsplib/v34fsk.h"
#include "dsplib/v34info.h"
#include "dsplib/v34rx.h"	/* bitreverse */

/*
 * ---------------------------------------------------------------------------
 * The session object at +0x3548.
 *
 * `VPcmV34Main.cpp` hangs its state off one pointer in the V.34 object, and
 * the four functions here reach four things through it.  None of them is
 * reconstructed, so the offsets are named constants and the reads are
 * spelled out rather than given a struct that would be a guess.
 */

/*
 * +0x6120.  Selects which INFO0 variant is in play: NON-ZERO IS INFO0a AND
 * ZERO IS INFO0d, and two independent sets of strings say so.
 * `V34GiveINFO0dBits` prints the letter directly -- `giveINFO0%cBits`, `a`
 * when non-zero -- and `V34SetINFO0aBits`'s four "setting..." strings land
 * on the same axis (see there).
 *
 * That is as far as the evidence goes, and it is deliberately not called
 * answer-versus-call: the flag also swaps two bit POSITIONS and their two
 * SOURCES between its branches, which a plain role would not.
 */
#define SESSION_VARIANT		0x6120

/*
 * +0x611c.  ONE LOCATION, TWO WIDTHS: `V34GiveINFO1aBits` writes it as an
 * int -- 0 on entry, 1 when the upstream baud index is 6 -- and reads it
 * back with `movswl` as a short.  The same aliasing the receiver
 * object's +0x12c has, and recorded for the same reason (v34recv.h).
 */
#define SESSION_UINFO6		0x611c

/* +0x612c and +0x1760.  Two further pointers, into blocks of bytes. */
#define SESSION_CAPS		0x612c
#define SESSION_UPSTREAM	0x1760

static int
session_int(const void *s, unsigned off)
{
	return *(const int *)((const char *)s + off);
}

static void *
session_ptr(const void *s, unsigned off)
{
	return *(void *const *)((const char *)s + off);
}

/*
 * ---------------------------------------------------------------------------
 * The probe results.
 */

/*
 * Copy 25 doubles out of a 44-byte-stride record the C++ side owns.
 *
 * The count and the stride are both the object's: the loop's bound is
 * `cmp $0x18,%ax` after the increment, so 0..24 inclusive, and the source
 * cursor starts at +0x20 and advances by 0x2c.  44 is not a multiple of 8,
 * so every other source double is only 4-byte aligned -- which `fldl` does
 * not mind and a `double *` in C would.  Copied a byte at a time for that
 * reason; the value is not touched, only moved.
 *
 * Returns 0, on both paths.  There is no error code to invent.
 */
int
V34GiveProbeResults(void *objp, const void *src)
{
	struct v34_object *obj = (struct v34_object *)objp;
	const unsigned char *p;
	int i, k;

	/* Neither PCM receiver running: nothing to record. */
	if (obj->v90_receiver == 0 && obj->k56flex_receiver == 0)
		return 0;

	p = (const unsigned char *)src + V34_PROBE_OFFSET;
	for (i = 0; i < V34_PROBE_RESULTS; i++) {
		union {
			double d;
			unsigned char b[sizeof(double)];
		} u;

		for (k = 0; k < (int)sizeof(double); k++)
			u.b[k] = p[k];
		obj->probe_results[i] = u.d;
		p += V34_PROBE_STRIDE;
	}

	return 0;
}

/*
 * ---------------------------------------------------------------------------
 * INFO0, outbound.
 */

/*
 * The two names the object prints for the PCM modulation in play.
 *
 * `V34SetINFO0aBits` selects between them on `k56flex_receiver`, and the
 * parenthesis in the first is the original author's: K.56Flex and V.34 send
 * the same INFO0, so there is nothing to choose between them here and the
 * string says so.
 */
static const char *
pcm_name(const struct v34_object *obj)
{
	return obj->k56flex_receiver != 0 ? "K.56Flex (same as V.34)" : "V.34";
}

/*
 * Set the one INFO0d bit the V.90 receiver cares about.
 *
 * Does nothing at all unless a V.90 receiver is running, and then writes a
 * constant into index 12 of the message.  The smallest function in the file
 * and the only one that touches that index.
 */
void
V34SetINFO0dBits(void *objp, short *bits)
{
	struct v34_object *obj = (struct v34_object *)objp;

	if (obj->v90_receiver == 0)
		return;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("V90, setINFO0dBits\n");

	bits[12] = 30;
}

/*
 * Assemble the outbound INFO0.
 *
 * FOUR CASES, ON TWO INDEPENDENT TESTS -- the session variant at +0x6120 and
 * whether a V.90 receiver is running -- and each has its own string:
 *
 *     variant  v90_receiver
 *      != 0      != 0        "setting info0a for V.PCM"
 *      != 0      == 0        "setting info0a for %s"
 *      == 0      != 0        "setting info0d (Digital) for V.PCM"
 *      == 0      == 0        "setting info0 (Caller) for %s"
 *
 * BOTH AXES ARE CLEAN, which is what makes the table worth having: the
 * variant picks `info0a` against `info0d`, matching the letter
 * `V34GiveINFO0dBits` prints, and `v90_receiver` picks "for V.PCM" against
 * "for <whatever pcm_name says>".
 *
 * The two V.PCM strings were transcribed the wrong way round on the first
 * attempt and the transcript comparison caught it.  Nothing else could have:
 * they are on branches whose stores are identical, so every byte of state
 * agreed.  Finding F171.
 *
 * THE TWO NON-TRIVIAL CASES ARE NEAR-DUPLICATES AND ARE NOT THE SAME.  Both
 * set the same two leading shorts and both then test two flags and set two
 * bits, but:
 *
 *   - the SHORT-PHASE-2 request sets bit 0 when the variant is non-zero and
 *     bit 1 when it is zero;
 *   - the V.92 indication sets bit 1 and bit 0 respectively -- the other way
 *     round again;
 *   - and its SOURCE differs.  Non-zero variant reads a byte at +0x11 of the
 *     session's capability block; zero variant reads the object's own
 *     `local_v92`.
 *
 * Two swapped bits and two different sources between two blocks that print
 * the same two strings.  Transcribed separately rather than factored for
 * exactly that reason -- finding F130 is about the helper that looked shared
 * and was not.  Only the non-zero-variant case sets `prev_bulk_delay`, and
 * only the zero-variant case fills indices 2 and 3 with rate information.
 */
void
V34SetINFO0aBits(void *objp, short *bits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	const void *sess = obj->p3548;

	if (session_int(sess, SESSION_VARIANT) != 0) {
		if (obj->v90_receiver == 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"setINFO0aBits - setting info0a " "for %s\n", pcm_name(obj));
			bits[0] = (short)0xff;
			bits[1] = (short)0x84;
			bits[2] = 0;
			return;
		}

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"setINFO0aBits - setting info0a " "for V.PCM\n");
		bits[0] = (short)0xff;
		bits[1] = (short)0x84;

		if (obj->local_short != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"...requesting short phase2...\n");
			bits[1] = (short)(bits[1] | 1);
			obj->prev_bulk_delay = 0x14;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"Setting prev bulk delay = %d\n",
					0x14);
		}

		if (*((const char *)session_ptr(sess, SESSION_CAPS) + 0x11)
		    != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"...indicating V.92 " "capabilities...\n");
			bits[1] = (short)(bits[1] | 2);
		}

		bits[2] = 0;
		return;
	}

	if (obj->v90_receiver == 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("setINFO0aBits - setting info0 " "(Caller) for %s\n",
					     pcm_name(obj));
		bits[0] = (short)0xff;
		bits[1] = (short)0x84;
		bits[2] = 0;
		return;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("setINFO0aBits - setting info0d "
				     "(Digital) for V.PCM\n");
	bits[0] = (short)0xff;
	bits[1] = (short)0x84;

	if (obj->local_short != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"...requesting short phase2...\n");
		bits[1] = (short)(bits[1] | 2);
	}
	if (obj->local_v92 != 0) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"...indicating V.92 capabilities...\n");
		bits[1] = (short)(bits[1] | 1);
	}

	/*
	 * The upstream rate, from a byte the session object holds at +9 of
	 * its upstream block, bit-reversed over five bits and then split
	 * across two shorts: three bits into index 2 under a constant 0x30,
	 * and the remaining two into the top of index 3.
	 *
	 * The shift is ARITHMETIC in the object -- `cwtl` then `sar $2` --
	 * which cannot matter for a five-bit reverse but is kept so it stays
	 * true if the width ever moves.
	 */
	bits[2] = 0x30;
	{
		const void *up = session_ptr(sess, SESSION_UPSTREAM);
		int r = bitreverse((unsigned short)
				   *((const unsigned char *)up + 9), 5);
		int v;

		bits[2] = (short)(((short)r >> 2 & 7) | (unsigned short)bits[2]);

		v = ((short)r << 6) & 0xc0;
		if (*(const int *)((const char *)up + 0xc) == 1)
			v |= 0x20;
		bits[3] = (short)v;
		if (*(const int *)up == 1)
			bits[3] = (short)(bits[3] | 0x10);
		bits[3] = (short)(bits[3] | 8);
	}
}

/*
 * ---------------------------------------------------------------------------
 * INFO0, inbound.
 */

/*
 * Unpack a received INFO0 into the object's bit vector, and decide whether
 * a short phase 2 is on.
 *
 * ONE FUNCTION FOR BOTH VARIANTS.  The name says `0d` and the debug strings
 * say `giveINFO0%cBits` and `rxinfo0%c` -- the letter is `a` or `d`
 * according to the session variant -- so this decodes either.  What the
 * variant changes is only which of two adjacent bits is the remote's V.92
 * capability and which is its short-phase-2 request: indices 26 and 27 swap
 * roles and nothing else does.
 *
 * THE BIT VECTOR IS ONE BIT PER INT, 41 of them, MSB first:
 *
 *     0..11   the constant 0xf72, bits 11 down to 0
 *     12..19  message short 0, bits 7 down to 0
 *     20..27  message short 1
 *     28..35  message short 2
 *     36..40  message short 3, bits 7 down to 3
 *
 * The leading twelve are a fixed preamble the object rebuilds every call
 * rather than storing, which is what makes 0xf72 worth writing down: it is
 * the INFO0 header the decoder expects, not data.
 */
void
V34GiveINFO0dBits(void *objp, const short *bits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	const void *sess = obj->p3548;
	int *v = obj->info0_bits;
	int variant;
	int remote_v92, remote_short;
	short i;

	if (obj->v90_receiver == 0)
		return;

	variant = session_int(sess, SESSION_VARIANT);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main, giveINFO0%cBits\n",
				     variant == 0 ? 'd' : 'a');
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main, rxinfo0%c = ",
				     variant == 0 ? 'd' : 'a');
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
			(unsigned short)bits[0], (unsigned short)bits[1],
			(unsigned short)bits[2], (unsigned short)bits[3],
			(unsigned short)bits[4], (unsigned short)bits[5],
			(unsigned short)bits[6], (unsigned short)bits[7],
			(unsigned short)bits[8], (unsigned short)bits[9]);

	for (i = 0; i <= 11; i++)
		v[i] = (0xf72 >> (11 - i)) & 1;
	for (i = 12; i <= 19; i++)
		v[i] = ((unsigned short)bits[0] >> (19 - i)) & 1;
	for (i = 20; i <= 27; i++)
		v[i] = ((unsigned short)bits[1] >> (27 - i)) & 1;
	for (i = 28; i <= 35; i++)
		v[i] = ((unsigned short)bits[2] >> (35 - i)) & 1;
	for (i = 36; i <= 40; i++)
		v[i] = ((unsigned short)bits[3] >> (43 - i)) & 1;

	/*
	 * The two bits swap with the variant, and the object's own debug
	 * string is what names them: "remoteV92" and "remoteShort".
	 */
	if (variant != 0) {
		remote_v92 = v[27];
		remote_short = v[26];
	} else {
		remote_v92 = v[26];
		remote_short = v[27];
	}

	obj->remote_v92 = (short)remote_v92;

	/*
	 * All four have to be set.  The object tests the first three as 16
	 * bits and the fourth as 32; the values are 0 or 1 either way, so it
	 * is written as the conjunction it is.
	 */
	obj->is_short = (obj->local_v92 != 0 && obj->remote_v92 != 0
			 && obj->local_short != 0 && remote_short != 0)
			? 1 : 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"VPcmV34Main: localV92- %d, remoteV92- %d, "
			"localShort- %d, remoteShort- %d, isShort- %d\n",
			obj->local_v92, obj->remote_v92, obj->local_short,
			remote_short, obj->is_short);
}

/*
 * ---------------------------------------------------------------------------
 * INFO1a, inbound.
 */

/*
 * Take apart a received INFO1a.
 *
 * TWO COMPLETELY DIFFERENT JOBS, chosen by which PCM receiver is running,
 * and they share only the first statement:
 *
 *   - with a V.90 receiver, decode the upstream baud index and the Uinfo
 *     code, and hand them on to the session object;
 *   - without one but with a K56Flex receiver, read a single bit that says
 *     whether the remote will do K56Flex, and set `k56flex_receiver` to 2
 *     or 0 accordingly;
 *   - with neither, do nothing.
 *
 * The shared first statement is `*(int *)(session + 0x611c) = 0`, and it
 * happens before either test -- so a call that does nothing else still
 * clears that flag.  The return value is the same location read back as a
 * short, so it is 1 exactly when the upstream baud index was 6 and 0
 * otherwise.
 */
int
V34GiveINFO1aBits(void *objp, const short *bits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	void *sess = obj->p3548;
	const void *pcm = obj->pac18;
	int baud_index;
	int uinfo;

	*(int *)((char *)sess + SESSION_UINFO6) = 0;

	if (obj->v90_receiver == 0) {
		if (obj->k56flex_receiver == 0)
			goto out;

		if (((unsigned short)bits[3] & 8) != 0) {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"VPcmV34Main: K56Flex enabled by " "remote, PCM type: local %d, remote "
					"%d (A=1, Mu=0)\r\n",
					*(const int *)((const char *)pcm
						       + 0xc),
					(unsigned short)bits[3] & 2);
			obj->k56flex_receiver = 2;
		} else {
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"VPcmV34Main: K56Flex disabled by " "remote\r\n");
			obj->k56flex_receiver = 0;
		}
		goto out;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: giveINFO1aBits\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"VPcmV34Main: rxinfo1a = 0x%x,0x%x,0x%x,0x%x,0x%x,"
			"0x%x,0x%x,0x%x,0x%x,0x%x\n",
			(unsigned short)bits[0], (unsigned short)bits[1],
			(unsigned short)bits[2], (unsigned short)bits[3],
			(unsigned short)bits[4], (unsigned short)bits[5],
			(unsigned short)bits[6], (unsigned short)bits[7],
			(unsigned short)bits[8], (unsigned short)bits[9]);

	/*
	 * The upstream baud index: three bits, gathered from two shorts and
	 * REORDERED on the way -- bit 1 of index 2 becomes bit 0, bit 0
	 * becomes bit 1, and bit 7 of index 3 becomes bit 2.  Written as the
	 * object computes it rather than as a mask and a shift, because the
	 * first two really do swap.
	 *
	 * WHICH OF THIS AND `uinfo` BELOW IS WHICH comes from the format
	 * strings and from nothing else: this three-bit value is the one the
	 * object prints as "upstream baud index" and the seven-bit reversed
	 * one below is the one it prints as "Uinfo".  The names read more
	 * naturally the other way round -- a seven-bit baud index and a
	 * three-bit Uinfo -- and the first transcription had them that way.
	 * The object's own words win.  Finding F171.
	 */
	baud_index = (((unsigned short)bits[2] & 2) >> 1)
		     + ((unsigned short)bits[2] & 1) * 2
		     + (((unsigned short)bits[3] & 0x80) >> 5);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"VPcmV34Main: upstream baud index = %d\r\n",
			baud_index);

	if ((short)baud_index == 6) {
		void *caps;

		*(int *)((char *)sess + SESSION_UINFO6) = 1;

		/*
		 * Three two-bit fields out of index 0, each assembled with
		 * the HIGHER-NUMBERED BIT AS THE LOW ONE: bit7 + 2*bit6,
		 * bit5 + 2*bit4, bit3 + 2*bit2 -- a pair read backwards, and
		 * the same reversal `bits[2]`'s two Uinfo bits get.  Stored
		 * in the object and then copied on as bytes, which is the
		 * only reason they are stored at all.
		 */
		obj->short_abce = (short)((((unsigned short)bits[0] & 0x80) >> 7)
				     + (((unsigned short)bits[0] & 0x40) >> 5));
		obj->short_abd0 = (short)((((unsigned short)bits[0] & 0x20) >> 5)
				     + (((unsigned short)bits[0] & 0x10) >> 3));
		obj->short_abd2 = (short)((((unsigned short)bits[0] & 0x08) >> 3)
				     + (((unsigned short)bits[0] & 0x04) >> 1));

		caps = session_ptr(sess, SESSION_CAPS);
		*((char *)caps + 0x14) = (char)obj->short_abce;
		caps = session_ptr(sess, SESSION_CAPS);
		*((char *)caps + 0x15) = (char)(unsigned short)obj->short_abd0;
		caps = session_ptr(sess, SESSION_CAPS);
		*((char *)caps + 0x16) = (char)(unsigned short)obj->short_abd2;
	}

	/*
	 * Uinfo: seven bits assembled from the low three of index 1 and the
	 * high nibble of index 2, then bit-reversed, and written into BOTH
	 * session blocks at +8 -- capability block first, which is the order
	 * the object stores them in.
	 */
	uinfo = bitreverse((unsigned short)
			   ((((unsigned short)bits[1] & 7) << 4)
			    + (((unsigned short)bits[2] & 0xf0) >> 4)), 7);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("VPcmV34Main: Uinfo is %d\r\n",
				     (short)uinfo);

	*((char *)session_ptr(sess, SESSION_CAPS) + 8) = (char)(short)uinfo;
	*((char *)session_ptr(sess, SESSION_UPSTREAM) + 8) =
		(char)(short)uinfo;

out:
	return *(const short *)((const char *)sess + SESSION_UINFO6);
}

/*
 * ---------------------------------------------------------------------------
 * Modem on Hold.
 */

/*
 * Build the first short of one of V.92's six MOH messages.
 *
 * A jump table on `moh_message`, and the six are named by the object's own
 * strings rather than by anything this reconstruction supplies:
 *
 *     0  MHreq   0x33          3  MHcda   0xbb
 *     1  MHfrr   0xdd          4  MHack   0x50 + moh_holdtime_code
 *     2  MHclrd  0x95/96/9a    5  MHnack  0x77
 *
 * OUT OF RANGE DOES NOTHING AT ALL -- not even the debug line -- and the
 * caller's buffer is left exactly as it was.  The object bounds the selector
 * with one unsigned compare, `cmp $0x5; ja`, so a negative value falls out
 * there too; the switch below reaches the same place by enumerating the six
 * and defaulting, which needs no cast to agree.
 *
 * TWO CASES STORE TWICE.  MHack writes 0x50 and then overwrites it with
 * `0x50 + moh_holdtime_code`; MHclrd writes 0x90 and then one of three values.  Both
 * intermediate stores are to the same short and are immediately replaced, so
 * only the final value is written here -- there is no observable difference
 * and the object's own compiler would have been free to drop them too.
 */
void
VPcmV34SetMohMessageBits(void *objp, short *bits)
{
	struct v34_object *obj = (struct v34_object *)objp;

	switch (obj->moh_message) {
	case 0:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34SetMohMessageBits: "
					     "Building MHreq message...\r\n");
		bits[0] = 0x33;
		break;
	case 1:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34SetMohMessageBits: "
					     "Building MHfrr message...\r\n");
		bits[0] = (short)0xdd;
		break;
	case 2:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34SetMohMessageBits: "
					     "Building MHclrd message...\r\n");
		if (obj->moh_clrd_sel == 0)
			bits[0] = (short)0x95;
		else if (obj->moh_clrd_sel == 1)
			bits[0] = (short)0x96;
		else
			bits[0] = (short)0x9a;
		break;
	case 3:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34SetMohMessageBits: "
					     "Building MHcda message...\r\n");
		bits[0] = (short)0xbb;
		break;
	case 4:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34SetMohMessageBits: "
					     "Building MHack message...\r\n");
		bits[0] = (short)((unsigned short)obj->moh_holdtime_code + 0x50);
		break;
	case 5:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("VPcmV34SetMohMessageBits: "
					     "Building MHnack message...\r\n");
		bits[0] = 0x77;
		break;
	default:
		break;
	}
}

/*
 * Decode the first short of an arriving MOH message.
 *
 * The mirror of the function above, and the two agree on their numbering
 * even though they are different fields -- 0 MHreq, 1 MHfrr, 2 MHcld,
 * 3 MHcda, 4 MHack, 5 MHnack.  What arrives goes in `moh_recvd` and what we
 * are about to send is in `moh_message`, four bytes apart.
 *
 * THREE MESSAGES CARRY A NIBBLE AND THE OTHERS DO NOT:
 *
 *     0x5X  MHack   X is a time-out period code   -> moh_holdtime_code
 *     0x9X  MHcld   X is a disconnect reason      -> moh_clrd_sel, recoded 0/1/2
 *     0x75  MHnack  no nibble, but sets short_abe2 = 3
 *
 * `0x75` and `0x77` are both MHnack and both decode to 5.  The difference is
 * in the strings -- "may NOT init MOH in the future" against "may" -- and in
 * `short_abe2`, which only 0x75 writes.  The sending side has one MHnack and
 * always builds 0x77, so this end can say the stronger thing and cannot
 * hear itself say it.
 *
 * THE TWO KINDS OF ARM MATCH DIFFERENTLY.  0x33, 0x77, 0x75, 0xbb and 0xdd
 * are compared as whole 16-bit values; the 0x5X and 0x9X arms mask with 0xf0
 * and ignore everything above bit 7.  So 0x150 is an MHack and 0x133 is not
 * an MHreq.  See D50.
 *
 * `MHclrd` in the builder and `MHcld` here are the object's own two
 * spellings of one message; neither is this reconstruction's typo.
 */
void
VPcmV34InterpretMohMessageBits(void *objp, const short *bits)
{
	struct v34_object *obj = (struct v34_object *)objp;
	unsigned w = (unsigned short)bits[0];

	if (w == 0x33) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("MHreq message detected !\r\n");
		obj->moh_recvd = 0;
		return;
	}
	if ((w & 0xf0) == 0x50) {
		obj->moh_holdtime_code = (short)(w & 0xf);
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("MHack message detected ! , "
					     "Time out period code = %d\r\n",
					     (int)(w & 0xf));
		obj->moh_recvd = 4;
		return;
	}
	if (w == 0x77) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("MHnack message detected (modem "
					     "may init MOH in the future) " "!\r\n");
		obj->moh_recvd = 5;
		return;
	}
	if (w == 0x75) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("MHnack message detected (modem "
					     "may NOT init MOH in the future) " "!\r\n");
		obj->short_abe2 = 3;
		obj->moh_recvd = 5;
		return;
	}
	if ((w & 0xf0) == 0x90) {
		switch (w & 0xf) {
		case 5:
			obj->moh_clrd_sel = 0;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"MHcld message detected ! , disconnect " "reason is incoming call\r\n");
			break;
		case 6:
			obj->moh_clrd_sel = 1;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"MHcld message detected ! , disconnect " "reason is outgoing call\r\n");
			break;
		case 0xa:
			obj->moh_clrd_sel = 2;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"MHcld message detected ! , disconnect " "reason is other reason\r\n");
			break;
		default:
			/*
			 * The same 2 the "other reason" case sets, and the
			 * string says why: a reserved code is assumed to mean
			 * other.  Two arms, one value.
			 */
			obj->moh_clrd_sel = 2;
			if (DSPLIB_DEBUG_ON())
				dsplibs_debug_printf(
					"MHcld message detected ! , reserved " "disconnect reason (assume other "
					"reason)\r\n");
			break;
		}
		obj->moh_recvd = 2;
		return;
	}
	if (w == 0xbb) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("MHcda message detected !\r\n");
		obj->moh_recvd = 3;
		return;
	}
	if (w == 0xdd) {
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("MHfrr message detected !\r\n");
		obj->moh_recvd = 1;
		return;
	}

	/*
	 * Nothing matched.  Four lines rather than one, with a rule above and
	 * below the other three -- so the transcript is the only place this
	 * arm differs from a plain MHnack, `moh_recvd` being 5 either way.
	 *
	 * The message is RE-READ from the buffer for the `%X`, at 0x8bb0,
	 * rather than kept in the register it is already in.  Nothing can
	 * have changed it.
	 */
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"====================================="
			"====================================\r\n");
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Illegal MH message detected !!! " "(rxsq[0] = %X)\r\n",
				     (int)(unsigned short)bits[0]);
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("forcing message type to MH NACK\r\n");
	obj->moh_recvd = 5;
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"====================================="
			"====================================\r\n");
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define V34INFO_ASSERT(name, field, off) \
	typedef char v34info_off_##name[ \
		((int)__builtin_offsetof(struct v34_object, field) == (off)) \
		? 1 : -1]

V34INFO_ASSERT(sess,   p3548,           0x3548);
V34INFO_ASSERT(probe,  probe_results,   0xa258);
V34INFO_ASSERT(info0,  info0_bits,      0xa8a4);
V34INFO_ASSERT(lv92,   local_v92,       0xabc6);
V34INFO_ASSERT(rv92,   remote_v92,      0xabc8);
V34INFO_ASSERT(lshort, local_short,     0xabca);
V34INFO_ASSERT(isshrt, is_short,        0xabcc);
V34INFO_ASSERT(abce,   short_abce,           0xabce);
V34INFO_ASSERT(abd0,   short_abd0,           0xabd0);
V34INFO_ASSERT(abd2,   short_abd2,           0xabd2);
V34INFO_ASSERT(mohv,   moh_holdtime_code,           0xabe0);
V34INFO_ASSERT(mohm,   moh_message,     0xabf0);
V34INFO_ASSERT(mohrx,  moh_recvd,       0xabf4);
V34INFO_ASSERT(abe2,   short_abe2,           0xabe2);
V34INFO_ASSERT(mohr,   moh_clrd_sel,           0xabfa);
V34INFO_ASSERT(bulk,   prev_bulk_delay, 0xac02);
V34INFO_ASSERT(pac18,  pac18,           0xac18);

/* The unpack writes v[40] and no further. */
typedef char v34info_bits_fit[(V34_INFO0_BITS > 40) ? 1 : -1];

#endif
