# V34 handshake body placement

Ordinary placement of one transmit arm does not recover the native complete
call graph under the retained period profile. A six-cell experiment replaces
one unsupported helper boundary with three or six others. The broader native
root remains an open structural recovery problem.

## Evidence and previous experiments

At source pin `dfc23b7d1e92f5913b22b124771211d3e4650771`, the native
`v34handshak` is 61,541 bytes with 394 calls and three indirect jumps. The
captured period baseline is 12,867 bytes with 113 calls, two indirect jumps
and 24 emitted local functions absent from the native object. The inventory
resolves ordinary local calls and relocation-backed calls. A call-only count
must not be used to exclude tail-jump consumers.

F11503 already established that placing the nineteen transmit arms in a shared
header did not make all of them inline. The five still outlined are
`v34tx1_trnseg4`, `v34tx1_trnseg4a`, `v34tx1_jtxmit`, `v34tx1_xmitmp` and
`v34tx1_tx_dpsk`. The header's claim that header placement makes every arm
inline is historical and overstates the measured result.

F11512 tested seven different repeated stragglers as macros and
`always_inline`, under retained and diagnostic `localonly` profiles. Macro
expansion duplicated the repeated transmit switch instead of recovering the
native shared dispatch. Its whole-closure `always_inline` control reached
165,000 bytes. These are prior results, not discoveries of this investigation.
F11523 refuted the proposed distinct native exit classes: the coloured
register paths reach the same loop test.

## Six-cell case66 control

`tools/recovery_v34_trnseg4a_case.py` crosses the old snapshot loop index
`int i` with the independently audited `short i`. Within each axis it compares
the outlined baseline, ordinary placement of case66, and ordinary placement
of case66 plus its snapshot/rates bodies. Each body has one physical include
home reused by the direct fixture facade. There are no new optimisation
options, forced inline attributes, register constraints or padding.

| Snapshot index | Placement | Root bytes | Calls | TU functions |
|---|---|---:|---:|---:|
| `int` | Outlined | 12,867 | 113 | 53 |
| `short` | Outlined | 12,867 | 113 | 53 |
| `int` | Case66 | 12,544 | 109 | 55 |
| `short` | Case66 | 12,544 | 109 | 55 |
| `int` | Case66, snapshot and rates | 11,587 | 103 | 58 |
| `short` | Case66, snapshot and rates | 11,599 | 103 | 58 |

Both outlined controls reproduce their frozen complete TU objects raw. All
six cells preserve strong exports/bindings, data bytes and all nine exact
bodies among the 29 native-common bodies. The direct fixture objects are raw
identical across all six cells. Code and relocation-consumer changes are
recorded for every common body, including non-exact bodies.

The compiler trace identifies the mechanism. The baseline inlines the snapshot
and rates bodies into the arm, then rejects the arm at the inline limit.
Putting the arm into the root causes snapshot, rates and `t3m_micro49` to hit
the limit instead. Placing the nested bodies directly removes those two calls
but newly outlines `v34tx1_exmit`, `v34tx1_ppseg`, `v34tx1_sbarseg`,
`v34tx1_silence` and `v34tx1_txmd`, as well as `t3m_micro49`.

All four arm exits preserve expression evaluation and rejoin the single while
test. Native early exits at 0x63941, 0x6409a and 0x6431f reach the shared loop
condition at 0x629e7. No exit classes are invented.

Each existing `t_v34hstx1` period run passes 26,039 checks: six tests, 156,234
checks in total. These are direct-arm component probes with synthetic internal
states and the reference once-per-block tail. They establish this component
boundary; they do not establish modem lifecycle reachability. No fixtures,
mutations or tolerances were added.

No candidate was adopted. This decision follows the displaced unsupported
boundaries in the complete graph, rather than the larger size gap or absence
of exact gains.

## Replay and artifacts

Run from the worktree root:

```sh
python3 tools/recovery_v34_handshake_boundary.py
python3 tools/recovery_v34_trnseg4a_case.py
python3 tools/recovery_v34_trnseg4a_case.py --period
```

`build/v34-trnseg4a-case293/` holds frozen baseline objects/config, source and
header overlays, compiler and assembler identity, actual complete commands,
object hashes, dumps, complete-body audit, call inventory and per-cell period
logs. The compiler helper appends `DSPLIB_REPRODUCE_BUGS` after configurable
flags. For a fresh output directory, pass `--baseline-dir` and
`--snapshot-object`; the latter can name the preserved `frozen/snapshot.o`
from the existing experiment instead of a temporary width-audit output.

The boundary inventory uses the captured frozen `dfc23b7d1` baseline and its
raw-identical compiler trace. It does not assert that a later canonical object
matches that historical trace.

## Broader diagnostic scope

At `94caa17be36d0c8e53c3eb92827bf68fb874331f`, the source graph contains
114 defined native-absent helpers reachable from the root. The 23 emitted
native-root fragments absorb another 91 source helpers. The 24th extra,
`probe_preemp_shape`, belongs to `probeselect` and is excluded from root
placement. Native-symbol call boundaries remain outside this closure.

`tools/recovery_v34_root_closure.py` tests this full closure as a diagnostic
body expansion with hygienic typed parameter bindings and local return labels.
It is not an approved production macro rewrite. This extends the source scope
beyond the six-cell experiment while retaining its compiler profile; the
prior seven-macro and whole-closure forced-inline results remain explicit
controls on interpretation.

## Full closure and shared dispatch result

The full 114-body expansion removes all 23 emitted root-fragment helpers.
Its root is 149,526 bytes, with 410 direct calls and 106 indirect jumps. The
initial period RTL already contains one 82-entry table, one 40-entry table
and **104 copies of the 70-entry transmit dispatch**. The duplication therefore
precedes CSE, register allocation and scheduling. It reproduces the structural
problem behind F11512 at a broader source scope; expansion alone does not
explain the native shared control flow.

A further isolated cell preserves the complete expansion but sends every
terminal transmit-dispatch route to one root-local label and one dispatch
body. Arguments are evaluated once. The four used frame fields (`obj`, `m`,
`rx`, `progress`) and the separate signed-short transmit-state argument are
copied into root-lifetime locals before the jump. No pointer to a helper's
local frame escapes its scope.

The source audit checks 91 call contexts across 27 owner functions in the
28-node dispatch ancestry. This includes 56 terminal `t3c_txblock` contexts,
two terminal `t3m_txblock` contexts, and one valued helper whose sole caller
tests the result and immediately returns. Independent review by the typed
offset audit agent confirmed that none of the nineteen table1 transmit arms,
or their source-graph descendants, reaches either dispatch helper. This keeps
the table1 queue/fill backedge separate from table2 completion.

Native table1 arm exits converge on the queue/fill comparison at
0x629e7, whose `jl` reaches 0x62950. Table2 dispatch is the indirect jump at
0x62b00. Its common completion at 0x62a40 performs the tail operations and
reaches the epilogue at 0x62ad8 and `ret` at 0x62ae2. The isolated shared label
executes that table2 body and its original tail before returning. It does
not turn table1 loop continuations into root returns. The context census is
bounded to the listed source graph and fixtures; it does not prove every
possible runtime state or unlisted caller.

| Cell | Root bytes | Direct calls | Indirect jumps | Initial RTL tables |
|---|---:|---:|---:|---|
| Native | 61,541 | 394 | 3 | Native dispatch sizes: 82, 70, 40 |
| Outlined baseline | 12,867 | 113 | 2 | 82, 40 |
| Full body expansion | 149,526 | 410 | 106 | 82, 104 × 70, 40 |
| Full expansion, shared table2 | 66,889 | 413 | 3 | 82, 40, 70 |

The shared cell preserves strong bindings, canonical data bytes and all nine
exact bodies among the 29 native-common bodies. Eleven common bodies change;
the complete audit retains their bytes and relocations. Changed verdicts are
`getbit` SIZE 14→18, `V34SetupDemodulator` SIZE 11→41, `datapumpv34` SIZE 7→23,
`v34handshakinit` SIZE 11→12, `v34setuptxmit` BYTES 239→238, and the root
SIZE 48,674→5,348. Other changed bodies retain their former verdict distance.
The unrelated `probe_preemp_shape` remains emitted.

All three placement cells pass all three existing fixtures:

| Fixture | Checks per cell | Boundary |
|---|---:|---|
| `t_v34hshak` | 14,368,945 | Existing handshake component probes |
| `t_v34hsfsk` | 3,795 | Existing handshake/FSK component probes |
| `t_v34hstx1` | 26,039 | Direct transmit-arm component probes |
| Total | 14,398,779 | Three fixtures per cell |

The selected production object and matching direct-fixture object are copied
into isolated period directories, hashed, and checked for replacement after
the run. These results supplement the native graph and source-context evidence;
they do not establish public modem lifecycle reachability.

## Diagnostic and receiver-setup boundaries

The shared cell has all native root callee targets. Its only additional target
is `setupreceiver`, called twice. Both source call sites correspond to native
inlined regions: at 0x64c04 and 0x6a0c8, the root calls `rxinit`, then reads
rate configuration words at +0xaa96/+0xaaa8 and executes the receiver baud and
carrier setup. The source calls are in `v34tx1_trnseg4` and `t4_trn2`.
Standalone native `setupreceiver` remains an exported function; its existence
does not establish a call boundary at these two root sites.

Native and shared-cell roots reference the same 110 unique string texts, with
no missing or added text. Twelve string-reference multiplicities differ.
These are not twelve new source messages:

| Message family | Native refs | Shared refs | Physical source site |
|---|---:|---:|---|
| DET_SYNC, info1a | 1 | 2 | `t41_marks_late` |
| DET_SYNC, info1c | 1 | 2 | `t41_marks_late` |
| Repeated info0 after 1200 | 1 | 7 | `t46_body_repeated_late` |
| Phase-answer count3 | 1 | 2 | `t46_info0_counting` |
| FRR signal drop after NACK | 1 | 4 | `t41_frr_nack` |
| Detected info0 | 1 | 2 | `t46_body_detected` |
| Microstate transition | 53 | 62 | `hs_set_microstate` |
| Transmit-state transition | 60 | 66 | `hs_set_txstate` |
| MP sequence | 1 | 2 | Conditional format in `t4_mp_print` |
| MP1 sequence | 1 | 2 | Same conditional source call |
| Receiver AGC gain | 3 | 1 | Two native inlined receiver-setup copies |
| Receiver baud/carrier | 7 | 5 | Same two inlined copies |

The extra occurrences come from cloned source blocks; the two reduced families
are hidden behind the two remaining `setupreceiver` calls. Static multiplicity
is not a transcript equivalence proof. Runtime transcript comparisons retain
their existing period fixture scope.

`tools/recovery_v34_debug_calls.py` resolves the actual outgoing format slot
by bounded backwards CFG analysis, rather than assigning a nearby string to a
call. It resolves all 262 native and 286 shared-cell debug calls. Three native
calls and five shared-cell calls admit two conditional format literals; these
are retained as format sets. Partial-register aliases invalidate a tracked
register, ESP adjustments stop stack-slot tracking, call-clobbered registers
become unknown, and omitted indirect/table predecessors remain unknown. Other
arguments are reported only where the model resolves them; unresolved loads
and entry values are not treated as equivalent arguments. The resolver makes
no complete argument- or path-equivalence claim.

## Replay of broader controls

```sh
python3 tools/recovery_v34_root_closure.py
python3 tools/recovery_v34_root_closure.py --period
python3 tools/recovery_v34_root_closure.py --period-roots
python3 tools/recovery_v34_debug_controls.py
python3 tools/recovery_v34_debug_calls.py
```

`build/v34-root-closure293/` holds the pinned domain, frozen inputs, complete
commands, overlays, dumps, all-body collateral, terminal-context census,
initial-RTL dispatch inventory, format consumers and period logs. The tool
supports `--cell` for a named cell without rebuilding the other objects.
Rejected generator attempts are retained separately: one incorrectly renamed
member tokens; another omitted function facades needed by the fixture's
function-pointer table. Neither attempt supplies a valid differential result.
The source pins remain available through
`archive/pr293-before-rebase-20261018` after the PR rebase.

The generated macro source remains diagnostic. A production design must retain
one body home and direct fixture access while expressing the native ordinary
cases and shared table2 continuation clearly. It must also account for the two
receiver-setup inline boundaries and the residual cloned diagnostic blocks.
No production source change is approved by this experiment alone.

Resolver apparatus controls assemble seven one-call objects with the period
toolchain. The positive control resolves its one format. Six negatives remain
unresolved or non-string as intended: a null format, removed outgoing format
assignment, intervening call clobber, partial-register write, ESP adjustment,
and omitted indirect predecessor. The removed-assignment object still contains
the referenced literal in a register, so a string census alone would not catch
it. A separate actual ELF-string perturbation changes a known referenced
format while preserving all 286 candidate calls; the format comparison rejects
it. The control log reports each denominator explicitly.

All five V34 investigation CLI entry points parse `--help` before importing
experiment dependencies, compiling or writing artifacts. Importing the
diagnostic resolver or its control runner starts no subprocess or artifact
write; resolver dependencies that perform toolchain discovery load only when
an audit is requested. These properties were checked with nonexistent output
paths and a subprocess launch forbidden during import.

The control runner accepts an independent output directory and frozen config:

```sh
python3 tools/recovery_v34_debug_controls.py --help
python3 tools/recovery_v34_debug_controls.py \
  --out build/v34-root-closure293/resolver-controls-cli \
  --config build/v34-root-closure293/frozen/.build-config
```

The earlier accidental `--help` invocation's sandbox Docker failure is retained
at `resolver-controls/invalid-cli-help-sandbox/positive.compile.log`. Its result
is invalid apparatus evidence; the repaired CLI reruns all seven controls.

The three-fixture claim is substantiated by three complementary summaries:
`period-full-root.json` retains the outlined and full-expansion direct-arm rows;
`period.json` retains the shared-table2 direct-arm row; `period-roots.json`
contains both root fixtures for all three cells. The separate
`verified-fixture-provenance.json` records an audit of all nine combinations.
It checks each recorded production-object hash against both its cell object
and its actual period-directory copy. For the direct-arm fixture it also
checks the matching fixture-object hash at both locations. All three direct
runs report 53 + 46 + 25,940 = 26,039 checks. This is evidence from the broader
closure experiment, independent of the earlier six-cell case66 runs.

The shared production-object SHA-256 is
`70ef03e2a83de4d8baa723006d2d87a70f62cde6ba80bdbc1d79b6ddda470cad`;
its direct-fixture SHA-256 is
`fc8264287b6b30f641f5056b8bd89322e44acafc1e50b361b3060a838f7b13ff`.
Future single-cell compilation, audit and period runs merge existing summary
rows instead of discarding other cells. Historical rows have not been invented
or retroactively presented as newly executed tests.

## Merged-source continuation checkpoint

The ordinary continuation experiment has been replayed on merged source pin
`28ccee0669a7f02a1663736b10900d239928b824`. Artifacts are separate from the
historical runs, under `build/v34-continuations-current/`. The unchanged full TU
reproduces the current canonical object raw, SHA-256
`6b4972bed29ab1d4115bc518736e0efe957d010664c4db6d2ea14073838416d9`.
The frozen canonical object, complete build configuration and provenance are
in `canonical-baseline/`; compiler/assembler identity and actual complete
commands are recorded by `results.json` and `identity.log`. Gentoo period
flags and `DSPLIB_REPRODUCE_BUGS` remain unchanged.

The generator retains PR295's normal-only `t3m_info0_carrier` helper and
predicate. It asserts that the complete `t3m_micro47` body and the normal helper
text survive verbatim in all three cells; `continuation-domain.json` records
the assertion result and body hash. Period BUGS mode excludes the normal-only
predicate without deleting it from the source overlays.

| Cell | Root bytes | Direct calls | Initial RTL tables | TU bodies | Native-absent extras | Changed common bodies |
|---|---:|---:|---|---:|---:|---:|
| Unchanged | 12,867 | 113 | 82, 40 | 53 | 24 | 0 |
| Ordinary terminal continuations | 24,868 | 199 | 82, 40, 70 | 48 | 19 | 17 |
| Continuations and ordinary table1 | 42,026 | 308 | 82, 40, 70 | 38 | 9 | 14 |

All 29 native-common bodies are audited in each cell. All nine exact bodies,
strong bindings and canonical data bytes remain unchanged. The terminal cell
changes `V34SetupDemodulator` from SIZE 11 to SIZE 41 as well as reducing the
root gap. The table1 cell additionally changes `getbit` SIZE 14→18,
`datapumpv34` SIZE 7→23 and `v34handshakinit` SIZE 11→12. Complete non-exact
bodies and relocation-consumer changes are retained in `audit.json`.

The source uses 27 ordinary void continuation labels, an ordinary scoped valued
helper and ordinary frame-initialisation blocks. Table1 adds nineteen ordinary
case bodies. Each of the 48 physical `.inc` bodies is also consumed by its
original function facade where needed; no GNU statement-expression body macros
or forced-inline attributes are introduced. The direct transmit fixture object
is raw-identical in all three cells. Independent review of the 48 bodies,
27 transfer macros and 91 transfer contexts found no skipped post-call work,
escaped local-frame pointer or incorrect nested return; its scope remains
static source review supplemented by the stated fixtures.

All nine cell/fixture combinations pass. Per cell: `t_v34hstx1` 26,039 checks,
`t_v34hshak` 14,368,945 checks and `t_v34hsfsk` 3,795 checks, totalling
14,398,779. `period.json`, `period-roots.json` and
`verified-fixture-provenance.json` verify the exact selected production and
fixture object hashes at the actual period directories. These are the three
focused component fixtures, not a new whole-phase or modem lifecycle claim.

Replay from the worktree root:

```sh
python3 tools/recovery_v34_continuations.py --help
python3 tools/recovery_v34_continuations.py \
  --revision 28ccee0669a7f02a1663736b10900d239928b824 \
  --out build/v34-continuations-current
python3 tools/recovery_v34_continuations.py \
  --revision 28ccee0669a7f02a1663736b10900d239928b824 \
  --out build/v34-continuations-current --period
```

`--revision` also permits an explicit historical source pin; immutable output
manifests reject mixing revisions or canonical inputs in one output directory.
`--canonical-object` and `--config` select archived baseline inputs when needed.
The current results' inherited case66 hypothesis description was corrected to
the actual three-cell continuation domain; the original inherited text is
preserved in the same JSON record, and historical experiment records were not
modified.

Production source remains unchanged at this checkpoint. The table1 partial is
a supported retention candidate: it restores native shared dispatch and reduces
unsupported boundaries while passing the component comparisons. Readable
multiline transfer bindings were checked separately under `multiline/`: all
three complete production objects and all three direct-fixture objects are
raw-identical to the objects used by the nine passing focused runs.
`multiline-identity.json` records the six pairs of hashes.
The next separate discriminator is the single-call T72 ladder and its two
native-inline measurement bodies. That experiment must retain the ordering of
`rxtiming`, count/sample reloads, both `dftenergy` calls, zero/final accumulator
stores, the zero-denominator branch and the subsequent bin reinitialisers.
T72 expansion is paused while this source checkpoint is reviewed.

### Integration and whole-object limits

The multiline table1 candidate is installed as the shared-body implementation.
The canonical Gentoo object and the runtime object both equal the validated
candidate raw, SHA-256
`abd7bd4b768eccf6d7ce528e164542fc212fb32cd29d5eea602e47f64ef09ed5`.
All 300 runtime objects match their canonical counterparts. The final
`make phase J=4` passes 393 period fixtures with no failures and all structural
checks; the object hashes were checked again after that run. The fresh census remains
1,109/1,852 exact, with the same exact-name set as the retained floor:
32 REGALLOC, 78 BYTES, 633 SIZE, and no unresolved or relocation-only verdicts.

The initial installation preserved generated-file timestamps, allowing the
canonical build to reuse the previous object. The raw-object comparison caught
this; installation timestamps were refreshed and the build rerun. An overlapping
whole-tree before-census was marked invalid and excluded. The partial-link
baseline below instead reconstructs all 300 inputs by their recorded PR293
landing hashes; only the V34 object differs after adoption. Both links use the
same recovered 300-input manifest and period linker.

| Partial-link measurement | Before | After |
| --- | ---: | ---: |
| Positioned reference bytes equal / 943,398 | 72,202 | 71,784 |
| Exact relocations / 18,317 | 1,002 | 997 |
| Exact symbols / 2,931 | 457 | 457 |
| Exact content sections | 68 | 68 |
| Candidate `.text` bytes (reference 728,304) | 712,329 | 703,609 |

Both strict comparisons remain DIFFERENT. The shared-body recovery removes
duplicated helper code as it moves operations into the root; the resulting
layout shifts reduce positioned-byte matches. This is an explained structural
recovery, not a whole-object byte gain. The root remains 19,515 bytes shorter
than native, and the remaining helper boundaries need investigation.
The complete comparisons and input manifests are in
`build/v34-continuation-partial/{before,after}/`.

Extraction moves mutation anchors as well as source. The migration preserves
all 16 affected suite identities, binaries and 2,428 mutation rows, plus 29
non-mutation entries. Of the mutation rows, 1,455 remain in place, 927 move
verbatim, 43 adapt their return syntax, and three preserve their dispatch-fault
scope in the ordinary cases. Explicit per-row physical paths retain their
original function owner; the shared resolver proves the include belongs to
that function and respects quoted-include precedence. Conditional includes do
not supply ownership proof. This maintains the static apparatus without a
runtime mutation campaign or snapshot refresh.

The apparatus has 24 resolver, restoration and live-mutant detector controls;
all 10,044 targets across 285 suites pass the final static checks. The live-mutant
detector follows the same physical paths while permitting an applied mutant's
original anchor to be absent. It still rejects invalid ownership and paths.

After adoption, the historical outlined cell was replayed against all three
focused fixtures: 14,398,779 checks pass. Its production object remains
`6b4972be...` in every isolated runtime directory, while the installed canonical
object remains `abd7bd4b...`. The full hashes and fixture identities are in
`build/v34-continuations-current/historical-replay-final/replay-identity.json`.
This control verifies that the replay preserves the old body rather than
silently rebuilding it from the newly installed source.

The next T72 cell is bounded to one ladder expansion and two measurement
expansions. Native ladder entry is `0x6551a`, with `rxtiming` at `0x65524`;
the measurement regions are `0x69cd7..0x69d78` and `0x6a6c8..0x6a760`.
Keep the sample/count reads after `rxtiming` and preserve both zero-denominator
rejoins into their reinitialisers. The often-mentioned `0x64c04` and `0x6a0c8`
regions instead concern receiver setup and belong to a separate experiment.

After installation, replay uses the unchanged inputs already frozen under
`OUT/canonical-baseline/`. The tool verifies their hashes and source pin and
preserves their original manifest paths. It does not substitute the newly
adopted canonical object for the historical unchanged baseline. Isolated period
runs keep the selected compiled object and fixture newer than the installed
source, then assert both hashes after `make`; this prevents a timestamp rebuild
from silently replacing the baseline cell with adopted source.

For a fresh output directory, name the archived unchanged inputs explicitly:

```sh
python3 tools/recovery_v34_continuations.py \
  --revision 28ccee0669a7f02a1663736b10900d239928b824 \
  --out build/v34-continuations-current/fresh-replay \
  --canonical-object build/v34-continuations-current/canonical-baseline/src_pump_v34_V34hshak.c.o \
  --config build/v34-continuations-current/canonical-baseline/.build-config
```

A new output directory without both archived inputs is rejected. Existing
outputs can be audited or replayed without those options; passing alternate
input paths verifies identical bytes without rewriting the original provenance.
The frozen unchanged-object hash remains `6b4972be...`; the installed candidate
hash is `abd7bd4b...` and serves a different comparison role.

The final EOF/directive cleanup was checked in
`build/v34-continuations-current/format-final/`. All 48 include bodies now end
with exactly one newline, and the nested valued-helper macro is undefined
before its replacement definition. All six generated production/fixture
objects remain raw-identical to the validated three-cell checkpoint.
`installed-identity.json` additionally records direct period-compiler builds
of the installed production source and direct fixture, matching SHA-256
`abd7bd4b768eccf6d7ce528e164542fc212fb32cd29d5eea602e47f64ef09ed5` and
`2f7d62cc46bfd4ef030f20aca6dc944a1a4bb7ef393f7ded1b276bff9f1b420f`.
No further full-phase run was performed as part of this cleanup verification.
