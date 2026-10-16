# getPower stack allocation and nomination coverage

Baseline f420cd89, current complete V90ConstellationPower TU and headers;
retained Gentoo configuration and mandatory bug define. Original and current
getPower are574B with ten exchanged double stack operands at0x28/0x30. The
only other disassembly-row difference is a jump table's section offset. All
six original/current destinations are already independently verified at
instruction boundaries:275,304,336,368,400,42.

First reproduce one unchanged full TU raw, then observe installed cc1plus
stack requests read-only. Require saved/plain/observed assembly and objects
to agree raw. Identify whether the double homes come from expansion locals,
temporary assignment or reload spills before generating source variants.
Map requests back to named RTL and final frame offsets; source declaration
order alone is not the explanation. No register/stack constraints or flags.

Prediction: the ten changes are a consistent slot bijection once the actual
six-case jump-table identity is verified. Falsifier: any destination/operand
differs or the bijection is inconsistent. Repair nomination only after this
positive fires; a changed table target must remain a negative. Never relax
the strict byte comparator or normalize arbitrary section relocations.

New source controls require a separately recorded allocation/lifetime witness.
Audit the whole TU, exact helper, other bodies, bindings/data and table targets.
Any retained source needs batch-end period/structural validation, full census
and partial-link comparison. No fuzzing/mutation execution.

## Observed allocation and three-cell source control

The unchanged full TU and read-only observer agree raw. Six getPower requests
include two DF reload spills from alter_reg: pseudo63 power at frame-16, then
pseudo85 fraction at-24. They become stack+0x30/+0x28 respectively, opposite
the original. This is observed reload allocation, not entry-local reservation.

Test exactly baseline, fraction lifted to function scope after power, and
fraction lifted before power. Keep power initialization before the helper call,
all arithmetic/rounding, branch assignments and calls unchanged. Fraction is
assigned on every path before consumption and its address is never taken.
Prediction: fraction-first creates its pseudo earlier and exchanges the two
reload homes; power-first separates scope from order. Falsifier: either the
homes do not exchange or additional operations prevent identity. This finite
scope control does not authorize unrelated declaration permutations. Verify
all nine bodies and six table destinations, then raw-equal stack observation
on every cell and an exact helper control.
