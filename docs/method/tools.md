# What is portable, and what each tool assumes about its host

Every number cites `docs/findings.md` in this tree. Paths are relative to the
reconstruction's root.

Most tools here are about *this* blob and do not travel: the translation-unit
mapper, the coefficient extractors and the offset checker. The methods below
cover reusable tools and their host assumptions.

---

## Current tools

The mutation runner, snapshot manager, anchor checker and reanchoring tool
described in older revisions were removed. Their archived measurements are
retained in the method history, but they are not part of the current workflow.

## `tools/refcheck.py` — hold the tree to its own cross-references

Every claim is argued once and cited everywhere else. `--dangling` (the default,
cheap, in the phase gate) checks that every reference resolves. `--since REV`
checks that each still *means* what it did at REV, and must be run against each
parent after a merge:

```sh
git log --merges -1 --format=%P | tr ' ' '\n' | \
    xargs -I{} tools/refcheck.py --since {}
```

**The cheap mode is not the important one. Misdirection is worse than
dangling** — a dangling reference is loud, a missed renumber still resolves, to
an entry about something else, and reads exactly like a correct citation. All
six survivors of one earlier merge were of the second kind (`tools/refcheck.py`),
and the ninth collision went unnoticed for 158 commits because every reference
still resolved (finding F543).

**Assumes** your citations are written in forms it parses — `finding N`,
`findings N, M and K`, and a bare `DN`. Write them that way from day one: a
comma-separated list once matched on its first number only, and a bare
parenthetical `(350)` was missed by an automated renumber pass (finding F543).

Two things it grew that have nothing to do with references, and both belong
wherever your one tool already walks every tracked file:

- **Conflict markers.** The record reached `origin` with `<<<<<<< HEAD` in it —
  a merge resolved by script, staged, committed, every gate green, because
  nothing else reads prose. The record is the deliverable, so a marker in it is
  as much a defect as a failing test.

---

## Two more that are method-shaped, if your blob carries diagnostics

- **`tools/debugaudit.py`** — which diagnostic call sites are missing, what
  they were going to say, and (`--invented`) every string literal *your tree*
  carries that appears nowhere in the object. A string in that list was written
  rather than read, usually from the function's own name, and no test catches it
  unless something compares that function's transcript; four were found this way
  (finding F180). It scans **every** literal, not only the ones at a call site,
  because a format reached through a variable or indexed out of a table has no
  literal at the call — and those are exactly the sites a reader assumes are
  covered.
- **`tools/devaudit.py`** — which entries in the deviation register have a test
  behind them, by asking whether any compiled test object references the
  function's blob alias. The test is a **necessary** condition, so **a "no" is
  conclusive and a "yes" is an invitation to look**; that asymmetry is what
  turns 59 entries into a short list worth reading. Two ways a "no" is still
  wrong, both met in practice: a table read internally by a tested function, and
  an alias that exists at six addresses because the name is file-static in six
  translation units (finding F622).

## What does not travel

The disassembler wrapper, the TU mapper, the coefficient tools, the offset
checker and the codegen comparator are all specific to an x86-32 ELF object
built by GCC 3.4. The *comparator's* design is portable and its precision note
is the part to copy: **it compares mnemonics, not bytes** — two functions
storing the same constants to different offsets both read as `mov mov mov` —
and its total-bytes percentage is the weak number that moves when you emit more
code, not only more of the right code (finding F616, and `tiers.md`).

Two things about its **ratchet** are portable and both cost a run here.
`--ratchet` compares against the recorded floor and `--update` is what writes
it, so a gain measured at a merge and reported rather than blessed leaves the
floor behind, and a later regression prints `gained` (finding F556). And the
period-toolchain container reports a partial object set on its **first**
invocation — `identical ... now 48` against 105 on every run after — so a
floor blessed from one cold reading is sixty symbols too low for ever
(finding F651).
