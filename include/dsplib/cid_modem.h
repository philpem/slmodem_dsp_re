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
 * Only what this batch's leaves touch is modelled; the rest is padding until
 * the CID service pass (cid_modem, cid_progress, cid_get_strings) names it.
 */

#ifndef DSPLIB_CID_MODEM_H
#define DSPLIB_CID_MODEM_H

struct dtmf_rx;
struct cid;

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
	unsigned char pad_268[0x190];	/* +0x268                           */
	int f3f8;			/* +0x3f8 cleared by cid_create and
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
