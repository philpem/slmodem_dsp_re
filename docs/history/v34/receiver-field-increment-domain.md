# Receiver field increment boundary

Baseline04eee73f. A distinct follow-up to shared-counter-results: original
INC/CMP word/store chains do not contain the reconstruction's extra post-INC
MOVZWL. Test the ordinary direct field increment followed by the same wrap
check, retaining all unsigned-short fields, constants, boundary and source
position. Unlike the previous family, no ternary is introduced. This asks
whether the temporary-local truncation boundary, rather than result sharing,
accounts for that instruction. No alternate-width or cast permutations.

For each complete V27rx/V29rx TU: baseline, eq_train only, decision only, both.
Eight cells. Replace only counter-local assignment and its subsequent wrap
stores with ++dec->sym_count and if(field==wrap)field=restart. Remove the now
unused count local only in those functions (there are no other uses). The
counter is incremented/stored before testing at source level; inspect whether
GCC moves that store to the original common sink or preserves a conflicting
order. Any original alias/input read before this block must remain before it.

Prediction: the redundant zero-extension disappears, or the early memory store
falsifies the original ordering. Full-TU raw baseline, all body/data/binding/
relocation controls, gains/losses and stage evidence required. No adoption for
size alone, no more arithmetic spellings if this misses. No fuzz/mutation run.

Post-measurement scope: adopt only the three independently verified counter
components (both V27 methods and V29 eq_train). These reproduce the original
load/INC/word-CMP/normal-store and separate immediate wrap-store sequence,
removing the baseline's extra zero extension. They remain non-exact functions.
V29 decision is not adopted: both controls change the owner register before
the matching store and the deliberately bounded path tracer refuses that case.
