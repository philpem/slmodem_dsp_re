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
	int f264;			/* +0x264 <- cid_create's 2nd arg;
					 *         rewritten by cid_value   */
	/*
	 * +0x268 is `cid_progress`'s BLOCK BUFFER, and that function is what
	 * settles it: it copies the caller's samples into
	 * `0x266(%ebx,%edx,2)` with `%edx = f3f8 + 1` -- the same address as
	 * `0x268 + f3f8*2` -- until `f3f8` reaches the block length, then
	 * hands `ctx + 0x268` and that length to `dtmf_modem` / `cid_modem`
	 * as their sample pointer.  So it is one `short` array and `f3f8` is
	 * its fill level.
	 *
	 * THE LENGTH IS BOUNDED FROM BOTH SIDES AND THE TWO DO NOT MEET.
	 * The lower bound is measured: `cid_progress`'s block length is 192
	 * at any line rate other than 8000, so at least 192 shorts are
	 * written.  The upper bound is the layout -- `f3f8` sits at +0x3f8
	 * and 0x3f8 - 0x268 is exactly 400 bytes -- so at most 200.  Nothing
	 * in the object reads or writes +0x3e8..+0x3f8, so the array is
	 * declared to fill the space; the eight shorts past 192 are the
	 * reading, not a measurement.
	 */
	short samples[200];		/* +0x268 cid_progress's block      */
	int f3f8;			/* +0x3f8 fill level of `samples`;
					 *         cleared by cid_create and
					 *         cid_reset                */
};

/* One home for the sizeof claim above. */
#define CID_MODEM_BYTES	0x3fc

/*
 * Set the detector thresholds on whichever receivers the mode says exist:
 * the DTMF receiver's `sens` (modes 1 and 5) and the FSK receiver's f028
 * (modes 0 and 5).  `thr` arrives as an int and is stored as a short, which
 * is the object's own truncation.
 */
void cid_threshold(struct cid_modem *ctx, int thr);

/* Store `v` at +0x264 -- the same slot cid_create seeds from its 2nd arg. */
void cid_value(struct cid_modem *ctx, int v);

/*
 * Render whatever the receivers collected into `ctx->strings` and return it.
 * The buffer is cleared first, in full, so the answer is always a run of
 * NUL-terminated fields even when nothing rendered.
 *
 * WHICH RECEIVER IS ASKED is `mode != 0 && mode != 2` -- the DTMF side then,
 * and the FSK side otherwise.  Note that mode 2 takes the FSK path despite
 * being above 1, which no other function in this file does; `cid_create`
 * clamps anything above 1 to 5, so it is reachable only by a caller that
 * writes `mode` itself.
 *
 * On the DTMF side it copies SIXTEEN bytes of the receiver's `digits[20]`,
 * with no terminator of its own -- the twentieth byte and the terminator are
 * the DTMF receiver's business.  On the FSK side `ctx->f264 == 2` selects the
 * raw hex dump and anything else the labelled rendering.
 */
char *cid_get_strings(struct cid_modem *ctx);

/*
 * Build the service object and its receivers.  A null `ctx` allocates
 * CID_MODEM_BYTES and takes `mode` from the argument -- clamped to
 * CID_MODE_AUTOMATIC above 1; a non-null one keeps the mode it has and only
 * (re)builds, handing each constructor the existing pointer so a second call
 * reuses the allocation.  `cid_val` lands at +0x264.  Null on failure.
 *
 * Returns `void *` because `CID_create` stores it as one; the object it hands
 * back is the `struct cid_modem` it was given or allocated.
 */
void *cid_create(struct cid_modem *ctx, int cid_val, int mode);

/*
 * Free the receivers and the object.  `ctx->fsk` is freed unconditionally,
 * which is safe only because the modes that never build one leave it null.
 */
void cid_delete(struct cid_modem *ctx);

/*
 * The service's own state machine, and the top of the Caller ID receiver.
 *
 * It buffers `*count` samples from `in` into `ctx->samples` and, every time
 * that buffer fills to one block, hands the block to whichever receivers
 * `ctx->mode` selects and reads their verdicts; a call with enough samples
 * runs several blocks, and a partial block is left in the buffer for the next
 * call.  `*count` is set to zero on entry and never read again -- it is an
 * argument in name and an input-length-in, nothing-out in fact.
 *
 * THE THIRD ARGUMENT IS NEVER READ.  `0x48(%esp)` appears nowhere in the 992
 * bytes; it is kept because `CID_process` passes a literal 0 in that slot and
 * the object's stack layout says the function takes four.
 *
 * The result is what `CID_process` turns into its own: 0 while the message is
 * still arriving, 1 for a complete one, and 2 or 3 for the several ways it
 * gives up.  Nothing returns -1.
 */
short cid_progress(struct cid_modem *ctx, short *in, int what, short *count);

/*
 * Reset both receivers in place.  Same clamp as cid_create and the same
 * mode gating, but it BUILDS nothing: raising the mode here and then calling
 * this walks a null receiver, exactly as the object does.
 */
void cid_reset(struct cid_modem *ctx);

/*
 * Retune both receivers to a new LINE rate.  Same mode gating as
 * cid_threshold: the DTMF receiver's `rate` for modes 1 and 5, the FSK
 * receiver's for modes 0 and 5.  `rate` arrives as an int and is stored as a
 * short on both sides, which is the object's own truncation.
 *
 * See src/service/cid.c for what it does to the FSK receiver's f02c.
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

/* First entry at offset >= 2 whose tag is `tag`; -1 if none. */
int _look_for(const char *buf, char tag);

/*
 * First entry at offset >= `start` whose tag is none of 1, 2 or 7 and whose
 * offset is not `except`; -1 if none.  (Tags 1, 2 and 7 are the date/time
 * and the two number/name entries the caller has already taken; `except`
 * lets it skip one further entry by position.)
 */
int _look_for_other_than(const char *buf, short start, int except);

/*
 * Hex-dump a raw message: buf[1] + 2 bytes (capped at 0xf5), two lowercase
 * hex digits each, NUL-terminated.  `out` therefore needs up to 2*0xf5 + 1
 * bytes.  The cap is on the BYTE count before doubling, which is what the
 * object does -- not on the output length.
 */
void data_raw(const char *buf, char *out);

/*
 * The two renderers `cid_get_strings` picks between, both taking the FSK
 * receiver and the 0x258-byte scratch at `cid_modem + 0x008`.  The message is
 * `cid->data` and `cid->pack_len` says whether there is one; both produce a
 * run of NUL-terminated fields, which is the shape `CID_process` walks.
 *
 * `data_unformatted_output` is one inlined `data_raw` over the whole message.
 *
 * `data_formatted_output` writes `LABEL = value` fields -- DATE, TIME, NMBR
 * and, for a multiple-data-message frame, NAME and one MESG per leftover
 * tag.  It writes NOTHING when `pack_len` is zero, and 600 bytes is the
 * object's own budget for it.  See src/service/data.c for the four places it
 * runs past what a careful reading would allow, all of them reproduced.
 */
void data_unformatted_output(struct cid *cid, char *out);
void data_formatted_output(struct cid *cid, char *out);

#endif /* DSPLIB_CID_MODEM_H */
