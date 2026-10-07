# RMS reciprocal expression mode

Original `fComputeRMSValueFloatBuf` (0xae120, 82 bytes) loads x87 one at
0xae13b before the mean division and retains it across the residual-power
loop. At the tail it divides that stack value by the unsigned count and
multiplies by accumulated power. Retained source emits a float-memory
reverse division at the tail and lacks the retained stack one.

Two complete-TU cells: unchanged source and the native C double literal
`1.0` instead of `1.0f` in the reciprocal. This tests expression mode, not
floating storage or an invented rounding boundary. It retains float locals,
unsigned count, loops and every read/write. The hypothesis is that the
original unsuffixed literal accounts for the stack constant's lifetime;
the original instruction does not uniquely prove that spelling. No further
literal-width or local-type matrix is authorized by this witness.

Require raw unchanged Gentoo object reproduction and unchanged flags with
bug reproduction enabled. Review complete bodies, data and symbol/relocation
metadata. Only a complete strict match with explaining original operands and
passing the deciding period differential can be adopted.

The double-literal control emits FLD1 but adds a final SF store/reload absent
from the original, and does not preserve the constant across the residual
loop. A final independent operand-use control retains the SF reciprocal and
writes `(1.0f / (float)n) * acc`: the original keeps the reciprocal numerator
live while computing power and consumes it as the division's numerator
before the product. Test this one reversal only (three total cells); do not
continue through local type/constant or accumulator permutations on a miss.
