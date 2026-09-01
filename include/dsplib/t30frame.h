/*
 * t30frame.h -- Class 1 fax: naming a T.30 frame from its first three octets.
 *
 * Two functions and a 36-entry table, sitting in the object between the FIFO
 * cluster (`.text` 0x096bb0..0x096df0) and the modulation wrappers.  THE
 * SPLIT INTO AUTHOR FILES INSIDE THAT CLUSTER IS NOT ESTABLISHED: the two
 * functions follow `FIFO_full_test` with nothing but alignment between them,
 * and `FrameNames` follows `FIFO_CFG` in `.rodata` the same way, so the
 * addresses discriminate neither for nor against them sharing `fifo.c`'s
 * translation unit.  They are given their own file because they are their own
 * subject; that is a convention, not a measurement, and it is said here
 * rather than implied.
 *
 * `aReversedCharsArray` is NOT here -- see `class1tx.h`.
 */

#ifndef DSPLIB_T30FRAME_H
#define DSPLIB_T30FRAME_H

/*
 * The frame identifier recovered from the first three octets of an HDLC
 * frame, which for T.30 are the address, the control octet and the facsimile
 * control field.
 *
 *   address != 0xFF                  -> 0xFF, and nothing else is looked at
 *   control is 0x03 or 0x13          -> the FCF bit-reversed
 *   any other control                -> the FCF as it stands, OR 0x8000
 *
 * and in both of the last two an identifier above 0x84 is masked to seven
 * bits, which drops the final-frame bit T.30 puts at 0x80.
 *
 * THE 0x8000 IS A "NOT A STANDARD CONTROL OCTET" MARKER and the only caller
 * strips it before naming the frame (`and $0xffff7fff` at 0x95c7b).  It is
 * described by what sets it rather than named, because nothing in the object
 * says what it is for.
 *
 * The three arguments really are three octets and not a buffer, whatever the
 * name says: the object reads `cmpb` on the first and `movzbl` on the other
 * two, three separate argument slots.
 */
int GetT30FrameIDFromBuffer(unsigned char address, unsigned char control,
			    unsigned char fcf);

/*
 * The author's name for an identifier -- "DIS - Digital Identification
 * Signal" and so on, 36 of them, byte for byte as the object has them.  An
 * identifier not in the table is formatted into a 64-byte static as
 * "Unknown frame (ID=0x%02x)", so the answer is never NULL and the buffer is
 * overwritten by the next unknown.
 */
char *GetT30FrameNameByID(int id);

#endif /* DSPLIB_T30FRAME_H */
