/* Fdsp.c -- FDSP datapump allocation and ownership state. */

#include "dsplib/fdspkrnl.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"

/*
 * The one kernel FDSP_DP_Create hands out, and a counter that is written
 * and never read.
 *
 * Both are `b` (LOCAL) in the blob, so both are `static` here, the same move
 * `bInternalBeepInProgress` above already makes.  A test names them and
 * reaches them through the test tier's globalized copies
 * (tools/testvisible.py).
 *
 * `uCorrelationReportsNo` has exactly ONE relocation against it in the
 * whole 1.2 MB -- the store at 0xae61d, below -- so nothing in the object
 * ever reads it back.  Its name is the author's and its purpose is not
 * established by anything here.
 */
static struct fdsp_kernel *pGlobalFDSPObj;
static unsigned int uCorrelationReportsNo;

/*
 * Tear the kernel down: the buffer block, then each channel's taps and the
 * channel itself, then the kernel, then the global.
 *
 * THE CHANNEL POINTERS ARE DEREFERENCED BEFORE THEY ARE TESTED.  `mov
 * 0x14(%ebx),%eax; mov 0x1680(%eax),%edx` at 0xae533 reads the taps out of
 * `chan_a` and only then does 0xae540 ask whether `chan_a` was NULL, and
 * `chan_b` is the same three instructions later.  So the NULL tests protect
 * `sysdep_free` and nothing else, and a kernel with a missing channel
 * faults here rather than being freed.  It is the object's shape and is
 * written as the object has it; the only caller that can produce such a
 * kernel is FDSP_DP_Create's own out-of-memory path, which the harness
 * allocator cannot drive.
 */
void
FDSP_DP_Delete(struct fdsp_kernel *k)
{
	if (k == 0)
		return;
	if (k->buffers != 0)
		sysdep_free(k->buffers);
	if (k->chan_a->coef != 0)
		sysdep_free(k->chan_a->coef);
	if (k->chan_a != 0)
		sysdep_free(k->chan_a);
	if (k->chan_b->coef != 0)
		sysdep_free(k->chan_b->coef);
	if (k->chan_b != 0)
		sysdep_free(k->chan_b);
	sysdep_free(k);
	pGlobalFDSPObj = 0;
}

/*
 * Create or re-initialise the kernel.
 *
 * A NULL `k` allocates the whole tree -- kernel, buffer block, both
 * channels, both tap arrays -- and a non-NULL one is simply re-initialised,
 * which is what the object's own "Reinitialization requested" line reports.
 * Either way the object ends up in FDSP_Kernel_InitObj's state and in
 * `pGlobalFDSPObj`.
 *
 * THE TWO ARGUMENTS ARE NAMED BY THE OBJECT'S OWN FORMAT STRING, which is
 * "ver 120 sRxSamplesDelay %d ,sTxSamplesDelay %d \n" at `.rodata.str1.4`
 * 0x12f30 -- so the rx delay is chan_a's window offset and the tx delay is
 * chan_b's, and that is also what fixes which channel is which direction.
 * Both are `short` (`movswl` at 0xae5de and 0xae5e3).
 *
 * `status` is 2 unless the RX delay is NEGATIVE, in which case it is 0:
 * `sar $0x1f; not; and $0x2` is a branchless `(rx >= 0) ? 2 : 0`, and it
 * lands on the field InitObj has just set to 2.
 *
 * THE ALLOCATION CHAIN'S FAILURE ARMS CANNOT BE DRIVEN HERE.  Six
 * allocations each guard the next, and the harness allocator does not fail
 * on request, so `t_fdspdp` exercises the success path and the
 * re-initialisation path and records the rest.  Two of them are worse than
 * a leak and are written as the object has them: the two `offset` stores at
 * 0xae6c2 happen BEFORE the chain's result is tested, so a failed channel
 * allocation is dereferenced there; and the cleanup that follows is
 * FDSP_DP_Delete, whose own unguarded dereference is described above.
 */
struct fdsp_kernel *
FDSP_DP_Create(struct fdsp_kernel *k, short sRxSamplesDelay,
	       short sTxSamplesDelay)
{
	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("ver 120 sRxSamplesDelay %d ," "sTxSamplesDelay %d \n",
				     sRxSamplesDelay, sTxSamplesDelay);
	if (k == 0) {
		int ok;

		k = sysdep_malloc(sizeof(*k));
		ok = k != 0;
		if (ok) {
			k->chan_b = 0;
			k->chan_a = 0;
			k->buffers = 0;
		}
		if (ok) {
			k->buffers = (struct fdsp_buffers *)
				     sysdep_malloc(sizeof(*k->buffers));
			ok = k->buffers != 0;
		}
		if (ok) {
			k->chan_b = (struct fdsp_channel *)
				    sysdep_malloc(sizeof(*k->chan_b));
			ok = k->chan_b != 0;
		}
		if (ok) {
			k->chan_a = (struct fdsp_channel *)
				    sysdep_malloc(sizeof(*k->chan_a));
			ok = k->chan_a != 0;
		}
		if (ok) {
			k->chan_b->coef = (float *)
					  sysdep_malloc(240 * sizeof(float));
			ok = k->chan_b->coef != 0;
		}
		if (ok) {
			k->chan_a->coef = (float *)
					  sysdep_malloc(240 * sizeof(float));
			ok = k->chan_a->coef != 0;
		}
		k->chan_a->offset = sRxSamplesDelay;
		k->chan_b->offset = sTxSamplesDelay;
		if (!ok) {
			FDSP_DP_Delete(k);
			k = 0;
		}
	} else if (DSPLIB_DEBUG_ON()) {
		dsplibs_debug_printf("Reinitialization requested: \n");
	}

	if (k != 0) {
		FDSP_Kernel_InitObj(k);
		k->status = (sRxSamplesDelay >= 0) ? 2 : 0;
	}
	pGlobalFDSPObj = k;
	uCorrelationReportsNo = 0;
	return k;
}
