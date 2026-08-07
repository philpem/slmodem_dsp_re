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

Finding 220 measured this, so it is not a guess. **Context growth is
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
(finding 220).

## The second tier: comparing code generation

`.comment` names the original's compiler 279 times over — **GCC 3.4.2**, built
22 September 2005 (finding 606). `tools/toolchain/` has a container with it,
and `make similarity` builds every translation unit with it and compares the
result against the blob, function by function.

This answers a question the differential tier cannot: not "does it behave the
same" but "did the same compiler, given our source, emit what the original's
compiler emitted". Currently **92 of 365 shared symbols match on their
instruction sequence** — mnemonics, not bytes; see the precision note below
before quoting that number.

The flags were derived from the object, not guessed, and are in
`tools/toolchain/build.sh` with the evidence beside each:

    -O2 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
    -fomit-frame-pointer -maccumulate-outgoing-args      (no PIC, no SSP)

`-mtune=i686` is worth knowing about: `-march` and `-mtune` are separate
questions and only the first leaves a trace, so "no cmov in 1.2 MB" bounds the
instruction set and says nothing about scheduling. Finding it took the match
from 30 to 82 (finding 612). `-frename-registers` took it to 92 and settled
`-O2` against `-O3` (616).

### The rule for reading a codegen difference

**Act on what the compiler was FORCED to encode. Ignore what it was free to
choose.** Every mistake in this area — and there were five in one session —
came from getting that backwards.

- **Forced, so act on it:** the signedness of a load whose 32-bit result is
  used. `movzwl` where we emit `movswl` on a value that indexes a table means
  the declared type differs. That found a real defect no test could see
  (613), because the two readings agree over every value the field holds.
- **Free, so ignore it:** register allocation. Instruction scheduling. The
  extension on a load whose upper half is discarded — a 16-bit field copy can
  use either instruction (614). Chasing these means permuting source until the
  output matches, which is fitting the compiler, not recovering the source.
- **In between, and it needs the strong test:** statement order. GCC does NOT
  simply preserve it — `toneiir_reset`'s source is already in the object's
  order and the compiler reorders ours (617). A store-order difference is a
  hint; the acceptance test is FULL-TEXT identity, operands included. Two
  functions passed it, seventeen did not and were left alone.

### The tools, and their precision

- `compare.py` — the per-symbol comparison and the ratchet. It compares
  MNEMONICS, not bytes: two functions storing the same constants to different
  offsets both read as `mov mov mov`. The total-bytes percentage is the weak
  number and moves when we emit more code, not only more of the right code.
- `compare.py --ratchet` — fails only on a DECREASE, and is deliberately not
  in `make phase`. 100% is not the target: different factoring differs for
  ever while behaving identically.
- `storeorder.py`, `extcheck.py` — triage aids, not gates. `extcheck` runs
  about one true positive in four (619) and every hit must be traced by hand.

**Any tool here must be shown to fire.** `extcheck` printed "(none)" through
four broken versions and there was no way to tell a clean tree from a dead
detector; it is now validated by reintroducing a known defect and watching it
appear. Finding 134's argument, and it caught two tools this session.

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

**The V.90 session holds 247–331.** This tree's recent work is 340 onwards;
leave a gap and check every branch, not just `master`, before claiming a block.

Numbers have collided eight times across parallel sessions. Run
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
- **objdump swaps FDIVP/FDIVRP and FSUBP/FSUBRP.** The `DE` pop encodings
  print as their own opposite: `de f1` reads `fdivp` and IS `FDIVRP`
  (`ST(1) = ST(0)/ST(1)`). The `D8` register forms are fine. For any popping
  divide or subtract, read the bytes, not the mnemonic — finding 245.
- A string reference is an `R_386_32` against the SECTION symbol with the
  offset as an inline addend, so searching the disassembly for a string's
  address finds nothing and proves nothing. `tools/relocscan.py --at
  .rodata.str1.1:0xNNNN` is what answers "who references this string" —
  finding 604, where not knowing that had a defect misdiagnosed for weeks.
- A per-FUNCTION count across an inlining boundary measures our factoring, not
  our completeness: where we split one of the original's functions into static
  helpers, the helpers have no blob symbol and their bytes count against
  neither side. Read `debugaudit.py --missing`'s per-file rollup first
  (finding 605), and `compare.py`'s per-object one (610).
- Use `tools/dis.py`, not raw `objdump`, for anything that might touch a
  table: objdump prints relocations on their own lines and every convenient
  way of trimming its output drops them, turning a table of pointers into a
  table of plausible small integers. That mistake has been made three times.
- Other sessions work in sibling worktrees. Check `git worktree list` and
  `git status` before touching one, and never `git stash` in a tree you do
  not own.
