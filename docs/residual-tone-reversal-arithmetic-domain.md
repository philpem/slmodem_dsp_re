# Arithmetic-only sample ownership, retaining the alias-visible final reload

The opening capture hypothesis in residual-tone-reversal-domain.md is refuted
at the final history store: original0xab245 reloads samples[n] AFTER rev_age
store0xab239. Retaining it across the store is not supported. Previous four
measurements remain valid compiler controls, but no full capture is adopted.

Fresh independent read boundaries: original0xab295 captures current sample and
0xab29a captures outgoing hist[idx], reused in correlation and squares through
0xab2f0..0xab2f9. Both arithmetic reads precede saturation/age changes. Preserve
the final samples[n] reload. Four cells with sequential energy updates fixed:
no captures; arithmetic-only current capture; outgoing short capture; both.
Capture at existing first arithmetic read, short type unchanged. No loads moved
across filter call, ring-index update, or state write. Unsaturated int sums,
shifted-square boundaries, short conversions and threshold remain unchanged.

Raw sequential-only repeat required; full TU audit. This closes on misses, no
further register/color/local-order permutations. Explicit captures are source
hypotheses from read/reuse witnesses, not proof of a unique author spelling.
No source retention without supported recovery and period gate. No fuzzing or
mutation execution.
