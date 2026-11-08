# Mutation testing retirement

Mutation suites, anchor checking, snapshot maintenance and mutation-specific CI
self-tests are deprecated. They are not part of routine development, source
recovery, build, commit, merge or release validation. Do not add, repair, run or
refresh them unless the owner explicitly requests a named legacy investigation.

The owner reiterated this decision on 2026-11-05. It supersedes the older
[issue #296](https://github.com/philpem/slmodem_dsp_re/issues/296) policy that
retired global campaigns but retained mandatory anchor validation. Keeping that
anchor gate still imposed maintenance on every source rewrite and contradicted
the later deprecation instruction.

## Current gates

`make refs` validates citations and its own citation-parser controls. It does
not read mutation suites or invoke anchor/snapshot tooling. The old
`make mutation-snapshot` target and mutation-specific CI self-test step are
removed. The legacy runners and suites have been removed. Historical records remain
available in Git for provenance; they are not current coverage.

Reconstruction acceptance remains unchanged: Gentoo GCC 3.4.2-r2 differential
comparisons against the blob, exact failure semantics, explicit test counts,
valid fixture histories and structural/provenance checks. Modern portability
checks remain separate. Meaningful differential regression cases, including
constructor-driven input histories and observable callback/parameter traffic,
remain required when addressing a demonstrated source defect.

This retirement does not introduce a fuzzing prerequisite or justify widening
any comparison tolerance. Historical detector lessons remain useful, including
known negative controls, explicit denominators and checks against vacuous
results. They do not impose mutation execution or locator maintenance.

The former detailed policy and measurements are retained in Git history at
`322cf6670:docs/method/mutation-policy.md`.
