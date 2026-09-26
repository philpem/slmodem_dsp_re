/*
 * faxvmi_hdlc.c -- Class 1 fax: the VMI's HDLC framing codec.
 *
 * Split out of the merged faxvmi.c; the FILE is faxvmi_hdlc.c, between
 * faxvmi_asyc.c and faxvmi_pack.c; its two functions fill [0x95b10, 0x964e0)
 * contiguously.  Bodies moved verbatim (finding F11399).
 */
#include <stddef.h>

#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/nulldp.h"
#include "dsplib/sysdep.h"
#include "dsplib/t30frame.h"

/*
 * HDLC framing on the way OUT, and it is the packer the other two are simple
 * cases of.
 *
 * WHAT COMES OUT OF THE RING IS LENGTH-PREFIXED, which is exactly what
 * `faxvmi_write_frame` puts in: one element of length, then that many octets,
 * then the two FCS octets it appended.  So the refill has three states and
 * `framer->pack_frame_left` is what selects between them:
 *
 *   left == 0, ring has data   the element is a LENGTH.  Take it, and send
 *                              THREE flag octets -- word 0x7E7E7E, mask
 *                              0x800000, twenty-four bits -- before any of the
 *                              frame.  `pack_flagging` goes to 1.
 *   left != 0                  the element is a data octet.  Word is the
 *                              octet, mask 0x80, `pack_flagging` goes to 0,
 *                              and `pack_frame_left` counts down.
 *   ring empty                 one flag octet, 0x7E with mask 0x80, and
 *                              `vmi->underrun` goes to 1.  That is the idle
 *                              pattern, not a fault report.
 *
 * ZERO INSERTION IS GATED ON `pack_flagging`, which is what makes the flags
 * transparent: when five consecutive ones have been shifted into the
 * accumulator and the source is NOT a flag, the object clears the current bit
 * IN `pack_word` and does NOT advance the mask, so the same bit position is
 * emitted again -- as a zero -- on the next pass.  The accumulator therefore
 * receives the stuffed zero and the octet's remaining bits follow it.
 *
 * THE MASK IS 32 BITS HERE AND ONLY HERE.  The other two packers hold it in
 * an `unsigned short`, and their loads are `movzwl`; this one loads and
 * stores the whole dword (0x95b4c and 0x95d60), because 0x800000 does not fit
 * in sixteen bits.  That is what makes `pack_mask` an `unsigned int` rather
 * than a short with a wide store.
 *
 * THE DEBUG PREAMBLE IS NOT ALL DEBUG.  When `count` is non-zero the object
 * walks the ring from `rd + 1` to `wr` and copies three octets out of it
 * WHATEVER the debug level is; only the printing is guarded.  It is written
 * that way here because that is what is there, and because the walk reads
 * `fifo[rd + 1]` without wrapping that first index -- D1121.
 */
int
faxvmi_hdlc_frame(struct faxvmi *vmi, unsigned short *src, short count)
{
	struct faxvmi_framer *fr;
	struct faxvmi_link *lk;
	unsigned short *out;
	unsigned int word, acc, mask, elem_mask;
	unsigned short bit, width;
	short n, nleft, left;
	int flagging;

	left = (short)(count - faxvmi_write_frame(vmi, &src, count));

	fr = vmi->framer;
	lk = vmi->link;
	word = fr->pack_word;
	mask = fr->pack_mask;
	acc = fr->pack_acc;
	bit = fr->pack_bit;
	n = lk->pack_count;
	out = lk->buf;
	width = lk->pack_width;
	elem_mask = (1u << width) - 1;
	nleft = fr->pack_frame_left;
	flagging = fr->pack_flagging;

	if (count != 0) {
		unsigned short len = fr->fifo[fr->rd];
		unsigned int i;

		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf(
			    "HDLC Transmitted Frame (Length = %d, iR = %d,"
			    " iW = %d): \n", len, fr->rd, fr->wr);

		/* D1121: `rd + 1` is not wrapped before the first read */
		i = (unsigned int)fr->rd + 1;
		while (i != (unsigned int)fr->wr) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("%02X,", fr->fifo[i]);
			i = ((int)fr->fifo_size > (int)(i + 1)) ? i + 1 : 0;
		}
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("\n");

		if (len > 2) {
			unsigned char hdr[3];
			int k;

			hdr[0] = hdr[1] = hdr[2] = 0;
			i = (unsigned int)fr->rd + 1;
			for (k = 0; k <= 2; k++) {
				hdr[k] = (unsigned char)fr->fifo[i];
				i = ((int)fr->fifo_size > (int)(i + 1))
				    ? i + 1 : 0;
			}
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "FCL1: FRAME TRANSMITTED (%s)\n",
				    GetT30FrameNameByID(
					(int)(GetT30FrameIDFromBuffer(hdr[0], hdr[1],
								hdr[2])
					& 0xffff7fff)));
		}
	}

	while (n != 0) {
		unsigned int b;

		if (mask == 0) {
			if (fr->count != 0) {
				unsigned short rd = fr->rd;

				if (nleft != 0) {
					word = fr->fifo[rd];
					mask = 0x80;
					flagging = 0;
					nleft = (short)(nleft - 1);
				} else {
					nleft = (short)fr->fifo[rd];
					word = 0x7e7e7e;
					mask = 0x800000;
					flagging = 1;
				}
				fr->rd = (unsigned short)(rd + 1);
				fr->count = (unsigned short)(fr->count - 1);
				if (fr->rd >= fr->fifo_size)
					fr->rd = 0;
			} else {
				word = 0x7e;
				mask = 0x80;
				flagging = 1;
				vmi->underrun = 1;
			}
		}
		b = (word & mask) != 0;
		acc = (acc << 1) | b;
		/* `&`, not `&&`: the object computes both with `sete` and
		 * ANDs the results (0x95cf5..0x95d02) */
		if ((flagging == 0) & ((acc & 0x1f) == 0x1f))
			word &= ~mask;	/* the stuffed zero: same bit again */
		else
			mask >>= 1;
		bit = (unsigned short)(bit + 1);
		if (bit == width) {
			bit = 0;
			*out++ = (unsigned short)(acc & elem_mask);
			n = (short)(n - 1);
		}
	}

	fr->pack_mask = mask;
	fr->pack_acc = acc;
	fr->pack_word = word;
	fr->pack_frame_left = nleft;
	fr->pack_flagging = flagging;
	fr->pack_bit = bit;

	left = (short)(left - faxvmi_write_frame(vmi, &src, left));
	vmi->framer->residue = left;
	return vmi->link->pack_count;
}

/*
 * HDLC framing.
 *
 * The bit half is ordinary: a run of five ones swallows the next zero, a run
 * of six is a flag, and eight collected bits are an octet appended to
 * `framer->frame`.  Octets are only kept once `flags_wanted` has counted down
 * -- FAXVMI_create and FAXVMI_control both set it to 2, so two opening flags
 * are required before a frame is believed.
 *
 * THE REPAIR PASS IS THE INTERESTING HALF, and the author's own words for it
 * are in the format strings below.  A frame whose FCS is wrong is not dropped
 * on the first look.  It is retried with the T.30 address octet forced, then
 * with the control octet forced, and then the whole buffer is walked one bit
 * to the left and all three tried again, up to seven times -- a one-bit slip
 * in the receiver is recoverable and costs only arithmetic.  Only when that
 * is exhausted is the frame emitted with a length of ZERO, which still
 * advances the frame count.
 *
 * SIZE `dst` FROM THE INPUT, NOT FROM `vmi->max_frame`: the guard on the
 * output is `emitted + frame_len >= max_frame` and the object never
 * accumulates `emitted` (D1073), so several frames in one call can write
 * several times `max_frame` elements.
 */
int
faxvmi_hdlc_unframe(struct faxvmi *vmi, unsigned short *dst, short count)
{
	struct faxvmi_framer *fr = vmi->framer;
	struct faxvmi_link *lk = vmi->link;
	unsigned int word = fr->unpack_word;
	unsigned int acc = fr->unpack_acc;
	unsigned short mask = fr->unpack_mask;
	unsigned short bit = fr->unpack_bit;
	unsigned short *src = lk->buf;
	unsigned int top = 1u << (lk->unpack_width - 1);
	short flen = fr->frame_len;
	short ones = fr->ones;
	short nframes = 0;
	short left = count;
	/*
	 * D1073.  The object's own update of this is a no-op -- it reloads
	 * the slot at 0x96384, sign-extends it and stores it straight back --
	 * so it is zero for the whole call and the guard below is really
	 * `frame_len >= max_frame`.  Reproduced: writing the accumulation the
	 * name suggests would make this function reject frames the object
	 * accepts.
	 */
	short emitted = 0;
	int overflow = 0;

	while (left != 0) {
		unsigned int b;
		unsigned short fcs;
		short save0, save1;
		short shift, len, i;

		if (mask == 0) {
			mask = (unsigned short)top;
			word = *src++;
			left = (short)(left - 1);
		}
		b = (word & mask) != 0;
		mask = (unsigned short)(mask >> 1);

		if (ones == 5 && b == 0) {
			/* the stuffed zero: it is not data */
			goto next_bit;
		}
		if (ones != 6) {
			acc = (acc << 1) | b;
			bit = (unsigned short)(bit + 1);
			if (bit == 8) {
				if (fr->flags_wanted == 0
				    && (int)flen < (int)fr->frame_size) {
					fr->in_frame = 1;
					fr->frame[flen] =
					    (unsigned short)(acc & 0xff);
					flen = (short)(flen + 1);
				}
				bit = 0;
			}
			goto next_bit;
		}

		/* ones == 6: a flag, 0111 1110, and `b` is its closing zero */
		if (b == 0 && fr->flags_wanted != 0) {
			fr->flags_wanted =
			    (unsigned short)(fr->flags_wanted - 1);
			goto close_flag;
		}
		if (b != 0 || fr->in_frame == 0)
			goto close_flag;

		/* a closing flag over octets that were kept: a whole frame */
		fcs = (unsigned short)faxvmi_gen_fcs16(fr->frame, flen);
		if (fcs == FAXVMI_FCS16_GOOD)
			goto emit;

		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("Frame with bad CRC\n");
		for (i = 0; i < flen; i++)
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("%2x ", fr->frame[i]);
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("\n");
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("Replacing the first byte...\n");

		save0 = (short)fr->frame[0];
		fr->frame[0] = FAXVMI_T30_ADDRESS;
		fcs = (unsigned short)faxvmi_gen_fcs16(fr->frame, flen);
		if (fcs == FAXVMI_FCS16_GOOD)
			goto emit;

		save1 = (short)fr->frame[1];
		if (dsplibs_debug_level > 1)
			dsplibs_debug_printf("Replacing the second byte...\n");
		fr->frame[1] = FAXVMI_T30_CONTROL;
		fcs = (unsigned short)faxvmi_gen_fcs16(fr->frame, flen);
		if (fcs == FAXVMI_FCS16_GOOD)
			goto emit;

		/*
		 * Neither substitution helped, so the alignment is suspect.
		 * Put the two octets back, drop one from the length, and walk
		 * the whole buffer one bit to the left up to seven times,
		 * trying all three readings after each step.
		 */
		fr->frame[0] = (unsigned short)save0;
		fr->frame[1] = (unsigned short)save1;
		len = flen;
		flen = (short)(flen - 1);

		for (shift = 1; shift <= FAXVMI_MAX_BIT_SHIFT; shift++) {
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "Shifting data %d bits\n", shift);
			/*
			 * `len` is the length BEFORE the decrement above, so
			 * the last iteration reads one element past the
			 * octets that were stored -- D1074.
			 */
			for (i = 0; i < len; i++) {
				short nxt = (short)fr->frame[i + 1];

				fr->frame[i] = (unsigned short)
				    (((fr->frame[i] << 1) & 0xfe)
				     | ((nxt >> 7) & 1));
			}
			for (i = 0; i < flen; i++)
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf("%2x ",
							     fr->frame[i]);
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("\n");

			fcs = (unsigned short)
			    faxvmi_gen_fcs16(fr->frame, flen);
			if (fcs == FAXVMI_FCS16_GOOD) {
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "CRC is now OK!\n");
				break;
			}
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf("Bad CRC!\n");
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "Replacing first byte ...\n");
			save0 = (short)fr->frame[0];
			fr->frame[0] = FAXVMI_T30_ADDRESS;
			fcs = (unsigned short)
			    faxvmi_gen_fcs16(fr->frame, flen);
			if (fcs == FAXVMI_FCS16_GOOD) {
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "CRC is now OK!\n");
				break;
			}
			save1 = (short)fr->frame[1];
			if (dsplibs_debug_level > 1)
				dsplibs_debug_printf(
				    "Replacing the second byte ...\n");
			fr->frame[1] = FAXVMI_T30_CONTROL;
			fcs = (unsigned short)
			    faxvmi_gen_fcs16(fr->frame, flen);
			if (fcs == FAXVMI_FCS16_GOOD) {
				if (dsplibs_debug_level > 1)
					dsplibs_debug_printf(
					    "CRC is now OK!\n");
				break;
			}
			fr->frame[0] = (unsigned short)save0;
			fr->frame[1] = (unsigned short)save1;
		}
		/*
		 * Give up: emit the frame with a length of ZERO rather than
		 * suppress it, so the frame count still advances.
		 */
		if (fcs != FAXVMI_FCS16_GOOD)
			flen = 0;

	emit:
		if ((int)emitted + (int)flen >= (int)vmi->max_frame) {
			overflow = 1;
		} else {
			*dst++ = (unsigned short)flen;
			for (i = 0; i < flen; i++)
				*dst++ = fr->frame[i];
			/*
			 * D1073 again: the object's store here is
			 * `emitted = emitted`, so nothing accumulates.
			 */
			flen = 0;
			nframes = (short)(nframes + 1);
		}
		/*
		 * NOTE the asymmetry, and it is the object's: `flen` is reset
		 * only on the arm that EMITTED.  A frame refused for want of
		 * room leaves the assembly buffer where it is, so the next
		 * frame's octets follow it.  D1075.
		 */

	close_flag:
		fr->in_frame = 0;
		bit = 0;

	next_bit:
		if (b != 0)
			ones = (short)(ones + 1);
		else
			ones = 0;
	}

	fr->unpack_mask = mask;
	fr->unpack_bit = bit;
	fr->unpack_word = word;
	fr->frame_len = flen;
	fr->unpack_acc = acc;
	fr->ones = ones;
	vmi->overflow = overflow;
	return nframes;
}
