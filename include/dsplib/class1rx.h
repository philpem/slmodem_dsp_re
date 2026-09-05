/**
 * @file class1rx.h
 * @brief Class 1 fax: the receive-side VMI constructors.
 *
 * The `class1rx.c` span is 2,495 bytes and five symbols, in the object's own
 * order (from `nm -S`, not a span listing -- the two addresses this file
 * first carried were wrong in exactly this ordering, F8535):
 *
 *   init_vmi_v17rx         .text 0x093e80   308
 *   init_vmi_v29rx         .text 0x093fc0   225
 *   init_vmi_v27rx         .text 0x0940b0   230
 *   _delete_data_rx_modem  .text 0x0941a0   149
 *   _init_receiver         .text 0x094240  1583
 *
 * All five are now written (`_init_receiver` and its transmit-side twin
 * landed once `FAXVMI_control`/`vxx_control` unblocked them, F10115/F10117).
 *
 * All three constructors and `_delete_data_rx_modem` are `t` in the object
 * (file-local) and are declared `extern` here instead: their only caller,
 * `_init_receiver`, is in the same translation unit, so a `static` spelling
 * would be functions this tree could neither reach nor test on their own.
 * This is deviation D1081 -- the storage class is ours, not the author's --
 * and the pass that takes `_init_receiver` apart from its own file (should
 * that ever happen) is what should take these back to `static`.
 *
 * What the three constructors have in common, which is nearly everything:
 *
 *   1. allocate a modem configuration of exactly `sizeof` its table;
 *   2. announce themselves at debug level > 2 ("Initializing VMI_V*_RX
 *      Modem No ECM (Simple Packing)" -- the object's own words, the
 *      strongest evidence here for what they are for);
 *   3. copy the table over the allocation, then override the bit rate from
 *      the caller and store the caller's fourth argument in the last field;
 *   4. copy `FAXVMI_CFG` over the head of the caller's VMI, then override
 *      seven of its nine fields -- including `slot`, the only field that
 *      differs between the three (12, 8, 10 for V.17, V.27ter, V.29).
 *
 * V.17 does one thing more: it clears +0x14 and installs three further
 * allocations of 0x62, 0x62 and 2 bytes. The two 0x62s are distinct
 * allocations at different offsets, not interchangeable, so `t_faxcfg.c`
 * checks all three addresses are distinct rather than just non-null.
 *
 * Neither the malloc result nor the VMI pointer is null-checked, in any of
 * the three -- the object's own omission, reproduced.
 *
 * The third parameter is never read by any of the three: they load their
 * first, second and fourth arguments and leave the slot at `esp+0x28`
 * alone. Declared here so the ABI matches; named for what is known about
 * it, which is nothing.
 */

#ifndef DSPLIB_CLASS1RX_H
#define DSPLIB_CLASS1RX_H

struct faxvmi_cfg;

/**
 * @brief Build a "No ECM (Simple Packing)" V.17 receive VMI/config pair.
 *
 * Allocates a `struct v17rx_cfg`, copies `V17RX_CFG` over it, sets
 * `bit_rate` and `ptr_0024 = arg_3`, clears `int_0014`, and allocates three
 * further sub-blocks (0x62, 0x62, 2 bytes) into `ptr_0018`/`ptr_001c`/
 * `ptr_0020`. Copies `FAXVMI_CFG` over `*vmi`, overrides seven fields
 * (`ptr_0014 = arg_3`, the four VMI constants, `slot = VMI_SLOT_V17RX`,
 * `modem_cfg = cfg`).
 *
 * @param vmi       The VMI config block to build in place.
 * @param bit_rate  16 bits wide in the object (`movzwl` from a 32-bit
 *                  argument slot; the widening is dead, so the parameter's
 *                  own signedness is unconstrained -- `unsigned short` is
 *                  the spelling that matches the encoding chosen (F614).
 * @param arg_2     Unread.
 * @param arg_3     Stored into both the modem config and the VMI block.
 */
void init_vmi_v17rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		    int arg_2, void *arg_3);

/**
 * @brief Build a "No ECM (Simple Packing)" V.27ter receive VMI/config pair.
 *
 * Allocates a `struct v27rx_cfg`, copies `V27RX_CFG` over it, sets
 * `bit_rate` and `ptr_0018 = arg_3`. Copies `FAXVMI_CFG` over `*vmi` and
 * overrides the same seven fields as init_vmi_v17rx(), with
 * `slot = VMI_SLOT_V27RX`.
 *
 * @param vmi       The VMI config block to build in place.
 * @param bit_rate  See init_vmi_v17rx()'s note on this parameter's width.
 * @param arg_2     Unread.
 * @param arg_3     Stored into both the modem config and the VMI block.
 */
void init_vmi_v27rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		    int arg_2, void *arg_3);

/**
 * @brief Build a "No ECM (Simple Packing)" V.29 receive VMI/config pair.
 *
 * Allocates a `struct v29rx_cfg`, copies `V29RX_CFG` over it, sets
 * `bit_rate` and `ptr_0014 = arg_3`. Copies `FAXVMI_CFG` over `*vmi` and
 * overrides the same seven fields as init_vmi_v17rx(), with
 * `slot = VMI_SLOT_V29RX`.
 *
 * @param vmi       The VMI config block to build in place.
 * @param bit_rate  See init_vmi_v17rx()'s note on this parameter's width.
 * @param arg_2     Unread.
 * @param arg_3     Stored into both the modem config and the VMI block.
 */
void init_vmi_v29rx(struct faxvmi_cfg *vmi, unsigned short bit_rate,
		    int arg_2, void *arg_3);

/** The `slot` each of the three constructors above plants, from `faxvmi.h`'s slot map. */
#define VMI_SLOT_V27RX		8
#define VMI_SLOT_V29RX		10
#define VMI_SLOT_V17RX		12

struct fax_class1;

/**
 * @brief Tear the receive-side data modem down.
 *
 * Frees the config one of the three constructors above built (a V.17
 * receiver's three sub-allocations first, in `ptr_0018`/`ptr_001c`/
 * `ptr_0020` order), then the config itself, the VMI block that held it, and
 * finally the FAXVMI handle at `ctx->vmi_b`. `ctx->modem_vmi` is cleared
 * before the `FAXVMI_delete` call and `ctx->vmi_b` after it -- the object's
 * own order, not incidental: `FAXVMI_delete` is handed the value read out of
 * `vmi_b` before either field is touched.
 *
 * @param ctx  The fax session whose current data receiver is being torn down.
 */
void _delete_data_rx_modem(struct fax_class1 *ctx);

/**
 * @brief Build or reinitialise the data-mode receiver for a T.30 rate code.
 *
 * `.text` 0x094240, 1,583 bytes -- see `class1rx.c` for the full
 * derivation. Dispatches on the modulation `_set_modem_rate`'s own code
 * space selects (V.27ter/V.29/V.17, `class1.c`, duplicated inline here
 * rather than called) between a fresh-create path (`ctx->modem_vmi ==
 * NULL`, or after tearing down a modulation switch) that builds a new VMI
 * through one of the three constructors above and `FAXVMI_create`, and a
 * reinit path (already on this modulation) that merges a per-modulation
 * `.data` template (`V17RX_CTL`/`V27RX_CTL`/`V29RX_CTL`) into the all-zero
 * `FAXVMI_CTL` and sends it through `FAXVMI_control` -- which is why
 * `FAXVMI_control` had to land first before this function could even link
 * (F8492/F8493).
 *
 * @param ctx        The fax session.
 * @param rate_code  A T.30 modem-rate code, the same code space
 *                   `_set_modem_rate` (`class1.c`) recognises.
 */
void _init_receiver(struct fax_class1 *ctx, int rate_code);

#endif /* DSPLIB_CLASS1RX_H */
