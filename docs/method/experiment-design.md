# Reconstruction experiments without getting stuck

Agent workflow distilled from [GitHub #22](https://github.com/philpem/slmodem_dsp_re/issues/22).
Use with [refinement.md](refinement.md), which describes the individual levers.
Measured examples and commands live in [cid-dcr-audit.md](../cid-dcr-audit.md);
outstanding work lives in issues.

## The target is an object, not the minimum of a score

The reference object is canonical. Current source and flags are hypotheses,
including source that currently produces exact functions. Two reconstruction
choices can compensate for one another. Do not freeze them merely because
changing one loses matches, or conclude that those losses prove wrong source.

Exploration need not improve monotonically. Keep informative losing controls
and test justified combinations. Do not merge them as production improvements.
A higher exact-function count or fewer differing bytes does not establish
recovery: behavior, instructions/operands, call boundaries, data, exports and
partial-link layout must all agree. Do not claim a global optimum from a finite
experiment. Preserve complete function bodies and original bugs; never delete
behavior, add padding or weaken measurement to obtain a hit. Prefer clear,
idiomatic source when equally supported spellings produce the same output.

## 1. Validate the experiment before interpreting it

Read the issue's latest results and relevant audit section. Identify what is
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

Control drift, unexplained missing symbols, or empty results stop interpretation.
Preserve invalid runs as invalid and rerun; do not repair their conclusions
after the fact. #22 required reruns for omitted register renaming and omitted
bug reproduction.

A period-compiler rejection under alternative flags is evidence about the
tested source/profile pair. Investigate required inline bodies, instantiation
and visibility before ruling out the profile globally or changing source just
to make it compile. An incomplete build has no valid whole-object accuracy
denominator; report compilation coverage and the failures separately.

## 2. Cross explanations, rather than hill-climbing one axis

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

## 6. Separate diagnosis, acceptance and completion

Label outcomes: invalid experiment, negative result, mechanism control, source
candidate, or validated retained change. Attributes, duplication or barriers
used only to isolate a mechanism are diagnostics, not recovered source. A
successful diagnostic needs independent source evidence before retention.

Retained reconstruction changes must pass Gentoo `make period`, with its test
denominator and the actual candidate source/profile, not a stale default build.
Run `make phase` before calling a branch finished; interpret its modern tier
according to AGENTS.md. Record full-TU and strict partial-link before/after,
including new losses. Differential equivalence alone does not establish codegen.

Project completion is the original partially linked object under
`partialcmp.py --require-exact`. A failing gate remains DIFFERENT; intermediate
improvement is not completion. Do not alter the measurement model unless an
independently demonstrated apparatus defect needs a separately tested fix.

## Handoff and delegated-batch template

Give each delegate a bounded TU/helper-family inquiry, isolated artifacts and
an explicit exclusion of `re/`. The parent reviews actual commands, independently
rescores results and inspects losses before adopting conclusions. Shared tools
and instructions land on master; active branches merge/rebase it.

Record this ledger in the issue or a linked artifact:

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

Issues are the open-work queue; findings/deviations are the measured record.
Link them so the next agent resumes rather than rediscovers the starting point.
