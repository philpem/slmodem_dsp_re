/*
 * silence.h -- the voice path's silence detector, plus `_status`, which is
 * its neighbour in the blob and also the thing it emits through.
 *
 * The five symbols sit together at the end of the `Fdspkrnl.c` span:
 * _status 0xb02e0, silence_is_more_then 0xb0360, silence_create 0xb03b0,
 * silence_delete 0xb0410, silence_progress 0xb0420.
 *
 * What the detector does, in one paragraph: silence_progress accumulates the
 * sum of squares of 800 samples -- 100 ms at 8 kHz -- divides by 800, and
 * compares that against one of four thresholds picked by a setting it asks
 * its owner for.  Blocks under the threshold advance a counter; when the
 * counter passes a second setting, it appends a DLE escape and starts again.
 * The escape is `DLE q` once any block has been over the threshold and
 * `DLE s` until then, which is the voice-mode distinction between quiet
 * after speech and silence from the start.
 *
 * TWO FIELD NAMES COME FROM THE OBJECT'S OWN DEBUG LINES and not from
 * inference: "energy = %d, threshold = %d" names +0x10 and the table, and
 * "silence counter %d, level %d" names +0x08.
 */

#ifndef DSPLIB_SILENCE_H
#define DSPLIB_SILENCE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The two settings silence_progress asks its owner for, through the callback
 * the creator installed.  The numbers are the object's; what they SELECT is
 * named below from what the code does with the answers, which is all the
 * object says about them.
 */

/* Answer 1..3 picks a row of the threshold table; 0 or >3 disables. */
#define SILENCE_PARAM_LEVEL	0x52

/* Answer is the number of 100 ms blocks of silence before an escape. */
#define SILENCE_PARAM_TIME	0x53

/*
 * `sizeof` is 0x14 -- what silence_create asks sysdep_malloc for.  The two
 * leading words are its second and third arguments, and silence_progress is
 * what types them: it calls the second through the first.
 */
struct silence {
	void		*obj;		/* +0x00 handed back to the callback */
	unsigned int	(*query)(void *obj, int what);
					/* +0x04 the settings source         */
	unsigned short	count;		/* +0x08 consecutive silent blocks;
					 *       the object's own debug line
					 *       calls it the silence counter */
	unsigned short	nsamp;		/* +0x0a samples into the current
					 *       800-sample block            */
	short		saw_signal;	/* +0x0c set by the first block over
					 *       the threshold, never cleared;
					 *       picks `q` over `s`          */
	/*
	 * +0x0e was `pad_0e[2]` -- REMOVED (finding F10151): `saw_signal`
	 * ends at +0x0e and the struct's own 4-byte alignment (forced by
	 * `obj`/`query`) leaves exactly this gap ahead of `energy`, a
	 * `float`. `dis.py` over `silence_create`/`silence_delete`/
	 * `silence_is_more_then`/`silence_progress` finds no access to
	 * offset 0x0e/0x0f.
	 */
	float		energy;		/* +0x10 the running sum of squares,
					 *       then the block mean; the
					 *       debug line calls it energy  */
};

/**
 * @brief Construct a silence detector.
 *
 * Allocates @p s when it is NULL and, unlike FIFO8_create(), checks that
 * allocation: a failure returns NULL rather than faulting.
 *
 * @param s      Existing state to initialise, or NULL to allocate one.
 * @param obj    Opaque pointer passed back to @p query.
 * @param query  Callback the detector asks for #SILENCE_PARAM_LEVEL and
 *               #SILENCE_PARAM_TIME.
 * @return @p s, the newly allocated state, or NULL on allocation failure.
 */
struct silence *silence_create(struct silence *s, void *obj,
			       unsigned int (*query)(void *obj, int what));

/** @brief Free a silence detector. @param s The detector to free. */
void silence_delete(struct silence *s);

/**
 * @brief Has the current silent run passed @p t seconds?
 *
 * The object multiplies @p t by 10.0f and truncates toward zero before
 * comparing against `count`, which advances once per 100 ms block -- so
 * @p t is in seconds (silence_progress() is what settles that; the name
 * is the blob's own).
 *
 * @param s  Detector state.
 * @param t  Threshold, in seconds.
 * @return Non-zero if the current silent run has passed @p t.
 */
int silence_is_more_then(struct silence *s, float t);

/**
 * @brief Process one block of @p n samples and emit any silence escapes.
 *
 * Accumulates the sum of squares of 800 samples (100 ms at 8 kHz), and at
 * each completed block compares the mean against a threshold (picked via
 * #SILENCE_PARAM_LEVEL); blocks under it advance a counter, and once the
 * counter passes #SILENCE_PARAM_TIME blocks it appends a DLE escape
 * (`DLE q` if any prior block was over the threshold, `DLE s` otherwise)
 * and restarts.
 *
 * @param s    Detector state.
 * @param buf  Input samples, @p n of them.
 * @param n    Number of samples in @p buf.
 * @param out  Output buffer; each silence decision appends 2 bytes at
 *             `*out`. `out` itself is NOT advanced between decisions
 *             within one call (deviation D983).
 * @param len  Input/output: bumped by 2 for each decision this block
 *             completes.
 */
void silence_progress(struct silence *s, float *buf, short n,
		      unsigned char *out, unsigned short *len);

/**
 * @brief Append a two-byte DLE escape (0x10, @p code) at `*out`.
 *
 * Not part of the silence detector proper -- it is `_status` in the blob,
 * a neighbour that silence_progress() is the only reconstructed caller of
 * (inlined into both of that function's arms in the object). The debug
 * line the object prints for it is "DLE %d", with @p code promoted from
 * a signed char.
 *
 * @param out   Output buffer; the escape is appended at `*out`.
 * @param len   Input/output: bumped by 2.
 * @param code  The DLE escape code (e.g. 'q' or 's').
 */
void _status(unsigned char *out, unsigned short *len, char code);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_SILENCE_H */
