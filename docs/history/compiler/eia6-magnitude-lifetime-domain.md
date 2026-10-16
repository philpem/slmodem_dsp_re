# Early magnitude lifetime control

Base75e7ef4b. Fraction builtin abs and explicit-if both recover the original
fraction-before-magnitude integer conversion and magnitude outgoing+8 store.
They produce identical full objects, but remain SIZE8. Neither is adopted.

Independent remaining original use/lifetime evidence: FABS4538c occurs
immediately after the live whole conversion, and its floating result remains
live across fractional subtraction/multiply/conversion until magnitude
FISTPL453c5. Candidate FABS remains later, alongside fractional multiply.
Test that graph directly, without register/stack guessing: introduce the
double-valued fabs result immediately after whole and retain its sole integer
conversion in the diagnostic. Hold the builtin fractional use, SF producer,
default literals, copy graph, predicates and callbacks fixed.

Three full-TU cells: production; previously measured builtin fraction;
early live magnitude. Prediction: the magnitude ABS remains before fraction
arithmetic, with conversion still in outgoing+8. Whole body must be EXACT
before adoption; an optimizer-erased temporary refutes this bounded carrier.
No precision/slot/ordering permutations beyond the observed lifetime.
