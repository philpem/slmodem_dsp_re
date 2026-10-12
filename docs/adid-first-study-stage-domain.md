# ADID first-study late-pass diagnostic domain

Baseline90b7f0d1, complete retained ADID TU, unchanged production profile and
DSPLIB_REPRODUCE_BUGS. No source variants or option/profile experiment.

Prediction: separate byte-publication stores survive reload; peephole2 replaces
their immediate values with scratch-register moves; subsequent CFG cleanup can
share them if it chooses the same scratch register. Localize the count/sum
clearing-order difference separately rather than assuming the same mechanism.

Collect initial, combine, reload, postreload, flow2, peephole2, ce3, register
renaming, block reorder, second scheduling and stack dumps. Require successful
compilation and raw equality to the retained complete object before interpreting
the trace. Full -da crashes on this TU; preserve that failed run as invalid,
exclude its observations, and rerun with the bounded diagnostic switches.

The detector must show two publication stores before the merge and one after;
report inspected stages and count/sum ordering at each. Read upstream release
source as an explanation of observed Gentoo execution, not as proof that its
patch stack has been inspected. No source adoption, flags disabling optimization,
forced colors, mutation/fuzzing or source-order permutations in this domain.
