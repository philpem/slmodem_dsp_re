# Retain supported constellation clearing-counter types

Baseline347b5b89, fixed Gentoo profile/bug define. Replay the already measured
four cells independently crossing unsigned eight-word clearing counters in
getConstellationMask/getCodecConstellationMask against the current complete TU.
The prior domain closed exactness, not the source-type witness: original CMP7/JBE
and current CMP7/JLE still differ. Do not reopen index/cursor/scope permutations.

Adopt the combined unsigned counter only if it restores both original branch
operations, its inline caller changes exactly those same two predicates, and
all remaining bodies, symbol/data/relocation metadata remain unchanged. These
are supported partial improvements under the owner's current scope; no unique
original spelling or whole-function identity is inferred. Signed which/index
and unsigned data-loop counter remain untouched. All clearing counter values
0..8 are representable identically, so the eight writes and termination are
unchanged for every input. Require integrated raw-object equality to the
independent candidate, no whole-tree exact losses, and final make phase.
