# PSD getter scalar-mode transfer

Pin5cd55f7a, four complete-TU controls on current fixed Gentoo profile/bug define.
Previous capture-rate/guarded-do family is closed; do not repeat it. Fresh
read-only raw-equal getFrequencies observation identifies its extra4B word as
SF expansion temporary via assign_386_stack_local/gen_truncxfsf2, not a spill.
Original has no local word and fildll/fmul/fmul/fstps with only final narrowing.
The process scalar lever already reproduces its full original x87 stream.

Cross natural scalar expression i*sampleRate*(1.0f/m_length) versus existing
explicit long-double arithmetic, independently with bins>i versus i<bins.
No reassociation: preserve (i*sampleRate)*reciprocal and unsigned conversion
width. Four cells baseline, scalar, comparison, both. No rate capture or do-loop,
declaration/history permutations, flags or forced-register controls.

Prediction: scalar expression removes unused XF-to-SF expansion word while
retaining extended period arithmetic; greater-than source condition may recover
the original cmp0/JBE entry and CMP/JA backedge. Falsifier: new intermediate
narrowing/width changes, unguarded reciprocal, missing original parameter
ownership, or disagreement in period differential. No modern precision claim.
Audit all13 shared bodies/data, especially whether process's six rows change;
its renaming ticks are function-local. No fuzz/mutation execution.

Conclusion and executable audit: [results](psd-conversion-results.md).
