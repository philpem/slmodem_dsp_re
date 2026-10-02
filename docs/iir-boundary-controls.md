# Shared IIR count, cursor and feed-forward boundaries

F11603. [Eight-cell predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950025189)
at43ef6841, full src/dsp/fpm_iir.c. Cross short postdecrement sections, grouped
coefficient/state cursor consumption, explicit feed-forward truncation before
saturation. Preserve initialized input and existing return; no fabricated
uninitialized-output edit. F26/F8161/F8166 were semantic/inlining/census work;
V22 IIR controls concerned another TU, so this domain was fresh.

| Cell | Scalar (blob191B) | Block (blob287B) | II (blob232B) |
| --- | --- | --- | --- |
| baseline |181|258|213|
| narrow |201|278|213|
| cursors |199|276|213|
| cursors+narrow |199|276|213|
| count |176|271|213|
| count+narrow |192|271|213|
| count+cursors |190|269|213|
| all |190|269|213|

Eight sources/eight raw complete emissions, no exact hit/gain/loss (0/3
unchanged). Unchanged production baseline raw-reproduced. All3 functions,
export/import types/binding/visibility and allocated nontext preserved; no
named OBJECT data. Scalar and inlined block change throughout; count cells
also change untouched II's canonical body at unchanged213B. This bystander
must be included in complete-TU review, not described as unchanged from size.
Complete-object-audit.json records symbol/nontext controls; results.json records
every changed body and verdict.

Combined scalar has58vs59 instructions and block79vs86 after padding removal.
They are not register-only recoveries or exact one-byte preimages. Keep all
compares/arithmetic/return evidence, rather than rewriting declarations,
forcing saved registers or reproducing arbitrary undefined return contents.
Scalar/block zero-sections are excluded by existing fixture/reference audit;
no runtime result is inferred for them or negative section counts here.

Close this eight-cell family with no source/flag adoption. No candidate
runtime, whole-tree census or partial-link gate was run. Existing fixed
cascade/saturation/block tests are not evidence for unrun candidates. Reopen
only with independent initial compiler-stage evidence. This bounds the declared
source families, not all IIR preimages or the original profile. Production
remains884/1852 exact,88,310 exact bytes, last fixed phase385/0.

Replay tools/playbook_iir_boundaries.py --domain <linked URL>.
Artifacts build/playbook-iir-boundaries preserve actual full saved flags,
mandatory DSPLIB_REPRODUCE_BUGS, GentooGCC3.4.2-r2 and executed selected
assembler2.15.92.0.2, commands/source/header hashes, complete objects and initial
RTL. No fuzzing or mutation execution.
