/*
 * encode.h -- the obfuscated diagnostic channel.  See src/core/encode.c.
 *
 * `edprintf` is what most of the V.90/V.92 half of the object uses to say
 * anything: 145 functions reference it.  What it emits is not readable text
 * -- each byte becomes two characters through a rotating key, framed by
 * `$!$ ` and `????` -- so this is a channel for the manufacturer's own tool
 * rather than for a log a user reads.
 */

#ifndef DSPLIB_ENCODE_H
#define DSPLIB_ENCODE_H

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The key, and the two buffers, all sized as the object sizes them. */
#define ENCODE_KEY_LEN	10	/* `offsetarr`, .data+0x94a0            */
#define ENCODE_FMT_MAX	0x100	/* `temp.0`: what vsnprintf is given    */
#define ENCODE_OUT_MAX	0x10e	/* `cEncodedTemp.1`, and the guard's bound */

/**
 * @brief Encode one byte and step the shared key.
 *
 * The whole byte, not a nibble, so this is not the step edprintf() applies
 * to its own output -- and both move the same shared counter, so
 * interleaving calls to the two shifts the key under both.
 *
 * @param c  The byte to encode.
 * @return The encoded byte.
 */
char cEncodeChar(unsigned char c);

/**
 * @brief Format a diagnostic message, encode it, and hand it to
 * `dsplibs_debug_printf`.
 *
 * Only the final print is gated on the debug level -- formatting and
 * encoding always run, so every call resets the shared key to zero and
 * leaves it wherever its own output ended. A caller that mixes edprintf()
 * and cEncodeChar() gets different characters out of the latter depending
 * on how many edprintf() calls preceded it, at any debug level.
 *
 * A formatted message longer than 131 characters is replaced by "too long
 * print string", and that path is the one call that does NOT reset the key.
 *
 * @param fmt  printf-style format string.
 * @param ...  Format arguments.
 */
void edprintf(const char *fmt, ...);

/*
 * Set non-zero to print the readable message instead of the encoded one.
 *
 * NOT SOMETHING THE ORIGINAL HAS -- see docs/deviations.md, D40.  It is zero
 * by default and read only from inside the debug-level gate, so a build with
 * `dsplibs_debug_level` at zero, which is every shipping one, runs exactly
 * the instructions the object runs.
 *
 * When it is on the encoding still runs in full and only the printed string
 * changes, so turning it on cannot perturb anything else -- in particular
 * `cEncodeChar`, which shares the counter, returns the same characters
 * either way.
 *
 * For logs that have already been captured, or that came from the original
 * binary, `tools/eddecode.py` decodes after the fact instead.
 */
extern int dsplib_encode_plain;

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_ENCODE_H */
