/**
 * @file fax.h
 * @brief The FAX service's own context object, `struct fax_ctx`, and the
 *        four entry points that share it: #FAX_create, #FAX_delete,
 *        #FAX_class1_command and #FAX_process.
 *
 * All four sit in the object right after the `VOICE_*` group and before the
 * ring detector -- `nm -S --size-sort` over `.text 0x1450..0x2121` shows
 * FAX_delete, FAX_create, FAX_class1_command, FAX_process, then RD_create --
 * and are reconstructed in `src/service/voice.c` itself, alongside the
 * unrelated ring-detector and voice entry points that merely happen to
 * share the translation unit. This header holds the FAX side's own type,
 * separate from `struct voice_ctx` (`voice.h`).
 *
 * `struct fax_ctx` is not `struct fax_class1` (`class1.h`). It is the outer
 * session object #FAX_create allocates and #FAX_delete frees, which itself
 * owns one `struct fax_class1 *` (at +0x4) -- the T.30/Class-1 state machine
 * `class1.c`/`class1rx.c`/`class1tx.c` model -- plus two resampler handles
 * (`struct rc *`, `fixedrc.h`) that bridge the host's rate to the pump's own.
 *
 * All four entry points and the struct's fields established from them are
 * now written; see `src/service/voice.c`'s own banner on each function for
 * the full derivation (findings F10105, F10106, F10121) and the struct/
 * prototype comments below for what each field is.
 *
 * `sizeof` is not settled: #FAX_create allocates `0x28bc` bytes
 * (`movl $0x28bc,(%esp)` at 0x151b, ahead of `sysdep_malloc`), larger than
 * every offset this batch reaches, so the struct extends past what is
 * modelled here. #FAX_MODELLED_BYTES is a modelling bound, not a size
 * claim, the same convention `class1.h`'s `CLASS1_MODELLED_BYTES` uses.
 */

#ifndef DSPLIB_FAX_H
#define DSPLIB_FAX_H

struct fax_class1;
struct rc;

struct fax_ctx {
	void *modem;			/* +0x000 handed to modem_recv_from_tty/
					 * modem_send_to_tty as their own first
					 * argument (evidence class 2, matching
					 * their declared shapes and
					 * FAX_create's own first argument;
					 * F10106)                            */
	struct fax_class1 *class1;	/* +0x004 the Class 1 session,
					 * created by FAX_create's own
					 * fax_class1_create(NULL, &local)
					 * call, torn down by FAX_delete's own
					 * fax_class1_delete call             */
	int host_rx_enable;		/* +0x008 gates whether FAX_process
					 * polls modem_recv_from_tty at all
					 * this cycle: set to 1 only by the
					 * FAX_CLASS1_CONNECT dispatch case,
					 * cleared by every other case that
					 * touches it (usage inference, F10106) */
	int host_rx_want;		/* +0x00c byte count to request from
					 * modem_recv_from_tty next cycle
					 * (clamped to 0x1000), overwritten
					 * after every fax_class1_progress call
					 * from that call's own out-pointer --
					 * so its steady-state meaning is "what
					 * fax_class1_progress wants read
					 * next" (usage inference, F10106)    */
	unsigned char host_tx_buf[0x1000]; /* +0x010 the host-bound byte
					 * stream fax_class1_progress fills for
					 * FAX_process to forward via
					 * modem_send_to_tty; sized by the exact
					 * 0x1000 gap to host_rx_buf (F10106)  */
	unsigned char host_rx_buf[0x1000]; /* +0x1010 modem_recv_from_tty's
					 * destination buffer and
					 * fax_class1_progress's input; sized
					 * by the 0x1000 read clamp and the
					 * exact gap to rc_a (F10106)          */
	struct rc *rc_a;		/* +0x2010 resampler handle, checked
					 * and deleted before rc_b in
					 * FAX_delete's own order            */
	struct rc *rc_b;		/* +0x2014 resampler handle          */
	short rx_resampled[0xa0];	/* +0x2018 RcFixed_Resample's `out`
					 * target for rc_a: the pump-rate rx
					 * buffer handed to
					 * fax_class1_progress (F10106)        */
	short tx_pump_rate[0xa0];	/* +0x2158 fax_class1_progress's `tx`
					 * target when rc_a != NULL (else
					 * written straight into out_ring),
					 * then RcFixed_Resample's `in` for
					 * rc_b (F10106)                       */
	int host_frame_samples;	/* +0x2298 the sample count FAX_process
					 * batches per host-rate chunk, and
					 * RcFixed_Resample's in_count/out_count
					 * for both calls.  Rank-1 evidence:
					 * FAX_process's own debug string is
					 * "fax: process: samples count %d !=
					 * %d" (.rodata.str1.4 0x350), printed
					 * with this field and the literal
					 * 0xa0 (CLASS1_BLOCK_SAMPLES,
					 * class1.h) (F10106)                  */
	int out_produced;		/* +0x229c running total incremented by
					 * host_frame_samples once per flush
					 * cycle; nothing reads it back (usage
					 * inference, F10106)                  */
	unsigned char pad_22a0[4];	/* +0x22a0 unmodelled                 */
	int out_read_cursor;		/* +0x22a4 extraction cursor into
					 * out_ring (samples, wraps mod
					 * 2*host_frame_samples), advanced
					 * every inner iteration regardless of
					 * whether it flushed (usage inference,
					 * F10106)                             */
	int out_write_half;		/* +0x22a8 a 0/host_frame_samples
					 * ping-pong selector toggled once per
					 * flush cycle, picking which half of
					 * out_ring the next flush's tx-
					 * direction data lands in (usage
					 * inference, F10106)                  */
	short out_ring[2 * 0xa0];	/* +0x22ac the double-buffered ring
					 * fax_class1_progress (identity path)
					 * or the rc_b RcFixed_Resample call
					 * (resampled path) writes host-rate tx
					 * samples into, and FAX_process's own
					 * output is drained from.  2 *
					 * host_frame_samples shorts, matching
					 * in_ring's size; ends +0x252c, 0x80
					 * bytes short of in_pending (F10106)  */
	unsigned char pad_252c[0x25ac - 0x252c]; /* +0x252c unmodelled --
					 * `in_pending` is pinned to +0x25ac by
					 * the object's own `add $0x25ac,%eax`,
					 * leaving this gap unclaimed         */
	int in_pending;			/* +0x25ac samples buffered in in_ring
					 * awaiting a flush; incremented per
					 * inner-loop copy-in, decremented by
					 * host_frame_samples once per flush
					 * (usage inference, F10106)           */
	int in_write_cursor;		/* +0x25b0 fill cursor into in_ring
					 * (samples, wraps mod
					 * 2*host_frame_samples) (usage
					 * inference, F10106)                  */
	unsigned char pad_25b4[4];	/* +0x25b4 unmodelled                 */
	int in_read_half;		/* +0x25b8 out_write_half's
					 * counterpart for the input side: a
					 * 0/host_frame_samples ping-pong
					 * toggle picking which half of
					 * in_ring the next flush reads its
					 * pump-rate input from (usage
					 * inference, F10106)                  */
	short in_ring[2 * 0xa0];	/* +0x25bc the double-buffered ring
					 * FAX_process's own input is copied
					 * into, and the rc_a RcFixed_Resample
					 * call (or the identity path directly)
					 * reads from.  Sized by the exact
					 * 0x280 gap to the struct's own
					 * 0x28bc allocation size, with 0x80
					 * bytes left over (pad_283c) (F10106) */
	unsigned char pad_283c[0x28bc - 0x283c]; /* +0x283c to the object's
					 * own 0x28bc allocation size, unread
					 * by anything written so far        */
};

/*
 * The "usage inference" fields above are each FAX_process's own derivation
 * and none is contradicted by a stronger source; see that function's own
 * banner in `src/service/voice.c` for the full control-flow account
 * (F10106). `in`/`out` (FAX_process's own second and third parameters) are
 * declared `void *` rather than `short *`: the function never dereferences
 * either as a sample array, only hands each to `sysdep_memcpy` with an
 * explicit byte length, and the top-level per-outer-iteration pointer
 * advance is unscaled against the sample-granular `count` argument
 * (`add %eax,0x94(%esp)`, no `*2`) -- a real, faithfully-reproduced property
 * of the object, not resolved further here.
 */

#define FAX_MODELLED_BYTES	0x283c

/**
 * @brief Create a FAX session: a Class 1 state machine plus, where the
 * host rate needs it, a pair of resamplers to the pump's 8 kHz.
 *
 * `originate` nonzero builds a session-initiated (originating) Class 1
 * session; zero builds an answer-initiated one whose first state generates
 * the T.30 CED tone (`struct fax_class1_cfg::mode`, class1.h, values
 * `CLASS1_ANS_ORG_NORMAL`/`CLASS1_ANS_ORG_ANSWER`). `rate` selects the
 * resampler pair: 8000 needs none (identity), 9600 or 48000 build one via
 * `RcFixed_Create` (four distinct converter IDs across the two -- 3/2 for
 * 9600, 5/4 for 48000, `rc_a` then `rc_b`); any other value fails. `s7`
 * (the config's `s7_timeout`) comes from `modem_get_sreg(modem, 7)` --
 * slmodemd's own S-register accessor, declared `extern` in `voice.c` the
 * same way `modem_recv_from_tty`/`modem_send_to_tty` already are, since no
 * dsplib header owns it.
 *
 * On failure (an unrecognised `rate`, either resampler returning NULL, or
 * `fax_class1_create` returning NULL) the object tears down whatever it
 * already built -- `rc_a`, `rc_b`, `class1` -- in that order, inline rather
 * than by calling #FAX_delete, even reusing #FAX_delete's own debug string
 * ("fax: delete...\n") at the same site.
 *
 * @param modem      The host modem object, forwarded to
 *                    modem_recv_from_tty/modem_send_to_tty.
 * @param originate  Nonzero for a session-initiated call, zero for
 *                    answer-initiated.
 * @param rate       Host sample rate: 8000, 9600 or 48000.
 * @return The new session, or NULL on failure.
 */
struct fax_ctx *FAX_create(void *modem, int originate, unsigned int rate);

/**
 * @brief Tear down a FAX session.
 *
 * In the object's own order: print a debug line ("fax: delete...\n",
 * `.rodata.str1.1` 0x1be) when `dsplibs_debug_level > 1`, delete `rc_a` and
 * `rc_b` if set, delete `class1` if set (and clear the field), then free
 * `ctx` itself. Every check is unconditional and independent -- there is
 * no early return.
 *
 * @param ctx  The session to free.
 */
void FAX_delete(struct fax_ctx *ctx);

/**
 * @brief Send a Class 1 command to a FAX session.
 *
 * `cmd` is one of the six #FAXC1_FTS-and-siblings codes below -- the outer
 * numbering, a different space from `fax_class1_command`'s own
 * `FAX_CLASS1_*_COMMAND` (class1.h), which this function remaps into after
 * validating `arg` (cast through `(int)(long)`, never dereferenced -- a
 * rate code for FTM/FRM against the twelve `_set_modem_rate` recognises, or
 * exactly 3 for FTH/FRH, the V.21 control-channel sentinel
 * `_cHDLCrx_init_from_idle` itself tests for). `ctx == NULL` or
 * `ctx->class1 == NULL` returns -1 immediately; an invalid `cmd` or a
 * rejected `arg` also returns -1, each logged at debug level > 1 with the
 * object's own per-command string ("fax:  FAXC1_FTH, %x\n" and five
 * siblings). A validated call always returns 1 -- `fax_class1_command`'s
 * own return is discarded.
 *
 * @param ctx  The session.
 * @param cmd  One of #FAXC1_FTS .. #FAXC1_FRH.
 * @param arg  Command argument (a rate code, or the FTH/FRH sentinel 3);
 *             cast to an integer, never dereferenced.
 * @return 1 on success, -1 on any rejection.
 */
int FAX_class1_command(struct fax_ctx *ctx, int cmd, void *arg);

/*
 * FAX_class1_command's own `cmd` numbering -- the object's own debug
 * strings ("fax:  FAXC1_FTS, %x\n" etc, rank-1 evidence), in the jump
 * table's own order (`.rodata` 0x60, six entries). NOT
 * `FAX_CLASS1_*_COMMAND` (class1.h): FTS=0 there is TS=4 here, and the
 * remap is total, not merely offset.
 */
#define FAXC1_FTS	0
#define FAXC1_FRS	1
#define FAXC1_FTM	2
#define FAXC1_FRM	3
#define FAXC1_FTH	4
#define FAXC1_FRH	5

/**
 * @brief Pump samples through a FAX session: read/resample/dispatch/write.
 *
 * Consumes `count` samples' worth of `in`, in chunks of
 * `ctx->host_frame_samples` samples, and produces the same count of `out`.
 * Full derivation (the two ring buffers, the eleven-way dispatch on
 * `fax_class1_progress`'s own `FAX_CLASS1_*` return, the ping-pong halves)
 * is in `src/service/voice.c`'s own banner on the function (F10106); this
 * file's struct comments above carry only the field-by-field evidence.
 *
 * `in`/`out` are `void *`, not `short *` -- see the struct's own note above
 * for why. `count` is in the same units `host_frame_samples` is (matched
 * directly against it, unscaled, every outer iteration).
 *
 * @param ctx    The session.
 * @param in     Host-rate input samples, `count` of them.
 * @param out    Host-rate output samples, `count` of them.
 * @param count  Samples to process.
 * @return The composed status word (see `src/service/voice.c`'s own banner
 *         for its bits).
 */
int FAX_process(struct fax_ctx *ctx, const void *in, void *out, int count);

#endif /* DSPLIB_FAX_H */
