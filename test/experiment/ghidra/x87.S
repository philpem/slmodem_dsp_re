/* Synthetic Ghidra calibration: preserve these instruction sequences.
 * This is analysis apparatus, not reconstructed modem source.
 * The first three functions round the product to float; wide does not. */
.text
.macro NARROW name, op
.globl \name
.type \name,@function
\name:
 subl $4,%esp
 flds 12(%esp)
 fmuls 8(%esp)
 fstps (%esp)
 flds (%esp)
 \op 16(%esp)
 popl %eax
 ret
.size \name,.-\name
.endm
NARROW calibration_nsub,fsubs
NARROW calibration_nadd,fadds
NARROW calibration_ndiv,fdivs
.globl calibration_wide
.type calibration_wide,@function
calibration_wide:
 flds 8(%esp)
 fmuls 4(%esp)
 fsubs 12(%esp)
 ret
.size calibration_wide,.-calibration_wide
.section .note.GNU-stack,"",@progbits
