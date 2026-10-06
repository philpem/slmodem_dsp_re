# EIA6 usual floating math promotions

Declared2ca02aec after the six source-mode controls independently recover
live FISTL for SF/DF and original final FCOMPP only for coherent SF. Earlier
trace identifies scale fold at lreg and zero fold at combine, not stack.
Original SF10000 constant does not establish multiplication mode:10000.0
is exactly SF-representable and a DF expression can load the same SF pool.
Likewise fabs has the same machine opcode for SF/DF, and original zero is
FLDZ. These operands leave coherent float-suffixed math and ordinary default
double literal/fabs math as bounded idiomatic alternatives, not recovered types.

Five fullTUs: untouched production baseline, existing SF/DF expanded-unequal
controls, and SF/DF producer with usual double10000.0, sign0.0, fabs and xf
comparison0.0. Same producer initializer/matching whole lift, existing18copy,
no new arithmetic operations/counters/fields/callbacks/flags/declarations/slots.
Preserve float xf across callback. Predict ordinary promotions can retain
scale/zero registers and reproduce original multiply/compare pop forms.
Falsifier extra narrowing/changed operation graph or no complete byte EXACT.
No source adoption by size/operand-only gain; complete TU audit and period
differential mandatory if a winner exists. Close after5cells, do not try
per-expression type/predicate/declaration spelling products. No fuzz/mutation,
V34/PR263/shared headers/issue22 writes.
