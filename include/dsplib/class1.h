/*
 * class1.h -- Class 1 fax: the session object, as far as this batch reads it.
 *
 * THE FAX PHASE IS DELIBERATELY LAST (see CLAUDE.md); what is here now is
 * the handful of `class1.c`-span symbols that nothing in the object reaches
 * -- exported API with no internal referrer, finding F8320's bucket --
 * written on their own merit.  So this header models ONLY what those leaves
 * touch, and the pad regions are expected to become fields when the fax
 * phase reads `fax_class1_create` (0x092e20) properly.
 *
 * What is already established about the object from this batch's reads:
 *
 *   +0x1200 / +0x120c / +0x1210
 *                       three more torn-down-by-`fax_class1_delete` fields
 *                       (vmi_c, vmi_c_cfg, vmi_a_cfg) -- see that function's own
 *                       comment in class1.c for what little is established
 *   +0x1204 / +0x1208   two handles handed to FAXVMI_status by
 *                       fax_class1_status -- receive-side for states 4..6,
 *                       transmit-side for 12..13 (which side is which is
 *                       NOT settled; the state numbers are).  +0x1208 is also
 *                       what `_delete_data_rx_modem` and `_delete_data_tx_
 *                       modem` hand `FAXVMI_delete`, which types both as
 *                       `struct faxvmi *`
 *   +0x1214             the current data modem's `struct faxvmi_cfg *`, freed
 *                       by whichever of `_delete_data_rx_modem` /
 *                       `_delete_data_tx_modem` tears the modem down -- ONE
 *                       field, reused across directions, since Class 1 fax
 *                       is half-duplex
 *   +0x121c             the session state machine's state number.  0x0f and
 *                       0x10 are the send/receive silence states (their
 *                       init functions store exactly those), 4..6 and
 *                       12..13 are the ranges fax_class1_status reports on
 *   +0x1228             a countdown the state inits arm: silence samples
 *                       over 2 for the silence states, 0 for tx-nulls and
 *                       the HDLC between-buffer state
 *   +0x1250             armed to 1 by _handle_hdlc_input_open, and read
 *                       back minus one into +0x000 by .._close
 *
 * `fax_class1_create` writes at least to +0x12dc and takes `&ctx->0x12e0`
 * as a sub-block, so the object extends past this struct; nothing here
 * allocates one, and CLASS1_MODELLED_BYTES is a modelling bound, not a size
 * claim.
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
						 * LENGTH-PREFIXED records -- entry
						 * `i` is a length, the next `i+1`
						 * entries are the record's own
						 * (byte-valued) elements, and the
						 * NEXT record starts right after
						 * -- read only up to `superframe_len` bytes
						 * of it, in the same shape
						 * `_handle_hdlc_input_close`
						 * leaves a frame in.  NAMED ON
						 * RANK-1 EVIDENCE:
						 * `_hdlc_receive_between_buffers_state`'s
						 * own bounds-check arm prints
						 * "SuperFrame full, skipping HDLC
						 * frame!\n" (`.rodata.str1.4`
						 * 0x1278c) when a record will not
						 * fit in it -- the author's own word
						 * for this buffer, not a guess; see
						 * class1tx.c.  Sized from ADDRESS
						 * CONTIGUITY alone (it runs right up
						 * to `vmi_c`'s own +0x1200, the next
						 * already-established field, with no
						 * access this batch saw past that)
						 * rather than a size constant
						 * anywhere -- usage inference for the
						 * SIZE, rank-1 for the NAME         */
	struct faxvmi *vmi_c;		/* +0x1200 a THIRD FAXVMI handle,
						 * torn down by `fax_class1_delete`
						 * before `vmi_a`/`vmi_b` -- typed
						 * the same way, by its own
						 * `FAXVMI_delete` call.  Nothing
						 * reconstructed so far establishes
						 * which of the three roles
						 * (V.21, data, or something else)
						 * this one plays                   */
	struct faxvmi *vmi_a;		/* +0x1204 FAXVMI handle, states
					 * 4..6 of fax_class1_status.  Typed
					 * `struct faxvmi *` on vmi_b's own
					 * evidence -- the comment above
					 * calls the two "two handles" of
					 * the same kind                     */
	struct faxvmi *vmi_b;		/* +0x1208 FAXVMI handle, states
					 * 12..13.  Typed from
					 * `_delete_data_rx_modem`'s and
					 * `_delete_data_tx_modem`'s own
					 * call, `FAXVMI_delete(struct
					 * faxvmi *)` -- evidence class 2    */
	struct faxvmi_cfg *vmi_c_cfg;	/* +0x120c the `struct faxvmi_cfg`
						 * `fax_class1_create`'s fresh path
						 * builds for the V.21 TX control
						 * channel, and hands to
						 * `FAXVMI_create(NULL, ...)` to
						 * build `vmi_c` -- rank 2: that
						 * function's own second parameter
						 * is `const struct faxvmi_cfg *`.
						 * `fax_class1_delete` frees it at
						 * TWO offsets when non-null --
						 * `*(p+0x10)` first, then `p` itself
						 * -- and `+0x10` of `struct
						 * faxvmi_cfg` is exactly
						 * `modem_cfg` (faxcfg.h), so the
						 * sub-allocation it owns is the
						 * `struct v21tx_cfg` underneath.
						 * RETYPED FROM `void *`: an earlier
						 * pass, written before
						 * `fax_class1_create` existed, could
						 * not see either owner            */
	struct faxvmi_cfg *vmi_a_cfg;	/* +0x1210 the same shape as
						 * `vmi_c_cfg`, but for the V.21 RX
						 * control channel and `vmi_a` --
						 * `fax_class1_create`'s fresh path
						 * builds a `struct v21rx_cfg` as its
						 * own `modem_cfg`.  Torn down the
						 * same way, one field later --
						 * `fax_class1_delete`'s own order is
						 * vmi_c_cfg, vmi_c, vmi_a_cfg, vmi_a */
	struct faxvmi_cfg *modem_vmi;	/* +0x1214 the CURRENT data modem's
					 * VMI config block -- one field,
					 * reused for whichever direction is
					 * active, since Class 1 fax is
					 * half-duplex.  `_delete_data_rx_
					 * modem` frees it (and, for a V.17
					 * receiver, three sub-allocations of
					 * the `struct v17rx_cfg` it owns
					 * first) and `_delete_data_tx_modem`
					 * frees it the same way with no
					 * sub-frees                         */
	int ans_org;			/* +0x1218 `fax_class1_create`'s own
						 * word, PROMOTED FROM `pad_1218`:
						 * its debug line names the local
						 * this field is copied from
						 * verbatim, "fax: fax_class1 will
						 * created (ans_org=%d, s7=%d)\n"
						 * (evidence class 1).  1 for the
						 * ordinary session
						 * (`struct fax_class1_cfg`'s own
						 * `mode == 1`), 2 for the
						 * answer-tone-only one (`mode ==
						 * 2`) -- so this is `mode`'s own
						 * value, copied through unchanged.
						 * Nothing reconstructed reads it
						 * back yet                        */
	int state;			/* +0x121c the session state        */
	int prev_state;			/* +0x1220 the state as of the LAST
					 * call to `fax_class1_progress` --
					 * compared against `state` after each
					 * dispatch to log a transition, then
					 * updated to match                 */
	int frame_end_latch;		/* +0x1224 set 1 on hdlc close when
					 * flags004 bit 4                   */
	int countdown;			/* +0x1228 armed by the state inits */
	int status;			/* +0x122c the FAX_CLASS1_* code
					 * fax_class1_progress RETURNS
					 * (0x93a43: it loads this and
					 * leaves it in eax)                */
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
						 * guessed one                      */
	int f1234;			/* +0x1234 the same function's other
						 * half: 0x8000 for every V.17
						 * rate and for V.29 9600, 0x75a2
						 * for V.29 7200, 0x4000 for both
						 * V.27ter rates.  Usage inference
						 * only, same as `f1230`            */
	int last_in_byte;		/* +0x1238 the LAST byte of the block
					 * _handle_data_input was handed,
					 * kept whether or not the block is
					 * consumed                         */
	int delayed_status;		/* +0x123c the object's OWN word,
					 * "STATUS = DELAYED_STATUS" (its
					 * debug line, rank-1 evidence):
					 * applied to `status` when
					 * `delayed_status_countdown` reaches
					 * zero                              */
	int delayed_status_countdown;	/* +0x1240 decremented once per
					 * `fax_class1_progress` call while
					 * nonzero; on the call it reaches
					 * zero, `status` is set from
					 * `delayed_status`                 */
	int modem_direction;		/* +0x1244 `fax_class1_delete` reads
						 * this to choose `_delete_data_rx_
						 * modem` (when ==
						 * CLASS1_MODEM_DIR_RX) or
						 * `_delete_data_tx_modem`
						 * (otherwise) for `modem_vmi`/
						 * `vmi_b` -- so it marks which
						 * direction the CURRENT data modem
						 * is.  Both values it is ever
						 * observed to hold are named below
						 * (`fax_class1_command`'s own two
						 * writers, class1.c: the RM command
						 * sets CLASS1_MODEM_DIR_RX, the TM
						 * command CLASS1_MODEM_DIR_TX);
						 * whether a THIRD value is possible
						 * is not established               */
	int current_mod;		/* +0x1248 the modulation currently
						 * installed at `modem_vmi`/`vmi_b`
						 * -- 0 V.27ter, 1 V.29, 2 V.17.
						 * `_init_receiver`/`_init_
						 * transmitter` (class1rx.c/
						 * class1tx.c) compare a freshly
						 * derived modulation index against
						 * this field to choose their
						 * reinit path over a fresh create,
						 * and write it back on every exit.
						 * Evidence class 1: both the
						 * compare and every store are
						 * `cmpl`/`movl`, forced 32-bit    */
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
						 * own code space, exactly (finding
						 * F10051): `fax_class1_progress`
						 * switches on it to pick which
						 * modulation's quality latch to
						 * test.  NOT YET the name of a
						 * writer, only of this one reader   */
	struct fpm_tone *tone;		/* +0x1258 torn down by
						 * `fax_class1_delete`'s own
						 * `FPM_TONE_delete` call -- a tone
						 * generator/detector, CONFIRMED for
						 * `CLASS1_ANSWER_TONE_STATE` now
						 * that `_answer_tone_state` is
						 * written: it hands this straight
						 * to `FPM_TONE_generate`, evidence
						 * class 2.  `fax_class1_create`
						 * builds it at one of two
						 * frequencies (1100 Hz ordinary
						 * session, 2100 Hz -- the T.30 CED
						 * answer tone -- the
						 * `ans_org == 2` one), both with
						 * `scale = 6400` and phase
						 * reversals disabled            */
	int tone_cadence_phase;		/* +0x125c a 0/1 phase flag in
					 * `_hdlc_receive_look_carrier_state`'s
					 * own tone-cadence machine: 0 is the
					 * silence phase (accumulate `tone_cadence_timer` to
					 * 0x5dc0/24000 then flip to 1), 1 is
					 * the tone phase (`FPM_TONE_generate`
					 * instead of `_put_silence`, accumulate
					 * `tone_cadence_timer` to 0xfa0/4000 then flip back
					 * to 0).  Usage inference only         */
	int tone_cadence_timer;		/* +0x1260 the sample accumulator that
					 * phase flips on -- reset to 0 and
					 * bumped by `CLASS1_BLOCK_SAMPLES` per
					 * call in EITHER phase, never both at
					 * once.  Usage inference only          */
	int cng_enabled;		/* +0x1264 PROMOTED FROM the neutral
					 * `f1264`, and renamed on rank-1
					 * evidence: `fax_class1_create`
					 * UNCONDITIONALLY clears it to 0
					 * first (both `cfg->mode` finishes
					 * reach that store), then the
					 * ORDINARY-session finish alone
					 * overrides it to `(cfg->disable_cng
					 * == 0)` -- the `mode ==
					 * CLASS1_ANS_ORG_ANSWER` finish never
					 * touches it again, so it stays 0
					 * (disabled) on that path regardless
					 * of `cfg->disable_cng`.  The
					 * object's OWN debug line at the
					 * override site is "CNG generation
					 * disabled" (no trailing newline --
					 * the object's own), printed exactly
					 * when the override ends up 0.  Reused
					 * as a general
					 * tone-cadence gate everywhere else it
					 * is read: 0 disables the whole
					 * `tone_cadence_timer`/`tone_cadence_phase` cadence machine
					 * (plain silence only) and decides
					 * whether a look-carrier timeout is
					 * reported as NO_CARRIER (gate==0) or
					 * silently absorbed into the cadence
					 * path (gate!=0).  Cleared both on a
					 * fresh entry to HDLC_RECEIVE_LOOK_
					 * CARRIER_STATE and on a hard timeout;
					 * `fax_class1_create` is the only
					 * reconstructed writer that can set it
					 * nonzero                             */
	int answer_tone_blocks;		/* +0x1268 PROMOTED FROM `pad_1268`:
					 * the countdown threshold
					 * `_answer_tone_state` compares
					 * `countdown` against.  `fax_class1_
					 * create` derives it from
					 * `cfg->answer_tone_ms / 20` (block
					 * period at the implied 8 kHz/160-
					 * sample rate; independently re-
					 * derived via the object's own
					 * `imul $0x66666667`/`sar $3`
					 * reciprocal, not guessed) when
					 * `cfg->answer_tone_ms` is nonzero,
					 * else a literal default of 150 (3
					 * seconds).  The object's own debug
					 * line for the derived case is
					 * "Answer tone length %d ms" --
					 * evidence class 1 for the FIELD's
					 * unit, usage inference for the name
					 * itself                              */
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
					 * `transmit_enabled`.  See that
					 * function's own comment in class1tx.c.
					 * Usage inference only                 */
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
					 * return (0 or 1, but stored as the
					 * full 32-bit `eax`) -- evidence
					 * class 2, typed by the callee whose
					 * return it holds verbatim           */
	int buffers_sent;		/* +0x1284 incremented once per call
					 * `hdlc_frame_done` came back nonzero.
					 * Evidence class 1: the object's own
					 * debug line names it, "At
					 * %2d.%02d[sec] Elapsed 1 second, send
					 * %d buffers\n" (`_t30_preabmle_state`)
					 */
	struct fax_fifo *tx_fifo;	/* +0x1288 fax_class1_info(1) reads
					 * an unsigned short at +0xc of it.
					 * Typed from `_delete_data_tx_modem`'s
					 * own call, `FIFO_delete(struct
					 * fax_fifo *)` -- evidence class 2  */
	int transmit_enabled;		/* +0x128c `_tx_scrambled_ones_state`'s
					 * own latch: 0 until `tx_connect_countdown` (above)
					 * reaches its one-shot zero, then 1 for
					 * the rest of the session.  Gates a
					 * second check (`tx_fifo_ready`, below) that
					 * decides between the ordinary
					 * FAXVMI_process path and a raw
					 * FIFO_read path.  Evidence class 1:
					 * the object's own debug line at the
					 * site that sets it is "At %2d.%02d
					 * [sec] ENABLE_TRANSMIT in _tx_
					 * scrambled_ones_state\n"              */
	int tx_bytes_per_block;		/* +0x1290 the `count` `_tx_nulls_
					 * state` asks `FIFO_read(ctx->tx_fifo,
					 * ...)` for on every call.  A FULL
					 * 32-bit field, NOT `unsigned short`:
					 * `_tx_scrambled_ones_state`'s own fill
					 * loop reads it with a plain `mov
					 * 0x1290(%ebx),%edx` (0x9d200+0xa0)
					 * and uses the whole register directly
					 * as a loop bound and in an unmasked
					 * `cmp` against a FIFO count -- not
					 * `movzwl`, so the field itself is
					 * 32 bits wide, corroborated by `_tx_
					 * nulls_state`'s own SECOND access at
					 * the same offset (0x9d104, a plain
					 * `cmp 0x1290(%ebx),%eax`) alongside
					 * its first, narrower `movzwl` one
					 * (0x9d0e2) that merely narrows the
					 * RESULT for a 16-bit call argument
					 * (class1tx.c's own "dead extension
					 * follows the destination, not the
					 * field" rule).  A test that modelled
					 * this as `unsigned short` plus two
					 * bytes of `pad_1292` left those two
					 * bytes as fill()-random garbage,
					 * which this loop-bound read turned
					 * into a value near 2^29 and walked
					 * off the struct -- segfault, not a
					 * defect in the callee (finding in
					 * this batch's own entry).  The
					 * object's own writer of a small
					 * value into a genuine `int` leaves
					 * the upper 16 bits zero, which is
					 * why nothing FIRST caught this on
					 * the object's own side; the evidence
					 * class is 1 (forced instruction
					 * width) for the WIDTH, usage
					 * inference for the MEANING          */
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
					 * fresh every call via `setge` on that
					 * comparison.  When this AND
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
	int s7_timeout;			/* +0x12b4 T.30's own name for this:
					 * `_hdlc_receive_look_carrier_state`'s
					 * own debug line is "...curent_timeout
					 * = %d, No carrier in _hdlc_receive_
					 * look_carrier_state, S7 = %d[sec]\n"
					 * with this field as the last arg
					 * (evidence class 1).  That function
					 * multiplies it by 50, or by 8000 and
					 * divides by `*rx_count`, to get a
					 * block-count timeout threshold; the
					 * `*rx_count == 0` fallback (`* 50`) is
					 * exactly what the general formula
					 * reduces to at the usual 160-sample
					 * block and an implied 8 kHz rate --
					 * corroborating, not a second
					 * derivation                          */
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
					 * walks `ctx->vmi_a->link->int_0014` to
					 * a pointer, then that pointer's own
					 * +0x50 (`V21RX_OBJ_DSP`, v21fax.h) to
					 * another, and stores the sign-extended
					 * shorts at +0x30/+0x32 of THAT into
					 * rx_agc_mult/rx_agc_shift (widened to
					 * the full 32-bit store the object
					 * makes).  RETYPED, NOT LEFT NEUTRAL:
					 * `V21RX_OBJ_DSP` is `rx + 0x50` and
					 * `struct fpm_agc`'s own two fields
					 * `mult`/`shift` (gain mantissa Q15,
					 * gain exponent as a left shift,
					 * fpm_agc.h) sit at +0x24/+0x26 of the
					 * `fpm_agc` that `V21RX_create`/
					 * `DemodDataV21` build at `dsp + 0x0c`
					 * -- `0x0c + 0x24 = 0x30`, `0x0c +
					 * 0x26 = 0x32`, exactly the two offsets
					 * read here.  Evidence class 2 (a
					 * modelled struct reached by tiling,
					 * the same rank v17fax.h's
					 * `V17RXS_SRE_ADAPT` family uses) --
					 * this WITHDRAWS an earlier "plausibly
					 * a measured baud rate or frequency
					 * pair" guess at this site, which the
					 * tiling contradicts             */
	int rx_agc_shift;		/* +0x12c4 see rx_agc_mult             */
	int superframe_countdown;	/* +0x12c8 `_hdlc_emulate_receive_
						 * state`'s own between-record
						 * countdown: decremented once per
						 * call, and a value that was <= 0
						 * BEFORE the decrement is what
						 * fires the next record (or the
						 * idle transition once `superframe`
						 * is exhausted).  Reset to 2      */
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
						 * +0x1c; V.17 -> +0x5c, then +0x20
						 * (`dis.py`, 0x944b3-0x944c8,
						 * 0x946e5-0x94704, 0x94689-0x946b3).
						 * Nothing establishes what it MEANS
						 * beyond "propagated to the wrapped
						 * modem" -- usage inference only,
						 * kept neutral on purpose          */
	int gain_attenuation_db;	/* +0x12d8 fax_class1_info(0);
					 * create clears it.  NAMED ON RANK-1
					 * EVIDENCE: `_hdlc_receive_look_
					 * carrier_state`'s own gain-request
					 * scan (class1tx.c) stores `12 - 3*i`
					 * here on a match against
					 * `HDLC_LOOK_CARRIER_LEVELS[i]` and
					 * prints the object's own debug line
					 * "Gain Attenuation Reuqest: +%d[dB],
					 * avg_rms = %d" with this field as
					 * the first `%d` (the author's own
					 * typo, kept) -- so the field is a
					 * requested attenuation in dB (12, 9,
					 * 6 or 3), read back by
					 * `fax_class1_info`'s own selector 0 */
	const short *iir_coeff;		/* +0x12dc create: a .rodata ptr.
					 * Typed from `fax_class1_progress`'s
					 * own call, `FPM_iir_filt_II(short *,
					 * const short *coeff, ...)` -- the
					 * coefficient table for the 2-section
					 * filter below (evidence class 2)   */
	short iir_state[8];		/* +0x12e0 `FPM_iir_filt_II`'s own
					 * state, 4 words/section per
					 * `fpm_iir.h` times the 2 sections
					 * `fax_class1_progress` passes as a
					 * literal -- sized from the callee's
					 * own contract, not guessed          */
	int iir_enabled;		/* +0x12f0 gates whether
					 * `fax_class1_progress` runs the IIR
					 * filter tick at all this call.
					 * Usage inference only               */
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

#define CLASS1_MODELLED_BYTES	0x12f4

/*
 * `fax_class1_create`'s SECOND ARGUMENT -- a 0x18-byte configuration record
 * read at six fixed offsets (`dis.py`, 0x92e4d..0x92fbf) and nowhere named
 * by the object as a struct (`FAX_create` builds one on its own stack, not
 * from a `.data`/`.rodata` template).  Every field here is usage inference
 * except `answer_tone_ms`, which the object's own debug line types
 * ("Answer tone length %d ms") and `mode`, which the object's own debug line
 * NAMES ("ans_org", `FAX_create`'s print -- see fax.h): both rank-1
 * evidence, the strongest this tree recognises.  `s7_timeout` is named for
 * the ALREADY-ESTABLISHED `struct fax_class1::s7_timeout` field it is copied
 * into unconditionally, one field over.
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
	int iir_enable;			/* +0x0c nonzero -> ctx->iir_enabled = 1.  Every
				 * traced caller (`FAX_create`) sets this,
				 * so no reconstructed caller leaves the IIR
				 * tick off; usage inference on the NAME,
				 * not the effect (class1.h's own `iir_enabled`
				 * note)                                     */
	int answer_tone_ms;		/* +0x10 nonzero -> ctx->answer_tone_blocks =
				 * this / 20 (block period).  Zero -> the
				 * object's own literal default, 150 blocks
				 * (3 seconds).  Rank-1 evidence: "Answer
				 * tone length %d ms" is the object's own
				 * debug line at the site that derives it   */
	int disable_cng;		/* +0x14 -> ctx->cng_enabled = (this == 0).
				 * Rank-1 evidence: "CNG generation
				 * disabled\n" is the object's own debug
				 * line, printed exactly when this is
				 * nonzero                                   */
};

/* `struct fax_class1_cfg::mode` -- see the struct's own field comment. */
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
 * THE STATE NUMBERS ARE THE AUTHOR'S OWN, read out of `states_names`
 * (.rodata 0x9360, twenty {int, char *} pairs that `fax_class1_progress`
 * searches at 0x93826 to log a transition).  That is the strongest class of
 * evidence this tree recognises, so the spellings are kept exactly as
 * written -- `RECIEVE` included.  The `CLASS1_` prefix is ours; the object's
 * names carry none and `IDLE_STATE` is too generic to expose.
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
 * And the status codes, from `status_names` (.rodata 0x9300, eleven pairs).
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
 * `command_names` (six) -- see class1.c for the derivation and D1330 for why
 * they are global here where the object has them file-local.  Declared here
 * so a test can compare them against the blob's own copy without a reader in
 * `src/` yet.
 */
struct class1_name {
	int id;
	char *name;
};

extern struct class1_name states_names[20];
extern struct class1_name status_names[11];
extern struct class1_name command_names[6];

/*
 * The high-pass filter `fax_class1_progress`'s IIR tick runs (`iir_coeff`,
 * above) -- `.rodata` 0x9430, ten shorts, the object's OWN symbol name
 * (`nm`: `r FAX_HP_COEFF`).  `fax_class1_create` is its only writer.
 */
extern const short FAX_HP_COEFF[10];

/*
 * `fax_class1_command`'s own `cmd` argument -- `command_names`'s six ids,
 * the object's own strings verbatim (.rodata 0x9400, six {int, char *}
 * pairs).  Rank-1 evidence, the strongest class this tree recognises.  Not
 * to be confused with `FAXC1_FTS`/etc (`fax.h`), `FAX_class1_command`'s OWN
 * `cmd` numbering, which is a DIFFERENT space that `voice.c` remaps into
 * this one.
 */
#define FAX_CLASS1_TH_COMMAND	0
#define FAX_CLASS1_TM_COMMAND	1
#define FAX_CLASS1_RM_COMMAND	2
#define FAX_CLASS1_RH_COMMAND	3
#define FAX_CLASS1_TS_COMMAND	4
#define FAX_CLASS1_RS_COMMAND	5

/*
 * The silence detector's threshold, and the block it emits.  Both are
 * immediates in `_recieve_silence_state`; the object's own string calls the
 * first a "silence treshold" (its spelling) and `fax_class1_progress` calls
 * the second `TxSmpCnt`, complaining "ERROR: TxSmpCnt != 160 !!!" when a
 * handler leaves anything else behind.
 */
#define CLASS1_SILENCE_THRESHOLD	0x64
#define CLASS1_BLOCK_SAMPLES		0xa0

/*
 * The host-link framing bytes.  DLE doubles itself in the data and DLE ETX
 * ends a buffer; the object's own string for a DLE that reached the escape
 * arm is "CLASS1: DLE %1X in data".
 */
#define CLASS1_DLE	0x10
#define CLASS1_ETX	0x03

/*
 * `flags004` bit 4.  Both `_handle_hdlc_input` and `_handle_hdlc_input_close`
 * test it at end of frame and set `frame_end_latch` when it is on, and nothing else in
 * the object touches either.  So the SITE is established and the meaning is
 * not: the name records what the bit gates, not what it configures.
 */
#define CLASS1_FLAG_FRAME_END_LATCH	0x10

/*
 * `_handle_data_input`'s two limits: the run of zero elements it appends
 * after DLE ETX, and the index past which it stops appending them.
 */
#define CLASS1_ETX_PAD_ELEMENTS		20
#define CLASS1_ETX_PAD_LIMIT		0x7ff

/*
 * `_hdlc_emulate_receive_state`'s own stack table of record lengths, sized
 * from the function's OWN stack frame: `sub $0x50,%esp` reserves 0x50 bytes,
 * the largest outgoing call (`cTOOLS_handle_hdlc_output`, five arguments)
 * uses the first 0x14 of them, and the table itself is indexed at
 * `0x20(%esp,%ecx,4)` -- so it runs from 0x20 to 0x50, 0x30 bytes of 4-byte
 * entries, twelve.  Nothing bounds the PARSE loop that fills it against this
 * count; more than twelve records in one call overruns it exactly as it
 * would in the object, and is reproduced rather than guarded.
 */
#define CLASS1_EMU_MAX_FRAMES		12

/*
 * The state-init family: (object, an argument) -> int.  The silence pair's
 * argument is a sample count, halved into `countdown` with a floor of 1;
 * both leave that halved value in eax, which is declared here as the return
 * because the family returns int (_idle_state_init returns 0) -- no caller
 * is known to read it.
 */
int _send_silence_state_init(struct fax_class1 *ctx, int samples);
int _recieve_silence_state_init(struct fax_class1 *ctx, int samples);

/*
 * Reads nothing, returns 0.  ARITY NOT SETTLED (`xor eax; ret`, three bytes);
 * one context pointer is declared because that is what the rest of the
 * `*_state_init` family takes.
 */
int _idle_state_init(struct fax_class1 *ctx);

/*
 * THE STATE HANDLER CONTRACT.  Nine arguments, and the count is read off
 * `fax_class1_progress`'s marshalling at 0x0937bd..0x0937f8, which fills
 * (%esp) through 0x20(%esp) and then dispatches
 * `call *class1_state_functions(,%edx,4)`.
 *
 * What each one is, and how well it is established:
 *
 *   ctx        +0   the session.  Certain -- every handler writes its fields.
 *   rx         +4   the received block.  `_recieve_silence_state` hands it
 *                   to `FPM_rms(const short *, unsigned short)`, which types
 *                   it; that is evidence class 2, a callee that types it.
 *   tx         +8   the block to transmit.  All three handlers here fill it
 *                   with `CLASS1_BLOCK_SAMPLES` zeroes, 16 bits at a time.
 *   word3      +12  NOT READ by any handler this batch writes.  Only the
 *   word4      +16  WIDTH is established (a 32-bit argument slot); `int` is
 *   word7      +28  the least claim, and is not a claim that they are ints.
 *   rx_count   +20  how many samples `rx` holds.  `fax_class1_progress`
 *                   passes the address of its OWN copy, not its caller's
 *                   pointer (0x0936d7: it loads `*arg5` into a local and
 *                   passes `&local`), so a handler writing through this does
 *                   not reach the caller.
 *   tx_count   +24  how many samples the handler left in `tx`.  The author's
 *                   word is `TxSmpCnt`: `fax_class1_progress` checks
 *                   `*tx_count == 160` right after the dispatch and prints
 *                   "ERROR: TxSmpCnt != 160 !!!" when it is not (0x09389f).
 *   word8      +32  read at entry and written at exit by
 *                   `_recieve_silence_state`, and by nothing else here.  A
 *                   non-zero value on the way IN abandons the wait -- the
 *                   object's own string is "Abort waiting for silence!" --
 *                   and 5 is written into it when energy appears.  TWO
 *                   READINGS FIT (a caller's abort/result word, or the next
 *                   state, since 5 is HDLC_RECEIVE_STATE) and nothing here
 *                   chooses between them, so it keeps a neutral name.
 *
 * Every handler returns 0 in the three reconstructed so far; nothing is
 * known to read the return.
 */
typedef int (*class1_state_fn)(struct fax_class1 *ctx, const short *rx,
			       short *tx, int word3, int word4,
			       int *rx_count, int *tx_count, int word7,
			       int *word8);

/*
 * `.bss` (COMMON in the object, 0x4c bytes = 19 * 4), installed by
 * `fax_class1_create` (unwritten) and indexed with no bounds check by
 * `fax_class1_progress`'s own dispatch (`call *class1_state_functions
 * (,%edx,4)`).  Declared here so a test can install handlers into it
 * without a writer in `src/` yet -- it starts all-NULL, same as the
 * object's own COMMON storage.
 */
extern class1_state_fn class1_state_functions[19];

/*
 * IDLE_STATE.  Zero `*rx_count` samples of `tx` and report that many -- or,
 * when `*rx_count` is not positive, a whole block of 160.  Reads no field of
 * the session at all.
 */
int _idle_state(struct fax_class1 *ctx, const short *rx, short *tx,
		int word3, int word4, int *rx_count, int *tx_count,
		int word7, int *word8);

/*
 * SEND_SILENCE_STATE.  Count one block off `countdown`, transmit a block of
 * silence, and report FAX_CLASS1_OK_NO_CARRIER once the countdown reaches
 * zero.  The state does NOT leave itself -- it is `fax_class1_progress` that
 * acts on the status.
 */
int _send_silence_state(struct fax_class1 *ctx, const short *rx, short *tx,
			int word3, int word4, int *rx_count, int *tx_count,
			int word7, int *word8);

/*
 * RECIEVE_SILENCE_STATE (the author's spelling).  Transmit a block of
 * silence, measure the received block's RMS into `energy`, and:
 *
 *   *word8 non-zero            -> abandon: OK_NO_CARRIER, go IDLE
 *   energy >  the threshold    -> restart the count, NO_MESSAGE, *word8 = 5
 *   energy <= the threshold    -> one more silent block; when that reaches
 *                                 `countdown`, OK_NO_CARRIER and go IDLE
 *
 * The threshold test is `> 100` on a signed short, so a NEGATIVE energy
 * counts as silence -- `FPM_rms` cannot return one, and the test is written
 * as the object has it rather than as it would have to be if it could.
 */
int _recieve_silence_state(struct fax_class1 *ctx, const short *rx, short *tx,
			   int word3, int word4, int *rx_count, int *tx_count,
			   int word7, int *word8);

/*
 * Zero `count` elements of `buf`.  The object's own return value is whatever
 * the loop counter (`i`) holds on exit -- `count` on a normal completion,
 * 0 if `count` was not positive -- rather than the argument itself; no
 * caller reconstructed here reads it.  `.text` 0x092b70, 28 bytes, and the
 * TU's first global -- immediately before `_send_silence_state_init`.
 */
int _put_silence(short *buf, int count);

/*
 * T.30 rate code -> (modulation, bit rate).  The four two-code ranges are
 * V.17 (with/without short training); the singles below them select V.29
 * (mod 1) and V.27ter (mod 0).  A code outside the table writes NOTHING --
 * both outputs keep their callers' values -- and the ranges are separate
 * IFs in the object, so 0x91/0x92 would fall through the later equality
 * tests unharmed either way.
 */
void _set_modem_rate(int code, int *mod, int *rate);

/* Bits per symbol for a bit rate; 0 for one not in the table. */
int _sym_size(int rate);

/*
 * info(ctx, 0, out): *out = gain_attenuation_db.  info(ctx, 1, out): *out = the unsigned
 * short at tx_fifo + 0xc, or 0 with tx_fifo null.  Any other selector writes
 * nothing.  Returns 0, always.
 */
int fax_class1_info(struct fax_class1 *ctx, int sel, int *out);

/*
 * Returns 0, reads nothing.  ARITY NOT SETTLED by the object (`xor eax;
 * ret`); one pointer is declared as the least claim compatible with the
 * name.
 */
int fax_class1_GetConstalation(void *ctx);

/*
 * Tear the whole session down, in the object's own order: `vmi_c_cfg` (and its
 * one sub-allocation at +0x10), `vmi_c`, `vmi_a_cfg` (the same sub-allocation
 * shape as `vmi_c_cfg`), `vmi_a` -- then, when both `modem_vmi` and `vmi_b` are
 * non-null, the current data modem (`_delete_data_rx_modem` when
 * `modem_direction == 1`, `_delete_data_tx_modem` otherwise) -- then `tx_fifo` (a FIFO)
 * and `tone` (a tone detector), and finally the session object itself.
 * Returns 1, always -- `.text` 0x0093bf0, 347 bytes.
 */
int fax_class1_delete(struct fax_class1 *ctx);

/*
 * Report the active VMI handle's status.  `.text` 0x0093b50, 150 bytes,
 * between `fax_class1_progress` and `fax_class1_delete` in the object
 * (written after both, at the end of `class1.c` -- see that file's own
 * banner for why the file does not follow address order here).
 *
 * `modem_status` is forwarded unchanged into the local status record's
 * `modem_status` field -- `struct faxvmi_status`'s own "IN" contract, see
 * `faxvmi.h` -- so it is `void *` here, the least claim; `FAXVMI_status`
 * treats a non-NULL value as a pointer into the wrapped modulation's own
 * status buffer and does nothing with it otherwise itself.
 *
 * Selects `vmi_a` for `state` 4..6 and `vmi_b` for 12..13 (matching
 * class1.h's own note on those two fields, now from this function's own
 * evidence rather than inferred); anything else touches nothing and
 * returns 0.  A matching range always returns 1, never the wrapped call's
 * own result.  F10104.
 */
int fax_class1_status(struct fax_class1 *ctx, void *modem_status);

/*
 * The session dispatcher.  `.text` 0x0936d0, 1,145 bytes.  Declined twice
 * before this wave (F9802, referenced by the wave-6 ledger) on
 * `ctx->0x1254`'s meaning; findings F10051/F10052 settle it as a T.30 rate
 * code selecting which of three already-complete modulations' quality latch
 * to test.
 *
 * SAME NINE-ARGUMENT SHAPE AS `class1_state_fn` -- ctx, rx, tx, word3,
 * word4, rx_count, tx_count, word7, word8 -- confirmed by tracing every
 * stack slot of the marshalling call at 0x0937b7..0x0937f8 back to this
 * function's OWN arguments in the same positions.  `rx` is `short *`, not
 * `const`, because this function (not any state handler) hands it to
 * `FPM_iir_filt_II`, which filters in place; `word7` is `int *`, not a bare
 * int, because this function writes `*word7 = 0` on every call, the first
 * evidence anywhere in this tree that it is a pointer at all.
 *
 * WHAT IT DOES, in the object's own order:
 *
 *   1. When `ctx->f127c` is nonzero, scan up to `*rx_count / 8` samples of
 *      `rx`, counting the nonzero ones into `ctx->f127c`; if that exceeds
 *      `*rx_count / 16`, reset it to 0.  Debug-level-gated per-sample
 *      printing throughout (the object's own strings).
 *   2. Advance the clock: `clock_frac += 2`, rolling into `clock_sec` past
 *      99.  Clear `*word7` and `status` unconditionally.  When `iir_enabled` is
 *      nonzero, run one `FPM_iir_filt_II` tick over `rx` (2 sections,
 *      `(short)*rx_count` samples, coefficients `iir_coeff`, state
 *      `iir_state`).
 *   3. Dispatch: `class1_state_functions[ctx->state](ctx, rx, tx, word3,
 *      word4, &local_rx_count, tx_count, word7, word8)`, where
 *      `local_rx_count` is THIS function's own copy of `*rx_count` (D-shape:
 *      the handler never sees the caller's pointer). No bounds check on
 *      `ctx->state` -- reproduced, not guarded.
 *   4. If `ctx->state` changed across the dispatch, log the transition
 *      (`states_names`, both old and new) at debug level > 1.  Update
 *      `prev_state`.
 *   5. If `*tx_count != CLASS1_BLOCK_SAMPLES`, log "ERROR: TxSmpCnt != 160
 *      !!!" at debug level > 1 (class1.h's own long-established note).
 *   6. Apply a delayed status: if `delayed_status_countdown` is nonzero and
 *      reaches 0 on THIS call, `status = delayed_status` (the object's own
 *      words, "STATUS = DELAYED_STATUS").
 *   7. If `status` is still FAX_CLASS1_NO_MESSAGE, switch on
 *      `modem_rate_code` -- the four V.17 LONG-TRAINING codes only (0x91,
 *      0x79, 0x61, 0x49; the short-training codes `_set_modem_rate` also
 *      recognises are NOT tested here), the two V.29 codes, or the two
 *      V.27ter codes -- and, only when `modem_direction == 1`, walk `vmi_b->link->
 *      int_0014` to the active modem and test/clear that modulation's own
 *      quality latch (`V17RXS_SHORT_4FB2` at `V17RX_OBJ_STATE`,
 *      `V29RX_SHORT_4F62` at `V29_OBJ_RX`, `V27RX_Q_FLAG` at `V27_OBJ_RX`).
 *      A set latch becomes `status = FAX_CLASS1_ACCEPT_RATE`.
 *   8. If `status` is non-zero at this point (from the dispatch, the
 *      delayed-status apply, or step 7), log it (`status_names`) at debug
 *      level > 1.
 *   9. Return `status`.
 */
int fax_class1_progress(struct fax_class1 *ctx, short *rx, short *tx,
			int word3, int word4, int *rx_count, int *tx_count,
			int *word7, int *word8);

/*
 * ANSWER_TONE_STATE (14).  `.text` 0x092d60, 94 bytes.  Count one block off
 * `countdown` (armed by `fax_class1_create`'s own `answer_tone_ms`-derived
 * `answer_tone_blocks`); once `countdown` EXCEEDS it, hand off to
 * `cHDLCtx_preamble_state_init` (its return discarded) instead of
 * generating another block.  Otherwise generate one block of the session's
 * tone (`ctx->tone`, `FPM_TONE_generate`) and report
 * `*tx_count = CLASS1_BLOCK_SAMPLES`.  Returns 0 on both paths.
 */
int _answer_tone_state(struct fax_class1 *ctx, const short *rx, short *tx,
		       int word3, int word4, int *rx_count, int *tx_count,
		       int word7, int *word8);

/*
 * The session dispatcher (control side).  `.text` 0x093420, 615 bytes.
 * `cmd` is one of the six `FAX_CLASS1_*_COMMAND` codes above; `arg3` is a
 * T.30 rate code (TH/TM/RM/RH) or a raw sample count (TS/RS, the same
 * argument `_send_silence_state_init`/`_recieve_silence_state_init` take);
 * `arg4` is read ONLY by the TM command, into `silence_blocks`.  Logs the
 * incoming command (its own name, off `command_names`) and, unless `cmd ==
 * FAX_CLASS1_RH_COMMAND`, clears `superframe_len` -- both unconditionally, before
 * dispatching.  A `cmd` outside 0..5 does the log-and-clear and returns 1
 * without dispatching anything.  ALWAYS returns 1; nothing reconstructed
 * reads back any other value.  See class1.c for each command's own
 * derivation.
 */
int fax_class1_command(struct fax_class1 *ctx, int cmd, int arg3, int arg4);

/*
 * Build (or reinitialise) a Class 1 fax session.  `.text` 0x092e20, 1,532
 * bytes -- the largest single piece of `class1.c`.  `existing` NULL
 * allocates a fresh `sizeof(struct fax_class1)`-ish object
 * (`CLASS1_MODELLED_BYTES`, though the real object extends past what this
 * batch models); non-NULL reinitialises it in place, INCLUDING re-installing
 * the global `class1_state_functions` table and rebuilding the session's
 * tone generator -- but NOT rebuilding `vmi_c`/`vmi_a` (the V.21 handles),
 * which only the fresh path builds.  `cfg` is never NULL in any traced
 * caller and this function does not check it either.  Returns the (possibly
 * freshly allocated) session, or the same `existing` on every path (never
 * NULL, even on an internal allocation failure this batch could not trace a
 * check for).  See class1.c for the full derivation, including the
 * `cfg->mode == 2` answer-tone-only branch and the nineteen `class1_state_
 * functions` installs (all in `states_names`' own order).
 */
struct fax_class1 *fax_class1_create(struct fax_class1 *existing,
				     const struct fax_class1_cfg *cfg);

#endif /* DSPLIB_CLASS1_H */
