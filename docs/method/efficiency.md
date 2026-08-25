# Token and time

Every number cites `docs/findings.md` in this tree.

Two budgets, and they are unrelated. Tokens are spent by the agent and the
limit is a **turn count**. Wall-clock is spent by the machine and the limit is
**work repeated because nobody wrote the answer down**.

---

## Part 1 — tokens: the wall is a turn count, not a size

**Context growth is cumulative output, and the ratio converges to 1**
(finding F220). Measured on one session that reached the wall:

| turn | context | cumulative output | ratio |
|--:|--:|--:|--:|
| 25 | 69,945 | 12,453 | 5.62 |
| 100 | 143,961 | 86,842 | 1.66 |
| 300 | 359,920 | 281,435 | 1.28 |
| 508 | 623,348 | 579,161 | **1.08** |

Past the fixed cost of the first few turns, context growth *is* output growth.
Output ran 1,100–1,800 tokens per turn across **every** session measured, which
puts the wall near 500–900 turns whatever the turns are about. **The five
sessions that reached it had 400–600 turns. The ones that finished had 10–25.**

### It is not the disassembly, and this was checked

The intuitive explanation — the function does not fit — is wrong:

- `probeselect`, 6,173 bytes, is 1,523 instruction lines and about **14,500
  tokens** for one clean read.
- `v34handshak`, the largest function in the object at 61,541 bytes, is about
  **131,000 tokens**.

Both fit a 1 M window with room to spare. Nor is it tool output: across one
1,826-turn session, every disassembly tool together — the disassembler,
`objdump`, the CFG splitter, the relocation scanner, `readelf`, the call-graph
tool — produced **153 KB, 12% of all tool results** (finding F220).

### One remedy changes the exponent; the rest are constant factors

**Delegate.** A subagent's turns do not accumulate in the parent's window. You
pay for the prompt and the final report, not the two hundred round trips
between. This is the only structural fix (finding F220). Delegate anything that
can run the phase gate and see its own work pass.

Everything below is a constant factor, and constant factors on a 500-turn
budget are still worth having.

- **Batch investigation into one script.** One session made **760 Bash calls
  averaging 1.4 KB of output** (finding F220). Ten greps whose answers are
  needed together is ten turns, ten tool results and ten lots of reasoning;
  one script that prints all ten is one. At ~1,400 output tokens a turn, 100
  turns saved is 140 K of window.
- **Make the harness say which field diverged.** 819 of 1,667 checks computed
  an offset and threw it away at the point of printing, so a failure said
  `got 68, reference 136` without saying which byte — and the session earned
  that back by re-reading the disassembly, which is the loop that consumes the
  context. The repair was five lines and touched no call site: append the
  input when the format string has no conversion for it (finding F220).
- **Compare whole objects with one call, not a byte loop.** A helper that
  coalesces differing bytes into runs reports one wrong 32-bit accumulator as
  one line rather than four, counts one check per object rather than 1,948,
  and reports the first difference first because everything after it is
  consequence (finding F220).
- **Checkpoint as you go.** The limit is per session, so what it costs you is
  the cost of *resuming*. Field offsets settled, cases done, what was tried and
  failed — into the task or a doc, while you work.
- **Do not re-read a file you just edited.** The edit tool fails loudly if it
  did not apply.

---

## Part 2 — wall-clock

### Mutation suites are embarrassingly parallel

A mutation is: write the mutant over the source, build, run, put the source
back. That is genuinely serial **in one tree** — two workers would be writing
the same file, which is finding F349's accident performed on purpose. But
mutations are independent of *each other*, so the answer is **a tree per
worker, not a lock** (finding F541).

The numbers that make it easy:

```
  source tree without .git and build*        7.8 MB
  build directory for one test binary        7.5 MB
  rebuild + relink of one translation unit   0.67 s
  run of one test binary                     0.73 s

  209 mutations   serial     287.22 s
                  --jobs 8    61.69 s     4.7x
```

Eight workers cost about 120 MB of temporary disk, created and removed per
run, on a twelve-core machine that had been idle for 91% of a serial run.

Three things that are not obvious:

- **The shard runs the ordinary serial path.** What counts as caught,
  unusable, equivalent, or recorded-equivalent-and-caught-anyway is not
  reimplemented; a shard hands its four lists back as JSON and the parent
  merges them through the same reporting function. This is the tier that
  decides whether a claim is tested at all, and a second copy of "what counts
  as caught" is precisely the kind of thing that drifts (finding F541).
- **Workers must be siblings of the real tree, not under `/tmp`.** A relative
  symlink into a peer directory only resolves at the same depth; a worker
  under `/tmp` builds everything and then dies naming it (finding F541).
- **A dying shard must abort the run.** Losing one silently would drop an
  eighth of the suite and still print a confident total. Injected a dying
  shard → `SHARD 1 DIED -- its mutations were NOT run` and exit 2; healthy run
  → exit 0. Verdicts checked identical to a serial run **per mutation**, not
  only on the totals (finding F541).

### A worker has to earn its setup

`--all --jobs 8` is what anyone reaches for, and **thirty of the forty-eight
suites have fewer than twenty mutations**. Copying the tree and doing one cold
build is nothing on a big suite and is the entire run on a small one. Measured
over three suites of 6, 9 and 8 mutations:

```
  before the threshold:   serial 11.2 s    --jobs 4   17.9 s    60% SLOWER
  after it:               serial 10.9 s    --jobs 8   11.7 s
```

A threshold fixed it — a worker gets at least 12 mutations or it is not
started, and one worker means the serial path. The big suite still shards
eight ways with its verdicts unchanged (finding F541).

> **Two runs, one table.** This document first printed `--jobs 4  17.9 s`
> against `serial 10.9 s` and called it 64%, which is how finding F541 first
> recorded it — but the 10.9 s is from the run *after* the threshold went in,
> whose parallel figure is 11.7 s. The finding was corrected and this had
> already been distilled from the uncorrected text. **A document distilled
> from a record inherits that record's errors and does not inherit its
> corrections**, so a set like this needs re-reading against the record
> whenever a cited finding is amended, not only when new ones land.

**The general point: measure the flag that looks obviously good.** Nobody would
have put a stopwatch on "parallel is faster".

### The third axis: parallelism was attacking the wrong question

Both sections above are about making a 749-mutation answer arrive sooner.
**The batch did not want a 749-mutation answer.** `mutate.py` offered
`--suite` (all of one) and `--all` (all of everything) and nothing between
them, so a batch iterating on a handful of labels re-ran the whole suite each
time it wanted to know whether its own change still failed (finding F555):

```
  v34hshak, full suite, --jobs 8      62 s
  v34hshak, --only "<one label>"    4.17 s     (baseline build + the mutant)
  v34hstx1, full suite, --jobs 4    ~180 s
  v34hstx1, --only "<one label>"      ~4 s
```

One batch working on `v34hstx1` ran its 749 mutations at 16:45, again at
16:51, and `mutsnap --update` ran them a third time at 16:55–16:59 — roughly
fourteen minutes of a task that had touched a handful of labels. Eight workers
make that full answer arrive in 93 s instead of fifteen minutes, which is
worth having; four lines making the *question* smaller made it 4 s
(finding F555).

> **Measure what is being asked for, not only what is slow.** Parallelism is
> the answer to "this measurement takes too long". It is not the answer to
> "this is not the measurement I want".

The per-mutation cost is arithmetic — 0.67 s to rebuild and relink plus 0.73 s
to run — so a full suite is unavoidable once you ask for it. The defect was
having no way not to ask for it. What a subset run must *not* be allowed to
do is masquerade as a full one; that half is `gates.md` rule 2.

### Half the waste was not parallelism — it was re-measuring

The suites were about half the wall-clock of a batch, so parallelism took
maybe a quarter off. The other half is that **every batch ran a baseline the
previous batch had already measured**, because nothing in the tree recorded the
current per-suite numbers (finding F541).

Writing them into prose does not work and this tree proved it over a year: the
numbers in `docs/findings.md` are snapshots from whichever batch created each
suite, and one suite recorded at 77 mutations had 215. **A number nobody can
tell is stale is worse than no number, because it gets quoted**
(`tools/mutsnap.py`).

The fix is a **keyed** snapshot: record the verdicts together with a hash of
everything that can reach the test binary. If the key matches, the recorded
verdicts are valid by construction and the batch may use them as its baseline.
If it does not, the entry says so. **Staleness becomes detectable without
re-measuring**, which is the entire point — checking it by re-measuring is the
cost being avoided (finding F545). All 48 suites: **2,874 mutations, 12 minutes
at `--jobs 8`**, against roughly an hour serially. The `--check` itself costs
**0.03 s**.

**Key on the whole link closure.** A first design keyed each suite on its own
source plus its own mutation JSON. That is unsound here: the build's object
list is `find src -name '*.c'`, so **every test binary links all of `src/`**,
and editing one file can change another suite's verdicts while both of those
hashes still match. A per-file key would have been a precise lie. The key
covers the Makefile, `src/`, `include/`, the harness, the suite's own driver,
its mutations, and the mutation runner — because what counts as caught is the
runner's code (finding F545).

**And when the key invalidates everything, re-run — do not re-key.** Wiring the
check into the Makefile invalidated all 48 entries, because the Makefile is in
the closure. It was re-run: twelve more minutes. Re-keying "because the edit
obviously cannot change a verdict" is exactly the reasoning that produces a
false baseline with a hash vouching for it, and the coarse key is doing its job
when it refuses to let you make that argument (finding F545).

**That bill arrives again and again, and it is still the right trade.** A
field-map batch edited `include/`, which is in the closure, so all 48 entries
went stale for a header change that moved no `.text` byte at all: re-run
rather than re-keyed, 2,874 mutations in **15m 40s at `--jobs 4`** on a
machine shared three ways, and **not one verdict moved** (finding F638). Adding
one flag to the mutation runner invalidated all 48 again, because the runner
is what decides what "caught" means (finding F555). Both are the key doing its
job. Budget for a full re-run at every merge rather than for a cleverer key.
The re-runs that appear to buy nothing are what make the one that catches
something believable: the same comparison, on a different day, found a
mutation that had gone CAUGHT → UNUSABLE while the phase gate stayed green
(finding F637, and `tiers.md` §2).
