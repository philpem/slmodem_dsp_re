# Tolerance / split-register audit — all 14 entries blocked

Audit disposition: **complete for the 14-entry register; migration deferred**.
This is an audit record, not approval of the tolerances, compiler explanations,
or mutation coverage. No exemption is enabled. Worktree: `mutate-workdir`,
branch `improve/issue172-pass-4`, fixes relative to review base `51ee3b92`.
Follow-up remains part of issue #172 / PR176, PR179 and PR181.

## 1. Evidence supporting the bounded claims

The independent review is preserved at
`/home/philpem/.local/share/opencode/tool-output/tool_0be25f319001uGv8JMgeJO6WSS`.
It inspected all 14 entries and reproduced their ordinary exit-1 behavior.
This follow-up freshly built all 14 binaries and the affected typed-span
fixtures, then reran every raw binary and every wrapper: **14/14 raw exits 1;
14/14 wrapper exits 1**. No signal or output-shape failure was approved.

Shared apparatus corrections and controls:

* Double comparisons reject unequal infinities before arithmetic; equal
  infinities and pairs of NaNs retain established scalar numerical semantics.
  Normalized finite comparison avoids overflow turning a mismatch into
  `Inf <= Inf`. Controls include both directions, both infinity signs, NaNs,
  neighboring maximum finite values, and overflowing difference/bound cases.
* Typed-object comparisons validate the **entire** span list before reading
  either object: size 4/8 only, complete bounds, multiplication overflow,
  ordering and overlap. Invalid descriptors fail rather than clip. Nested
  Resampler fixtures select explicit valid class-size prefixes; every remaining
  byte is compared. Omitted element sizes at legacy callers are now explicit 4.
* Period/no-define typed-object storage is raw, including NaN payloads and zero
  signs. Scalar APIs remain numerical; documentation no longer equates their
  zero-budget behavior with raw storage identity.
* Float ULP distance uses fixed-width unsigned keys/distance, with zero signs
  coalesced and NaNs classified separately. No native-long partial initialization
  or signed subtraction overflow remains.
* Mixed mode has explicit state: `rtol=0` is truly zero; `atol=0` still uses
  reference scaling. Scoped word comparisons restore the mode as well as values.

Verification: numeric/storage safety **54/54 controls in each of four builds**
(ILP32/LP64 × modern/no-define); prior graph/word controls **17/17 per mode**;
prior modern field/float controls **48/48 and 4/4**; register controls **12/12**.
`make phase J=1 -j1`: **383 passed, 0 failed**, exit 0, with positive structural
denominators. New safety binaries are not period differential fixtures, so the
period binary denominator remains 383. No full modern or mutation sweep ran.

## 2. Missing evidence, invalid inference, and per-entry disposition

### Common blockers

**Literal-output v1 is insufficient, even when byte-for-byte identical.**
`strcmp(...) == 0` failures serialize only `got 0, reference 1`; two wholly
different underlying transcripts can produce the same assertion line at the
same input. Object diagnostics abbreviate differing runs/bytes even with
`DSPLIB_MAX_REPORT=0`. The new negative control demonstrates this collision.
`literal_output_matches` is diagnostic-only; `declared_failure` and the CLI
refuse authorization even for a matching v1 record. **Do not migrate by copying
stdout.** A future protocol must preserve lossless underlying evidence, complete
assertion/field/bin/input identity, and compiler/architecture/flags/binary/source
identity, plus independent justification and same-site corruption controls.

Historical disassembly and claims such as “no source form” or “period pass
proves the original source” have not all been re-derived. A passing period
differential supports behavior over tested inputs, not uniqueness of original
source or universal compiler causation. These claims are not migration authority.

### Complete 14-entry register

Every row below is **BLOCKED**. Counts are failed / checked in the specified
group from the focused rebuilt modern artifacts; full group and binary totals
are in `fixture-status.json`.

| Entry | Observed surface | Missing evidence / required discriminating control |
|---|---|---|
| `t_v90eqdatatrans` | `enterDataPhase`, 2/96 | Lossless actual text/numeric fields; reject wrong wording, sign or scale at the **same** failing tag. Parent retains state/arena/return checks but diagnostic-only mutation coverage is unresolved. |
| `t_vpcmflomodentrans` | `getUinfoValue`, 2/200; 624 total checks | Separate compiler/libm/formatting explanations; preserve other transcript rows; same-tag nonnumeric corruption must fail. |
| `t_v34info1atrans` | INFO1a, 96/5712 | Lossless L2 diagnostics and full composed-trial identity; corrupt INFO fields and unrelated text at an already-failing tag. |
| `t_v90cdadjusttrans` | Loud diagnostics, 26/244 | Four distinct transcript sites need identity; six zero-check groups are not valid v1 denominators; diagnostic-only mutation observations and same-tag scale/text controls are missing. |
| `t_v90modproganalog` | Analog group 6/111; 10436 total | Independent feedback-error bound; exact field/value evidence; corruption beyond allowed behavior inside a known divergent word. Companion duplicates unrelated groups and auxiliary output. Parent has the six-word/24-byte mask at 8601 and five working graph-corruption controls. |
| `t_v90adidrecip` | Mean/variance group, 16/262 | Abbreviated whole-object evidence is not lossless. Three methods/guards in the group must not all become exempt. Corrupt an unrelated byte in an already-failing object. |
| `t_v90trn2designrecip` | Reciprocal 2/4; cap 2/1072 | This is a design/decision/output divergence, not an approved rounding tolerance. Require structured mapping fields and both-side success/cap controls; unrelated constellation/return corruption must fail. |
| `t_psd` | Frequencies 1/8169; process 3767/140404, 3996 tolerance-only passes | Separate frequency association from FFT/decibel effects. Buffer/trial identity is lost in word-index diagnostics. Resolve red mutation suite; corrupt a guard, scale/option or bin at a known failing index. |
| `t_v90equ` | Seven failing groups; setters 5608/86026; exact powers 80/105 | **Corrected provenance:** exact-power fixture calls `setLinearEquBeta` / `setDfeBeta`, not `convertEqualizerToMmx`. The previous accumulator disassembly does not explain this group; precise cause remains unreviewed. Separate NaN from finite effects, resolve red suite, reject unrelated state/guard corruption. |
| `t_v90p4dnan` | Keep-rate group, 2/9 | Scope/legal-state evidence for planted negative energies and an actual NaN witness (flag 1 alone is insufficient). Preserve transcript/state/progress assertions as hard checks. |
| `t_v90equproc` | RESET 731/120974; phase-4 1/18023 | Structured field/run evidence; group exemptions are too broad. Inputs compare sides rather than original seeds. Corrupt unrelated output/peer/guard/input storage. |
| `t_v90specproc` | Process 301/35450 in this build | Historical GCC13 count differs; missing bin identity; both PSDs use the reconstruction constructor. Reject moved-bin, return, state and transcript faults. |
| `t_v92ecnan` | Sentinel group, 492/2703 | Full buffer/object evidence, shared-input integrity, signaling-NaN transport, guard/history controls. Keep reference-only witnesses hard. |
| `t_v90adidnan` | Window group, 6/25 | **Corrected provenance:** NaNs are skipped, preventing qualification; they do not qualify. Two qualifying controls and three NaN layouts need lossless object/transcript evidence and guard controls. Corrupt a qualifying row and an unrelated field of a failing row. No replacement assembly explanation is claimed. |

### Mutation-suite conflicts and transcript observation holes

The register/suite audit found **2/14** registered binaries still targeted by
mutation suites: `psd -> build/test/t_psd` and
`v90equ -> build/test/t_v90equ` in `test/mutations/suites.json`. Both fresh
baselines are red. These suites are not currently scoreable under the ordinary
exit-code mutation contract. The other **12/14** have no suite reference.
No declaration, baseline or snapshot was altered to hide this conflict.

Conserving parent+companion assertion counts is not conserving mutation catches.
Moving all transcript assertions to red companions can remove the only observer
of diagnostic-only mutations still registered against the clean parent:

* `test/mutations/v90eqdata.json`: DFE printed scale and sum/absolute-sum swaps.
* `test/mutations/v90cdadjust.json`: printed scale and final dBm0 ladder offset.

These are explicit unresolved coverage holes, not claimed retained catches.
Next work needs a per-assertion split map and targeted diagnostic-fault catches;
no sweep was run or inferred here. Analog parent/companion counts additionally
duplicate substantial work and must not be summed as a conservation proof.

### Provisional tolerance policies

The owner-authorized graph policy (1e-4 absolute / 1e-6 relative) remains scoped
to modelled words. The 2.5x observed maximum is **not** an independent correctness
bound. Earlier fixture-wide policies in `t_floatarma`, `t_resampler`,
`t_v90demctor`, `t_v90demprog`, and `t_v92modstate` persist across groups and
reach all eligible comparisons there, not merely observed differing sites.
Their observed maxima/margins do not establish downstream acceptability.
No budget was enlarged or fitted in this follow-up. Beyond-budget controls
establish apparatus rejection boundaries, not modem algorithm correctness.
Direct unequal-size discovery controls for every individual graph fixture and
the broader split mutation-observation work remain unproven by these shared
apparatus controls.

## 3. Narrowest justified claim

The seven requested apparatus/documentation blockers have been addressed with
focused positive/negative controls. The full register has been audited and its
missing evidence recorded. **Migration remains deferred, and every registered
wrapper is explicitly red.** Neither tolerance-policy approval nor preservation
of all mutation catches is claimed. Existing modern failures remain visible:
among the ten additional focused fixtures, eight pass; `t_floatarma` still
fails 556/137472 and 178/19964, and `t_v92modstate` fails 4/33373 in progress.

## 4. Reproduction and next discriminating work

Run from the worktree, one heavy job at a time:

```
make J=1 -j1 build/safety/t_numeric_storage build/safety/t_numeric_storage_exact \
  build/safety/t_numeric_storage64 build/safety/t_numeric_storage_exact64
build/safety/t_numeric_storage
build/safety/t_numeric_storage_exact
build/safety/t_numeric_storage64
build/safety/t_numeric_storage_exact64
python3 -m unittest discover -s test/safety -p test_gccdiverge.py -v
python3 tools/gccdiverge.py t_v90eqdatatrans build/test/t_v90eqdatatrans
make phase J=1 -j1
```

The wrapper must exit **1**, even if a v1 output record is supplied. Safety and
phase commands must exit **0**. Before any future migration, design a lossless
structured evidence protocol, bind build identity, independently review each
claimed divergence, and prove same-site unrelated corruption is rejected.
Resolve the two red mutation-suite conflicts and diagnostic-observation holes
as separate explicit work; do not bless them using a failing baseline.

Artifacts: `/tmp/opencode/pr181-fixes/followup/`. `run_safety.py`,
`run_fixtures.py`, and `run_gate.py` record exact commands and direct subprocess
exit codes. `safety-status.json`, `fixture-status.json` (including binary SHA256s
and all group denominators), `gate-status.json`, `blocked-register.json`,
individual logs, and the original compiler macro logs in the parent directory
are the evidence packet. The first safety compilation lacked the new ULP
prototype and produced no binary; only the subsequent successful build/run is
valid numerical evidence.
