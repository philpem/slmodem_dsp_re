# The method, separated from the project

This directory records reconstruction technique, evidence rules and compiler
policy. For this modem's modules and algorithms, use the
[Maintainer Manual](../maintainer/index.md). Completed project scheduling plans,
including the [V.34 fast pass](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fastpass.md), are historical.

## Repeat the reconstruction on another project

Start with [Recovering source from a compiled object](recovering-source.md).
It explains the sequence taken here, the lessons brought forward from later
work, and the checkpoints to establish in a new project. The examples assume
GCC 3 and a relocatable x86 ELF object with surviving symbols; each guide
identifies the evidence and tools that need adapting for another target.

1. Follow [the recovery workflow](recovering-source.md) from the reference
   inventory through a differential oracle to tested source and integration.
2. Use [Reading object code into source](reading-object-code.md) while
   recovering call contracts, layouts, arithmetic, control flow and tables.
3. Use [Recovering a GCC build environment](recovering-build.md) alongside
   that work to identify the toolchain, test option hypotheses and diagnose
   generated-code differences.

## Detailed method references

[Function fidelity reporting](fidelity-reporting.md) explains EXACT, REGALLOC,
NONMATCH, contiguous instruction runs and diagnostic byte-size records.

| file | what it is |
|---|---|
| [Recovery timeline](timeline.md) | Dated milestones, scoped progress figures and identifiable agent/tool provenance. |
| [Recovery workflow](recovering-source.md) | Ordered procedure, project history and checkpoints for a new reconstruction. |
| [Object-reading techniques](reading-object-code.md) | Relocations, ABI, types, control flow, fixed point, x87 and data recovery. |
| [Build recovery](recovering-build.md) | GCC provenance, option experiments, compiler stages and identity checks. |
| [Compiler policy](compilers.md) | Period compiler authority, portability and floating-point boundaries. |
| [Experiment design](experiment-design.md) | Baseline controls, crossed experiments and stopping conditions. |
| [Evidence](evidence.md) | What each form of evidence establishes. |
| [Equivalence](equivalence.md) | Function identity and bounded/composite proof. |
| [Conformance plan](conformance-plan.md) | Independent standards checks and their limits. |
| `CLAUDE.md.template` | the drop-in project instructions. Seven `<PLACEHOLDER>`s. Adapt after establishing your project's boundary. |
| `tiers.md` | the current oracles and what each is structurally blind to; archived mutation results are labelled historical |
| [Efficiency](efficiency.md) | Model usage, context isolation, free local computation and elapsed-time budgets. |
| `refinement.md` | what actually closes the last bytes: every lever with its counterexample |
| [Byte bit tests](byte-bit-tests.md) | a measured narrow-operand source form for immediate bit-7 tests, with negative controls and transfer limits |
| `placement-ledger.md` | causal linked-position ledger: why each function moved, and the first shift |
| [Agent batches](agents.md) | Owned tasks, compact briefs, context costs, shared data and integration. |
| `tools.md` | which tools are portable, and what each assumes about its host |
| `recording.md` | how to write the record: naming, the scope of a quantity, correcting a pushed finding |

The [historical working-instruction snapshot](working-instructions-history.md)
preserves the examples formerly loaded at every agent startup. Its counts and
queues are historical; [CLAUDE.md](../../CLAUDE.md) holds the active rules.

## The rule these were written under

**Every claim carries the measurement that justifies it and the finding number
it came from.** A rule without its evidence gets negotiated away by the next
session under time pressure: *"delegate large functions"* is ignorable,
*"sessions that finished took 10–25 turns; the five that ran out took 400–600
(finding F220)"* is not.

Where this tree learned something by getting it wrong, these files say so. That
is what makes them worth trusting.

Numbers cite `docs/findings.md` **in this tree**. `CLAUDE.md.template` is the
one file meant to be copied *out*, so its citations will not resolve in a new
project — they are the provenance of each rule, not links, and they should be
kept for that reason. The method documents stay here and their references are checked
by `tools/refcheck.py` like everything else — which only works because they are
tracked files: the checker walks the index, so a new document that has not been
`git add`ed is invisible to it (finding F670). `recording.md` is where the rest
of that discipline lives.

## What is deliberately not here

No evidence exists in this tree for how many batches to run concurrently, which
model or reasoning effort to give one, how to phrase a brief beyond the
measured/inherited rule, or how often to review. Those are real questions this
project never measured, and their absence is not a recommendation either way.
