# DTMF decision eager predicate and numeric ownership controls

Precompile domain, revision856c1ecb. Original dtmf_test executes all five ratio
comparisons, materializing SETAE/NEG/AND rather than a short-circuit branch chain.
The retained source uses &&, and its actual period body short-circuits. Original
also has three FLDZ copies before mode selection, consistent with initial sum,
max_lo and max_hi values being initialized together, whereas retained initializes
these values just before their respective loops. Four complete-TU controls cross
bitwise eager conjunction with early numeric initialization. No arbitrary
register assignment, flags or store permutations. Evaluate full dtmf_test and
inlined consumers and every TU bystander/data/metadata.

First four controls: eager bitwise conjunction replaces short-circuiting but
emits SETAE/AND without original NEG masks;418B baseline,404B eager, neither
matches412B. Early numeric initialization changes body but not size. The
original NEG/AND structure independently points to GCC3 noce destination-identity
conditional clearing (existing F11568/F11570 lever), rather than a bitwise
Boolean expression. Second domain: baseline plus sequential `ok = ok && cond`
and independent `if (!cond) ok = 0`, each crossed with early numeric initialization.
This is five cells, not a predicate-order search. Keep five original comparisons
in their measured order and the separate upper-rest rejection.

Results: baseline418B; early-only418B (distinct canonical body); eager controls
404B. Sequential conjunction controls481B; independent-clear controls419B.
None matches original412B. All nine controls preserve dtmf_detect's full612B
body, dtmf_progress and dtmf_set_easy; the decision source does not affect the
inlined budget-regime body in this TU. Two exact symbols remain exact.

`tools/gcc3_batch100_dtmf_mask_trace.py` fires on three known source cases,
four RTL stages each (12 measured stage cases). Baseline initial RTL has zero
SImode AND and16 if_then_else; eager has four AND and11 branches already at
initial expansion. Conditional clear starts with zero AND/NEG and16 branches;
ce1 creates five AND/five NEG with11 branches, ce2 reduces to four AND/three
NEG, stable through ce3. Thus direct bitwise source and conditional clearing
are distinct compiler histories despite superficially eager final comparisons.
Original has the fuller masks; no unique source preimage established. Stop
these eager/clear/early-numeric families; do not add arbitrary masks or barriers.

Complete-TU audit passes9/9 controls: all four canonical symbols, metadata,
allocated nontext bytes/relocations, no named data objects, all bystanders
accounted. Saved baseline raw-reproduces actual production; no adoption.
