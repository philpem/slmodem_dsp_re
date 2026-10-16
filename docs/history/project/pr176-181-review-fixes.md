# PR176 / PR179 / PR181 review-fix packet

Local, uncommitted follow-up to `51ee3b92`, branch `improve/issue172-pass-4`.
This packet is for parent review and the subsequent independent review of the
**entire** tolerance/split register. It is not that independent review.

**Follow-up:** the full 14-entry audit and subsequent shared-apparatus fixes
are now recorded in `docs/tolerance-split-register-review.md`. That record is
authoritative for the current disposition. The first-pass results below remain
as evidence of the earlier containment work, not approval of migration.

## Implemented boundaries

* `gccdiverge.py`: all declarations fail closed. `exact_transcript_v1` is now
  diagnostic-only and CANNOT authorize an exemption. Equal literal output can
  hide unrelated underlying strcmp failures at the same tag or abbreviated
  object differences. Lossless structured evidence and assertion/build identity
  are still required; copying stdout is explicitly insufficient. Signals,
  incomplete output, zero denominators and stale green entries remain failures.
* `diff_eq_float_word`: period compares raw binary32 words (including zero
  signs and NaN payloads); modern comparisons use a per-call mixed allowance
  and restore the previous policy immediately. Inputs are raw against both
  original seeds. Ordinary outputs retain their previous default allowance.
* `region_float_graph.h`: shared classifier follows both sides of each named
  path through the paired allocation graph, bounds every pointer read and
  float span, and reports classified words / total words independently of
  differing values. Discovery rejects unequal allocation lengths instead of
  clipping. Optional paths may be absent on both sides; mismatches fail.
* `t_v90modprog`: complete after-call parent graph restored; modern mask is
  only demodulator +0xe8 and +0xfc..+0x10f, input 8601, six float words / 24
  bytes. Other inputs retain the existing typed modern comparison (including
  signed-zero numerical equality); period graph comparison is raw.
* `Makefile` and register explanation: GCC 3.4.2 **also** implies
  finite-math-only under fast-math. Only the modern flag remedy is retained.

## Policy limitations for independent review

The existing approved 1e-4 absolute / 1e-6 relative policy is retained solely
on the three fixtures' modelled heap spans. The observed maximum and its 2.5x
margin are **not independent algorithmic correctness evidence**. No tolerance
has been increased. Passing beyond-budget and changed-input controls validates
the apparatus, not the DSP acceptability of that policy.

All **14/14 register entries remain blocked**, pending assertion-level review:

```
t_v90eqdatatrans       t_vpcmflomodentrans   t_v34info1atrans
t_v90cdadjusttrans     t_v90modproganalog    t_v90adidrecip
t_v90trn2designrecip   t_psd                t_v90equ
t_v90p4dnan           t_v90equproc         t_v90specproc
t_v92ecnan            t_v90adidnan
```

For migration, obtain lossless underlying evidence in a structured assertion
protocol with full assertion/input/build identity, confirm denominators, verify
unrelated assertions remain observable, and corrupt the SAME failing site/tag
in an unrelated way. Uncapped current diagnostics alone are not lossless.
Do not derive an exemption by copying output. The completed audit records
remaining split/mutation holes, policy bounds and unreviewed provenance in
`docs/tolerance-split-register-review.md`.

## Validation and artifacts

All heavy jobs use `J=1 -j1`, one at a time. No mutation sweep was run.

* `make phase J=1 -j1`: **383 passed, 0 failed**, exit 0. New safety probes
  live under `test/safety`, not `test/unit`; the differential binary count
  remains 383. Initial `phase.log` timed out and is **invalid**; use
  `phase-last.log` is the final complete run, exit 0.
* Register controls: **11 tests passed**, including simulated signal return,
  malformed/truncated/missing output, wrong assertion in the same group,
  changed input, zero denominator and now-green rejection.
* Shared graph/word apparatus: **17 controls passed per mode**, modern and
  no-define. Includes `-0/+0`, distinct NaN payloads, the exact input
  perturbation 0.369140625 -> 0.3691906333, ordinary-output budget isolation,
  `0x38d1b718` vs zero, wrong side-B paths and invalid spans.
* Existing float and field-typed safety controls: **4/4 and 48/48**, in both
  modern and no-define modes.
* All **10 focused parents pass**. Seven companions retain measured failure
  exits; all seven wrappers reject them, exit 1. Detailed per-group and total
  denominators: `denominators.json`, plus `focused-status.json`.
* Analog parent negative controls `guard`, `phase2`, `params`, `state`,
  `unmasked-float`: each reports **1 failed / 10526 checks**, exit 1; baseline
  reports **0 / 10525**, exit 0. These opt-in probes restore the perturbed
  byte before any further modem operation.
* `python3 tools/refcheck.py`: **14050 references**, 0 unresolved, 0 pending,
  0 stale, exit 0. `git diff --check`: exit 0.

Focused modern totals (failed / checks; raw binary exits, no exemptions):

| Parent | Result | Companion | Result |
|---|---:|---|---:|
| t_vpcmrunpcm | 0 / 1366502 | — | — |
| t_v90rundemod | 0 / 987551 | — | — |
| t_vpcmqcline | 0 / 243405 | — | — |
| t_v90modprog | 0 / 10525 | t_v90modproganalog | 6 / 10436 |
| t_v90eqdata | 0 / 1160 | t_v90eqdatatrans | 2 / 96 |
| t_vpcmflomodem | 0 / 58537 | t_vpcmflomodentrans | 2 / 624 |
| t_v34info1a | 0 / 548369 | t_v34info1atrans | 96 / 5712 |
| t_v90cdadjust | 0 / 5715 | t_v90cdadjusttrans | 26 / 244 |
| t_v90adid | 0 / 22044 | t_v90adidrecip | 16 / 262 |
| t_v90trn2design | 0 / 1680 | t_v90trn2designrecip | 4 / 1076 |

Classified float words / total words (equal values are included):
`t_vpcmrunpcm` **1923465 / 20977235**; `t_v90rundemod`
**1453020 / 15846580**; `qcLine` **279885 / 3052415**;
`resetPhase3` **81104 / 914072**. Each guard requires a positive classified
surface smaller than half the total, rather than counting tolerated differences.

Artifacts (local directory `/tmp/opencode/pr181-fixes/`):

* `run_focused.py`: exact sequential commands and direct subprocess statuses.
* `summarize.py`, `summary.log`, `denominators.json`: per-group results.
* `blocked-register.json`: complete 14-entry register with explanations.
* `tolerance-split-inventory.json`: scoped apparatus/split call-site inventory
  (an index for review, not a semantic completeness claim).
* `period-fast-math-macros.log`, `period-override-macros.log`: original compiler
  macro evidence (`__FINITE_MATH_ONLY__` 1 and 0 respectively).
* Individual fixture, companion, wrapper and negative-control logs.

Early standalone compilation attempts omitted the repository's bare-C++ link
flags; those were build-apparatus failures, not numerical evidence. The
checked-in Makefile safety rules now include the 32-bit architecture flags
explicitly and the successful focused build exercises them.
