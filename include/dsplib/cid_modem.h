/*
 * cid_modem.h -- the Caller ID service's top-level object, and the leaves of
 * the original's `cid.c` and `Data.c` translation units (finding F1410 for
 * the TU map: cid.c is the ten functions 0x08fce0..0x090467, Data.c the three
 * from 0x090470).
 *
 * This object sits ABOVE both receivers: +0x00 points at the DTMF receiver
 * (`struct dtmf_rx`, dtmf_rx.h) and +0x04 at the FSK receiver (`struct cid`,
 * cid.h).  `cid_create` allocates 0x3fc bytes, stores its `mode` argument at
 * +0x260 and its second argument at +0x264, and builds whichever receivers
 * the mode asks for.
 *
 * THE MODE ENCODING, from cid_create/cid_reset's tests (each tests `!= 0`,
 * `!= 1`, `== 5` -- never a bit): 0 builds/resets the FSK side only, 1 the
 * DTMF side only, and anything above 1 is clamped to 5, which is both.  Note
 * the polarity: mode 0 is FSK, not DTMF.
 *
 * `cid_progress` is what makes it a STATE and not just a configuration: from
 * mode 5 it writes 3 the moment the DTMF receiver reports digits arriving and
 * 2 the moment the FSK receiver reports a whole message, so the automatic mode
 * commits to one side and stays there.  See the constants below.
 *
 * Every byte of the object is modelled now that `cid_progress` is written;
 * +0x268 was the last padded span and is its block buffer.
 */

#ifndef DSPLIB_CID_MODEM_H
#define DSPLIB_CID_MODEM_H

struct dtmf_rx;
struct cid;

/*
 * THE FIVE VALUES OF `mode`, and the evidence for each name.
 *
 * 0, 1 and 5 are structural: every function in cid.c gates the FSK side on
 * `mode != 1` and the DTMF side on `mode != 0`, and `cid_create` clamps
 * anything above 1 to 5 while printing " AUTOMATIC MODULATION MODE " -- the
 * author's own words, so CID_MODE_AUTOMATIC is evidence rule 1.
 *
 * 3 is ALSO the author's own word.  `cid_progress` prints
 * "\n DTMF demodulator Failed after CID_MESSAGE state \n" on exactly the test
 * `mode == 3`, so 3 is the state the author called CID_MESSAGE: the automatic
 * mode has seen DTMF digits arriving and committed to the DTMF receiver.
 *
 * 2 is usage inference (evidence rule 3) from two independent sites that
 * agree: `cid_progress` writes it only when the FSK receiver has returned a
 * whole checksum-good message, in the same statement pair that makes the
 * function return success, and `cid_get_strings` is the one function that
 * routes mode 2 to the FSK renderer rather than the DTMF one.  So it is "the
 * FSK message is in", named conservatively.
 */
#define CID_MODE_FSK		0	/* FSK receiver only                */
#define CID_MODE_DTMF		1	/* DTMF receiver only               */
#define CID_MODE_FSK_DONE	2	/* an FSK message has been received */
#define CID_MODE_CID_MESSAGE	3	/* committed to DTMF -- the string  */
#define CID_MODE_AUTOMATIC	5	/* both, and undecided              */

/*
 * The block `cid_progress` accumulates before it asks a receiver anything:
 * 160 samples at a line rate of 8000 and 192 at anything else, which is 20 ms
 * either way.  The object spells it as a comparison against 8000 followed by
 * `sete/dec/and $0xffffffe0/lea 0xc0(%eax)`, so the two constants differ by
 * exactly 32 and 192 is the one the comparison falls through to.
 *
 * 192 is also `CID_process`'s chunk, so the service feeds exactly one 9600 Hz
 * block per call; at 8000 it feeds one block and 32 samples of the next.
 */
#define CID_BLOCK_8000		160
#define CID_BLOCK_9600		192

/* 0x3fc bytes; `cid_create` allocates exactly that. */
struct cid_modem {
	struct dtmf_rx *dtmf;		/* +0x000 DTMF receiver, modes 1/5  */
	struct cid *fsk;		/* +0x004 FSK receiver, modes 0/5   */
	/*
	 * +0x008 is the STRING BUFFER, and `cid_get_strings` is what settles
	 * it: that function memsets exactly 0x258 bytes from +0x008, hands the
	 * same address to `data_formatted_output` / `data_unformatted_output`
	 * as their output, and RETURNS it.  So the whole span is one array and
	 * not a field bank -- 600 bytes of NUL-terminated fields, which is the
	 * shape `CID_process` walks.
	 */
	char strings[0x258];		/* +0x008 what cid_get_strings returns */
	int mode;			/* +0x260 see the encoding above    */
	/*
	 * +0x264. Was `f264`; renamed to match the parameter it is a pure
	 * copy of -- `cid_create`'s own `cid_val` argument (see its own
	 * doc comment below), which `cid_value()` can later overwrite with
	 * a value of the caller's choosing. `src/service/cid.c`'s
	 * `CID_VALUE_RAW` names the one value the object itself tests it
	 * against (usage inference; see that macro's own comment).
	 */
	int cid_val;			/* +0x264 <- cid_create's 2nd arg;
					 *         rewritten by cid_value   */
	/*
	 * +0x268 is `cid_progress`'s BLOCK BUFFER, and that function is what
	 * settles it: it copies the caller's samples into
	 * `0x266(%ebx,%edx,2)` with `%edx = samples_fill + 1` -- the same address as
	 * `0x268 + samples_fill*2` -- until `samples_fill` reaches the block length, then
	 * hands `ctx + 0x268` and that length to `dtmf_modem` / `cid_modem`
	 * as their sample pointer.  So it is one `short` array and `samples_fill` is
	 * its fill level.
	 *
	 * THE LENGTH IS BOUNDED FROM BOTH SIDES AND THE TWO DO NOT MEET.
	 * The lower bound is measured: `cid_progress`'s block length is 192
	 * at any line rate other than 8000, so at least 192 shorts are
	 * written.  The upper bound is the layout -- `samples_fill` sits at +0x3f8
	 * and 0x3f8 - 0x268 is exactly 400 bytes -- so at most 200.  Nothing
	 * in the object reads or writes +0x3e8..+0x3f8, so the array is
	 * declared to fill the space; the eight shorts past 192 are the
	 * reading, not a measurement.
	 */
	short samples[200];		/* +0x268 cid_progress's block      */
	int samples_fill;		/* +0x3f8 fill level of `samples`;
					 *         cleared by cid_create and
					 *         cid_reset                */
};

/* One home for the sizeof claim above. */
#define CID_MODEM_BYTES	0x3fc

/**
 * @brief Set the detection threshold on whichever receivers the mode
 * says exist.
 *
 * @param ctx  The service object.
 * @param thr  Threshold, stored on the DTMF receiver's `sens` (modes 1
 *             and 5) and/or the FSK receiver's threshold (modes 0 and
 *             5). Arrives as an int and is stored as a short, the
 *             object's own truncation.
 */
void cid_threshold(struct cid_modem *ctx, int thr);

/**
 * @brief Store a value at +0x264, the same slot cid_create() seeds from
 * its `cid_val` argument.
 * @param ctx  The service object.
 * @param v    The value to store.
 */
void cid_value(struct cid_modem *ctx, int v);

/**
 * @brief Render whatever the receivers collected into `ctx->strings`.
 *
 * The buffer is cleared first, in full, so the answer is always a run
 * of NUL-terminated fields even when nothing rendered. Which receiver is
 * asked is `mode != 0 && mode != 2` -- the DTMF side then, and the FSK
 * side otherwise; mode 2 takes the FSK path despite being above 1, which
 * no other function in this file does (reachable only by a caller that
 * writes `mode` itself, since cid_create() clamps anything above 1 to
 * 5). On the DTMF side it copies SIXTEEN bytes of the receiver's
 * `digits[20]`, with no terminator of its own. On the FSK side
 * `ctx->cid_val == 2` selects the raw hex dump and anything else the
 * labelled rendering.
 *
 * @param ctx  The service object.
 * @return `ctx->strings`.
 */
char *cid_get_strings(struct cid_modem *ctx);

/**
 * @brief Build the Caller ID service object and its receivers.
 *
 * @param ctx      NULL allocates #CID_MODEM_BYTES and takes @p mode from
 *                 the argument, clamped to #CID_MODE_AUTOMATIC above 1;
 *                 non-NULL keeps the mode it already has and only
 *                 (re)builds, handing each constructor the existing
 *                 pointer so a second call reuses the allocation.
 * @param cid_val  Stored at +0x264 (see cid_value()).
 * @param mode     Initial mode, only used when @p ctx is NULL.
 * @return The `struct cid_modem *` (as `void *`, matching `CID_create`'s
 *         own storage), or NULL on failure.
 */
void *cid_create(struct cid_modem *ctx, int cid_val, int mode);

/**
 * @brief Free the receivers and the service object.
 *
 * `ctx->fsk` is freed unconditionally, which is safe only because the
 * modes that never build one leave it NULL.
 *
 * @param ctx  The service object to free.
 */
void cid_delete(struct cid_modem *ctx);

/**
 * @brief The service's own state machine: buffer samples and hand
 * completed blocks to whichever receivers the mode selects.
 *
 * Buffers `*count` samples from @p in into `ctx->samples` and, every
 * time that buffer fills to one block, hands the block to the selected
 * receiver(s) and reads their verdicts; a call with enough samples runs
 * several blocks, and a partial block is left in the buffer for the
 * next call.
 *
 * @param ctx    The service object, updated in place.
 * @param in     Input samples.
 * @param what   NEVER READ -- `CID_process` always passes 0, kept only
 *               because the object's stack layout says the function
 *               takes four arguments.
 * @param count  In: number of samples in @p in. Set to zero on entry
 *               and never read again afterwards -- an argument in name,
 *               input-length-in/nothing-out in fact.
 * @return 0 while the message is still arriving, 1 for a complete one,
 *         and 2 or 3 for the several ways it gives up. Never -1.
 */
short cid_progress(struct cid_modem *ctx, short *in, int what, short *count);

/**
 * @brief Reset both receivers in place.
 *
 * Same clamp as cid_create() and the same mode gating, but this BUILDS
 * nothing: raising the mode here and then calling this walks a null
 * receiver, exactly as the object does.
 *
 * @param ctx  The service object to reset.
 */
void cid_reset(struct cid_modem *ctx);

/**
 * @brief Retune both receivers to a new line rate.
 *
 * Same mode gating as cid_threshold(): the DTMF receiver's `rate` for
 * modes 1 and 5, the FSK receiver's for modes 0 and 5. See
 * src/service/cid.c for what it does to the FSK receiver's `f02c`.
 *
 * @param ctx   The service object.
 * @param rate  New line rate. Arrives as an int and is stored as a
 *              short on both sides, the object's own truncation.
 */
void cid_freq_sampl(struct cid_modem *ctx, int rate);

/*
 * ---------------------------------------------------------------------------
 * The two TLV walkers cid_get_strings drives, and Data.c's hex dumper.
 *
 * A received Caller ID message is tag-length-value: buf[0] message type,
 * buf[1] total length, then entries of (tag, length, bytes...).  Both
 * walkers take the whole message and return the OFFSET of a matching entry's
 * tag byte, or -1.  Lengths are read as SIGNED chars and positions held in
 * shorts -- both the object's own widths, kept because a length byte >= 0x80
 * walks backwards under them.
 */

/**
 * @brief Find the first TLV entry with a given tag.
 * @param buf  The whole message (`buf[0]` type, `buf[1]` total length).
 * @param tag  The tag to look for.
 * @return Offset of the first matching entry at offset >= 2, or -1 if none.
 */
int _look_for(const char *buf, char tag);

/**
 * @brief Find the first TLV entry whose tag is not one the caller has
 * already taken.
 *
 * Tags 1, 2 and 7 are the date/time and the two number/name entries the
 * caller has already taken; @p except lets it skip one further entry by
 * position.
 *
 * @param buf     The whole message.
 * @param start   Search starts at this offset.
 * @param except  An offset to also skip, or a value no entry can have.
 * @return Offset of the first entry at offset >= @p start whose tag is
 *         none of 1, 2 or 7 and whose offset is not @p except, or -1 if
 *         none.
 */
int _look_for_other_than(const char *buf, short start, int except);

/**
 * @brief Hex-dump a raw Caller ID message.
 * @param buf  The message: `buf[1] + 2` bytes are dumped, capped at 0xf5
 *             (the cap is on the byte count before doubling, matching
 *             the object).
 * @param out  Output buffer, needs up to `2*0xf5 + 1` bytes: two
 *             lowercase hex digits per byte, NUL-terminated.
 */
void data_raw(const char *buf, char *out);

/**
 * @brief Render an FSK Caller ID message as one hex dump.
 *
 * One of the two renderers cid_get_strings() picks between; an inlined
 * data_raw() over the whole message (`cid->data`, present iff
 * `cid->pack_len` is nonzero).
 *
 * @param cid  The FSK receiver holding the message.
 * @param out  Output buffer, the 0x258-byte scratch at `cid_modem + 0x008`.
 */
void data_unformatted_output(struct cid *cid, char *out);

/**
 * @brief Render an FSK Caller ID message as labelled fields.
 *
 * Writes `LABEL = value` fields -- DATE, TIME, NMBR and, for a
 * multiple-data-message frame, NAME and one MESG per leftover tag.
 * Writes NOTHING when `pack_len` is zero. See src/service/data.c for the
 * four places it runs past what a careful reading would allow, all of
 * them reproduced.
 *
 * @param cid  The FSK receiver holding the message.
 * @param out  Output buffer, 600 bytes, the object's own budget.
 */
void data_formatted_output(struct cid *cid, char *out);

#endif /* DSPLIB_CID_MODEM_H */
