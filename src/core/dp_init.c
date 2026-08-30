/*
 * dp_init.c -- the aggregate the host calls once, reconstructed from
 * dsplibs.o `dp_init.c`.
 *
 *   prop_dp_init   .text 0x000000   44 bytes
 *   prop_dp_exit   .text 0x000030   44 bytes
 *
 * The very first bytes of the object: slmodemd's `modem_main.c` declares
 * `extern int prop_dp_init(void)` and `extern void prop_dp_exit(void)` and
 * calls each exactly once, at daemon start and shutdown, and these two fan
 * that out to every datapump's own registration pair.  Seven calls each, in
 * the same order both ways -- exit does NOT unwind in reverse:
 *
 *     call, b103, v22, v23, v32, v8, vpcm
 *
 * Both end `xor %eax,%eax; ret`, so both return 0 as an `int` even though
 * the host declares the exit half `void` -- the asymmetry is in slmodemd's
 * extern, not here.  Neither takes an argument and neither has any state of
 * its own; the operations tables live with their datapumps.
 *
 * The blob's span for the OTHER half of the name is not this file:
 * `CID_create`/`CID_delete`/`CID_process` sit at 0x340 with `dcr.c`'s four
 * functions between (0x60..0x33f), so one contiguous translation unit
 * containing both halves is impossible and the CID wrappers live in
 * `src/service/cid.c`.
 */

#include "dsplib/b103.h"
#include "dsplib/call.h"
#include "dsplib/dp.h"
#include "dsplib/v23.h"
#include "dsplib/v32.h"
#include "dsplib/v8dp.h"
#include "dsplib/vpcm.h"

/*
 * The ONE datapump this tree has not written yet.  No header declares it;
 * V.32's pair is now `src/pump/v32/v32.c`'s and comes in through `v32.h`.
 * Declared `int`/`void` on the family pattern -- the result is discarded, so
 * the caller's code is the `call` either way.
 *
 * WEAK, the `DSPLIB_VPCM_UNWRITTEN` idiom (vpcm.h explains it at length):
 * the differential binaries bridge each name to the blob's copy through
 * `test/harness/unwritten.c`, and the interop binaries -- no blob, and
 * nothing there calls `prop_dp_init` -- resolve the weak references to
 * zero.  No null tests: the object calls all seven unconditionally.
 */
#define DSPLIB_DPINIT_UNWRITTEN __attribute__((weak))
extern int dp_v22_init(void) DSPLIB_DPINIT_UNWRITTEN;
extern void dp_v22_exit(void) DSPLIB_DPINIT_UNWRITTEN;

int
prop_dp_init(void)
{
	dp_call_init();
	dp_b103_init();
	dp_v22_init();
	dp_v23_init();
	dp_v32_init();
	dp_v8_init();
	dp_vpcm_init();
	return 0;
}

int
prop_dp_exit(void)
{
	dp_call_exit();
	dp_b103_exit();
	dp_v22_exit();
	dp_v23_exit();
	dp_v32_exit();
	dp_v8_exit();
	dp_vpcm_exit();
	return 0;
}
