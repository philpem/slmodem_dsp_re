/*
 * encode.c -- the obfuscated diagnostic channel.
 *
 * Two functions, 402 bytes, and one of the most-depended-on things left in
 * the object: **145 functions are blocked on `edprintf` alone**, nearly all
 * of the V.90/V.92 C++ side.  It is the reason so much of that half cannot
 * be started, and it is 351 bytes of string handling.
 *
 * The translation unit is `encode.c`, anchored exactly: its `STT_FILE` entry
 * is followed by `offsetarr`, `iEncodeOffset`, `temp.0` and
 * `cEncodedTemp.1`, and `.text` puts `cEncodeChar` and `edprintf` after
 * `Vparser.c`'s two stubs with nothing but `pow.S` following.  So this file
 * is the whole TU, not a slice of one, and it is the object's LAST C
 * translation unit.
 *
 * WHAT IT DOES, AND WHY A "PRINT" FUNCTION ENCODES.  `edprintf` formats its
 * arguments, then emits each byte as TWO characters -- high nibble then low
 * -- with a rotating offset added to each, wrapped in `$!$ ` and `????`:
 *
 *     edprintf("Hi")  ->  $!$ 8>8@????        (from a fresh counter)
 *
 * so the diagnostic that reaches the log is not readable and needs the
 * offset table to recover.  That is the point: it is a channel for
 * diagnostics the manufacturer's own tool reads and a user does not.  The
 * frame characters are what a decoder looks for.
 *
 * THE OFFSET IS A ROTATING KEY, not a checksum.  `offsetarr` is ten entries
 * and `iEncodeOffset` steps through them once per emitted character,
 * wrapping 9 -> 0, so the same byte encodes differently depending on where
 * in the stream it falls.  The counter is FILE-STATIC AND SHARED: an
 * `edprintf` resets it to zero and then leaves it wherever its own output
 * ended, and a `cEncodeChar` in between moves it on by one.  Neither
 * function is independent of the other's history.
 */

#include "dsplib/debug.h"
#include "dsplib/encode.h"
#include "dsplib/sysdep.h"

/*
 * The rotating key, `offsetarr` in the object: ten ints at .data+0x94a0.
 * Only the low byte of each is used -- the addition is 8-bit -- and all ten
 * are in 0..9, so the encoded characters land in the printable range just
 * above '0' for ordinary text.
 *
 * Nine appears twice and zero not at all; it is not a permutation of 0..9
 * and there is no arithmetic in it worth deriving.  Emitted as found.
 */
static const int offsetarr[ENCODE_KEY_LEN] = {
	4, 6, 2, 7, 1, 9, 3, 5, 8, 7
};

/*
 * Where the counter has got to.  `iEncodeOffset` in the object, and a
 * global there rather than a function static, which is what lets the two
 * functions share it.
 */
static int iEncodeOffset;

/*
 * The formatting scratch and the encoded result, both function statics in
 * the object -- `temp.0` and `cEncodedTemp.1`.
 *
 * ONE BYTE LARGER THAN THE ORIGINAL'S.  `cEncodedTemp` is 0x10e = 270 bytes
 * there, and the length guard admits `2 * len + 8 <= 270`; at the boundary
 * that is a 4-byte prefix, 262 encoded characters and a 4-byte suffix, whose
 * terminator goes to index 270 -- one past the end.  See D39.  The array
 * here is 271 so that terminator has somewhere legal to land; nothing else
 * changes, because the guard is reproduced exactly and every character of
 * every string is at the same index either way.
 */
static char temp[ENCODE_FMT_MAX];
static char cEncodedTemp[ENCODE_OUT_MAX + 1];

/*
 * One character through the key: add the current offset and '0', then step.
 *
 * THE ADDITION IS 8-BIT in both callers -- `add %al,%cl` in `cEncodeChar`
 * and `add 0x94a0(,%eax,4),%dl` in `edprintf`, both byte operands -- so the
 * result wraps in a char rather than in an int.  It cannot matter for the
 * values `offsetarr` holds; it is kept because it is what the object does.
 *
 * The object has this twice rather than calling it once, which is why the
 * two copies advance the same counter in the same way: they are one step
 * written out in two places, not two policies.
 */
static char
encode_step(int v)
{
	char c = (char)(v + offsetarr[iEncodeOffset] + '0');

	iEncodeOffset = (iEncodeOffset == ENCODE_KEY_LEN - 1)
			? 0 : iEncodeOffset + 1;
	return c;
}

/*
 * Encode one character and step the key.
 *
 * Takes the byte whole -- no nibble split -- so this is not the step
 * `edprintf` uses on its own output, and mixing the two shifts the key
 * under both.  Nothing in the object calls it; it is exported for whatever
 * reads the channel from outside.
 */
char
cEncodeChar(unsigned char c)
{
	return encode_step((int)c);
}

/*
 * Format, encode and hand the result to the debug hook.
 *
 * THE WORK HAPPENS WHETHER OR NOT ANYTHING IS LISTENING.  Only the final
 * `dsplibs_debug_printf` is behind the level test: the formatting, the
 * encoding and -- the part that matters -- the reset and advance of
 * `iEncodeOffset` all run regardless.  So a build with diagnostics off
 * still moves the key, and `cEncodeChar`'s output depends on how many
 * `edprintf` calls preceded it.  That is the only externally visible effect
 * this function has when the level is zero, and it is why the test can
 * check it without reading a transcript.
 *
 * THE TOO-LONG PATH DOES NOT RESET.  It replaces the buffer with a fixed
 * message and returns, leaving `iEncodeOffset` exactly as it was -- unlike
 * every successful call, which starts by zeroing it.
 *
 * THE HIGH NIBBLE IS AN ARITHMETIC SHIFT.  `sar $0x4,%dl` on a byte, so a
 * source byte of 0x80 or more contributes -8..-1 rather than 8..15 and
 * encodes below '0'.  Formatted text is ASCII and does not reach there, but
 * a format string carrying a high byte would, and the shift is signed here
 * for that reason.
 */
void
edprintf(const char *fmt, ...)
{
	va_list ap;
	unsigned len;
	unsigned i;
	unsigned o;

	va_start(ap, fmt);
	sysdep_vsnprintf(temp, ENCODE_FMT_MAX, fmt, ap);
	va_end(ap);

	/*
	 * 2 per character, plus the four-byte prefix and the four-byte
	 * suffix.  The terminator is not counted, which is D39.
	 */
	len = sysdep_strlen(temp);
	if (2 * len + 8 > ENCODE_OUT_MAX) {
		sysdep_strcpy(cEncodedTemp, "too long print string");
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("%s\n", cEncodedTemp);
		return;
	}

	iEncodeOffset = 0;
	sysdep_strcpy(cEncodedTemp, "$!$ ");
	o = sysdep_strlen(cEncodedTemp);

	/*
	 * `strlen(temp)` is recomputed on every iteration in the object.
	 * Nothing writes `temp` inside the loop, so it is a call per
	 * character and not a re-read of anything that moves; reproduced
	 * because it is free to reproduce and the alternative is to assert
	 * that it cannot matter.
	 */
	for (i = 0; i < sysdep_strlen(temp); i++) {
		char c = temp[i];

		cEncodedTemp[o++] = encode_step((int)(signed char)c >> 4);
		cEncodedTemp[o++] = encode_step((int)(c & 0xf));
	}

	cEncodedTemp[o] = '\0';
	sysdep_strcat(cEncodedTemp, "????");

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("%s\n", cEncodedTemp);
}
