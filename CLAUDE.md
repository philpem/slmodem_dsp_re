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

## One type, one home

A `class`, `struct`, `enum` or `union` is **defined in exactly one file**.
Everyone else forward-declares it or includes that file. A forward declaration
is not a definition and is never a problem.

`make phase` gates this through `tools/onedef.py`, which carries the two
duplicates this tree still has and the reason for each. Adding a third needs
a reason written there; removing one is progress.

It is not a style rule. Two definitions of one type is undefined behaviour the
moment both reach a translation unit, and it fails silently -- the compiler
picks one, and every offset, `sizeof` and allocation in the other half is
quietly wrong. `V90Parameters` is 0x504 in one header and 0x558 in the other,
and `V90ModemCtor.cpp` carries a long comment about which of the two it must
not include, because allocating the smaller and using the larger
under-allocates by 84 bytes and passes every test not run under a checking
allocator.

It was also a portability wall: six enums were spelled in two headers each,
which is legal for the C++11 OPAQUE DECLARATION they were and illegal for the
C++98 DEFINITION they had to become. See `docs/method/compilers.md`.

## The compiler that decides is the PERIOD one

`make period` builds `src/`, `test/harness/` and `test/unit/` with GCC 3.4.2
in `tools/toolchain/`, links them against the blob with binutils 2.15, and
runs the suite. Our source and the object, compiled by the same compiler,
compared at runtime -- so a difference is a difference in the code and not in
the toolchain.

**IT IS GCC 3.4.2 ITSELF SINCE FINDING 2200**, bootstrapped from the GNU
tarball by `tools/toolchain/Dockerfile.exact`, and until then it was Debian
sarge's `3.4.4` prerelease while every comment in the tree said 3.4.2. Build
the image once:

```
docker build --platform linux/386 -f tools/toolchain/Dockerfile.exact \
             -t dsplibs-tc342 tools/toolchain      # about a minute
```

The old 3.4.4 image is still built by `tools/toolchain/Dockerfile` and still
selectable -- `PERIOD_IMG=dsplibs-tc`, `TC_IMAGE=dsplibs-tc` -- because it is
the other arm of every A/B in 2200. Both are green at 183 passed / 0 failed.

**`make phase` RUNS IT**, so `make phase` needs docker and the
`tools/toolchain` image. It is incremental and sound -- an object is reused
only if it is newer than its source and than every header -- so an unchanged
tree relinks rather than rebuilding: about 34 s of the run. `make one T=...`
is still the fast loop between commits.

The modern build runs in the same `phase` and still has to pass. It is the
portability check, and `make check64` proves the tree is 64-bit clean. Where
GCC 13 provably cannot reproduce the object from correct source, the site is
declared in `tools/gccdiverge.json` -- five entries today, ten checks --
rather than papered over in `src/`. That register names CHECKS, not tests, and
a stale entry (an allow-listed test that starts passing) fails the gate.
**`make period` has no allow-list and is not getting one.**

Four of those five are one cause and were added together: the object's
equality tests are a single ordered `fcom` with no parity test, which GCC 13
will not emit at all -- `-mno-ieee-fp` is accepted by it and does nothing, and
`-ffinite-math-only` does the job by withdrawing NaN semantics from the whole
translation unit, which breaks eleven other sites that depend on them. So the
source is the object's, `make period` proves it, and the modern build
declares. Findings 2300 and 2304.

**A rejection in `src/` under GCC 3.4.2 is a finding, not a portability
nuisance** -- the author wrote this code for that compiler, so anything it
refuses is something the author cannot have written. A rejection in `test/` is
plumbing; fix it freely. And where our own APPARATUS needs something the old
compiler lacks, the shim goes in `tools/toolchain/period_compat.h`, outside
the reconstruction -- never in the source being reconstructed.

`docs/method/compilers.md` is the register of every variance found so far,
including the one that was silent: 78 files guard their offset assertions on
`__SIZEOF_POINTER__`, a GCC 4.6+ predefine, so under 3.4.2 the guard read
`#if 0` and every assertion vanished while the file compiled clean.

## The second tier: comparing code generation

`.comment` names the original's compiler 279 times over — **GCC 3.4.2**, built
22 September 2005 (finding 606). `tools/toolchain/` has a container with it,
and `make similarity` builds every translation unit with it and compares the
result against the blob, function by function.

This answers a question the differential tier cannot: not "does it behave the
same" but "did the same compiler, given our source, emit what the original's
compiler emitted". Currently **330 of 924 compared symbols match on their
instruction sequence** — mnemonics, not bytes; see the precision note below
before quoting that number. That figure was 92 of 365 when this paragraph was
written; it is measured at `c181797` on the exact compiler, and
`tools/toolchain/ratchet.json` is the stored baseline `compare.py --ratchet`
moves against.

**Six of those 330 are the compiler and not the code.** On sarge's 3.4.4 the
same tree matches 324, and the exact 3.4.2 gains six and loses none. So a
number quoted from this tool is only meaningful with the compiler beside it,
which is why `compare.py` now prints the blob's `.comment` and ours on every
run. Finding 2200, and 2201 for what the blob's Gentoo patch stack means:
stock 3.4.2 is the exact POINT RELEASE, never the exact compiler.

**THE GENTOO SOURCES ARE NOW RECOVERED** and that gap is closable. All six of
`gcc-3.4.2-r2`'s `SRC_URI` files verify byte-exact against Gentoo's digest,
with the real ebuild, its `toolchain.eclass` and all 96 in-tree patches, in
`tools/toolchain/gentoo-3.4.2-r2/`. The archives are not in git -- run
`tools/toolchain/gentoo-3.4.2-r2/fetch-distfiles.sh` to pull and verify them.
What proves it is the right recipe is not a checksum but the object's
**double space** after `3.4.2`: the eclass calls `gcc_version_patch` with an
empty `BRANCH_UPDATE`, so the argument carries a leading space and the sed
adds another. Nobody has rebuilt with it yet, so `-O3` (2155) and
`-mno-ieee-fp` (1990) still rest on stock 3.4.2 until re-measured. Finding
2320.

The flags were derived from the object, not guessed, and are in
`tools/toolchain/build.sh` with the evidence beside each:

    -O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
    -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
                                                        (no PIC, no SSP)

`-mtune=i686` is worth knowing about: `-march` and `-mtune` are separate
questions and only the first leaves a trace, so "no cmov in 1.2 MB" bounds the
instruction set and says nothing about scheduling. Finding it took the match
from 30 to 82 (finding 612). `-frename-registers` took it to 92 (616).

**The level is `-O3`, and 616's ruling against it is overturned** -- on a tree
three times the size, `-O2` matches 313 and `-O3` matches 324, and the `-O3`
set gains 15 while losing 4 rather than swapping. 616 measured at 92 of 365,
where `-finline-functions` had almost nothing to inline across. `make period`
is green at both, and there is no divergence to declare. Finding 2155.

`-mno-ieee-fp` is the newest and its worth is not in its +2 (302 -> 304): the object's float
compares are ordered, 406 `fcom`-family against four `fucom` that are all
inside libm's `pow`, and the default `-mieee-fp` emits `fucom` for every
comparison whatever the source says. Until it was set, **every float
comparison in every function read as a codegen mismatch** -- so a numeric
function's per-symbol diff was measuring our flags, not our source. Finding
1990.

**IT IS NOW IN `period_inner.sh` TOO, and the two flag sets no longer
diverge.** Setting it there used to cost five suites, and 1990 refused to
guess whether that meant five defects or a wrong flag. It was five defects, in
three shapes -- an `x < c || x > c` idiom written for the wrong flag (2300), a
compare whose operand order came from a DECLARATION ORDER (2301), and a loop
constant that was not hoisted (2302) -- plus one in the apparatus, where the
flag folded `x != x` to zero and deleted the harness's NaN detector (2303).
The cost is on the modern side and is declared above.

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

- **A `.c` calling a `.cpp` is about the LINK LINE, and the link line has been
  fixed.** This used to read "a `.c` may not call anything defined in a
  `.cpp`", because the interop binaries linked only `$(SRC)` -- every `.c`
  under `src/`, no C++ -- so such a call compiled, linked 32-bit, passed `make
  one`, and failed `make phase` at `t_spandsp_v23` with an undefined
  reference. They now also link `$(CXXOBJ64)`, a 64-bit build of the C++ half,
  and `src/pump/v34/v34hshak.c` already calls `V34SetINFO1aBits` and
  `V34SetINFO0aBits` across that boundary with `make phase` green. So the call
  is allowed; what is NOT allowed is adding a link target that omits
  `$(CXXOBJ64)`, which brings the whole failure straight back. `make test` and
  `make one` still cannot see any of this.

  **The link line is necessary and not sufficient.** The callee must also be
  `extern "C"` -- otherwise its name is mangled and the C side's reference
  matches nothing -- and a FREE FUNCTION, since a member takes a `this` and
  has no unmangled form to name. So a `.cpp` of plain functions
  (`v34hstx1.cpp`: `nm -g` shows nineteen `T v34tx1_*` and zero `_Z`) is
  callable, and a `.cpp` of real classes (`VPcmFloModem`, `K56FlexFloModem`)
  is not, whatever the Makefile does. Check with `nm -g`, not by reading.
  Findings 333 and 344 for why `v34hstx1.cpp` is a `.cpp` at all; 711 for the
  expiry and the three conditions.
- **A relocation on a call proves nothing about the translation unit; its
  ABSENCE does.** A resolved PC-relative displacement with no relocation means
  the target is `LOCAL` and in the same TU. A relocation being present only
  means the symbol is `GLOBAL`. Findings 306 and 333.
- `tools/dis.py` shadows the standard library's `dis`, which `inspect`
  imports. A tool in `tools/` that reaches for pyelftools dies with
  `AttributeError: module 'dis' has no attribute 'COMPILER_FLAG_NAMES'`,
  naming neither the directory nor the file. See the top of
  `tools/whichfield.py` for the fix.
- **objdump swaps FDIVP/FDIVRP and FSUBP/FSUBRP.** The `DE` pop encodings
  print as their own opposite: `de f1` reads `fdivp` and IS `FDIVRP`
  (`ST(1) = ST(0)/ST(1)`). The `D8` register forms are fine. For any popping
  divide or subtract, read the bytes, not the mnemonic — finding 245.

  **Upgrading objdump does not help and `tools/dis.py` now tells you anyway.**
  Binutils 2.15 in the container and 2.42 on the host print these identically
  in AT&T syntax; it is what the syntax means, not a bug being carried.
  `objdump -M intel` renders the same bytes the architecture's way, and
  `dis.py` runs that second pass for you and appends `<== Intel: fdivp` to any
  line where the two disagree — 4 lines of 763 on a real function, silent
  elsewhere. Finding 2156, which also records how the measurement nearly went
  wrong: comparing two objdumps under different `-M` settings made them look
  exactly opposite and almost retired 245.
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
- **`git checkout --ours <file>` replaces the WHOLE FILE**, not the conflicted
  hunk, so it silently discards every other change the merge had already
  applied cleanly to it. Nothing fails and the suite still passes. Use it only
  where one side's file is wanted entire (a generated artefact); otherwise edit
  the markers by hand, and afterwards grep for a distinctive string from each
  side's contribution. A merge that compiles is not a merge that kept
  everything -- finding 700.
- Other sessions work in sibling worktrees. Check `git worktree list` and
  `git status` before touching one, and never `git stash` in a tree you do
  not own.
