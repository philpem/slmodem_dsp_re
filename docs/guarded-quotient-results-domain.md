# Guarded quotient field results

Baseline04eee73f. One conditional-assignment hypothesis per complete V90 TU:
V90SignBitsExtractor::reset width, V90SpectralShaper::reset blockLength,
V90Mapper::reset signBitGroupSize. Three raw baselines/three controls (six
complete compilations). Original guards avoid zero-divisor DIV and the two
paths write the quotient or zero before any use. Their actual machine stores
remain separate; this is a wider source preimage hypothesis than F11877's
common physical store, not an asserted identical mechanism.

The original quotient and zero fallback justify the ordinary guarded quotient
expression. Preserve all unsigned operand/field types, statement positions,
member reloads, call boundaries and export bindings. No new locals or captures,
register constraints, ordering/type/flag sweeps or pointer lifetime extensions.
The earlier SSF local-pointer/order domains stay closed; this changes only its
independently guarded quotient, not the declined SSF base-register spelling.

Prediction: conditional-value ownership changes allocation/RTL while retaining
the original zero-divisor boundary and quotient semantics. If all streams merge,
close. Assess every emitted body/data/relocation and gain/loss; particularly
verify any exact producer remains exact before using a bystander gain. Do not
adopt a candidate solely for an unsupported bystander: source expression and
changed producer body must remain independently supported by original operands.
No fuzzing/mutation execution. Batch source gates only if a cell is adopted.
