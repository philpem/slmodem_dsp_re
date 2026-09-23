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
 *
 * ALL SEVEN ARE NOW WRITTEN.  `dp_v22_init`/`dp_v22_exit`
 * (`src/pump/v22/v22.c`) were the last graduation, following V.32's into
 * `v32.h`.  Both used to be declared here under a local
 * `DSPLIB_DPINIT_UNWRITTEN` weak idiom -- the same shape `vpcm.h`'s
 * `DSPLIB_VPCM_UNWRITTEN` used to carry before that apparatus was removed -- so that the differential
 * binaries could bridge the real names to the blob's copy through
 * `test/harness/unwritten.c` while `v22.c` did not yet exist; that bridge is
 * long gone (see `unwritten.c`'s own note) and this file no longer needs the
 * idiom at all.  Every prototype below comes from its own datapump's header;
 * nothing here is declared weak.  Finding F8538/F8600-8604 record when
 * `v22.c` itself landed; this wiring was the piece left behind.
 */

#include "dsplib/b103.h"
#include "dsplib/call.h"
#include "dsplib/dp.h"
#include "dsplib/v22.h"
#include "dsplib/v23.h"
#include "dsplib/v32.h"
#include "dsplib/v8dp.h"
#include "dsplib/vpcm.h"

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
