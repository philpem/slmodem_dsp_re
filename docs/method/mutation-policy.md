# Mutation testing policy

Mutation execution and snapshot maintenance are optional investigations.
No build, commit, merge or release requires a whole-tree campaign, a snapshot
refresh, or current mutation records. This implements the owner decision in
[issue #296](https://github.com/philpem/slmodem_dsp_re/issues/296).

## Gate dependencies

| Command | Checks | Executes mutants? |
|---|---|---|
| `make phase` | Prerequisites, period differential tests and structural/provenance checks | No |
| `make test` | Modern differential tests, firewall, strings, offsets and refs | No |
| `make portability` | Prerequisites, test, link exceptions, 64-bit, interop, coverage and debug coverage | No |
| `make phase-full` | Phase and portability | No |
| `make refs` | Citation validation and mutation-anchor structure | No |
| `make mutation-snapshot` | Optional snapshot hashes and bookkeeping | No |
| `tools/mutate.py --suite NAME --only TEXT` | Explicitly selected planted alternatives | Yes |
| `tools/mutsnap.py --update NAME` or `--verify NAME` | Explicitly named complete suites | Yes |

`PHASE_TIERS` and the period comparison rules are unchanged. Exact comparisons,
failure semantics and test selection denominators remain authoritative.
The checked-in Gentoo period CI runs three smoke tests and code-generation
metrics; partial-link CI checks object metrics; documentation CI audits and
builds the guide. None runs a mutation campaign.

`refs` retains anchor validation because detached, ambiguous, vacuous and
misdirected locators have produced misleading historical results. It reports
its suite and mutation denominators. Retaining this structural check does not
require executing or refreshing the associated suites.

## Testing priority

Prioritise meaningful period-compiler differential tests against the blob for
functions whose machine code is not identical. Extend valid construction and
processing histories, boundary inputs and observations of outputs, persistent
state and callbacks where current fixtures leave gaps.

The coverage-guided fuzzer uses the same differential oracle; it supplies
additional generated inputs. It has no assumed priority over focused fixtures
and does not replace their state-validity or comparison obligations. Expand it
only where it demonstrates useful additional input coverage at an acceptable
cost. No fuzzing expansion or all-function fuzz/mutation routing is a condition
of retiring mutation maintenance.

## Targeted investigation

First state the question and boundary: for example, whether a fixture observes
a diagnostic argument swap. Establish valid inputs and a green unmodified
baseline under the chosen compiler. Then select the relevant alternatives:

```sh
python3 tools/anchorcheck.py pulse
MUTATE_TIMING=1 /usr/bin/time -v python3 tools/mutate.py \
    --suite pulse --only 'swap the entry' --verbose --jobs 1
```

Use a disk-backed `MUTATE_WORKDIR` with adequate free space, as required by the
runner. Worker copies isolate planted edits from the source checkout. The
runner refuses a failing baseline. A filtered run prints `SUBSET: n of N` and
cannot be recorded as a complete-suite baseline.

Record the compiler, revision, selected/total suite and mutant counts, elapsed
and CPU time, build counts, equivalent/unusable cases, exclusions, the failing
assertion and investigator time. Separate source defects, fixture/oracle or
reachability defects, detached locators, known findings and zero-finding runs.
Inspect `--verbose` output: a non-zero exit, crash, timeout or string-scan
rejection alone does not establish that the intended assertion fired. Demonstrate
a known negative control before relying on a detector's clean result.

If recording an entire named suite serves the investigation:

```sh
python3 tools/mutsnap.py --update --jobs 1 pulse
python3 tools/mutsnap.py --verify --jobs 1 pulse
```

Omitting suite names from either execution command fails before any build.
`--check` and `--check --strict` remain optional inspection commands. Stale
entries describe historical results; never change keys by hand or present a
stale result as current. Missing or inconsistent bookkeeping can fail optional
inspection without making a reconstruction gate fail. The coarse source key
is retained: all source objects are linked into each test, so hashing only the
mutated translation unit would miss dependencies. Compiler identity and some
external inputs are outside that key and must be recorded separately.

The dedicated `tools/mutsnap-rerecord.sh` full-refresh wrapper is retired.
`mutate.py --all` remains a legacy explicit command for reproducibility; there
is no routine requirement to run it.

## Evidence and costs

The review used master `67d401f8d7991dcda8dd05e896642bcd3da5fb11` on
2026-10-18. Single local command timings, including process startup:

| Check | Wall time | Result |
|---|---:|---|
| Snapshot inspection | 0.73 s | Exit 0; 0 current, 283 stale, 2 declared-unscoreable of 285 registered |
| Citation validation | 7.95 s | Exit 0; 13,064 references checked |
| Anchor validation, `v90demprog` only | 0.78 s | Exit 0 |
| Full anchor validation | More than 20 s | Timed out; no complete verdict |

The original anchor check had a measured lower bound rather than negligible
cost. This change replaces its per-character Python scan with overlapping
string searches, preserving match positions. The complete updated check passed
285 suites / 10,042 mutants with zero skipped, detached, ambiguous, vacuous or
misdirected entries in **0.43 s wall / 0.41 s user / 0.02 s system**. Boundary,
overlap, missing and empty-anchor controls preserve the original search semantics. CPU time, build counts and investigator
time were not measured in the initial review; these checks compile no mutants.

Historical findings provide bounded evidence:

- F432 records nine mutants reported caught after their locators moved to the
  wrong behaviour. A kill did not validate the named claim.
- F10241 records 155 receive-chain trials with an invalid Phase-3 seed. Fixture
  lifecycle auditing exposed the invalid input history.
- F10243/F10244 record stronger complete-message and asymmetric argument-order
  fixtures rejecting previously surviving alternatives. These are fixture
  improvements, not counts of newly discovered source defects.
- F11501 records approximately 13.5 minutes for a serial 100-mutant suite and
  three additional observable alternatives after fixture improvements.
- F11529 records 5,381,408 fuzz executions in 241 seconds, 118/135 instrumented
  counters and zero source divergences, with a firing planted negative control.
  That narrow integer-kernel run does not establish coverage of modem-wide state.

There is no controlled runtime-versus-actionable-finding comparison across these
methods. Kill percentage cannot establish positive or negative defect-finding
ROI. The policy retires recurring maintenance without requiring another global
campaign. Any proposal to restore a mandatory campaign must first demonstrate
incremental actionable findings against focused differential tests and fixture
lifecycle auditing, using known escaped-fixture controls and retaining unsuccessful
runs. Broader fuzzing is a separate coverage project, not a condition of this change.

## Related work

This policy supersedes the global refresh obligation in
[#138](https://github.com/philpem/slmodem_dsp_re/issues/138).
[#237](https://github.com/philpem/slmodem_dsp_re/issues/237)'s targeted fallback
remains available; its proposed all-function routing and narrower dependency
keys are separate work, not new release gates. Reconstruction PR #293 is
outside this change.

## Validation of this change

The optional pulse control ran on Ubuntu GCC 13.3.0
(`gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`), with `--jobs 1`, after a
sandboxed attempt could not run the 32-bit baseline. Native execution passed
the baseline and rejected exactly one selected argument-swap mutant out of
nine. The level-2 and level-3 transcript assertions fired, including zero-break
cases; no mutants were unusable or equivalent. The subset was not recorded.

The successful command took **7.26 s wall, 23.07 s user and 6.72 s system**
(including parallel work within `make`): one baseline build invocation, one
mutant build invocation, and one string scan. The unsuccessful sandbox attempt
took **10.86 s wall, 21.96 s user and 5.62 s system** and produced no mutant
verdict. These are detector controls, not new source findings: **zero new source
or fixture defects**, one expected planted alternative detected. Investigator
time was not instrumented. Logs were `/tmp/dsp-mutation-targeted-native.log`
and `/tmp/dsp-mutation-targeted.log`; the commands above reproduce the controls.

The mutation policy and existing subset controls passed **19 tests**, including
refusal of an implicit campaign, named-suite dispatch and make dependency/recipe
inspection. The period and portability gate graph cannot reach snapshot
inspection or mutant execution. The maintainer guide built with pinned MkDocs
1.6.1 and Material 9.7.6; its audit passed 504 documentation targets, 300/300
module links and 972 generated HTML targets/fragments, with two firing negative
controls. No source, header, differential fixture, comparison tolerance or
historical mutation record changed.

The complete `make phase J=4` run passed **392 period differential tests / zero
failures**, plus the structural/provenance checks. The selected baseline is
master's suite; the separate INFO0 correction PR #295 is not part of this
worktree. The initial cold gate took 198.97 s wall; container CPU work is outside
`time`'s host process accounting, so its user/system figures are not campaign
CPU measurements. Optional snapshot inspection after the change took 0.09 s
wall and still reported stale historical keys without executing mutants.
