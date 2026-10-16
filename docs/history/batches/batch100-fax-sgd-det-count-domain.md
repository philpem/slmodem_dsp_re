# SGD detector count conditions

Pinned 856c1ecb, complete Gentoo period full TU control. Original SGD_sequence_det has three unsigned-word countdowns: history slide 0x9f550..56a, input append 0x9f56c..588, correlation 0x9f5d8..607. Each decrements a promoted count and tests the incremented low word for zero; the current source spells explicit 0xffff sentinels. Batch50 closed only SGD_correlate's signed count and SGD_pattern_det; sequence_det's unsigned loops were not included.

Predeclare four cells: baseline; replace both copy loops with unsigned-short postdecrement; replace only inner correlation loop; cross both classes. Initialize each count from exactly its old pre-decrement expression's value before minus one. All pointer advances, narrow accumulators, outer signed n guard, best-index uninitialized behavior, short base wrap, ties and status arithmetic remain unchanged. Count zero executes zero bodies in both forms, and each nonzero unsigned count executes exactly that count. Full TU bodies, exports, metadata, named data and nontext canonical relocations must be audited. No index/register/order expansion without a new witness.

## Measured outcome

- baseline: {}.
- copy-counts: {}.
- correlation-count: {'SGD_sequence_det': ['SIZE', 12]}.
- both-count-classes: {'SGD_sequence_det': ['SIZE', 12]}.

No exact gain or loss; no source adopted. Full-TU baseline, complete flags/identities and raw objects retained; all bodies and canonical nontext audited.
