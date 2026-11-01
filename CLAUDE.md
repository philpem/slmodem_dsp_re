# Working in this tree

This project reconstructs `slmodemd/dsplibs.o`. `AGENTS.md` is a symlink to this
file. Read [README.md](README.md) and the [method index](docs/method/README.md)
for orientation, then only the references needed by the current inquiry.

## Boundaries that always apply

- **Never read, search, import or reference `re/`.** It belongs to another effort.
  Exclude it explicitly from delegated tasks and Ghidra analysis too.
- Preserve unfamiliar changes and other workers' worktrees. Check status,
  revision and ownership before editing. Work in an owned worktree; never stash
  or reset someone else's work. Use `git -C` or a checked working directory.
- Missing-source reconstruction is complete, including fax. Current work is
  source/type/ownership fidelity, generated-code convergence and validation.
  Measure current denominators; historical service schedules and counts are
  not the queue. Do not infer module or original TU ownership from span labels.
- SpanDSP is a test peer only. Do not copy its implementation into `src/`.
- Recover original behaviour, including original bugs. Repairs require separate
  authorisation and the established `DSPLIB_REPRODUCE_BUGS` reproduction arm.
  Do not silently fix the reproduction build or introduce another bug gate.
- Keep original function structure during reconstruction. A state-machine
  refactor into new helpers is separate work, not a way to reconstruct a body.

## Source-recovery workflow

A short request to continue source recovery authorises investigation and bounded
implementation. Do not require an issue number, another worker or a new detailed
prompt to start. Honour the owner's current priorities and delivery permission.

1. **Establish the baseline.** Record revision, relevant source/header hashes,
   native input, compiler/profile and current named grades. Refresh stale objects.
   Search Hindsight narrowly for the current inquiry, then verify relevant claims
   against the source and artifacts. Memory is historical evidence, not a live
   measurement. Use compact passages rather than whole large pages.
2. **Choose a closure unit.** Normally one complete function or a small family
   sharing a demonstrated discrepancy. State the native constraints, hypothesis,
   competing explanation, prediction, falsifier, candidate domain and limit.
   Keep useful partials together on the closure branch. For convergence work,
   report whole-function promotions separately from partial source recoveries.
3. **Resolve the compiler question.** When lowering is unexplained, inspect the
   matching GCC source/dumps and actual inputs, dependencies and decision rules.
   Reproduce the unchanged full-TU object through any experimental compiler path
   before trusting it. A current compiler mechanism does not reveal the original
   compiler input or original RTL. Use controlled source/option interactions;
   do not freeze today's source/profile merely because some functions match.
4. **Test supported alternatives.** Natural C consistent with the complete native
   evidence does not need a uniquely identifiable original spelling. Keep
   informative losing controls and test justified combinations. Do not substitute
   blind cast/reorder/structure palettes, artificial scratch consumers, volatile
   barriers, padding, assembly or unsupported attributes for an explanation.
   A diagnostic flag hit alone is not a retained profile. A supported source/profile
   combination needs complete-TU native evidence, named collateral review and
   period/object parity; record its scope and remaining ambiguity.
5. **Review the whole result.** Account for all affected-TU bodies, calls, data,
   relocations, bindings and named gains/losses. Get independent evidence review
   for a positive candidate. A net score gain can hide a loss. Explained structural
   tradeoffs require explicit review and a recovery plan; never silently lower a
   ratchet. Source changes must preserve the period differential oracle.
6. **Validate and deliver.** Run focused period checks during development and
   `make phase` before integrating reconstruction or fixture changes. Report
   denominators and the actual command's exit status. Run codegen/partial-link
   checks appropriate to the claim. Submit and merge ready PRs when authorised.
   At the boundary, report closures, partials, negatives, costs and next steps.

Read [experiment design](docs/method/experiment-design.md) before source/profile
experiments and the relevant [Playbook](docs/method/refinement.md) section. A
negative result excludes its tested domain under its controls, not all equivalent
source. Reopen it only with a named new discriminator. Two batches without a new
explanatory observation require a hypothesis/scope review, not more synonyms.
A closed spelling family does not explain an emitter: trace the first pass that
changes the unexplained operation before declaring that no discriminator remains.

## Validation and measurement

- Gentoo GCC 3.4.2-r2 is the reconstruction authority. Use the configured image
  and complete flags in `tools/toolchain/period.mk` and the build's `.build-config`.
  Every reconstruction experiment must enable `DSPLIB_REPRODUCE_BUGS`; use
  `tools/experiment_toolchain.py`. Record actual compiler and selected assembler
  versions. Stock GCC 3.4.2 is an explicit A/B control, not the default authority.
- `make phase` runs period differential and structural checks. `make test` is
  not its replacement. Modern GCC, 64-bit and interop tiers are explicit
  portability checks (`make portability` / `make phase-full`). Never alter
  reconstruction source to satisfy a modern compiler or modern rounding result.
  Put compiler compatibility in apparatus flags/shims. See
  [compiler policy](docs/method/compilers.md).
- No reconstruction source is committed without a passing period differential.
  Do not widen tolerances, add a period allow-list or edit source for an impossible
  fixture. Establish valid inputs at a stated boundary. Label planted internal
  states as synthetic exploratory probes unless their history is established.
- New apparatus needs appropriate controls and nonzero denominators. Demonstrate
  a detector firing on a known input and rejecting a known negative before relying
  on it. Harness/tool changes are apparatus, not source that must match a blob.
- Use **EXACT**, **REGALLOC** and **NONMATCH** for fidelity; keep missing/unresolved
  evidence separate. Equal size is not a grade. Longest runs and mnemonic/opcode
  counts are diagnostic: they do not prove register relationships, branch targets
  or equivalence. Use `tools/toolchain/byteident.py` and the
  [fidelity definitions](docs/method/fidelity-reporting.md). For alternate object
  trees, set `TC_OUT` explicitly and supply fresh selected-partial-link proof
  context. An unchanged object's hash does not validate independent proof inputs.
  If fresh context still refuses an unsupported native pattern, retain its
  unresolved grade and record the refusal; do not alter source to fit a recogniser.
- Partial-object comparison has its own stronger semantic/raw identity claims.
  Report DIFFERENT honestly. Defer alignment when the owner prioritises functions.
- Mutation campaigns/snapshot refreshes are optional named investigations, not
  implicit commit/merge gates. Historical verdicts are not current evidence;
  require a valid green baseline, controls and denominators for a new campaign.
- Do not edit gate inputs while a gate runs. Preserve the wrapped command's exit
  status; a trailing `echo` or `tail` can report success after a failing gate.

## Types, names and byte addresses

- Define each class/struct/enum/union in one home. Include or forward-declare it
  elsewhere. Run the existing one-definition/layout checks; do not copy live
  exception counts from historical prose.
- Every raw-offset-to-member change must preserve owner, complete nested byte
  offset and access width at each use. Verify the period layout and distinguish
  plausible alternative fields in differential histories. Struct assertions alone
  do not prove that a use selected the right member.
- Prefer binary format strings and typed caller/callee evidence for names.
  Decompiled types/names are hypotheses. Do not promote an inference to a fact.
  Uncertain roles may keep neutral names with their derivation recorded.
- Name known flags by their actual bit values; preserve the object's masks.
  Do not rewrite them as bitfields without site-specific instruction evidence.
  A dead upper half is not proof of field signedness: follow the value to its use.
- Use `diff_eq_obj` for whole-object comparisons and `tools/whichfield.py` for
  offsets. Use loops only where regions genuinely must be excluded.

## Ghidra and object evidence

Ghidra/MCP can inspect CFGs, calls, cross-references, strings, data and P-code;
it can propose types and structure or run bounded exploratory emulation. Identify
program, input hash, version, mapping, boundaries and calling convention first.
Pass the program explicitly when several are open; preserve analyst annotations.
Verify decisive bytes/instructions/relocations independently against the ELF and
`tools/dis.py`. Keep imported evidence separate from reconstructed annotations.
Decompiler C, similarity and high P-code are not equivalence proofs and do not
replace period execution or GCC dumps. See
[object-reading techniques](docs/method/reading-object-code.md) and
[equivalence](docs/method/equivalence.md).

Use relocation-aware tools for tables and strings. A named-relocation-only call
count misses resolved local calls. Verify x87 popping subtraction/division from
bytes or Intel syntax. Before importing pyelftools from a script under `tools/`,
account for `tools/dis.py` shadowing Python's standard `dis` module.

## Model-token economy and bounded work

Local tools are free in the current environment; model context, output and
reasoning are the scarce budget. Run local analyses freely when useful, save
full results to scratch and return compact discriminating evidence. Do not
replace a necessary disassembly read with many speculative model turns.

- Separate elapsed-time, candidate/CPU and model-usage budgets. Aggregate token
  counters, cached inputs and billed cost are different measurements. Record
  available usage honestly; do not invent a monetary saving.
- Prefer one compact worker context per owned inquiry when delegation is
  authorised. Send baseline, question, constraints, artifact paths and controls;
  do not fork a long conversation by default. Delegation isolates context but
  the worker's intermediate tokens still cost. More workers need evidence of
  useful independent work, not merely available slots.
- Avoid full knowledge-page dumps, repeated broad searches, full logs and
  narration of routine results. Batch independent local reads and return only
  the decisive excerpts. Verify cached evidence by hashes before reusing it.
- Agree a wall-clock/candidate limit and a success/stop condition. Local CPU
  work may be plentiful; more model turns still need a discriminating question.
  Do not silently extend a failed trial or integrate partials solely to fill it.
- Astra is not the routine GCC tracing worker. A strategic review needs a compact
  packet, budget and stopping rule unless already authorised.

See [efficiency](docs/method/efficiency.md) for the historical measurements and
current interpretation. The [historical instruction snapshot](docs/method/working-instructions-history.md)
preserves detailed examples; read relevant sections only, not the whole archive.

## Records, issues and shared work

Use `scratch/<inquiry>/` for one-off scripts, variants, logs and evidence; keep
normal build outputs under `build/`. Do not promote temporary scripts into
`tools/` without ongoing use, a documented interface and maintained controls.
Use Hindsight for concise durable results, closed domains and next discriminators.
Do not create a permanent progress diary for every batch. Maintained docs hold
stable methods and reusable findings. See [tools guidance](tools/README.md).

GitHub issues are for demonstrated original-DSP bugs needing a separately gated
repair, not routine recovery plans, hypotheses or progress. Check duplicates and
include native evidence, valid boundary, consequence and repair criteria. Existing
issue links are historical provenance, not a work queue. PRs still carry review
and validation evidence.

Append measured findings/deviations without renumbering history. Check numbering
and references with `python3 tools/refcheck.py`; a resolving reference can still
point at the wrong fact. Shared tools/instructions belong on master; keep active
branches current after they land. When merging, preserve both contributions;
`checkout --ours` replaces an entire file, not only the conflict.
