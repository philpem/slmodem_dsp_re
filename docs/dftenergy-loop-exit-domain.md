# dftenergy searched-index exit forms

Source baseline80da7b0c full src/pump/v34/DFTC.c/current headers, retained
Gentoo profile and mandatory bug define. Macro/expression widening recognizes
hexadecimal bound0x12, omitted by the earlier decimal-only screen.

Original209B retains searched-bit loop at739e0..73a04 with short k. On success
it stores k+1 at73a2b then jumps directly to common bin advancement73a0c;
on exhaustion it stores20 at73a06 then falls through. Retained221B source
breaks and separately checks k>0x12 before the default store. No loop expansion.

Three cells: raw baseline; explicit success jump to common bin advancement;
and a single post-search publication of k+1 (success k0..18, exhaustion k19).
Prediction: jump form removes redundant post-break predicate and recovers the
original success/default graph. Single publication tests whether the optimizer
can split one store into the two observed arms. Preserve arithmetic, shifts,
store age and existing short counters; no statement/operand permutation.
Full-TU/raw/metadata/data audit required. All misses close this finite domain.
No forced registers, profile changes, fuzzing or mutation execution.

Follow-up: retained already publishes integer energy before double energy,
like the original; there is no store-order source correction. Original scale
is copied before the bin loop and its low byte is read as the shift count;
retained hoists a masked unsigned working value. Cross the three declared exit
forms with unsigned-char working count or direct masked count expressions.
Keep &31 in every form (no undefined-count tradeoff). Nine complete-TU cells,
including the three original column controls, bound this byte-count ownership
axis. No operand/store permutations. Bare mask removal was previously closed
and is not repeated. These are low-byte working-count/consumer forms, not a
claim to uniquely recover the argument prototype.
