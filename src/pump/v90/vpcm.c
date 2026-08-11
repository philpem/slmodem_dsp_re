/*
 * vpcm.c -- the V.PCM datapump's `.process`, .text 0x3e40, 1,662 bytes.
 *
 * `vpcm_run` is the only function in `dsplibs.o` that calls `modem_get_bits`
 * (0x3fea) or `modem_put_bits` (0x4153) for V.34: the other four datapumps'
 * data entries are `v32_process`, `v23_process`, `v22_process` and
 * `b103_process`, and `dp_wrapper_run` is `.process` for four other tables
 * and not for this one.  Finding 963.  So nothing in this tree carries a byte
 * of V.34 payload without it, and finding 968 measured the stronger claim:
 * the object does not SURVIVE data mode without it either, because this is
 * where the sample buffers are moved and where the payload the data branch
 * expects is fetched.
 *
 * WHAT IT ACTUALLY IS, which is not what the size suggests.  It is a
 * block-quantising buffer pair, a bit pipe and a seventeen-way status
 * dispatch; the modem is entirely inside `VPcmV34Progress`, which it calls
 * once.  That is why its closure is 268 symbols and 199,967 bytes and why
 * this file is 400 lines: the V.90, V.92 and K56Flex chains are behind one
 * call, not behind seventeen arms.  All seventeen arms ARE written here.  The
 * unwritten boundary is the five `VPcmV34*` entry points, and `vpcm.h`
 * explains how they are guarded.
 *
 * THE SHAPE, from the disassembly:
 *
 *     nproc = ((inq.count + count) / 4) * 4          quantise to four samples
 *     if inq.count > 0: append `in` to inq, and READ FROM inq INSTEAD
 *     if nproc > 0:
 *         if mute > 0:  zero nproc samples into outq, count the mute down
 *         else:
 *             modem_get_bits -> txbits, expanded byte-to-int IN PLACE
 *             in[] -> fin[] as floats
 *             prog = VPcmV34Progress(v34, fin, fout, nproc, rxbits, &nrx,
 *                                    txbits, &nbits)
 *             VPcmV34GetCleanedSamples -> modem_debug_log_data
 *             fout[] -> outq as shorts, truncating
 *             clamp nbits to 1024, keep it for next time
 *             rxbits[] -> bytes IN PLACE -> modem_put_bits
 *             dispatch on prog, then on the mode change it produced
 *     leftover of `in` -> inq;  count samples of outq -> `out`;  compact outq
 *
 * The two in-place expansions are not a liberty: 0x3ffa stores an int at
 * `0xb178(%ebx,%edx,4)` having just read a byte from `0xb178(%ebx,%edx,1)`,
 * downwards so that no store overwrites an unread byte, and 0x4120/0x412b do
 * the same upwards for the receive direction.  One 4 KB array serves both
 * widths in each direction.
 */

#include <stdlib.h>

#include "dsplib/debug.h"
#include "dsplib/dp.h"
#include "dsplib/modem_params.h"
#include "dsplib/sysdep.h"

/*
 * The five unwritten entry points are declared WEAK, so an undefined one
 * resolves to zero instead of failing the link of the 76 binaries that never
 * call this function.  See the long comment in `vpcm.h`.
 */
#define DSPLIB_VPCM_UNWRITTEN	__attribute__((weak))
#include "dsplib/vpcm.h"

/* The bit pipe.  Undefined in the object; the host supplies both. */
extern int modem_get_bits(void *m, int nbits, unsigned char *buf, int n);
extern int modem_put_bits(void *m, int nbits, const unsigned char *buf, int n);

/*
 * The datapump registry, also the host's.  `src/pump/v23/v23.c` declares the
 * same two the same way and for the same reason: there is no header for them
 * in this tree because there is none in the object either -- they are
 * undefined symbols, resolved by slmodemd's `modem.c`.
 */
extern int modem_dp_register(int id, void *op);

/*
 * `dp_param_get` -- src/core/dp_param.c, and it is `modem_get_param(modem,
 * MDMPRM_DPRUNTIME)` under another name (include/dsplib/modem_params.h).
 * `vpcm_create` calls it at 0x3ac2 WITHOUT touching the outgoing argument
 * slots, immediately after a two-argument `modem_get_param`; that is the
 * compiler leaving a dead second slot alone and not evidence of a second
 * parameter.
 */
#include "dsplib/dp_param.h"

/*
 * The K56Flex husk.  `include/dsplib/K56FlexFloModem.h` declares both, but it
 * is a C++ header (it carries a class) and this translation unit is C, so the
 * two prototypes are repeated here rather than the header being included.
 * Both are `extern "C"` free functions in the object, which is what makes the
 * call legal across the boundary.
 */
extern void *K56FLEX_Create(void *, void *, void *, int);
extern void K56FLEX_Delete(void *obj);

/*
 * `VPcmV34Create` -- .text 0xaa70, and the LAST thing `vpcm_create` does.
 * IT RETURNS 0 ON EVERY PATH (deviation D149), so the cleanup arm that tests
 * its answer is dead in the shipped object.
 */
extern int VPcmV34Create(void *obj, int side, int max_frag, void *dpRuntime,
			 int sessionType);

/*
 * ---------------------------------------------------------------------------
 * The unwritten-path record.
 *
 * `v34hshak.c`'s `t3m_notwritten` verbatim in shape, and for its reasons:
 * ALWAYS record a code, and ALWAYS stop unless a test has said by name that
 * it is going to read the code afterwards.  An entry point that returned
 * quietly would leave `vpcm_run` running and carrying nothing, which is
 * exactly what a subtle defect looks like -- and a `.process` is the worst
 * place in the object for that, because the caller's only evidence is a
 * buffer of samples that would be silence either way.
 *
 * The code is a code and not a string because `tools/debugaudit.py
 * --invented` holds every literal in `src/` against the object's `.rodata`,
 * so a diagnostic phrase this tree made up cannot live here at all (findings
 * 180 and 201).  The names are in the test.
 */
static int vpcm_unwritten_code;
static int vpcm_unwritten_soft;

int
vpcm_unwritten(void)
{
	return vpcm_unwritten_code;
}

void
vpcm_unwritten_reset(void)
{
	vpcm_unwritten_code = VPCM_WRITTEN;
	vpcm_unwritten_soft = 1;
}

static void
vpcm_notwritten(int what)
{
	if (vpcm_unwritten_code == VPCM_WRITTEN)
		vpcm_unwritten_code = what;
	if (!vpcm_unwritten_soft)
		abort();
}

/*
 * ---------------------------------------------------------------------------
 * One buffer through the V.PCM datapump.
 */
int
vpcm_run(struct dp *dp, void *in_v, void *out_v, int count)
{
	struct vpcm_root *s = (struct vpcm_root *)dp->dp_data;
	short *in = (short *)in_v;
	short *out = (short *)out_v;
	int status = DPSTAT_OK;
	int pending = s->inq.count;
	/*
	 * 0x3e6e-0x3e7d, and the `if (edx < 0) eax = edx + 3` fixup at 0x4187
	 * is what says this is a signed division by four and not a mask: a
	 * mask would need no correction.  It cannot go negative on any input
	 * the host produces, and the object still pays for the branch.
	 */
	int nproc = ((pending + count) / 4) * 4;

	/*
	 * THE INPUT PARAMETER IS REBOUND, and 0x3eac is where.  When the queue
	 * already holds samples the new block is appended to them and
	 * everything downstream -- the float conversion at 0x4008 and the
	 * leftover copy at 0x3f36 -- reads the QUEUE, not the caller's buffer.
	 *
	 * UNEXERCISED, and recorded rather than assumed: with the host's
	 * `count` of 48 and the queue starting empty, `nproc` is 48 and the
	 * leftover is zero for ever, so no test in this tree has ever had
	 * `pending > 0`.  It is written from the disassembly and it is not
	 * proved by anything green.
	 */
	if (pending > 0) {
		sysdep_memcpy(&s->inq.buf[pending], in,
			      (size_t)count * sizeof(short));
		in = s->inq.buf;
	}

	if (nproc > 0) {
		short *outp = &s->outq.buf[s->outq.count];
		int newstat = DPSTAT_OK;
		int mode = s->mode;

		/*
		 * The mute counter, root +0xd250: `vpcm_create` seeds it from
		 * the runtime block's bit 4 and the line is silenced until it
		 * runs out.
		 *
		 * EXERCISED ON EVERY CALL, and this comment said the opposite
		 * until finding 1002 measured it.  `vpcm_create` seeds it at
		 * 528 -- exactly eleven 48-sample blocks -- so blocks 0 to 10
		 * of every call take this arm and `VPcmV34Progress` is not
		 * called at all in them.  Bisected with aborting probes:
		 * `mute > 0` aborts, `mute > 480` aborts, `mute > 528`
		 * survives, and `mute % 48 != 0` survives.
		 *
		 * Nothing downstream noticed because it is only ever positive
		 * while `status` and `mode` are both still zero -- the mute
		 * ends 1,580 blocks before the connect.  That is also why
		 * "the mute path is always taken" fails 78 checks rather than
		 * something narrower: the branch is real and driven, and only
		 * the countdown stops it swallowing the whole call.
		 */
		if (s->mute > 0) {
			int left;

			sysdep_memset(outp, 0, (size_t)nproc * sizeof(short));
			left = s->mute - nproc;
			s->mute = left > 0 ? left : 0;
		} else {
			int nbits = s->nbits;
			int nrx = 0;
			int ncleaned = 0;
			void *cleaned = 0;
			int prog = 0;
			int i;

			/*
			 * `modem_get_bits` fills BYTES and the modulator wants
			 * INTS, so the array is walked downwards and widened
			 * in place.  0x3ff6-0x4002.
			 */
			if (nbits > 0) {
				nbits = modem_get_bits(dp->modem, 1,
						       (unsigned char *)
						       s->txbits, nbits);
				for (i = nbits - 1; i >= 0; i--)
					s->txbits[i] = ((unsigned char *)
							s->txbits)[i];
			}

			for (i = 0; i < nproc; i++)
				s->fin[i] = in[i];

			if (VPcmV34Progress == 0)
				vpcm_notwritten(VPCM_UNWRITTEN_PROGRESS);
			else
				prog = VPcmV34Progress(&s->v34, s->fin, s->fout,
						       nproc, s->rxbits, &nrx,
						       s->txbits, &nbits);

			if (VPcmV34GetCleanedSamples == 0)
				vpcm_notwritten(VPCM_UNWRITTEN_CLEANED);
			else
				cleaned = VPcmV34GetCleanedSamples(&s->v34,
								   &ncleaned);
			if (cleaned != 0 && ncleaned > 0)
				modem_debug_log_data(dp->modem, 3, cleaned,
						     ncleaned
						     * (int)sizeof(short));

			/*
			 * `fnstcw` / `or $0xc00` / `fistps` at 0x40b4-0x40e2 is
			 * GCC's cast to integer: round toward zero, not to
			 * nearest.
			 */
			for (i = 0; i < nproc; i++)
				outp[i] = (short)s->fout[i];

			/*
			 * `cmp $0x400,%eax; jbe` at 0x40ef is UNSIGNED where
			 * every other test on this variable -- 0x3fca's `jle`,
			 * the `jns` closing the widening loop -- is signed.
			 * The comparison is against an unsigned bound, so it is
			 * spelled as one here; it is a forced encoding and not
			 * a free choice.
			 */
			if ((unsigned)nbits > VPCM_MAX_BITS) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"vpcm: too many bits "
						"requested (%d)\n", nbits);
				nbits = (int)VPCM_MAX_BITS;
			}
			s->nbits = nbits;

			/* And the same widening backwards, one bit per byte. */
			if (nrx > 0) {
				unsigned char *rb =
					(unsigned char *)s->rxbits;

				for (i = 0; i < nrx; i++)
					rb[i] = (unsigned char)
						(s->rxbits[i] & 1);
				modem_put_bits(dp->modem, 1, rb, nrx);
			}

			/*
			 * ---------------------------------------------------
			 * The dispatch, .rodata+0x164, seventeen entries.
			 *
			 * It only runs when the code CHANGED; an unchanged
			 * code 0 counts towards the training timeout instead.
			 */
			if (s->status != prog) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"vpcm: New status %d\n", prog);

				switch (prog) {
				case VPCM_PROG_RESTART_P2:
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
							"vpcm: Re-starting "
							"phase II\n");
					s->stall = 0;
					mode = VPCM_MODE_IDLE;
					/*
					 * Give back the delay the phase-II
					 * completion took, once, and only if
					 * it was taken.
					 */
					if (s->params->addedDelay > 0
					    && s->extradelay != 0) {
						modem_set_param(dp->modem,
							MDMPRM_UPDATE_DELAY,
							-s->extradelay);
						s->params->addedDelay = 0;
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
								"vpcm: P2 RESTART: decrease delay!! init %d, ext %d, add %d\n",
								s->params->hwDelay,
								s->params->dmaDelay,
								s->params->addedDelay);
					}
					break;

				case VPCM_PROG_P2_DONE:
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
							"vpcm: Phase II "
							"completed !!!\n");
					if (s->extradelay != 0
					    && s->params->addedDelay == 0) {
						modem_set_param(dp->modem,
							MDMPRM_UPDATE_DELAY,
							s->extradelay);
						s->params->addedDelay =
							s->extradelay;
						if (DSPLIB_DEBUG_ON())
							dsplibs_debug_printf(
								"vpcm: P2 FINISHED: increase delay!! init %d, ext %d, add %d\n",
								s->params->hwDelay,
								s->params->dmaDelay,
								s->extradelay);
					}
					break;

				case VPCM_PROG_CONNECT_A:
				case VPCM_PROG_CONNECT_B:
					mode = VPCM_MODE_CONNECTED;
					break;

				case VPCM_PROG_IDLE_A:
				case VPCM_PROG_IDLE_B:
					mode = VPCM_MODE_IDLE;
					break;

				case VPCM_PROG_FAIL_A:
				case VPCM_PROG_FAIL_B:
				case VPCM_PROG_FAIL_C:
					mode = VPCM_MODE_ERROR;
					break;

				case VPCM_PROG_SAME_LINE:
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
							"vpcm: Same Line "
							"Verification "
							"Status\n");
					break;

				default:
					break;
				}
			} else if (prog == VPCM_PROG_RESTART_P2) {
				/*
				 * 3,000 blocks -- 144,000 samples, fifteen
				 * seconds -- of the handshake reporting no
				 * progress at all.  The counter is not reset
				 * here, so every block after the deadline
				 * fails again.
				 */
				s->stall++;
				if (s->stall > VPCM_TRAIN_TIMEOUT) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
							"vpcm: train "
							"timeout!\n");
					mode = VPCM_MODE_ERROR;
				}
			}
			s->status = prog;

			/*
			 * ---------------------------------------------------
			 * And what the mode CHANGE means to the host.  Only a
			 * transition is reported; a mode that stays 1 does not
			 * connect twice.
			 */
			if (s->mode != mode) {
				if (mode == VPCM_MODE_ERROR) {
					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
							"vpcm: Link Error\n");
					/*
					 * -1, not DPSTAT_ERROR: 0x42b8 is
					 * `mov $0xffffffff`.  Recorded because
					 * it is the object's value and not a
					 * DPSTAT_* code at all.
					 */
					newstat = -1;
				} else if (mode == VPCM_MODE_CONNECTED) {
					int dpid = 0;
					int rxrate = 0;
					int txrate = 0;

					if (VPcmV34GetCurrentSessionDP == 0)
						vpcm_notwritten(
						    VPCM_UNWRITTEN_SESSIONDP);
					else
						dpid =
						  VPcmV34GetCurrentSessionDP(
							&s->v34);
					if (VPcmV34GetCurrentRxBitRate == 0)
						vpcm_notwritten(
						    VPCM_UNWRITTEN_RXBITRATE);
					else
						rxrate =
						  VPcmV34GetCurrentRxBitRate(
							&s->v34);
					if (VPcmV34GetCurrentTxBitRate == 0)
						vpcm_notwritten(
						    VPCM_UNWRITTEN_TXBITRATE);
					else
						txrate =
						  VPcmV34GetCurrentTxBitRate(
							&s->v34);

					if (DSPLIB_DEBUG_ON())
						dsplibs_debug_printf(
							"vpcm: Link: DP is V.%d, rate: rx %d, tx %d\n",
							dpid, rxrate, txrate);

					/*
					 * The datapump id the host sees is
					 * rewritten to whichever standard the
					 * session settled on.  V.34 is the
					 * fallback for everything that is not
					 * one of the other two -- 0x4422 is
					 * the `else`, not a third compare.
					 *
					 * WRITTEN AND UNEXERCISED: the V.34
					 * call this tree drives reports 34,
					 * so the 0x5a and 0x5c stores are
					 * read out of the object and are not
					 * covered by any test here.
					 */
					if (dpid == VPCM_DP_V90)
						s->dp.id = VPCM_DP_V90;
					else if (dpid == VPCM_DP_V92)
						s->dp.id = VPCM_DP_V92;
					else
						s->dp.id = VPCM_DP_V34;

					/*
					 * `s->dp.modem` and not `dp->modem`:
					 * 0x4439 and 0x4455 reach the handle
					 * through the ROOT while every other
					 * call in this function reaches it
					 * through the argument.  The two are
					 * the same pointer -- `vpcm_create`
					 * stores the root into its own +0x10
					 * -- so this is the object's spelling
					 * preserved, not a difference.
					 */
					modem_set_param(s->dp.modem,
							MDMPRM_TX_RATE, txrate);
					modem_set_param(s->dp.modem,
							MDMPRM_RX_RATE, rxrate);
					newstat = DPSTAT_CONNECT;
				}
			}
			s->mode = mode;

			if (nbits == 0 && mode == VPCM_MODE_CONNECTED) {
				if (DSPLIB_DEBUG_ON())
					dsplibs_debug_printf(
						"vpcm: No symbols "
						"Requested!!!\n");
			}

			if (newstat != DPSTAT_OK)
				status = newstat;
		}
	}

	/*
	 * The tail, 0x3f19-0x3fb6, and it runs whether or not anything was
	 * processed.  Three copies: what `in` had left over goes to the front
	 * of the input queue, `count` samples come off the front of the output
	 * queue into the caller's buffer, and the output queue is compacted.
	 */
	pending = s->inq.count;
	if (pending + count != nproc)
		sysdep_memcpy(s->inq.buf, in + nproc,
			      (size_t)(pending + count - nproc)
			      * sizeof(short));
	s->inq.count = pending + count - nproc;

	sysdep_memcpy(out, s->outq.buf, (size_t)count * sizeof(short));
	sysdep_memcpy(s->outq.buf, &s->outq.buf[count],
		      (size_t)(s->outq.count + nproc - count) * sizeof(short));
	s->outq.count = s->outq.count + nproc - count;

	return status;
}

/*
 * ---------------------------------------------------------------------------
 * Layout, pinned.  Every one of these is an offset `vpcm_run` or
 * `vpcm_create` addresses literally in the disassembly, and the whole point
 * of the struct is that they land where the blob's do.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define VPCM_ASSERT(name, field, off) \
	typedef char vpcm_off_##name[ \
		((int)__builtin_offsetof(struct vpcm_root, field) == (off)) \
		? 1 : -1]

VPCM_ASSERT(id,	    dp.id,	0x00000);	/* 0x4422 movl $0x22,(%ebx) */
VPCM_ASSERT(modem,  dp.modem,	0x00004);	/* 0x4439 mov 0x4(%ebx)     */
VPCM_ASSERT(data,   dp.dp_data,	0x00010);	/* 0x3e55 mov 0x10(%esi)    */
VPCM_ASSERT(status, status,	0x00014);	/* 0x415b cmp 0x14(%ebx)    */
VPCM_ASSERT(mode,   mode,	0x00018);	/* 0x3edb mov 0x18(%ebx)    */
VPCM_ASSERT(nbits,  nbits,	0x0001c);	/* 0x3fbd mov 0x1c(%ebx)    */
VPCM_ASSERT(stall,  stall,	0x00020);	/* 0x4270 mov 0x20(%ebx)    */
VPCM_ASSERT(info,   info,	0x00024);	/* 0x3abf mov %eax,0x24     */
VPCM_ASSERT(params, params,	0x00028);	/* 0x41a6 mov 0x28(%ebx)    */
VPCM_ASSERT(v34,    v34,	0x0002c);	/* 0x403b lea 0x2c(%ebx)    */
VPCM_ASSERT(fin,    fin,	0x0ac78);	/* 0x400f fstps 0xac78      */
VPCM_ASSERT(fout,   fout,	0x0aef8);	/* 0x40d0 flds  0xaef8      */
VPCM_ASSERT(txbits, txbits,	0x0b178);	/* 0x3ffa mov ...,0xb178    */
VPCM_ASSERT(rxbits, rxbits,	0x0c178);	/* 0x4120 movzbl 0xc178     */
VPCM_ASSERT(outq,   outq,	0x0d178);	/* 0x3eba mov 0xd178(%ebx)  */
VPCM_ASSERT(inq,    inq,	0x0d1e4);	/* 0x3e68 mov 0xd1e4(%ebx)  */
VPCM_ASSERT(mute,   mute,	0x0d250);	/* 0x3ed5 mov 0xd250(%ebx)  */
VPCM_ASSERT(deladd, extradelay,	0x0d254);	/* 0x419c mov 0xd254(%ebx)  */

/* `movl $0xd258,(%esp)` at 0x3a42, the whole allocation. */
typedef char vpcm_root_size[(sizeof(struct vpcm_root) == 0xd258) ? 1 : -1];

/*
 * The queues' capacity is what is left between them and what follows, so it
 * is a consequence of the offsets above rather than an independent claim --
 * asserted anyway, because a `short buf[48]` would fit every test in this
 * tree and still be wrong.
 */
typedef char vpcm_queue_size[(sizeof(struct vpcm_queue) == 0x6c) ? 1 : -1];

/* Both bit arrays are exactly the clamp, 1024 entries of four bytes. */
typedef char vpcm_bits_size[
	(sizeof(((struct vpcm_root *)0)->txbits) == VPCM_MAX_BITS * 4
	 && sizeof(((struct vpcm_root *)0)->rxbits) == VPCM_MAX_BITS * 4)
	? 1 : -1];

#endif /* 32-bit */

/*
 * ===========================================================================
 * `vpcm_create` -- 0x3a00, 0x3c9 = 969 bytes.
 * ===========================================================================
 *
 * THREE GUARDS, ONE ALLOCATION AND ONE CLEANUP LADDER.  The guards return
 * NULL through the same epilogue -- 0x3c7f is reached from the `srate`
 * mismatch, the `max_frag` cap and the failed `sysdep_malloc` alike, with
 * `%ecx` zeroed before each jump -- so the three are one `return 0` in the
 * source and not three.
 *
 * `srate` MUST BE EXACTLY 9600 (`cmp $0x2580,%esi; jne`) and `max_frag` must
 * be at most 48 (`cmpl $0x30; jg`).  48 is `MODEM_FRAG`, so the guard value
 * is the host's real value and not a bound (docs/configuration.md).
 *
 * THE ROOT IS ITS OWN `struct dp` and stores its own address at +0x10; see
 * vpcm.h.  It is memset to zero over its whole 0xd258 before any field is
 * written, which is why the three explicit `= 0`s below are worth keeping:
 * they are separate stores in the object at 0x3a9c, 0x3aa3 and 0x3aaa, after
 * the memset, so they are in the source too.
 *
 * `VPcmV34Create` RETURNS ZERO ON EVERY PATH, so the ladder's last rung is
 * dead code in the shipped object -- deviation D149, already recorded.  It is
 * written anyway because the object has it and because the deadness is a
 * property of the callee and not of this function.
 *
 * ---------------------------------------------------------------------------
 * THE MUTE COUNTER'S TEST IS INVERTED FROM WHAT IT LOOKS LIKE
 *
 *     0x3bd4  cmp $0x1,%ecx
 *     0x3bd7  sbb %edi,%edi
 *     0x3bd9  and $0x210,%edi
 *
 * `sbb %edi,%edi` leaves -1 when the carry is set and 0 when it is not, and
 * `cmp $1` sets the carry when the value is BELOW one.  So the mask survives
 * when the V.92 bit is CLEAR and is discarded when it is set: a modem that is
 * NOT doing V.92 mutes its first 528 samples, and one that is does not.  The
 * reading that the shape suggests -- mute when the bit is set -- is the wrong
 * way round, and a test that swept only one value of that bit would not tell
 * the two apart.
 *
 * ---------------------------------------------------------------------------
 * THE DELAY ARITHMETIC
 *
 * `modem_get_param(modem, MDMPRM_IODELAY) + 4` is the hardware delay; the DMA
 * delay is that less 0x30, plus whatever `extradelay` holds.  If the hardware
 * delay EXCEEDS 0xf4 the function first tells the host so
 * (`modem_set_param(modem, 13, 0xf4 - d)`), clamps `extradelay` to at least
 * 0x180 -- `neg` then `cmp $0x180; jge` on the NEGATED difference, so the
 * stored value is a positive number of samples -- and then redoes the
 * arithmetic with 0xf4 in place of the measured delay.  The re-entry at
 * 0x3c03 with `%edx` = 0xf4 is what says the second pass uses the cap and not
 * the original.
 */
struct dp *
vpcm_create(void *modem, int id, int caller, int srate, int max_frag,
	    struct dp_operations *op)
{
	struct vpcm_root *s;
	unsigned int qcFlags;
	int side, nsamples, minRate, maxRate, sessionType, hwDelay;

	side = (caller == 0);

	if (srate != VPCM_SRATE)
		return 0;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "vpcm: create: dp %d, caller %d, frag %d (size %d).\n",
		    id, caller, max_frag, (int)sizeof(struct vpcm_root));

	if (max_frag > VPCM_MAX_FRAG)
		return 0;

	s = (struct vpcm_root *)sysdep_malloc(sizeof(struct vpcm_root));
	if (s == 0)
		return 0;

	sysdep_memset(s, 0, sizeof(struct vpcm_root));

	s->dp.dp_data = s;
	s->outq.count = 4;
	s->dp.modem = modem;
	s->dp.id = id;
	s->dp.op = op;
	s->status = 0;
	s->mode = VPCM_MODE_IDLE;
	s->stall = 0;

	s->info = (struct dsp_info *)modem_get_param(modem, MDMPRM_DSPINFO);
	s->params = (struct _tagModemParameters *)dp_param_get(modem);
	s->params->paramFile = 0;

	/*
	 * Milliseconds, and `VPCMXF_Create` turns it back into samples with
	 * the OTHER rate when it is told to.  48 samples at 9600 is 5 ms.
	 */
	nsamples = (max_frag * 1000) / srate;

	s->v34.xf = VPCMXF_Create(0, &s->v34, s->params, (unsigned int)nsamples,
				  0);
	if (s->v34.xf == 0) {
		sysdep_free(s);
		return 0;
	}

	s->v34.k56 = K56FLEX_Create(0, &s->v34, s->params, nsamples);
	if (s->v34.k56 == 0) {
		VPCMXF_Delete(s->v34.xf);
		sysdep_free(s);
		return 0;
	}

	minRate = modem_get_param(modem, MDMPRM_MIN_RATE);
	maxRate = modem_get_param(modem, MDMPRM_MAX_RATE);
	if ((unsigned int)maxRate > VPCM_MAX_RATE_CAP)
		maxRate = (int)VPCM_MAX_RATE_CAP;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("vpcm: VPCM rate limits: %d-%d\n",
				     minRate, maxRate);

	s->params->vpcmRateLimitLow = (unsigned int)minRate;
	s->params->vpcmRateLimitHigh = (unsigned int)maxRate;
	s->params->minRate = VPCM_PARAM_MIN_RATE;
	s->params->maxRate = VPCM_PARAM_MAX_RATE;

	/*
	 * The V.92 bit of `qcFlags` survives only when the caller asked for
	 * the V.92 datapump; every other id clears it.  Then bit 5 is cleared
	 * unconditionally.  Two read-modify-writes of the same byte, in that
	 * order, both re-loading it.
	 */
	if (id == VPCM_DP_V92)
		qcFlags = (s->params->qcFlags >> 4) & 1;
	else
		qcFlags = 0;

	s->params->qcFlags = (unsigned char)
	    ((s->params->qcFlags & 0xef) | ((qcFlags & 1) << 4));
	s->params->qcFlags &= (unsigned char)0xdf;

	/* See the file comment: the mask survives when the bit is CLEAR. */
	s->mute = (((s->params->qcFlags >> 4) & 1) < 1) ? VPCM_MUTE_SAMPLES : 0;

	hwDelay = modem_get_param(modem, MDMPRM_IODELAY) + 4;
	if (VPCM_DELAY_CAP - hwDelay < 0) {
		int excess;

		modem_set_param(modem, MDMPRM_UPDATE_DELAY,
				VPCM_DELAY_CAP - hwDelay);
		excess = -(VPCM_DELAY_CAP - hwDelay);
		if (excess < VPCM_EXTRADELAY_MIN)
			excess = VPCM_EXTRADELAY_MIN;
		s->extradelay = excess;
		hwDelay = VPCM_DELAY_CAP;
	}

	/*
	 * THE DMA DELAY IS STORED TWICE, and the first store is not dead:
	 * 0x3c09 writes `hwDelay - 0x30` into +0x68, 0x3c0c writes `hwDelay`
	 * into +0x64, and 0x3c1e writes +0x68 again with `%eax` -- the same
	 * register, not a reload -- plus `extradelay`.  A compiler that had
	 * been given one expression would emit one store; a `+=` whose left
	 * side it has just written keeps the value in a register and emits
	 * two.  So the three statements below are the source's three.
	 */
	s->params->dmaDelay = hwDelay - 0x30;
	s->params->hwDelay = hwDelay;
	s->params->dmaDelay += s->extradelay;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("vpcm: Delays: HW %d, DMA %d\n",
				     s->params->hwDelay, s->params->dmaDelay);

	s->params->addedDelay = 0;

	if (id == VPCM_DP_V92)
		sessionType = 2;
	else
		sessionType = (id == VPCM_DP_V90);

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
		    "vpcm: initial dp V.%d, session type %d.\n",
		    id, sessionType);

	/* D149: this returns 0 on every path, so the arm below is dead. */
	if (VPcmV34Create(&s->v34, side, max_frag, s->params, sessionType)
	    != 0) {
		VPCMXF_Delete(s->v34.xf);
		K56FLEX_Delete(s->v34.k56);
		sysdep_free(s);
		return 0;
	}

	return &s->dp;
}

/*
 * ===========================================================================
 * `vpcm_delete` -- 0x3dd0, 0x6e = 110 bytes.
 * ===========================================================================
 *
 * TWO WORDS COPIED OUT THROUGH `dsp_info`, AND THEY ARE THE ONLY THING THIS
 * FUNCTION PRODUCES BESIDES FREED MEMORY.  `connectionType` (+0x48) and
 * `clockDeviation` (+0x4c) of the runtime block go to the host's record at
 * its +0x00 and +0x04, in that order in the source and the reverse in the
 * object -- 0x3dea reads +0x4c and stores it at +0x04 first, then 0x3df0
 * reads +0x48 for +0x00.  Two independent stores, so the order between them
 * is the compiler's.
 *
 * NO NULL TESTS ANYWHERE.  `dp`, `dp->dp_data`, `params` and `info` are all
 * dereferenced unguarded, which is why a harness that leaves `MDMPRM_DSPINFO`
 * at a default faults here and not in `vpcm_create` (docs/configuration.md).
 */
int
vpcm_delete(struct dp *dp)
{
	struct vpcm_root *s = (struct vpcm_root *)dp->dp_data;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("vpcm: delete...\n");

	s->info->connection_type = (unsigned int)s->params->connectionType;
	s->info->clock_deviation = s->params->clockDeviation;

	VPCMXF_SessionTermination(s->v34.xf);
	VPCMXF_Delete(s->v34.xf);
	K56FLEX_Delete(s->v34.k56);
	sysdep_free(s);

	return 0;
}

/*
 * ===========================================================================
 * `vpcm_op` -- .data+0x30, 24 bytes, and `dp_vpcm_init` -- 0x44c0, 72 bytes.
 * ===========================================================================
 *
 * The table is `name`, `use_count`, `create`, `destroy`, `process`, `hangup`
 * and its bytes are 0x47c into .rodata.str1.1 ("VPCM"), 0, 0x3a00, 0x3dd0,
 * 0x3e40 and 0 -- so `process` is `vpcm_run` DIRECTLY and not
 * `dp_wrapper_run`, which is what distinguishes this datapump from V.23 and
 * Bell 103 (see src/pump/v23/v23.c on why those two go through the wrapper).
 * `use_count` and `hangup` are zero and are left unwritten here for the same
 * reason they are there.
 *
 * `dp_vpcm_init` registers ONE table under THREE ids, 0x22, 0x5a and 0x5c,
 * which is 34, 90 and 92: V.34, V.90 and V.92 are one datapump, and which of
 * them a session is going to be is `vpcm_create`'s `id` argument and then
 * `vpcm_run`'s to change.  It returns 0 unconditionally.
 */
struct dp_operations vpcm_op = {
	.name = "VPCM",
	.create = vpcm_create,
	.destroy = vpcm_delete,
	.process = vpcm_run
	/* use_count and hangup are zero */
};

int
dp_vpcm_init(void)
{
	modem_dp_register(VPCM_DP_V34, &vpcm_op);
	modem_dp_register(VPCM_DP_V90, &vpcm_op);
	modem_dp_register(VPCM_DP_V92, &vpcm_op);
	return 0;
}
