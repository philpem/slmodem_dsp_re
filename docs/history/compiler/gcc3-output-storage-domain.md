# Output storage and aligned HI moves: predeclared domain

Base f7b95e35, retained Gentoo profile and complete production-before objects.
PR263 scope observed at97e8a10c; no V34/header/Makefile/mutation edits here.

New discriminator: the compiler machine pattern *movhi_1 selects an SI load
for aligned HI memory. Original div32 count is at esp+0x10, candidate at+0x12;
initial RTL already records count=-2/mantissa=-4 whereas the now-exact div16
sibling records mantissa=-2/count=-4. Trace the lexical output declarations to
these allocations, rather than fitting a late register or frame size.

Five complete div32 cells: unchanged baseline; pointed-helper control;
baseline with the sibling's mantissa/count declaration boundary; pointed
helper with that boundary; same pointed boundary expressed as one grouped
ushort declaration. The last two spellings check the shared order property,
not a permutation search. No helper parameter-order changes or artificial
padding/alignment/type changes. Predict output addresses already differ in
01.rtl and the final count load remains HI while aligned machine selection
uses MOVL. Bare paired baseline need not restore helper-owned storage.

Falsifier: no announced count promotion, differing output-address arithmetic
beyond the two declared words, altered exports/data/bystanders, or load width
explained by a real SI memory operation instead of the HI pattern. Stop or
reframe after all five cells; never adopt a near hit. Original and complete
candidate bodies, compiler flags/identities and allocation traces are retained.


## Sqrt transfer, declared before scoring

Original sqrt_dp count is atesp+0/mantissa+2; prior pointed-while count+2 /
mantissa+0. Its initial caller declarations put exponent before mantissa,
the inverse of both exact reciprocal siblings. Three cells: retained baseline;
prior pointed-while output1/half1/word1 control; same helper/operands with
mantissa then exponent output declarations. No dead-pop fitting, no slot
padding or output width change. Predict only the output homes/alignment will
move; complete exactness remains a test, not assumed. Stop after three cells.


## Logarithm result ownership, independent four-cell boundary

Prior pointed helper already has the original output homes and aligned HI
count load. The remaining table-derived result is in EDX with MOVSWL return;
original table-derived result occupies EAX and returns through CWTL. Test a
source result owner, not an explicit register or assembly spelling: baseline;
prior pointed-owner control; short table-term local followed by compound
subtraction and return; int table-term local followed by compound subtraction
and implicit short return. Table-term>>3 always fits signed short, so both
narrowing boundaries preserve behavior. Predict local result ownership may
change initial value assignment/coalescing; if all emit the same tail or
still differ, close without arithmetic association/permutation controls.
