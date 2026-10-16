# Source recovery: escaping the current plateau

Prepared 16 October 2026 for implementation by Sol. Target: recover the
original source structure and reach complete object identity, preserving the
Gentoo differential authority. This is an investigation programme, not a
claim that a finite search guarantees recovery of the original source.

## Baseline and scope

Use upstream **`4169cb3b3108a840a51bca9ebfa086102c4c69c6`**, PR #287.
The opening audit found the main working checkout at `47174bf3`, with
unrelated edits. During implementation it was fast-forwarded to `4169cb3b`
at the owner's request, preserving those edits. Implement on the isolated
`investigate/source-recovery-next` branch rather than the dirty master tree.
The existing
`/tmp/slmodem-maintainer-fixes` worktree at `d0ae2e96` has the same Git tree
as the upstream merge and supplied the objects examined here.

Fresh read-only comparison of its 300 reproduction objects produced:

| Measurement | Result |
| --- | ---: |
| Shared function names | 1,852 |
| EXACT | 1,086 (58.6%) |
| Original bytes in exact functions | 120,322 / 720,125 (16.7%) |
| SIZE / BYTES / REGALLOC / UNRESOLVED | 667 / 65 / 29 / 5 |
| Named relocation mismatches | 0 |
| Positioned allocated bytes equal in the partial link | 69,291 / 943,398 |
| Exact relocation records | 1,031 / 18,317 |
| Exact defined-symbol records | 397 / 2,907 |
| Compared sections, including GNU-stack policy, reference / candidate | 92 / 99 |
| NOBITS bytes, reference / candidate | 2,836 / 2,812 |
| Strict partial-link verdict | DIFFERENT, exit 1 |

Function identity canonicalises relocation targets; it does not mean those
functions already occupy their original positions. Positioned-byte agreement
is highly sensitive to earlier layout differences and is not a useful scalar
measure of how much source remains to recover.

These are fresh comparisons of existing build artifacts, not a fresh 300-TU
rebuild or a new full-phase run. Source, object inventory and build provenance
were checked; implementation must establish its own clean baseline.
The exact-set ratchet currently enforces 1,074 names, twelve fewer than the
observed exact set. Preserve all 1,086 as the campaign baseline; update the
durable ratchet only after reproducing and validating that baseline.

The accompanying [audit evidence](source-recovery-audit-2026-10-16.json)
records measurements, artifact paths and controlled comparator probes.

## Why the search has stalled

1. **Symbol presence conceals incomplete bodies.** `service.py` and
   `worklist.py` can report zero unwritten symbols while an implemented
   function omits an original arm. `RcFixed_Resample` is a verified example,
   detailed below. The retired V.34/V.PCM defensive guards remain retired;
   this finding does not reopen them or fax reconstruction generally.
2. **The selection policy favours small, nearly matching functions.** The
   historical TU plan deferred large low-anchor bodies. The latest
   `residual_stage_inventory.py` still nominates only BYTES/REGALLOC.
   A fresh census applying the older value-timing screen's eligibility rules
   includes 386 of 766 nonexact names and 132,549 original bytes; 467,254
   nonexact original bytes fall outside those rules. Other tools do inspect
   some of that remainder. These figures demonstrate this screen's limits,
   not whole-project neglect.
3. **Closed experiments can become broader prohibitions in handoffs.**
   A negative declaration-order, scratch-register or source-form domain
   closes only that domain. The recent fixed-profile campaign was bounded;
   it does not supersede the source/profile crossed experiments permitted
   by `docs/method/experiment-design.md`. Conversely, freedom to cross
   explanations is not a reason to repeat exhausted matrices.
4. **Compiler tracing explains retained transformations, not original RTL.**
   The original has no RTL dump. Finding the stage where our instruction
   acquired its register does not establish which original declaration or
   owner would reproduce it. Traces need a source, operand, lifetime or
   caller/callee witness before they justify an edit.
5. **Local source topology and whole-object obligations are separate debts.**
   Generic loops, extracted helpers and changed TU boundaries may preserve
   behaviour while altering expansion, optimisation and allocation. Data
   extents, emitted symbols, COMDAT selection and link order remain even
   after every shared function matches.
6. **The stated completion gate is weaker than literal file identity.**
   `partialcmp.py` reports `raw_exact` but `--require-exact` checks its
   link-relevant comparison. Its intentional treatment of `.comment` alone
   permits different raw files. Controlled probes also found omitted ELF
   metadata dimensions. Fix measurement independently of source recovery.

The existing method already warns against one-axis hill climbing. The needed
change is to enforce those warnings in the queue, evidence inventory and
completion gates, while spending substantial effort on structural SIZE work.

## Ordered work packages

### P0 — Freeze a reproducible baseline and an explicit completion contract

Create an isolated worktree from the pinned upstream revision. Record source
and header hashes, image digest, actual compiler/assembler/linker versions,
complete commands, `.build-config`, all input-object hashes and ordered link
manifest. Use `experiment_toolchain.py`; append `DSPLIB_REPRODUCE_BUGS` after
configurable flags. Reproduce unchanged complete TUs before experiments.

Run `make phase`, `make partial-link`, the function comparator and the strict
partial comparator. Retain separate exit statuses and denominators. The
expected test-binary count in the latest landing is 388; explain any change.
Save all exact names, not just the checked-in floor.

Keep three named completion checks:

* All reference functions present and byte/relocation-exact, with conservative
  duplicate-copy handling and no unresolved targets.
* Complete link-relevant ELF equality, including symbols, data and layout.
* Literal file identity: `cmp` succeeds and SHA-256 agrees with the original.

For the user's 100% byte-identical objective, the last check is required too.
Implement an explicit raw gate while preserving the useful diagnostic census.
Do not weaken the reference or normalise away unexplained differences.
Non-allocated sections, ELF headers, section/string/symbol-table packing and
toolchain-produced metadata become an explicit final backlog.

Repair and test the independently demonstrated `partialcmp.py` omissions:

* `sh_entsize` is parsed but excluded from compared section records. Changing
  a mergeable section's entry size from 4 to 8 currently passes.
* Allocated sections other than PROGBITS/NOBITS are excluded. An added
  allocated `.init_array` currently passes.
* Undefined-symbol records are excluded. An added unused undefined symbol
  currently passes; also test changed binding/type/visibility of an existing
  referenced undefined symbol.

Audit the remaining ELF record fields explicitly rather than assuming these
three probes exhaust the comparator. Exact-copy controls must pass; changed
data must fail; each new negative control must demonstrate the specific
omission before repair. A `.comment` change should remain acceptable to the
documented link-relevant comparison and fail the new raw gate. These are
apparatus corrections, not recovered source or new exact-function gains.

**Deliverable:** reproducible baseline manifest, versioned comparator controls,
raw completion gate, and a machine-readable residual ledger covering every
nonexact function and non-function object dimension.

### P1 — Recover the omitted RcFixed kind-1 conversion

Latest `src/core/FixedRC.c`, around lines 541–550, explicitly declines the
mode 0/1 multistage resampler at original `RcFixed_Resample+0x215`.
The constructor already creates these modes. `test/unit/t_rcresample.c`,
around lines 295–330, constructs both implementations, calls both and
separately asserts that the blob converts and the reconstruction returns
zero output. Running the existing Gentoo period binary during this audit
passed 29 groups, including those eight discrepancy-documenting checks.

This is a valid constructor-created component boundary. Existing modem
callers' use of modes 2..7 does not supply the missing object bytes. It does
limit any claim about end-to-end modem reachability.

Read the whole original kind-1 arm and its constructor/reset/delete state
contract. Recover the multistage delay lines, counts, widths, coefficients,
streaming updates and output-limit semantics. First replace the discrepancy
expectations with real cross-implementation comparisons and demonstrate the
current failure. Use separate output buffers and compare state where valid.
Heap pointers and the three coefficient-table pointers have different raw
addresses on the two sides: compare allocation extents and pointed-to history
contents, and verify coefficient target identities/payloads separately.

Cover modes 0 and 1, reset/reuse, split input, block boundaries, output limits,
impulse/constant/sign-changing inputs and constructor-initialised histories.
Derive buffer capacities and admissible calls from the original before
running them. Do not use unproven planted states to repair the source.
The current constructor/reset clears only four bytes of each 6/174/30-byte
history allocation; the harness supplies its own allocation-fill pattern.
Preserve the original partial initialisation. Distinguish constructor-written
history from harness-provided fill and audit reads of the remainder before
claiming lifecycle reachability or adding further initialisation.

The function is 2,640 original bytes versus 493 reconstructed bytes. This
does **not** attribute all 2,147 bytes to one source transformation. During
implementation, a complete original-body read corrected the historical
F610/F611 interpretation: the repeated literal filter loops belong to kind 1;
the kind-0 region b13df–b1434 uses one generic runtime-tap Q14 convolution.
Do not open the previously proposed separate kind-0 specialisation matrix.
The implementation results record the exact ranges, dataflow and controls.

**Acceptance:** component differential equality, full period/phase gates,
complete FixedRC TU and partial-link audits. Mark the original omission
closed even if the completed function still needs code-generation recovery.
Record that remaining work explicitly.

### P2 — Test fixed-field expansion in the two DIL packers

Candidate pair: `V90DILdesPCK.cpp` and `V92DILdesPCK.cpp`.
The paired original/current sizes are 3,278/1,446 and 4,160/2,267 bytes.
Current helpers are already inlined; this is not evidence for a missing
helper call or a larger inline limit.

In the V.90 source, sixteen seven-bit field operations are represented by
one runtime loop (`V90DILdesPCK.cpp`, around lines 203–210). Original
disassembly has independent seven-bit backedges and constant descriptor
offsets, starting at `0x4d4c0`, `0x4d4f5`, `0x4d533`. Current disassembly
instead computes `j/2`, branches on `j<8`, and uses an indexed descriptor
address. This supplies a structural discriminator before reading scores.

Declare an eight-cell experiment: two sibling TUs, each with a 2×2 cross.

| Axis | Retained form | Alternative to test |
| --- | --- | --- |
| Field expansion | Runtime loop over sixteen fields | Sixteen ordinary calls with witnessed constant field arguments |
| Seven-bit position | Helper-relative `i=0..6` | Absolute destination counter over the same seven positions |

Validate both axes against each sibling's actual original operations before
generating cells. Keep value-shift order, field sequence, widths and flags
fixed. No forced-inline attribute, register constraint or padding.

**Prediction:** expansion produces sixteen literal-offset regions and their
independent backedges; position form explains address/count reduction.
**Falsifier:** the predicted graph does not appear, or other operations/read
ages change without evidence. A smaller SIZE gap alone is insufficient.
Audit all emitted functions, data, binding and source-only collateral.
If expansion is optimised away, inspect the responsible pass and close the
declared source domain before proposing one isolated diagnostic option.

Original integer-to-x87 load width is a separate follow-up: do not combine
an apparent FILDS/FILDL discrepancy into the opening matrix without tracing
the original carrier and its producer. The experiment tests a source family;
even an exact hit would not prove a uniquely original spelling.

### P3 — Make screening account for its exclusions

Do this alongside P1/P2, not as a prerequisite for every source experiment.
Extend the residual ledger to all 766 nonexact names, every defining copy,
original byte weight, function family and complete owner TU. Include missing
and candidate-only bodies separately. Every filter must report its accepted,
rejected, unsupported and unresolved denominators with reasons.

Fresh call-result screening covered 300 objects, 1,886 shared defining bodies,
and 1,747 equal named-call sequences containing 4,692 calls. It found zero
new nominations. The other 139 shared bodies were outside that sequence
comparison; 48 emitted bodies had no matching reference body. Preserve those
facts instead of calling zero nominations a whole-object negative.

A separate current census found 145 retained call sites outside named-call
matching: 95 indirect calls and 50 direct calls without relocation names,
across 75 unique shared symbols. Resolve local direct targets through actual
ELF address ownership, with ambiguity refusal. For indirect calls, trace
the callback pointer, arguments, returned value and surrounding reads; do not
invent a callee identity. Count tail calls separately and include them.

Replace linear call-epoch reasoning with CFG-aware value/use analysis for a
bounded pilot containing a conditional call, a loop and a jump table. Keep
unknown aliases and paths explicit. Validate against existing positive and
negative controls plus one example for each added capability.

Generalise only proven observer mechanisms: the PSD regrename observer,
currently tied to a `process` function header, is a useful starting point.
Require exact symbol/clone selection and raw-equal observed/plain compiler
outputs. Preserve explicit refusals for ambiguous clone headers, unsupported
QI/q scratch modelling and missing dump windows. Do not label current
compiler-stage tracing as recovery of original compiler state.

**Deliverable:** coverage/exclusion report and two independently witnessed
source hypotheses outside the old small-function filter, or a bounded
negative result naming the next missing evidence. More nominations alone
are not a recovery result.

### P4 — Work structural SIZE families and partial-link debt continuously

Allocate at least half of source-investigation capacity to structural SIZE
families; keep a smaller queue for transferable small-function mechanisms.
This is a proposed scheduling policy, not an estimate of remaining effort.
Rank by original bytes, independent evidence and family transfer, not byte
distance alone. Never postpone all large bodies until every small miss closes.

For `v34handshak`, the 61,541-byte original versus 12,867-byte current named
body is not a missing-code estimate. Reconstructed helpers own much of the
work. Build a reference-region to source-helper/inline-instance correspondence
over the complete TU, preserving live-ins, live-outs, calls and shared exits.
Start with one dispatch region whose original external calls and joins are
known. Compare its entire transitive helper closure, not an isolated arm.

Do not repeat the macro/forced-inline, shared-exit, t46/setupreceiver,
no-crossjumping or rejoin-class trials already recorded in F11512 and
F11519–F11523. Reopen only with a changed boundary or independently established
type/alias/read-age/inline-origin fact. Use existing component/lifecycle
fixtures and audit reachability before interpreting failures.

Run a separate object-debt lane from the beginning:

* Enumerate data extents and deliberately added apparatus. `encode.c`
  declares `cEncodedTemp[ENCODE_OUT_MAX + 1]`, 271 bytes versus the original
  270, unconditionally, to address D39. Restore the faithful-build extent
  through the project's established bug-compatibility policy, with original
  boundary behaviour measured before retention; keep any operational fix
  explicit. Do not shrink other data to compensate for this byte.
* Audit the nine reference-only and 55 candidate-only defined names from the
  unique-name census. This is broader than shared functions and collapses
  duplicate local names; use the ordered symbol comparator for authority.
  Reference-only names include `pow` and its constants; candidate-only names
  include reconstruction helpers and table/debug inspection APIs. Determine
  required source/link ownership; do not remove behaviour or blindly rename
  symbols for a score.
* Revisit the 21 unanchored entries among 300 link inputs using FILE/LOCAL
  provenance and data/relocation ownership. Preserve ambiguity rather than
  manufacturing a link order. Trace selected COMDAT instances in the actual
  partial link; do not replace worst-copy function grading with best-copy
  grading. Both measurements serve distinct purposes.
* Resolve the five anonymous-target proofs listed in the evidence file using
  bounded ELF/CFG proofs and hostile controls. No byte-prefix acceptance or
  unresolved-as-exact credit. A proof improvement is a measurement change,
  reported separately from a source gain.

Once independent large-family observations implicate a shared pass/profile,
reopen #22 with a discriminating source/profile cross. Verify its latest issue
and artifact ledger first: the old audit proposed whole-object validation of
O2 plus automatic inlining, web and unswitching, but that prose alone does not
prove the experiment remains unrun. Compare full nonexact bodies, bindings
and the partial link, including informative losing controls. Temporary losses
are permitted during isolated diagnosis; adoption still needs explained,
validated complete-object evidence. Never weaken the ratchet to bless a
convenient net score.

## Iteration and acceptance

At each batch opening, record the immutable baseline, reference witness,
hypothesis, competing explanation, predicted discriminator, finite cell count,
controls and stopping condition. At closure record distinct generated sources,
distinct emissions, exact gains **and losses**, original bytes recovered,
missing/new definitions, data/call/binding effects and test denominators.

Classify the result as invalid experiment, negative result, mechanism control,
supported partial recovery, or validated exact recovery. Two batches without
a new explanatory observation trigger a scope review. Preserve the tested
domain and the evidence needed to reopen it in a searchable ledger.

For retained source, run the actual candidate under Gentoo `make period` and
finish with `make phase`; preserve all original bugs in reproduction mode.
Use `diff_eq_obj` for valid object comparisons. Do not widen tolerance or
repair source for modern compiler behaviour. Portability remains separate.
Record strict partial-link and raw-file status even while they fail.

Schedule the next batch from the updated residual ledger. After P1/P2,
continue with the strongest structural witness uncovered by P3/P4; do not
automatically return to the nearest BYTES mismatch. Completion requires all
three P0 checks, not exhaustion of the current candidate list.

## Verified closed work and housekeeping

> 🧠 **From Hindsight memory (Residual compiler-stage family batch)** — The
> preceding campaign closed its supported queue without establishing a global
> ceiling. Memory also identified later follow-ups to check against the tree.

Git ancestry, patch-equivalence and file-content comparisons now verify that
PRs #284, #285 and #287 have landed. The six bounded maintainer debt findings
are resolved on upstream, including Phase 4 type consolidation, typed
supervisor recovery and chunked V.PCM lifecycle coverage. Some standalone
branch hashes differ because the commits were integrated separately.

`getPower`'s 574-byte recovery is complete. `getFrame`/`putFrame` already have
their documented partial recovery; Mapping/PSD spill-home work and PSD
regrename tracing also ran. Consult their current result documents before
selecting residual work. These are not opening tasks for this programme.

Memory corrections were recorded for these statuses, the stale root checkout,
and the distinction between symbol coverage and RcFixed's omitted arm.
Nine worktree registrations pointing to absent directories were pruned.
Existing worktrees were retained: several have uncommitted changes, and even
clean ones contain compiler traces/build artifacts needed for replay. Merged
Git history alone does not make those untracked artifacts redundant.

## Copyable Sol brief

Start from `4169cb3b` in an isolated worktree and implement P0, then P1 and
the eight-cell P2 investigation. Develop P3 alongside them; maintain P4's
object-debt inventory from the first baseline. Read `AGENTS.md`, this plan
and `docs/method/experiment-design.md`. Never access the excluded directory.
Preserve existing worktrees and user edits. Delegate one bounded complete-TU
family at a time and independently review its commands, objects and losses.
Do not reopen closed domains without a new discriminator. Produce reviewable
commits only after the required gates, with explicit partial-link/raw status.
After each batch, choose the next evidence-backed experiment from the whole
residual ledger until the completion checks pass or a specific external
dependency blocks further work. Do not promise a fixed number of matches.
