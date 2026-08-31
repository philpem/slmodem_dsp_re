/*
 * class1rx.h -- Class 1 fax: the receive-side VMI constructors.
 *
 * The `class1rx.c` span is 2,495 bytes and five symbols:
 *
 *   _init_receiver         .text 0x0941a0  1583   NOT written (343 unwritten)
 *   init_vmi_v17rx         .text 0x093e80   308   written
 *   init_vmi_v27rx         .text 0x0940b0   230   written
 *   init_vmi_v29rx         .text 0x093fc0   225   written
 *   _delete_data_rx_modem  .text 0x094150   149   NOT written (16 unwritten)
 *
 * ALL THREE ARE `t` IN THE OBJECT -- file-local -- AND ARE GLOBAL HERE.  Only
 * `_init_receiver` calls them, and it is 343 unwritten symbols away, so a
 * `static` spelling would be a function this tree could neither reach nor
 * test.  This is wave 2's `v22_delete` again, and it is deviation D1081: the
 * storage class is ours, not the author's, and the pass that writes
 * `_init_receiver` should take it back to `static`.
 *
 * WHAT THE THREE HAVE IN COMMON, which is nearly everything:
 *
 *   1. allocate a modem configuration of exactly `sizeof` its table;
 *   2. announce themselves at debug level > 2;
 *   3. copy the table over the allocation, then override the bit rate from
 *      the caller and store the caller's fourth argument in the LAST field;
 *   4. copy `FAXVMI_CFG` over the head of the caller's VMI, then override
 *      seven of its nine fields -- including `slot`, which is the only thing
 *      that differs between the three: 12, 8, 10 for V.17, V.27ter, V.29.
 *
 * V.17 does one thing more: it clears +0x14 and installs three further
 * allocations of 0x62, 0x62 and 2 bytes.  The two 0x62s are the same size and
 * are NOT interchangeable -- they are stored at different offsets and a test
 * that only checked "both non-null" would not see them swapped, so
 * `t_faxcfg.c` checks that all three are distinct addresses.
 *
 * NEITHER THE MALLOC RESULT NOR THE VMI POINTER IS CHECKED FOR NULL, in any
 * of the three.  That is the object's, reproduced.
 *
 * THE THIRD PARAMETER IS NEVER READ.  All three functions load their first,
 * second and fourth arguments and leave the slot at `esp+0x28` alone.  It is
 * declared here so the ABI matches and named for what is known about it,
 * which is nothing.
 */

#ifndef DSPLIB_CLASS1RX_H
#define DSPLIB_CLASS1RX_H

struct faxvmi_cfg;

/*
 * `bit_rate` is loaded with `movzwl` from a 32-bit argument slot, so the
 * declared parameter is 16 bits wide; the extension is dead (only the low
 * half is ever stored) and so carries no signedness claim of its own -- see
 * finding F614's rule.  `unsigned short` is the spelling that matches the
 * encoding the object chose.
 */
void init_vmi_v17rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		    int arg_2, void *arg_3);
void init_vmi_v27rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		    int arg_2, void *arg_3);
void init_vmi_v29rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		    int arg_2, void *arg_3);

/* The `slot` each of the three plants, from `faxvmi.h`'s slot map. */
#define VMI_SLOT_V27RX		8
#define VMI_SLOT_V29RX		10
#define VMI_SLOT_V17RX		12

#endif /* DSPLIB_CLASS1RX_H */
