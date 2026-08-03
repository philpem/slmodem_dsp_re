# The V.34 fast pass

**Applies to tasks #33 and #35–#45. Deferred work is task #47.**

Phase 6 is ~110 KB of code — `v34handshak` alone is 61 KB across 87 states.
At the pace phases 1–5 were done it is many sessions. This is the agreed
change of sequencing, decided after V.34's first 4.5 KB.

It is **sequencing, not a drop in quality**. Nothing that establishes
correctness is skipped.

## What every module still gets

1. Read from the disassembly, not from a summary of it.
2. Written in C.
3. **A tier-1 differential test against the blob.** Non-negotiable, and not
   overhead: it is what makes writing fast. `V34TimingFilter` took three
   attempts and each fault was localised in one run. Without it there is no
   way to know the code is right and no way to find out later.
4. A one-line entry in `docs/deviations.md` for anything that looks wrong,
   marked `unmeasured`.
5. Brief structural comments — what the object is, what the fields are, what
   the loops do.

## What is deferred to #47

- **Coefficient derivations.** Tracing a table to its closed form serves
  *regeneration* at a different sample rate. Until the 8 kHz retarget, a
  byte-exact copy is byte-exact and the differential test proves it with no
  derivation at all.
- **Reachability measurement** for deviations. The entry is cheap; measuring
  whether it fires is not.
- **File-header rationale** and findings prose.

## Why the one-line deviation entries are not deferred

An unrecorded observation is unrecoverable — it will not be noticed twice.
D28 is the precedent: entered cheaply, later measured, then retracted. That
is the process working, and it only works if the entry exists.

## The one rule that is not relaxed

**Nothing is committed that has not passed a differential test**, and nothing
is committed that is wrong-but-plausible. If a function cannot be made to
pass, it is left out and the attempt is recorded. Two functions were reverted
under exactly this rule during the first V.34 pass.

Run `make phase`, not `make test`.

## What the task numbers refer to

**The numbering is used throughout this tree and defined nowhere in it** —
not in `README.md`, not in the plan, not in any document here. Written down
because three sessions share this history and `#39` has already been quoted
in a hand-over summary by someone who could not look it up.

Only what the repository itself settles is recorded. Everything below is
sourced; nothing is inferred from adjacency of the numbers.

| | what it is | where that comes from |
|---|---|---|
| #23 | removing the limit finding 70 puts on `t_v8create`'s comparison | finding 70 |
| #33, #35–#45 | the V.34 fast pass | the line at the top of this file |
| #36 | `V34RX.c`'s remaining functions | finding 117 lists what was left in it |
| #38 | `V34hshak.c`'s support functions — that TU except `v34handshak` | this session's brief; findings 170–149 |
| **#39–#45** | **`v34handshak` itself**, 61,541 bytes over 87 states, split seven ways | findings 117 and 144, `include/dsplib/v34hshak.h` |
| #47 | the deferred work listed above | this file |
| #49 | check whether `demapFrame` can produce an out-of-range group | finding 129 |

`#34`, `#37`, `#40`–`#44`, `#46` and `#48` are named nowhere. `#34` is
outside the fast pass's stated range; the rest fall inside a range and have
no content recorded against them individually.

### The state-to-task split for #39–#45 is unassigned

Nothing records which of the eighty-seven states belong to `#39`, and it
**cannot be read off the state list**. What a dispatch case owns has to come
from the control-flow graph — that is the whole argument in
`tools/cfgsplit.py`'s header, which exists because the obvious estimate
(distance to the next jump-table target) produced three different 45–55 KB
"states", none of which exist.

**What is now settled, and was not when this section was written.** The
three state words are named — +0x3592 microstate, +0x3594 rxstate, +0x3596
txstate (finding 180) — and `StateName` is emitted, so `cfgsplit`'s
per-state byte counts can be read against the author's names rather than
against indices. The three machines are concurrent, so a per-state split has
to say which machine a state belongs to before it can say which task.

`cfgsplit` has not been run on `v34handshak`. The recorded runs are
`rxtiming` (finding 121k), `decodeDepth` (finding 131) and one that reported
a whole function unreached. Pointing it at `v34handshak` needs its three jump
tables located first, and `--entries` was needed for both of the functions it
has been run on — so **locating the dispatch and running `cfgsplit` is the
first job of #39, not a prerequisite for starting it.** Until that is done,
"#39" names a share of the work and not a set of states.
