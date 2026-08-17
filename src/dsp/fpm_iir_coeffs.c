/*
 * fpm_iir_coeffs.c -- Fixed Point Modem: shared IIR coefficient sets.
 *
 * DELIBERATELY EMPTY.  This file held `COEF_DC`, which has moved to
 * `src/dsp/fpm_mtd.c`; the derivation is in the comment beside the definition
 * there and in finding 3621.  In short: the object's `.data` order is the
 * link order, `COEF_DC` at 0x081d0 sits immediately after `DEF_COEFS` at
 * 0x081bc (a `fpm_mtd.c` local, so its translation unit is known and not
 * guessed), and `fpm_iir.c` is STT_FILE #619 against `fpm_mtd.c`'s #623 --
 * so an IIR-side home for it is not merely unattested but ruled out.  Its one
 * reference in the object is inside `FPM_MTD_detect`.
 *
 * The file is kept rather than deleted because it was never one of the
 * object's 283 translation units in the first place -- it was our factoring,
 * and this note is the record of why that factoring was wrong.  It emits no
 * `.data`, so it cannot come between `fpm_mtd.c` and `fpm_phasor.c` in the
 * link, which is what `FPM_phasor`'s out-of-domain SINE sign lookup reads
 * across.  That adjacency is reproduced and deliberately not asserted --
 * finding 3624 and D392 for what displaces it.
 *
 * `docs/coefficients.md` remains the register of recovered designs.
 */
