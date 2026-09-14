/*
 * fax.c -- the empty translation unit the object records.
 *
 * dsplibs.o carries a FILE record `fax.c` immediately after `voice.c` with
 * no file-local symbols after it, and no function or object is attributed to
 * it: the FAX public entries live in `class1.c`'s unit here, and under the
 * catch-all `FixedRC.c` file record there.  Whatever the original fax.c
 * defined produced no symbol of its own, so this file exists only to emit the
 * same FILE record in the same position.  It is deliberately empty.
 */
