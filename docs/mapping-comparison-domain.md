# Mapping comparison operand orientation

Four full-TU cells on witnessed guarded-length/postincrement cell axis-1-1,
pin d13949ad; raw baseline must reproduce the independently compiled control.
Original compares length register against constellationSize[k] memory, and
compares length against n with JA for the inner backedge. Candidate reverses
both operands (memory versus length and n versus length/JB). Test the two source
comparison orientations independently and together: length!=size[k], length>n.
No changes to types, loops, declarations, source history, calls or arithmetic.

Prediction: retained GCC3 may preserve source comparison ownership and restore
the original operand/condition pairs. Falsifier: compiler canonicalizes both,
or other operations contradict the original graph. No byte/register permutation
search. Audit both standalone and inlined caller, all other bodies/data.
No fuzz/mutation execution. Close orientation spelling if controls are inert.
