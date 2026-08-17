/*
 * v29data.h -- ITU-T V.29 (fax): the transmitter's no-carrier leaf.
 *
 * `TxNoCarrierV29` is `TxNoCarrierV17` with one thing changed, and that one
 * thing is the whole point of keeping the two apart: V.29's shaper is
 * configured with `cfg.mapped` CLEAR, so the ring carries I and Q directly
 * and this function zeroes both rails where V.17 writes a constellation
 * index into `sym`.  Finding 3641 is the pair of independent statements that
 * settle which is which.
 *
 * `tools/service.py` puts it on the FAX side.
 *
 * THE INSTANCE IS NOT MODELLED; the parameter is `void *` and the offsets are
 * named constants, following `include/dsplib/v22data.h`'s ruling.
 *
 * ---------------------------------------------------------------------------
 * THE OFFSETS, CONFIRMED TWICE
 *
 * Once by this function and once by `V29TX_create` (addresses into dsplibs.o):
 *
 *   9bd69  sysdep_malloc(0x9c)  -> obj + 0x24     the transmitter's block
 *   9bd81  sysdep_malloc(0x64)  -> fp + 0x08      100 bytes = 50 shorts, the
 *   9bd93  sysdep_malloc(0x64)  -> fp + 0x0c      ring's two RAILS
 *   9bcba  SMC_init             -> fp + 0x34
 *   9bd57  FPM_PPS_init         -> fp + 0x64, from a configuration whose
 *                                  `mapped` is 0 (xor %ebx,%ebx at 9bce8)
 *                                  and whose coefficients are
 *                                  V29TX_PPS_IFILT / V29TX_PPS_QFILT
 *
 * `struct fpm_smc_ring` at fp + 0x08 puts those two allocations at `i` and
 * `q`, the cursor this function advances at `widx` (fp + 0x14) and its bound
 * at `len` (fp + 0x18).  `sym` (fp + 0x10) is never allocated by the
 * constructor and never read by the shaper on this configuration, which is
 * consistent and is why the direct form has to be the one in use.
 */

#ifndef DSPLIB_V29DATA_H
#define DSPLIB_V29DATA_H

/* The transmitter's private block, 0x9c bytes from `sysdep_malloc`. */
#define V29TX_OBJ_FP		0x24

#define V29FP_SMC_RING		0x08	/* struct fpm_smc_ring              */
#define V29FP_PPS		0x64	/* struct fpm_pps                    */

/*
 * Zero `count` symbol slots on both rails and shape them into `out`,
 * returning the number of samples written.
 *
 * THE SECOND ARGUMENT IS NEVER READ -- nothing at 0x34(%esp) is touched while
 * 0x30, 0x38 and 0x3c are -- so the function takes four arguments and ignores
 * the second, exactly as `TxNoCarrierV17` does.
 *
 * The cursor is written back AFTER the shaper runs, through a fresh read of
 * the instance pointer.  Not observable: `FPM_PPS_filter` writes only `ridx`
 * of the ring it is given.
 */
unsigned short TxNoCarrierV29(void *modem, const unsigned short *data,
			      short *out, unsigned short count);

#endif /* DSPLIB_V29DATA_H */
