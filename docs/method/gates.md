# The pattern behind every tooling defect this tree has found

Every number cites `docs/findings.md` in this tree.

## The pattern

**A result indistinguishable from success.**

Not "the tool was wrong" — a tool that is wrong loudly gets fixed the same
afternoon. Every defect found in this tree's own tooling produced output that
a clean tree would also produce, so nothing distinguished a passing run from a
detector that had checked nothing.

Six instances, each found the hard way:

| the defect | what it printed |
|---|---|
| an anchor matches twice (finding 347) | `UNUSABLE` — and the suite still says `0 NOT caught` |
| an anchor stays unique and silently re-points (finding 432) | `CAUGHT`, at a claim nobody made |
| a registered suite is skipped, four silent `continue`s (finding 540) | `0 anchors wrong`, exit 0, having checked nothing |
| a mutation whose `replace` equals its `find` (finding 542) | `survived, equivalent` — same as a real equivalent |
| a signedness detector that cannot find its own defect (finding 618) | `(none)`, through four consecutive versions |
| 242 diagnostic call sites dropped (finding 134) | nothing — the debug level ships at 0, every gate is `> 1` |

Two more of the same shape, from adjacent tools: a malformed mutation registry
makes every suite unrunnable while the phase gate stays green, because nothing
in the phase boundary opens the file (finding 346); and a suite pointed at the
wrong binary reports NOT CAUGHT for everything, which is what an untested claim
looks like — six sets were misread that way before a manifest existed
(`tools/mutate.py`).

---

## The five rules

### 1. Make the tool COUNT what it examined, and FAIL on the difference

"A count of what was checked is the difference between a clean tree and a dead
detector" (finding 540). Neither the anchor checker nor the reference checker
said how many suites it had actually looked at, and four `continue`s in one
`main` were silent: a registry entry of the wrong shape, a source path that no
longer exists, a registered suite with no mutation file, and a mutation file
that will not parse.

### 2. Reporting is not gating

When the vacuous-mutation problem was first fixed, the runner learned to print

```
  ????  INJECTED: replace equals find     VACUOUS -- REPLACE == FIND
```

and count it UNUSABLE. **That was not enough**, and the reason is finding 347:
an unusable mutation does not fail a run. The repair converted a mutation that
was invisible into one that was visible in a log nobody re-reads — most of the
distance, and not the part that holds.

The static half is free — no build, no suite, a string compare over JSON the
anchor checker already parses — and **it exits 1**. It costs milliseconds
against 2,874 mutations, and a vacuous entry now cannot be committed at all
(finding 542).

> **If the run still exits 0, the report is a comment.**

The converse also holds, and it is why the staleness half of the snapshot check
only *reports*: a gate that is red by default is worse than none, because it
gets ignored or switched off (finding 545). Fail on MISSING, ORPHANED and
INCONSISTENT — the things that are unambiguously wrong. Report the thing that
is merely out of date.

### 3. A detector nobody has seen fire is not a detector

Finding 134's argument, and it has been quoted against four separate tools.

The signedness detector took **five corrections, and the fifth was found only
by testing the tool against the one defect already known. The first four
versions all reported confidently and none of them could find it** (finding
618). Validation, in both directions, on a known answer:

```
  reintroduce  struct b103_hdx.mode as `short`   -> reports TxHdxStartB103
  restore      unsigned short                    -> the report disappears
```

Before that test it printed `(none)` and there was no way to tell a clean tree
from a broken detector.

The anchor checker was validated the same way, by reintroducing five distinct
defects and watching each appear with a non-zero exit (finding 540):

```
  stale source path                       exit=1  SKIPPED     source ... does not exist
  registered suite with no mutation file  exit=1  SKIPPED     registered, but there is no ...
  `fn` naming the wrong C++ method        exit=1  RE-POINTED
  `fn` naming a macro the regex accepted  exit=1  BAD fn      names no function in ...
  nothing wrong                           exit=0  (nothing)
```

### 4. Measure the same thing two ways and compare

The snapshot records per-mutation verdicts **and** the summary line, from
different code paths, and cross-checks them. Both times the check fired, the
tool was wrong and not the record (finding 545):

1. `%-52s` **pads but does not truncate.** The label regexes capped at 52
   characters, so all 21 of one suite's longer labels matched nothing: 188
   verdicts recorded under a summary saying 209.
2. An unusable mutation **does** reach a verdict line. The comparison added
   `unusable` to the verdict count on the assumption those entries were
   missing. It agreed with itself on 47 suites and flagged the one suite in the
   tree with an unusable mutation — **wrong by exactly the number of cases that
   could distinguish it, which is the shape of every bug in this area.**

Twenty lines. Neither bug was reachable any other way.

### 5. Prefer a structural test to a lexical one

The anchor checker's definition-finder was `^([A-Za-z_][A-Za-z_0-9]*)\(`. That
character class excludes `:`, so **every qualified C++ method was invisible** —
and a bare `^NAME(` also matches a macro invocation at column 0, so what the
index filled with instead was noise. One file reported **45 definitions, all 45
of them the same macro, and not one a function**; the structural version
reports 8, which is how many it has (finding 540).

Two consequences, the second worse than the first: the enclosing-function
lookup returned a macro name for every anchor in every C++ file, and the exact
`"fn"` field — *the one mechanism documented as having no false positives* —
would report `BAD fn` for any real method while **accepting the macro's name**.

> Match the name, walk to the matching close paren, require the next non-space
> character to be `{`. That separates a definition from a macro call and from a
> forward declaration, and it does not care what characters the name is spelled
> with.

---

## Two failure modes worth naming separately

**Uniqueness is not the property that matters — WHERE it lands is.** Nine
mutation entries in one suite were mutating a different arm from the one their
label named, and all nine were reported CAUGHT. Two were anchored on "the last
`t3m_txblock` before the section comment", which was arm 59's exit until arm 58
landed after it. Seven labelled `the shared reset (47, 49, 50)` were mutating
arm 51's record fill: the two bodies share six lines exactly, the shared reset's
copy sits at one tab and arm 51's at two, the runner matches a substring, and at
some earlier repair they had been made unique by **deepening** them — which
moved all seven. The repair is a **leading newline** (finding 432).

> "The last X in the file" and "the X before the next section comment" are
> anchors about the file's *layout*, and the file's layout is exactly what the
> next batch changes. An anchor must sit inside the thing it is about, and
> *check* that it does rather than assume it. Where the code has no unique
> line, the diagnostic string it prints is the one thing that is its own.

They were noticed only because the NOT-CAUGHT count went 0 → 4 and two of the
four were separable. **Had the arm's exit been unseparable they would have gone
on passing, at a claim nobody made.** Finding the first two by accident is not
a method; the sweep that found the other seven is four lines on top of the
function ranges the repair script already computed (finding 432).

**A defect the gate cannot see is still a defect, and the tool is the test.**
The 242 dropped diagnostic call sites could not have been caught by anything —
the debug level ships at zero and every gate is `> 1`, so a missing call and a
present one behave identically under every test in the tree. That is why it
needs a *tool* rather than a test (finding 134). And the first version of that
count was itself wrong — it reported 399 calls, which was the count of
functions containing a call — an error found by writing the tool to make the
count repeatable. **That is a fair argument for making one-off measurements
into tools.**

---

## What a merge does to your gates

The first time two long-lived branches met here, the anchor uniqueness check
found a **live defect on the mainline the moment they touched**: a diagnostic
message that one commit had put in two switch cases without re-anchoring the
mutation pinned to it. It had matched twice ever since — reported UNUSABLE, and
unusable does not fail a run. The mainline's own checker had no uniqueness
check, so **the merge was the first moment anything looked** (finding 544).
Second live instance that check found in a day; both had been silently untested
for months.
