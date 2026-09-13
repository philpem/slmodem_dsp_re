/**
 * @file class1.h
 * @brief Class 1 fax: the session object, as far as this batch reads it.
 *
 * The fax phase is deliberately last (see CLAUDE.md); what is here now
 * grew from a handful of `class1.c`-span symbols that nothing else in the
 * object reached (exported API with no internal referrer, finding
 * F8320's bucket), written on their own merit. So this header models
 * `struct fax_class1` as far as the session dispatcher and its state
 * handlers need it -- `fax_class1_create` (below) is what actually
 * allocates and owns it.
 *
 * `fax_class1_create` writes at least as far as +0x12dc and takes
 * `&ctx->0x12e0` as a sub-block, so the object extends past this struct;
 * nothing here allocates one, and #CLASS1_MODELLED_BYTES is a modelling
 * bound, not a `sizeof` claim.
 */

#ifndef DSPLIB_CLASS1_H
#define DSPLIB_CLASS1_H

struct faxvmi;
struct faxvmi_cfg;
struct fax_fifo;
struct fpm_tone;

struct fax_class1 {
	short scratch_frame_len;	/* +0x0000 <- hdlc_write_cursor - 1, by
					 * _handle_hdlc_input_close         */
	unsigned char pad_002[2];	/* +0x0002                          */
	unsigned char flags004;		/* +0x0004 bit 4 gates frame_end_latch        */
	unsigned char pad_005[0xffb];	/* +0x0005                          */
	unsigned short superframe[0x100];	/* +0x1000 `_hdlc_emulate_receive_
						 * state`'s own buffer: a run of
						 * length-prefixed records -- entry
						 * `i` is a length, the next `i+1`
						 * entries are the record's own
						 * (byte-valued) elements, and the
						 * next record starts right after --
						 * read only up to `superframe_len`
						 * bytes of it, in the same shape
						 * `_handle_hdlc_input_close`
						 * leaves a frame in. Named from
						 * the object's own debug string
						 * ("SuperFrame full, skipping HDLC
						 * frame!\n"); sized from address
						 * contiguity with `vmi_c`, not a
						 * size constant anywhere (F10133).
						 */
	struct faxvmi *vmi_c;		/* +0x1200 a third FAXVMI handle, torn
					 * down by `fax_class1_delete` before
					 * `vmi_a`/`vmi_b`, and typed the same
					 * way as they are, by its own
					 * `FAXVMI_delete` call (F10055). Which
					 * of the three roles (V.21, data, or
					 * something else) this one plays is
					 * not established.                 */
	struct faxvmi *vmi_a;		/* +0x1204 FAXVMI handle, states
					 * 4..6 of fax_class1_status. Typed
					 * `struct faxvmi *` on `vmi_b`'s own
					 * evidence, the two being "two
					 * handles" of the same kind (F10053) */
	struct faxvmi *vmi_b;		/* +0x1208 FAXVMI handle, states
					 * 12..13. Typed from
					 * `_delete_data_rx_modem`'s and
					 * `_delete_data_tx_modem`'s own
					 * `FAXVMI_delete(struct faxvmi *)`
					 * call (F10053)                    */
	struct faxvmi_cfg *vmi_c_cfg;	/* +0x120c the `struct faxvmi_cfg`
					 * `fax_class1_create`'s fresh path
					 * builds for the V.21 TX control
					 * channel and hands to
					 * `FAXVMI_create(NULL, ...)` to build
					 * `vmi_c`. `fax_class1_delete` frees
					 * its one sub-allocation
					 * (`modem_cfg`, `faxcfg.h` +0x10 --
					 * a `struct v21tx_cfg`) before freeing
					 * this block itself (F10133).      */
	struct faxvmi_cfg *vmi_a_cfg;	/* +0x1210 the same shape as
					 * `vmi_c_cfg`, but for the V.21 RX
					 * control channel and `vmi_a` --
					 * `fax_class1_create`'s fresh path
					 * builds a `struct v21rx_cfg` as its
					 * own `modem_cfg`. Torn down the same
					 * way, one field later: `fax_class1_
					 * delete`'s own order is vmi_c_cfg,
					 * vmi_c, vmi_a_cfg, vmi_a (F10133). */
	struct faxvmi_cfg *modem_vmi;	/* +0x1214 the CURRENT data modem's
					 * VMI config block -- one field,
					 * reused for whichever direction is
					 * active, since Class 1 fax is
					 * half-duplex (F10053). `_delete_data_
					 * rx_modem` frees it (and, for a V.17
					 * receiver, three sub-allocations of
					 * the `struct v17rx_cfg` it owns
					 * first) and `_delete_data_tx_modem`
					 * frees it the same way with no
					 * sub-frees                        */
	int ans_org;			/* +0x1218 a copy of `struct
					 * fax_class1_cfg::mode`: 1 for the
					 * ordinary session, 2 for the
					 * answer-tone-only one -- named from
					 * `fax_class1_create`'s own debug
					 * line, "fax: fax_class1 will
					 * created (ans_org=%d, s7=%d)\n".
					 * Nothing reconstructed reads it back
					 * yet.                             */
	int state;			/* +0x121c the session state        */
	int prev_state;			/* +0x1220 the state as of the LAST
					 * call to `fax_class1_progress` --
					 * compared against `state` after each
					 * dispatch to log a transition, then
					 * updated to match                 */
	int frame_end_latch;		/* +0x1224 set 1 on hdlc close when
					 * flags004 bit 4 is on (F10133)    */
	int countdown;			/* +0x1228 armed by the state inits */
	int status;			/* +0x122c the FAX_CLASS1_* code
					 * `fax_class1_progress` returns     */
	int f1230;			/* +0x1230 written once by
					 * `_init_transmitter`
					 * (class1tx.c), keyed on the
					 * rate_code group: 0x30 for
					 * V.17 14400/12000, 0x18 for
					 * V.17 9600/7200 and V.29
					 * 9600/7200, 0xc for V.27ter
					 * 4800, 0x6 for V.27ter 2400.
					 * Nothing reconstructed reads
					 * it back yet.  Usage inference
					 * only -- no format string or
					 * typed reader establishes what
					 * it means, so it keeps a
					 * neutral name rather than a
					 * guessed one (F10118)              */
	int f1234;			/* +0x1234 the same function's other
					 * half: 0x8000 for every V.17
					 * rate and for V.29 9600, 0x75a2
					 * for V.29 7200, 0x4000 for both
					 * V.27ter rates.  Usage inference
					 * only, same as `f1230` (F10118)   */
	int last_in_byte;		/* +0x1238 the LAST byte of the block
					 * _handle_data_input was handed,
					 * kept whether or not the block is
					 * consumed                         */
	int delayed_status;		/* +0x123c applied to `status` when
					 * `delayed_status_countdown` reaches
					 * zero. The object's own debug line
					 * for the mechanism is "STATUS =
					 * DELAYED_STATUS".                 */
	int delayed_status_countdown;	/* +0x1240 decremented once per
					 * `fax_class1_progress` call while
					 * nonzero; on the call it reaches
					 * zero, `status` is set from
					 * `delayed_status`                 */
	int modem_direction;		/* +0x1244 which direction the
					 * CURRENT data modem (`modem_vmi`/
					 * `vmi_b`) is: `fax_class1_delete`
					 * reads it to choose `_delete_data_
					 * rx_modem` (== #CLASS1_MODEM_DIR_RX)
					 * or `_delete_data_tx_modem`
					 * (otherwise). A discriminant, not a
					 * flag -- `fax_class1_command`'s RM
					 * command writes
					 * #CLASS1_MODEM_DIR_RX, its TM command
					 * #CLASS1_MODEM_DIR_TX (F10133);
					 * whether a third value is possible
					 * is not established.              */
	int current_mod;		/* +0x1248 the modulation currently
					 * installed at `modem_vmi`/`vmi_b`
					 * -- 0 V.27ter, 1 V.29, 2 V.17.
					 * `_init_receiver`/`_init_
					 * transmitter` (class1rx.c/
					 * class1tx.c) compare a freshly
					 * derived modulation index against
					 * this field to choose their
					 * reinit path over a fresh create,
					 * and write it back on every exit
					 * (forced 32-bit `cmpl`/`movl`,
					 * F10116).                         */
	int dle_seen;			/* +0x124c a DLE has been seen and
					 * the next byte is its argument.
					 * Both _handle_data_input and
					 * _handle_hdlc_input keep their
					 * escape state HERE, in the same
					 * field                            */
	int hdlc_write_cursor;		/* +0x1250 the write cursor into the
					 * HDLC receive frame, in elements.
					 * _handle_hdlc_input_open arms it
					 * to 1 and both _..._input and
					 * _..._close report `hdlc_write_cursor - 1` as
					 * the length in scratch_frame_len               */
	int modem_rate_code;		/* +0x1254 the negotiated T.30 modem
					 * rate code -- `_set_modem_rate`'s
					 * own code space, exactly (F10051):
					 * `fax_class1_progress` switches on
					 * it to pick which modulation's
					 * quality latch to test. Not yet
					 * the name of a writer, only of this
					 * one reader.                       */
	struct fpm_tone *tone;		/* +0x1258 a tone generator/detector,
					 * torn down by `fax_class1_delete`'s
					 * own `FPM_TONE_delete` call
					 * (F10055), and confirmed for
					 * `CLASS1_ANSWER_TONE_STATE`: `_answer_
					 * tone_state` hands this straight to
					 * `FPM_TONE_generate`. `fax_class1_
					 * create` builds it at one of two
					 * frequencies (1100 Hz ordinary
					 * session, 2100 Hz -- the T.30 CED
					 * answer tone -- for `ans_org == 2`),
					 * both with `scale = 6400` and phase
					 * reversals disabled.               */
	int tone_cadence_phase;		/* +0x125c a 0/1 phase flag in
					 * `_hdlc_receive_look_carrier_state`'s
					 * own tone-cadence machine (F10133):
					 * 0 is the silence phase (accumulate
					 * `tone_cadence_timer` to 0x5dc0/24000
					 * then flip to 1), 1 is the tone
					 * phase (`FPM_TONE_generate` instead
					 * of `_put_silence`, accumulate
					 * `tone_cadence_timer` to 0xfa0/4000
					 * then flip back to 0). Usage
					 * inference only.                  */
	int tone_cadence_timer;		/* +0x1260 the sample accumulator that
					 * phase flips on -- reset to 0 and
					 * bumped by `CLASS1_BLOCK_SAMPLES` per
					 * call in EITHER phase, never both at
					 * once. Usage inference only
					 * (F10133).                         */
	int cng_enabled;		/* +0x1264 `fax_class1_create`
					 * unconditionally clears it, then the
					 * ORDINARY-session finish alone
					 * overrides it to `(cfg->disable_cng
					 * == 0)` -- the answer-tone-only
					 * finish never touches it again, so
					 * it stays disabled on that path
					 * regardless of `cfg->disable_cng`
					 * (named from the object's own debug
					 * line at the override site, "CNG
					 * generation disabled").  Reused as a
					 * general tone-cadence gate
					 * everywhere else it is read: 0
					 * disables the whole `tone_cadence_
					 * timer`/`tone_cadence_phase` cadence
					 * machine (plain silence only) and
					 * decides whether a look-carrier
					 * timeout is reported as NO_CARRIER
					 * (gate==0) or silently absorbed into
					 * the cadence path (gate!=0). Cleared
					 * both on a fresh entry to HDLC_
					 * RECEIVE_LOOK_CARRIER_STATE and on a
					 * hard timeout; `fax_class1_create` is
					 * the only reconstructed writer that
					 * can set it nonzero.               */
	int answer_tone_blocks;		/* +0x1268 the countdown threshold
					 * `_answer_tone_state` compares
					 * `countdown` against. `fax_class1_
					 * create` derives it from
					 * `cfg->answer_tone_ms / 20` (block
					 * period at the implied 8 kHz/160-
					 * sample rate) when
					 * `cfg->answer_tone_ms` is nonzero,
					 * else a literal default of 150 (3
					 * seconds). The object's own debug
					 * line for the derived case is
					 * "Answer tone length %d ms".      */
	int tx_rate;			/* +0x126c written once by
					 * `_init_transmitter`
					 * (class1tx.c): the same
					 * negotiated bit rate it just
					 * derived from `rate_code`
					 * (0x960/0x12c0/0x1c20/0x2580/
					 * 0x2ee0/0x3840).  Nothing
					 * reconstructed reads it back
					 * yet.  Usage inference only  */
	int tx_connect_countdown;	/* +0x1270 `_tx_scrambled_ones_state`'s
					 * own one-shot countdown: decremented
					 * once per call while positive, and
					 * reaching exactly 0 on the way down
					 * fires a debug line ("At %2d.%02d[sec]
					 * Tx connect\n") and sets
					 * `transmit_enabled`. Usage inference
					 * only (F10133).                    */
	/*
	 * A timestamp pair.  The object prints them together as
	 * "At %2d.%02d[sec]" -- in _hdlc_receive_state_init and twice in
	 * fax_class1_progress -- which is the whole of the evidence for the
	 * names.
	 */
	int clock_sec;			/* +0x1274                          */
	int clock_frac;			/* +0x1278                          */
	int f127c;			/* +0x127c a nonzero-sample counter
					 * `fax_class1_progress` maintains
					 * over up to `*rx_count / 8` samples
					 * of `rx` per call, resetting to 0
					 * when it exceeds `*rx_count / 16` --
					 * nothing traced reads it back, so
					 * what it is FOR is not established */
	int hdlc_frame_done;		/* +0x1280 `_t30_preabmle_state`'s own
					 * copy of `_handle_hdlc_input`'s raw
					 * return (0 or 1), typed by the callee
					 * whose return it holds verbatim   */
	int buffers_sent;		/* +0x1284 incremented once per call
					 * `hdlc_frame_done` came back nonzero.
					 * Named from the object's own debug
					 * line, "At %2d.%02d[sec] Elapsed 1
					 * second, send %d buffers\n"
					 * (`_t30_preabmle_state`).          */
	struct fax_fifo *tx_fifo;	/* +0x1288 `fax_class1_info(1)` reads
					 * an unsigned short at +0xc of it.
					 * Typed from `_delete_data_tx_modem`'s
					 * own `FIFO_delete(struct fax_fifo *)`
					 * call (F10053).                    */
	int transmit_enabled;		/* +0x128c `_tx_scrambled_ones_state`'s
					 * own latch: 0 until `tx_connect_countdown` (above)
					 * reaches its one-shot zero, then 1 for
					 * the rest of the session.  Gates a
					 * second check (`tx_fifo_ready`, below) that
					 * decides between the ordinary
					 * FAXVMI_process path and a raw
					 * FIFO_read path.  Named from the
					 * object's own debug line at the site
					 * that sets it, "At %2d.%02d[sec]
					 * ENABLE_TRANSMIT in _tx_
					 * scrambled_ones_state\n".          */
	int tx_bytes_per_block;		/* +0x1290 the `count` `_tx_nulls_
					 * state` asks `FIFO_read(ctx->tx_fifo,
					 * ...)` for on every call. A FULL
					 * 32-bit `int`, not `unsigned short`:
					 * `_tx_scrambled_ones_state`'s own fill
					 * loop uses the whole register as a
					 * loop bound and in an unmasked
					 * `cmp` against a FIFO count, and
					 * `_tx_nulls_state` makes a second,
					 * independent full-width access at
					 * the same offset.  A model that once
					 * left two bytes here as `pad_1292`
					 * left them as fill()-random garbage,
					 * which that loop-bound read turned
					 * into a value near 2^29 and walked
					 * off the struct -- a real segfault
					 * this width fix closed, not a defect
					 * in the callee.                    */
	int tx_connect_latch;		/* +0x1294 `_tx_data_state`'s and
					 * `_tx_scrambled_ones_state`'s own
					 * one-shot latch, set to 1 the first
					 * time FAXVMI_process's raw return has
					 * bit 0 of its second byte set (an
					 * unnamed bit of the wrapped
					 * modulation's own status word --
					 * `faxvmi.h`'s note on the low 24 bits
					 * applies) while it was still 0; gates
					 * `tx_connect_countdown`'s own arming and a one-time
					 * debug line.  Usage inference only    */
	int tx_fifo_ready;		/* +0x1298 `_tx_scrambled_ones_state`'s
					 * own per-call flag: 1 when
					 * `ctx->tx_fifo->count >= ctx->tx_bytes_per_block`
					 * (the tx FIFO already holds at least
					 * one read-quantum's worth), computed
					 * fresh every call. When this AND
					 * `transmit_enabled` are both set, the
					 * function takes a raw `FIFO_read` path
					 * instead of driving FAXVMI_process.
					 * Usage inference only                 */
	int async_locked;		/* +0x129c the start-bit search has
					 * succeeded and the alignment
					 * below is frozen                  */
	unsigned int async_window;	/* +0x12a0 the three bytes before the
					 * one being converted; the window
					 * is (byte << 24) | this           */
	int async_shift;		/* +0x12a4 where the recovered octet
					 * starts in that window (16 with
					 * no search needed)                */
	unsigned int async_mask;	/* +0x12a8 and the eight bits it
					 * covers (0xff0000 likewise)       */
	unsigned char pad_12ac[4];	/* +0x12ac                          */
	int data_input_closed;		/* +0x12b0 _handle_data_input has seen
					 * DLE ETX; every later call
					 * consumes nothing and reports
					 * zero                             */
	int s7_timeout;			/* +0x12b4 T.30's own name for this,
					 * from `_hdlc_receive_look_carrier_
					 * state`'s own debug line ("...curent_
					 * timeout = %d, No carrier ... S7 =
					 * %d[sec]\n", this field last). That
					 * function multiplies it by 50, or by
					 * 8000 and divides by `*rx_count`, to
					 * get a block-count timeout threshold;
					 * the `*rx_count == 0` fallback (`*
					 * 50`) is exactly what the general
					 * formula reduces to at the usual
					 * 160-sample block and an implied
					 * 8 kHz rate.                       */
	unsigned int silence_blocks;	/* +0x12b8 blocks seen below the
					 * silence threshold; compared
					 * against `countdown` UNSIGNED
					 * (`jae`, 0x092d10)                */
	short energy;			/* +0x12bc the last block's FPM_rms.
					 * The author's word: the object
					 * prints exactly this value as
					 * "Energy %d"                      */
	int rx_agc_mult;		/* +0x12c0 `_hdlc_receive_state`, on a
					 * successfully-closed nonempty frame,
					 * walks `ctx->vmi_a->link->int_0014`
					 * to a pointer, then that pointer's
					 * own +0x50 (`V21RX_OBJ_DSP`,
					 * v21fax.h) to another, and stores
					 * the sign-extended shorts at
					 * +0x30/+0x32 of THAT into
					 * `rx_agc_mult`/`rx_agc_shift` -- a
					 * snapshot of the V.21 receiver's own
					 * AGC gain (mantissa Q15, exponent as
					 * a left shift, `fpm_agc.h`), taken
					 * whenever an HDLC frame closes.
					 * Typed by tiling the same modelled
					 * `struct fpm_agc` v17fax.h's
					 * `V17RXS_SRE_ADAPT` family already
					 * reaches (F10133); what the snapshot
					 * is FOR is not established -- nothing
					 * reconstructed reads it back.      */
	int rx_agc_shift;		/* +0x12c4 see rx_agc_mult             */
	int superframe_countdown;	/* +0x12c8 `_hdlc_emulate_receive_
					 * state`'s own between-record
					 * countdown: decremented once per
					 * call, and a value that was <= 0
					 * BEFORE the decrement is what
					 * fires the next record (or the
					 * idle transition once `superframe`
					 * is exhausted).  Reset to 2 (F10133) */
	int superframe_read_idx;	/* +0x12cc the same function's "next
					 * record to emit" index into the
					 * record COUNT `superframe` parses to
					 * (not a byte offset).  Reset to 0 */
	int superframe_len;		/* +0x12d0 and the valid byte length
					 * of `superframe` for this batch of
					 * records.  Reset to 0.  All three
					 * are read-and-written by that one
					 * function only, this batch        */
	int f12d4;			/* +0x12d4 read (32-bit `mov`) by
					 * `_init_receiver`/`_init_
					 * transmitter` on every exit path
					 * (both fresh-create and reinit)
					 * and its LOW 16 BITS stored into a
					 * per-modulation offset of the
					 * wrapped modem object reached via
					 * `vmi_b->link->int_0014`:
					 * V.27ter -> that object's +0x50,
					 * then +0x14; V.29 -> +0x4c, then
					 * +0x1c; V.17 -> +0x5c, then +0x20.
					 * Nothing establishes what it MEANS
					 * beyond "propagated to the wrapped
					 * modem" -- usage inference only,
					 * kept neutral on purpose          */
	int gain_attenuation_db;	/* +0x12d8 `fax_class1_info(0)`; create
					 * clears it. `_hdlc_receive_look_
					 * carrier_state`'s own gain-request
					 * scan (class1tx.c) stores `12 - 3*i`
					 * here on a match against
					 * `HDLC_LOOK_CARRIER_LEVELS[i]` and
					 * prints the object's own debug line
					 * "Gain Attenuation Reuqest: +%d[dB],
					 * avg_rms = %d" with this field as
					 * the first `%d` (the author's own
					 * typo, kept) -- a requested
					 * attenuation in dB (12, 9, 6 or 3)
					 * (F10133).                        */
	const short *iir_coeff;		/* +0x12dc create: a .rodata ptr.
					 * Typed from `fax_class1_progress`'s
					 * own call, `FPM_iir_filt_II(short *,
					 * const short *coeff, ...)` -- the
					 * coefficient table for the 2-section
					 * filter below (F10133).            */
	short iir_state[8];		/* +0x12e0 `FPM_iir_filt_II`'s own
					 * state, 4 words/section per
					 * `fpm_iir.h` times the 2 sections
					 * `fax_class1_progress` passes as a
					 * literal -- sized from the callee's
					 * own contract, not guessed          */
	int iir_enabled;		/* +0x12f0 gates whether
					 * `fax_class1_progress` runs the IIR
					 * filter tick at all this call.
					 * Usage inference only (F10133).    */
};

/*
 * `pad_12be[2]` is gone (pad-region removal audit, F10145): `energy` (a
 * `short`) ends at +0x12be and `rx_agc_mult` needs 4-byte alignment, so the
 * compiler inserts the same 2-byte gap on its own; nothing in this tree
 * ever named the field.  `pad_002[2]`, `pad_005[0xffb]` and `pad_12ac[4]`
 * elsewhere in this struct are NOT the same shape -- each would put its
 * following field at a DIFFERENT offset than the object's if simply
 * deleted (`flags004`, a `char`, needs no alignment at all; `async_mask`
 * already ends 4-byte aligned) -- so those three stay explicit, on the same
 * ground this file's own banner already gives them: real, unmodelled
 * content, not compiler padding.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define CLASS1_ASSERT_OFF(field, off) \
	typedef char fax_class1_off_##field[ \
		((int)__builtin_offsetof(struct fax_class1, field) \
			== (off)) ? 1 : -1]
CLASS1_ASSERT_OFF(rx_agc_mult, 0x12c0);
#endif

/** A modelling bound on `struct fax_class1`, not its true (larger) `sizeof`. See this file's own banner. */
#define CLASS1_MODELLED_BYTES	0x12f4

/**
 * `fax_class1_create`'s second argument -- a 0x18-byte configuration record
 * read at six fixed offsets and nowhere named by the object as a struct
 * (`FAX_create` builds one on its own stack, not from a `.data`/`.rodata`
 * template). Every field here is usage inference except `answer_tone_ms`,
 * which the object's own debug line types ("Answer tone length %d ms") and
 * `mode`, which the object's own debug line names ("ans_org", `FAX_create`'s
 * print -- see fax.h). `s7_timeout` is named for the already-established
 * `struct fax_class1::s7_timeout` field it is copied into unconditionally,
 * one field over.
 */
struct fax_class1_cfg {
	int mode;			/* +0x00 1: ordinary session (state starts
				 * at CLASS1_HDLC_RECEIVE_LOOK_CARRIER_STATE).
				 * 2: answer-tone-only (state starts at
				 * CLASS1_ANSWER_TONE_STATE, the tone
				 * generator is built at 2100 Hz -- the T.30
				 * CED tone -- instead of 1100, and
				 * `_cHDLCrx_init_from_idle` is never called).
				 * Copied verbatim into `ctx->ans_org`     */
	int s7_timeout;			/* +0x04 -> ctx->s7_timeout, unconditional  */
	int f08;			/* +0x08 -> ctx->f12d4, unconditional.  No
				 * reconstructed reader beyond that field's
				 * own (F10116/F10117); kept neutral         */
	int iir_enable;			/* +0x0c nonzero -> ctx->iir_enabled = 1 in
				 * every traced caller (`FAX_create`), so no
				 * reconstructed caller leaves the IIR tick
				 * off; usage inference on the NAME, not the
				 * effect                                    */
	int answer_tone_ms;		/* +0x10 nonzero -> ctx->answer_tone_blocks =
				 * this / 20 (block period).  Zero -> the
				 * object's own literal default, 150 blocks
				 * (3 seconds).  Named from the object's own
				 * debug line, "Answer tone length %d ms",
				 * at the site that derives it.              */
	int disable_cng;		/* +0x14 -> ctx->cng_enabled = (this == 0).
				 * Named from the object's own debug line,
				 * "CNG generation disabled\n", printed
				 * exactly when this is nonzero.             */
};

/** `struct fax_class1_cfg::mode` -- see the struct's own field comment. */
#define CLASS1_ANS_ORG_NORMAL	1
#define CLASS1_ANS_ORG_ANSWER	2

/*
 * `struct fax_class1::modem_direction` -- see that field's own comment.
 * `fax_class1_command`'s RM command (data receive) writes the first, its TM
 * command (data transmit) the second; `fax_class1_delete` and
 * `fax_class1_progress` both test against the first alone.
 */
#define CLASS1_MODEM_DIR_RX	1
#define CLASS1_MODEM_DIR_TX	2

/*
 * The state numbers are the author's own, read out of `states_names`
 * (.rodata, twenty {int, char *} pairs `fax_class1_progress` searches to log
 * a transition) -- the strongest class of evidence this tree recognises, so
 * the spellings are kept exactly as written, `RECIEVE` included. The
 * `CLASS1_` prefix is ours; the object's names carry none and `IDLE_STATE`
 * is too generic to expose.
 *
 * `class1_state_functions` is a 0x4c-byte COMMON array, so it holds nineteen
 * handlers -- state 19 is the count, not a state.
 */
#define CLASS1_T30_SILENCE_BEFORE_PREAMBLE_STATE	0
#define CLASS1_T30_PREAMBLE_STATE			1
#define CLASS1_SEND_HDLC_BUFFER_STATE			2
#define CLASS1_SEND_HDLC_BETWEEN_BUFFER_STATE		3
#define CLASS1_HDLC_RECEIVE_LOOK_CARRIER_STATE		4
#define CLASS1_HDLC_RECEIVE_STATE			5
#define CLASS1_HDLC_RECEIVE_BETWEEN_BUFFERS_STATE	6
#define CLASS1_HDLC_EMULATE_RECEIVE_STATE		7
#define CLASS1_IDLE_STATE				8
#define CLASS1_TX_SCRAMBLED_ONES_STATE			9
#define CLASS1_TX_DATA_STATE				10
#define CLASS1_TX_NULLS_STATE				11
#define CLASS1_RX_LOOK_CARRIER				12
#define CLASS1_RX_DATA_STATE				13
#define CLASS1_ANSWER_TONE_STATE			14
#define CLASS1_SEND_SILENCE_STATE			15
#define CLASS1_RECIEVE_SILENCE_STATE			16
#define CLASS1_CHDLCTX_OFF_STATE			17
#define CLASS1_TX_SILENCE_BEFORE_SCRM_ONES		18
#define CLASS1_MAX_STATES				19

/*
 * The status codes, from `status_names` (.rodata, eleven pairs).
 * `ctx->status` holds one of these and `fax_class1_progress` returns it.
 */
#define FAX_CLASS1_NO_MESSAGE			0
#define FAX_CLASS1_OK				1
#define FAX_CLASS1_ERROR			2
#define FAX_CLASS1_OK_NO_CARRIER		3
#define FAX_CLASS1_ERROR_NO_CARRIER		4
#define FAX_CLASS1_ERROR_ON_HOOK		5
#define FAX_CLASS1_CONNECT			6
#define FAX_CLASS1_NO_CARRIER			7
#define FAX_CLASS1_NO_CARRIER_NO_MESSAGE	8
#define FAX_CLASS1_OTHER_CARRIER		9
#define FAX_CLASS1_ACCEPT_RATE			10

/*
 * `states_names` (twenty entries), `status_names` (eleven) and
 * `command_names` (six) -- see class1.c for the derivation.  FILE-LOCAL in
 * the object (`r`, `nm`), so all three are `static` in class1.c and declare
 * nothing here; a differential test that names one declares it itself and
 * reaches it through the globalized test copy (tools/testvisible.py), which
 * the partial-link candidate keeps LOCAL.
 */
struct class1_name {
	int id;
	char *name;
};

/**
 * The high-pass filter `fax_class1_progress`'s IIR tick runs (`iir_coeff`,
 * above) -- ten shorts, the object's own symbol name (`nm`: `r
 * FAX_HP_COEFF`). `fax_class1_create` is its only writer.  FILE-LOCAL in the
 * object, so `static` in class1.c; it is a data symbol, so no test can name
 * it directly except through the globalized test copy (tools/testvisible.py).
 */

/*
 * `fax_class1_command`'s own `cmd` argument -- `command_names`'s six ids,
 * the object's own strings verbatim. Not to be confused with
 * `FAXC1_FTS`/etc (`fax.h`), `FAX_class1_command`'s OWN `cmd` numbering,
 * which is a different space that `voice.c` remaps into this one.
 */
#define FAX_CLASS1_TH_COMMAND	0
#define FAX_CLASS1_TM_COMMAND	1
#define FAX_CLASS1_RM_COMMAND	2
#define FAX_CLASS1_RH_COMMAND	3
#define FAX_CLASS1_TS_COMMAND	4
#define FAX_CLASS1_RS_COMMAND	5

/*
 * The silence detector's threshold, and the block it emits. `fax_class1_
 * progress` calls the second `TxSmpCnt` (its own name), complaining "ERROR:
 * TxSmpCnt != 160 !!!" when a handler leaves anything else behind.
 */
#define CLASS1_SILENCE_THRESHOLD	0x64
#define CLASS1_BLOCK_SAMPLES		0xa0

/*
 * The host-link framing bytes. DLE doubles itself in the data and DLE ETX
 * ends a buffer; the object's own string for a DLE that reached the escape
 * arm is "CLASS1: DLE %1X in data".
 */
#define CLASS1_DLE	0x10
#define CLASS1_ETX	0x03

/*
 * `flags004` bit 4. Both `_handle_hdlc_input` and `_handle_hdlc_input_close`
 * test it at end of frame and set `frame_end_latch` when it is on, and
 * nothing else in the object touches either -- so the site is established
 * and the meaning is not: the name records what the bit gates, not what it
 * configures.
 */
#define CLASS1_FLAG_FRAME_END_LATCH	0x10

/*
 * `_handle_data_input`'s two limits: the run of zero elements it appends
 * after DLE ETX, and the index past which it stops appending them.
 */
#define CLASS1_ETX_PAD_ELEMENTS		20
#define CLASS1_ETX_PAD_LIMIT		0x7ff

/*
 * `_hdlc_emulate_receive_state`'s own stack table of record lengths: twelve
 * 4-byte entries, sized from the function's own stack frame (the largest
 * outgoing call leaves room for exactly this many before the table starts).
 * Nothing bounds the PARSE loop that fills it against this count; more than
 * twelve records in one call overruns it exactly as it would in the object,
 * and is reproduced rather than guarded.
 */
#define CLASS1_EMU_MAX_FRAMES		12

/**
 * @brief SEND_SILENCE_STATE's own re-init: arm the silence countdown.
 *
 * @param ctx      The fax session.
 * @param samples  A sample count, halved into `countdown` with a floor of 1.
 * @return The halved value, though no known caller reads it.
 */
int _send_silence_state_init(struct fax_class1 *ctx, int samples);

/**
 * @brief RECIEVE_SILENCE_STATE's own re-init: arm the silence countdown.
 *
 * @param ctx      The fax session.
 * @param samples  A sample count, halved into `countdown` with a floor of 1.
 * @return The halved value, though no known caller reads it.
 */
int _recieve_silence_state_init(struct fax_class1 *ctx, int samples);

/**
 * @brief IDLE_STATE's own re-init.
 *
 * Reads nothing.  Arity not settled by the object (`xor eax; ret`, three
 * bytes); one context pointer is declared because that is what the rest of
 * the `*_state_init` family takes.
 *
 * @param ctx  The fax session.
 * @return Always 0.
 */
int _idle_state_init(struct fax_class1 *ctx);

/**
 * The state handler contract: nine arguments, read off `fax_class1_
 * progress`'s own marshalling before it dispatches
 * `class1_state_functions[ctx->state](...)`.
 *
 *   ctx        the session.  Certain -- every handler writes its fields.
 *   rx         the received block.  `_recieve_silence_state` hands it to
 *              `FPM_rms(const short *, unsigned short)`, which types it.
 *   tx         the block to transmit.
 *   word3      not read by any handler this batch writes.  Only the
 *   word4      WIDTH is established (a 32-bit argument slot); `int` is
 *   word7      the least claim, and is not a claim that they are ints.
 *   rx_count   how many samples `rx` holds.  `fax_class1_progress` passes
 *              the address of its OWN copy, not its caller's pointer, so a
 *              handler writing through this does not reach the caller.
 *   tx_count   how many samples the handler left in `tx`.  The author's
 *              word is `TxSmpCnt`: `fax_class1_progress` checks
 *              `*tx_count == 160` right after the dispatch and prints
 *              "ERROR: TxSmpCnt != 160 !!!" when it is not.
 *   word8      read at entry and written at exit by `_recieve_silence_
 *              state`, and by nothing else here.  A non-zero value on the
 *              way IN abandons the wait -- the object's own string is
 *              "Abort waiting for silence!" -- and 5 is written into it
 *              when energy appears.  Two readings fit (a caller's
 *              abort/result word, or the next state, since 5 is
 *              HDLC_RECEIVE_STATE) and nothing here chooses between them,
 *              so it keeps a neutral name.
 *
 * Every handler returns 0 in the three reconstructed so far; nothing is
 * known to read the return.
 */
typedef int (*class1_state_fn)(struct fax_class1 *ctx, const short *rx,
			       short *tx, int word3, int word4,
			       int *rx_count, int *tx_count, int word7,
			       int *word8);

/**
 * `.bss` (COMMON in the object, 0x4c bytes = 19 * 4), installed by
 * `fax_class1_create` and indexed with no bounds check by `fax_class1_
 * progress`'s own dispatch. Declared here so a test can install handlers
 * into it directly -- it starts all-NULL, same as the object's own COMMON
 * storage.
 */
extern class1_state_fn class1_state_functions[19];

/**
 * @brief IDLE_STATE (8).
 *
 * Zero `*rx_count` samples of `tx` and report that many -- or, when
 * `*rx_count` is not positive, a whole block of 160. Reads no field of the
 * session at all.  FILE-LOCAL in the object (`t`), so it is `static` in
 * class1.c and declares nothing here; the tests that name it declare it
 * themselves and reach it through the globalized test copy
 * (tools/testvisible.py).
 */

/**
 * @brief SEND_SILENCE_STATE (15).
 *
 * Counts one block off `countdown`, transmits a block of silence, and
 * reports #FAX_CLASS1_OK_NO_CARRIER once the countdown reaches zero. The
 * state does not leave itself -- it is `fax_class1_progress` that acts on
 * the status.  FILE-LOCAL in the object (`t`), so it is `static` in
 * class1.c and declares nothing here; the tests that name it declare it
 * themselves and reach it through the globalized test copy
 * (tools/testvisible.py).
 */

/**
 * @brief RECIEVE_SILENCE_STATE (16) -- the author's spelling.
 *
 * Transmits a block of silence, measures the received block's RMS into
 * `energy`, and:
 *   - `*word8` non-zero: abandon -- OK_NO_CARRIER, go idle.
 *   - `energy` above the threshold: restart the count, NO_MESSAGE, `*word8 = 5`.
 *   - `energy` at or below the threshold: one more silent block; once
 *     that reaches `countdown`, OK_NO_CARRIER and go idle.
 *
 * The threshold test is `> 100` on a signed short, so a negative energy
 * counts as silence -- `FPM_rms` cannot return one, and the test is written
 * as the object has it rather than as it would have to be if it could.
 * FILE-LOCAL in the object (`t`), so it is `static` in class1.c and declares
 * nothing here; the tests that name it declare it themselves and reach it
 * through the globalized test copy (tools/testvisible.py).
 */

/**
 * @brief Zero `count` elements of `buf`.
 *
 * `.text` 0x092b70, 28 bytes, and the TU's first global -- immediately
 * before _send_silence_state_init().
 *
 * @param buf    Buffer to zero.
 * @param count  Number of elements to zero.
 * @return The object's own return value is whatever the loop counter holds
 *         on exit -- `count` on a normal completion, 0 if `count` was not
 *         positive -- rather than the argument itself; no reconstructed
 *         caller reads it.
 */
int _put_silence(short *buf, int count);

/**
 * @brief Translate a T.30 rate code to a modulation and bit rate.
 *
 * The four two-code ranges are V.17 (with/without short training); the
 * singles below them select V.29 (mod 1) and V.27ter (mod 0). A code
 * outside the table writes nothing -- both outputs keep their callers'
 * values -- and the ranges are separate ifs in the object, so 0x91/0x92
 * would fall through the later equality tests unharmed either way.
 *
 * @param code  A T.30 modem-rate code.
 * @param mod   Out: the modulation (0 V.27ter, 1 V.29, 2 V.17), unchanged
 *              on a code outside the table.
 * @param rate  Out: the bit rate, unchanged on a code outside the table.
 */
void _set_modem_rate(int code, int *mod, int *rate);

/**
 * @brief Bits per symbol for a bit rate.
 * @param rate  A bit rate.
 * @return Bits per symbol, or 0 for a rate not in the table.
 */
int _sym_size(int rate);

/**
 * @brief Report a diagnostic value from the session.
 *
 * Selector 0: `*out = gain_attenuation_db`. Selector 1: `*out` = the
 * unsigned short at `tx_fifo + 0xc`, or 0 with `tx_fifo` null. Any other
 * selector writes nothing.
 *
 * @param ctx  The fax session.
 * @param sel  0 or 1; see above.
 * @param out  Where the selected value is written.
 * @return Always 0.
 */
int fax_class1_info(struct fax_class1 *ctx, int sel, int *out);

/**
 * @brief Unimplemented in the object: reads nothing, returns 0.
 *
 * Arity not settled by the object (`xor eax; ret`); one pointer is
 * declared as the least claim compatible with the name.
 */
int fax_class1_GetConstalation(void *ctx);

/**
 * @brief Tear the whole session down.
 *
 * `.text` 0x0093bf0, 347 bytes. Frees, in the object's own order:
 * `vmi_c_cfg` (and its one sub-allocation at +0x10), `vmi_c`, `vmi_a_cfg`
 * (the same sub-allocation shape as `vmi_c_cfg`), `vmi_a` -- then, when
 * both `modem_vmi` and `vmi_b` are non-null, the current data modem
 * (`_delete_data_rx_modem` when `modem_direction == 1`, `_delete_data_tx_
 * modem` otherwise) -- then `tx_fifo` and `tone` -- and finally the session
 * object itself.
 *
 * @param ctx  The fax session to tear down.
 * @return Always 1.
 */
int fax_class1_delete(struct fax_class1 *ctx);

/**
 * @brief Report the active VMI handle's status.
 *
 * `.text` 0x0093b50, 150 bytes. Selects `vmi_a` for `state` 4..6 and
 * `vmi_b` for 12..13 (F10104); anything else touches nothing and returns 0.
 * A matching range always returns 1, never the wrapped call's own result.
 *
 * @param ctx           The fax session.
 * @param modem_status  Forwarded unchanged into the local status record's
 *                       `modem_status` field (`struct faxvmi_status`'s own
 *                       "IN" contract, `faxvmi.h`); `FAXVMI_status` treats a
 *                       non-NULL value as a pointer into the wrapped
 *                       modulation's own status buffer and does nothing
 *                       with it otherwise itself.
 * @return 1 if `state` is in a reported range, 0 otherwise.
 */
int fax_class1_status(struct fax_class1 *ctx, void *modem_status);

/**
 * @brief The session dispatcher.
 *
 * `.text` 0x0936d0, 1,145 bytes. `ctx->modem_rate_code` (F10051/F10052) is a
 * T.30 rate code selecting which of three already-complete modulations'
 * quality latch to test. `rx` is `short *`, not `const`, because this
 * function (not any state handler) hands it to `FPM_iir_filt_II`, which
 * filters in place; `word7` is `int *`, not a bare int, because this
 * function writes `*word7 = 0` on every call.
 *
 * What it does, in the object's own order:
 *
 *   1. When `ctx->f127c` is nonzero, scan up to `*rx_count / 8` samples of
 *      `rx`, counting the nonzero ones into `ctx->f127c`; if that exceeds
 *      `*rx_count / 16`, reset it to 0.
 *   2. Advance the clock: `clock_frac += 2`, rolling into `clock_sec` past
 *      99. Clear `*word7` and `status` unconditionally. When `iir_enabled`
 *      is nonzero, run one `FPM_iir_filt_II` tick over `rx` (2 sections,
 *      `(short)*rx_count` samples, coefficients `iir_coeff`, state
 *      `iir_state`).
 *   3. Dispatch: `class1_state_functions[ctx->state](ctx, rx, tx, word3,
 *      word4, &local_rx_count, tx_count, word7, word8)`, where
 *      `local_rx_count` is this function's own copy of `*rx_count` (the
 *      handler never sees the caller's pointer). No bounds check on
 *      `ctx->state` -- reproduced, not guarded.
 *   4. If `ctx->state` changed across the dispatch, log the transition
 *      (`states_names`, both old and new) at debug level > 1. Update
 *      `prev_state`.
 *   5. If `*tx_count != CLASS1_BLOCK_SAMPLES`, log "ERROR: TxSmpCnt != 160
 *      !!!" at debug level > 1.
 *   6. Apply a delayed status: if `delayed_status_countdown` is nonzero and
 *      reaches 0 on this call, `status = delayed_status`.
 *   7. If `status` is still #FAX_CLASS1_NO_MESSAGE, switch on
 *      `modem_rate_code` -- the four V.17 long-training codes only, the two
 *      V.29 codes, or the two V.27ter codes -- and, only when
 *      `modem_direction == 1`, walk `vmi_b->link->int_0014` to the active
 *      modem and test/clear that modulation's own quality latch
 *      (`V17RXS_SHORT_4FB2` at `V17RX_OBJ_STATE`, `V29RX_SHORT_4F62` at
 *      `V29_OBJ_RX`, `V27RX_Q_FLAG` at `V27_OBJ_RX`). A set latch becomes
 *      `status = FAX_CLASS1_ACCEPT_RATE`.
 *   8. If `status` is non-zero at this point, log it (`status_names`) at
 *      debug level > 1.
 *   9. Return `status`.
 *
 * @param ctx        The fax session.
 * @param rx         The received block, filtered in place when `iir_enabled`.
 * @param tx         The block to transmit.
 * @param word3      See the state-handler contract above.
 * @param word4      See the state-handler contract above.
 * @param rx_count   How many samples `rx` holds.
 * @param tx_count   Out: how many samples the dispatched handler left in `tx`.
 * @param word7      Cleared to 0 on every call, then passed through to the
 *                   dispatched handler.
 * @param word8      See the state-handler contract above.
 * @return The session's current #FAX_CLASS1_* status code.
 */
int fax_class1_progress(struct fax_class1 *ctx, short *rx, short *tx,
			int word3, int word4, int *rx_count, int *tx_count,
			int *word7, int *word8);

/**
 * @brief ANSWER_TONE_STATE (14). `.text` 0x092d60, 94 bytes.
 *
 * Counts one block off `countdown` (armed by `fax_class1_create`'s own
 * `answer_tone_ms`-derived `answer_tone_blocks`); once `countdown` exceeds
 * it, hands off to `cHDLCtx_preamble_state_init` (its return discarded)
 * instead of generating another block. Otherwise generates one block of
 * the session's tone (`ctx->tone`, `FPM_TONE_generate`) and reports
 * `*tx_count = CLASS1_BLOCK_SAMPLES`. Returns 0 on both paths.
 * FILE-LOCAL in the object (`t`), so it is `static` in class1.c and declares
 * nothing here; the tests that name it declare it themselves and reach it
 * through the globalized test copy (tools/testvisible.py).
 */

/**
 * @brief The session dispatcher (control side).
 *
 * `.text` 0x093420, 615 bytes. Logs the incoming command (its own name,
 * off `command_names`) and, unless `cmd == FAX_CLASS1_RH_COMMAND`, clears
 * `superframe_len` -- both unconditionally, before dispatching. A `cmd`
 * outside 0..5 does the log-and-clear and returns 1 without dispatching
 * anything. See class1.c for each command's own derivation.
 *
 * @param ctx   The fax session.
 * @param cmd   One of the six #FAX_CLASS1_*_COMMAND codes above.
 * @param arg3  A T.30 rate code (TH/TM/RM/RH) or a raw sample count
 *              (TS/RS, the same argument `_send_silence_state_init`/
 *              `_recieve_silence_state_init` take).
 * @param arg4  Read only by the TM command, into `silence_blocks`.
 * @return Always 1; nothing reconstructed reads back any other value.
 */
int fax_class1_command(struct fax_class1 *ctx, int cmd, int arg3, int arg4);

/**
 * @brief Build or reinitialise a Class 1 fax session.
 *
 * `.text` 0x092e20, 1,532 bytes -- the largest single piece of `class1.c`.
 * `existing == NULL` allocates a fresh #CLASS1_MODELLED_BYTES-ish object
 * (though the real object extends past what this batch models);
 * non-NULL reinitialises it in place, including re-installing the global
 * `class1_state_functions` table and rebuilding the session's tone
 * generator -- but not rebuilding `vmi_c`/`vmi_a` (the V.21 handles), which
 * only the fresh path builds. `cfg` is never NULL in any traced caller and
 * this function does not check it either. See class1.c for the full
 * derivation, including the `cfg->mode == 2` answer-tone-only branch and
 * the nineteen `class1_state_functions` installs (all in `states_names`'
 * own order).
 *
 * @param existing  NULL for a fresh session, or an existing one to reinitialise.
 * @param cfg       Configuration; see struct fax_class1_cfg.
 * @return The (possibly freshly allocated) session, or `existing` on every
 *         path -- never NULL, even on an internal allocation failure this
 *         batch could not trace a check for.
 */
struct fax_class1 *fax_class1_create(struct fax_class1 *existing,
				     const struct fax_class1_cfg *cfg);

#endif /* DSPLIB_CLASS1_H */
