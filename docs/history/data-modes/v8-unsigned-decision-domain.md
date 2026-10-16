# V8 unsigned dispatch: declared before compilation

Base 38626248, complete V8Dpsk.c, retained Gentoo profile, reproduce-bugs
appended last. No production source/header edits; PR263 scope excluded.

Original CMP1/JB dispatch contrasts with signed-switch CMP1/JLE. Independently,
all four original bit-input loads are MOVZWL. Test exactly four cells: unchanged
baseline, repeated wrapped signed switch, unsigned decision carrier, and that
unsigned carrier with unsigned-short bit formals. No declaration permutations.
Decision values remain 0/1/2; subtraction still wraps explicitly through unsigned
arithmetic before signed positivity. Bit operations already convert to unsigned
short, so formal signedness preserves bit patterns. Neither witness alone proves
the entire original source family. No runtime, fuzzing or mutation execution.

Require baseline raw repeat and repeated signed-switch source/object hashes.
Audit all emitted bodies, binding/exports, allocated data/BSS and nontext
relocations. Stop after these cells; near sizes do not justify adoption. Inspect
final dispatch branch and bit-input extension instructions. Complete exactness
is required before source adoption and the period gate.

## Results

All four cells compile and repeat baseline raw. Repeated signed switch is
source/raw-object identical to the previous domain. Original1100B; baseline1121,
signed switch1097, unsigned switch1089, unsigned switch/zero-bit1089. Unsigned
switch restores CMP1/JB; signed control has CMP1/JLE. The zero-bit crossing
restores all four MOVZWL captures without changing function size. All four
guarded countdown backedges retain DEC, flag-preserving moves, JNE. No complete
exact gains/losses, no production adoption. Full audit across all four packages
is tools/v8_remainder_owner_audit.py, with explicit positive/refusal controls.
