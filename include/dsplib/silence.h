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
	 * +0x0e was `pad_0e[2]` -- REMOVED (finding F10145): `saw_signal`
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

/*
 * Initialise `s`, allocating it when NULL -- and, unlike FIFO8_create,
 * checking that allocation: a failure returns NULL rather than faulting.
 */
struct silence *silence_create(struct silence *s, void *obj,
			       unsigned int (*query)(void *obj, int what));

/* Free the object. */
void silence_delete(struct silence *s);

/*
 * Has the silent run passed `t`?
 *
 * The object multiplies `t` by 10.0f and truncates toward zero before the
 * comparison, and `count` advances once per 100 ms block -- so `t` is in
 * SECONDS.  (silence_progress is what settles that; the name is the blob's.)
 */
int silence_is_more_then(struct silence *s, float t);

/*
 * One block of `n` samples.  Appends `DLE q` or `DLE s` at `*out` and adds 2
 * to `*len` for each silence decision the block completes; `out` is NOT
 * advanced between decisions -- see docs/deviations.md D983.
 */
void silence_progress(struct silence *s, float *buf, short n,
		      unsigned char *out, unsigned short *len);

/*
 * Append a two-byte DLE escape -- 0x10 then `code` -- at `*out`, and add 2
 * to `*len`.  It is `_status` in the blob and it is not part of the silence
 * detector, but silence_progress is the only reconstructed caller, and the
 * object inlines it into both of that function's arms.
 *
 * The debug line the object prints for it is "DLE %d", with `code` promoted
 * from a signed char -- which is what types the third parameter.
 */
void _status(unsigned char *out, unsigned short *len, char code);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_SILENCE_H */
