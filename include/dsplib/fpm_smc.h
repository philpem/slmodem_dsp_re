/*
 * fpm_smc.h -- Fixed Point Modem: Symbol Mapper/Coder.
 *
 * ONLY THE SYMBOL RING IS MODELLED HERE, and only the part of it that two
 * traced functions read.  `fpm_smc.c` itself is unwritten; this header exists
 * because `v22_pps.c` takes one of these as a parameter and a type may be
 * defined in exactly one file.  Putting it in `v22_pps.h` would have made a
 * future `fpm_smc.c` include the V.22 pump's header for its own type, which
 * is backwards.
 *
 * `ModDataV22` (blob 0x8e310) shows what the object is for: it calls
 * `FPM_SMC_encoder(smc, syms, bits, nbits)` and then
 * `V22_PPS_filter(pps, syms, out, nsyms)` on the same second argument, so
 * this is the buffer the symbol coder fills and the modulator drains.  In the
 * V.22 modem both live in the datapump block, the SMC state at +0x48 and this
 * at +0xa0.
 *
 * WHAT EACH FIELD IS EVIDENCED BY.  Nothing below is inferred from a name or
 * from what a ring buffer usually looks like:
 *
 *   +0x08  loaded at `V22_PPS_filter` +0x8c and used at +0xc4 --
 *          `movzbl (%ebx,%edi,2),%ebp`, a BYTE load at `base + rd * 2`, so
 *          the element stride is two and the value used is the low byte.
 *   +0x0e  read at `V22_PPS_filter` +0x84 (`movswl 0xe(%esi),%ecx`) as the
 *          index of that load, and written back at +0x222
 *          (`mov %bx,0xe(%esi)`), advanced by one and wrapped to zero the
 *          moment `idx + 1` reaches +0x10.
 *   +0x10  the bound of that wrap, read at +0x88 as a signed short.  Also
 *          read by `FPM_SMC_encoder` at 0xa9c33.
 *
 * +0x00..+0x07 and +0x0c are NOT established.  `FPM_SMC_encoder` reads +0x0c
 * as a signed short at 0xa9c28, which is where a write cursor would live, but
 * that function has not been traced and a plausible position is not evidence.
 * They are named as padding so that nothing reads meaning into them.
 *
 * `sizeof` IS NOT ESTABLISHED EITHER.  Nothing seen so far allocates one, and
 * the highest byte any traced code touches is +0x11.  The declaration below
 * covers exactly that and no more, so it is a floor and not a measurement.
 */

#ifndef DSPLIB_FPM_SMC_H
#define DSPLIB_FPM_SMC_H

struct fpm_smc_syms {
	unsigned char pad_00[8];	/* +0x00 not established         */
	short *sym;			/* +0x08 one symbol per entry,
					 *       used as its low byte    */
	unsigned char pad_0c[2];	/* +0x0c not established         */
	short rd;			/* +0x0e read cursor             */
	short size;			/* +0x10 wrap bound for `rd`     */
};

#endif /* DSPLIB_FPM_SMC_H */
