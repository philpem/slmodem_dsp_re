# Reconstruction experiments without getting stuck

Agent workflow distilled from [GitHub #22](https://github.com/philpem/slmodem_dsp_re/issues/22).
Use with [refinement.md](refinement.md), which describes the individual levers.
Measured examples and commands live in [cid-dcr-audit.md](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/cid-dcr-audit.md);
current investigation state and next discriminators live in Hindsight. Historical
issue links are provenance, not a progress queue. GitHub issues are reserved for
demonstrated original-DSP bugs needing gated post-reconstruction repairs; see
`AGENTS.md` for the current policy and autonomous startup workflow.

## The target is an object, not the minimum of a score

The reference object is canonical. Current source and flags are hypotheses,
including source that currently produces exact functions. Two reconstruction
choices can compensate for one another. Do not freeze them merely because
changing one loses matches, or conclude that those losses prove wrong source.

Exploration need not improve monotonically. Keep informative losing controls
and test justified combinations. Do not merge diagnostic controls merely to
continue an experiment. A source recovery may retain an explained byte
regression when original-source fidelity improves; apply the review below.
A higher exact-function count or fewer differing bytes does not establish
recovery: behavior, instructions/operands, call boundaries, data, exports and
partial-link layout must all agree. Do not claim a global optimum from a finite
experiment. Preserve complete function bodies and original bugs; never delete
behavior, add padding or weaken measurement to obtain a hit. Prefer clear,
idiomatic source when equally supported spellings produce the same output.

## 1. Validate the experiment before interpreting it

Read relevant Hindsight conclusions and the audit section; verify them against
the current code/artifacts. Identify what is
still open; do not restart an already exhausted experiment without new evidence.

Use recovered Gentoo GCC 3.4.2-r2 and its selected period assembler/linker.
Every reconstruction compilation must enable `DSPLIB_REPRODUCE_BUGS`; use
`tools/experiment_toolchain.py` to append it after configurable flags. Record
actual commands, versions, source/header revision or hashes, object hashes,
and the variant domain. Compare commands with the baseline `.build-config`,
including register renaming and C++ flags. A version label alone is inadequate.

First compile the unchanged full TU through the experimental build path.
Require reproduction of the baseline object, or explain every relevant body,
relocation, data and binding difference before trusting the matrix. Source-path
or STT_FILE changes are not a blanket excuse. Isolate profile outputs; do not
reuse a source-timestamp-only cache after changing flags. Demonstrate a new
generator/detector changing a known input, and report its denominator.

An alternate comparison tree also needs the current selected partial-link
proof context used by the grader. Set `TC_OUT` explicitly, rebuild that context
and retain the relevant linked inputs. A scratch tree containing byte-identical
objects can still yield incomplete evidence if those independent proof inputs
are absent. Preserve such a run as invalid rather than waiving an unresolved
verdict or changing source to satisfy it.

Control drift, unexplained missing symbols, or empty results stop interpretation.
Preserve invalid runs as invalid and rerun; do not repair their conclusions
after the fact. #22 required reruns for omitted register renaming and omitted
bug reproduction.

A period-compiler rejection under alternative flags is evidence about the
tested source/profile pair. Investigate required inline bodies, instantiation
and visibility before ruling out the profile globally or changing source just
to make it compile. An incomplete build has no valid whole-object accuracy
denominator; report compilation coverage and the failures separately.

## 2. Establish the compiler cause, then cross supported explanations

When the emitter is unexplained, trace the first relevant difference through
matching period GCC source and dumps before proposing code alternatives. Inspect
actual inputs, pass admission/legality rules, dataflow and decisions. Do not
replace an unknown cause with source/reorder/cast palettes. A measured current
mechanism does not recover the original source graph, and a static admission
barrier is not proof that removing it would produce the native result. Reproduce
the unchanged baseline through any diagnostic compiler path before trusting it.

Before compiling, write a prediction and a falsifier. Example: "If automatic
inlining causes this mismatch, disabling it should restore the helper call;
changing only the standalone helper would leave the explanation incomplete."

Start with this small crossed design:

| | Retained flags | Alternative flags |
| --- | --- | --- |
| Retained source | Baseline | Option effect |
| Evidence-backed source alternative | Source effect | Interaction |

Keep optimization level, individual passes, tuning and source form separate.
Expand the product where controls show interactions. Inspect the period
compiler's actual enabled passes when reconstructing a level from named
options; accepting an option spelling does not prove it changes code.

V32 recovered four O2 losses with source static inline. Plain O2 recovered
14 losses from the bundled O2/no-rerun-CSE profile. PCM needed web, automatic
inlining **and unswitching** to reproduce its O3 object. A one-axis search
could miss all three explanations.

A pass control that restores a complete native body is an admission to the
crossed inquiry. If it loses an existing exact neighbour, map that neighbour's
native captures, callback epochs and dependencies before rejecting the profile.
The old exact source may rely on a compensating optimisation. Conversely, an
exact gain does not waive a neighbour's loss. Resolve or explicitly review every
named obligation before retention; a diagnostic hit alone does not justify a
new build exception. Confirm the original TU boundary, profile scope and actual
period/comparison object parity. A command line need not be uniquely original
to be a supported, accurately qualified reconstruction hypothesis.

Keep alternative explanatory candidates, including useful losing ones. Prefer
common-profile explanations, but do not manufacture per-file exceptions to
protect today's exact set. A loss under a flag is evidence to investigate its
source/flag interaction, not grounds to declare either side wrong immediately.

## 3. Widen causal scope when local changes stop explaining the mismatch

Compile and score the full TU. Helpers, callers, preceding emitted functions,
declarations and data can alter an unchanged function. Validate any standalone
model against the full-TU controls before relying on it.

| Observation | Next discriminating check |
| --- | --- |
| Missing/extra helper calls | Inline status, definition placement, cost estimates and passes; inspect exported binding too |
| A wrapper CALLS a helper the reference calls, but ours INLINES it (or the reverse), with no flag change | Check the FILE: the two may be separate TUs in the object that this tree merged into one. `-O3` cannot inline across a TU. Move the helper to the FILE symbol's own file and re-measure before trying a flag; the global `-fno-unit-at-a-time` profile buys the same result at tree-wide cost (F11351) |
| Register-only/dead-pop difference | Preceding TU emission and inlining; inspect live ranges before rewriting stores/types |
| Load width/signedness or arithmetic differs | Reference operands, field/local types, constants and uses |
| Stack-size difference | Actual spills, outgoing-call area and alignment; frame size alone does not establish a guarding patch |
| Source variants emit identical code | Verify the generator, then inspect helper visibility or another mechanism instead of retrying synonyms |
| Different TUs give conflicting results | Cross source and option controls across the affected families before adding exceptions |

V92Jd reset registers changed when **getters** moved; reset stores did not
need editing. Copying full reference emission order was not the successful
source order. Rxcid's constructor became exact under O3 after a callee source
change altered inline cost; the constructor did not thereby validate reset.

## 4. Finish a declared domain, then stop or reframe it

Define the candidate family and cell count before reading scores. Enumerate
small finite families completely. For large families, declare a bounded
screening stage and the observation that would justify expanding it; do not
call a sample exhaustive. Track distinct source variants, distinct emissions
and exact hits so a broken generator cannot masquerade as a negative result.

Zero hits excludes **that domain under those controls**, not equivalent C or
an optimization level globally. One hit is unique within the declared family,
not proof that no other original program could emit it. Multiple hits establish
only shared properties. Near-misses remain evidence, not recovered source.

At every batch boundary choose: close the tested hypothesis, name the next
discriminating control, or hand off a documented unresolved question. Two
consecutive batches with no new explanatory observation trigger a mandatory
scope/hypothesis review, not more nearby spellings. This is a work-management
trigger, not a mathematical claim that two batches exhaust the problem.

Before reopening a stalled line, identify the changed assumption: a newly
found helper, source/flag interaction, untested threshold interval, or invalid
old controls. If nothing changed, stop that line and work another hypothesis.
Record what evidence would reopen it so future agents do not repeat the loop.

Phase4's useful interval was 81–82, between coarse controls at 80 and 100;
that justified enumerating 81–99. Rxcid's 384 store-order cells all failed to
reproduce reset; taking the closest cell would not establish its source order.

## 5. Inspect all changes, not just gains

For every successful cell retain the shared-symbol inventory/denominator,
exact-name set, sizes, bodies/relocations and binding. Report gains **and**
losses, including missing/new symbols. Equal SIZE/BYTES scores can hide changed
defects: compare those bodies too. Inspect changed call edges and data targets.

Use `byteident.py`, not a new scoring loop. SIZE(N) is a length gap, not N
differing bytes; `--why` is a rejection, not the complete difference.
UNRESOLVED remains unresolved even when raw bytes agree. Instruction identity
does not prove exported-symbol binding. Treat export loss/weakening as a failed
replacement constraint, even when caller matches improve.

Do not call a larger SIZE gap a regression without reading its cause.
`RD_create` moved from SIZE(3) to SIZE(12) under no-unit while restoring the
reference's constructor call and its offset. The twelve-byte deficit belonged
to three switch-arm stores sharing one tail, a separate source/pass question.
The smaller gap had hidden the wrong inline boundary. Conversely, a restored
call does not settle the switch or recover the complete function.

Broaden promising flags to representative TUs and then the full object before
proposing global adoption. Phase4's gains hid pump inlining losses under O2.
Limits 81–82 fixed that local combination but had global losses and new calls
inside already-nonexact functions. V92Modulator inline recovered constructors
but removed the strong reset export. A scalar score concealed each problem.

When several local controls point to the same pass combination, test that
combination on the complete object before continuing file-by-file. #22's
O2 + automatic inlining + web control recovered 82 of the original 91 losses
together, but still lost nine baseline exact symbols and changed nonexact
bodies and partial-link layout. A local recovery ledger and a compatible
global candidate are different results; report both, with disjoint symbol
sets. A combined-pass recovery is not an individually isolated mechanism or
proof of a source mistake.

Check the prose against the machine records. Retain full mangled names for
overloads and constructor variants; a shortened display name is not a symbol
identity. Label whether a difference is against the reference, retained O3,
or the previous experimental cell. Compare actual body/relocation values,
not their differently formatted serializations. In a source/profile cross,
source-only collateral must be measured at the same profile; do not attribute
all O2-versus-O3 changes to one inline keyword.

## 6. Separate diagnosis, acceptance and completion

Label outcomes: invalid experiment, negative result, mechanism control, source
candidate, or validated retained change. Attributes, duplication or barriers
used only to isolate a mechanism are diagnostics, not recovered source. A
successful diagnostic needs independent source evidence before retention.

An exact-membership ratchet failure triggers review; it is not an automatic
veto on independently witnessed structural recovery. Identify every lost/gained
name and original byte count, inspect complete-TU collateral, and establish
the source improvement and cause of the regression. Keep period differential
failures hard. After that review, an explained byte regression may be accepted
with an explicit current-floor rebase, the historical floor preserved in Git,
and a recorded recovery plan. Never hide a loss behind a net gain or reset a
floor just to obtain a green gate. The
[V.34 example](refinement.md#review-exact-losses-against-source-fidelity)
demonstrates why an unchanged neighbour can lose register bytes as preceding
source becomes more faithful. Finish the structural recovery, then remeasure
and recover that neighbour against the settled compiler state.

Retained reconstruction changes must pass Gentoo `make period`, with its test
denominator and the actual candidate source/profile, not a stale default build.
Run `make phase` before calling a reconstruction branch finished; run and report
`make portability` or `make phase-full` for a portability/release claim. Record
full-TU and strict partial-link before/after, including new losses.
Differential equivalence alone does not establish codegen.

Project completion is the original partially linked object under
`partialcmp.py --require-exact --require-raw-exact`, or
`make partial-compare-exact`. `--require-exact` checks semantic ELF equality;
`--require-raw-exact` additionally requires literal file bytes. A failing gate
remains DIFFERENT; intermediate improvement is not completion. Do not alter the measurement model unless an
independently demonstrated apparatus defect needs a separately tested fix.

## Handoff and delegated-batch template

Give each delegate a bounded TU/helper-family inquiry, isolated artifacts and
an explicit exclusion of `re/`. The parent reviews actual commands, independently
rescores results and inspects losses before adopting conclusions. Shared tools
and instructions land on master; active branches merge/rebase it.

Keep this ledger in the bounded scratch contract and artifacts. Batch its durable
result into Hindsight; a permanent handover/wave report is not required:

```text
Question and reference evidence:
Baseline/source/header identity; compiler/as identity; complete flags:
Hypothesis; competing explanation; predicted discriminator:
Domain/cell count; controls; artifact location:
Compile/rejection counts; shared-symbol denominators:
Exact gains/losses; missing/binding changes; nonexact body/call/data changes:
Independent review; differential/partial-link status (or NOT RUN):
Conclusion limited to this domain; alternatives still open:
Next test OR stopping reason and evidence needed to reopen:
```

Hindsight carries resumable investigation state, closed domains and next controls.
Findings/deviations preserve measured records. Issues are only for demonstrated
original-DSP bugs needing post-reconstruction repair behind the established
`DSPLIB_REPRODUCE_BUGS` gates. PRs retain code-review and validation evidence.
