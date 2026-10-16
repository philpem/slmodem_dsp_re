# SDM configuration capture order

Declare raw902 baseline, compound/postincrement recovered seed, and seed
with cached configuration capture in field-offset order: nbits, mask, notmask,
shift1, shift2, reg. Reference prologue loads precisely this order; seed loads
nbits, shift1, shift2, mask, notmask, reg. This is one independently witnessed
configuration boundary, not enumerating store or definition permutations.
Retain all types/expressions and unchanged complete TU baseline. Audit initial
RTL and full bystanders/metadata/data/nontext relocations. No broader order
domain if this control fails.

Measured result: 3cells field-order control168B/SIZE1, no gain. Closed one witnessed order, no permutations. All thirteen SDM TUs pass complete data/nontext/relocation/metadata/bystander audit; only descrambler changes, zero exact losses. No adoption.
