# V34 history energy cursor and countdown

Declared before compilation: baseline 902f47fa complete v34filters.c TU,
plus source history-pointer advancement and countdown crossed independently
(four total cells). Blob V34EchoEstimateDelayLineEnergy loads hist+0x10 before
its taps zero guard, advances that cursor two bytes on every iteration, and
uses a decreasing unsigned count (dec/jne). Reconstruction instead indexes
hist by increasing k and tests k against taps. This is a source cursor/count
boundary, not a register permutation. No historical compiled domain was found
for this symbol; F7865 merely listed its size delta.

Preserve unsigned guard, short sample multiplication, wrap-defined accumulator,
empty-input outcome. Both cursor and sample capture occur before arithmetic.
Complete TU baseline must raw-reproduce under current retained profile,
DSPLIB_REPRODUCE_BUGS and actual assembler; record full symbol/data/relocation
and nonexact bystander changes. Close the four-cell domain on a miss; no
padding or arbitrary order synonyms. No runtime harness, mutation or fuzz runs;
parent gates the final batch.

Result: ascending source loop with advancing hist cursor is EXACT53B.
Explicit countdown cells miss; .09.loop says "Can reverse loop", "Reversed
loop", changing source +1 counter to -1. Initial RTL advances hist by2.
4/4 complete-TU metadata/data/nontext/relocation and bystander audits pass;
14/26→15/26 exact, no loss. Adopt only the pointer boundary.
