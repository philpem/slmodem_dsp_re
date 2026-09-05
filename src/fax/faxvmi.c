/*
 * faxvmi.c -- Class 1 fax: the VMI's framing layer.
 *
 * Reconstructed from dsplibs.o's class1tx.c +94 span (Faxvmi cluster,
 * 0x095120..0x096bad).  Written here, in the object's own emission order:
 *
 *   FAXVMI_message        .text 0x0957b0     55
 *   faxvmi_asyc_pack      .text 0x0957f0    436
 *   faxvmi_asyc_unpack    .text 0x0959b0    349
 *   faxvmi_hdlc_frame     .text 0x095b10    928
 *   faxvmi_hdlc_unframe   .text 0x095eb0   1577
 *   faxvmi_simp_pack      .text 0x0964e0    401
 *   faxvmi_simp_unpack    .text 0x096680    243
 *   faxvmi_gen_fcs16      .text 0x096780    108
 *   faxvmi_byte_reverse   .text 0x0967f0     93
 *   faxvmi_frame_reverse  .text 0x096850    151
 *   faxvmi_write_fifo     .text 0x0968f0    157
 *   faxvmi_write_frame    .text 0x096990    542
 *   vmi_unpack            .rodata 0x94a8     12  (3 slots)
 *   vmi_pack              .rodata 0x94b4     12  (3 slots)
 *   vmi_reverse           .rodata 0x94c0     12  (3 slots)
 *   vxx_message           .rodata 0x94e0     52  (13 slots)
 *
 * THIS BATCH ALSO ADDS `FAXVMI_control` (0x095650, 338 bytes) and the last of
 * the six 13-slot `vxx_*` tables, `vxx_control` (.rodata, 52 bytes) -- unblocked
 * once `V17TX_control`/`V29TX_control`/`V29RX_control` landed on `master` in
 * waves 9/10, closing the gap this banner used to describe.  `FAXVMI_control`
 * itself is a direct dependency of `_init_receiver`/`_init_transmitter`
 * (`class1rx.c`/`class1tx.c`), whose own reinit path calls it; see findings
 * F10108/F10109 for the prior tracing this confirms independently against
 * `dis.py`.  The five dispatch tables `vmi_pack`, `vmi_unpack`, `vmi_reverse`,
 * `vxx_delete` and `vxx_status` were already written by prior waves; every
 * entry of all six `vxx_*` tables now exists, so the link constraint
 * F8492/F8493 no longer blocks any of them.
 *
 * THIS BATCH ADDS `FAXVMI_create` and `FAXVMI_process` and the two dispatch
 * tables their own table lookups need, `vxx_create` and `vxx_process` -- the
 * last two of the six 13-slot `vxx_*` tables, and the reason these two entry
 * points were blocked until now.  All eighteen callees both tables reach
 * (`null_*` plus the eight `v??tx_*`/`v??rx_*` adapters, `faxadapt.h`/
 * `nulldp.h`) were already written by a prior wave (`faxadapt.c` landed on
 * `master` after this file's branch point; merged in rather than
 * re-derived), so both tables link clean.  `FAXVMI_delete`, `FAXVMI_status`
 * and `FAXVMI_CTL`, the all-zero 24-byte "quiescent control record"
 * `FAXVMI_control`'s own callers pass instead of NULL, were added by the
 * prior wave that carried `vxx_delete`/`vxx_status` and are unchanged here.
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
#include "dsplib/faxadapt.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/nulldp.h"
#include "dsplib/sysdep.h"
#include "dsplib/t30frame.h"

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
FRAMER_ASSERT_OFF(pack_frame_left, 0x46);
FRAMER_ASSERT_OFF(frame_len, 0x48);
FRAMER_ASSERT_OFF(flags_wanted, 0x4a);
FRAMER_ASSERT_OFF(pack_flagging, 0x4c);
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

/*
 * The three framing tables, in the object's own `.rodata` order -- unpack at
 * 0x94a8, pack at 0x94b4, reverse at 0x94c0, immediately before
 * `vxx_message` at 0x94e0.  Every entry is read from the relocation at its
 * address; none is inferred from the function names.
 *
 * THEY ARE FILE-LOCAL IN THE OBJECT (`r`, not `R`) AND GLOBAL HERE -- D1122.
 * The only reader is `FAXVMI_process`, which this tree has not written, so a
 * `static` copy would have no referent and the compiler would discard it,
 * taking the comparison against the blob with it.  Same shape as D1081.
 */
faxvmi_frame_fn const vmi_unpack[3] = {
	faxvmi_simp_unpack,
	faxvmi_asyc_unpack,
	faxvmi_hdlc_unframe,
};

faxvmi_frame_fn const vmi_pack[3] = {
	faxvmi_simp_pack,
	faxvmi_asyc_pack,
	faxvmi_hdlc_frame,
};

faxvmi_reverse_fn const vmi_reverse[3] = {
	faxvmi_byte_reverse,
	faxvmi_byte_reverse,
	faxvmi_frame_reverse,
};

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
 * `vxx_create`.  `null_create` already shares the table's own untyped
 * signature (`nulldp.h`); the eight `v??tx_create`/`v??rx_create` each take
 * a typed `const struct v??[tr]x_cfg *` (`faxadapt.h`) and need the explicit
 * cast, same shape as `vxx_status` below.
 */
faxvmi_create_fn const vxx_create[13] = {
	null_create,
	null_create,
	null_create,
	null_create,
	null_create,
	(faxvmi_create_fn)v21tx_create,
	(faxvmi_create_fn)v21rx_create,
	(faxvmi_create_fn)v27tx_create,
	(faxvmi_create_fn)v27rx_create,
	(faxvmi_create_fn)v29tx_create,
	(faxvmi_create_fn)v29rx_create,
	(faxvmi_create_fn)v17tx_create,
	(faxvmi_create_fn)v17rx_create,
};

/*
 * `FAXVMI_create`, 0x095120.  See `faxvmi.h` for the full derivation; this is
 * the object's own control flow read straight off `dis.py`, not tidied:
 *
 *   - `created` is the object's own `%ebp` -- 0 throughout unless `vmi` was
 *     NULL on entry, in which case it is set to 1 (0x953c8) right after `vmi`
 *     and `vmi->framer` are allocated, and tested three more times
 *     (0x95165, 0x9521f, 0x95278) to gate every OTHER allocation and nothing
 *     else: the ring/frame-buffer/link-buffer CONTENTS are cleared or
 *     refilled unconditionally either way.
 *   - the six-dword config copy (`src`) is written once because the object's
 *     two source arms -- the caller's `cfg` and the `FAXVMI_CFG` default --
 *     rejoin at the exact same six stores (0x95141..0x95160 / 0x95376..
 *     0x953a6).
 *   - `fr->unpack_bit = fr->pack_bit; fr->short_002e = fr->pad_001e;` is a
 *     DWORD reload-and-store in the object (0x951d4, 0x95201: the whole
 *     dword at framer+0x1c is read back into a register and stored whole to
 *     framer+0x2c) -- both fields, not just the named one, which is why
 *     `pad_001e` is read here even though nothing else in this tree ever
 *     writes it.
 */
struct faxvmi *
FAXVMI_create(struct faxvmi *vmi, const struct faxvmi_cfg *cfg)
{
	struct faxvmi_framer *fr;
	struct faxvmi_link *lk;
	const struct faxvmi_cfg *src;
	int created = 0;
	unsigned short need, ring;
	int i;

	if (vmi == NULL) {
		vmi = sysdep_malloc(0x2c);
		vmi->framer = sysdep_malloc(0x58);
		created = 1;
		vmi->link = sysdep_malloc(0x18);
	}

	src = (cfg != NULL) ? cfg : &FAXVMI_CFG;
	vmi->mode = src->mode;
	vmi->pad_0002 = src->short_0002;
	vmi->reverse = src->reverse;
	vmi->fifo_size = src->fifo_size;
	vmi->max_frame = src->max_frame;
	vmi->frame_size = src->frame_size;
	vmi->slot = src->slot;
	vmi->int_0010 = (int)(long)src->modem_cfg;
	vmi->int_0014 = (int)(long)src->ptr_0014;

	fr = vmi->framer;

	if (created) {
		need = (unsigned short)(vmi->max_frame + 3);
		ring = (vmi->fifo_size >= need) ? vmi->fifo_size : need;
		fr->fifo = sysdep_malloc((unsigned int)ring * 2);
		fr->fifo_size = ring;
	}
	if (fr->fifo_size != 0) {
		for (i = 0; i < fr->fifo_size; i++)
			fr->fifo[i] = 0;
	}
	fr->rd = 0;
	fr->wr = 0;
	fr->count = 0;
	fr->pack_bit = 0;
	fr->residue = 0;
	fr->unpack_bit = fr->pack_bit;		/* dword reload, see above */
	fr->short_002e = fr->pad_001e;
	fr->pack_mask = 0;
	fr->pack_word = (unsigned int)-1;
	fr->pack_acc = (unsigned int)-1;
	fr->unpack_mask = 0;
	fr->unpack_word = (unsigned int)-1;
	fr->unpack_acc = (unsigned int)-1;
	fr->async_hunt = 1;
	fr->zero_run_bits = 0;
	fr->zero_run_send = 0;
	fr->zero_run_seen = 0;

	if (created) {
		fr->frame = sysdep_malloc((unsigned int)vmi->frame_size * 2);
		fr->frame_size = vmi->frame_size;
	}
	if (fr->frame_size != 0) {
		for (i = 0; i < fr->frame_size; i++)
			fr->frame[i] = 0;
	}
	fr->pack_frame_left = 0;
	fr->frame_len = 0;
	fr->flags_wanted = 2;
	fr->pack_flagging = 1;
	fr->ones = 0;
	fr->in_frame = 0;

	lk = vmi->link;
	if (created) {
		lk->int_0014 = 0;
		lk->buf = sysdep_malloc(0x64);
		lk->ptr_0000 = sysdep_malloc(0x190);
	}
	for (i = 0; i <= 49; i++)
		lk->buf[i] = 0xffff;
	for (i = 0; i <= 199; i++)
		lk->ptr_0000[i] = 0;

	vxx_create[(unsigned short)vmi->slot](lk,
					      (const void *)(long)vmi->int_0010);

	vmi->underrun = 1;
	vmi->overflow = 0;
	vmi->status = 0;
	return vmi;
}

/*
 * `vxx_delete`.  Every entry already shares the one signature
 * `void (*)(struct faxvmi_link *)` (`faxadapt.h`, `nulldp.h`), so no cast is
 * needed anywhere below -- unlike `vxx_status`, next.
 */
faxvmi_delete_fn const vxx_delete[13] = {
	null_delete,
	null_delete,
	null_delete,
	null_delete,
	null_delete,
	v21tx_delete,
	v21rx_delete,
	v27tx_delete,
	v27rx_delete,
	v29tx_delete,
	v29rx_delete,
	v17tx_delete,
	v17rx_delete,
};

/*
 * `vxx_status`.  `v17tx_status`/`v17rx_status`/`v21tx_status`/`v21rx_status`
 * are declared in `faxadapt.h` with a typed second argument
 * (`struct v17_status *` / `struct v21_status *`); the table's own element
 * type is the untyped form `FAXVMI_status` itself calls through
 * (`faxvmi.h`), so those four need the explicit cast the other nine do not.
 */
faxvmi_status_fn const vxx_status[13] = {
	null_status,
	null_status,
	null_status,
	null_status,
	null_status,
	(faxvmi_status_fn)v21tx_status,
	(faxvmi_status_fn)v21rx_status,
	v27tx_status,
	v27rx_status,
	v29tx_status,
	v29rx_status,
	(faxvmi_status_fn)v17tx_status,
	(faxvmi_status_fn)v17rx_status,
};

/*
 * `vxx_process`.  `null_process` is declared `int` (`nulldp.h`); the eight
 * `v??tx_process`/`v??rx_process` are declared `void` (`faxadapt.h`) and need
 * the cast -- see `faxvmi.h` for why the table's own element type is
 * `int`-returning.
 */
faxvmi_process_fn const vxx_process[13] = {
	null_process,
	null_process,
	null_process,
	null_process,
	null_process,
	(faxvmi_process_fn)v21tx_process,
	(faxvmi_process_fn)v21rx_process,
	(faxvmi_process_fn)v27tx_process,
	(faxvmi_process_fn)v27rx_process,
	(faxvmi_process_fn)v29tx_process,
	(faxvmi_process_fn)v29rx_process,
	(faxvmi_process_fn)v17tx_process,
	(faxvmi_process_fn)v17rx_process,
};

/*
 * `vxx_control`, `.rodata` 0x9560 -- the sixth and last 13-slot table, same
 * slot order as every other one (0..4 null, 5 v21tx, 6 v21rx, 7 v27tx,
 * 8 v27rx, 9 v29tx, 10 v29rx, 11 v17tx, 12 v17rx).  `null_control`
 * (`nulldp.h`) and `v27tx_control`/`v27rx_control` (`faxadapt.h`, already
 * untyped `void *req`) match the table's own untyped signature; the other
 * six take a typed request pointer and need the same explicit cast
 * `vxx_status`/`vxx_create` already carry for their own mismatched entries.
 */
faxvmi_control_fn const vxx_control[13] = {
	null_control,
	null_control,
	null_control,
	null_control,
	null_control,
	(faxvmi_control_fn)v21tx_control,
	(faxvmi_control_fn)v21rx_control,
	v27tx_control,
	v27rx_control,
	(faxvmi_control_fn)v29tx_control,
	(faxvmi_control_fn)v29rx_control,
	(faxvmi_control_fn)v17tx_control,
	(faxvmi_control_fn)v17rx_control,
};

/*
 * `FAXVMI_control`'s own quiescent instance -- see `faxvmi.h` for the full
 * derivation.  All 24 bytes are zero in the object.
 */
const struct faxvmi_ctl FAXVMI_CTL = { 0 };

/*
 * `vmi->slot`'s own teardown, then every heap block `FAXVMI_create`
 * allocated, then `vmi` itself -- read straight off `FAXVMI_delete`
 * (0x0953f0): `vxx_delete[slot](link)`, `free(link->ptr_0000)`,
 * `free(link->buf)`, `free(framer->frame)`, `free(framer->fifo)`,
 * `free(link)`, `free(framer)`, tail-call `free(vmi)`.  `link` and `framer`
 * are re-read from `vmi` at each step in the object; caching them once here
 * is equivalent because nothing between the reads writes either field.
 */
void
FAXVMI_delete(struct faxvmi *vmi)
{
	struct faxvmi_link *lk = vmi->link;
	struct faxvmi_framer *fr = vmi->framer;

	vxx_delete[(unsigned short)vmi->slot](lk);
	sysdep_free(lk->ptr_0000);
	sysdep_free(lk->buf);
	sysdep_free(fr->frame);
	sysdep_free(fr->fifo);
	sysdep_free(lk);
	sysdep_free(fr);
	sysdep_free(vmi);
}

/*
 * `FAXVMI_process`, 0x095470.  See `faxvmi.h` for the pipeline (reverse,
 * pack, wrapped-modem process, unpack, reverse, status word); this is the
 * object's own control flow read straight off `dis.py`.
 *
 * `n` DOUBLES AS THE `count` `vxx_process[slot]` IS PASSED BY REFERENCE.  It
 * starts as `vmi_pack`'s own return (`link->pack_count`, truncated to 16
 * bits -- 0x954ba stores only `%ax`), and `vmi_unpack`'s own `count`
 * argument is READ BACK OUT OF THE SAME LOCAL after the `vxx_process` call
 * (0x954e0), not recomputed -- so whatever the table entry left there
 * (zeroed, for every real modulation; untouched, for `null_process`) is what
 * gets unpacked.  D-worthy but already spelled out in `faxvmi.h`'s own
 * comment on the table, so not re-derived here.
 *
 * THE STATUS WORD'S LOW BITS AND BITS 29..31 ARE `ret` UNCHANGED.  Only the
 * five FAXVMI_STATUS_* bits are touched: all five are masked to 0 first
 * (0x9550f), four are conditionally OR'd back in (underrun, full, residue,
 * overflow, in that order -- 0x95515..0x9555b), and the fifth (zero-run) is
 * conditionally AND'd OUT rather than in, which is redundant against the
 * initial mask and reproduced anyway; see `faxvmi.h`.
 */
int
FAXVMI_process(struct faxvmi *vmi, unsigned short *data, short *pcm,
	       short *count, unsigned short *result)
{
	struct faxvmi_framer *fr = vmi->framer;
	unsigned short n;
	int ret, m, status;

	if (vmi->reverse)
		vmi_reverse[vmi->mode](data, *count);

	n = (unsigned short)vmi_pack[vmi->mode](vmi, data, *count);

	ret = vxx_process[(unsigned short)vmi->slot](vmi->link, pcm, &n,
						      result);

	m = vmi_unpack[vmi->mode](vmi, data, (short)n);
	*count = (short)m;

	if (vmi->reverse)
		vmi_reverse[vmi->mode](data, (short)m);

	status = ret & ~(FAXVMI_STATUS_UNDERRUN | FAXVMI_STATUS_FULL |
			  FAXVMI_STATUS_RESIDUE | FAXVMI_STATUS_OVERFLOW |
			  FAXVMI_STATUS_ZERORUN);
	if (vmi->underrun)
		status |= FAXVMI_STATUS_UNDERRUN;
	if (fr->fifo_size - fr->count < vmi->max_frame)
		status |= FAXVMI_STATUS_FULL;
	if (fr->residue)
		status |= FAXVMI_STATUS_RESIDUE;
	if (vmi->overflow)
		status |= FAXVMI_STATUS_OVERFLOW;
	if (fr->zero_run_seen)
		status &= ~FAXVMI_STATUS_ZERORUN;	/* redundant; see above */

	vmi->status = status;
	return status;
}

/*
 * `status->modem_status` is read FIRST, before any other field is touched,
 * and -- when non-NULL -- passed straight to `vxx_status[vmi->slot]` as its
 * own second argument; the return value becomes this function's return
 * value instead of 0.  Every other field is then written unconditionally
 * from `vmi`'s own counters.  `status == NULL` returns -1 without touching
 * anything.  Read off `FAXVMI_status` (0x0955c0).
 */
int
FAXVMI_status(struct faxvmi *vmi, struct faxvmi_status *status)
{
	struct faxvmi_framer *fr;
	int ret = 0;

	if (status == NULL)
		return -1;

	if (status->modem_status != NULL)
		ret = vxx_status[(unsigned short)vmi->slot](vmi->link,
							     status->modem_status);

	fr = vmi->framer;
	status->room = (unsigned short)(fr->fifo_size - fr->count);
	status->residue = fr->residue;
	status->underrun = vmi->underrun;
	status->overflow = vmi->overflow;
	status->zero_run_seen = fr->zero_run_seen;
	status->flag_0014 = (fr->flags_wanted <= 1);

	return ret;
}

/*
 * `FAXVMI_control`, 0x095650.  See `faxvmi.h` for the full six-step
 * derivation; this is the object's own control flow read straight off
 * `dis.py`, not tidied.  The recursion (step 2) happens before anything
 * else so `ret` (the object's own `%edi`) is fixed before the reset/empty
 * steps run, matching the object's own register lifetime rather than being
 * moved for readability.
 */
int
FAXVMI_control(struct faxvmi *vmi, const struct faxvmi_ctl *ctl)
{
	struct faxvmi_framer *fr;
	int ret = 0;
	int i;

	if (ctl == NULL)
		return -1;

	if (ctl->int_0014 != 0)
		ret = vxx_control[(unsigned short)vmi->slot](
			vmi->link, (void *)(long)ctl->int_0014);

	if (ctl->int_000c != 0 && ctl->short_0010 <= 2) {
		fr = vmi->framer;

		if (fr->frame_size != 0) {
			for (i = 0; i < fr->frame_size; i++)
				fr->frame[i] = 0;
		}
		fr->pack_frame_left = 0;
		fr->frame_len = 0;
		fr->flags_wanted = 2;
		fr->ones = 0;
		fr->pack_flagging = 1;
		fr->in_frame = 0;

		fr = vmi->framer;
		fr->pack_bit = 0;
		fr->unpack_bit = fr->pack_bit;		/* dword reload, see
							 * FAXVMI_create's own
							 * comment            */
		fr->short_002e = fr->pad_001e;
		fr->zero_run_bits = 0;
		fr->unpack_mask = 0;
		fr->unpack_word = (unsigned int)-1;
		fr->unpack_acc = (unsigned int)-1;
		fr->async_hunt = 1;
		fr->zero_run_send = 0;
		fr->zero_run_seen = 0;
		fr->pack_mask = 0;
		fr->pack_word = (unsigned int)-1;
		fr->pack_acc = (unsigned int)-1;

		vmi->mode = ctl->short_0010;
	}

	fr = vmi->framer;
	if (ctl->ptr_0000 != NULL) {
		if (fr->fifo_size != 0) {
			for (i = 0; i < fr->fifo_size; i++)
				fr->fifo[i] = 0;
		}
		fr->rd = 0;
		fr->wr = 0;
		fr->count = 0;
		fr->residue = 0;
	}

	fr->zero_run_bits = (unsigned short)ctl->short_0008;
	fr->zero_run_send = ctl->int_0004;

	return ret;
}

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
