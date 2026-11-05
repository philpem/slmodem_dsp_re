# Recovery tooling and model evaluation pilot

This pilot tests whether better candidate evidence, compiler diagnostics and
model routing reduce the work needed for a validated reconstruction. The measured wave is dated 2026-11-05 against baseline
`f87958ea59b935c8e3d2d99251b3fb6aa6ec17e2`. Final review and rescue outcomes
are recorded below; this is a small workload pilot, not a general model ranking.

## Acceptance evidence

The pilot has six deliverables:

1. Audit register width and bit position in REGALLOC classification, exercise
   positive and negative controls, and measure the effect on the current
   object inventory. A classification correction is not a source recovery.
2. Provide a reusable full-TU candidate checker and experiment ledger. Record
   native evidence, input identities, compiler configuration, component
   changes, fixture domains, negative controls and reopening conditions.
3. Improve compiler-stage comparison and bounded owner/value tracing. Report
   unsupported and ambiguous cases. Trace actual compiler decisions before
   extending the allocator model; demonstrate that observation preserves code.
4. Run source-recovery experiments with interacting changes. For two supported
   changes, compare baseline, A, B and A+B. A failed individual change does not
   disprove the combination. Adopt source only after native-evidence review,
   whole-TU collateral review and the period differential gate.
5. Run eight independent Sol–Astra task pairs, followed by bounded rescue
   reviews. Measure validated findings, mistakes, review work, elapsed time and
   available usage. Keep historical controls separate from unresolved tasks.
6. Publish the measured results and limitations, update working instructions
   and the playbook, and store durable conclusions in Hindsight.

## Paired model protocol

The initial comparison uses `gpt-6.1-sol` and `gpt-6-astra`, both at `high`
reasoning effort. Each task has a 15-minute investigation limit and a maximum
of 40 underlying tool calls, whichever comes first. Orchestration wrappers
are recorded separately. The initial briefs were ambiguous about this distinction;
it was clarified during the O2/I1 runs, so the budgets were not perfectly uniform. These are pilot limits, not measured
optimal settings. Record overruns and interrupted runs rather than treating
them as ordinary completions. Compilation and tool time count towards elapsed
time. The same effort label need not imply equal internal computation.

Eight tasks cover two cases each in tooling, ownership/width, interacting
source changes, and compiler diagnostics. Freeze briefs, input hashes, compiler
configuration and evidence before either member of a pair starts. Use separate
output directories and fresh agent contexts. Neither agent may inspect the
other's outputs or use live memory updates. Supply the same frozen memory
packet to both. Record any accidental exposure as contamination.

The artifact protocol and complete input manifest are stored under
`build/recovery-pilot/`. These build artifacts are local evidence, not durable
distribution dependencies. Summarise results and reproduction instructions in
this document before closing the pilot.

The initial tasks are:

| ID | Category | Task | Evidence role |
|---|---|---|---|
| T1 | Tooling | Register-renaming classifier audit | Historical control |
| T2 | Tooling | Persistent comparison cache/provenance audit | Unresolved audit |
| O1 | Ownership | FAXVMI constructor width nomination | Historical control |
| O2 | Ownership | CPt magnitude and caller residual | Unresolved recovery |
| I1 | Interaction | CrossDataLinks helper factoring | Historical control |
| I2 | Interaction | V90CP constructors and reset helpers | Unresolved recovery |
| R1 | Compiler | Psd scratch trace prediction limits | Historical control |
| R2 | Compiler | ADID residual allocation | Unresolved recovery |

Historical controls measure whether the workflow can recover or reject a
known result. They do not count as new fixes. Unresolved cases measure useful
new findings, including a supported partial recovery or a discriminating
experiment. A convincing explanation without verification earns no fix credit.

Use compiler results and executable controls as the primary grader. Record
review corrections separately. Hide model identity during initial assessment
where practical, and disclose where it was visible. Repeat disagreements and
test proposed routing rules on additional cases before generalising from this
small sample.

## Rescue reviews and cost

Have Astra review bounded Sol failures and a sample of negative conclusions.
Distinguish fresh independent attempts from reviews that receive Sol's work.
Have Sol review selected Astra results too. Report additional validated findings
and their incremental cost; do not credit a reviewer for repeating an existing
finding. These reviews test the risk of missed fixes rather than guaranteeing
that none remain.

Record model identifiers, effort, task limits, elapsed time, tool calls and
available input/output usage. Unavailable token or billing information remains
unknown. Do not infer it from response length or invent a per-task price.
Compare end-to-end cost, including failed attempts, handoffs and validation.

## Memory and provisional routing

Use Hindsight to retrieve prior hypotheses and decisions before production
experiments. The ledger supplies exact input hashes and evidence references;
memory supplies the reason for the decision. A stale record remains history,
but is not current validation. Reopening a negative result requires a stated
change in evidence, inputs or tested combination.

Sol is the provisional default. Candidate escalation triggers are conflicting
evidence, several interacting changes, unresolved allocator causality and
high-impact negative conclusions. These are hypotheses to test, not demonstrated
Astra strengths. Keep routing rules dated and linked to measured evidence.
AGENTS.md can guide agent selection; it cannot switch a running agent's model.

## Measured tooling and source results

The final census is 1,132 EXACT, 27 REGALLOC, 74 BYTES and 619 SIZE out of
1,852 symbols (720,125 native code bytes). EXACT means relocation-aware
instruction bytes under this tool's documented comparison, not literal identity
of the complete linked object. The source gain is `generateCPt`, 94 bytes.
Two REGALLOC-to-BYTES changes are stricter classification, not source losses.

| Apparatus | Result | Remaining limit |
|---|---|---|
| Register classifier | Width and bit-position checks; conditional-move destination reads; overwritten correspondence tracking. 38 alpha controls and 59 relocation controls pass, including Python `-O`. | Linear traversal does not model CFG joins, all implicit registers or ABI return obligations. REGALLOC is a triage label. |
| Comparison session | Content-addressed private snapshots prevent a write-and-restore race from attaching grades to the wrong input hash. Both models independently reproduced the old defect. 14 tests pass normally and with `-O`. | Captures one byte sequence per file, not an atomic snapshot of an entire build. Use frozen build inputs. |
| Experiment ledger | Explicit baseline/A/B/AB, whole-TU comparison, command-linked period and linked-body evidence, generated-output and analyzer hashes, freshness queries and frozen retrieval exports. 11 controls pass normally and with `-O`. | Arbitrary commands are trusted apparatus; a declared compiler identity or fixture domain is not independently proved by JSON. |
| Stage divergence | Anchored structural alignment, operand paths, retained register/source annotations, ambiguous and unmatched records; cgraph phase/call records with unparsed text retained. 27 control groups pass normally and with `-O`. | First observed difference is not necessarily the cause. Candidate dumps are not original compiler dumps. |
| Owner tracing | Bounded CFG and cdecl argument/stack provenance, access widths and bit positions; 17 real ELF control groups pass normally and with `-O`. | Loaded pointers and call returns remain site-local; only entry-argument roots and constant subobjects pair across objects. |
| Allocation observation | Read-only local-allocation decisions and global-pass entry, compiler/hash-pinned replay, 10 validation controls. Real run: 10 target / 141 TU choices, 2 target / 46 TU pass entries; assembly and object unchanged. An absent target fails explicitly. | Not a complete global allocator, reload or eviction model. |

The width/lane matrix exercised 1,152 register-view pairs: 216 accepted and
936 rejected, including 256 old false accepts. Width/lane repair alone did not
change the production census. Conditional-read and overwritten-map repairs
conservatively demoted `V92Phase4Modulator::recivedRt` and `V90Jd` C1. In the
former, linear traversal carries a mapping through a preceding tail jump; in
the latter, a dead low-byte value can legally be replaced. These are known
precision limits, not discoveries that those reconstructions are wrong.

The owner screen covered the 24 smallest of 37 eligible non-fax create/C1
symbols with native sizes 32–1,000 bytes. The refreshed snapshot-based run took 3.036 seconds. Native/candidate
MOV stores were 48/49 supported, 321/324 unknown and 8/8 ambiguous; unsupported
instruction counts were 103/96. There were zero nominations and zero tool
errors. Only about 13% of these decoded stores were supported: do not interpret
this as evidence that no owner or width defects remain. Manual review correctly
rejected the FAXVMI nomination because equal displacement `+0x1c` referred to
different owners (`framer->pack_bit` versus root `overflow`).

### The adopted CPt change and its TU interaction

`V92Phase4Modulator::generateCPt` keeps the same operations and unsigned-wide
magnitude followed by a signed-short conversion, but places the return tail
inside each exclusive branch. GCC merges the standalone tails while retaining
the native pair of signed conversions in both inlined caller arms. Native
witnesses are at `0x180dd/0x180e0` and `0x189a9/0x189ae`. A shared machine tail
does not establish a shared source tail.

Across 44 compared TU functions, EXACT increases from 33 to 34 with no exact
loss. Only `generateCPt`, `generateSymbol` and `reset` change. The caller grows
from 4,002 to 4,034 bytes against native 4,055. Reset remains 290 bytes but its
raw mismatch count rises from 46 to 190. Exports and emitted order are unchanged;
masked data payload and layout agree; 25 of 30 switch-table offsets move with
unchanged owners and resolved instruction boundaries. This collateral was
reviewed explicitly, not hidden by the aggregate gain.

Both observational captures have 102 scratch-search events and identical
per-function event counts. The first selected-register difference is event 57
in `generateCPt`: cursor 3 selects EAX and ends at 49 before the change, versus
EDX and cursor 2 afterwards. Reset's entry cursor changes from 50 to 3 and its
first selected scratch register from EAX to EBX. Reset's RTL is equal through
`27.flow2`, first differing at `28.peephole2`; the changed CPt/caller differ from
initial RTL. This is direct evidence of TU-carried scratch state, not a claim
that every allocator decision is TU-global.

The full `make phase BUILD=build/recovery-pilot/phase J=8` gate passed 398 period
tests and all structural checks. Focused `t_v92p4sym`, `t_v92p4mod` and
`t_vpcmrunpcm` also passed. Parent verification checked all three changed bodies
in all three linked fixtures: nine bodies, 13,254 body bytes with
all PC32/R32 fields checked, including 90 table destinations. A control removing
the terminal signed-short conversions fails at `generateCPt` as intended.
Eight existing mutation anchors retain their original faults, applied to both
new tails where appropriate; no mutation campaign or snapshot refresh was run.

Fixture domains remain bounded. The existing symbol grid contains synthetic
component states; it does not prove lifecycle reachability. Sol's additional
32 constructor/reset histories run 700 state-0 generated symbols and 300 direct
CPt calls each, passing 128,128 checks. They do not cover state 1. Exact code,
unchanged operations and the period checks support adoption, not a claim of
unique original source spelling or complete modem coverage.

The interaction checker also replayed the historical CrossDataLinks four-cell
experiment: baseline BYTES, input-helper-only SIZE, output-helper-only BYTES,
both helpers REGALLOC. Each cell passes 178,693 component checks and linked
CrossDataLinks body/constant verification; the other 22 TU functions are
unchanged. A wrong-gain control (1/31999 instead of 1/32000) fails at `cross f[]`.
This is validation of the new apparatus, not another new recovery.

## Eight paired tasks: parent assessment

Each task used independent fresh contexts and the same frozen baseline packet:
2,350 hashed files, including sources, tools, 300 period objects, native object
and selected compiler evidence. No live Hindsight updates or peer output were
available to paired workers. Source adoption and final checks happened outside
those blinded task contexts. Parent assessment was **not blind to model identity**.

| Task | Sol outcome | Astra outcome | Measured elapsed seconds (Sol / Astra) |
|---|---|---|---|
| T1, historical classifier audit | Native controls for lane, conditional read and other limits; patch missed AX/AL width and overwritten correspondence | Also found width mismatch and supplied overwritten-map repair | 417 / 348 |
| T2, unresolved cache audit | Reproduced write-and-restore provenance bug; snapshot repair | Independently reproduced same bug; equivalent snapshot repair | 282 / 385 |
| O1, historical owner control | Correctly rejected false width nomination | Correctly rejected same nomination | 100 / 153 |
| O2, unresolved CPt | Found and differentially validated duplicated-tail recovery, then adopted by parent | 34 builds, no gain; located caller conversion elimination at combine | 502 / 640 |
| I1, historical interaction | Recovered helper A+B REGALLOC interaction | Same interaction and no unrelated body changes | 326 / 305 |
| I2, unresolved V90CP | 19 cells, no justified gain | 18 variants, no justified gain; noticed swapped C1/C2 body correspondence | 536 / 466 |
| R1, historical trace control | Correctly found supplied traces identical, not independent heldout prediction | Same conclusion and discriminating negative control | at least 244 / at least 290 |
| R2, unresolved ADID | Confirmed one-byte scratch choice with cursor intervention; no source proposal | Traced same mechanism; two predecessor changes and their combination did not fix it | 374 / 551 |

These durations include tool/compiler work from each report's first measured
clock. Some clocks start after initial reads; R1 explicitly lacks total-task
timestamps. Starts were staggered on a shared host. T1 Sol briefly wrote a
Python cache in the frozen tree; it was removed and input hashes rechecked.
Several workers attempted unavailable fixture/docs paths in the intentionally
bounded packet and did not substitute live inputs. O2 Astra reached the 40-call
investigation cap and used two further report-maintenance calls. These protocol
limitations preclude a clean latency or efficiency ranking.

Model token counts, billing and complete CPU costs were unavailable. They remain
unknown; aggregate thread usage is not a per-model measure. Tool-call counts
and command logs are in each local report. Parent reran 23 T1 executable pairs
(46 binaries), reviewed O1's 17 ELF instruction witnesses, reproduced both T2
race/repair controls and verified the adopted recovery independently. Historical
controls are not credited as new source fixes.

R1 replay passes 8/8 events in both directions because the supplied traces are
byte-identical. Its 24 order permutations are conditional predictions, not 24
forward compiler validations. R2's forced cursor produces an exact dead pop
byte but is a diagnostic intervention, not recoverable source. Do not carry
compiler-state edits or constructor-clone renaming into reconstruction source.

## Bounded rescue and final review

These reviews receive earlier outputs and therefore are not independent paired
attempts. They measure incremental checking and new discriminators.

| Review | Additional verified result | Measured cost |
|---|---|---|
| Astra reviewing Sol O2 positive | Supports adoption; rechecked native bytes/relocation, all nine linked bodies, existing binaries and 32 reset histories. Two new linked-body corruption controls reject instruction and relocation faults. | 379 seconds, 21 calls including coordination |
| Astra reviewing Sol I2 negative | No recovery in six new cells plus two controls. Clone streams are equal through flow2 and first differ at peephole2; unit-at-a-time settings do not change complete objects. This narrows the mechanism before register renaming. | 300 seconds, 21 calls |
| Sol reviewing Astra T1 repairs | Eight heldout native pairs: four equivalent accepted, four unequal rejected. Two additional equivalent pairs reproduce the known conservative CFG/low-byte demotions. No additional false accept found. | 20 calls including coordination; total elapsed unavailable |
| Final independent Astra review | Found the same provenance-race class in owner tracing and stage parsing. Both received read-once input fixes and executable/file-backed regressions before signoff. | Two P2 findings resolved and independently retested; no billing or token data exposed |

The I2 rescue initially read the live README and method index contrary to its
frozen-only brief; this is disclosed in its report. No live source or peer I2
Astra outcome was supplied. The T1 rescue is heldout control validation, not a
second blind Sol–Astra race. The disagreement has not been replicated in a new
blind paired task, so category-level routing remains provisional. The positive
O2 rescue found no hidden Astra-only recovery; the negative I2 rescue added a
useful stage discriminator, not a new exact function.

Review also corrected a common apparatus failure mode: endpoint hashes cannot
detect a transient rewrite followed by restoration. The comparison session and
owner tracer now analyze private snapshots of bytes captured and hashed together;
the stage tool parses and hashes the same captured dump bytes. These are per-file
snapshots, not atomic snapshots of an entire source/compiler build. Use immutable
captures and preserve input manifests for multi-file experiments.

## Priorities after this pilot

1. Use the full-TU checker and ledger for every candidate batch. Preserve failed
   component cells and test supported combinations before dismissing them.
   Stable input snapshots and freshness checks prevent wasted or false results.
2. Extend owner tracing with path- and epoch-aware loaded-pointer provenance,
   using manual false-nomination cases as negative controls. Its current low
   coverage is the clearest measured gap in automatic candidate screening.
3. Add bounded value/def-use correspondence around the first meaningful RTL
   divergence, retaining ambiguous mappings. The CPt combine-stage result is a
   useful location and variable clue even without original RTL.
4. Extend read-only allocator observation through global choices, evictions and
   reload before implementing a predictive model of those decisions. Measure
   actual residual families that such a model could close. The current evidence
   supports scratch-state tracing, not a wholesale allocator simulator yet.
5. Improve REGALLOC's CFG/implicit-register treatment separately from source
   work. Keep it a screening label until executable controls establish broader
   soundness; never let its label waive the period differential.

Use Sol as the provisional general worker and Astra for selected independent
adversarial audits, conflicting evidence and high-impact negative reviews.
The evidence does not show that Astra should own every hard reconstruction:
Sol found this wave's source gain and both solved the cache bug. A small pilot
cannot guarantee that either model missed nothing. Revisit routing after more
heldout tasks, recording additional fixes and review cost rather than judging
explanatory confidence or response length.

## Reproduction and artifact scope

Run apparatus controls independently of the reconstruction gate:

```sh
python3 tools/toolchain/byteident.py --self-test
python3 tools/gcc3_search_compare_test.py
python3 tools/recovery_experiment_test.py
python3 tools/gcc3_owner_trace_test.py
python3 tools/gcc3_alloc_trace_test.py
python3 tools/gcc3_stage_divergence_validate.py --help
make phase BUILD=build/recovery-pilot/phase J=8
python3 tools/toolchain/byteident.py --ratchet --limit 0
```

Repeat the Python controls with `python3 -O` when changing guards. The stage
validator and observation tools require explicit fresh output/capture arguments;
use their `--help` interfaces. The experiment manifest schema and command/evidence
contract are documented in `tools/recovery_experiment.py`'s module docstring.
`run --manifest FILE --ledger DIR`, `query --symbol NAME --ledger DIR` and
`export --tu PATH --ledger DIR` provide recording, freshness checks and frozen
memory packets. Exported packets are retrieval evidence, not proof of fresh
validation. Retrieve reasons from Hindsight, check artifact hashes, and record
what changed before reopening a negative result.

Local evidence is under `build/recovery-pilot/`: `post-cpt-census.json`,
`cpt-parent-comparison.json`, `cpt-linked-proof.json`, `cpt-causality.json`,
`cpt-*-stages-final.json`, `phase-cpt.log`, `owner/residual-screen-final.json`,
`allocator/final-{positive,absent}/`, `cross3/`, `eval/` and `rescue/`.
Build artifacts are not versioned dependencies. The durable conclusions,
controls and commands above must remain understandable without them.
