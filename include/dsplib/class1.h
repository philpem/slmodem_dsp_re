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
 *                       (vmi_c, f120c, f1210) -- see that function's own
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
	short f000;			/* +0x0000 <- f1250 - 1, by
					 * _handle_hdlc_input_close         */
	unsigned char pad_002[2];	/* +0x0002                          */
	unsigned char flags004;		/* +0x0004 bit 4 gates f1224        */
	unsigned char pad_005[0xffb];	/* +0x0005                          */
	unsigned short f1000[0x100];	/* +0x1000 `_hdlc_emulate_receive_
						 * state`'s own buffer: a run of
						 * LENGTH-PREFIXED records -- entry
						 * `i` is a length, the next `i+1`
						 * entries are the record's own
						 * (byte-valued) elements, and the
						 * NEXT record starts right after
						 * -- read only up to `f12d0` bytes
						 * of it, in the same shape
						 * `_handle_hdlc_input_close`
						 * leaves a frame in.  Sized from
						 * ADDRESS CONTIGUITY alone (it runs
						 * right up to `vmi_c`'s own +0x1200,
						 * the next already-established
						 * field, with no access this batch
						 * saw past that) rather than a size
						 * constant anywhere -- usage
						 * inference, the weakest class     */
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
	void *f120c;			/* +0x120c a pointer `fax_class1_delete`
						 * frees at TWO offsets when non-null
						 * -- `*(p+0x10)` first, then `p`
						 * itself -- so it owns exactly one
						 * sub-allocation at +0x10.  Neither
						 * the outer nor the inner object's
						 * shape is established beyond that */
	void *f1210;			/* +0x1210 the same shape as `f120c`,
						 * torn down the same way, one field
						 * later -- `fax_class1_delete`'s own
						 * order is f120c, vmi_c, f1210,
						 * vmi_a                            */
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
	unsigned char pad_1218[4];	/* +0x1218                          */
	int state;			/* +0x121c the session state        */
	int prev_state;			/* +0x1220 the state as of the LAST
					 * call to `fax_class1_progress` --
					 * compared against `state` after each
					 * dispatch to log a transition, then
					 * updated to match                 */
	int f1224;			/* +0x1224 set 1 on hdlc close when
					 * flags004 bit 4                   */
	int countdown;			/* +0x1228 armed by the state inits */
	int status;			/* +0x122c the FAX_CLASS1_* code
					 * fax_class1_progress RETURNS
					 * (0x93a43: it loads this and
					 * leaves it in eax)                */
	unsigned char pad_1230[8];	/* +0x1230                          */
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
	int f1244;			/* +0x1244 `fax_class1_delete` reads
						 * this to choose `_delete_data_rx_
						 * modem` (when == 1) or `_delete_
						 * data_tx_modem` (otherwise) for
						 * `modem_vmi`/`vmi_b` -- so it marks
						 * which direction the CURRENT data
						 * modem is, but whether it is a
						 * plain boolean or carries other
						 * values is not established        */
	unsigned char pad_1248[4];	/* +0x1248                          */
	int dle_seen;			/* +0x124c a DLE has been seen and
					 * the next byte is its argument.
					 * Both _handle_data_input and
					 * _handle_hdlc_input keep their
					 * escape state HERE, in the same
					 * field                            */
	int f1250;			/* +0x1250 the write cursor into the
					 * HDLC receive frame, in elements.
					 * _handle_hdlc_input_open arms it
					 * to 1 and both _..._input and
					 * _..._close report `f1250 - 1` as
					 * the length in f000               */
	int modem_rate_code;		/* +0x1254 the negotiated T.30 modem
						 * rate code -- `_set_modem_rate`'s
						 * own code space, exactly (finding
						 * F10051): `fax_class1_progress`
						 * switches on it to pick which
						 * modulation's quality latch to
						 * test.  NOT YET the name of a
						 * writer, only of this one reader   */
	struct fpm_tone *f1258;	/* +0x1258 torn down by
						 * `fax_class1_delete`'s own
						 * `FPM_TONE_delete` call -- a tone
						 * detector, plausibly for
						 * `CLASS1_ANSWER_TONE_STATE`, not
						 * otherwise established             */
	unsigned char pad_125c[0x18];	/* +0x125c                          */
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
	unsigned char pad_1280[8];	/* +0x1280                          */
	struct fax_fifo *f1288;	/* +0x1288 fax_class1_info(1) reads
					 * an unsigned short at +0xc of it.
					 * Typed from `_delete_data_tx_modem`'s
					 * own call, `FIFO_delete(struct
					 * fax_fifo *)` -- evidence class 2  */
	unsigned char pad_128c[0x10];	/* +0x128c                          */
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
	unsigned char pad_12b4[4];	/* +0x12b4                          */
	unsigned int silence_blocks;	/* +0x12b8 blocks seen below the
					 * silence threshold; compared
					 * against `countdown` UNSIGNED
					 * (`jae`, 0x092d10)                */
	short energy;			/* +0x12bc the last block's FPM_rms.
					 * The author's word: the object
					 * prints exactly this value as
					 * "Energy %d"                      */
	unsigned char pad_12be[0xa];	/* +0x12be                          */
	int f12c8;			/* +0x12c8 `_hdlc_emulate_receive_
						 * state`'s own between-record
						 * countdown: decremented once per
						 * call, and a value that was <= 0
						 * BEFORE the decrement is what
						 * fires the next record (or the
						 * idle transition once `f1000`
						 * is exhausted).  Reset to 2      */
	int f12cc;			/* +0x12cc the same function's "next
						 * record to emit" index into the
						 * record COUNT `f1000` parses to
						 * (not a byte offset).  Reset to 0 */
	int f12d0;			/* +0x12d0 and the valid byte length
						 * of `f1000` for this batch of
						 * records.  Reset to 0.  All three
						 * are read-and-written by that one
						 * function only, this batch        */
	unsigned char pad_12d4[4];	/* +0x12d4                          */
	int f12d8;			/* +0x12d8 fax_class1_info(0);
					 * create clears it                 */
	const short *f12dc;		/* +0x12dc create: a .rodata ptr.
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
	int f12f0;			/* +0x12f0 gates whether
					 * `fax_class1_progress` runs the IIR
					 * filter tick at all this call.
					 * Usage inference only               */
};

#define CLASS1_MODELLED_BYTES	0x12f4

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
 * `states_names` (twenty entries) and `status_names` (eleven) -- see
 * class1.c for the derivation and D1330 for why they are global here where
 * the object has them file-local.  Declared here so a test can compare them
 * against the blob's own copy without a reader in `src/` yet.
 */
struct class1_name {
	int id;
	char *name;
};

extern struct class1_name states_names[20];
extern struct class1_name status_names[11];

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
 * test it at end of frame and set `f1224` when it is on, and nothing else in
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
 * info(ctx, 0, out): *out = f12d8.  info(ctx, 1, out): *out = the unsigned
 * short at f1288 + 0xc, or 0 with f1288 null.  Any other selector writes
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
 * Tear the whole session down, in the object's own order: `f120c` (and its
 * one sub-allocation at +0x10), `vmi_c`, `f1210` (the same sub-allocation
 * shape as `f120c`), `vmi_a` -- then, when both `modem_vmi` and `vmi_b` are
 * non-null, the current data modem (`_delete_data_rx_modem` when
 * `f1244 == 1`, `_delete_data_tx_modem` otherwise) -- then `f1288` (a FIFO)
 * and `f1258` (a tone detector), and finally the session object itself.
 * Returns 1, always -- `.text` 0x0093bf0, 347 bytes.
 */
int fax_class1_delete(struct fax_class1 *ctx);

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
 *      99.  Clear `*word7` and `status` unconditionally.  When `f12f0` is
 *      nonzero, run one `FPM_iir_filt_II` tick over `rx` (2 sections,
 *      `(short)*rx_count` samples, coefficients `f12dc`, state
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
 *      V.27ter codes -- and, only when `f1244 == 1`, walk `vmi_b->link->
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

#endif /* DSPLIB_CLASS1_H */
