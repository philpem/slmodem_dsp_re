/*
 * detector.h -- the tone detector object's three one-line setters.
 *
 * `detector_create` (0xad480) and `detector_progress` (0xad6e0) are not
 * reconstructed -- each still reaches unwritten symbols -- so the part of the
 * object modelled here is what the three setters and `detector_delete`
 * (0xad620, written) establish.  The SIZE is not a guess: `detector_create`
 * allocates 0x38 bytes at 0xad5da, and it stores 1 into +0x34 itself, so
 * "output in stream" is the created default.
 *
 * The two output modes are named from the author's own function names, which
 * is the strongest evidence available for a field with no format string:
 * detector_set_output_status writes 0 and detector_set_output_in_stream
 * writes 1 into the same word.
 */

#ifndef DSPLIB_DETECTOR_H
#define DSPLIB_DETECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Values of struct detector's `output_mode`. */
#define DETECTOR_OUTPUT_STATUS		0
#define DETECTOR_OUTPUT_IN_STREAM	1

struct cadence;
struct fdsp_tone;

struct detector {
	short	enable;			/* +0x00 detector_set_enable's word  */
	unsigned char pad_0002[0x04 - 0x02];
	/*
	 * +0x04..+0x20 are typed by `detector_delete`, which is the only
	 * reconstructed function that touches them: it hands +0x04 straight
	 * to `sysdep_free`, the three at +0x08 to `cadence_delete` (each
	 * guarded against NULL), and the four at +0x14 to `TONE_delete`
	 * through a subscript, which is what makes those four an ARRAY and
	 * the three cadences three separate fields.
	 */
	void	*ptr_0004;		/* +0x04 sysdep_free'd unconditionally */
	struct cadence *cadence_0008;	/* +0x08 */
	struct cadence *cadence_000c;	/* +0x0c */
	struct cadence *cadence_0010;	/* +0x10 */
	struct fdsp_tone *tone[4];	/* +0x14 +0x18 +0x1c +0x20 */
	unsigned char pad_0024[0x34 - 0x24];
					/* +0x24 detector_create's business  */
	int	output_mode;		/* +0x34 DETECTOR_OUTPUT_*           */
	/* 0x38 bytes in total -- detector_create's allocation size. */
};

/* Whole-word store; nothing here reads the value back. */
void detector_set_enable(struct detector *d, short enable);

/*
 * Tear the detector down: the +0x04 block, then all four tones, then whichever
 * of the three cadences are non-NULL, then the detector itself.  The tone
 * pointers are NOT guarded -- `TONE_delete` is handed all four whatever they
 * hold -- and neither is the +0x04 block.
 */
void detector_delete(struct detector *d);

void detector_set_output_status(struct detector *d);
void detector_set_output_in_stream(struct detector *d);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_DETECTOR_H */
