/*
 * faxvmi.c -- Class 1 fax: the VMI's core object and its framing dispatch.
 *
 * Reconstructed from dsplibs.o.  The object splits this machine across eight
 * translation units; this one is the core, and the framing codecs it
 * dispatches to are in their own blob FILEs (finding F11399):
 *
 *   FAXVMI_create      .text 0x095120
 *   FAXVMI_delete      .text 0x0953f0
 *   FAXVMI_process     .text 0x095470
 *   FAXVMI_status      .text 0x0955c0
 *   FAXVMI_control     .text 0x095650
 *   FAXVMI_message     .text 0x0957b0
 *   FAXVMI_CFG         .rodata 0x009490
 *   FAXVMI_CTL         .rodata 0x009478
 *   FAXVMI_STS         .rodata 0x00945c
 *   vmi_unpack/pack/reverse .rodata 0x0094a8/0x0094b4/0x0094c0
 *
 * The codecs (`faxvmi_asyc.c`, `faxvmi_hdlc.c`, `faxvmi_pack.c`,
 * `faxvmi_utls.c`, `faxvmififo.c`) and the null datapump (`faxvmi_null.c`)
 * are the other FILEs of the same family; see faxvmi.h for the model.
 *
 * Differential test: test/unit/t_nulldp.c and the fax tier.
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
 * THEY ARE FILE-LOCAL IN THE OBJECT (`r`, not `R`), so all three are
 * `static` here now -- `FAXVMI_process` is the reader, so the referent is
 * in this TU and the compiler keeps them.  D1122 recorded the earlier
 * `extern` spelling while `FAXVMI_process` was still unwritten.
 */
static faxvmi_frame_fn const vmi_unpack[3] = {
	faxvmi_simp_unpack,
	faxvmi_asyc_unpack,
	faxvmi_hdlc_unframe,
};

static faxvmi_frame_fn const vmi_pack[3] = {
	faxvmi_simp_pack,
	faxvmi_asyc_pack,
	faxvmi_hdlc_frame,
};

static faxvmi_reverse_fn const vmi_reverse[3] = {
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
