/*
 * fpm.h -- fixed-point maths kernels used by the datapumps.
 *
 * The `fpm_*` layer is Q15 fixed point throughout, so every function here is
 * held to bit-exact equivalence with the original -- no tolerance.
 */

#ifndef DSPLIB_FPM_H
#define DSPLIB_FPM_H

/*
 * Q15 square root.  Input and output are unsigned Q15 fractions.
 *
 * Defined for 0x0000..0x7fff.  The original reads past its lookup table for
 * larger inputs; this version clamps.  See src/dsp/fpm_sqrt.c.
 */
unsigned short FPM_sqrt(unsigned short x);

/* Table introspection, for the generator self-check in the unit tests. */
unsigned short FPM_sqrt_table_generate(int index);
unsigned short FPM_sqrt_table_entry(int index);
int FPM_sqrt_table_size(void);

#endif /* DSPLIB_FPM_H */
