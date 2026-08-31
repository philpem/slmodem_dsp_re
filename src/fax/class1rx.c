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
 * `_init_receiver` (0x0941a0, 1,583 bytes) and `_delete_data_rx_modem`
 * (0x094150, 149) are the rest of the span and are NOT here: the first still
 * needs 343 unwritten symbols and the second 16, and a reference from `src/`
 * to an unwritten blob symbol fails the whole suite at link (F8492, F8493).
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

#include "dsplib/class1rx.h"
#include "dsplib/faxcfg.h"
#include "dsplib/debug.h"
#include "dsplib/sysdep.h"

void
init_vmi_v17rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
	       int arg_2, void *arg_3)
{
	struct v17rx_cfg *cfg = sysdep_malloc(sizeof(struct v17rx_cfg));

	(void)arg_2;

	if (DSPLIB_DEBUG_VERBOSE())
		dsplibs_debug_printf(
			"Initializing VMI_V17_RX Modem No ECM "
			"(Simple Packing)\n");

	*cfg = V17RX_CFG;
	cfg->bit_rate = bit_rate;
	cfg->ptr_0024 = arg_3;
	cfg->int_0014 = 0;
	cfg->ptr_0018 = sysdep_malloc(0x62);
	cfg->ptr_001c = sysdep_malloc(0x62);
	cfg->ptr_0020 = sysdep_malloc(2);

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->short_0000 = 0;
	vmi->int_0004 = 1;
	vmi->short_0008 = 320;
	vmi->short_000a = 165;
	vmi->short_000c = 0;
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
			"Initializing VMI_V29_RX Modem No ECM "
			"(Simple Packing)\n");

	*cfg = V29RX_CFG;
	cfg->bit_rate = bit_rate;
	cfg->ptr_0014 = arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->short_0000 = 0;
	vmi->int_0004 = 1;
	vmi->short_0008 = 320;
	vmi->short_000a = 165;
	vmi->short_000c = 0;
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
			"Initializing VMI_V27_RX Modem No ECM "
			"(Simple Packing)\n");

	*cfg = V27RX_CFG;
	cfg->bit_rate = bit_rate;
	cfg->ptr_0018 = arg_3;

	*vmi = FAXVMI_CFG;
	vmi->ptr_0014 = arg_3;
	vmi->short_0000 = 0;
	vmi->int_0004 = 1;
	vmi->short_0008 = 320;
	vmi->short_000a = 165;
	vmi->short_000c = 0;
	vmi->slot = VMI_SLOT_V27RX;
	vmi->modem_cfg = cfg;
}
