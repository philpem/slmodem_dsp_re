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

/*
 * .bss 0x0008c8, 8 bytes -- exactly `sizeof(struct sgd_control_req)`, and
 * never written anywhere in the object.  V.17, V.27ter and V.29's transmit
 * half-duplex machines (`TxNextStateV17`/`V27`/`V29`, 7+4+2 = 13 sites, all
 * of them a plain load) each build a fresh `sgd_control_req` on their own
 * stack for every state transition, filling `gen` locally and copying
 * `SGD_CTL.det` straight across unread and unset -- so every call this
 * object makes to `SGD_control` through it passes a `det` of NULL, and the
 * detector half of `SGD_control` is a no-op on every one of these sites.
 * Bare name, no protocol prefix, shared across all three modulations'
 * transmit families symmetrically: this is shared TX-side scratch, not any
 * one modulation's table.  See `SGD_CTL` in `Sgd.c` and findings F9600/F9700.
 */
extern struct sgd_control_req SGD_CTL;

/**
 * @brief Construct an SGD object, or re-arm a caller-supplied one.
 *
 * Copies @p cfg (or the built-in ::SGD_CFG if NULL) into the object,
 * resets the generator position/repeat count and the whole status block,
 * and zeroes `hist_span` symbols of the receive history. The history
 * buffer itself is allocated only when @p s is NULL (a fresh object); a
 * caller-supplied @p s must already have a valid `hist` pointer, since
 * this always zeroes through it.
 *
 * @param s    Existing object to re-arm, or NULL to allocate a new one
 *             (and its history buffer).
 * @param cfg  Configuration, or NULL for the built-in default.
 * @return @p s, or the newly allocated object.
 */
struct sgd *SGD_create(struct sgd *s, const struct sgd_cfg *cfg);

/**
 * @brief Free an SGD object and its history buffer.
 * @param s The object to free.
 */
void SGD_delete(struct sgd *s);

/**
 * @brief Replace either or both halves of an SGD object's configuration.
 *
 * A NULL @p req->gen or @p req->det leaves that half untouched. Applying
 * the generator half resets `seq_pos`, `status.seq_reps` and `word_left`;
 * applying the detector half re-derives `hist_span` and `thresh`,
 * re-zeroes the history, and clears every status field except
 * `seq_reps` -- so a detector-only control leaves the generator's
 * completion count standing.
 *
 * @param s    Object to reconfigure.
 * @param req  Request; either pointer may be NULL.
 */
void SGD_control(struct sgd *s, struct sgd_control_req *req);

/**
 * @brief Read out and clear an SGD object's status block.
 *
 * Copies the whole status block to @p out (a struct assignment, so any
 * bytes the object never writes ride out as whatever the allocator left),
 * then clears every field except `seq_reps`.
 *
 * @param s    Object to read.
 * @param out  Destination for the status block.
 */
void SGD_status(struct sgd *s, struct sgd_status *out);

/**
 * @brief Shift @p n symbols out of the current data word.
 *
 * Emits the fields of `cfg.gen.data_word`, most significant first,
 * wrapping back to the top of the word (and repeating it) once
 * `word_syms` fields have been emitted.
 *
 * @param s    Object.
 * @param out  Output buffer, @p n symbols.
 * @param n    Number of symbols to emit.
 * @return Fields of the current word consumed so far, i.e.
 *         `(word_syms - 1) - word_left` after this call.
 */
short SGD_symbol_gen(struct sgd *s, unsigned short *out, short n);

/**
 * @brief Emit @p n symbols of the training sequence.
 *
 * An "active" generator (repeating, or on its first pass) emits the
 * configured sequence; an inactive one emits `idle_sym`. A repeating
 * generator bumps `status.seq_reps` and restarts at the end of the
 * sequence; a one-shot generator sets `status.seq_reps` to 1 and goes
 * inactive.
 *
 * @param s    Object.
 * @param out  Output buffer, @p n symbols.
 * @param n    Number of symbols to emit.
 * @return Symbols of the sequence remaining: `seq_len - seq_pos`.
 */
short SGD_sequence_gen(struct sgd *s, unsigned short *out, short n);

/**
 * @brief Correlate two symbol runs by Hamming distance and report a score.
 *
 * Sums `FPM_xor_table[a[i] ^ b[i]]` over @p n symbols and scales by
 * `1 / (*p * n)` in INTEGER arithmetic (deviation D1064) -- which is 1
 * only when `*p * n` is exactly +-1 and 0 for every other magnitude
 * (and a divide fault when it is zero), so for any real configuration
 * the accumulation is dead and the function answers a constant 1.
 * Reproduced exactly; both runs must be byte-ranged (see F8494) or the
 * table index runs past `FPM_xor_table`.
 *
 * @param p  Pointer to the scale's divisor.
 * @param a  First symbol run, @p n symbols.
 * @param b  Second symbol run, @p n symbols.
 * @param n  Number of symbols to correlate.
 * @return `1 - acc * scale`, effectively a constant 1 for real inputs.
 */
short SGD_correlate(const short *p, const unsigned short *a,
		    const unsigned short *b, short n);

/**
 * @brief Slide @p n new symbols into the history and search for the
 *        reference sequence.
 *
 * Appends @p sym to the history (discarding the oldest @p n), then
 * correlates the reference sequence against @p n successive alignments,
 * keeping the smallest Hamming distance (ties to the earliest alignment,
 * with an early exit on an exact match). A distance above `thresh` is no
 * detection: the status block is left untouched and -1 is returned.
 *
 * @param s    Object (history buffer, reference sequence, threshold).
 * @param sym  New symbols to append, @p n of them.
 * @param n    Number of new symbols; deviation D1060 requires `n >= 1`
 *             (an `n <= 0` call reads an uninitialised alignment index,
 *             reproducing the object's own behaviour).
 * @return The winning alignment's index, or -1 if none was within
 *         threshold.
 */
short SGD_sequence_det(struct sgd *s, const unsigned short *sym, short n);

/**
 * @brief Walk @p n symbols bit-serially through the pattern matcher.
 *
 * Shifts each symbol into a 32-bit register, most significant bit first,
 * and after every bit compares the register against `pat_match` under
 * `pat_mask`. On a hit, the remaining bits of the symbol that carried it
 * are still shifted in (so the register stays aligned to the symbol
 * boundary), and the index of that symbol is returned.
 *
 * @param s    Object (shift register and pattern configuration).
 * @param sym  Symbols to walk, @p n of them.
 * @param n    Number of symbols. `cfg.sym_bits == 16` never terminates
 *             (deviation D1062, reproduced from the object's arithmetic
 *             mask shift).
 * @return The index of the symbol that completed a match, or -1 if the
 *         whole run passed without one (with the register saved either
 *         way).
 */
short SGD_pattern_det(struct sgd *s, const short *sym, short n);

#endif /* DSPLIB_SGD_H */
