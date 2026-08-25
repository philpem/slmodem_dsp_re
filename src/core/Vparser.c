/*
 * Vparser.c -- the parameter-file reader, as the shipped object contains it.
 *
 * THE TRANSLATION UNIT NAME IS THE OBJECT'S OWN.  `tools/tumap.py` recovers
 * `Vparser.c` from the `STT_FILE` sequence, TU 276 of 283, and the two
 * symbols below are the whole of it -- six bytes of `.text`, between the G.711
 * conversions and `encode.c`'s `cEncodeChar`.  `src/core/encode.c`'s header
 * comment already names this file as its predecessor in `.text`.
 *
 * BOTH FUNCTIONS ARE STUBS IN THE SHIPPED BUILD, and that is a measurement:
 *
 *     000b0990 <Vparser_read_int>:      31 c0    xor %eax,%eax
 *                                       c3       ret
 *     000b09a0 <Vparser_read_float>:    31 c0    xor %eax,%eax
 *                                       c3       ret
 *
 * three bytes each.  An `awk` over `objdump -dr` of the whole of `.text` says
 * `V90Parameters::loadParams` and `V92Parameters::loadParams` are the ONLY
 * callers of either, anywhere.  So the released library reads no parameter
 * file: every one of the 349 calls writes nothing and returns 0, and both
 * `loadParams` members are behaviourally no-ops.  Finding F860.
 *
 * WHAT WAS REMOVED IS NOT RECOVERABLE AND IS NOT INVENTED HERE.  A build of
 * this library with a real parser would open `paramFile`, find `name` and
 * write `*value`; none of that is in the object, so none of it is written
 * here.  Reconstructing what a stub would have been is exactly the
 * wrong-but-plausible code the tree's one hard rule forbids -- there would be
 * no oracle for a line of it.  What IS reconstructed is the object's own
 * behaviour: store nothing, return zero.
 *
 * WHY `return 0` AND NOT AN EMPTY `void`.  GCC emits a bare `ret` for a
 * `void` function that falls off its end.  These zero `%eax` first, so a
 * value is being returned and it is 0.  See include/dsplib/Vparser.h for what
 * that does and does not settle.
 */

#include "dsplib/Vparser.h"

int
Vparser_read_int(char *paramFile, const char *name, int *value)
{
	(void)paramFile;
	(void)name;
	(void)value;

	return 0;
}

int
Vparser_read_float(char *paramFile, const char *name, float *value)
{
	(void)paramFile;
	(void)name;
	(void)value;

	return 0;
}
