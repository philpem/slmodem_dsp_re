# Fraction argument-use domain

Base75e7ef4b. The scheduler replay reveals a new, earlier witness: initial
RTL has whole FIX174, fraction FIX183, magnitude ABS200/FIX206. Combine
deletes183 and substitutes its FIX into the later fractional argument207,
after magnitude206. Thus the conversion inversion is already present at
combine, not caused by sched2 or physical x87 stack conversion.

The original fraction FISTPL precedes magnitude FISTPL, which writes directly
to outgoing argument+8. Test one bounded use-graph hypothesis: the fractional
absolute value was formed before argument expansion rather than by the
retained conditional argument expression. Four full-TU controls: untouched
production; fixed SF default-math diagnostic; builtin integer abs(frac) in
the argument; explicit if(frac<0) frac=-frac before xf and the diagnostic.
Producer precision, literals, copy graph and callback reloads stay fixed.

Prediction: a different fractional use graph may retain the initial conversion
before magnitude through combine and restore direct argument-slot magnitude
conversion. Falsifier: all forms sink it identically or fail the complete
body comparison. Partial order recovery is not an adoption criterion. No
volatile, alias, slot or declaration permutations; no undefined shifts.
Evaluate all four cells, full-TU bodies/data/metadata, then close the family.
