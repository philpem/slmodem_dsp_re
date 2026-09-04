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
 * three pure buffer walks came from the leaf pass; the batch after it added
 * the three unpackers and the two ring writers; this one adds
 * `faxvmi_simp_pack` and `faxvmi_asyc_pack`.  `FAXVMI_create`, `_delete`,
 * `_process`, `_status`, `_control` and the third packer `faxvmi_hdlc_frame`
 * are NOT written -- but create, control, process and status are read below
 * for the layout, because they are what establishes the sizes and the initial
 * values, and every field's evidence is quoted with it.
 *
 * THE PACK SIDE IS THE UNPACK SIDE'S MIRROR, and that is a structural fact
 * rather than a resemblance (F9190).  `framer` carries TWO bit engines of
 * four fields each, at +0x10..+0x1c and +0x20..+0x2c, and `FAXVMI_create`
 * initialises the two identically -- mask 0, word -1, acc -1, bit 0 at
 * 0x951d7..0x95201.  The packers use the first and the unpackers the second,
 * so they are spelled `pack_*` and `unpack_*` here.
 */

/*
 * RECONCILING THIS WITH `faxcfg.h`, WHICH ARRIVED ON ANOTHER BRANCH OF THE
 * SAME WAVE.  `struct faxvmi_cfg` there is the 24 bytes of `FAXVMI_CFG` that
 * the `init_vmi_*` constructors copy over the HEAD of a `struct faxvmi`, so
 * it is this structure's leading sub-object and the two describe the same
 * memory.  They AGREE on every width, and neither needs changing to be
 * correct; what the two halves have between them is more evidence than either
 * had alone, so whoever unifies them should carry these over:
 *
 *   +0x04  `int_0004` there, `reverse` here.  faxcfg records that the table
 *          holds 0 and the constructors set it to 1; FAXVMI_process is what
 *          says what 1 DOES -- it routes the block through
 *          `vmi_reverse[mode]` in both directions (F9010).  The two readings
 *          agree: a receive path wants the wire's bit order undone.
 *   +0x08  `short_0008` there, `fifo_size` here -- it is the ring capacity
 *          FAXVMI_create asks for.
 *   +0x0a  `short_000a` there, `max_frame` here, and it has four independent
 *          confirmations (F9011, F9019).
 *   +0x0c  `short_000c` there, `frame_size` here -- it sizes the HDLC
 *          assembly buffer.
 *
 * The other direction is worth as much: faxcfg's `modem_cfg` at +0x10 is the
 * second argument `FAXVMI_create` hands `vxx_create`, which is all this file
 * knew about it, and its `slot` at +0x0e is this file's `slot`.
 *
 * Embedding `struct faxvmi_cfg` as a member here is the obvious tidy-up and
 * is NOT done: it would change every offset comment in this header for no
 * behavioural gain, and both files are correct as they stand.
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
 * It is ONE object carrying several unrelated things, which is why it looks
 * odd:
 *
 *   +0x00..+0x0d   a ring of 16-bit elements: the ring writers fill it, the
 *                  packers drain it
 *   +0x10..+0x1f   the bit-level PACKER's state
 *   +0x20..+0x3f   the bit-level UNPACKER's state
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
	/* --- the ring: the writers fill it, the packers drain it ----- */
	unsigned short *fifo;		/* +0x00 `size` elements, from
					 *       sysdep_malloc(size * 2)     */
	unsigned short fifo_size;	/* +0x04 capacity, in elements      */
	unsigned short rd;		/* +0x06 READ cursor, in elements.
					 * All three packers take the next
					 * element from `fifo[rd]`, store
					 * rd + 1 back, wrap it to zero at
					 * `fifo_size` and drop `count` by
					 * one -- the exact mirror of what
					 * the writers do with `wr`.
					 *
					 * THE AUTHOR NAMES BOTH CURSORS.
					 * faxvmi_hdlc_frame's debug line
					 * (0x95e72) is "HDLC Transmitted
					 * Frame (Length = %d, iR = %d,
					 * iW = %d)" and its arguments are the
					 * length, +0x06 and +0x08 in that
					 * order -- so `iR` is this and `iW`
					 * is the next field, from a format
					 * string rather than from use
					 * (F9191)                           */
	unsigned short wr;		/* +0x08 write cursor, in elements;
					 * `iW` in the line quoted above     */
	unsigned short count;		/* +0x0a occupancy, in elements     */
	int residue;			/* +0x0c input elements the packer
					 * could NOT get into the ring: each
					 * packer subtracts what its two
					 * faxvmi_write_fifo calls accepted
					 * from `count` and stores the
					 * balance here as its last act.
					 * create zeroes it, FAXVMI_status
					 * copies it to the status record's
					 * +0x04, and FAXVMI_process makes it
					 * bit 26 of the status word (F9192) */

	/* --- the bit-level PACKER, mirror of the unpacker below ------ */
	unsigned int pack_mask;		/* +0x10 the bit being taken out of
					 * `pack_word`; zero means "fetch the
					 * next source".  32 bits wide with
					 * every reader holding it in an
					 * `unsigned short` local, exactly as
					 * `unpack_mask` is -- movzwl in,
					 * movl out.  create: 0             */
	unsigned int pack_word;		/* +0x14 the source being shifted out:
					 * a bare octet with mask 0x80 for
					 * the simple form, a ten-bit async
					 * character with mask 0x200 for the
					 * async one.  create: -1           */
	unsigned int pack_acc;		/* +0x18 the bit accumulator, emptied
					 * into an output element every
					 * `link->pack_width` bits.
					 * create: -1                       */
	unsigned short pack_bit;	/* +0x1c bits accumulated into the
					 * element under construction.
					 * create and control write a WORD of
					 * zero here and then read a DWORD
					 * back from it into +0x2c          */
	unsigned short pad_001e;	/* +0x1e never written               */

	/* --- the bit-level UNPACKER ---------------------------------- */
	unsigned int unpack_mask;	/* +0x20 the bit being taken out of
					 * `unpack_word`, walking down from
					 * 1 << (link->unpack_width - 1);
					 * zero means "fetch the next
					 * element".  The field is 32 bits
					 * and every reader holds it in an
					 * `unsigned short` local, which is
					 * why the loads are movzwl and the
					 * stores are movl (F9012)          */
	unsigned int unpack_word;	/* +0x24 the element being consumed */
	unsigned int unpack_acc;	/* +0x28 the bit accumulator.  It is
					 * 32 bits because the async framer
					 * tests 23 bits of history in it   */
	unsigned short unpack_bit;	/* +0x2c bits assembled into the
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
	unsigned short zero_run_bits;	/* +0x34 how many more ZERO BITS
					 * faxvmi_asyc_pack is to send in
					 * place of characters; it emits one
					 * per bit and counts this down.
					 * FAXVMI_control copies the control
					 * record's +0x08 here, so it is a
					 * request from outside (F9193).
					 * Named as the receive side's
					 * `zero_run_seen` is: a sustained
					 * zero run in async framing is what
					 * a BREAK is, but the object gives
					 * no word for it and neither does
					 * this                             */
	int zero_run_send;		/* +0x38 nonzero while that run is
					 * being sent; faxvmi_asyc_pack
					 * clears it when the count reaches
					 * zero.  FAXVMI_control copies the
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
	short pack_frame_left;		/* +0x46 octets of the frame
					 * `faxvmi_hdlc_frame` is transmitting
					 * that are still in the ring.  Zero
					 * means the next element is a LENGTH
					 * and a fresh frame starts; non-zero
					 * means it is a data octet and this
					 * counts down.  SIGNED: the object
					 * loads it with `movswl` (0x95b7f) and
					 * stores sixteen bits (0x95d81), and
					 * the length it is loaded FROM is read
					 * `movswl` too (0x95de5).  create and
					 * control zero it                   */
	short frame_len;		/* +0x48 octets assembled so far    */
	unsigned short flags_wanted;	/* +0x4a opening flags still to be
					 * seen before octets are kept;
					 * create and control both set it to
					 * 2 and each closing flag decrements
					 * it while it is nonzero           */
	int pack_flagging;		/* +0x4c non-zero while
					 * `faxvmi_hdlc_frame`'s source is a
					 * FLAG rather than frame data -- the
					 * three-flag preamble, or the single
					 * flag an empty ring sends.  It gates
					 * the zero insertion and nothing else,
					 * which is what makes flags
					 * transparent: five ones inside data
					 * are stuffed, five ones inside 0x7E
					 * are not.  create and control set it
					 * to 1                              */
	short ones;			/* +0x50 the run of consecutive one
					 * bits: 5 destuffs the next zero,
					 * 6 is a flag                     */
	int in_frame;			/* +0x54 set when an octet is stored
					 * into `frame`, cleared at every
					 * flag.  A closing flag with this
					 * set is a complete frame          */
};

/*
 * `pad_0036` and `pad_0052` are gone (pad-region removal audit, F10145):
 * both were exactly the 2-byte gap the compiler inserts on its own ahead of
 * the following 4-byte-aligned `int` (`zero_run_send` at +0x38,
 * `in_frame` at +0x54), never read or written anywhere in this tree, and
 * unlike `pad_001e` above there is no dword-reload trick that names either
 * one.  These assertions are what proves it -- `__builtin_offsetof` against
 * the struct as it stands NOW, so a future edit that reintroduces a real
 * gap fails to compile rather than silently shifting these two fields.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define FRAMER_ASSERT_OFF(field, off) \
	typedef char faxvmi_framer_off_##field[ \
		((int)__builtin_offsetof(struct faxvmi_framer, field) \
			== (off)) ? 1 : -1]
FRAMER_ASSERT_OFF(zero_run_send, 0x38);
FRAMER_ASSERT_OFF(in_frame, 0x54);
typedef char faxvmi_framer_size[
	(sizeof(struct faxvmi_framer) == 0x58) ? 1 : -1];
#endif

/*
 * THE LINK BLOCK, `vmi->link` (+0x28).  `sysdep_malloc(0x18)` at 0x953d4, so
 * 24 bytes, and it is the handle every `vxx_*` entry point receives:
 * `FAXVMI_create` calls `vxx_create[slot](vmi->link, cfg->int_0010)` and
 * message, status and control all pass it as their first argument.
 *
 * Four fields are established here, and every one of them by being READ.
 * `buf` is the element buffer BOTH DIRECTIONS use: the three unpackers take
 * their input elements from it and the three packers write their output
 * elements INTO it, so it was called `rx` while only the unpackers had been
 * read and that name is now retired (F9194).  One faxvmi instance wraps one
 * modulation and a slot is either transmit or receive, so a given instance
 * only ever uses it one way round.
 *
 * THE TWO WIDTHS ARE TWO FIELDS, not one read twice.  The packers take
 * `pack_width` from +0x0e and the unpackers `unpack_width` from +0x10; both
 * are `movzwl` loads of sixteen bits at those two offsets, four bytes apart.
 * Neither is written by FAXVMI_create -- `vxx_create` fills them, and this
 * file does not know what it puts there.
 */
struct faxvmi_link {
	unsigned short *ptr_0000;	/* +0x00 sysdep_malloc(0x190) --
					 * 200 elements, which create zeroes */
	unsigned short *buf;		/* +0x04 sysdep_malloc(0x64) -- 50
					 * elements, which create fills with
					 * 0xffff.  The unpackers' input and
					 * the packers' output               */
	unsigned char pad_0008[4];	/* +0x08                             */
	short pack_count;		/* +0x0c how many elements a packer
					 * produces per call.  Read `movswl`,
					 * counted down in a local, never
					 * written back -- and it is what
					 * faxvmi_asyc_pack RETURNS, reloaded
					 * from the object at the end        */
	unsigned short pack_width;	/* +0x0e significant bits per output
					 * element: every packer builds
					 * (1 << pack_width) - 1 and masks
					 * the accumulator with it before
					 * storing                           */
	unsigned short unpack_width;	/* +0x10 significant bits per input
					 * element: every unpacker starts its
					 * mask at 1 << (unpack_width - 1)   */
	int int_0014;			/* +0x14 create zeroes it            */
};

/*
 * `pad_0012` is gone (pad-region removal audit, F10145): `unpack_width` ends
 * at +0x12 and `int_0014` needs 4-byte alignment, so the compiler inserts
 * the same 2-byte gap on its own; nothing in this tree ever names the field.
 * `pad_0008[4]` above is NOT the same shape and stays explicit -- `buf` ends
 * already 4-byte aligned at +0x0c, where `pack_count` (a `short`, needing no
 * more than 2-byte alignment) would land with no gap at all if the pad were
 * deleted, so those four bytes are a real, unexplained gap and not
 * alignment filler; CLAUDE.md's `VPcmFloModem::pad_6fb8[4]` is the exact
 * precedent for leaving a shape like this alone.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define LINK_ASSERT_OFF(field, off) \
	typedef char faxvmi_link_off_##field[ \
		((int)__builtin_offsetof(struct faxvmi_link, field) \
			== (off)) ? 1 : -1]
LINK_ASSERT_OFF(int_0014, 0x14);
typedef char faxvmi_link_size[
	(sizeof(struct faxvmi_link) == 0x18) ? 1 : -1];
#endif

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
	int underrun;			/* +0x18 the transmit ring ran dry.
					 * ALL THREE PACKERS set it to 1 on
					 * the one arm where the bit engine
					 * wanted another source element and
					 * `framer->count` was zero, and each
					 * transmits fill instead -- zero for
					 * the simple form, a mark bit for
					 * the async one (0x9658d, 0x958fb,
					 * 0x95cd8).  faxvmi_write_fifo and
					 * faxvmi_write_frame clear it when a
					 * whole request is accepted; create
					 * sets it to 1 because nothing has
					 * been written yet; FAXVMI_status
					 * copies it to the status record's
					 * +0x08 and FAXVMI_process makes it
					 * bit 24 of the status word.
					 *
					 * SEVEN SITES AND ONE READING, which
					 * is what took this from a neutral
					 * name to a real one (F9195).  The
					 * author's own phrase for the
					 * condition is in the transmit
					 * modulations' message tables --
					 * "ERROR: Transmit Input Queue
					 * Under-run", code 8 of each -- so
					 * the word is the object's even
					 * though no format string prints
					 * THIS field                        */
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
 * `vxx_create[slot]`, `.rodata` 0x9620 -- the FIRST of the six 13-slot tables
 * in address order and the one `FAXVMI_create` (below) itself dispatches
 * through.  Same 13-slot order as every other vxx table, read straight off
 * this table's own relocations (`objdump -r`, 0x9620..0x9650): 0..4
 * null_create, 5 v21tx_create, 6 v21rx_create, 7 v27tx_create,
 * 8 v27rx_create, 9 v29tx_create, 10 v29rx_create, 11 v17tx_create,
 * 12 v17rx_create.
 *
 * `null_create` takes an untyped `const void *cfg` (`nulldp.h`); the eight
 * `v??tx_create`/`v??rx_create` each take a typed `const struct v??[tr]x_cfg
 * *` (`faxadapt.h`).  The table's own caller passes `vmi->int_0010` --
 * plain `int` -- straight into the argument slot with no type-specific
 * handling (`FAXVMI_create`, 0x952be..0x952cf), so the table's element type
 * is the untyped form and the eight typed entries need the same kind of
 * explicit cast `vxx_status` already carries for its own typed four.
 */
typedef void (*faxvmi_create_fn)(struct faxvmi_link *dp, const void *cfg);
extern faxvmi_create_fn const vxx_create[13];

/*
 * `struct faxvmi`'s own constructor, 0x095120, 705 bytes -- the FIRST of the
 * five entry points in address order.  Two behaviours share one function:
 *
 *   `vmi == NULL`   a FRESH create.  `vmi`, `vmi->framer` and `vmi->link` are
 *                   allocated here (`sysdep_malloc(0x2c)`/`(0x58)`/`(0x18)`,
 *                   0x953ab/0x953c0/0x953d4) and every heap block below is
 *                   allocated too.
 *   `vmi != NULL`   a REINIT of an existing instance.  Nothing is
 *                   (re)allocated; the ring, the frame buffer and the link's
 *                   two buffers are cleared/refilled in place instead.
 *
 * `cfg == NULL` copies `FAXVMI_CFG` over `vmi`'s leading six dwords instead
 * of the caller's own (0x95376..0x953a6); both arms rejoin at the exact same
 * six stores (0x95141..0x95160), which is why they are written here as one
 * assignment from a selected source rather than as two branches.
 *
 * The ring's capacity is `max(cfg->fifo_size, cfg->max_frame + 3)`
 * (0x9516b..0x95185) -- the `+ 3` is `faxvmi_write_frame`'s own per-frame
 * overhead, confirmed independently in `faxvmi_framer`'s own comment above.
 * `vmi->underrun` is left set to 1 on every path (create has written
 * nothing yet); `vmi->overflow` and `vmi->status` are cleared.
 */
struct faxvmi *FAXVMI_create(struct faxvmi *vmi, const struct faxvmi_cfg *cfg);

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

/*
 * `vxx_delete[slot]`.  Same 13-slot order as every other vxx table
 * (`nulldp.h`'s own reading of the layout, confirmed again by this table's
 * own relocations): 0..4 null_delete, 5 v21tx_delete, 6 v21rx_delete,
 * 7 v27tx_delete, 8 v27rx_delete, 9 v29tx_delete, 10 v29rx_delete,
 * 11 v17tx_delete, 12 v17rx_delete -- all thirteen already written
 * (`faxadapt.h`, `nulldp.h`) with the SAME signature, so no cast is needed
 * anywhere in the table's initialiser.
 */
typedef void (*faxvmi_delete_fn)(struct faxvmi_link *dp);
extern faxvmi_delete_fn const vxx_delete[13];

/*
 * `vmi->slot`'s own teardown: releases the wrapped modulation, then every
 * heap block `FAXVMI_create` allocated, then `vmi` itself.  Evidence is
 * `FAXVMI_delete` (0x0953f0, read here as it is not yet written -- see the
 * top of faxvmi.c for what still blocks it).
 */
void FAXVMI_delete(struct faxvmi *vmi);

/*
 * `vxx_status[slot]`.  Same 13-slot order and order of evidence as
 * `vxx_delete` above.  UNLIKE `vxx_delete`, the table's own callers do NOT
 * all share one declared signature: `v17tx_status`/`v17rx_status` and
 * `v21tx_status`/`v21rx_status` take a typed `struct v17_status *` /
 * `struct v21_status *` in `faxadapt.h`, where every other slot and the
 * table's own caller (`FAXVMI_status`, below) take a bare `void *` --
 * `null_status`'s own declaration in `nulldp.h` is explicit that this is
 * exact rather than a guess, because neither the caller nor `null_status`
 * itself ever narrows the pointer.  So the table's element type is the
 * untyped form, and the four typed entries need an explicit cast in the
 * initialiser; nothing here says those four functions are wrong to take a
 * typed pointer; the table just cannot express it.
 */
typedef int (*faxvmi_status_fn)(struct faxvmi_link *dp, void *status);
extern faxvmi_status_fn const vxx_status[13];

/*
 * THE STATUS RECORD, `FAXVMI_status`'s second argument.  Caller-allocated --
 * no `sysdep_malloc` sizes it anywhere in `FAXVMI_status` -- so unlike
 * `struct faxvmi`/`_framer`/`_link` there is no total size to assert; only
 * the fields this function itself touches are known, all read off
 * `FAXVMI_status` (0x0955c0, read here as evidence; the function is not yet
 * written, see faxvmi.c).
 *
 * `modem_status` is read FIRST, before anything is written, and passed
 * UNCHANGED to `vxx_status[vmi->slot]` as its own second argument when it is
 * non-NULL -- a caller wanting the wrapped modulation's own status pre-loads
 * a pointer for it there.  Every other field is written unconditionally.
 */
struct faxvmi_status {
	unsigned short room;		/* +0x00 fifo_size - count: elements
					 * of the ring still free           */
	int residue;			/* +0x04 = framer->residue -- the
					 * 2-byte gap `room` leaves ahead of
					 * this 4-byte-aligned field is the
					 * compiler's own doing now, not a
					 * named `pad_0002`: FAXVMI_status
					 * never wrote it (pad-region removal
					 * audit, F10145), and the existing
					 * STATUS_ASSERT_OFF(residue, 0x04)
					 * below still holds, which is the
					 * proof                             */
	int underrun;			/* +0x08 = vmi->underrun             */
	int overflow;			/* +0x0c = vmi->overflow             */
	int zero_run_seen;		/* +0x10 = framer->zero_run_seen     */
	int flag_0014;			/* +0x14 (framer->flags_wanted <= 1)
					 * as 0/1 -- at least one opening
					 * flag has been seen since the HDLC
					 * receiver last reset.  Usage
					 * inference only: the object never
					 * names this bit                    */
	void *modem_status;		/* +0x18 IN: forwarded to
					 * vxx_status[slot] when non-NULL;
					 * never written by this function    */
};

/*
 * Every field but the last is a plain `int`/`unsigned short`, so these hold
 * on both a 32- and a 64-bit build without `__SIZEOF_POINTER__`'s guard --
 * unlike `faxvmi_link`, the one pointer here is the LAST field and does not
 * shift anything before it.
 */
#define STATUS_ASSERT_OFF(field, off) \
	typedef char faxvmi_status_off_##field[ \
		((int)__builtin_offsetof(struct faxvmi_status, field) \
		 == (off)) ? 1 : -1]
STATUS_ASSERT_OFF(room, 0x00);
STATUS_ASSERT_OFF(residue, 0x04);
STATUS_ASSERT_OFF(underrun, 0x08);
STATUS_ASSERT_OFF(overflow, 0x0c);
STATUS_ASSERT_OFF(zero_run_seen, 0x10);
STATUS_ASSERT_OFF(flag_0014, 0x14);
STATUS_ASSERT_OFF(modem_status, 0x18);

/*
 * Fill `status` from `vmi`'s own counters and, if `status->modem_status` is
 * set, from the wrapped modulation's own status too -- in which case the
 * return value is THAT call's return, not 0.  `status == NULL` returns -1
 * without touching anything.
 */
int FAXVMI_status(struct faxvmi *vmi, struct faxvmi_status *status);

/*
 * THE CONTROL RECORD, `FAXVMI_control`'s second argument -- `FAXVMI_CTL`
 * below is the object's own instance of it, not a struct this file's
 * functions consume.  Evidence is `FAXVMI_control` (0x095650), read here for
 * the same reason `FAXVMI_status`'s record is: `FAXVMI_control` itself is
 * NOT yet written (this wave's brief excludes it -- it needs `SetScramblerV27`,
 * `TxHdxABV17` and dozens more that two concurrent strands are writing this
 * wave), but `FAXVMI_CTL` is a plain 24-byte `.rodata` object with no
 * dependency of its own, and its type is what this file needs to declare it.
 *
 * `ptr_0000` nonzero also empties the ring (`framer->rd`/`wr`/`count`/
 * `residue` all zeroed, `framer->fifo` overwritten with zero out to
 * `fifo_size`).  `int_0004`/`short_0008` are copied straight into
 * `framer->zero_run_send`/`zero_run_bits` -- already named from this same
 * evidence in `faxvmi_framer`'s own fields, above.  `int_000c` nonzero (and
 * `short_0010` at most 2) additionally resets the whole HDLC receiver to
 * `FAXVMI_create`'s own initial values and sets `vmi->mode` from
 * `short_0010`; `int_0014` nonzero makes `FAXVMI_control` recurse through
 * `vxx_control[vmi->slot](vmi->link, (void *)(long)ctl->int_0014)` before
 * applying anything else -- `int_0014` ITSELF cast to a pointer, not a
 * literal -1: `dis.py` shows `mov 0x14(%ebx),%eax; test %eax,%eax; jne
 * 0x95786` landing directly on the call site's argument setup with no
 * intervening write to `%eax`, so the tested value is what's passed.
 * Four of the six real fields already have a habitable name (the struct
 * carried eight members before the pad-region removal audit, F10145, took
 * out `pad_000a` and `pad_0012` -- both were the compiler's own alignment
 * gap ahead of `int_000c`/`int_0014`, confirmed by the CTL_ASSERT_OFF
 * entries below still holding); the rest are usage inference only, hedged
 * as such, and left neutral rather than guessed further -- CLAUDE.md's
 * "naming wrongly is worse than padding" ground.
 */
#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
#define CTL_ASSERT_OFF(field, off) \
	typedef char faxvmi_ctl_off_##field[ \
		((int)__builtin_offsetof(struct faxvmi_ctl, field) == (off)) \
			? 1 : -1]
#endif

struct faxvmi_ctl {
	void *ptr_0000;		/* +0x00 nonzero: also empty the ring */
	int int_0004;		/* +0x04 -> framer->zero_run_send     */
	unsigned short short_0008;	/* +0x08 -> framer->zero_run_bits */
	int int_000c;		/* +0x0c nonzero: full framer reset + mode
				 * change from short_0010.  The 2-byte gap
				 * `short_0008` leaves ahead of this is the
				 * compiler's own alignment now, not a named
				 * `pad_000a` -- FAXVMI_control never read it
				 * (pad-region removal audit, F10145), and
				 * the CTL_ASSERT_OFF(int_000c, 0x0c) below
				 * still holds, which is the proof          */
	unsigned short short_0010;	/* +0x10 new vmi->mode, 0..2       */
	int int_0014;		/* +0x14 nonzero: also call
				 * vxx_control[vmi->slot](vmi->link,
				 * (void *)(long)int_0014) before anything
				 * else applies.  Same removal as
				 * `int_000c`'s own note above: `pad_0012`
				 * is gone, `short_0010` ends at +0x12 and
				 * this field's own 4-byte alignment
				 * reproduces the gap, CTL_ASSERT_OFF(
				 * int_0014, 0x14) below is the proof       */
};

#if defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 4
CTL_ASSERT_OFF(ptr_0000, 0x00);
CTL_ASSERT_OFF(int_0004, 0x04);
CTL_ASSERT_OFF(short_0008, 0x08);
CTL_ASSERT_OFF(int_000c, 0x0c);
CTL_ASSERT_OFF(short_0010, 0x10);
CTL_ASSERT_OFF(int_0014, 0x14);
typedef char faxvmi_ctl_size[(sizeof(struct faxvmi_ctl) == 0x18) ? 1 : -1];
#endif

/*
 * The object's own instance: 24 bytes of zero at `.rodata` 0x9478,
 * referenced from many places inside the per-modulation
 * `v??tx_control`/`v??rx_control` functions (`faxadapt.c`, all eight now
 * written) -- every one of those references reads as passing `&FAXVMI_CTL`
 * where `FAXVMI_control`'s `ctl == NULL` path is not what is wanted:
 * `FAXVMI_control` itself returns -1 immediately on a null `ctl` without
 * ever touching this object, so a caller wanting the all-fields-quiescent
 * behaviour (no ring clear, no zero-run change, no mode change, no
 * `vxx_control` recursion) passes the address of this all-zero record
 * instead of NULL.  `R`, global, in the object; global here too.
 */
extern const struct faxvmi_ctl FAXVMI_CTL;

/*
 * `vxx_control[slot]`, `.rodata` 0x9560 -- the last of the six 13-slot
 * tables.  Same slot order as the rest (0..4 null_control, 5 v21tx_control,
 * 6 v21rx_control, 7 v27tx_control, 8 v27rx_control, 9 v29tx_control,
 * 10 v29rx_control, 11 v17tx_control, 12 v17rx_control), read off this
 * table's own relocations (0x9560..0x9590).  Every entry already returns
 * `int` (`faxadapt.h`, `nulldp.h`), so unlike `vxx_process` this table needs
 * no return-type cast -- only the argument-pointer casts six of the eight
 * typed entries carry, the same shape `vxx_status`/`vxx_create` already use.
 */
typedef int (*faxvmi_control_fn)(struct faxvmi_link *dp, void *arg);
extern faxvmi_control_fn const vxx_control[13];

/*
 * `FAXVMI_control`, 0x095650, 338 bytes.  Applies `ctl`'s effects to `vmi`
 * in the object's own order:
 *
 *   1. `ctl == NULL` returns -1 immediately, nothing touched.
 *   2. `ctl->int_0014` nonzero recurses FIRST, before anything else applies:
 *      `vxx_control[vmi->slot](vmi->link, (void *)(long)ctl->int_0014)`,
 *      and that call's return becomes this function's own return (0
 *      otherwise) -- confirmed independently against `dis.py`, matching
 *      F10108's own trace of this same site.
 *   3. `ctl->int_000c` nonzero AND `ctl->short_0010 <= 2` (unsigned; an
 *      out-of-range mode is treated as if `int_000c` were zero, the whole
 *      block skipped) -- a full framer reset to `FAXVMI_create`'s own
 *      initial values (frame buffer zeroed to `frame_size`, then
 *      `pack_frame_left`/`frame_len`/`ones`/`in_frame` cleared,
 *      `flags_wanted` = 2, `pack_flagging` = 1; then `pack_bit` = 0 and the
 *      dword-reload trick `unpack_bit = pack_bit; short_002e = pad_001e;`
 *      `FAXVMI_create` itself uses; `zero_run_bits` = 0 -- overwritten again
 *      at step 5 below if `ctl->short_0008` also applies; `unpack_mask` = 0,
 *      `unpack_word`/`unpack_acc` = -1, `async_hunt` = 1, `zero_run_send` = 0,
 *      `zero_run_seen` = 0, `pack_mask` = 0, `pack_word`/`pack_acc` = -1) --
 *      and sets `vmi->mode = ctl->short_0010`.
 *   4. `ctl->ptr_0000` nonzero empties the ring: `fifo[]` zeroed to
 *      `fifo_size`, then `rd`/`wr`/`count`/`residue` = 0.
 *   5. Unconditionally: `zero_run_send = ctl->int_0004`,
 *      `zero_run_bits = ctl->short_0008`.
 *   6. Returns whatever step 2 left (0 if `int_0014` was zero).
 */
int FAXVMI_control(struct faxvmi *vmi, const struct faxvmi_ctl *ctl);

/*
 * `vxx_process[slot]`, `.rodata` 0x9520.  Same 13-slot order, read off this
 * table's own relocations (0x9520..0x9550): 0..4 null_process, 5
 * v21tx_process, 6 v21rx_process, 7 v27tx_process, 8 v27rx_process, 9
 * v29tx_process, 10 v29rx_process, 11 v17tx_process, 12 v17rx_process.
 *
 * THE RETURN VALUE IS NOT DECORATIVE.  `null_process` is declared `int` and
 * returns -1 always (`nulldp.h`); the eight `v??tx_process`/`v??rx_process`
 * are declared `void` (`faxadapt.h`) -- but `FAXVMI_process` (below) reads
 * `%eax` straight out of every one of these calls and folds it into the
 * status word it returns (0x954ec..0x95564), so the table's element type is
 * `int`-returning and the eight `void` entries need the same cast
 * `vxx_status` already carries for ITS mismatched four.  For those eight,
 * this reproduces whatever their own last-touched register happened to hold
 * (typically the wrapped `V??[TR]X_modem`'s own return, since nothing after
 * that call inside them touches `%eax`) -- an UNSPECIFIED value by C's own
 * rules that only the exact compiler which built both sides can be trusted
 * to reproduce identically; see `make period` vs the modern tier in
 * CLAUDE.md.
 *
 * The four formal arguments are untyped here for the same reason: TX and RX
 * entries share the same four PHYSICAL slots (`faxvmi_link *`, `short *`,
 * `unsigned short *`, `unsigned short *`) but disagree on which of the last
 * three is `out`/`in` and which is `count`/`result` (`faxadapt.h`'s own
 * comment on the mirrored shapes).  `FAXVMI_process` calls through this
 * table without ever caring which is which, so it is written that way here
 * too, and the TX/RX declarations in `faxadapt.h`/`nulldp.h` remain the
 * typed record of what each entry actually does with them.
 */
typedef int (*faxvmi_process_fn)(struct faxvmi_link *dp, short *a,
				 unsigned short *b, unsigned short *c);
extern faxvmi_process_fn const vxx_process[13];

/*
 * `vmi->status`'s own bits, read off `FAXVMI_process` (0x9550a..0x95561).
 * `vxx_process[slot]`'s raw return supplies the low 24 bits and whatever it
 * left in bits 29..31 UNCHANGED; these five are the only bits FAXVMI_process
 * itself computes, by masking them all to 0 first (`and $0xe0ffffff`,
 * 0x9550f) and then setting four of them with an `or` when their own
 * condition holds.  The fifth, ZERORUN, is the odd one out: the object
 * clears it (`and $0xefffffff`, 0x9555b) rather than sets it, WHEN
 * `zero_run_seen` is true -- redundant against the initial mask (the bit is
 * already 0), but it is what is there, so it is reproduced rather than
 * folded away.
 */
#define FAXVMI_STATUS_UNDERRUN	0x01000000	/* vmi->underrun            */
#define FAXVMI_STATUS_FULL	0x02000000	/* room < vmi->max_frame    */
#define FAXVMI_STATUS_RESIDUE	0x04000000	/* framer->residue != 0     */
#define FAXVMI_STATUS_OVERFLOW	0x08000000	/* vmi->overflow            */
#define FAXVMI_STATUS_ZERORUN	0x10000000	/* CLEARED, not set, when
						 * framer->zero_run_seen     */

/*
 * `struct faxvmi`'s own per-block driver, 0x095470, 327 bytes.  One call
 * moves one block both ways through the wrapped modulation:
 *
 *   1. `vmi->reverse`: `vmi_reverse[mode](data, *count)` -- bit-reverse the
 *      caller's buffer IN PLACE before anything else touches it.
 *   2. `n = vmi_pack[mode](vmi, data, *count)` -- frame `*count` elements of
 *      `data` into `vmi->link->buf`; `n` is `link->pack_count`.
 *   3. `ret = vxx_process[slot](vmi->link, pcm, &n, result)` -- drive one
 *      block through the wrapped modem.  `n` is passed BY REFERENCE and the
 *      table's own entries (`faxadapt.h`) both copy it to `*result` and then
 *      zero it, so by step 4 it is 0 for every real modulation and UNCHANGED
 *      (still `link->pack_count`) for the null slot, which never touches it.
 *   4. `m = vmi_unpack[mode](vmi, data, n)` -- unframe `n` elements back out
 *      of `vmi->link->buf` into `data`.  `*count` is then set to `m`.
 *   5. `vmi->reverse`: `vmi_reverse[mode](data, m)` -- bit-reverse the
 *      result, symmetric with step 1.
 *   6. `vmi->status` is composed from `ret` and the FAXVMI_STATUS_* bits
 *      above, stored, and returned.
 *
 * Read off the object with the register names spelled out because five
 * arguments cross two calls apiece: `data` is `vmi_pack`/`vmi_unpack`'s own
 * `src`/`dst` and `vmi_reverse`'s `buf`; `pcm` and `result` are passed
 * straight through to `vxx_process` as its own 2nd and 4th arguments;
 * `count` is read signed on entry (feeds step 2), then overwritten with `m`
 * on exit (0x9549a, 0x954fc).
 */
int FAXVMI_process(struct faxvmi *vmi, unsigned short *data, short *pcm,
		    short *count, unsigned short *result);

/*
 * The object's own zeroed `struct faxvmi_status` template, `.rodata`
 * 0x945c, 28 bytes -- `tabdump.py --sym FAXVMI_STS --type u32 --count 7`
 * reads every dword 0.  `fax_class1_status` (`class1.c`) copies it into a
 * local, overwrites `modem_status` with its own second argument, and hands
 * the local to `FAXVMI_status` -- the compiler proves the template's own
 * `modem_status` dword is dead (about to be overwritten) and omits copying
 * it, which is why only six of the seven dwords move in that function's
 * disassembly.  Referenced from one other, still-unwritten site
 * (`class1tx.c`'s span, `_send_hdlc_buffer_state` at 0x9e4b6 and neighbours)
 * -- defined in `class1.c`, its first writer, not here and not in
 * `faxvmi.c`, on the same footing as `FAXVMI_CTL`.
 */
extern const struct faxvmi_status FAXVMI_STS;

/* --------------------------------------------------------------------- */
/* The packers: `vmi_pack[mode]`.                                         */

/*
 * ALL THREE HAVE THE SAME CONTRACT, and it is NOT the unpackers' contract
 * turned round.  `FAXVMI_process` calls `vmi_pack[mode](vmi, buf, *count)`
 * FIRST, hands the return to `vxx_process` as a sample count, and only then
 * unpacks (0x954ab..0x954f5).
 *
 * `src` is the caller's buffer of `count` elements and it is NOT read here.
 * Each packer hands it straight to `faxvmi_write_fifo`, which copies what
 * fits into `framer`'s ring and ADVANCES the cursor; the packer then drains
 * the ring bit by bit into `vmi->link->buf` and calls the writer a second
 * time with whatever is left over.  So the ring is the only thing that
 * carries data between the two halves, and `framer->residue` is what the
 * caller must re-present next time.
 *
 * THE OUTPUT IS `link->buf` AND ITS LENGTH IS `link->pack_count`, NOT
 * ANYTHING DERIVED FROM `count`.  Both write exactly `pack_count` elements of
 * `pack_width` bits whatever the input was; a starved ring is padded with
 * fill rather than shortening the block.  `link->buf` is 50 elements
 * (`sysdep_malloc(0x64)` in FAXVMI_create), so a `pack_count` above that
 * overruns it -- there as here.
 *
 * WHAT THEY RETURN DIFFERS, and the difference is real: the simple form
 * counts only the elements it filled BEFORE the ring first ran dry, and the
 * async form returns `pack_count` unconditionally.
 */

/*
 * Simple framing: eight bits per source octet, MSB first, no framing at all.
 * A ring that runs dry contributes a zero octet, sets `vmi->underrun`, and
 * stops the return value advancing for the rest of the call.
 */
int faxvmi_simp_pack(struct faxvmi *vmi, unsigned short *src, short count);

/*
 * Asynchronous framing: each source octet becomes a ten-bit character --
 * a zero start bit, the eight data bits MSB first, then a one stop bit,
 * assembled as `((octet << 1) | 1) & ~0x200` and shifted out from 0x200
 * down.  A ring that runs dry contributes a single MARK bit and sets
 * `vmi->underrun`, which is the idle line.
 *
 * `framer->zero_run_send` diverts it: while that is set, one ZERO bit is sent
 * per call to the refill and `framer->zero_run_bits` counts down.
 */
int faxvmi_asyc_pack(struct faxvmi *vmi, unsigned short *src, short count);

/*
 * HDLC framing: three flag octets, then the frame, with a zero inserted after
 * every five consecutive one bits of DATA.  What comes out of the ring is
 * what `faxvmi_write_frame` put in -- one length element, then that many
 * octets, then two FCS octets -- so `framer->pack_frame_left` is what says
 * whether the next element is a length or an octet.  An empty ring sends one
 * flag and raises `vmi->underrun`; flags are the idle pattern, so that is not
 * a stall.
 */
int faxvmi_hdlc_frame(struct faxvmi *vmi, unsigned short *src, short count);

/*
 * The HDLC flag, `0111 1110`, and the run of ones that forces a stuffed zero
 * inside data.  The object holds three flags at once for a frame preamble.
 */
#define FAXVMI_HDLC_FLAG	0x7e
#define FAXVMI_HDLC_FLAG3	0x7e7e7e

/* --------------------------------------------------------------------- */
/* The unpackers: `vmi_unpack[mode]`.                                     */

/*
 * ALL THREE HAVE THE SAME CONTRACT.  `count` input elements are taken from
 * `vmi->link->buf` -- NOT from a caller's buffer -- and octets are written to
 * `dst`, one octet per 16-bit element, low byte only.  The return is how many
 * elements were written to `dst`, and it is a short widened to int.
 *
 * THE INPUT CURSOR IS NEVER WRITTEN BACK.  Each call re-reads
 * `vmi->link->buf` and walks a LOCAL copy of it; the advanced pointer is
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

/* --------------------------------------------------------------------- */
/*
 * THE THREE DISPATCH TABLES, indexed by `vmi->mode`.  They carry the AUTHOR'S
 * OWN NAMES in the object's symbol table -- `vmi_unpack` at .rodata 0x94a8,
 * `vmi_pack` at 0x94b4, `vmi_reverse` at 0x94c0, three entries each -- and
 * their contents are read from the relocations at those addresses, not
 * guessed.  They are `const` because the object puts them in `.rodata`, and
 * file-local there; `symmap.py` globalises them, so they are comparable
 * against the blob entry by entry.
 *
 * The pack and unpack forms take a `struct faxvmi *`; the reverse form takes
 * a bare buffer.  Two different shapes, so two typedefs.
 */
typedef int (*faxvmi_frame_fn)(struct faxvmi *vmi, unsigned short *buf,
			       short count);
typedef void (*faxvmi_reverse_fn)(unsigned short *buf, short count);

extern faxvmi_frame_fn const vmi_pack[3];
extern faxvmi_frame_fn const vmi_unpack[3];
extern faxvmi_reverse_fn const vmi_reverse[3];

#endif /* DSPLIB_FAXVMI_H */
