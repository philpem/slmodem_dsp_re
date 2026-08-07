# Running batches in parallel

Every number cites `docs/findings.md` in this tree.

Parallelism here means **a worktree and a branch per batch**, each batch owning
a disjoint set of source files, each running the phase gate and seeing its own
work pass. It is the only structural remedy for the token wall (finding 220,
and `efficiency.md`), and it costs three things: hand-overs go wrong, shared
data structures collide, and the merge is where both bills arrive.

This file is what was learned paying them. Nothing here is about how many
agents to run, which model to use, or how to phrase a prompt — **none of that
was measured in this tree, so none of it is here.**

---

## 1. Scope a batch by what it owns, not by what it is about

Two batches editing the same file is a merge conflict you chose. Two batches
editing *different* files that share a data structure is a merge conflict you
did not.

- No `src/` file was touched by both lines of the biggest merge here, so **the
  substance merged itself** — all seven conflicts were in shared tooling and
  docs (finding 544).
- The shared things that collided: the Makefile, one tool's exclusion list, the
  deviation register, and the findings file. Plan for those; the source is
  fine.

## 2. A brief must mark MEASURED and INHERITED separately

In one session a relayed premise was wrong **five times**, and in each case the
receiving batch disproved it rather than working around it:

| what was relayed | what was true |
|---|---|
| the accept path's byte copy has no bound and meets it at length 0x4d | the dispatch sits *ahead* of the copy; no sized arm reaches it (finding 352a) |
| landing txstate 66 would retire an uncaught mutation | 66's block ends in an unconditional jump, **and** the named mutation was a different transfer entirely — wrong in both halves (finding 359a) |
| microstate 58 is on the `= 4` side of the reset split | it has one of each (finding 359a) |
| "keep `w4_hs_t2`'s copy of table 2" | there were **three** copies, not two, and the most complete reading was a third file's (finding 550) |
| the reload is at `0x62b45` | `0x62b45` is where the `mode == 1` branch lands; **the reload is `0x62b5f`** (finding 591) |

The common shape: **a hand-over paragraph is written by an agent that has not
driven the thing it is describing.** Passed on as established fact, it gives
the next agent a premise it will either trust or spend time disproving
(finding 352a).

> **"The previous batch reports X" is a lead. "I measured X" is a fact. A brief
> must say which it is passing on** (findings 352a and 359a), **and the
> receiver must check an inherited claim before acting.** Four words.

Two corollaries the same sessions produced:

- **An instruction can be un-obeyable by the time it is read.** "Keep this
  one's" was right about which of the two files *then in the merge* had the
  better arms; it did not survive a third copy existing and a caller being
  measured that did not exist when it was written (finding 550). Record the
  *cost of each direction* so the next batch does not have to re-derive it.
- **A status column is written from one tree.** One document ended a merge
  saying, in two different places, that two *different* items were the last one
  open. Both batches wrote their line from their own tree and each was right
  about what it could see. The count in the test's own header was wrong on both
  sides too — one said seventeen, the other eighteen, and the answer was
  nineteen (finding 359a). Saying "write this column from the tree" in the
  legend was not enough.

## 3. Shared data structures break across batch boundaries silently

Mutation anchors are the worked example, and this happened **five times**
before it was tooled and repeatedly after.

The runner matches an anchor as a substring of the whole file. When a second
near-identical arm of a big dispatch lands, an anchor written against the first
matches twice — **and a doubly-matching anchor reports UNUSABLE, which does not
fail a run.** The suite still prints `0 NOT caught`. Three suites quietly lost
mutations that way (finding 347); landing two more arms broke eighteen anchors
at once (finding 432); and merging two batches broke two neighbouring suites'
anchors for the third time in one file (finding 446).

What works:

- **After adding to a shared file, re-run every suite over it and read the
  UNUSABLE count, not the NOT-CAUGHT count.** The one that matters is the one
  that does not fail (finding 347).
- **Give each batch a macro-name prefix of its own.** `T41_`, `T44_`, `T46_`
  were introduced against `#define` collisions and turned out to be just as
  useful as the thing that makes an anchor unique: a shared call line is what
  every arm has, and a prefixed offset name is what only one has (finding 347).
  The repair tool picks the occurrence whose surrounding lines mention the
  batch's own prefix most, **and prints the line number it chose for every
  one**, so the choice is auditable rather than trusted.
- **Watch for a mutation whose validity depends on what else is absent.** One
  rewrote its own `case` label; that compiled while its arm was the only one in
  the tree and became a duplicate case label the moment a neighbour landed
  (finding 347).
- **A killed run leaves its mutant in the source, and one was committed that
  way** (finding 349). Check `git diff` before committing after a run that did
  not finish.

## 4. Numbering collides. Plan the collision, do not hope

Nine collisions here. The ninth was **twenty-three numbers**, allocated by both
lines to entirely different findings and quoted freely on both sides all
session (finding 543).

- **Leave a gap, and check every branch — not just the mainline — before
  claiming a block.**
- **Your reference checker cannot see the failure that matters.** Every
  re-pointed reference still *resolves*; there IS a finding by that number
  after the merge, it just belongs to somebody else. A clean run after a merge
  proves nothing about it, which is why the count of dangling references is the
  wrong instrument and **nobody noticed for 158 commits** (finding 543).
  Misdirection is worse than dangling: dangling is loud (`tools/refcheck.py`).
- **Renumber first, merge second.** The other way round, the merge has to
  resolve twenty-three heading collisions inside an append-only file, which is
  a hunk resolution — and hunk-resolving the record is how content gets
  silently dropped. Renumbered first, the two sides no longer overlap and the
  merge is two appends (finding 543).
- **Move the side with fewer REFERENCES, not fewer findings.** Renumbering is a
  text substitution over references, so its risk scales with how many
  references there are, not with which line "owns" the numbers or which is the
  mainline. Here it was 113 against 25 (finding 543).
- **A bare-number rewrite is not available.** In exactly the colliding range,
  this tree had filter coefficients `340`, `348` and `361` in one source file,
  `619`, `355` in another, and byte counts in two docs — indistinguishable from
  a citation on digits alone. The rewrite matched explicit forms only (86
  references across 16 files), and then **every remaining occurrence in prose
  was listed and read by hand**, which is what caught a bare `(350)`
  parenthetical the automated pass had missed (finding 543).
- **Write citations in the forms your checker parses.** A comma-separated list
  — `Findings 354, 356 and this one` — matched on its first number and not its
  second, leaving the second dangling. That was caught only because the target
  number no longer existed at all; **had the collision run the other way, the
  same miss would have left a reference that still resolved, at the wrong
  finding, with nothing able to detect it.** The dangling check worked there
  purely by luck of direction (finding 543).
- **Every moved finding says what it used to be called.** That note is the only
  signal a branch cut before the merge will ever get (finding 543).
- Task numbers are not safe either: two task stores existed here whose
  `#11`–`#22` were different work. Say which store you mean.

## 5. Most merge conflicts are COMBINATIONS, not choices

158 commits met 44. Of seven conflicted files, **four could not be resolved by
choosing a side — and in each case picking one would have compiled, passed, and
quietly dropped the other line's work** (finding 544):

```
  Makefile        one line added -no-pie, the other added the C++ object list,
                  on the same six link rules.  BOTH are needed.
  a tool's        15 header exclusions from one line's work, 25 from the
  skip list       other's.  Union: 32.
  the deviation   one line's D62 against the other's D59-D61.  Union.
  register
  the findings    two append streams.  Both.
```

Only two were genuine supersessions.

**And taking the superset is not the same as merging it.** One side's hunk had
renamed a variable because in *its* version the variable was unused. The other
side used it eleven lines further down, **outside the conflict**. Taking that
hunk whole produced a `NameError` — caught immediately, but only because it was
a Python name error in a script that runs. **The same shape in C, or in a
branch not exercised by the phase gate, is a silent merge defect.**

> **A conflict hunk is a window, and the variable it renames may be read
> outside the window** (finding 544).

## 6. What the merge is worth

Numbers neither side could produce alone (finding 544):

```
  diagnostic sites never executed    30 of 431  ->   6 of 432
  suite line coverage over src/          98.1%  ->    98.6%
```

One line's call-progress and dialler work closed twenty-four of the sites the
other could not reach, and the other's handshake work is what the remaining 432
measure against.

## 7. What is deliberately absent from this file

No evidence exists in this tree for: how many batches to run concurrently,
which model or reasoning effort to give a batch, how to phrase a brief beyond
the measured/inherited rule above, or how often to review. Those are real
questions and this project never measured them. Do not read their absence as a
recommendation either way.
