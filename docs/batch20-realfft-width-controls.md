# realfft separation temporary width control

Baseline 93d7eee1, retained full Gentoo C++ profile. Only two complete-TU
cells: unchanged source and `h1r,h1i,h2r,h2i` declared float rather than double.
The source commentary/F1354 records plain float expressions and removal of
explicit double operand casts. The Numerical Recipes form likewise groups
these temporaries with float c1/c2. This is a distinct assignment-width
boundary, not a reordered arithmetic search. Blob has no intermediate memory
narrowing in the separation loop; that alone cannot prove the declared width.
Baseline realfft is 525 vs blob505 bytes, four1 is exact365. Stop this domain
if the float boundary does not recover the full body; no adjacent width or
arbitrary declaration permutations inferred from a size change.

Require raw unchanged full-TU reproduction, bug define, actual assembler,
RTL dumps, all bodies/data/metadata/relocations audit. No harness execution;
batch owner gates once at the end.

## Result

2/2 valid complete TUs, raw unchanged baseline reproduction. Float separation
recovers realfft505B exactly, four1 retains its complete365B exact
body. Initial RTL gives every named temporary DF in baseline and SF in the
candidate, so the source boundary detector fires. All nontext bytes,
relocation records, named objects, bindings, visibility and allocated section
sizes are identical; only realfft changes. Width adopted; batch-end period
validation remains the owner responsibility.

No standalone memory narrowing in the blob does not imply double source:
GCC3 keeps plain float assignments in excess-precision registers here. The
assignment width nevertheless changes its internal expression modes and
register-stack lifetime, accounting for the different wr spill/initialization
and twenty extra bytes. F1354 already excluded widening the input expressions;
this corrects the remaining widened assignment destinations.

Reproduce with tools/gcc3_batch20_fft_width_reproduce.py --domain this file;
audit with tools/gcc3_batch20_cpp_boundaries_audit.py after both domains run.
