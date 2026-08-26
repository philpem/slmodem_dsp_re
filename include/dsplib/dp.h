/*
 * dp.h -- DataPump: the interface slmodemd defines.
 *
 * THE LAYOUTS ARE NO LONGER COPIED HERE.  `struct dp`, `struct dp_operations`
 * and the seven DPSTAT_* codes come from slmodemd's own headers, vendored
 * verbatim at `third_party/slmodem/`, hash-pinned by `tools/vendor.json` and
 * gated by `tools/vendorcheck.py`.  This file used to carry a hand copy of
 * all three and a comment saying so; a hand copy of a layout is a divergence
 * no differential test can see, and F8402 found a real one -- `struct
 * dsp_info`'s `clock_deviation` -- within an hour of the copies being taken.
 * Checking two spellings for drift is weaker than having one spelling.
 * Findings F8401, F8402 and F8410.
 *
 * The offsets still matter to the reconstruction: dp_wrapper_run reaches the
 * wrapper state through `dp->dp_data`, which the disassembly shows at +0x10.
 * That is now asserted against the vendored layout rather than restated.
 */

#ifndef DSPLIB_DP_H
#define DSPLIB_DP_H

/*
 * PULLED IN FIRST, DELIBERATELY, so that it is OUTSIDE the macro window
 * below.  `modem_dp.h` includes `<modem_defs.h>` itself, and that file's own
 * `__MODEM_DEFS_H__` guard then makes the inner include a no-op -- so the
 * window contains modem_dp.h's own text and nothing else, and no system
 * header is ever preprocessed with `delete` defined.  `modem_defs.h` reaches
 * `<sys/types.h>`, which under C++ is exactly where a stray `delete` macro
 * would do damage.
 */
#include <modem_defs.h>

/*
 * `modem_dp.h` declares `int (*delete)(struct dp *);` and `delete` is a C++
 * keyword.  No `.cpp` includes THIS header directly, but `dsplib/vpcm.h`
 * does, and `VPcmXfCreate.cpp`, `v34pcmmain.cpp` and `v34pcmcreate.cpp` all
 * include that.  A forward declaration does not get them out of it:
 * `struct vpcm_root` EMBEDS `struct dp` as its first member because the
 * object does (`3a76: mov %ebx,0x10(%ebx)` is `root->dp.dp_data = root`), and
 * an embedded member needs the complete type.
 *
 * VERBATIM MEANS THE VENDORED FILE IS NEVER EDITED AND EVERY ACCOMMODATION
 * LANDS ON OUR SIDE -- that is the rule that makes the drift check mean
 * anything, and `tools/vendorcheck.py` enforces it.  So the member is
 * macro-renamed for the C++ half only, and the macro is `#undef`d on the very
 * next line.  The layout is unaffected, the C half keeps the author's own
 * spelling, and no `.cpp` in this tree references the member -- checked with
 * a grep for `.delete`/`->delete`/`.destroy` over every `.cpp` and `.hpp`,
 * not assumed.  It is ugly and it is contained, which is the correct trade
 * when the alternative is editing a file whose whole value is being unedited.
 *
 * Findings F8401, F8402 and F8410.  DO NOT "TIDY" THIS AWAY.
 */
#ifdef __cplusplus
#define delete dp_delete_member
#endif
#include <modem_dp.h>
#ifdef __cplusplus
#undef delete
#endif

/*
 * A datapump's process entry point, as stored by dp_wrapper_create and
 * invoked by dp_wrapper_run.  Returns 0 for success, or a DPSTAT_* code.
 *
 * Ours, not slmodemd's: the wrapper is this library's own indirection and the
 * host never sees it.
 */
typedef int (*dp_process_fn)(void *dp, void *in, void *out, int count);

#endif /* DSPLIB_DP_H */
