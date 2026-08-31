/*
 * faxfifo.h -- Class 1 fax: the byte FIFO, as far as this batch reads it.
 *
 * Only `FIFO_full_test` is reconstructed (finding F8320's no-entry-point
 * bucket), so only the two fields it reads are modelled; `FIFO_create`
 * (.text 0x096bb0) lays the rest out and belongs to the fax phase.
 */

#ifndef DSPLIB_FAXFIFO_H
#define DSPLIB_FAXFIFO_H

struct fax_fifo {
	unsigned char pad_00[2];	/* +0x00                            */
	short size;			/* +0x02 capacity, in bytes         */
	unsigned char pad_04[8];	/* +0x04                            */
	unsigned short count;		/* +0x0c bytes held                 */
};

/*
 * Occupancy in Q14: 14747 / 16384 is 0.90008..., so this answers "at least
 * 90% full".
 */
#define FIFO_FULL_Q14	0x399b

/*
 * 1 when count/size >= 90%, 0 below -- as WRITTEN.  As COMPILED the
 * arithmetic is a 16-bit chain: count << 14 is TRUNCATED TO A SHORT before
 * the divide, so a count of 2 lands on -32768 and, for any POSITIVE size,
 * everything above count == 1 answers "not full" through a negative
 * quotient; with a positive size the only input that can answer 1 is
 * count == 1 with size == 1.  Reproduced, not repaired;
 * docs/deviations.md D952.  `size` is read signed, and zero divides.
 */
int FIFO_full_test(struct fax_fifo *f);

#endif /* DSPLIB_FAXFIFO_H */
