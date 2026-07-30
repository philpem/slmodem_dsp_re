/*
 * b103.c -- Bell 103 / V.21 datapump registration and glue.
 *
 * Reconstructed from dsplibs.o b103.c:
 *   dp_b103_init   .text 0x005840
 *   dp_b103_exit   .text 0x005880
 *   b103_ops       .data  0x0090
 *   b103_create    .text 0x005400   (pending -- see below)
 *   b103_delete    .text 0x0055a0   (pending)
 *   b103_process   .text 0x0055f0   (pending)
 *
 * This is the thin layer between the modem core and the Bell 103 modulation.
 * It owns nothing of the DSP; it registers the datapump, allocates the state,
 * and wires the modulation into dp_wrapper.
 *
 * The shape is worth reading off the ops table, because it explains the whole
 * datapump architecture in one line: `process` is **dp_wrapper_run**, not
 * b103_process.  The modem core always calls the wrapper, and the wrapper
 * calls b103_process at the datapump's own rate and fragment size once it has
 * buffered and rate-converted.  b103_process is handed to
 * dp_wrapper_create() as a function pointer and is never reachable from the
 * ops table at all.
 *
 * STATUS: registration is complete and verified.  b103_create, b103_delete
 * and b103_process are not yet reconstructed -- they depend on B103FP_create
 * (0x867 bytes), B103FP_delete and B103FP_modem, which are the modulation
 * proper.  Until those land, the ops table carries NULL for create and
 * delete, so the module registers correctly but cannot yet build a datapump.
 */

#include "dsplib/b103.h"
#include "dsplib/dp_wrapper.h"

/* Provided by the modem core (slmodemd/modem.c). */
extern int modem_dp_register(int id, void *op);
extern void modem_dp_deregister(int id, void *op);

/*
 * Bell 103 and V.21 share one implementation: same 300 bit/s FSK, differing
 * only in tone frequencies, which b103_create selects from the DP_ID.  That
 * is why one ops table is registered under both.
 */
struct dp_operations b103_ops = {
	"b103",				/* +0x00 name                     */
	0,				/* +0x04 use_count                */
	0,				/* +0x08 create  -- pending       */
	0,				/* +0x0c delete  -- pending       */
	(int (*)(struct dp *, void *, void *, int))dp_wrapper_run,
					/* +0x10 process = the wrapper    */
	0				/* +0x14 hangup, unused           */
};

int
dp_b103_init(void)
{
	modem_dp_register(DP_B103, &b103_ops);
	modem_dp_register(DP_V21, &b103_ops);
	return 0;
}

void
dp_b103_exit(void)
{
	modem_dp_deregister(DP_B103, &b103_ops);
	modem_dp_deregister(DP_V21, &b103_ops);
}
