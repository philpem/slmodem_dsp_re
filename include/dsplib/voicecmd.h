/*
 * voicecmd.h -- `voice_dle_command` and the two flags it sets.
 *
 *   voice_dle_command  .text 0x0abe20  196 bytes
 *
 * IT IS VOICE, NOT FAX, AND THE SPAN NAME SAYS OTHERWISE.  `tumap.py` puts
 * it in a bracket whose label is `class1tx.c`, and that label comes from the
 * blob's LAYOUT rather than from any claim about the module: the bracket is
 * shared with `voice.c#260`, the symbol is `voice_*`, and all three strings
 * it prints say "voice".  CLAUDE.md's rule for exactly this ("do not read a
 * span name as a module name") is why it lives in its own file here instead
 * of in `src/fax/class1tx.c`.
 *
 * WHAT IT IS.  The DLE-shielded control codes of a voice connection.  In a
 * voice call the modem escapes in-band commands with DLE; this handles two of
 * them and rejects everything else:
 *
 *   <DLE><ETX>  (0x03)  end of the voice data stream -- sets `dle_etx`,
 *                       returns 0
 *   <DLE><CAN>  (0x18)  abort -- sets `dle_can`, returns 9
 *   anything else       nothing is written; returns 0, and at debug level
 *                       > 1 prints "Unknown command - %2x"
 *
 * The command byte is loaded with `movsbl`, so it is a SIGNED char and a
 * high-bit byte reaches the default arm as a negative number rather than as
 * 0x80..0xff.  That is a real difference from `unsigned char` for exactly the
 * printed value, which is why the type is spelled out rather than left to a
 * plain `char` whose signedness varies by target.
 */

#ifndef DSPLIB_VOICECMD_H
#define DSPLIB_VOICECMD_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The two DLE-shielded codes the object knows, by their ASCII names -- which
 * is how the author's own strings spell them ("voice dle command: ETX",
 * "voice <CAN> command").
 */
#define VOICE_DLE_ETX	0x03
#define VOICE_DLE_CAN	0x18

/* What <DLE><CAN> returns.  The object has no name for it and neither has
 * slmodemd's `VOICE_STATUS_*` / `VOICE_CMD_*`, whose values do not reach 9,
 * so it is left as the number the object returns. */
#define VOICE_DLE_CAN_STATUS	9

/*
 * The voice service's context.  IT USED TO BE DEFINED HERE, modelled only as
 * far as `voice_dle_command` could see it -- a 0x744-byte pad and the two int
 * flags below.  `voice_online`, `voice_duplex` and `voice_tx` reach a dozen
 * more fields and `voice_create` settles the size at 0x7dc, so the definition
 * moved to its own home in `dsplib/voice.h` (finding F8785).  The two flags
 * `voice_dle_command` writes keep their names and their offsets there:
 *
 *   +0x744  dle_etx   set to 1 by <DLE><ETX>
 *   +0x748  dle_can   set to 1 by <DLE><CAN>
 *
 * Both are STILL usage inference, the weakest of CLAUDE.md's three grades:
 * what is established is the store and the author's printf beside it.
 * `voice_tx` is the first reconstructed reader of either -- it suppresses its
 * "not enough data" report once `dle_etx` is up -- and that is consistent
 * with the name without proving it.
 */
#include "dsplib/voice.h"

/**
 * @brief Handle a voice connection's DLE-shielded control code.
 *
 * `<DLE><ETX>` (0x03) marks end of the voice data stream and sets
 * `dle_etx`; `<DLE><CAN>` (0x18) is an abort and sets `dle_can`. Anything
 * else is ignored (logged at debug level > 1 only).
 *
 * @param v   The voice context.
 * @param cmd The command byte following the DLE, sign-extended.
 * @return 0 for `<DLE><ETX>` and for an unrecognised command;
 *         `VOICE_DLE_CAN_STATUS` (9) for `<DLE><CAN>`.
 */
int voice_dle_command(struct voice_ctx *v, signed char cmd);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_VOICECMD_H */
