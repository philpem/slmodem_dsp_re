# Working in this tree

A source reconstruction of `slmodemd/dsplibs.o`. Read `README.md` for what the
project is and `docs/fastpass.md` for the method. This file is the part that
is about *how to work here* rather than what the work is.

## Out of scope, absolutely

**Never read, search or reference the `re/` directory.** It is work in
progress belonging to a different effort. To you it does not exist. Scope any
subagent prompt to exclude it.

## The rule that is not relaxed

**Nothing is committed that has not passed a differential test**, and nothing
is committed that is wrong-but-plausible. If a function cannot be made to
pass, leave it out and record the attempt. The goal is a replacement that
behaves *identically* to the blob, so any test disagreeing with the blob is a
hard failure whatever build it came from — never a tolerance to widen.

Run `make phase`, not `make test`.

## Budget your turns, not your reading

Finding 216 measured this, so it is not a guess. **Context growth is
cumulative output, converging to 1:1** — every token you generate stays in the
window for the rest of the session. Output runs 1,100–1,800 tokens per turn,
which puts the wall near 500–900 turns *whatever the turns are about*.
Sessions that finished here took 10–25 turns; the five that ran out took
400–600.

The disassembly is not the problem. One clean read of `v34handshak`, the
largest function in the object, is ~131 K tokens against a 1 M window, and
every disassembly tool combined made 12% of one long session's tool output.

So:

- **Delegate a large function to a subagent.** Its turns do not accumulate in
  your window — you pay for the prompt and the final report, not the two
  hundred round trips between. This is the only change that alters the
  exponent. Delegate whatever can run `make phase` and see its own work pass.
- **Batch investigation.** One session made 760 Bash calls averaging 1.4 KB of
  output. Ten greps whose answers are needed together is ten turns and ten
  lots of reasoning; one script that prints all ten is one. At ~1,400 output
  tokens a turn, 100 turns saved is 140 K of window.
- **Checkpoint as you go.** The limit is per session, so what it costs you is
  the cost of *resuming*. Write field offsets settled, cases done, and what
  was tried and failed into the task or a doc while you work.
- **Do not re-read a file you just edited.** Edit fails loudly if it did not
  apply.

## Comparing objects

Use `diff_eq_obj`, not an open-coded byte loop and not a bare `memcmp`:

```c
diff_eq_obj("after process", struct v34_receiver, &a, &b, sample);
```

It coalesces differing bytes into runs, so one wrong 32-bit accumulator is one
report rather than four flooding the ten-line cap; it counts one check per
object rather than one per byte; and it reports the first difference first,
because everything after it is consequence. Then:

```
$ tools/whichfield.py struct v34_receiver 680
struct v34_receiver + 680  ->  pad_2a8[0]     (unsigned char, +680)
```

An offset landing in a `pad_*` region is itself the answer: that part is not
modelled as fields yet.

A loop is still right where some region must be skipped — two heap pointers
hold two different addresses and always will. Those keep working: `diff_eq_int`
now appends the input when the format string has no conversion for it, which
repaired 819 call sites that were silently discarding the offset they computed
(finding 216).

## Ghidra is scaffolding, never evidence

`tools/decompile.sh v34handshak` gives a decompilation to read control flow
and constants out of. Measured against a function this tree had already
verified by hand, it recovers branch structure, every constant and structure
offset, and the calling convention — and **destroys arrays**, which are most
of this object (`unsigned char allow[6]` comes back as three unrelated locals
written through `._2_1_` casts).

Three rules, and they are not negotiable:

- Every line goes through `tools/dis.py` before it goes into `src/`.
- **No name, comment or finding is ever written from decompiler output.** The
  record is the deliverable, and a Ghidra guess recorded as a derivation
  corrupts it.
- The differential test remains the only thing that decides. That is also why
  "the decompilation might not match the assembly" is not a risk here: it
  never ships and is never trusted, so a mismatch is just a failing test.

Treat floating-point output as suspect — the object is `-mfpmath=387` and
Ghidra's x87 modelling is weak. Nobody has measured it here.

## Findings and numbering

`docs/findings.md` is the record. Append; do not renumber history.

Numbers have collided three times across parallel sessions. Run
`python3 tools/refcheck.py` — it catches duplicates and dangling references.
It **cannot** catch a reference that still resolves but now points at the
wrong finding, so if you ever renumber, say in the finding what it used to be
called (212 and 213 are the worked example).

Task numbers are not safe across sessions either: two task stores exist whose
`#11`–`#22` are different work. `docs/fastpass.md` holds the mapping.

## Traps

- `tools/dis.py` shadows the standard library's `dis`, which `inspect`
  imports. A tool in `tools/` that reaches for pyelftools dies with
  `AttributeError: module 'dis' has no attribute 'COMPILER_FLAG_NAMES'`,
  naming neither the directory nor the file. See the top of
  `tools/whichfield.py` for the fix.
- Use `tools/dis.py`, not raw `objdump`, for anything that might touch a
  table: objdump prints relocations on their own lines and every convenient
  way of trimming its output drops them, turning a table of pointers into a
  table of plausible small integers. That mistake has been made three times.
- Other sessions work in sibling worktrees. Check `git worktree list` and
  `git status` before touching one, and never `git stash` in a tree you do
  not own.
