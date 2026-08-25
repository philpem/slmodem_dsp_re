# What is portable, and what each tool assumes about its host

Every number cites `docs/findings.md` in this tree. Paths are relative to the
reconstruction's root.

Most tools here are about *this* blob and do not travel: the translation-unit
mapper, the coefficient extractors, the offset checker. The five below are
about **method** rather than about a modem, and each one exists because a
specific failure happened. They are worth lifting into a new project on day
one, with the host assumptions listed against each.

---

## `tools/mutate.py` — break the code on purpose

The anti-vacuity tier. `--suite NAME`, `--all`, or a bare
`source binary mutations.json` triple. A mutation set is JSON:

```json
[{"label": "swap the two arguments",
  "find":  "...exact text, must appear exactly once...",
  "replace": "..."}]
```

**Assumes:**

- `make <target>` builds one test binary, and `make BUILD=<dir>` puts the
  objects somewhere else. That one existing parameter is what made parallelism
  cost no build-system change (finding F541).
- `test/mutations/suites.json` maps each set to `[source, binary]`. **Use the
  manifest, not the positional form** — getting the pairing wrong produces
  NOT CAUGHT for every mutation in the set, which is the same output an
  untested claim gives and indistinguishable from it without looking. Six sets
  were misread that way before the manifest existed.
- Verdicts are reproducible. 209 mutations were identical across a serial run
  and two eight-way parallel runs (finding F541). The one classification that is
  not reproducible in principle is `hang` — caught by a timeout rather than by
  a check — because a loaded machine can move it.

**`--jobs N`.** A tree per worker, not a lock: 209 mutations went 287.22 s →
61.69 s, 4.7x (finding F541). Two host assumptions that will bite:

- Workers are created as **siblings of the real tree**, not under `/tmp`,
  because a relative symlink into a peer directory only resolves at the same
  depth (finding F541). If your tree has no relative symlinks this is free; if
  it does, keep it.
- `MIN_PER_WORKER = 12`, because a worker must earn its setup. Below that
  threshold sharding made three small suites 60% slower (finding F541). Retune
  it against your own build time — the numbers behind it are a 0.67 s rebuild
  and a 0.73 s run.

A dying shard aborts the run with exit 2 rather than reporting a confident
partial total (finding F541).

**`--only TEXT`.** Runs just the mutations whose label contains TEXT, which is
the question a batch actually asks twenty times: *did the mutations I have
just touched still fail?* One label against `v34hshak`'s whole suite is
**4.17 s against 62 s at `--jobs 8`**; against `v34hstx1`, ~4 s against ~180 s
at `--jobs 4` (finding F555). The speed is the smaller half. **A subset run's
summary is otherwise indistinguishable from a full run's**, and that number
lands in commit messages, so the subset line deliberately does not contain the
string the snapshot recorder scans for — verified by grepping its output for
that string and getting zero (finding F555, and `gates.md` rule 2). Adding the
flag invalidated all 48 snapshot entries, because the runner is what decides
what "caught" means.

## `tools/mutsnap.py` — the keyed snapshot

Records what every suite last said **together with a hash of everything that
can reach its test binary**, so staleness is detectable without re-measuring.
That is what removes the baseline pass every batch was paying twice for
(finding F545). `--update`, `--check`, `--verify`, `--strict`, `--jobs`.

**Assumes:**

- **You can name the link closure.** Here it is `Makefile`, `src/`, `include/`,
  `test/harness/`, plus the suite's own driver, its mutation JSON, and
  `tools/mutate.py` — the runner, because *what counts as caught is its code*.
  Nothing else under `tools/` can move a verdict, so nothing else is in the key.
- **The closure is coarse on purpose.** A first design keyed each suite on its
  own source plus its own mutations. That is unsound when every test binary
  links all of `src/` — editing one file can change another suite's verdicts
  while both hashes still match. **If your binaries link narrowly, a narrower
  key is sound; if they link everything, a per-file key is a precise lie**
  (finding F545).
- **It fails on MISSING / ORPHANED / INCONSISTENT and only reports staleness.**
  Deliberate: a gate that is red by default is worse than none (finding F545,
  and `gates.md` rule 2).

Port the cross-check too: it records per-mutation verdicts and the summary line
from different code paths and compares them, and **both times it fired the tool
was wrong, not the record** (finding F545).

**What a day of using it costs, and it is the right trade.** The key is coarse,
so almost any edit invalidates every entry: naming twelve struct fields touched
`include/` and restaled all 48 (finding F638), and adding one flag to the
mutation runner restaled all 48 again (finding F555) — on the same day.
Re-running all 48 is 2,874 mutations:
**12 minutes at `--jobs 8`** (finding F545), **15m 40s at `--jobs 4`** on a
machine shared three ways (finding F638). The honest answer to invalidation is
therefore **re-run, not re-key**, including — especially — when the edit
obviously cannot move a verdict: that field-map re-run moved **not one verdict
out of 2,874** (finding F638), and the run that did find something found it
because `--verify` compares against the record **by name** rather than by
count, catching one mutation that had gone CAUGHT → UNUSABLE while the phase
gate stayed green (finding F637). A count would have seen one verdict move
between two columns of a 209-mutation suite and had nothing to say about which
mutation it was.

If you size the batches, `--jobs 4` on a shared machine ran the four biggest
suites — 749, 443, 214 and 209 mutations — in 7m 31s and the remaining 44 in
8m 09s (finding F638).

## `tools/anchorcheck.py` — the structural definition-finder

Answers "does each mutation still mutate the thing its label names". Two things
in it are worth lifting even if you never adopt its heuristic:

1. **The definition finder.** Match the name, walk to the matching close paren,
   require the next non-space character to be `{`. The lexical version
   (`^([A-Za-z_][A-Za-z_0-9]*)\(`) missed every qualified C++ method and filled
   its index with macro invocations at column 0 — 45 "definitions" in a file
   with 8 (finding F540). The structural test does not care what characters the
   name is spelled with, and it separates a definition from both a macro call
   and a forward declaration.
2. **The vacuous-mutation gate.** A string compare over the same JSON, no build
   and no suite, and **it exits 1** — so a mutation whose `replace` equals its
   `find` cannot be committed. Milliseconds against 2,874 mutations
   (finding F542).

**Assumes** the labels in your mutation sets name something the source names
too — here, a microstate that the dispatch binds to one function. That part is
a heuristic over prose and a clean run is not a proof; it is deliberately quiet
about an anchor in a shared helper and about a label with no identifier in it.
**It is the difference between checking nothing and checking the case that has
already gone wrong nine times** (finding F432).

It also counts and reports how many suites it examined, and fails on a skip —
which is the whole of finding F540.

## `tools/reanchor.py` — repairing anchors after a neighbour lands

When a second near-identical body lands, an anchor written against the first
matches twice, reports UNUSABLE, and does not fail the run (finding F347). The
repair is mechanical — grow the `find` string by whole lines until it matches
once — but **which occurrence to grow from is not.** Growing from the wrong one
silently re-points a mutation at a different claim, which is worse than leaving
it unusable.

**Assumes a per-batch naming prefix.** It picks the occurrence whose
surrounding 1,200 characters mention the batch's own prefix most (`--prefix
T44_`), **prints the line number it chose for every one** so the choice is
auditable, and refuses where no prefix separates the candidates. The prefixes
were introduced for a different reason — stopping `#define` collisions between
parallel batches — and turned out to be exactly what makes an anchor unique
(finding F347). If your project has no such convention, introduce one before you
need this tool.

**That refusal was a promise the tool did not keep until finding F2150.** The
guard was gated on `--prefix` having been given and the sort's second key was
the file offset descending, so with no `--prefix` every occurrence scored the
same, the guard could not be reached, and the LAST textual match won silently —
nine mutations moved out of `getV90Decision` and into `getV92Decision`, reported
as "9 re-anchored, 0 left for a human" (finding F2120; 455 is the same thing one
occurrence at a time). The guard is now unconditional and the offset tiebreak is
gone, so **an ambiguous anchor with no `--prefix` reads STUCK and exits 1**. The
repair was demonstrated against the exact input that beat it, in all three
directions: old tool wrong-and-silent, new tool refusing, new tool still placing
a case a prefix genuinely separates.

**Known limitation, and it is not academic:** it only grows anchors **upward**,
and two of one batch's hardest cases needed downward growth — six identical
lines that first differ in the diagnostic *below* them (finding F432).

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
- **The mutation registry parses.** Merging two branches that each appended a
  line produces a trailing comma about one time in three, and a malformed
  registry makes **every** suite unrunnable while the phase gate stays green
  (finding F346).

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
