/*
 * detector.h -- the tone detector object's three one-line setters.
 *
 * `detector_create` (0xad480), `detector_progress` (0xad6e0) and
 * `detector_delete` (0xad620) are not reconstructed -- each still reaches
 * unwritten symbols -- so only the part of the object those three setters
 * establish is modelled here.  The SIZE is not a guess: `detector_create`
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

struct detector {
	short	enable;			/* +0x00 detector_set_enable's word  */
	unsigned char pad_0002[0x34 - 0x02];
					/* +0x02 detector_create's business  */
	int	output_mode;		/* +0x34 DETECTOR_OUTPUT_*           */
	/* 0x38 bytes in total -- detector_create's allocation size. */
};

/* Whole-word store; nothing here reads the value back. */
void detector_set_enable(struct detector *d, short enable);

void detector_set_output_status(struct detector *d);
void detector_set_output_in_stream(struct detector *d);

#ifdef __cplusplus
}
#endif

#endif /* DSPLIB_DETECTOR_H */
