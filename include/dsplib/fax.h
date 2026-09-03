/*
 * fax.h -- the FAX service's own context object, `struct fax_ctx`, and the
 * four `voice.c#3 +3`-span entry points that share it: `FAX_create`,
 * `FAX_delete`, `FAX_class1_command` and `FAX_process`.
 *
 * THE SPAN LABEL IS NOT THE FILE.  All four sit in the object right after
 * the `VOICE_*` group and before the ring detector -- `nm -S --size-sort`
 * over `.text 0x1450..0x2121` shows FAX_delete, FAX_create,
 * FAX_class1_command, FAX_process, then RD_create -- in `src/service/voice.c`
 * itself, per this tree's own note there that "nothing in the `voice.c#1..#3`
 * spans is unwritten" no longer holds now that these four exist in the same
 * bracket.  This header holds the FAX SIDE's own type, separate from
 * `struct voice_ctx` (`voice.h`), because the two are unrelated objects
 * reached by unrelated entry points that merely happen to sit in the same
 * translation unit.
 *
 * `struct fax_ctx` IS NOT `struct fax_class1` (`class1.h`).  It is the
 * OUTER session object `FAX_create` allocates and `FAX_delete` frees, which
 * itself OWNS one `struct fax_class1 *` (at +0x4) -- the T.30/Class-1 state
 * machine class1.c/class1rx.c/class1tx.c model -- plus two resampler handles
 * (`struct rc *`, `fixedrc.h`) that bridge the host's rate to the pump's own.
 *
 * WHAT IS ESTABLISHED SO FAR, all from `FAX_delete` (0x001450, 172 bytes,
 * the only one of the four with every callee already written):
 *
 *   +0x004  class1   `struct fax_class1 *`, torn down by `fax_class1_delete`
 *                    -- evidence class 2, that callee's own signature
 *   +0x2010 rc_a     `struct rc *`, torn down by `RcFixed_Delete` -- same
 *                    evidence class.  Checked BEFORE `rc_b` in
 *                    `FAX_delete`'s own order
 *   +0x2014 rc_b     `struct rc *`, the second `RcFixed_Delete` call
 *
 * `FAX_create` (0x001500, 564 bytes) and `FAX_class1_command` (0x001740,
 * 708 bytes) are NOT written this wave -- see the blocked-symbols note
 * below -- but `FAX_create`'s own disassembly is read here anyway, for the
 * struct evidence it carries as `rc_a`/`rc_b`'s own constructor: it branches
 * on a sample-rate argument (0x1f40 = 8000, 0x2580 = 9600, 0xbb80 = 48000)
 * before calling `RcFixed_Create` twice, matching `fixedrc.h`'s own
 * "the pumps run at 8 kHz, the host at 9600" note, and its first two
 * arguments -- `mov 0x40(%esp),%edi` / `mov 0x48(%esp),%esi` after a
 * three-register push + `sub $0x30` frame -- are in the exact same stack
 * positions `VOICE_create(void *modem, unsigned int rate)` uses, which is
 * why `FAX_create`'s own (unwritten) prototype below carries that shape.
 *
 * `FAX_process` (0x001a10, 1,809 bytes) is now WRITTEN, and the fields
 * below are its own derivation:
 *
 *   +0x000  modem          `void *`, dereferenced and passed as
 *                          `modem_recv_from_tty`/`modem_send_to_tty`'s own
 *                          first argument -- evidence class 2, matching
 *                          `modem_recv_from_tty(void *m, void *buf, int n)`'s
 *                          declared shape and `FAX_create`'s own predicted
 *                          `(void *modem, unsigned int rate)` first argument
 *                          (this file's banner above).  Was `pad_000`
 *   +0x008  host_rx_enable `int`, 0/1.  Gates whether `FAX_process` polls
 *                          `modem_recv_from_tty` at all this cycle; set to 1
 *                          only by the `FAX_CLASS1_CONNECT` dispatch case,
 *                          cleared to 0 by every OTHER case that touches it
 *                          (`NO_MESSAGE` and `ACCEPT_RATE` leave it alone).
 *                          Usage inference only -- FAX_process.c's own
 *                          derivation, see `src/service/voice.c`
 *   +0x00c  host_rx_want   `int`, the byte count to request from
 *                          `modem_recv_from_tty` next cycle (clamped to
 *                          0x1000 before the call), OVERWRITTEN after every
 *                          `fax_class1_progress` call with whatever value
 *                          comes back through that call's own `word8`
 *                          out-pointer -- so its steady-state meaning is
 *                          "what `fax_class1_progress` wants read next", not
 *                          simply "the previous cycle's byte count".  Usage
 *                          inference
 *   +0x010  host_tx_buf    `unsigned char[0x1000]`, passed whole (its
 *                          address) as `fax_class1_progress`'s own `word3`
 *                          argument, and is exactly the buffer
 *                          `modem_send_to_tty` sends from afterwards with a
 *                          length taken from that same call's `word7`
 *                          out-pointer -- so this is the host-bound byte
 *                          stream `fax_class1_progress` fills for
 *                          `FAX_process` to forward.  Sized by the exact gap
 *                          to `host_rx_buf` (both regions are exactly
 *                          0x1000 bytes)
 *   +0x1010 host_rx_buf    `unsigned char[0x1000]`, `modem_recv_from_tty`'s
 *                          own destination buffer AND `fax_class1_progress`'s
 *                          `word4` argument.  Sized by the `0x1000` clamp on
 *                          the read request, confirmed by the exact 0x1000
 *                          gap to `rc_a` at +0x2010
 *   +0x2018 rx_resampled   `short[0xa0]`, `RcFixed_Resample`'s own `out`
 *                          target when `rc_a != NULL` -- the pump-rate `rx`
 *                          buffer handed to `fax_class1_progress`.  Sized by
 *                          the exact 0x140 (160 shorts) gap to `tx_pump_rate`
 *   +0x2158 tx_pump_rate   `short[0xa0]`, `fax_class1_progress`'s own `tx`
 *                          target when `rc_a != NULL` (skipped, writing
 *                          straight into `out_ring` instead, when
 *                          `rc_a == NULL`) -- then `RcFixed_Resample`'s own
 *                          `in` source for the `rc_b` call.  Sized by the
 *                          exact 0x140 gap to `host_frame_samples`
 *   +0x2298 host_frame_samples  `int`, the SAMPLE count `FAX_process`
 *                          batches per host-rate chunk, and `RcFixed_
 *                          Resample`'s own `in_count`/`out_count` for both
 *                          calls.  Rank-1 evidence: FAX_process's own debug
 *                          string is "fax: process: samples count %d != %d"
 *                          (`.rodata.str1.4` 0x350), printed with this field
 *                          and the literal `0xa0` (`CLASS1_BLOCK_SAMPLES`,
 *                          `class1.h`) as its two arguments -- the object's
 *                          own word for the field's unit
 *   +0x229c out_produced   `int`, a running total incremented by
 *                          `host_frame_samples` once per flush cycle;
 *                          nothing `FAX_process` itself reads back.  Usage
 *                          inference
 *   +0x22a4 out_read_cursor `int`, the extraction cursor into `out_ring`
 *                          (samples, wraps mod `2*host_frame_samples`),
 *                          advanced every INNER iteration regardless of
 *                          whether that iteration flushed.  Usage inference
 *   +0x22a8 out_write_half `int`, a two-value (0 / `host_frame_samples`)
 *                          ping-pong selector toggled once per flush cycle,
 *                          picking which half of `out_ring` the NEXT
 *                          flush's tx-direction data lands in.  Usage
 *                          inference
 *   +0x22ac out_ring       `short[2*0xa0]`, the double-buffered ring
 *                          `fax_class1_progress` (identity path) or
 *                          `RcFixed_Resample`'s `rc_b` call (resampled path)
 *                          writes host-rate tx samples into, and the
 *                          function's own `a3` output is drained from.
 *                          `2 * host_frame_samples` shorts (0x280 bytes),
 *                          matching `in_ring`'s own size below; ends at
 *                          +0x252c, 0x80 bytes short of `in_pending` at
 *                          +0x25ac (`pad_252c`, unclaimed)
 *   +0x25ac in_pending     `int`, samples buffered in `in_ring` awaiting a
 *                          flush; incremented per inner-loop copy-in,
 *                          decremented by `host_frame_samples` once per
 *                          flush.  Usage inference
 *   +0x25b0 in_write_cursor `int`, the fill cursor into `in_ring` (samples,
 *                          wraps mod `2*host_frame_samples`).  Usage
 *                          inference
 *   +0x25b8 in_read_half   `int`, `out_write_half`'s counterpart for the
 *                          input side: a 0 / `host_frame_samples` ping-pong
 *                          toggle picking which half of `in_ring` the NEXT
 *                          flush reads its pump-rate input from.  Usage
 *                          inference
 *   +0x25bc in_ring        `short[2*0xa0]`, the double-buffered ring
 *                          `FAX_process`'s own `a2` input is copied into,
 *                          and `RcFixed_Resample`'s `rc_a` call (or the
 *                          identity path directly) reads from.  Sized by
 *                          the exact 0x280 gap to the struct's own 0x28bc
 *                          allocation size (FAX_create's `sysdep_malloc`
 *                          argument, this file's banner above), with 0x80
 *                          bytes left over past it -- unclaimed, `pad_283c`
 *
 * Every one of the "usage inference" fields above is FAX_process's own
 * derivation and none is contradicted by a stronger source; see that
 * function's own banner in `src/service/voice.c` for the full control-flow
 * account.  `a2`/`a3` (the function's own second and third parameters) are
 * declared `void *` rather than `short *`: FAX_process itself never
 * dereferences either as a sample array, only hands each to `sysdep_memcpy`
 * with an explicit byte length, and the top-level per-outer-iteration
 * pointer advance is UNSCALED against the sample-granular `count` argument
 * (`add %eax,0x94(%esp)`, no `*2`) -- a real, faithfully-reproduced property
 * of the object, not resolved further here.
 *
 * NEITHER FAX_create NOR FAX_class1_command IS WRITTEN, RE-VERIFIED with
 * `dis.py`/`nm` rather than trusted from the prior wave's own note (F10111).
 * `FAXVMI_create` landed since that note was written and is NO LONGER the
 * blocker; the chain now bottoms out at `FAXVMI_control` (`faxvmi.c`, still
 * unwritten, assigned to nobody this wave) and at four `class1tx.c` leaf
 * inits (`_cHDLCrx_init_from_idle`, `_tx_scrambled_ones_init`,
 * `cHDLCtx_preamble_state_init`, `_rx_look_carrier_init`) that are
 * themselves either direct callers of `FAXVMI_control` or callers of
 * `_init_receiver`/`_init_transmitter`, which are ALSO blocked on
 * `FAXVMI_control` (and, per `_cHDLCrx_init_from_idle`'s own `V21RX_CTL`
 * load, on at least one `.data` request template besides).  Per CLAUDE.md's
 * own trap, a reference from `src/` to an unwritten blob symbol fails EVERY
 * test binary at link, so both bodies are left out rather than
 * written-and-broken; see docs/findings.md F10111 for the exact chain.
 */

#ifndef DSPLIB_FAX_H
#define DSPLIB_FAX_H

struct fax_class1;
struct rc;

/*
 * `sizeof` is NOT settled: `FAX_create` allocates `0x28bc` bytes
 * (`movl $0x28bc,(%esp)` at 0x151b, ahead of `sysdep_malloc`), which is
 * larger than every offset this batch reaches, so the struct extends past
 * what is modelled here.  `FAX_MODELLED_BYTES` is a modelling bound, not a
 * size claim, the same convention `class1.h`'s `CLASS1_MODELLED_BYTES`
 * uses.
 */
struct fax_ctx {
	void *modem;			/* +0x000 handed to modem_recv_from_tty/
					 * modem_send_to_tty as their own first
					 * argument -- see this file's banner */
	struct fax_class1 *class1;	/* +0x004 the Class 1 session,
					 * created by (unwritten) FAX_create,
					 * torn down by FAX_delete's own
					 * `fax_class1_delete` call          */
	int host_rx_enable;		/* +0x008 see this file's banner      */
	int host_rx_want;		/* +0x00c likewise                    */
	unsigned char host_tx_buf[0x1000]; /* +0x010 likewise             */
	unsigned char host_rx_buf[0x1000]; /* +0x1010 likewise            */
	struct rc *rc_a;		/* +0x2010 resampler handle, checked
					 * and deleted before rc_b in
					 * FAX_delete's own order            */
	struct rc *rc_b;		/* +0x2014 resampler handle          */
	short rx_resampled[0xa0];	/* +0x2018 see this file's banner     */
	short tx_pump_rate[0xa0];	/* +0x2158 likewise                   */
	int host_frame_samples;	/* +0x2298 likewise                   */
	int out_produced;		/* +0x229c likewise                   */
	unsigned char pad_22a0[4];	/* +0x22a0 unmodelled                 */
	int out_read_cursor;		/* +0x22a4 see this file's banner     */
	int out_write_half;		/* +0x22a8 likewise                   */
	short out_ring[2 * 0xa0];	/* +0x22ac likewise, ends +0x252c     */
	unsigned char pad_252c[0x25ac - 0x252c]; /* +0x252c unmodelled --
					 * `in_pending` is pinned to +0x25ac by
					 * the object's own `add $0x25ac,%eax`,
					 * leaving this gap unclaimed         */
	int in_pending;			/* +0x25ac likewise                   */
	int in_write_cursor;		/* +0x25b0 likewise                   */
	unsigned char pad_25b4[4];	/* +0x25b4 unmodelled                 */
	int in_read_half;		/* +0x25b8 see this file's banner     */
	short in_ring[2 * 0xa0];	/* +0x25bc likewise                   */
	unsigned char pad_283c[0x28bc - 0x283c]; /* +0x283c to the object's
					 * own 0x28bc allocation size, unread
					 * by anything written so far        */
};

#define FAX_MODELLED_BYTES	0x283c

/*
 * NOT WRITTEN.  `.text` 0x001500, 564 bytes -- calls the still-unwritten
 * `fax_class1_create` (blocked on `FAXVMI_control`, not `FAXVMI_create`,
 * which has since landed -- F10111), so a body here would fail every test
 * binary at link (CLAUDE.md's trap).  The argument shape is read off the
 * object's own stack layout, matching `VOICE_create`'s -- see this file's
 * own banner for the derivation.
 */
struct fax_ctx *FAX_create(void *modem, unsigned int rate);

/*
 * `.text` 0x001450, 172 bytes.  In the object's own order: print a debug
 * line ("fax: delete...\n", `.rodata.str1.1` 0x1be) when
 * `dsplibs_debug_level > 1`, delete `rc_a` and `rc_b` if set, delete
 * `class1` if set (and clear the field), then free `ctx` itself.  Every
 * check is unconditional and independent -- there is no early return.
 */
void FAX_delete(struct fax_ctx *ctx);

/*
 * NOT WRITTEN.  `.text` 0x001740, 708 bytes -- calls the still-unwritten
 * `fax_class1_command`, itself blocked on four of its own leaves
 * (`_cHDLCrx_init_from_idle`, `_tx_scrambled_ones_init`,
 * `cHDLCtx_preamble_state_init`, `_rx_look_carrier_init`, all
 * `class1tx.c`), which are in turn blocked on `FAXVMI_control`/
 * `_init_receiver`/`_init_transmitter`.  See docs/findings.md F10111 for
 * the full chain, re-verified this wave.
 */
int FAX_class1_command(struct fax_ctx *ctx, int cmd, void *arg);

/*
 * `.text` 0x001a10, 1,809 bytes.  The largest of the four: a read/resample/
 * dispatch/write pump that consumes `count` samples' worth of `in`, in
 * chunks of `ctx->host_frame_samples` samples, and produces the same count
 * of `out`.  Full derivation (the two ring buffers, the eleven-way dispatch
 * on `fax_class1_progress`'s own `FAX_CLASS1_*` return, the ping-pong
 * halves) is in `src/service/voice.c`'s own banner on the function; this
 * file's struct banner above carries only the field-by-field evidence.
 *
 * `in`/`out` are `void *`, not `short *` -- see this file's struct banner
 * for why.  `count` is in the same units `host_frame_samples` is (matched
 * directly against it, unscaled, every outer iteration).
 */
int FAX_process(struct fax_ctx *ctx, const void *in, void *out, int count);

#endif /* DSPLIB_FAX_H */
