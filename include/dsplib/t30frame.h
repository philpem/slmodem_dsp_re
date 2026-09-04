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
/**
 * @brief Name a T.30 frame from its first three octets.
 *
 * @param address  The frame's address octet; anything other than 0xFF
 *                 answers 0xFF and looks at nothing else.
 * @param control  The frame's control octet; 0x03 or 0x13 bit-reverses
 *                 @p fcf, anything else uses it as-is OR'd with 0x8000.
 * @param fcf      The facsimile control field.
 * @return The frame identifier: 0xFF for a non-standard address; otherwise
 *         @p fcf (bit-reversed or not, per @p control), masked to seven
 *         bits if above 0x84 (dropping T.30's final-frame bit), OR'd with
 *         0x8000 if @p control was not 0x03 or 0x13.
 */
int GetT30FrameIDFromBuffer(unsigned char address, unsigned char control,
			    unsigned char fcf);

/**
 * @brief Look up a T.30 frame identifier's name.
 *
 * @param id  Frame identifier, as returned by GetT30FrameIDFromBuffer().
 * @return The author's name for the identifier (e.g. "DIS - Digital
 *         Identification Signal") if it is one of the 36 known ones;
 *         otherwise "Unknown frame (ID=0x%02x)" formatted into a shared
 *         64-byte static buffer, which the next unknown-ID call
 *         overwrites. Never NULL.
 */
char *GetT30FrameNameByID(int id);

#endif /* DSPLIB_T30FRAME_H */
