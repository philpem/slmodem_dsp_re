/*
 * voicecmd.c -- `voice_dle_command`, the voice connection's DLE-shielded
 * control codes.
 *
 * Reconstructed from dsplibs.o:
 *
 *   voice_dle_command  .text 0x0abe20  196 bytes
 *
 * WHY IT IS NOT IN `src/fax/`.  `tumap.py` brackets 0xac960 and below under
 * the label `class1tx.c`, and the same bracket carries `voice.c#260`; a span
 * label is the blob's layout and not a module name (CLAUDE.md), and every
 * piece of evidence inside the function says voice -- the symbol name, and
 * all three strings it prints.  File layout is ours to choose, so it gets its
 * own file and this note.  Finding F8774.
 *
 * WHY IT IS NOT IN `voice.c` EITHER.  It is 0xa9000 bytes away from the
 * `voice.c#3` span, so whatever TU it belongs to, it is not that one.
 *
 * See include/dsplib/voicecmd.h for the two commands, the return values and
 * how far `struct voice_ctx` is modelled.
 */

#include "dsplib/voicecmd.h"
#include "dsplib/debug.h"

/*
 * Three arms, and the two that do something are symmetrical: print, set one
 * flag, return.  The default arm sets nothing at all -- so an unknown command
 * byte on a voice connection is silently ignored unless the debug level is
 * above 1, and the caller cannot tell it apart from <DLE><ETX> by the return
 * value alone.
 *
 * The command byte is compared after a sign extension, so 0x83 is -125 here
 * and not 131; it can only ever reach the default arm, and what the object
 * prints for it with `%2x` is the sign-extended word.
 */
int
voice_dle_command(struct voice_ctx *v, signed char cmd)
{
	switch (cmd) {
	case VOICE_DLE_ETX:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("voice dle command: ETX\n");
		v->dle_etx = 1;
		return 0;
	case VOICE_DLE_CAN:
		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf("voice <CAN> command\n");
		v->dle_can = 1;
		return VOICE_DLE_CAN_STATUS;
	}

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Unknown command - %2x\n", cmd);
	return 0;
}
