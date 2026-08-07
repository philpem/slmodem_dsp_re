# The method, separated from the project

`docs/fastpass.md` is *this* pass's task scheduling — how the V.34 states were
split, the two task stores, the renumbering. This directory is the other tier:
**what someone reconstructing a different blob of this size should build on day
one.** Nothing here is about modems.

| file | what it is |
|---|---|
| `CLAUDE.md.template` | the drop-in project instructions. Six `<PLACEHOLDER>`s. Start here. |
| `tiers.md` | the four oracles and what each is structurally blind to |
| `efficiency.md` | token budget and wall-clock budget, which are unrelated problems |
| `gates.md` | the pattern behind every tooling defect this tree has found |
| `agents.md` | parallel batches: briefs, shared data, numbering, merging |
| `tools.md` | which tools are portable, and what each assumes about its host |

## The rule these were written under

**Every claim carries the measurement that justifies it and the finding number
it came from.** A rule without its evidence gets negotiated away by the next
session under time pressure: *"delegate large functions"* is ignorable,
*"sessions that finished took 10–25 turns; the five that ran out took 400–600
(finding 220)"* is not.

Where this tree learned something by getting it wrong, these files say so. That
is what makes them worth trusting.

Numbers cite `docs/findings.md` **in this tree**. `CLAUDE.md.template` is the
one file meant to be copied *out*, so its citations will not resolve in a new
project — they are the provenance of each rule, not links, and they should be
kept for that reason. The other five stay here and their references are checked
by `tools/refcheck.py` like everything else.

## What is deliberately not here

No evidence exists in this tree for how many batches to run concurrently, which
model or reasoning effort to give one, how to phrase a brief beyond the
measured/inherited rule, or how often to review. Those are real questions this
project never measured, and their absence is not a recommendation either way.
