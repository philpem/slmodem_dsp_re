/*
 * call.h -- the call-setup datapump.
 *
 * This is the datapump that runs *before* a connection exists: it dials,
 * listens for what the line says back, and hands over to a real modem
 * datapump the moment it hears an answer.  Everything it does is
 * `CALLPROG_Progress`; the rest of this module is the plumbing that gets
 * samples in and out of it at the size it wants.
 *
 * It registers itself under `DP_CALL` from `dp_call_init`, and the operations
 * table is the only thing here with external linkage in the original -- the
 * four functions behind it are file statics, so a test reaches them by
 * calling `dp_call_init` and taking the pointers out of the registration.
 */

#ifndef DSPLIB_CALL_H
#define DSPLIB_CALL_H

#include "dsplib/dp.h"
#include "dsplib/callprog_state.h"
#include "dsplib/fixedrc.h"

/*
 * The fragment queues.  Each is a 192-byte ring the caller's samples flow
 * through, plus a 96-byte double buffer the DSP works in: `active` picks
 * which half of the latter is live, and it flips every block.  Samples in,
 * bytes for the offsets -- the original mixes the two units freely and the
 * names here say which is which.
 */
/*
 * The id this datapump registers under.  The others live in b103.h, which is
 * where the original happens to put them.
 */
#define DP_CALL			2

#define CALL_RING_BYTES		192	/* 96 samples             */
#define CALL_BLOCK_BYTES	96	/* 48 samples per block   */
#define CALL_BLOCK_SAMPLES	48
#define CALL_SCRATCH_SAMPLES	160	/* what a resampler may return */

struct call_queue {
	int	count;			/* +0x00  bytes held        */
	int	head;			/* +0x04  write offset      */
	int	tail;			/* +0x08  read offset       */
	int	active;			/* +0x0c  0 or 96           */
	short	data[CALL_RING_BYTES / 2];	/* +0x10 */
};

/*
 * The datapump object.  1484 bytes, allocated and zeroed by `call_create`.
 * `self` at +0x10 is what every entry point dereferences: the wrapper hands
 * back a `struct dp *` whose +0x10 is this same pointer.
 */
struct call_dp {
	int			id;		/* +0x000 */
	void			*modem;		/* +0x004 */
	int			f08;		/* +0x008 */
	struct dp_operations	*op;		/* +0x00c */
	struct call_dp		*self;		/* +0x010 */

	/* Only built when the host's rate is not 8 kHz. */
	struct rc		*rc_in;		/* +0x014 */
	struct rc		*rc_out;	/* +0x018 */

	/* Where the resamplers put their results. */
	short			from_host[CALL_SCRATCH_SAMPLES];	/* +0x01c */
	short			to_host[CALL_SCRATCH_SAMPLES];		/* +0x15c */

	struct call_queue	out_q;		/* +0x29c  towards the host */
	struct call_queue	in_q;		/* +0x36c  from the host    */

	/*
	 * Set when the supervisor reports an answer.  From then on the
	 * incoming block is discarded rather than processed -- the call is
	 * over and another datapump is taking the line.
	 */
	int			answered;	/* +0x43c */

	int			message;	/* +0x440  the last one     */

	struct callprog		callprog;	/* +0x444 */

	/* Read once at create, for the pulse dialler. */
	int			pulse_make;	/* +0x5b4 */
	int			pulse_break;	/* +0x5b8 */
	int			f5bc;		/* +0x5bc */
	int			pad[3];
};

/* Register the datapump.  Called from prop_dp_init. */
/*
 * The three dp_operations entry points, and the S-register adaptor.
 *
 * File-static in the object -- the ops table is the only thing in call.c with
 * external linkage there -- and static here too until finding F221 gave the
 * object's copies `ref_` aliases.  Declared so a test can call both sides by
 * name instead of reaching them through what `dp_call_init` registers.
 * `call_run` is `.process`, with no dp_wrapper in between: this datapump does
 * its own rate conversion.
 */
struct dp *call_create(void *modem, int id, int caller, int srate,
		       int max_frag, struct dp_operations *op);
int call_delete(struct dp *dp);
int call_run(struct dp *dp, void *in, void *out, int count);
long call_GetSRegister(void *modem, unsigned short num);

void dp_call_init(void);
void dp_call_exit(void);

#endif /* DSPLIB_CALL_H */
