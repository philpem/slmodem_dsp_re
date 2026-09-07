/*
 * class1rx.c -- Class 1 fax: the receive-side VMI constructors.
 *
 * Reconstructed from dsplibs.o's class1rx.c span:
 *
 *   init_vmi_v17rx   .text 0x093e80   308
 *   init_vmi_v29rx   .text 0x093fc0   225
 *   init_vmi_v27rx   .text 0x0940b0   230
 *
 * They are written in the object's own emission order, which is v17, v29,
 * v27 and not the numeric one.
 *
 * `_delete_data_rx_modem` (0x0941a0, 149 bytes) is also here now.
 *
 * `_init_receiver` (0x094240, 1,583 bytes) closes the span.  It dispatches
 * on a derived index 0/1/2 (V.27ter/V.29/V.17, `_set_modem_rate`'s own
 * inlined range checks -- no relocation to that shared function appears in
 * this range, so the object genuinely duplicates the logic rather than
 * calling it) between a FRESH-CREATE path (`ctx->modem_vmi == NULL`, or a
 * modulation switch after tearing the old one down) that dispatches through
 * `init_vmi_data_rx_modem[mod]` and `FAXVMI_create`, and a REINIT path
 * (`ctx->current_mod == mod` already) that merges a per-modulation `.data`
 * template (`V17RX_CTL`/`V27RX_CTL`/`V29RX_CTL`, below) with the all-zero
 * `FAXVMI_CTL` and calls `FAXVMI_control` -- which is why `FAXVMI_control`
 * had to land first (`faxvmi.c`) before this function could link at all
 * (F8492/F8493).  See `class1rx.h` for the field-by-field derivation.
 *
 * Read `class1rx.h` for the shape the three share, for why they are global
 * rather than `static`, and for the third parameter that none of them reads.
 *
 * THE FOUR CONSTANTS PLANTED IN THE VMI -- 1, 320, 165 and 0 at +0x04, +0x08,
 * +0x0a and +0x0c -- ARE NOT NAMED, deliberately.  They are `movl $0x1`,
 * `movw $0x140`, `movw $0xa5` and `movw $0x0` in all three functions, so they
 * are neither per-modulation nor derived from anything in scope, and nothing
 * reconstructed reads those fields back.  A name for them would be a guess
 * dressed as a derivation.  The one field that IS named is `slot`, whose
 * three values match `faxvmi.h`'s independently-derived slot map exactly.
 *
 * THE DEBUG STRINGS ARE THE AUTHOR'S OWN WORDS and are reproduced verbatim
 * from `.rodata.str1.4` 0x11878, 0x118b0 and 0x118e8.  "No ECM (Simple
 * Packing)" is what the object says these three constructors build, and it is
 * the strongest evidence in the file about what they are for.
 */

#include <stddef.h>

#include "dsplib/class1.h"
#include "dsplib/class1rx.h"
#include "dsplib/faxcfg.h"
#include "dsplib/faxvmi.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"
#include "dsplib/v17fax.h"
#include "dsplib/v27fax.h"
#include "dsplib/v29fax.h"

void
init_vmi_v17rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v17rx_cfg *cfg = sysdep_malloc(sizeof(struct v17rx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V17_RX Modem No ECM " "(Simple Packing)\n");

	*cfg = V17RX_CFG;
	cfg->ptr_0024 = arg_3;
	cfg->bit_rate = (short)bit_rate;
	cfg->int_0014 = 0;
	cfg->coefsave0 = sysdep_malloc(0x62);
	cfg->coefsave1 = sysdep_malloc(0x62);
	cfg->ratesave = sysdep_malloc(2);

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->mode = 0;
	vmi->reverse = 1;
	vmi->fifo_size = 320;
	vmi->max_frame = 165;
	vmi->frame_size = 0;
	vmi->slot = VMI_SLOT_V17RX;
	vmi->modem_cfg = cfg;
}

void
init_vmi_v29rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v29rx_cfg *cfg = sysdep_malloc(sizeof(struct v29rx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V29_RX Modem No ECM " "(Simple Packing)\n");

	*cfg = V29RX_CFG;
	cfg->bit_rate = (short)bit_rate;
	cfg->ptr_0014 = arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->mode = 0;
	vmi->reverse = 1;
	vmi->fifo_size = 320;
	vmi->max_frame = 165;
	vmi->frame_size = 0;
	vmi->slot = VMI_SLOT_V29RX;
	vmi->modem_cfg = cfg;
}

void
init_vmi_v27rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v27rx_cfg *cfg = sysdep_malloc(sizeof(struct v27rx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V27_RX Modem No ECM " "(Simple Packing)\n");

	*cfg = V27RX_CFG;
	cfg->bit_rate = (short)bit_rate;
	cfg->ptr_0018 = arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->mode = 0;
	vmi->reverse = 1;
	vmi->fifo_size = 320;
	vmi->max_frame = 165;
	vmi->frame_size = 0;
	vmi->slot = VMI_SLOT_V27RX;
	vmi->modem_cfg = cfg;
}

/*
 * Tear the receive-side data modem down: free the config `_init_receiver`
 * built (a V.17 receiver's three sub-allocations first -- `coefsave0`,
 * `coefsave1`, `ratesave`, in that order, none of it read back afterwards),
 * then the config itself, then the VMI block that held it, then the FAXVMI
 * handle at `ctx->vmi_b`.  `ctx->modem_vmi` is cleared BEFORE the
 * `FAXVMI_delete` call and `ctx->vmi_b` AFTER it, which is the object's own
 * order and not incidental: `FAXVMI_delete` is handed the value read out of
 * `ctx->vmi_b` before either field is touched.
 *
 * Every reload of `ctx->modem_vmi`/`vmi->modem_cfg` between frees is the
 * object's own -- `sysdep_free` is an opaque call as far as the compiler
 * knows, so it re-reads both pointers from memory after each one rather than
 * keeping them live across it.  Written the same way here rather than
 * cached in a local that would survive the calls.
 */
void
_delete_data_rx_modem(struct fax_class1 *ctx)
{
	struct faxvmi_cfg *vmi = ctx->modem_vmi;
	struct faxvmi *handle;

	if (vmi->slot == VMI_SLOT_V17RX) {
		struct v17rx_cfg *cfg = vmi->modem_cfg;

		sysdep_free(cfg->ratesave);
		vmi = ctx->modem_vmi;
		cfg = vmi->modem_cfg;
		sysdep_free(cfg->coefsave1);
		vmi = ctx->modem_vmi;
		cfg = vmi->modem_cfg;
		sysdep_free(cfg->coefsave0);
		vmi = ctx->modem_vmi;
	}

	sysdep_free(vmi->modem_cfg);
	vmi = ctx->modem_vmi;
	sysdep_free(vmi);

	handle = ctx->vmi_b;
	ctx->modem_vmi = NULL;
	FAXVMI_delete(handle);
	ctx->vmi_b = NULL;
}

/*
 * `init_vmi_data_rx_modem[mod]`, `.data` 0x7920, 12 bytes -- LOCAL in the
 * object (`d`, `nm`), so `static` here.  Three `R_386_32` relocations,
 * `nm`-resolved: index 0 V.27ter, 1 V.29, 2 V.17 -- the SAME order
 * `_init_receiver`'s own inlined `_set_modem_rate`-shaped derivation below
 * produces.  Independently re-confirmed against `objdump -r` (F10109 first
 * reported this table; not taken on that report alone here).
 */
static void (*const init_vmi_data_rx_modem[3])(struct faxvmi_cfg *,
						unsigned short, int, void *) = {
	init_vmi_v27rx,
	init_vmi_v29rx,
	init_vmi_v17rx,
};

/*
 * The three per-modulation REINIT request templates `_init_receiver`'s
 * reinit path merges with the all-zero `FAXVMI_CTL` before calling
 * `FAXVMI_control`.  Raw bytes taken with `objdump -s -j .data` against
 * `ref/slmodemd/dsplibs.o` and matched field-by-field against each type's
 * own established offsets (`v17fax.h`'s `struct v17rx_ctl`, `v27fax.h`'s new
 * `struct v27rx_ctl`, `v29fax.h`'s `struct v29rx_control_req`).  Zero
 * relocations in any of the three (F10109).
 *
 * `unmapped_0000`/`pad_0000`'s own UPPER 16 bits hold a per-modulation
 * PLACEHOLDER bit rate (0x3840/0x12c0/0x2580 = 14400/4800/9600) that
 * `_init_receiver` overwrites with the live negotiated rate before use
 * (`mov %di,...+2` in the disassembly); the flags/ctl1 byte's REINIT bit is
 * OR'd in at runtime, not baked into the constant.
 */
const struct v17rx_ctl V17RX_CTL = {
	{ 0x00, 0x00, 0x40, 0x38 },	/* unmapped_0000 */
	60000,				/* int_0004      */
	{ 0, 0, 0, 0 },			/* unmapped_0008 */
	0x00,				/* flags_0c      */
	0x00,				/* flags_0d      */
	{ 0, 0 },			/* unmapped_000e */
	0,				/* int_0010      */
};

const struct v27rx_ctl V27RX_CTL = {
	{ 0x00, 0x00, 0xc0, 0x12 },	/* unmapped_0000 */
	60000,				/* int_0004      */
	{ 0, 0, 0, 0 },			/* unmapped_0008 */
	0x00,				/* mask          */
	0x00,				/* flags         */
	{ 0, 0 },			/* unmapped_000e */
	{ 0, 0, 0, 0 },			/* unmapped_0010 */
};

const struct v29rx_control_req V29RX_CTL = {
	{ 0x00, 0x00, 0x80, 0x25 },	/* pad_0000 */
	60000,				/* int_0004 */
	{ 0, 0, 0, 0 },			/* pad_0008 */
	0x00,				/* ctl0     */
	0x00,				/* ctl1     */
};

/*
 * The three FRESH-CREATE finishers.  Each copies `ctx->rx_agc_mult`/`ctx->rx_agc_shift`
 * (16 bits each) into a per-modulation offset of the WRAPPED modem object
 * reached via `vmi_b->link->int_0014` cast to a pointer (the same
 * `(void *)(long)` idiom `faxadapt.c`'s `FIELD_PTR` macro already uses on
 * this exact field, just spelled out here rather than imported) -- but only
 * on the fresh path; the reinit path skips straight to the `f12d4` write.
 * `sub` is at a DIFFERENT offset for each of the three: V.27ter +0x54,
 * V.29 +0x50, V.17 +0x60 -- read straight off `dis.py`
 * (0x9455e-0x9457a, 0x946c2-0x946de, 0x94666-0x94682) and independently
 * re-confirmed for this pass.  Nothing establishes what `sub` or its own
 * +0x8c/+0x8e &c fields ARE beyond this site, so both stay raw offsets.
 *
 * The `f12d4` write and the state/current_mod update happen on BOTH paths
 * and are common to `finish_*rx`.
 */
static void
finish_v27rx(struct fax_class1 *ctx, int fresh)
{
	struct faxvmi_link *link = ctx->vmi_b->link;
	void *wrapped = (void *)(long)link->int_0014;

	if (fresh) {
		void *sub = *(void **)((char *)wrapped + 0x54);

		*(short *)((char *)sub + 0x8c) = (short)ctx->rx_agc_mult;
		*(short *)((char *)sub + 0x8e) = (short)ctx->rx_agc_shift;
		wrapped = (void *)(long)ctx->vmi_b->link->int_0014;
	}

	{
		void *sub = *(void **)((char *)wrapped + 0x50);

		*(short *)((char *)sub + 0x14) = (short)ctx->f12d4;
	}

	ctx->state = CLASS1_RX_LOOK_CARRIER;
	ctx->current_mod = 0;
}

static void
finish_v29rx(struct fax_class1 *ctx, int fresh)
{
	struct faxvmi_link *link = ctx->vmi_b->link;
	void *wrapped = (void *)(long)link->int_0014;

	if (fresh) {
		void *sub = *(void **)((char *)wrapped + 0x50);

		*(short *)((char *)sub + 0x88) = (short)ctx->rx_agc_mult;
		*(short *)((char *)sub + 0x8a) = (short)ctx->rx_agc_shift;
		wrapped = (void *)(long)ctx->vmi_b->link->int_0014;
	}

	{
		void *sub = *(void **)((char *)wrapped + 0x4c);

		*(short *)((char *)sub + 0x1c) = (short)ctx->f12d4;
	}

	ctx->current_mod = 1;
	ctx->state = CLASS1_RX_LOOK_CARRIER;
}

static void
finish_v17rx(struct fax_class1 *ctx, int fresh)
{
	struct faxvmi_link *link = ctx->vmi_b->link;
	void *wrapped = (void *)(long)link->int_0014;

	if (fresh) {
		void *sub = *(void **)((char *)wrapped + 0x60);

		*(short *)((char *)sub + 0xd8) = (short)ctx->rx_agc_mult;
		*(short *)((char *)sub + 0xda) = (short)ctx->rx_agc_shift;
		wrapped = (void *)(long)ctx->vmi_b->link->int_0014;
	}

	ctx->state = CLASS1_RX_LOOK_CARRIER;
	{
		void *sub = *(void **)((char *)wrapped + 0x5c);

		*(short *)((char *)sub + 0x20) = (short)ctx->f12d4;
	}
	ctx->current_mod = 2;
}

/*
 * `_init_receiver`, 0x094240, 1,583 bytes.  See `class1rx.h` for the full
 * derivation; this is the object's own control flow read straight off
 * `dis.py`, not tidied.  `rate_code` is the T.30 modem-rate code
 * `_set_modem_rate` (`class1.c`) already recognises -- the same four V.17
 * two-code ranges and the V.29/V.27ter singles, duplicated inline here
 * rather than called (no relocation to `_set_modem_rate` in this range).
 *
 * `sym` is the value `_sym_size` (`class1.c`) would answer for `rate` --
 * again inlined rather than called -- and it is stored into
 * `vmi_b->link->unpack_width`, a field `faxvmi.h` already names, ONLY on
 * the reinit path, right before the `FAXVMI_control` call.
 */
void
_init_receiver(struct fax_class1 *ctx, int rate_code)
{
	int mod = 0;
	int rate = 0;
	int sym;
	struct faxvmi_cfg *cfg;
	struct faxvmi_link *link;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf(
			"%2d.%02d[sec] Initializing RX modem receiver, " "MODEM_IDX=%d\n",
			ctx->clock_sec, ctx->clock_frac, ctx->current_mod);

	if ((rate_code == 0x4a || rate_code == 0x62 || rate_code == 0x7a ||
	     rate_code == 0x92) && DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Short train in V.17 mode is selected\n");

	/* `_set_modem_rate`'s own shape (class1.c), inlined */
	if ((unsigned)(rate_code - 0x91) <= 1) {
		rate = 0x3840;
		mod = 2;
	}
	if ((unsigned)(rate_code - 0x79) <= 1) {
		rate = 0x2ee0;
		mod = 2;
	}
	if ((unsigned)(rate_code - 0x61) <= 1) {
		rate = 0x2580;
		mod = 2;
	}
	if ((unsigned)(rate_code - 0x49) <= 1) {
		rate = 0x1c20;
		mod = 2;
	}
	if (rate_code == 0x60) {
		rate = 0x2580;
		mod = 1;
	} else if (rate_code == 0x48) {
		rate = 0x1c20;
		mod = 1;
	} else if (rate_code == 0x30) {
		rate = 0x12c0;
		mod = 0;
	} else if (rate_code == 0x18) {
		rate = 0x0960;
		mod = 0;
	}

	if (ctx->current_mod != mod && ctx->modem_vmi != NULL &&
	    ctx->vmi_b != NULL) {
		/* modulation SWITCH: tear the old modem down first */
		cfg = ctx->modem_vmi;

		if (DSPLIB_DEBUG_ON())
			dsplibs_debug_printf(
				"%2d.%02d[sec] New RX Modem... " "Deleting previous existing one\n",
				ctx->clock_sec, ctx->clock_frac);

		if (cfg->slot == VMI_SLOT_V17RX) {
			struct v17rx_cfg *v17 = cfg->modem_cfg;

			sysdep_free(v17->ratesave);
			cfg = ctx->modem_vmi;
			v17 = cfg->modem_cfg;
			sysdep_free(v17->coefsave1);
			cfg = ctx->modem_vmi;
			v17 = cfg->modem_cfg;
			sysdep_free(v17->coefsave0);
			cfg = ctx->modem_vmi;
		}
		sysdep_free(cfg->modem_cfg);
		cfg = ctx->modem_vmi;
		sysdep_free(cfg);

		{
			struct faxvmi *vmi = ctx->vmi_b;

			ctx->modem_vmi = NULL;
			FAXVMI_delete(vmi);
			ctx->vmi_b = NULL;
		}
	}

	if (ctx->modem_vmi == NULL) {
		/* FRESH CREATE */
		ctx->modem_vmi = sysdep_malloc(sizeof(struct faxvmi_cfg));
		init_vmi_data_rx_modem[mod](ctx->modem_vmi,
					    (unsigned short)rate, 0, NULL);

		cfg = ctx->modem_vmi;
		ctx->vmi_b = FAXVMI_create(NULL, cfg);

		if (mod == 2)
			finish_v17rx(ctx, 1);
		else if (mod == 1)
			finish_v29rx(ctx, 1);
		else
			finish_v27rx(ctx, 1);
		return;
	}

	/*
	 * REINIT PATH.  `ctx->vmi_b` is never NULL here: `modem_vmi` and
	 * `vmi_b` are set together (FRESH CREATE, above) and cleared
	 * together (the modulation-switch teardown, above) everywhere in
	 * this function, so reaching this path with `modem_vmi != NULL`
	 * (the `if` just above) means `vmi_b != NULL` too, by that
	 * invariant -- not by a check visible at this point. A whole-
	 * function static analyzer that only sees this branch in isolation
	 * cannot verify that and flags `ctx->vmi_b->link` below as an
	 * unguarded dereference; it is a false positive, not a bug
	 * (checked against FAXVMI_create's own allocation order and this
	 * file's existing differential tests).
	 */
	cfg = ctx->modem_vmi;

	if (DSPLIB_DEBUG_ON())
		dsplibs_debug_printf("Restarting existing RX modem\n");

	cfg = ctx->modem_vmi;

	{
		struct faxvmi_ctl ctl = FAXVMI_CTL;
		void *wrapped;

		if (cfg->slot == VMI_SLOT_V17RX) {
			struct v17rx_ctl req = V17RX_CTL;

			*(short *)((char *)&req + 2) = (short)rate;
			req.flags_0d |= V17RXCTL_REINIT;
			ctl.int_0014 = (int)(long)&req;

			link = ctx->vmi_b->link;
			wrapped = (void *)(long)link->int_0014;
			*(short *)((char *)wrapped + 4) = (short)rate;

			switch (rate) {
			case 0x960:  sym = 2; break;
			case 0x12c0: sym = 3; break;
			case 0x1c20: sym = 3; break;
			case 0x2580: sym = 4; break;
			case 0x2ee0: sym = 5; break;
			case 0x3840: sym = 6; break;
			default:     sym = 0; break;
			}
			link->unpack_width = (unsigned short)sym;

			FAXVMI_control(ctx->vmi_b, &ctl);
			finish_v17rx(ctx, 0);
		} else if (cfg->slot == VMI_SLOT_V29RX) {
			struct v29rx_control_req req = V29RX_CTL;

			*(short *)((char *)&req + 2) = (short)rate;
			req.ctl1 |= V29RXCTL_CTL1_BIT1;
			ctl.int_0014 = (int)(long)&req;

			link = ctx->vmi_b->link;
			wrapped = (void *)(long)link->int_0014;
			*(short *)((char *)wrapped + 4) = (short)rate;

			switch (rate) {
			case 0x960:  sym = 2; break;
			case 0x12c0: sym = 3; break;
			case 0x1c20: sym = 3; break;
			case 0x2580: sym = 4; break;
			case 0x2ee0: sym = 5; break;
			case 0x3840: sym = 6; break;
			default:     sym = 0; break;
			}
			link->unpack_width = (unsigned short)sym;

			FAXVMI_control(ctx->vmi_b, &ctl);
			finish_v29rx(ctx, 0);
		} else {
			struct v27rx_ctl req = V27RX_CTL;

			*(short *)((char *)&req + 2) = (short)rate;
			req.flags |= V27RXCTL_FLAGS_REINIT;
			ctl.int_0014 = (int)(long)&req;

			link = ctx->vmi_b->link;
			wrapped = (void *)(long)link->int_0014;
			*(short *)((char *)wrapped + 4) = (short)rate;

			switch (rate) {
			case 0x960:  sym = 2; break;
			case 0x12c0: sym = 3; break;
			case 0x1c20: sym = 3; break;
			case 0x2580: sym = 4; break;
			case 0x2ee0: sym = 5; break;
			case 0x3840: sym = 6; break;
			default:     sym = 0; break;
			}
			link->unpack_width = (unsigned short)sym;

			FAXVMI_control(ctx->vmi_b, &ctl);
			finish_v27rx(ctx, 0);
		}
	}
}
