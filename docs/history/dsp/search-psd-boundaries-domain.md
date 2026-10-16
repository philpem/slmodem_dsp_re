# Search-entry and PSD normalization boundaries

Baseline d13949ad, complete current TUs/headers, fixed Gentoo profile and bug
define. Require raw unchanged baseline reproduction and all-body audit. No
fuzz/mutation execution or production option change. Eight compiled controls:
two independent two-axis crosses, four cells each.

Mapping original first tests j<count before loading constellationSize[i]; current
source places that load before the search loop. Test moving length's declaration
and initialization into the j-loop before k. Preserve all compare/zero-length
semantics. Separately original unmatched tail stores distinctIndex[count], then
increments/publishes count, decrements a copy and stores group[i]. Test
distinctIndex[count++]=i; group[i]=count-1. Cross with delayed length. Prediction:
the first recovers guarded load/inner-search entry; the second recovers count
publication/subtraction. Falsifier: changes erased or extra unsupported operations.
No outer unroll, local-order, type, read barrier or flag controls.

PSD original all three bin loops test their bound before reciprocal/conversion;
peak has a direct fcom against retained peak rather than the candidate copy/fcomp.
Test division written in the loop expression instead of precomputed scale in
all three arms, preserving long-double operands and logarithm float narrowing.
Separately test float peak=0.0f: all assigned values are float bins (or exact
zero), independently bounding value precision; do not infer a unique original
type from this. Cross the two axes. Prediction: loop-expression division hoists
to the guarded preheader and preserves zero-bin no-evaluation; float peak may
recover the original comparison stack ownership. Falsifier: changed arithmetic
precision/NaN decision or unsupported reciprocal/rounding graph. No multiply
operand, accumulation type, option-order or declaration permutations.

Retained GCC RTL diagnoses these tested transformations, not unavailable
original RTL. Inspect whole bodies/callers, metadata/data, and original operation
graphs. Exact score alone cannot select a control; supported partial recovery
is allowed. Batch-end period/structural/census/partial-link before commit/push.
