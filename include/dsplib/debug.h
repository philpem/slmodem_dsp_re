/*
 * debug.h -- the diagnostic hooks dsplibs.o imports.
 *
 * Three symbols, all undefined in the object and supplied by whatever links
 * it:
 *
 *     nm -u ../slmodemd/dsplibs.o | grep -E 'debug'
 *
 * They are separate from sysdep.h because they are a different kind of
 * dependency.  `sysdep_*` are libc wrappers the datapumps genuinely need;
 * these are pure diagnostics, every use is gated on `dsplibs_debug_level`,
 * and slmodemd ships with that at zero -- so on a working modem none of the
 * call sites do anything at all.
 *
 * WHY THE RECONSTRUCTION CARRIES THEM ANYWAY.  Two reasons, and the second is
 * the one that matters.  The gating comparison is real code with real
 * branches, and a reconstruction that dropped it would differ from the object
 * in its control flow even when the output matched.  More usefully, the
 * format strings are the original author's own words about what the code is
 * doing -- `Carrier Detection Time Out `, `Energy drop detected......` -- and
 * several findings in this project rest on them.  Discarding a call site
 * discards the annotation.
 *
 * The level is the object's own type: `cmpl $0x1,dsplibs_debug_level`
 * compares a full 32-bit word, and every gate in the object is `> 1`.
 */

#ifndef DSPLIB_DEBUG_H
#define DSPLIB_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int dsplibs_debug_level;

int dsplibs_debug_printf(const char *fmt, ...);

/*
 * The modem core's data logger.  Declared here for completeness -- the object
 * imports it -- and not yet called by anything reconstructed.
 */
int modem_debug_log_data(void *m, unsigned id, const void *buf, int len);

/*
 * Every gate in the object is this comparison, spelled out here so a module
 * does not have to remember that it is `> 1` and not `>= 1`.
 */
#define DSPLIB_DEBUG_ON()	(dsplibs_debug_level > 1)

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_DEBUG_H */
