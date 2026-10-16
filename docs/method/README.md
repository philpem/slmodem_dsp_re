# The method, separated from the project

This directory records reconstruction technique, evidence rules and compiler
policy. For this modem's modules and algorithms, use the
[Maintainer Manual](../maintainer/index.md). Completed project scheduling plans,
including the [V.34 fast pass](../history/project/fastpass.md), are historical.

| file | what it is |
|---|---|
| [Compiler policy](compilers.md) | Period compiler authority, portability and floating-point boundaries. |
| [Experiment design](experiment-design.md) | Baseline controls, crossed experiments and stopping conditions. |
| [Evidence](evidence.md) | What each form of evidence establishes. |
| [Equivalence](equivalence.md) | Function identity and bounded/composite proof. |
| [Conformance plan](conformance-plan.md) | Independent standards checks and their limits. |
| `CLAUDE.md.template` | the drop-in project instructions. Seven `<PLACEHOLDER>`s. Start here. |
| `tiers.md` | the four oracles and what each is structurally blind to |
| `efficiency.md` | token budget and wall-clock budget, which are unrelated problems |
| `gates.md` | the pattern behind every tooling defect this tree has found |
| `refinement.md` | what actually closes the last bytes: every lever with its counterexample |
| `agents.md` | parallel batches: briefs, shared data, numbering, merging |
| `tools.md` | which tools are portable, and what each assumes about its host |
| `recording.md` | how to write the record: naming, the scope of a quantity, correcting a pushed finding |

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
