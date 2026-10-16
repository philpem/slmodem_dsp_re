# SDM initializer dataflow cross

Baseline 9e20a346 (production source identical to merged736cf61c). Both
FPM_SDM_init and SDM_init are86-byte BYTES18, with the same original machine
body. Original cfg-copy precedes one HI nbits read reused for mask and both
shifts; shift2 is computed/stored before mask/notmask and shift1. Retained source
puts shift2 last; GCC schedules it early anyway but chooses the opposite two
callee-saved colors and a different prologue save/load interleave.

Previous F10216 controls reversed the opening reg/cfg stores and changed
optimized-away pointer alias declaration order; those remain closed. This
new domain leaves the opening reg clear and aggregate cfg assignment intact.
Cross retained derived-field order / original observed shift2-first order with
repeated cfg.nbits expressions / one existing-type short nbits capture after
cfg copy. No source type, field width, arithmetic operator, literal or flag
changes. Capture type is cfg.nbits's existing short type, not a guessed width.
All three field reads are after the cfg copy; derived fields cannot alias cfg.
No runtime/lifecycle claim is made for undefined shift inputs.

Four cells per complete TU, two TUs. Require raw baseline reproduction, all
bystanders and metadata/data/binding review, then trace actual lreg/greg and
peephole/rnreg changes. A closer byte score is not adoption; require exact
original body and independent source factoring rationale. Close this finite
cross on misses. No return/prototype/scope synonyms or opening-store retry.
No mutation/fuzzing execution. Adoption, if supported, needs production census
and batch-end period/structural gate.
