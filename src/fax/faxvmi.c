/*
 * faxvmi.c -- Class 1 fax: the VMI's framing layer.
 *
 * Reconstructed from dsplibs.o's class1tx.c +94 span (Faxvmi cluster,
 * 0x095120..0x096bad).  Written here, in the object's own emission order:
 *
 *   FAXVMI_message        .text 0x0957b0     55
 *   faxvmi_asyc_pack      .text 0x0957f0    436
 *   faxvmi_asyc_unpack    .text 0x0959b0    349
 *   faxvmi_hdlc_unframe   .text 0x095eb0   1577
 *   faxvmi_simp_pack      .text 0x0964e0    401
 *   faxvmi_simp_unpack    .text 0x096680    243
 *   faxvmi_gen_fcs16      .text 0x096780    108
 *   faxvmi_byte_reverse   .text 0x0967f0     93
 *   faxvmi_frame_reverse  .text 0x096850    151
 *   faxvmi_write_fifo     .text 0x0968f0    157
 *   faxvmi_write_frame    .text 0x096990    542
 *   vxx_message           .rodata 0x94e0     52  (13 slots)
 *
 * NOT written, and read only as evidence: FAXVMI_create (0x095120),
 * _delete (0x0953f0), _process (0x095470), _status (0x0955c0), _control
 * (0x095650) and the third packer faxvmi_hdlc_frame (0x095b10).  The three
 * dispatch tables `vmi_pack`, `vmi_unpack` and `vmi_reverse` belong to that
 * half and are not written either -- `vmi_pack`'s third entry is
 * `faxvmi_hdlc_frame`, and naming a table entry that does not exist yet is
 * the link constraint, F8492/F8493.  So the table waits on ONE symbol now.
 *
 * THE FUNCTIONS HERE ARE ONE UNIT, and that is why they came together: every
 * one of them goes through `vmi->framer`, a single 88-byte object that is a
 * ring, two bit engines and an HDLC receiver at once.  Half-modelling it
 * would have put a guessed layout under all of them.  See `faxvmi.h` for the
 * model and the evidence per field.
 *
 * THE PACKERS ARE NOT THE UNPACKERS RUN BACKWARDS, and the shapes that differ
 * are worth stating once here rather than twice below:
 *
 *   - the caller's buffer is not read by the packer at all.  It goes to
 *     `faxvmi_write_fifo`, which is called TWICE -- once before the bit loop
 *     and once after -- and the ring is the only path between them.
 *   - the output length is `link->pack_count` and nothing else.  A starved
 *     ring is padded with fill, never short-blocked.
 *   - the bit engine is the framer's SECOND quartet, +0x10..+0x1c, whose
 *     four fields mirror the unpackers' +0x20..+0x2c one for one.
 */

#include <stddef.h>

#include "dsplib/class1tx.h"
#include "dsplib/debug.h"
#include "dsplib/faxvmi.h"

/*
 * The three sizes are not inferred: FAXVMI_create asks `sysdep_malloc` for
 * 0x2c, 0x58 and 0x18 at 0x953ab, 0x953c0 and 0x953d4.  The offsets are the
 * loads and stores quoted in faxvmi.h.  Under GCC 3.4.2 this whole block is
 * `#if 0` -- `__SIZEOF_POINTER__` is a 4.6 predefine -- which is the tree's
 * known and documented state; see docs/method/compilers.md.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4

#define VMI_ASSERT_OFF(field, off) \
	typedef char faxvmi_off_##field[ \
		((int)__builtin_offsetof(struct faxvmi, field) == (off)) \
			? 1 : -1]
#define FRAMER_ASSERT_OFF(field, off) \
	typedef char faxvmi_framer_off_##field[ \
		((int)__builtin_offsetof(struct faxvmi_framer, field) \
		 == (off)) ? 1 : -1]
#define LINK_ASSERT_OFF(field, off) \
	typedef char faxvmi_link_off_##field[ \
		((int)__builtin_offsetof(struct faxvmi_link, field) == (off)) \
			? 1 : -1]

VMI_ASSERT_OFF(mode, 0x00);
VMI_ASSERT_OFF(reverse, 0x04);
VMI_ASSERT_OFF(fifo_size, 0x08);
VMI_ASSERT_OFF(max_frame, 0x0a);
VMI_ASSERT_OFF(frame_size, 0x0c);
VMI_ASSERT_OFF(slot, 0x0e);
VMI_ASSERT_OFF(underrun, 0x18);
VMI_ASSERT_OFF(overflow, 0x1c);
VMI_ASSERT_OFF(status, 0x20);
VMI_ASSERT_OFF(framer, 0x24);
VMI_ASSERT_OFF(link, 0x28);
typedef char faxvmi_size[(sizeof(struct faxvmi) == 0x2c) ? 1 : -1];

FRAMER_ASSERT_OFF(fifo, 0x00);
FRAMER_ASSERT_OFF(fifo_size, 0x04);
FRAMER_ASSERT_OFF(rd, 0x06);
FRAMER_ASSERT_OFF(wr, 0x08);
FRAMER_ASSERT_OFF(count, 0x0a);
FRAMER_ASSERT_OFF(residue, 0x0c);
FRAMER_ASSERT_OFF(pack_mask, 0x10);
FRAMER_ASSERT_OFF(pack_word, 0x14);
FRAMER_ASSERT_OFF(pack_acc, 0x18);
FRAMER_ASSERT_OFF(pack_bit, 0x1c);
FRAMER_ASSERT_OFF(unpack_mask, 0x20);
FRAMER_ASSERT_OFF(unpack_word, 0x24);
FRAMER_ASSERT_OFF(unpack_acc, 0x28);
FRAMER_ASSERT_OFF(unpack_bit, 0x2c);
FRAMER_ASSERT_OFF(async_hunt, 0x30);
FRAMER_ASSERT_OFF(zero_run_bits, 0x34);
FRAMER_ASSERT_OFF(zero_run_send, 0x38);
FRAMER_ASSERT_OFF(zero_run_seen, 0x3c);
FRAMER_ASSERT_OFF(frame, 0x40);
FRAMER_ASSERT_OFF(frame_size, 0x44);
FRAMER_ASSERT_OFF(frame_len, 0x48);
FRAMER_ASSERT_OFF(flags_wanted, 0x4a);
FRAMER_ASSERT_OFF(int_004c, 0x4c);
FRAMER_ASSERT_OFF(ones, 0x50);
FRAMER_ASSERT_OFF(in_frame, 0x54);
typedef char faxvmi_framer_size[(sizeof(struct faxvmi_framer) == 0x58)
				? 1 : -1];

LINK_ASSERT_OFF(ptr_0000, 0x00);
LINK_ASSERT_OFF(buf, 0x04);
LINK_ASSERT_OFF(pack_count, 0x0c);
LINK_ASSERT_OFF(pack_width, 0x0e);
LINK_ASSERT_OFF(unpack_width, 0x10);
LINK_ASSERT_OFF(int_0014, 0x14);
typedef char faxvmi_link_size[(sizeof(struct faxvmi_link) == 0x18) ? 1 : -1];

#endif

faxvmi_message_fn const vxx_message[13] = {
	null_message,
	null_message,
	null_message,
	null_message,
	null_message,
	v21tx_message,
	v21rx_message,
	v27tx_message,
	v27rx_message,
	v29tx_message,
	v29rx_message,
	v17tx_message,
	v17rx_message,
};

/*
 * The out-parameter is initialised to NULL BEFORE the dispatch, so a slot
 * whose reporter wrote nothing would still answer NULL -- none of the
 * thirteen leaves it unwritten, but the belt is the object's and is kept.
 * There is NO guard on the slot: a stale index dispatches through whatever
 * follows the table, there as here.
 */
char *
FAXVMI_message(struct faxvmi *vmi, unsigned char code)
{
	char *msg = NULL;

	vxx_message[(unsigned short)vmi->slot](vmi->link, code, &msg);
	return msg;
}

/*
 * Asynchronous framing on the way OUT: each octet from the ring becomes a
 * ten-bit character.
 *
 *	word = ((octet << 1) | 1) & ~0x200,	mask = 0x200
 *
 * so the first bit shifted out is bit 9, forced to ZERO by the AND -- the
 * start bit; then the octet's eight bits, most significant first; then bit 0,
 * set by the OR -- the stop bit.  The `& ~0x200` is a separate instruction in
 * the object (0x95948) and is written as one here.
 *
 * THE REFILL HAS FOUR ARMS AND ONLY TWO OF THEM SET A MASK OF 1.  A zero-bit
 * run in progress emits one zero bit; the run's LAST refill clears
 * `zero_run_send`, sets `pack_word` to 1 and LEAVES THE MASK AT ZERO, so the
 * bit it emits is a space rather than the mark the assignment reads as --
 * D1120, reproduced.  An empty ring emits a single mark bit and raises
 * `vmi->underrun`.
 *
 * `pack_count` output elements are always produced, and that count is what is
 * returned -- re-read from the link object after the trailing write, not from
 * the local the loop counted down.
 */
int
faxvmi_asyc_pack(struct faxvmi *vmi, unsigned short *src, short count)
{
	struct faxvmi_framer *fr;
	struct faxvmi_link *lk;
	unsigned short *out;
	unsigned int word, acc, elem_mask;
	unsigned short mask, bit, width;
	short n, left;

	left = (short)(count - faxvmi_write_fifo(vmi, &src, count));

	lk = vmi->link;
	fr = vmi->framer;
	out = lk->buf;
	word = fr->pack_word;
	mask = (unsigned short)fr->pack_mask;
	n = lk->pack_count;
	width = lk->pack_width;
	bit = fr->pack_bit;
	acc = fr->pack_acc;
	elem_mask = (1u << width) - 1;

	while (n != 0) {
		unsigned int b;

		if (mask == 0) {
			if (fr->zero_run_send != 0) {
				if (fr->zero_run_bits != 0) {
					fr->zero_run_bits = (unsigned short)
					    (fr->zero_run_bits - 1);
					word = 0;
					mask = 1;
				} else {
					/* D1120: no mask, so a space */
					fr->zero_run_send = 0;
					word = 1;
				}
			} else if (fr->count != 0) {
				unsigned short rd = fr->rd;
				unsigned int v = fr->fifo[rd];

				fr->rd = (unsigned short)(rd + 1);
				fr->count = (unsigned short)(fr->count - 1);
				if (fr->rd >= fr->fifo_size)
					fr->rd = 0;
				word = ((v << 1) | 1) & 0xfffffdffu;
				mask = 0x200;
			} else {
				word = 1;
				mask = 1;
				vmi->underrun = 1;
			}
		}
		b = (word & mask) != 0;
		acc = (acc << 1) | b;
		bit = (unsigned short)(bit + 1);
		mask = (unsigned short)(mask >> 1);
		if (bit == width) {
			*out++ = (unsigned short)(acc & elem_mask);
			n = (short)(n - 1);
			bit = 0;
		}
	}

	fr->pack_bit = bit;
	fr->pack_mask = mask;
	fr->pack_acc = acc;
	fr->pack_word = word;

	left = (short)(left - faxvmi_write_fifo(vmi, &src, left));
	vmi->framer->residue = left;
	return vmi->link->pack_count;
}

/*
 * ============================ the unpackers ============================
 *
 * All three share a preamble and a postamble, and they are written out in
 * full in each rather than factored, because the object has them in full in
 * each: there is no shared helper in the disassembly and inventing one would
 * move three functions' code generation at once.
 *
 * The preamble takes the framer's bit position into locals; the postamble
 * puts it back.  `vmi->link->buf` is re-read every call and the advanced
 * cursor is DISCARDED -- see faxvmi.h.
 */

/*
 * Asynchronous framing: hunt for a start bit, then take eight data bits.
 *
 * `hunt` is the object's +0x30, which FAXVMI_create sets to 1.  While it is
 * set, each bit REPLACES it -- so a one keeps hunting and the first zero (the
 * start bit) clears it and the eight bits after that are the character.
 *
 * The two tests on `acc & 0x7fffff` run on every bit in both states.  23
 * zeros in a row means the line has gone hard down: the character in progress
 * is abandoned and the hunt restarts.  The first one bit after such a run --
 * the accumulator reading 22 zeros then a one, which is exactly the value 1 --
 * latches `framer->zero_run_seen`, which is a level the VMI exports and not
 * something this function acts on.
 */
int
faxvmi_asyc_unpack(struct faxvmi *vmi, unsigned short *dst, short count)
{
	struct faxvmi_framer *fr = vmi->framer;
	struct faxvmi_link *lk = vmi->link;
	unsigned int word = fr->unpack_word;
	unsigned short bit = fr->unpack_bit;
	unsigned short mask = fr->unpack_mask;
	unsigned int acc = fr->unpack_acc;
	unsigned short *src = lk->buf;
	unsigned int top = 1u << (lk->unpack_width - 1);
	int hunt = fr->async_hunt;
	int zero_run = 0;
	int overflow = 0;
	short nout = 0;
	short left = count;

	while (left != 0) {
		unsigned int b;

		if (mask == 0) {
			mask = (unsigned short)top;
			word = *src++;
			left = (short)(left - 1);
		}
		b = (word & mask) != 0;
		acc = (acc << 1) | b;
		mask = (unsigned short)(mask >> 1);

		if (hunt != 0) {
			hunt = (int)b;
		} else {
			bit = (unsigned short)(bit + 1);
			if (bit == 8) {
				bit = 0;
				hunt = 1;
				if ((int)nout >= (int)vmi->max_frame)
					overflow = 1;
				else {
					*dst++ = (unsigned short)(acc & 0xff);
					nout = (short)(nout + 1);
				}
			}
		}

		if ((acc & 0x7fffff) == 0) {
			bit = 0;
			hunt = 1;
			continue;
		}
		if ((acc & 0x7fffff) == 1)
			zero_run = 1;
	}

	fr->async_hunt = hunt;
	fr->unpack_mask = mask;
	fr->unpack_acc = acc;
	fr->zero_run_seen = zero_run;
	fr->unpack_word = word;
	fr->unpack_bit = bit;
	vmi->overflow = overflow;
	return nout;
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

/*
 * Simple framing on the way OUT: no framing.  Each ring element is shifted
 * out from bit 7 down, so the octet's most significant bit goes first, and an
 * empty ring contributes a zero octet.
 *
 * `real` IS NOT A SPELLING CHOICE.  The object holds a 0/1 value in a slot,
 * initialises it to 1, sets it to zero on the ring-empty arm ALONE, and ADDS
 * it to the running count at every emitted element (0x964fb, 0x96589,
 * 0x965c5).  So the return counts the elements produced before the ring first
 * ran dry and stops advancing for the rest of the call, while the block
 * itself is still filled to `pack_count`.  A `nout++` guarded by a test would
 * compile to a branch, which is not what is there; the two also disagree
 * whenever the ring refills after a gap, which the object never lets happen
 * because the flag is never set back to 1.
 */
int
faxvmi_simp_pack(struct faxvmi *vmi, unsigned short *src, short count)
{
	struct faxvmi_framer *fr;
	struct faxvmi_link *lk;
	unsigned short *out;
	unsigned int word, acc, elem_mask;
	unsigned short mask, bit, width;
	short n, nout = 0, left;
	short real = 1;

	left = (short)(count - faxvmi_write_fifo(vmi, &src, count));

	fr = vmi->framer;
	lk = vmi->link;
	mask = (unsigned short)fr->pack_mask;
	out = lk->buf;
	acc = fr->pack_acc;
	n = lk->pack_count;
	width = lk->pack_width;
	word = fr->pack_word;
	bit = fr->pack_bit;
	elem_mask = (1u << width) - 1;

	while (n != 0) {
		unsigned int b;

		if (mask == 0) {
			if (fr->count != 0) {
				unsigned short rd = fr->rd;

				word = fr->fifo[rd];
				fr->rd = (unsigned short)(rd + 1);
				fr->count = (unsigned short)(fr->count - 1);
				if (fr->rd >= fr->fifo_size)
					fr->rd = 0;
			} else {
				word = 0;
				real = 0;
				vmi->underrun = 1;
			}
			mask = 0x80;
		}
		b = (word & mask) != 0;
		acc = (acc << 1) | b;
		bit = (unsigned short)(bit + 1);
		mask = (unsigned short)(mask >> 1);
		if (bit == width) {
			nout = (short)(nout + real);
			*out++ = (unsigned short)(acc & elem_mask);
			bit = 0;
			n = (short)(n - 1);
		}
	}

	fr->pack_mask = mask;
	fr->pack_acc = acc;
	fr->pack_bit = bit;
	fr->pack_word = word;

	left = (short)(left - faxvmi_write_fifo(vmi, &src, left));
	vmi->framer->residue = left;
	return nout;
}

/*
 * Simple framing: no framing.  Eight bits are an octet, first bit taken into
 * bit 7, and the only thing that can go wrong is the destination filling up.
 */
int
faxvmi_simp_unpack(struct faxvmi *vmi, unsigned short *dst, short count)
{
	struct faxvmi_framer *fr = vmi->framer;
	struct faxvmi_link *lk = vmi->link;
	unsigned short mask = fr->unpack_mask;
	unsigned short *src = lk->buf;
	unsigned int word = fr->unpack_word;
	unsigned short bit = fr->unpack_bit;
	unsigned int top = 1u << (lk->unpack_width - 1);
	unsigned int acc = fr->unpack_acc;
	int overflow = 0;
	short nout = 0;
	short left = count;

	while (left != 0) {
		unsigned int b;

		if (mask == 0) {
			mask = (unsigned short)top;
			word = *src++;
			left = (short)(left - 1);
		}
		b = (word & mask) != 0;
		acc = (acc << 1) | b;
		mask = (unsigned short)(mask >> 1);
		bit = (unsigned short)(bit + 1);
		if (bit != 8)
			continue;
		bit = 0;
		if ((int)nout >= (int)vmi->max_frame) {
			overflow = 1;
			continue;
		}
		*dst++ = (unsigned short)(acc & 0xff);
		nout = (short)(nout + 1);
	}

	fr->unpack_mask = mask;
	fr->unpack_bit = bit;
	fr->unpack_word = word;
	fr->unpack_acc = acc;
	vmi->overflow = overflow;
	return nout;
}

/*
 * The framing layer's three pure buffer walks.
 *
 * All three count DOWN from `count` and stop at zero, so a negative count
 * runs 65536 + count times before the 16-bit counter reaches zero rather
 * than not running at all.  That is the object's and is reproduced; D1052.
 */

/*
 * Two nibble steps per element, high nibble first.  Written as the object
 * computes it: `t` is the polynomial multiplicand already shifted into bits
 * 15..12, so `t >> 11` is the x^5 term, `t >> 12` is the x^0 term, and `t`
 * itself is the x^12 one.  See faxvmi.h for the derivation of 0x1021.
 */
#define FCS16_NIBBLE(fcs, shifted)					\
	do {								\
		unsigned int t_ = ((shifted) ^ (fcs)) & 0xf000u;	\
		(fcs) = (unsigned short)					\
			(((((fcs) ^ (t_ >> 11)) << 4) ^ t_) | (t_ >> 12)); \
	} while (0)

int
faxvmi_gen_fcs16(unsigned short *buf, short count)
{
	unsigned short fcs = 0xffff;
	short i;

	for (i = count; i != 0; i--) {
		unsigned int octet = *buf++;

		FCS16_NIBBLE(fcs, octet << 8);
		FCS16_NIBBLE(fcs, octet << 12);
	}
	return (unsigned short)~fcs;
}

void
faxvmi_byte_reverse(unsigned short *buf, short count)
{
	short i;

	for (i = count; i != 0; i--) {
		unsigned short in = *buf;
		unsigned short out = 0;
		short bit;

		for (bit = 7; bit >= 0; bit--) {
			out = (unsigned short)((out << 1) | (in & 1));
			in >>= 1;
		}
		*buf++ = out;
	}
}

/*
 * The inner walk is faxvmi_byte_reverse's, which the object inlines here
 * while still emitting the standalone copy -- F605's inlining boundary, so a
 * per-function byte comparison will read this function long and that one
 * short.  Written as the call it is.
 */
void
faxvmi_frame_reverse(unsigned short *buf, short count)
{
	short i;

	for (i = count; i != 0; i--) {
		short len = (short)*buf++;

		faxvmi_byte_reverse(buf, len);
		buf += len;
	}
}

/*
 * ========================== the ring writers ==========================
 *
 * THE `taken` FLAG IS NOT A SPELLING CHOICE.  The object loads
 * `vmi->underrun` into a register BEFORE the loop, sets that register to zero
 * inside it, and stores it at the loop's NORMAL exit -- 0x968f6 loads,
 * 0x96961 zeroes, 0x96978 stores -- while the ring-full path returns without
 * passing the store at all.  So the field is cleared when the whole request
 * was accepted, left ALONE when the ring filled part-way, and re-stored
 * unchanged when the count was zero.  A `vmi->underrun = 0;` inside the loop
 * body would agree on the first case and disagree on the second, which is a
 * behavioural difference and not a codegen one.  D1070.
 */

int
faxvmi_write_fifo(struct faxvmi *vmi, unsigned short **src, short count)
{
	struct faxvmi_framer *fr = vmi->framer;
	unsigned short *p = *src;
	int taken = vmi->underrun;
	short n = 0;
	short i;

	for (i = count; i != 0; i--) {
		if (fr->count >= fr->fifo_size) {
			*src = p;
			return n;
		}
		fr->fifo[fr->wr] = *p++;
		fr->wr = (unsigned short)(fr->wr + 1);
		if (fr->wr >= fr->fifo_size)
			fr->wr = 0;
		fr->count = (unsigned short)(fr->count + 1);
		n = (short)(n + 1);
		taken = 0;
	}

	*src = p;
	vmi->underrun = taken;
	return n;
}

/*
 * One frame per iteration, each `*p` elements long with the length in front
 * of it.  Three elements of overhead go into the ring for every frame -- the
 * length and the two FCS octets -- which is where the ring's `+ 3` headroom
 * in FAXVMI_create comes from, and the fit test asks for room for all of them
 * before anything is written.
 *
 * The two FCS elements are appended WITHOUT a fullness test and WITHOUT
 * touching the occupancy, because the occupancy was raised by three up front.
 * If the copy loop still manages to fill the ring -- which it can, since the
 * fit test is signed and a negative length passes it -- the FCS is written
 * over the ring's oldest elements anyway.  That is the object's; D1071.
 */
int
faxvmi_write_frame(struct faxvmi *vmi, unsigned short **src, short count)
{
	struct faxvmi_framer *fr = vmi->framer;
	unsigned short *p = *src;
	short written = 0;
	short i;

	for (i = count; i != 0; i--) {
		short len = (short)*p;
		unsigned short fcs;
		int taken;
		short j;

		if ((int)fr->count + (int)len + 3 >= (int)fr->fifo_size)
			break;

		fr->fifo[fr->wr] = (unsigned short)(len + 2);
		fr->wr = (unsigned short)(fr->wr + 1);
		if (fr->wr >= fr->fifo_size)
			fr->wr = 0;
		fr->count = (unsigned short)(fr->count + 3);
		written = (short)(written + 1);
		p++;

		fcs = (unsigned short)faxvmi_gen_fcs16(p, len);

		taken = vmi->underrun;
		for (j = len; j != 0; j--) {
			if (fr->count >= fr->fifo_size)
				goto put_fcs;
			fr->fifo[fr->wr] = *p++;
			fr->wr = (unsigned short)(fr->wr + 1);
			if (fr->wr >= fr->fifo_size)
				fr->wr = 0;
			fr->count = (unsigned short)(fr->count + 1);
			taken = 0;
		}
		vmi->underrun = taken;

	put_fcs:
		fr->fifo[fr->wr] = (unsigned short)(fcs >> 8);
		fr->wr = (unsigned short)(fr->wr + 1);
		if (fr->wr >= fr->fifo_size)
			fr->wr = 0;
		fr->fifo[fr->wr] = (unsigned short)(fcs & 0xff);
		fr->wr = (unsigned short)(fr->wr + 1);
		if (fr->wr >= fr->fifo_size)
			fr->wr = 0;
	}

	*src = p;
	return written;
}
