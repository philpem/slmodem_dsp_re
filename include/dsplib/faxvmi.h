/*
 * faxvmi.h -- Class 1 fax: the Virtual Modem Interface.
 *
 * FAXVMI wraps one modulation behind a numbered slot and puts a framing layer
 * in front of it.  Two dispatches, and they are indexed by DIFFERENT fields:
 *
 *   +0x0e  slot   -> the 13-entry `vxx_*` tables, one entry per modulation.
 *                    The slot map, read off vxx_message's relocations:
 *                    0..4 null, 5 v21tx, 6 v21rx, 7 v27tx, 8 v27rx,
 *                    9 v29tx, 10 v29rx, 11 v17tx, 12 v17rx.
 *   +0x00  mode   -> the 3-entry framing tables.  THESE THREE TABLES CARRY
 *                    THE AUTHOR'S OWN NAMES in the object's symbol table --
 *                    `vmi_pack` (.rodata 0x94b4), `vmi_unpack` (0x94a8) and
 *                    `vmi_reverse` (0x94c0) -- and their contents name the
 *                    three modes:
 *
 *      vmi_pack    = { faxvmi_simp_pack,   faxvmi_asyc_pack,   faxvmi_hdlc_frame   }
 *      vmi_unpack  = { faxvmi_simp_unpack, faxvmi_asyc_unpack, faxvmi_hdlc_unframe }
 *      vmi_reverse = { faxvmi_byte_reverse, faxvmi_byte_reverse, faxvmi_frame_reverse }
 *
 * so mode 0 is "simple", 1 is "async" and 2 is "HDLC", and `FAXVMI_control`
 * rejects anything above 2 (`cmpw $0x2,0x10(%ebx); ja`) before storing it.
 * That is evidence class 1 for the mode constants below: the author's words,
 * not an inference from what the code does.
 *
 * WHAT IS RECONSTRUCTED HERE, and what is not.  `FAXVMI_message` and the
 * three pure buffer walks came from the leaf pass; this batch adds the three
 * unpackers and the two ring writers.  `FAXVMI_create`, `_delete`,
 * `_process`, `_status`, `_control` and the three packers are NOT written --
 * but create, control, process and status are read below for the layout,
 * because they are what establishes the sizes and the initial values, and
 * every field's evidence is quoted with it.
 */

#ifndef DSPLIB_FAXVMI_H
#define DSPLIB_FAXVMI_H

/* --------------------------------------------------------------------- */

/*
 * The framing mode: `struct faxvmi`'s +0x00, and the index into the three
 * tables above.  Named from those tables' own contents.
 */
#define FAXVMI_MODE_SIMP	0
#define FAXVMI_MODE_ASYC	1
#define FAXVMI_MODE_HDLC	2

/*
 * THE FRAMER, `vmi->framer` (+0x24).  `FAXVMI_create` allocates it with
 * `sysdep_malloc(0x58)` at 0x953c0, so it is 88 bytes and the layout below is
 * closed rather than open-ended.
 *
 * It is ONE object carrying TWO unrelated things, which is why it looks odd:
 *
 *   +0x00..+0x0b   a ring of 16-bit elements that the PACK side fills
 *   +0x20..+0x3c   the bit-level unpacker's state
 *   +0x40..+0x57   the HDLC receiver's frame assembly
 *
 * The ring's capacity is `max(cfg->fifo_size, cfg->max_frame + 3)` --
 * FAXVMI_create at 0x9516b computes exactly that before
 * `sysdep_malloc(size * 2)` -- and the `+ 3` is `faxvmi_write_frame`'s own
 * per-frame overhead: one length element and two FCS elements.  The two
 * readings agree, which is what makes `max_frame` a name rather than a guess.
 *
 * Fields still spelled `type_NNNN` are modelled (shape and size known from
 * the loads and stores that touch them) and unnamed, per CLAUDE.md's four
 * states.  Their initial values from FAXVMI_create are quoted because that is
 * all that is known about them.
 */
struct faxvmi_framer {
	/* --- the ring the packers fill ------------------------------- */
	unsigned short *fifo;		/* +0x00 `size` elements, from
					 *       sysdep_malloc(size * 2)     */
	unsigned short fifo_size;	/* +0x04 capacity, in elements      */
	unsigned short short_0006;	/* +0x06 create and control zero it;
					 *       nothing reconstructed reads
					 *       it                          */
	unsigned short wr;		/* +0x08 write cursor, in elements  */
	unsigned short count;		/* +0x0a occupancy, in elements     */
	int int_000c;			/* +0x0c create zeroes it;
					 *       FAXVMI_status copies it to
					 *       the status record's +0x04 and
					 *       FAXVMI_process makes it bit
					 *       24+2 of the status word     */
	int int_0010;			/* +0x10 create: 0                  */
	int int_0014;			/* +0x14 create: -1                 */
	int int_0018;			/* +0x18 create: -1                 */
	short short_001c;		/* +0x1c create and control write a
					 * WORD of zero here and then read a
					 * DWORD back from it into +0x2c; see
					 * the note on `bit` below           */
	short short_001e;		/* +0x1e never written               */

	/* --- the bit-level unpacker ---------------------------------- */
	unsigned int mask;		/* +0x20 the bit being taken out of
					 * `word`, walking down from
					 * 1 << (link->width - 1); zero means
					 * "fetch the next element".  The
					 * field is 32 bits and every reader
					 * holds it in an `unsigned short`
					 * local, which is why the loads are
					 * movzwl and the stores are movl   */
	unsigned int word;		/* +0x24 the element being consumed */
	unsigned int acc;		/* +0x28 the bit accumulator.  It is
					 * 32 bits because the async framer
					 * tests 23 bits of history in it   */
	unsigned short bit;		/* +0x2c bits assembled into the
					 * current octet, 0..7             */
	unsigned short short_002e;	/* +0x2e create and control reach it
					 * only as the upper half of their
					 * 32-bit store; every reader of
					 * +0x2c is a movzwl, so it is dead */
	int async_hunt;			/* +0x30 async framing state, and
					 * create sets it to 1: nonzero
					 * means "waiting for a start bit",
					 * zero means "counting the eight
					 * data bits".  Usage inference from
					 * faxvmi_asyc_unpack, but its two
					 * arms are unambiguous            */
	unsigned short short_0034;	/* +0x34 FAXVMI_control copies the
					 * control record's +0x08 here      */
	unsigned short pad_0036;	/* +0x36                            */
	int int_0038;			/* +0x38 FAXVMI_control copies the
					 * control record's +0x04 here      */
	int zero_run_seen;		/* +0x3c set by faxvmi_asyc_unpack
					 * when the accumulator's low 23
					 * bits read 22 zeros then a one --
					 * that is, when a long run of zero
					 * bits has just ended.  Exported by
					 * FAXVMI_status (+0x10) and clears
					 * bit 28 of FAXVMI_process's status
					 * word.  The name says what it
					 * detects; calling it a BREAK would
					 * be a step past the evidence      */

	/* --- the HDLC receiver --------------------------------------- */
	unsigned short *frame;		/* +0x40 assembly buffer,
					 * sysdep_malloc(cfg->frame_size * 2)
					 * -- one octet per 16-bit element  */
	unsigned short frame_size;	/* +0x44 its capacity, in elements  */
	unsigned short short_0046;	/* +0x46 create and control zero it */
	short frame_len;		/* +0x48 octets assembled so far    */
	unsigned short flags_wanted;	/* +0x4a opening flags still to be
					 * seen before octets are kept;
					 * create and control both set it to
					 * 2 and each closing flag decrements
					 * it while it is nonzero           */
	int int_004c;			/* +0x4c create and control: 1      */
	short ones;			/* +0x50 the run of consecutive one
					 * bits: 5 destuffs the next zero,
					 * 6 is a flag                     */
	short pad_0052;			/* +0x52                            */
	int in_frame;			/* +0x54 set when an octet is stored
					 * into `frame`, cleared at every
					 * flag.  A closing flag with this
					 * set is a complete frame          */
};

/*
 * THE LINK BLOCK, `vmi->link` (+0x28).  `sysdep_malloc(0x18)` at 0x953d4, so
 * 24 bytes, and it is the handle every `vxx_*` entry point receives:
 * `FAXVMI_create` calls `vxx_create[slot](vmi->link, cfg->int_0010)` and
 * message, status and control all pass it as their first argument.
 *
 * Only two fields are established here, and they are established by being
 * READ: all three unpackers take their input elements from `rx` and their
 * element width from `width`.  The rest is what FAXVMI_create does to it.
 */
struct faxvmi_link {
	unsigned short *ptr_0000;	/* +0x00 sysdep_malloc(0x190) --
					 * 200 elements, which create zeroes */
	unsigned short *rx;		/* +0x04 sysdep_malloc(0x64) -- 50
					 * elements, which create fills with
					 * 0xffff.  The unpackers' input     */
	unsigned char pad_0008[8];	/* +0x08                             */
	unsigned short width;		/* +0x10 significant bits per element:
					 * every unpacker starts its mask at
					 * 1 << (width - 1)                  */
	unsigned short pad_0012;	/* +0x12                             */
	int int_0014;			/* +0x14 create zeroes it            */
};

/*
 * `struct faxvmi` itself: `sysdep_malloc(0x2c)` at 0x953ab, so 44 bytes.
 *
 * +0x00..+0x17 is the CONFIGURATION, copied in one six-dword block from the
 * caller's argument or, when that is null, from `FAXVMI_CFG` (.rodata 0x9490,
 * 0x18 bytes).  +0x18 onwards is runtime state.
 */
struct faxvmi {
	unsigned short mode;		/* +0x00 FAXVMI_MODE_*               */
	unsigned short pad_0002;	/* +0x02                             */
	int reverse;			/* +0x04 nonzero routes the block
					 * through `vmi_reverse[mode]` on the
					 * way in and on the way out.  Named
					 * from the table it selects         */
	unsigned short fifo_size;	/* +0x08 the ring capacity asked for */
	unsigned short max_frame;	/* +0x0a the longest frame, in
					 * elements.  Four independent
					 * readings agree: the ring is sized
					 * to hold it plus three, the three
					 * unpackers stop producing at it,
					 * and FAXVMI_process reports "full"
					 * when the ring's free space falls
					 * below it                          */
	unsigned short frame_size;	/* +0x0c sizes the framer's HDLC
					 * assembly buffer                   */
	short slot;			/* +0x0e index into the vxx tables   */
	int int_0010;			/* +0x10 handed to vxx_create as its
					 * second argument                   */
	int int_0014;			/* +0x14 copied by create, read by
					 * nothing reconstructed             */
	int int_0018;			/* +0x18 create sets it to 1;
					 * faxvmi_write_fifo and
					 * faxvmi_write_frame clear it when a
					 * whole request is accepted;
					 * FAXVMI_status passes it to
					 * vxx_status as the request and only
					 * calls that entry point while it is
					 * nonzero; FAXVMI_process makes it
					 * bit 24 of the status word.  Four
					 * sites, no format string, and no
					 * reading of them that is more than
					 * a guess -- so it keeps a neutral
					 * name (CLAUDE.md: a wrong name is
					 * worse than a padded one)          */
	int overflow;			/* +0x1c set to 1 by all three
					 * unpackers, and only by them, on
					 * the one condition that the
					 * destination has taken max_frame
					 * elements already.  Status reports
					 * it at +0x0c and process makes it
					 * bit 27                            */
	int status;			/* +0x20 the last word FAXVMI_process
					 * composed and returned             */
	struct faxvmi_framer *framer;	/* +0x24                             */
	struct faxvmi_link *link;	/* +0x28 the vxx_* handle            */
};

/*
 * The dispatch contract every vxx_* table entry follows: (handle, code,
 * out-parameter).  The message form answers a string or NULL.
 */
typedef void (*faxvmi_message_fn)(void *handle, int code, char **out);

/* 13 slots; `.rodata` in the object, so `const` here. */
extern faxvmi_message_fn const vxx_message[13];

/*
 * Ask the wrapped modem for `code`'s message string.  NULL for a code the
 * modulation does not name.  `code` really is an unsigned char in the
 * object -- it is loaded with movzbl -- where the table entries take an
 * int; the narrowing is the author's.
 */
char *FAXVMI_message(struct faxvmi *vmi, unsigned char code);

/* --------------------------------------------------------------------- */
/* The unpackers: `vmi_unpack[mode]`.                                     */

/*
 * ALL THREE HAVE THE SAME CONTRACT.  `count` input elements are taken from
 * `vmi->link->rx` -- NOT from a caller's buffer -- and octets are written to
 * `dst`, one octet per 16-bit element, low byte only.  The return is how many
 * elements were written to `dst`, and it is a short widened to int.
 *
 * THE INPUT CURSOR IS NEVER WRITTEN BACK.  Each call re-reads
 * `vmi->link->rx` and walks a LOCAL copy of it; the advanced pointer is
 * discarded at the return.  All three do this, so it is the interface and not
 * an oversight: the caller re-presents the buffer, and the bit position
 * within an element is what the framer carries across calls (`mask`).
 *
 * SIZE `dst` FROM `count`, NOT FROM THE RETURN.  The simple and async forms
 * stop at `vmi->max_frame` octets and set `vmi->overflow`, so `dst` needs
 * `max_frame` elements; the HDLC form's guard does not accumulate across the
 * frames of one call (D1073) and can write far more.  This is F8607/D956's
 * shape and it is called out rather than discovered again.
 */

/*
 * Simple: eight consecutive bits are an octet, no framing at all.  The bit
 * order is MSB first -- the first bit taken ends up in bit 7.
 */
int faxvmi_simp_unpack(struct faxvmi *vmi, unsigned short *dst, short count);

/*
 * Asynchronous: a start bit then eight data bits, the start bit found by
 * hunting.  While `framer->async_hunt` is set every one bit is mark and the
 * first zero begins a character; the eight bits after it are the character
 * and the framer goes back to hunting.
 *
 * TWO THINGS RUN ON EVERY BIT, whatever the state, and both test the low 23
 * bits of the accumulator: 23 zeros restart the hunt with the bit counter
 * cleared, and 22 zeros followed by a one set `framer->zero_run_seen`.
 */
int faxvmi_asyc_unpack(struct faxvmi *vmi, unsigned short *dst, short count);

/*
 * HDLC: destuffing, flags, the frame check sequence, and a repair pass.
 * Frames are written to `dst` length-prefixed -- one element of length, then
 * that many octets -- which is `faxvmi_frame_reverse`'s layout.
 *
 * A frame whose FCS never comes right is emitted with a length of ZERO rather
 * than dropped, so the frame count still advances.
 */
int faxvmi_hdlc_unframe(struct faxvmi *vmi, unsigned short *dst, short count);

/*
 * The residue a correct HDLC frame leaves.  `faxvmi_gen_fcs16` complements on
 * the way out, so this is the complement of 0x1D0F -- the standard residue of
 * CRC-CCITT taken MSB-first from 0xFFFF over the data AND the two FCS octets.
 * Derived, not matched: 0xE2F0 = ~0x1D0F.
 */
#define FAXVMI_FCS16_GOOD	0xe2f0

/*
 * THE T.30 HEADER OCTETS the repair pass forces, and the author's own words
 * for what it is doing are in `faxvmi.c` beside them: "Replacing the first
 * byte...", "Replacing the second byte...".  0xff is HDLC's all-stations
 * address and 0xc8 the control octet, both bit-reversed as the frame buffer
 * holds them.
 */
#define FAXVMI_T30_ADDRESS	0xff
#define FAXVMI_T30_CONTROL	0xc8

/* The repair pass tries this many one-bit realignments, 1 through 7. */
#define FAXVMI_MAX_BIT_SHIFT	7

/* --------------------------------------------------------------------- */
/* The ring writers, which the packers call.                              */

/*
 * `*src` is a cursor the callee ADVANCES -- both writers take the pointer by
 * reference and store the new position back before returning, on every path.
 *
 * `faxvmi_write_fifo` copies up to `count` elements one for one and returns
 * how many the ring took.  `faxvmi_write_frame` takes `count` LENGTH-PREFIXED
 * frames, appending to the ring a length element of `len + 2`, the data, and
 * the two FCS octets; it returns how many frames it wrote.
 *
 * BOTH CLEAR `vmi->int_0018`, AND NEITHER DOES IT ON THE RING-FULL PATH.  The
 * object holds the value in a register across the copy loop and stores it at
 * the loop's normal exit, so a run that fills the ring part-way leaves the
 * field alone.  That is not a spelling detail: it is the difference between
 * "the field is zero because something was written" and "the field is zero
 * because the whole request was taken", and only the second is the object's.
 * D1070.
 */
int faxvmi_write_fifo(struct faxvmi *vmi, unsigned short **src, short count);
int faxvmi_write_frame(struct faxvmi *vmi, unsigned short **src, short count);

/*
 * THE FRAMING LAYER STORES ONE OCTET PER 16-BIT ELEMENT.  Every buffer these
 * three walk is `unsigned short *` with a stride of 2, and every load is a
 * `movzwl`; only the low eight bits of an element carry data.
 */

/*
 * The HDLC frame check sequence over `count` elements.
 *
 * THE GENERATOR IS 0x1021 -- x^16 + x^12 + x^5 + 1, the CRC-CCITT/T.30
 * polynomial -- with the register initialised to 0xFFFF, fed MOST significant
 * nibble first, and COMPLEMENTED on the way out.  That is read off the
 * instructions and not guessed: each of the two steps per element computes
 * n = (fcs ^ (octet << k)) >> 12 and then
 *
 *     fcs = (fcs << 4) ^ (n << 12) ^ (n << 5) ^ n
 *
 * and (n << 12) ^ (n << 5) ^ n is exactly n * 0x1021 with the x^16 term
 * dropped.  k is 8 for the high nibble and 12 for the low one, so the two
 * steps consume bits 7..4 then 3..0 of the element.  There is no reflection
 * anywhere in here; the bit order HDLC wants is arranged OUTSIDE, by
 * faxvmi_byte_reverse.
 */
int faxvmi_gen_fcs16(unsigned short *buf, short count);

/* Reverse the low eight bits of each of `count` elements, in place. */
void faxvmi_byte_reverse(unsigned short *buf, short count);

/*
 * `count` length-prefixed frames laid end to end -- one element of length,
 * then that many data elements -- each of which has faxvmi_byte_reverse run
 * over its data.  The length element itself is NOT reversed.
 */
void faxvmi_frame_reverse(unsigned short *buf, short count);

#endif /* DSPLIB_FAXVMI_H */
