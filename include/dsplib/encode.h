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

/*
 * Encode one byte and step the key.  The whole byte, not a nibble, so this
 * is not the step `edprintf` applies to its own output -- and both move the
 * same shared counter, so interleaving them shifts the key under both.
 */
char cEncodeChar(unsigned char c);

/*
 * Format, encode, and hand the result to `dsplibs_debug_printf`.
 *
 * ONLY THE LAST OF THOSE IS GATED ON THE DEBUG LEVEL.  With diagnostics off
 * the encoding still runs, so every call resets the shared key to zero and
 * leaves it wherever its own output ended.  A caller that mixes `edprintf`
 * and `cEncodeChar` gets different characters out of the latter depending on
 * how many of the former preceded it, at any debug level.
 *
 * A formatted message longer than 131 characters is replaced by "too long
 * print string", and that path is the one call that does NOT reset the key.
 */
void edprintf(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_ENCODE_H */
