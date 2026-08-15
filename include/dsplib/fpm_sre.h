/*
 * fpm_sre.h -- Fixed Point Modem: Symbol-timing REcovery.
 *
 * Only the V.32 data tables so far.  The block's configuration and state
 * structs are deliberately absent: FPM_SRE_init copies the configuration with
 * a 14-dword `rep movsl` and reads just three shorts out of it, so the other
 * field boundaries are inferred from how the constants pack rather than
 * measured, and nothing can settle them until FPM_SRE_recover is read.
 * Finding 1615.
 *
 * Reconstructed from dsplibs.o:
 *   SREv32_COFFS     .rodata 0x06ea0  362 B
 *   SREv32_PLL_K2    .data   0x07600    6 B
 *   SREv32_PLL_K1    .data   0x07606    6 B
 *   SREv32_XB_COFFS  .data   0x0760c   22 B
 *   SREv32_yCLOCK    .data   0x07622    6 B
 *   SREv32_xCLOCK    .data   0x07628    6 B
 */

#ifndef DSPLIB_FPM_SRE_H
#define DSPLIB_FPM_SRE_H

/*
 * The interpolation filter.  181 entries, and that width is MEASURED:
 * FPM_SRE_init copies it out with `movzwl (%ecx,%edx,2)`.  Its length is
 * carried separately in the configuration as 180, one short of the table.
 */
extern const short SREv32_COFFS[181];

/*
 * The remaining five are read only by FPM_SRE_recover, which is not
 * reconstructed, so their ELEMENT WIDTH IS UNVERIFIED.  They are not const:
 * all five live in .data.
 */
extern short SREv32_XB_COFFS[11];
extern short SREv32_PLL_K1[3];
extern short SREv32_PLL_K2[3];
extern short SREv32_xCLOCK[3];
extern short SREv32_yCLOCK[3];

#endif /* DSPLIB_FPM_SRE_H */
