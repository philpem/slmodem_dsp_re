# Tolerance / split-register audit — all 14 entries blocked

The strict p4d observer probes can be rerun with
`make J=1 -j1 safety-p4dnan-gcc14`. This explicit target builds its binary and
checks the measured GCC 14 ILP32 baseline; it is not a compiler-independent
period gate or an exemption validator. The five control methods pass, including
seven additional-failure probes, two constant-flag probes, and the lossless
same-failing-transcript probe. All register entries remain blocked.

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
| `t_v90eqdatatrans` | `enterDataPhase`, 2/96 | Still **BLOCKED**. Section 5 captures both texts and restores exact dyadic parent observers, catching the registered scale/sum mutants. No approved budget for the divergent DFE sums or conversion diagnostics. |
| `t_vpcmflomodentrans` | `getUinfoValue`, 2/200; 624 total checks | Still **BLOCKED**. Section 5 captures complete texts with method-specific assertion IDs and trial tags. L2 field policy and separate compiler/libm/formatting explanations remain unsupported. |
| `t_v34info1atrans` | INFO1a, 96/5712 | Still **BLOCKED**. Section 5 captures complete L2/INFO text and composed-trial tags. No independently justified, owner-approved L2 diagnostic budget; no migration. |
| `t_v90cdadjusttrans` | Loud diagnostics, 26/244 | Still **BLOCKED**. Section 5 distinguishes all four sites, repairs capture truncation and restores the exact power transcript in the parent. Real-K policy remains unsupported; zero-check companion groups are not authorization denominators. |
| `t_v90modproganalog` | Analog group 6/111; 10436 total | Independent feedback-error bound; exact field/value evidence; corruption beyond allowed behavior inside a known divergent word. Companion duplicates unrelated groups and auxiliary output. Parent has the six-word/24-byte mask at 8601 and five working graph-corruption controls. |
| `t_v90adidrecip` | Mean/variance group, 16/262 | Abbreviated whole-object evidence is not lossless. Three methods/guards in the group must not all become exempt. Corrupt an unrelated byte in an already-failing object. |
| `t_v90trn2designrecip` | Reciprocal 2/4; cap 2/1072 | This is a design/decision/output divergence, not an approved rounding tolerance. Require structured mapping fields and both-side success/cap controls; unrelated constellation/return corruption must fail. |
| `t_psd` | Frequencies 1/8169; process 3767/140404, 3996 tolerance-only passes | Separate frequency association from FFT/decibel effects. Buffer/trial identity is lost in word-index diagnostics. Resolve red mutation suite; corrupt a guard, scale/option or bin at a known failing index. |
| `t_v90equ` | Seven failing groups; setters 5608/86026; exact powers 80/105 | **Corrected provenance:** exact-power fixture calls `setLinearEquBeta` / `setDfeBeta`, not `convertEqualizerToMmx`. The previous accumulator disassembly does not explain this group; precise cause remains unreviewed. Separate NaN from finite effects, resolve red suite, reject unrelated state/guard corruption. |
| `t_v90p4dnan` | Historical keep-rate group, 2/9; strengthened fixture in section 6 | Still **BLOCKED**: missing actual production NaN witness; enabling diagnostics also exposes two hard transcript failures. Negative energies are synthetic adversarial inputs, with no reachability claim. |
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

These were explicit coverage holes at audit time. Section 5 restores and
measures the four named diagnostic-only catches without enabling exemptions.
Broader work still needs a per-assertion split map and diagnostic-fault catches;
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

## 5. First transcript repair batch — zero-budget observers, migration blocked

Local follow-through from `01616548d54555f9bc4525b7ef46a9077a4657b8`, on
`improve/issue172-pass-4`. **0/4 entries migrated; the other 10 remain blocked.**
This is a bounded mutation-observer and evidence-capture repair, not the
requested future field-policy authorization backend. Independent review is
pending; these changes have not been published.

### What the code now checks

* `t_v90eqdata` adds **32 exact whole-transcript checks**, one for every existing
  pattern-0 trial. The fixture generates multiples of 1/16 and 1/8 over at most
  16 taps; those sums are exactly representable in binary32. Selection is by
  that arithmetic input domain, not an observed passing/failing tag. All 96
  original cases still run in the exact companion, including the adversarial
  small-tail pattern. Parent checks rise **1160 -> 1192**; no input is removed.
* `t_v90cdadjust` restores **all 61 exact `adjustConstellationsPower`
  transcripts** to the parent. This includes sqrt scale, sign, text and dBm0
  ladder output. Parent checks rise **5715 -> 5776**, with the loud group
  **733 -> 794**. All four sites remain in the companion's 244 checks. This
  deliberate overlap is not a conservation-of-counts claim.
* The other two parents retain their existing comparisons. No new clean-parent
  L2 observer is claimed. Their complete original transcript checks remain red
  and exact in the companions; implementing a numeric field policy requires
  evidence and approval which this batch does not have.
* `transcript_exact` uses captured byte lengths, byte equality and callback
  counts, and rejects incomplete capture. No float parser, epsilon, field mask,
  sign normalization or output-based exemption is introduced. Exact text and
  period behavior retain a zero numerical budget.

The producer's opt-in `DSPLIB_TRANSCRIPT_EVIDENCE=1` records a sequence number,
hex-encoded assertion ID, input tag, completeness/outcome, both callback counts,
both lengths and **every captured byte**. Embedded NULs cannot hide a suffix.
`VPcmFloModem` uses the method name passed to `compare_all`, because several
methods reuse numeric input tags. This is **diagnostic collection only**:
`gccdiverge.py` does not authorize these records, and its 14 entries still fail
closed. A sequence number and raw bytes are not themselves a reviewed
build-bound field policy or proof against all malformed authorization requests.

### Capture defect found while obtaining the evidence

The former 16 KiB buffer silently truncated both sides of
`cdadjust.process` trials **5, 26 and 56**. Marking overflow exposed three
additional apparatus failures (29/244); they were not new DSP differences.
The capture buffer is now 256 KiB, with an explicit overflow/format-error flag
reset alongside the text. The rerun captures all four companions completely:
**6676/6676 records**, **126/6676 exact mismatches**, zero incomplete records.
Restoring the previously lost tails returns the designer to **26/244**.
Overflow remains a hard failure even when both truncated prefixes agree.

### Per-entry evidence and why no budget was adopted

| Entry | Fresh modern result | Example actual reference -> reconstruction | Disposition |
|---|---:|---|---|
| `t_v90eqdatatrans` | 2/96, exit 1 | `eqdata.enter`, input 1012045: DFE `coefs sum` and `abs coefs sum`, `+0.005849` -> `+0.005850` | Exact dyadic parent observer restored. The displayed gap is about 1.71e-4 relative, not the scalar 1e-6 default. No approved display/accumulation budget. |
| `t_v90cdadjusttrans` | 26/244, exit 1 | `cdadjust.design`, trial 3: real K after optimization, `+20.06271` -> `+20.06272` | Exact power-method observer restored. The scalar policy does not automatically authorize applying a tolerance to formatted real-K diagnostics. |
| `t_v34info1atrans` | 96/5712, exit 1 | `info1a.block`, input 2404000: `L2[15] = -0.17761` -> `-0.17762` (original spacing/CRLF retained in artifact) | L2 decimal quantization needs an independently justified field-specific policy; observed one-digit differences are not approval. |
| `t_vpcmflomodentrans` | 2/624, exit 1 | `after getUinfoValue`, input 200020: `L2[15] = -0.18124` -> `-0.18123` | Same policy blocker, separately identified method/input. No compiler or libm cause established here. |

The eqdata input 1012046 also differs in the later LE conversion diagnostics:
`+1024.0058` -> `+1024.0057`. A policy that named only the DFE rows would not
cover that record. The raw encoded diagnostic bytes and decoded examples are
both retained; stdout's Boolean `got 0, reference 1` is never authorization.

### Focused controls and artifacts

Four registered mutants, run individually with `--only`, `--jobs 1`, `J=1`
and the fixed disk-copy mutation tool, are caught by clean **parent** binaries:

| Registered mutation | Parent failing checks / group checks |
|---|---:|
| DFE minimum scale 1e10 -> 1e6 | 6/1192 |
| DFE sum and absolute sum swapped | 4/1192 |
| Initial sqrt fraction scale 1000 -> 100 | 60/794 |
| Final dBm0 omits ladder +1 | 61/794 |

These are **4/4 targeted catches**, not complete suite scores (the selections
come from suites of 10 and 35 mutations). Each baseline is green. No mutation
snapshot is updated. No full modern test or mutation sweep is run.

`t_transcript_evidence` adds one period fixture with 18 controls: exact positive,
same-site/input wrong text, sign, scale, units, final digit, newline, extra/missing
text, swapped fields, order, embedded NUL, callback count, equal truncation,
both overflow flags and reset. The modern run passes **18/18**. These test the
zero-budget comparison/capture mechanism; they do not validate a nonexistent
numeric allowance or approval protocol. Existing register controls still pass
**12/12**, keeping v1 unconditionally blocked.

Artifacts: `/tmp/opencode/transcript-batch/final/`, driven sequentially by
`/tmp/opencode/transcript_final.py`, `transcript_jobs.py` and
`transcript_batch.py` in `/tmp/opencode/`. The latter checks sequence numbers,
unique assertion/input pairs, lengths, exact outcomes, completeness and summary
totals while retaining both original byte strings in `.stderr` and `.json`.
`build.log` records a forced fresh modern build of the selected binaries;
`binary-sha256.json`, `inputs-and-objects-sha256.json`, compiler/version logs,
period image inspection, actual flags and individual status JSON files identify
the artifacts. Modern compiler: **Debian GCC/G++ 14.2.0-19**, ILP32/x87.
This is not a GCC 3 historical causation claim.

The preliminary directory is not the final gate packet: the first targeted
mutation runner timed out during its third selection, which was rerun; the
first phase run failed after its script was edited while running and is invalid.
That run also exposed concurrent compilation despite container `J=1`: the old
compile loop relied on `jobs` inside command substitution. The serial path now
calls `compile_one` synchronously. The final run must be read from its own
status/log, not inferred from preliminary PASS lines.

Final settled-source results (direct subprocess return codes, no tail pipelines):

* `make phase J=1 -j1`: **384 passed, 0 failed**, exit **0**, including the new
  18-control transcript fixture. Positive structural denominators and the phase
  boundary are in `final/phase.log`. The original period compiler reports
  `3.4.2 (Gentoo Linux 3.4.2-r2, ssp-3.4.1-1, pie-8.7.6.5)`; flags, including
  source `-ffast-math`, are preserved in the log rather than reconstructed from
  memory. No exemption is consulted by this tier.
* Selected modern parents: **4/4 pass**, exits **0**. Companions: **4/4 remain
  red**, exits **1**, with the denominators above. Their wrappers also exit
  **1**, **4/4**, as required while migration is unsupported.
* Transcript safety: **18/18** under modern and period builds. Numeric/storage
  safety: **54/54** in each ILP32 modern/no-define build. Graph/word safety:
  **17/17** in each modern/no-define build. Register safety: **12/12**. All
  safety drivers exit **0**; the injected differences inside those drivers
  deliberately produce failure diagnostics and are not real fixture failures.
* Final targeted mutation rerun: **4/4 caught**, four tool exits **0**,
  **0 uncaught, 0 unusable**. The assertion denominators in the table above
  are unchanged on this settled-source rerun.
* `refcheck`: **14050 references**, 0 unresolved, 0 pending, 0 stale; exit **0**.
  `git diff --check`: exit **0**.

Remaining work is still issue #172 / PR181: independent
review of this bounded repair; owner-approved diagnostic field budgets, if
justifiable; a build-bound, fail-closed authorization protocol with malformed,
duplicate, missing, unknown-assertion and same-failing-site corruption controls;
then entry-by-entry migration. All ten non-transcript entries retain their
section-2 blockers.

## 6. Single p4d migration attempt — stopped at the witness prerequisite

**Disposition: unmerged design and stronger fixture, zero authorization.** The
other 13 entries remain **BLOCKED**, and `t_v90p4dnan` is also still blocked.
`gccdiverge.py` and the register are unchanged. No assertion is exempted and no
numeric tolerance is introduced. Independent review is still required before
publication. This section supersedes the old p4d fixture's claim that flag 1
proved NaN; it does not establish a new compiler-causation claim.

### What prevented an honest migration

The reference V.90 tail, inspected with `tools/dis.py build/dsplibs_ref.o
0x268aa 0x26982`, computes the ratio, executes inline `fyl2x` at `0x268c2`,
stores the scaled result to a stack slot at `0x268d8`, then converts it to
integers for diagnostics. It reloads a saved float at `0x26956` before the
keep-rate comparison. The reconstruction also uses an inline logarithm under
the source build flags. The diagnostic callback receives a character and
integers, **not the original float bits**. Neither the flag nor that text is an
actual NaN witness. Recomputing `log10` in the test would witness a different
execution and is insufficient.

A possible next step is externally observing both real decision operands with
build-specific debugger/ptrace stops. That requires separate validated maps for
both methods and both compiler builds, preservation of the inferior's complete
x87 state, exactly-once hit/trial association, and instrumented/uninstrumented
controls. Reading arbitrary caller-stack offsets from C callbacks is not a safe
substitute. No such observer was implemented or validated in this pass; no raw
NaN observation is claimed. Consequently **migration stopped before implementing
an approval backend**, rather than substituting an inferred witness.

There is a second measured blocker: level 0 made the original transcript checks
compare empty strings. With diagnostics enabled at level 3, both synthetic
trials have unequal raw diagnostic bytes as well as different keep-rate flags.
These transcript differences remain hard failures. Even with a future NaN
witness, a keep-rate-only policy cannot approve this fixture while they fail.
This pass does not infer the meaning or cause of the differing encoded byte.

### Stronger strict fixture and bounded controls

`test/unit/t_v90p4dnan.cpp` now runs six fixed inputs for each of V.90 and V.92:
the original negative-energy/sample-300 synthetic trial; positive ratio 4;
zero ratio; and ratio 1 with thresholds 0, -1 and +1. The last three give exact
0 dB equality/below/above controls. The zero input tests log(0), not NaN.
The planted negative energy is **synthetic adversarial state, not evidence of
reachable modem state**. No general modem robustness or platform-independent
claim follows from these twelve cases.

Both sides have independent demappers and parameter storage. Checks cover
returned samples, expected progress/state/count/cursor, pointer identity,
immutable parameters, guards against their original seeds, full demapper bytes,
and every remaining demodulator byte. Only the independently checked pointers
and separately compared keep-rate field are normalized in comparison **copies**.
Production objects remain intact. Transcripts compare complete captured bytes,
callback counts and completeness, with nonempty-capture assertions. Finite
control flags have independent expected values on both sides. Parent assertions
are retained; no red mutation suite is registered.

`test/safety/test_v90p4dnan_fixture.py` runs actual binary probes, not source
mutations. Seven probes at synthetic trial 800000 produce additional named hard
failures: return, progress, state, unrelated state byte, guard, demapper cursor,
and parameter byte. Constant-zero and constant-one probes fail finite controls.
A same-site transcript probe is deliberately harder: the original assertion is
already red. Both executions still report the same Boolean failure and total;
the test checks the complete encoded evidence, the exact injected suffix,
callback increment, unchanged reference and all other trials. This demonstrates
the evidence difference without falsely calling a red exit a mutation catch.
An unknown probe is itself a hard failure. These are fixture-observer controls,
**not** validation of a structured exemption protocol.

### Minimal lossless protocol design (not implemented or authorized)

Use a dedicated evidence channel with versioned length-delimited records,
independent of human-readable stdout. The schema must have a closed set of keys
and record types, fixed maximum lengths, and reject duplicate keys. No caller
may register a contract by copying an observed failing transcript.

1. **Header:** schema ID, fixture ID, externally assigned supported-build ID,
   and exact expected assertion/input inventory version. Build identity must
   come from the wrapper's trusted build record, not whatever the binary says.
2. **Trial context:** fixed input ID including V.90/V.92 and case; original
   sample, count, period, threshold and before/after energy raw binary32 bits;
   both post-normalization energies; both actual observed ratio/log operands
   (width/endianness explicit), stop location, side and hit count. An integer
   bit classifier validates NaN on the actual log result consumed by the
   comparison. Context must match the predeclared input catalogue; a changed
   energy/threshold, finite witness, or missing observation rejects the run.
3. **Assertion:** stable semantic ID and input/side ID, typed raw got/want, all
   underlying bytes and lengths for objects/transcripts, callback counts and
   completeness. Include **passing and failing** assertions. Pointer fields
   need individually checked object identities; do not suppress arbitrary
   pointer-sized regions. Every byte suppressed from a canonical object must
   have an independent record. No Boolean transcript result or abbreviated
   object diagnostic can stand in for the underlying bytes.
4. **Final:** exact record, trial, assertion and failure counts, completed-run
   marker and channel digest. Independently recompute all outcomes from the
   records. Require every expected pair exactly once and no extra pairs, a
   complete final record, clean EOF, no timeout or signal, and ordinary raw
   exit 1 for the known modern divergence. Zero mismatches is stale, not an
   exception to approve. Period executes raw and must exit 0.

The only proposed exception is `p4d.keep_rate`, at the two predefined synthetic
trials, **raw modern 0 / reference 1**, with independently validated unordered
operands and exact input context. Other same-field values reject. Every other
assertion, including state/progress/return/transcript/guards at those very
trials, must pass. There is no fallback group policy. In particular the current
level-3 transcript failures would prevent authorization under this design.

**Identity/trust boundary:** initially support only an explicitly reviewed Linux
ELF32 little-endian i386/x87 build profile, separately naming Debian GCC/G++
14.2.0-19 and the period Gentoo 3.4.2-r2 control. This is a proposed bound, not
a currently supported authorization identity. Record actual per-TU compiler
commands (including source versus fixture/harness flags), compiler and linker
binary/version identities, container digest for period, architecture/ABI,
reference-object hash, source/header/apparatus hashes, object hashes, link
inputs/order and final executable hash. A git commit alone misses this dirty
tree; a fresh source hash does not prove an old object was rebuilt.

A trusted build recorder must bind that manifest to the actual completed build
and a reviewed profile, and an independently approved manifest digest must be
pinned outside the binary's evidence. A freely regenerated sidecar that simply
hashes whichever binary is presented is tautological and cannot authorize it.
The wrapper must verify current source/input and executable bytes against that
record, reject stale objects/manifests, and execute the verified file identity
(e.g. held descriptor with a safe execution mechanism) without a path-replacement
race. Later modification must fail the hash/identity check. Unreviewed compilers,
flags, architectures, environment overrides or manifests must fail closed. The
artifact hashes below are **audit identifiers only**, not such trusted approval.

**Unimplemented acceptance matrix:** the correct exceptional record must be
accepted only in an independently reviewed candidate validator; wrong modern or
reference flag, altered threshold/input, non-NaN witness, unrelated corruption
at the same failing trial, unexpected assertion/input/build IDs, duplicate,
truncated/missing records/final report, stale or edited binary, and SIGSEGV must
all reject. These protocol controls have **not** been run against a validator,
because the required observer and trusted build binding do not yet exist. The
existing 12 register controls continue to test unconditional fail-closed legacy
behavior; they are not substituted for this missing acceptance matrix.

Even an eventual approved wrapper must leave the raw modern binary red, so the
ordinary exit-code mutation caveat remains. Keep the strict parent and its
mutation observers; an assertion-aware mutation contract would be separate work.

### Verification and artifacts for this stopped candidate

All build/test work ran serially with `J=1 -j1`; no modern or mutation sweep.

| Measurement | Result |
|---|---|
| Raw modern sentinel, GCC/G++ 14.2.0-19, ELF32/x87 | **4/344 failed**, exit **1**: two keep-rate and two exact transcript assertions, at inputs 800000 and 800100 |
| Raw period sentinel, Gentoo 3.4.2-r2 | **344/344 passed**, exit **0** |
| Modern strict parent `t_v90p4ddec` | **93349/93349 passed** across eight groups, exit **0** |
| Fixture controls | **5/5 unittest methods passed**: baseline, seven additional-failure probes, two constant-flag probes, one raw-transcript corruption probe, unknown-probe rejection |
| Numeric/storage safety | **54/54** in each ILP32 modern/no-define build, exits **0** |
| Legacy register controls | **12/12**, exit **0**; no authorization enabled |
| Candidate wrapper | exit **1**, explicitly **BLOCKED** |
| `make phase J=1 -j1` | **384 passed, 0 failed**, exit **0**, positive structural denominators |
| `refcheck` | **14050 references**, 0 unresolved/pending/stale, exit **0** |
| `git diff --check` | exit **0** |

Artifacts: `/tmp/opencode/p4d-candidate/`. `status.json` and
`finish-status.json` record direct subprocess statuses and commands; `phase.log`,
`modern.log`, `period-sentinel-correct-path.log`, `parent.log`, `controls.log`,
`evidence.log` and individual `probe-*.log` preserve results. `blob-log-path.log`
and `modern-log-path.log` retain disassembly showing inline logarithms;
`artifact-sha256.json` identifies the binaries, reference object and selected
sources for audit only. The two sequential drivers are
`/tmp/opencode/p4d_candidate_verify.py` and `p4d_candidate_finish.py`.

The first driver completed phase successfully, then attempted the nonexistent
`build/period/test/t_v90p4dnan` path and raised `FileNotFoundError`. That empty
`period-sentinel.log` is **not a test result**. The second driver ran the correct
`build/period/t_v90p4dnan` and recorded exit 0 separately; no gate result is
inferred from the driver's exception. Preliminary fixture-control development
also exposed an ignored `transcript_exact` return; the settled fixture asserts
it explicitly and the same-site raw-evidence control now fires. Only the settled
artifact packet above supplies the reported control and gate denominators.
