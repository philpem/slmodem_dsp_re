/*
 * pcm.c -- ITU-T G.711 companding.
 *
 * Reconstructed from dsplibs.o .text 0x0b07a0-0x0b0951.
 *
 * Attribution note: the six conversion routines sit in the address bracket
 * shared by silence.c, PHASOR.c, TABLES.c, pcm.c, Vparser.c and encode.c, and
 * `ld -r` did not preserve the boundary between them.  tools/tuattrib.py
 * therefore reports them as ambiguous rather than guessing.  `pcm.c` is chosen
 * on content and on its position in the FILE order; that is an inference, not
 * a proven attribution.
 *
 * Provenance of the algorithm: this is recognisably the public-domain Sun
 * Microsystems G.711 reference implementation (the one that shipped as
 * g711.c with the SunOS audio tools and was widely copied), with one
 * substantive change.
 *
 *   Sun's version works on 13-bit A-law / 14-bit u-law magnitudes and starts
 *   by discarding low bits: `pcm_val >> 3` for A-law, `>> 2` for u-law.
 *   This version keeps full 16-bit resolution and never pre-shifts; the
 *   segment-end table is scaled up to match:
 *
 *       Sun   seg_aend[] = { 0x1F, 0x3F, ... 0xFFF }   (13-bit)
 *       here  seg_end[]  = {  255,  511, ... 32767 }   (16-bit, 8x Sun's)
 *
 *   The bias and rounding constants follow: the negative-input adjustment is
 *   `-x - 8` here where Sun uses `-x - 1`, again a factor of 8.
 *
 * Both directions are exact inverses only within the quantisation the codec
 * imposes; the differential test in test/unit/t_pcm.c checks all 65536 linear
 * inputs and all 256 code inputs against the original object.
 */

#include "dsplib/pcm.h"

/* Upper bound of each companding segment, at full 16-bit scale. */
static const short seg_end[8] = {
	255, 511, 1023, 2047, 4095, 8191, 16383, 32767,
};

/*
 * Locate the segment containing `mag`.  Returns 0..7, or 8 when the magnitude
 * exceeds the largest segment -- callers treat that as saturation.
 */
static int
search_segment(int mag)
{
	int seg;

	for (seg = 0; seg < 8; seg++) {
		if (seg_end[seg] >= mag)
			return seg;
	}
	return 8;
}

/*
 * 16-bit linear -> A-law.
 *
 * A-law code layout:  S EEE MMMM, then every other bit inverted (^ 0x55).
 * The even-bit inversion is what gives A-law its all-zeros-free line code.
 */
unsigned char
linear2alaw(int pcm_val)
{
	int mask, seg, mag;
	unsigned char aval;

	if (pcm_val >= 0) {
		mag = pcm_val;
		mask = 0xd5;		/* sign bit set, even bits inverted */
	} else {
		mag = -pcm_val - 8;
		mask = 0x55;
	}

	seg = search_segment(mag);
	if (seg >= 8)			/* out of range - saturate */
		return (unsigned char)(0x7f ^ mask);

	aval = (unsigned char)(seg << 4);
	/*
	 * Segments 0 and 1 share the same step size, so both take a fixed
	 * 4-bit shift; from segment 2 upward the step doubles per segment.
	 */
	if (seg < 2)
		aval |= (mag >> 4) & 0x0f;
	else
		aval |= (mag >> (seg + 3)) & 0x0f;

	return (unsigned char)(aval ^ mask);
}

/*
 * A-law -> 16-bit linear.
 */
int
alaw2linear(unsigned char a_val)
{
	int t, seg;

	a_val ^= 0x55;

	t = (a_val & 0x0f) << 4;
	seg = (int)(((unsigned)a_val & 0x70) >> 4);

	switch (seg) {
	case 0:
		t += 8;			/* half a step, to centre the bin */
		break;
	case 1:
		t += 0x108;		/* segment 1 starts one full segment up */
		break;
	default:
		t += 0x108;
		t <<= seg - 1;
		break;
	}

	return (a_val & 0x80) ? t : -t;
}

/*
 * 16-bit linear -> u-law.
 *
 * u-law adds a bias before segmentation so that the smallest segment is
 * centred on zero, then complements the whole code.
 */
unsigned char
linear2ulaw(int pcm_val)
{
	int mask, seg, mag;
	unsigned char uval;

	if (pcm_val < 0) {
		mag = PCM_ULAW_BIAS - pcm_val;
		mask = 0x7f;
	} else {
		mag = pcm_val + PCM_ULAW_BIAS;
		mask = 0xff;
	}

	seg = search_segment(mag);
	if (seg >= 8)			/* out of range - saturate */
		return (unsigned char)(0x7f ^ mask);

	uval = (unsigned char)((seg << 4) | ((mag >> (seg + 3)) & 0x0f));
	return (unsigned char)(uval ^ mask);
}

/*
 * u-law -> 16-bit linear.
 */
int
ulaw2linear(unsigned char u_val)
{
	int t;

	u_val = (unsigned char)~u_val;

	t = ((u_val & 0x0f) << 3) + PCM_ULAW_BIAS;
	t <<= (int)(((unsigned)u_val & 0x70) >> 4);

	return (u_val & 0x80) ? (PCM_ULAW_BIAS - t) : (t - PCM_ULAW_BIAS);
}

/*
 * Direct code-to-code conversion via lookup, avoiding a round trip through
 * linear (which would lose accuracy twice).  Both tables are extracted
 * verbatim from the original object; they are the standard G.711 conversion
 * tables from CCITT Recommendation G.711 Tables 1 and 2.
 */
static const unsigned char a2u_table[128] = {
	1, 3, 5, 7, 9, 11, 13, 15, 16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31, 32, 32, 33, 33, 34, 34, 35, 35,
	36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 48, 49, 49,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 64,
	65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 79,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
	112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
};

static const unsigned char u2a_table[128] = {
	1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
	9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 27, 29, 31, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
	46, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
	64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96,
	97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112,
	113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128,
};

/*
 * NO `& 0x7f` ON THE INDEX, and the object is what says so.  Each arm xors
 * with a constant whose bit 7 MATCHES the arm's own test -- 0xd5 and 0xff
 * where bit 7 is set, 0x55 and 0x7f where it is clear -- so the xor always
 * clears bit 7 and the index is already 0..127 on every one of the 256
 * inputs.  A mask there is therefore redundant over the whole domain, which
 * is exactly why no test could ever see it and why the codegen tier had to:
 *
 *     blob:  movzbl %al,%eax        the unsigned char -> int conversion
 *     ours:  and    $0x7f,%eax      the mask, and nothing else
 *
 * both three bytes, so the function stayed the right size while carrying an
 * operation the original does not have.  This is finding F613's shape -- a
 * difference the differential tier is structurally unable to reach -- and it
 * is recorded as finding F2901.  Removing it leaves the indices unchanged and
 * both tables are 128 entries, so nothing here goes out of bounds.
 */
unsigned char
alaw2ulaw(unsigned char a_val)
{
	return (a_val & 0x80)
		? (unsigned char)(0xff ^ a2u_table[a_val ^ 0xd5])
		: (unsigned char)(0x7f ^ a2u_table[a_val ^ 0x55]);
}

unsigned char
ulaw2alaw(unsigned char u_val)
{
	return (u_val & 0x80)
		? (unsigned char)(0xd5 ^ (u2a_table[0xff ^ u_val] - 1))
		: (unsigned char)(0x55 ^ (u2a_table[0x7f ^ u_val] - 1));
}
