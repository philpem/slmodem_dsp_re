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
   Consider ordinary C idioms early: a local alias of an argument or nested
   object, or a captured field value, may explain native load groups and value
   lifetimes. These consumed locals are not dummy register uses. Distinguish an
   opaque API cast from those aliases; infer interchangeable callbacks or C
   polymorphism only from actual use across functions, not from a cast alone.
   Screening tags and short bodies are navigation, not admissions: identify an
   actual native/source observation boundary before a transfer experiment.
   A batch limit is a ceiling; do not fill it with exhausted or weaker leads.
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
   Qualifiers are source hypotheses too: distinguish evidence-backed volatile
   accesses from artificial barriers, and check their scope against native
   readers and writers before changing a shared declaration.
   A diagnostic flag hit alone is not a retained profile. A supported source/profile
   combination needs complete-TU native evidence, named collateral review and
   period/object parity; record its scope and remaining ambiguity.
   Record the named hypothesis feature separately from the whole-function grade.
   NONMATCH does not show that a requested reload or lifetime failed to appear;
   inspect the controlled feature before accepting or rejecting its explanation.
5. **Review the whole result.** Account for all affected-TU bodies, calls, data,
   relocations, bindings and named gains/losses. Get independent evidence review
   for a positive candidate. A net score gain can hide a loss. Explained structural
   tradeoffs require explicit review and a recovery plan; never silently lower a
   ratchet. Source changes must preserve the period differential oracle.
6. **Validate and deliver.** Validate retained source with relevant focused period
   checks, then run `make phase` at the reviewed integration checkpoint. Report
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

### Experimental compiles and integration checks

**Experimental compiles do not require full gates, global censuses or CI.**
Establish an unchanged full-TU baseline through the experimental build path,
then use local native comparisons and affected-TU checks to investigate variants.
Rebuild linked proof inputs only when the local comparison needs them. A failed
or discarded experiment does not trigger `make phase`, a global `byteident.py`
census, whole-tree object parity, a full partial link or a remote CI run.

Before committing reconstruction source, require the relevant period differential
to pass. Review the complete affected TUs and decide which supported partials to
retain before freezing the integration candidate. Batch `make phase`, the global
fidelity census, actual/faithful object parity, partial-link checks and PR/CI
validation at that checkpoint. If retained inputs change afterwards, validate
the new candidate. Keep diagnostic results distinct from integration evidence.

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
- Use `tools/toolchain/diffvector.py` for independent function diagnostics.
  Keep its mapping, unknowns and explicit copy scope visible; counts and a
  successful report exit do not award a grade. See the
  [vector guide](docs/method/metric-vector.md).
- Partial-object comparison has its own stronger semantic/raw identity claims.
  Report DIFFERENT honestly. Defer alignment when the owner prioritises functions.
- Mutation suites, anchor checks and snapshot maintenance are retired. Do not
  add, repair, execute or refresh them unless the owner explicitly requests that
  named investigation. No build, commit, merge or release requires them. See
  [the retirement policy](docs/method/mutation-policy.md).
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

## Recovery experiments, memory and model selection

**Register allocation and code generation are coupled across the translation
unit. The complete TU, in original function order, is the recovery boundary.**
Earlier source changes can improve or regress later functions even when their
source is unchanged. In particular, period GCC carries peephole2 scratch-search
state between emitted functions; register choices can then change scheduling
and instruction layout. Individual function-local passes do not make functions
independent. This is established project knowledge, not a hypothesis to rediscover.
Distinguish textual definition order, actual GCC emission order and the linked
layout. Trace callgraph/template dependencies before interpreting order debt;
identical emitted bodies need not carry identical scratch-search histories.
Rebuild and compare every emitted function after each candidate. Getting one
function to EXACT may recover later REGALLOC/BYTES functions or disturb existing
EXACTs. Preserve those intermediate states, trace the affected neighbours, and
test supported changes together before retaining or rejecting them. Never freeze
an EXACT neighbour or veto a promising recovery solely because it regresses.
See the [Trn2d/K56 coupled example](docs/trn-k56-recovery-20261107.md).

Treat SIZE, BYTES and REGALLOC as measured code-generation categories, not
diagnoses of reconstruction correctness. REGALLOC does not prove semantic
equivalence: register lanes, implicit operands, control-flow live ranges and
ABI outputs require their own evidence. Retain the period differential gate.
Report native and candidate extents explicitly; an unsigned SIZE delta does
not establish which body is larger.
A signed/unsigned branch establishes the consumed comparison semantics, not a
unique field or formal declaration; check promotions and interpretation at use.

Use the [native/reconstruction graph screen](docs/method/native-graph-screen.md)
on changed translation units during recovery waves, and periodically screen the
remaining inexact corpus. Audit nominated owner/width/predicate/timing differences
against native instructions and fixture domains before making source changes.
Check selector/iteration identity and propagate known guard values across each
nominated continuation: a raw CFG path through a nullable-result join can be
value-infeasible. A static reachability difference alone is not an operation-age
or behavior difference. Audit every intervening caller publication as well as
callee effects: a callee that writes no caller memory does not make a read move
equivalent across a same-type alias store in the caller.
Report compared relations, ambiguous matches and unsupported effects; a bounded
clean subset does not establish equivalence or justify parking a function.

Report EXACT/REGALLOC promotions separately from partial SIZE/BYTES progress.
Before attributing a residual to allocation, compare complete reachable control
flow, value/flag lanes, owner-relative effects and call-crossing live state;
matching CFG topology alone is insufficient. State any bounded subset, manual
instruction correspondence, alias/address assumptions and opaque-callee contract.
A model-relative caller equivalence result neither changes the byte classifier
nor establishes the original source declaration. The
[InitMOH closed audit](tools/native_graph/initmoh_flow_case/README.md) is a worked
case, not a general corpus equivalence detector.

Keep address identity, alias metadata and declaration evidence separate.
A byte-pointer intermediate does not make a subsequent short/int dereference
alias-safe; check the final typed access against the declared object and measured
alias metadata. A full-word writer and low-word reader establish their consumed
widths, not a unique original field declaration. See the
[minimum-power audit](docs/min-power-recovery-20261108.md).
A cached cast pointer and repeated member accesses through a formal can retain
different period-GCC expression identities. When a compatibility view broadens
aliasing, test references to actual declared members with checked extents before
inventing another owner type. Cross these properties; their isolated outputs
can miss an exact combination. Equivalent formal/local copies can also select
different spill transport without changing the formal's type or ABI.
An explicit short local can promote to an SI pseudo while repeated declared-
member reads common into an internal HI pseudo. Check consumption and spill
metadata; a narrow native spill alone does not establish an original local.

Machine publication order does not uniquely identify source statement order.
Direct short-member preincrement can sink its store after a comparison, while
a capability pointer initialized before its value can emit the value store
first. Check the complete generated graph and crossed controls before rejecting
such source forms. The [getMP/rate recovery](docs/rate-layout-recovery-20261107.md)
records both, including temporary EXACT losses repaired by coupled owners.

When a supported owner-header change regresses a consumer, trace its actual
member metadata and inline copies before rejecting the model. The V34 TX
recovery repaired an intermediate handshak regression by migrating FreezeEcho
to its independently witnessed owner, gaining an EXACT public helper. Distinguish
capturing a mutable owner pointer from scheduling a constant member-address
addition on an already captured root; early arithmetic alone is not an earlier
observation of mutable state. See the
[coupled recovery](docs/create-tx-recovery-20261107.md).

Follow candidate stores through late if-conversion and renaming: peephole2
scratch choices can let `ce3` merge two stores that were separate earlier.
Measure the complete TU's scratch searches before attributing an effect to
the persistent cursor; `rnreg` can instead make a function-local choice that
changes `sched2` dependencies. The
[SetV90/TX follow-up](docs/tx-scheduler-recovery-20261107.md) observes both.
Record transitive analysis-helper hashes separately from the front-end tool:
an unchanged screen or comparator can still import a dirty helper.

Validate relocation type, field site, width and addend before canonicalizing
its target. A matching symbol name can still name a different function entry
or global word. Retain expression objects when serializing solver DAGs; a
recycled runtime identity must not silently relabel a proof node. Exercise
both failures with known counterexamples before trusting a clean report.
Keep resolved local-call distance differences in strict grades too. Measure
intervening function extents and padding when the target identity agrees;
recover that source/layout debt rather than fitting or masking the caller.
See the [quadbit/Phase34 follow-up](docs/quad-phase34-recovery-20261107.md).

Before proposing a recovery experiment, query Hindsight for the symbol, TU and
mechanism, then check the cited record against current inputs. Record the
hypothesis, native witness, source/header/compiler hashes, tested combinations,
fixture domain, whole-TU effects and the condition that would reopen a negative
result. Memory explains prior decisions; reproducible artifacts establish what
was measured. Stale evidence remains history, not current validation.

Several source differences can interact. For two independently supported
changes, test baseline, A, B and A+B before rejecting either because its isolated
grade did not improve. For larger combinations, use bounded experiments and
ablations. Preserve and trace intermediate regressions, including affected
EXACT neighbours, and test the coupled repair before deciding what to retain.
A failed isolated spelling is not a proof against the source property; a closer
byte count is not sufficient evidence to adopt it. Check
original function-order evidence and effects on later functions in the complete
TU; do not add artificial carriers to fit register choices. Before rebuilding a
repeated native algorithm by hand, compare it with existing helper bodies.
Recover supported call/inline boundaries and share the implementation where
appropriate; validate the public out-of-line entry and all affected consumers.
Treat shared template instantiation as a coupled provider/consumer boundary.
Account for removed duplicate weak copies separately, verify the remaining
provider and global closure, and retain original-ownership ambiguity.

Use GPT-6.1 Sol at High as the provisional default for reconstruction and
investigation agents. **If Sol High and Extra High investigations produce no
EXACT/REGALLOC recovery, escalate to Astra before parking the function.** This
is the owner's escalation rule (2026-11-08); partial SIZE/BYTES progress alone
does not close it. Give Astra the frozen inputs, native witnesses, tested
families and qualified negatives, then independently validate any new source
or coupled repair. Record what it added, including no further recovery.
Use Extra High (`xhigh`) for difficult causal inference,
interacting source/type changes or a high-impact negative conclusion when more
reasoning is justified. Delegate mechanical work to cheaper models when its
validation is well defined. Consider Astra for independent/adversarial review,
conflicting evidence, unresolved allocator causality or equivalence-detector
changes; a Sol Extra High attempt is another escalation option, not a mandatory
extra run before every independent review. The pilot compared both models at
High; Sol Extra High versus Astra efficiency remains unmeasured. Record actual
effort, usage when exposed, and cost per independently validated result rather
than inferring efficiency from model price or effort labels alone.
Earlier Astra review remains available for those reasons; the required
post-High/Extra-High escalation does not claim that Astra is always better.
Record what an escalation added, including attempts that added nothing, and
periodically sample completed or negative Sol investigations for missed fixes.
Keep final decisions tied to independently checked evidence, not model identity.

The [recovery pilot](docs/method/recovery-pilot.md) defines the paired evaluation
and records its status. Keep comparisons independent: identical frozen evidence,
fresh contexts, separate outputs, declared limits and no cross-agent result or
live-memory leakage. Historical controls and new recoveries have separate
denominators. Report actual usage where available and leave unavailable token
or billing measurements unknown. Model-routing instructions guide spawning;
they do not change the model of an existing agent.

The 2026-11-05 [pilot](docs/method/recovery-pilot.md) found complementary
results: Astra supplied additional classifier repairs, Sol found the adopted
CPt source recovery, and both independently found the cache race. A negative
rescue narrowed V90CP to peephole2 without recovering it. Use those measured
cases to guide escalation, not a blanket claim that one model sees every fix.
Repeat disagreements on new blind tasks before making category-wide rules.
For analysis provenance, hash the bytes actually parsed: before/after pathname
hashes alone miss write-and-restore races. Recheck frozen artifacts when memory
points at a prior result; retain its date and reopening condition.

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
