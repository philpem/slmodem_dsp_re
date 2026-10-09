# Value-use evidence checkpoint

Baseline `a95a6c65`, Gentoo production census1074/1852,118087 original bytes.
This pass compiled52complete TUs and audited479shared body verdicts:

| Area | Compilations | Body verdicts | Strict gains |
| --- | ---: | ---: | ---: |
| Small C getter/counter/reciprocal | 11 | 99 | 0 |
| Fax quality and service dispatch/results | 33 | 208 | 0 |
| V90 sample-slot and timing diagnostics | 8 | 172 | 0 |

Counts include raw repeats and controls, not52independent hypotheses. Two
unsupported timing whole-axis controls are excluded from source-recovery and
closure inference; their falseFABS nomination is explicitly corrected. Two
fax negative controls lose untouched EpochDetectV29; none are adopted.
Production source and compiler flags remain unchanged. No fuzz/mutation or
runtime harness runs were performed for these diagnostic-only changes.

The strongest new causal evidence is the unchanged EpochDetectV29 agreeing
through allocation/reload and first differing at peephole2's scratch choice,
then register renaming. That extends the existing constructor-stage positive
control to a second TU. It is not a full dynamic proof of the persistent
scratch-search cursor or a reason to force a register in source.

An independently recoverable source gap remains: FAX_class1_command's default
extra argument is uninitialized instead of originalzero. Existing callee use
ignores that argument except in the correctly suppliedFTM arm. It is recorded
as argument fidelity; no public operational failure or full-exact gain follows.

Further work should gather independent original uses and observe compiler
state where patterns stay equal through allocation. A useful bounded next
diagnostic is actual scratch-search events for constructor and EpochDetect
positive controls, with equal-control/refusal checks, before attributing other
register-only residuals to that mechanism. Do not restart closed local width,
store-order or literal families simply because a candidate has near size.
Source/profile questions remain coordinated with#22; this pass changed neither
that issue nor the other session's structure branch.

Detailed evidence: [small C](small-counter-use-results.md),
[fax quality](fax-quality-use-results.md), [fax service](fax-service-values-results.md),
[V90](v90-operand-tail-pass-results.md). FindingsF11869–F11872.
