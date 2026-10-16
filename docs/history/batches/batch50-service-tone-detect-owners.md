# Tone detector coefficient/state owners

Four cells: raw902 plus independently captured inline coefficient pointer
and state pair pointer, crossed. Blob prologue computes t+0x48 and t+0x54
into distinct lifetime owners before loop, and uses offsets0/4/8 and0/4 in
its recurrence. Current source names each struct member directly. Capture
const float *det_coef=t->det_coef and float *det_state=&t->det_z1 before loop;
replace only corresponding field accesses. Keep all arithmetic order/float
narrowing/counter/return semantics, no flags/header edits. TONE_kill recent
coefficient-scope domain is closed and not repeated; this detector captures
inline coefficients and a separate adjacent state pair. CompleteTU raw902
using historical headers because independent cosine header changed. No harness,
mutation or fuzzing; metadata/data/nontext/relocs/allbystanders audited.

Measured result: 4cells bestSIZE29, no gain; both owners required to approach shape. CompleteTU proof included tools/gcc3_batch50_integer_audit.py124/124; no adoption.
