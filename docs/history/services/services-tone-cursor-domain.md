# Service TONE filter cursor and mask conversion

Basee0052eec, sixteen complete TONE TUs. Original TONE_filter traverses both
delay segments with a decreasing pointer, caches coef before the input guard,
and emits an SI destination mask from a HI signed comparison. Compare walking
history cursor with retained indexing, coef capture outside the input loop,
and destination-preserving conditional-zero update crossed with short/SI
index carrier. Keep explicit short narrowing after increment and predicate:
this is conversion boundary recovery, not arbitrary register selection.

The cursor model computes independent segment roots; source pointer decreases
past array origin at the first segment's completion, matching the low-level
original cursor but not a claim about valid abstract-C pointer formation.
These cells are diagnostic and may be declined if no stronger safe source
preimage is established. No near-size adoption, no layout/flag/call changes.
Complete source baseline must reproduce; audit all bodies/data/bindings.
