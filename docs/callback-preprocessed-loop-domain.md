# Indirect callbacks and preprocessed fixed-loop screen

Source baseline80da7b0c (production source matches7edfb734),1079/1852 exact.
Read-only screen over all299 retained C/C++ TUs; one assembly TU excluded.
Use the retained Gentoo command to preprocess each original source file, with
complete flags/current headers and final mandatory bug define. Preserve line
markers to distinguish original source/header loop sites, commands and hashes.
No reconstruction flags or source changes. Included-header provenance must
remain inside src/ or include/ for project nominations; system loops excluded.

Use balanced for-header parsing, start/bound/update checks and a restricted
integer-expression evaluator. Recognize finite ascending unit-step loops of
2..64 iterations; unknown identifiers, casts, sizeof, enums and variable bounds
remain explicitly unsupported. C++ unqualified header owners may map ambiguously;
report all possible emitted owners rather than choosing a clone by dump order.
Macro bounds expand through the actual period preprocessor, not host assumptions.

Count indirect machine call sites for every original/retained emitted body.
A deficit of at least two original indirect sites nominates graph review, even
without a lexical fixed loop. A new fixed-loop owner with fewer original
backedges also nominates; prior literal-source nominations and closed putFrame
remain controls, not new source experiments. Count differences do not establish
callback identity, repeated source statements or per-path invocation counts.

Positive controls: actual putFrame18/6 indirect sites and fixed-group source
loop; live V92Precoder6/12 fixed-loop positive; synthetic macro-expanded header
with known line provenance and nested parentheses parses exactly. Negative
controls reject variable-bound and nonunit-step loops and equal callback counts.
Each new source-control domain requires original CFG/read-age evidence first.
No fuzzing/mutation, compiler-state interventions or excluded-directory access.
