# V29 epoch pair evaluation

Three complete V29rx TUs at6b4509bd: baseline, separate-sums, streamed-pairs.
Original epoch9b6d0 adds firstpair,9b6d6 shifts it before secondpair completes
9b6e7/9b6e9; source forms four narrowdeltas before one combinedexpression.
Hypothesis: two ordinary distance assignments bound live values, and source
streaming secondpair after firstsum can eliminate extra delta/spill lifetime.
Separate-sums control isolates expression boundaries from read lifetime.
Keep every difference cast/independent shift/final short narrowing/history
store/phasefold/alias-sensitive double average store/handover/counter/return
fixed. No declaration/type/flag/register/slot permutations. Nohelper inlining:
whole original epoch has0calls and3relocations(dataangles/callbackassignment).
Require rawbaseline fullTU/machinecommand/assembler/bugdefine-last proof,
complete allbody/binding/data/BSS/nontextrelocation audit; close after miss.
No runtime/fuzz/mutation; no sourceadoption from size, spill, or partialmatch.
