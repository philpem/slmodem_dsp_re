# Batch 5: V34 nonlinear encoder arithmetic lifetimes

Baseline 80c5dea3, retained complete Gentoo GCC 3.4.2-r2 profile and bug define.
Local pre-compile domain; no fuzzing/mutation, full phase deferred to batch end.

Reference119 bytes versus117 baseline. Both truncate mag/t/nonlinear gain to
signed16 bits at the same arithmetic points. Reference's final nonlinear sum
is formed in the old mag register and then sign-extended into a distinct gain
register; ours forms/extends g in AX. Its initial input loads and square-copy
order also differ. F8007 records the movswl/cwtl discrepancy, not a closed local
source family for this body; updateAlpha's independent lifetime recovery is a
method precedent, not a proof of this source.

Four complete V34TX.c cells cross: retained versus reversed commutative
square summands; retained g = (short)(mag + correction + unity) versus updating
mag with correction/unity then assigning g = (short)mag. The narrowing,
coefficient operations, input caching and output order remain unchanged.
Prediction: accumulator update preserves mag's lifetime through the final sum,
recovering the blob's separate gain extension; square operand source order may
recover its initial instruction ordering. Falsifier: no live-range boundary
change, raw baseline mismatch, collateral/data/relocation drift, or no exact
cell. Inspect initial/combine/allocation before any further source domain; no
register-name or arbitrary declaration permutations.


Outcome: four valid cells, four distinct sources/two raw objects. Final mag
update changes initial RTL (35→32 instructions) but canonicalizes by combine;
its complete object is raw-identical. Reversing squares changes only the target
and also misses. Seven bodies, complete metadata/data/nontext and canonical
relocation controls pass. Nine selected stage records retain the first boundary.
Family closed, no source adopted. Replay tools use historical80c5dea3 plus
--baseline-dir build/production-before after adoption of the parent batch.
