# TxHdxEQCondV27 operand/use boundaries

Sixteen full V27t_prc TUs at 9025b8d8, retained Gentoo profile. Four binary axes,
baseline plus all 15 combinations declared before compilation:

- c: positive signed countdown compared to zero-extended unsigned caller
  budget at a3bba..a3bc1; consistent unsigned 16-bit selected count in EBX. Retained
  signedbudget comparison chooses a negative taken count for high-bit budgets,
  unlike original. Original entrypositive countdown bounds selected count to
  0..32767. Preserve sourcefill's signed short index and32-bit comparison.
- m: countdown member subtraction at a3bc7..a3bd1 reads original member
  instead of subtracting from retained cached countdown. Use ordinary compound
  member assignment, no pointer/type/volatile abstraction.
- r: original rate loads a3c10/a3c34 stay inside the two output branches on
  every iteration, not retained one-time rate snapshot. Aliased output word
  writes can affect the next rate read; preserve original post-Scramble owner
  capture after nonzero count gate.
- p: original second loop has unsigned 16-bit postdecrement and current-word cursor
  captured before advancing2; test next word's bit4 and store currentword.
  Current signed short indexedforward walk replaced with this observed loop.

Literal7fill, short fill index, status, original one-past next word access,
all calls/order/count casts/output/budget/result and transition behavior fixed.
Only TxHdxEQCondV27 edited; no siblings, headers, declaration/register/slot
permutations or compilerprofiles. Prior GenEQTrnSequenceV27 analogous domain
belongs to another symbol; no prior-cell retry asserted as a fresh gain.
Complete raw baseline/flags/assembler/bug define-last provenance, every TU body,
exports/binding/visibility/data/BSS/nontextrelocation audit. No adoption from
near size or isolated operands. No runtime/fuzz/mutation; root final phase.

Follow-up declared after the first 16 cells: original EDI cursor initialized at
a3bcb before literal fill, and ESI countdown minus one formed at a3bec before
ScrambleDataV27. Cross those two lifetime boundaries on the all-four-witness
source only: baseline/all-four repeat controls plus 3 pre-call combinations,
5 full TUs. No adjacency/localorder choices; this asks whether actual before-call
lifetimes explain the original preserved registers and loop shape.
