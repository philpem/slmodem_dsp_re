/*
 * sgd.h -- the SGD sequence generator/detector, as the object lays it out.
 *
 * SGD is the training-sequence engine the Class 1 fax modulations share: it
 * emits a symbol sequence (or the bits of one data word) on the transmit
 * side, and on the receive side slides a history of received symbols past a
 * reference sequence looking for the best Hamming-distance alignment, plus a
 * separate bit-serial pattern matcher over a shift register.
 *
 * NAMES.  The object contains no format string that prints any of these
 * fields and no already-written caller types them, so every field name below
 * is USAGE INFERENCE -- CLAUDE.md's weakest class of evidence -- read from
 * what the nine functions do with it.  Where a role could not be bounded the
 * field keeps a `type_NNNN` name rather than a guessed one.  What is NOT
 * inference is the layout: every offset, width and signedness here was read
 * from `tools/dis.py` over the nine symbols.
 *
 * THE THREE CONFIG BLOCKS ARE THE OBJECT'S OWN DIVISION, not a tidying.
 * `SGD_control` takes a request of two pointers and copies six dwords from
 * the first into +0x08..+0x1f and five dwords from the second into
 * +0x20..+0x33, leaving +0x00..+0x07 settable only at construction.  So the
 * generator half and the detector half are separately replaceable structs
 * and the first two dwords are not.
 */

#ifndef DSPLIB_SGD_H
#define DSPLIB_SGD_H

/*
 * The generator half -- object +0x08, and SGD_control's first pointer.
 */
struct sgd_gen_cfg {
	const unsigned short *seq;	/* +0x00 symbol sequence to emit    */
	unsigned short seq_len;		/* +0x04 its length, in symbols     */
	short short_0006;		/* +0x06 never read by any SGD fn   */
	int seq_enable;			/* +0x08 non-zero: repeat for ever  */
	unsigned short idle_sym;	/* +0x0c emitted once the sequence
					 *       has run out and does not
					 *       repeat                     */
	short short_000e;		/* +0x0e never read                 */
	unsigned short data_word;	/* +0x10 the word SGD_symbol_gen
					 *       shifts out, most significant
					 *       field first                */
	short short_0012;		/* +0x12 never read                 */
	unsigned short word_syms;	/* +0x14 fields per data word       */
	short short_0016;		/* +0x16 never read                 */
};

/*
 * The detector half -- object +0x20, and SGD_control's second pointer.
 */
struct sgd_det_cfg {
	const unsigned short *ref;	/* +0x00 reference sequence         */
	unsigned short ref_len;		/* +0x04 its length, in symbols     */
	short ref_margin;		/* +0x06 threshold term: the accept
					 * limit is sym_bits*ref_len*
					 * (0x4000 - ref_margin) truncated to
					 * a short, so a LARGER margin is a
					 * TIGHTER threshold                */
	int pat_match;			/* +0x08 value the shift register
					 *       must equal, after masking  */
	int pat_mask;			/* +0x0c mask applied before the
					 *       comparison                 */
	int pat_out_mask;		/* +0x10 mask applied to the shift
					 *       register when reporting the
					 *       match in status.pat_data   */
};

/*
 * The whole 13-dword configuration, which `SGD_create` copies into the
 * object with one `rep movsl` -- a struct assignment of exactly this type.
 * `SGD_CFG` is the blob's default instance.
 */
struct sgd_cfg {
	short sym_bits;			/* +0x00 bits per symbol            */
	short hist_len;			/* +0x02 receive history, in symbols*/
	short hist_extra;		/* +0x04 extra symbols the history
					 * allocation carries; the buffer is
					 * 2*(hist_len+hist_extra)-2 BYTES
					 * while the zeroed span is
					 * hist_len+ref_len-1 SYMBOLS -- see
					 * the note on `hist` below         */
	short short_0006;		/* +0x06 never read                 */
	struct sgd_gen_cfg gen;		/* +0x08                            */
	struct sgd_det_cfg det;		/* +0x20                            */
};

/*
 * What `SGD_status` hands back, and the same 24 bytes that live at object
 * +0x34.  It is copied out with a STRUCT ASSIGNMENT -- six dword loads and
 * six dword stores -- and then cleared FIELD BY FIELD, two of the six as
 * 16-bit stores.  That asymmetry is why the two 2-byte holes are modelled
 * rather than padded away: the copy carries them out, nothing ever writes
 * them, and in a `SGD_create`-allocated object they hold whatever the
 * allocator left.  A test that compares this struct dword-wise is comparing
 * allocator fill in two of its six words, and must arrange for both sides to
 * see the same fill.
 */
struct sgd_status {
	int seq_found;			/* +0x00 1 once a sequence aligned
					 *       inside the threshold       */
	unsigned short *det_at;		/* +0x04 where in the HISTORY BUFFER
					 *       the best alignment starts  */
	short quality;			/* +0x08 1 - best/(sym_bits*ref_len),
					 * in INTEGER arithmetic, so it is 1
					 * for every alignment whose distance
					 * is under sym_bits*ref_len        */
	short short_000a;		/* +0x0a never written              */
	int pat_found;			/* +0x0c 1 once the bit pattern hit */
	int pat_data;			/* +0x10 the shift register at the
					 *       hit, masked by pat_out_mask*/
	unsigned short seq_reps;	/* +0x14 how many times the emitted
					 * sequence has restarted; set to 1
					 * rather than incremented when the
					 * generator is one-shot            */
	short short_0016;		/* +0x16 never written              */
};

/*
 * The object itself: 0x5c bytes, which is the literal `SGD_create` passes to
 * sysdep_malloc.
 */
struct sgd {
	struct sgd_cfg cfg;		/* +0x00                            */
	struct sgd_status status;	/* +0x34                            */
	unsigned short seq_pos;		/* +0x4c generator's index into seq */
	short word_left;		/* +0x4e SGD_symbol_gen's countdown
					 * through one data word; reset to
					 * word_syms-1 by create and by a
					 * generator-half control           */
	unsigned short *hist;		/* +0x50 receive history.  ALLOCATED
					 * ONLY ON THE FRESH PATH -- when the
					 * caller supplies the object,
					 * `SGD_create` copies 13 config
					 * dwords over +0x00..+0x33 and never
					 * touches +0x50, then zeroes
					 * hist_span symbols through it.  So
					 * a caller-supplied object must
					 * carry a valid buffer here already*/
	short hist_span;		/* +0x54 hist_len+ref_len-1, the
					 * span create/control zero         */
	short thresh;			/* +0x56 accept limit, see
					 *       det.ref_margin             */
	int pat_sr;			/* +0x58 pattern shift register     */
};

/* The blob's default configuration, .data 0x80e0. */
extern struct sgd_cfg SGD_CFG;

/*
 * `SGD_control`'s request.  Either pointer may be null, and each half is
 * applied independently: the generator half also resets seq_pos, seq_reps
 * and word_left, and the detector half additionally re-derives hist_span and
 * thresh, re-zeroes the history and clears five of the six status fields --
 * every one except seq_reps, which only the generator half clears.
 */
struct sgd_control_req {
	struct sgd_gen_cfg *gen;
	struct sgd_det_cfg *det;
};

struct sgd *SGD_create(struct sgd *s, const struct sgd_cfg *cfg);
void SGD_delete(struct sgd *s);
void SGD_control(struct sgd *s, struct sgd_control_req *req);
void SGD_status(struct sgd *s, struct sgd_status *out);

short SGD_symbol_gen(struct sgd *s, unsigned short *out, short n);
short SGD_sequence_gen(struct sgd *s, unsigned short *out, short n);

short SGD_correlate(const short *p, const unsigned short *a,
		    const unsigned short *b, short n);
short SGD_sequence_det(struct sgd *s, const unsigned short *sym, short n);
short SGD_pattern_det(struct sgd *s, const short *sym, short n);

#endif /* DSPLIB_SGD_H */
