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

**IT IS A RULE ABOUT `src/`, AND `testbench/` IS NOT `src/`.** The harness is
measurement apparatus -- it places calls, records both ends, and analyses what
came back. There is no blob to be differentially identical to, so the rule
cannot apply to it and must not be read as forbidding a commit there. What
DOES apply is the discipline those tools were built under and which cost more
to learn: a detector must report its denominator, and a tool that prints
nothing is indistinguishable from a tool that is broken (findings 134, 2400,
2401). Show a new analysis firing on a known input before trusting a clean
run from it.

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

## Naming: fields, and flags

Four states, and they are not the same problem:

- `pad_NNNN` — **unmodelled space.** We do not know how many fields are in it.
- `type_NNNN` (`short_2800`, `flags_0217`, `ptr_49b4`) — **modelled, unnamed.**
  Shape and size known, meaning not.
- bare `fNNNN` — neither. An offset wearing a name.
- a real name — the goal.

**If we know what something indicates, name it. That includes FLAGS.** A bare
`x & 0x40` states a bit position and hides a meaning, exactly as `f25d0` does.
Give it a named constant.

**Name by BIT VALUE and keep 1:1 with the object** — `#define FOO_TRAINED
(1 << 6)` or `0x40`, then `x & FOO_TRAINED`. A macro or enum constant is a
compile-time substitution and **cannot** move code generation, so this is free
and `compare.py` must not budge. If it does, something other than a name
changed.

**Bitfields are NOT free, and whether the original used them is MEASURABLE
rather than a preference.** A bitfield read compiles to a shift and a mask; an
explicit mask test compiles to `and`/`test` against an immediate. The blob is
dominated by the second — 1233 `and $imm`, 474 `test $imm`, 161 `andb`, 150
`testb` — so a bitfield rewrite would move the codegen tier AWAY from the
object at the sites it touched. Do not convert to bitfields to make a struct
read nicely; if a bitfield is ever right, it is because the object's own
instructions at that site say the author used one. Settle it per site, from
`dis.py`, like everything else.

A mask that is not a single bit is a different thing again: `0x0f` over a
four-bit field wants a named width and shift, not a flag name. There are 147
single-bit uses and 235 multi-bit ones, so check which you have before naming.

**Evidence order, strongest first**, and it matters more than completeness:

1. **A format string that prints the thing.** `.rodata` labels are the original
   author's own words. `tools/relocscan.py --at .rodata.str1.1:0xNNNN` finds
   who references one (finding 604). This is not the Ghidra prohibition — that
   rule forbids names from DECOMPILER OUTPUT, not from the binary's own text.
2. **A callee or caller that types it.** A mangled C++ name carries argument
   types; a field passed to `Scrambler<h,h>::process` has that element type
   because the mangling says so, not because it looked right.
3. **Usage inference.** Weakest. Say so in the finding when it is all you have.

**Naming something wrongly is worse than leaving it padded**, because a wrong
name is believed by every future reader, and no test can fail on it. Where the
role can be bounded but not established, keep a neutral name and put the
derivation in the comment — 3120 declined `+0x2f64` on exactly this ground and
that was the right call.

## One type, one home

A `class`, `struct`, `enum` or `union` is **defined in exactly one file**.
Everyone else forward-declares it or includes that file. A forward declaration
is not a definition and is never a problem.

`make phase` gates this through `tools/onedef.py`, which carries the duplicates
this tree still has and the reason for each. **It is ONE today —
`V90Phase4Demodulator`** — and the tool prints the count, so read it there
rather than from this paragraph. Adding one needs a reason written there;
removing one is progress.

It is not a style rule. Two definitions of one type is undefined behaviour the
moment both reach a translation unit, and it fails silently -- the compiler
picks one, and every offset, `sizeof` and allocation in the other half is
quietly wrong.

**The worked example is `V90Parameters`, and it is now FIXED — read it as
history, not as a live hazard.** It WAS 0x504 in one header and 0x558 in the
other, and `V90ModemCtor.cpp` carried a long comment about which of the two it
must not include, because allocating the smaller and using the larger
under-allocates by 84 bytes and passes every test not run under a checking
allocator. It has had one home since task #116; `include/dsplib/V90Parameters.h`
is that home, `sizeof` is 0x558, and the nineteen other headers naming the class
only forward-declare it. Keep the example — the failure mode is exactly as
described and cost real time — but do not send anyone hunting for it.

That correction came from an agent briefed to expect the trap, which measured
the tree instead of believing the brief (finding 6402). **A rules file has the
same shelf-life problem as a comment and no gate behind it**: findings 6100 and
6103 are the same defect in a source comment and in this file's own gccdiverge
tally. When a paragraph here states a COUNT or a live defect, check it against
the tool before repeating it.

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
declared in `tools/gccdiverge.json` -- seven entries today, twelve checks --
rather than papered over in `src/`. That register names CHECKS, not tests, and
a stale entry (an allow-listed test that starts passing) fails the gate.
**`make period` has no allow-list and is not getting one.**

**AN ENTRY COSTS ITS BINARY'S WHOLE MUTATION SURFACE, so the divergent check
goes in a binary of its own.** `tools/mutate.py` judges a mutant caught by a
non-zero exit, and a declared binary exits non-zero on the UNMUTATED source --
so it cannot score a mutation set against that baseline and it refuses, for
the SUITE and not the row. That silently removed nine suites and 647 verdicts
before anyone counted them (findings 2157 and 3002). Three binaries now carry
one declared check each for this reason -- `t_v90p4dnan`, `t_v92ecnan`,
`t_v90adidnan` -- and **none of them has a mutation suite**, because a
registered suite that can never be recorded reads MISSING to
`mutsnap.py --check` and fails the gate. `t_v92ecparams` is the same move the
other way round: three GREEN members lifted out of a declared parent, so it
keeps its suite. Split the VALUE where you can rather than the group -- the
blob treats a NaN and 177.0f as one input, so `t_v90leaves` lost one check of
4,570 and kept every shape. Findings 6000, 6001 and 6002.

`t_v90equproc` is declared without being a split -- it is `V90Equalizer::
process`'s own binary and its divergence is the whole test's, not one check
lifted out of a healthy group -- and the no-suite rule binds it just the same.
**Do not register a mutation suite for it.**

**The seven are two causes, and only two.** Five of them are the object's
equality tests: a single ordered `fcom` with no parity test, which GCC 13
will not emit at all -- `-mno-ieee-fp` is accepted by it and does nothing, and
`-ffinite-math-only` does the job by withdrawing NaN semantics from the whole
translation unit, which breaks eleven other sites that depend on them. So the
source is the object's, `make period` proves it, and the modern build
declares. `t_agc`, `t_v90equ`, `t_v92ecnan`, `t_v90adidnan` and `t_v90p4dnan`.
Findings 2300 and 2304.

The other two are **x87 excess precision**, where the object narrows an
intermediate the modern compiler keeps at 80 bits: `t_psd` in the FFT
butterflies reaching a decibel (1453), and `t_v90equproc` on the one
subtraction inside `V90Equalizer::process` whose difference feeds the squared
error, the DFE step and the high-error test (6203). Neither is closable by
choosing a type -- 6203 measured all three candidates, and the `float` the
author wrote is the only one that is exactly green on the period compiler.
`-fexcess-precision=standard` would close both and is a translation-unit-wide
change to flags this tree derived from the object.

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

**AND THE GENTOO COMPILER ITSELF IS NOW BUILT.** `dsplibs-tc342-gentoo` is
`sys-devel/gcc-3.4.2-r2` built from Gentoo's own ebuild inside Gentoo's own
stage3-x86-2005.0 -- glibc 2.3.4, binutils 2.15.92.0.2-r1 -- and it prints
the blob's `.comment` back byte for byte, double space and all:

```
tools/toolchain/build-gentoo-image.sh              # about a minute
TC_IMAGE=dsplibs-tc342-gentoo tools/toolchain/build.sh
PERIOD_IMG=dsplibs-tc342-gentoo make period
```

It needs the stage3 and the six `SRC_URI` tarballs, neither in git;
`tools/toolchain/gentoo-3.4.2-r2/fetch-distfiles.sh` pulls and verifies the
latter. **What it changes is nothing**: 182 of 183 objects come out
byte-identical to stock 3.4.2's, the symbol match is 334 either way with none
gained and none lost, and the single difference is a schedule permutation in
`DTMF_MTD_detect` that flips no symbol. So 2201's residual is now measured
rather than bounded, and the default stays `dsplibs-tc342` -- which anyone can
build from the network alone. `-O3` (2155) and `-mno-ieee-fp` (1990) were
re-measured on the real compiler and both survive symbol for symbol. Findings
2320, 2500 and 2501.

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

- `byteident.py` (`make byteident`) — **grade 0 and grade 1, and the only
  tool that measures either.** Grade 0 is positional byte identity: the
  same bytes in the same places, which is the first question anybody
  actually asks. Grade 1 is the same instructions and operands under one
  consistent register bijection. **393 of 1,200 at grade 0 and 416 at
  grade 0-or-1 today**, against `compare.py`'s 480 on the same tree — the
  144 in between have identical mnemonics and different operands.
  Relocated fields are compared by TARGET and branch targets are made
  function-relative; without both, a text comparison reports its own
  artefacts rather than the code's.
- `compare.py` — the per-symbol comparison and the ratchet. It compares
  MNEMONICS, not bytes, and drops OPERANDS, so it is an upper bound on
  grade 1 rather than a measurement of it: two functions storing the same constants to different
  offsets both read as `mov mov mov`. The total-bytes percentage is the weak
  number and moves when we emit more code, not only more of the right code.
- `compare.py --ratchet` — fails only on a DECREASE, and is deliberately not
  in `make phase`. 100% is not the target: different factoring differs for
  ever while behaving identically.
- `extcheck.py` — the signedness detector, and a triage aid, never a gate. It
  pairs `movswl` against `movzwl` on the same field and reports only where the
  32-bit result is USED, which is the forced case above. **18 candidates over
  938 symbols, and one report in five is real** — 14 traced, 3 true, 4 left
  unverified and named (finding 2402). Every hit must be traced against
  `dis.py` before anything is
  retyped; the twelve failures are a 16-bit compare, a signed branch on a
  16-bit test, a sum truncated by a cast, and a value masked to two bits, and
  no lookahead rule separates those from the real thing. 619 ruled that needs
  real dataflow and the ruling stands.
- `samesize.py` — the SAME SIZE, DIFFERENT INSTRUCTIONS bucket, which
  `compare.py` counts and does not print. `--all` dumps every aligned diff in
  one pass; `--identical` prints the identical SET, because a count can gain
  four and lose four and not move. The sharp slice: same byte count means
  nothing is missing and nothing is extra. **The free column is narrower here
  than the general rule** — `compare.py` drops operands, so pure register
  allocation already scores as identical and cannot reach this list; what is
  still free in it is scheduling, 614's discarded upper half, and 2411's
  integer if-conversion. 69 rows classified in finding 2900, of which three
  were real (2901), three were 614 and declined (2902), and ten are forced
  and named for the next pass (2903).
- `storeorder.py` — store order, and a HINT, not a defect list. 617's
  acceptance test is full-text identity, operands included: nineteen examined,
  two passed. It reports 57 differing functions and, of those, **the 14 whose
  mnemonics already match** — the only ones that test can ever pass. Read a run
  as "14 worth a look, 43 to leave alone". It also prints what its regex cannot
  see, which is any store at offset 0 or through `%esi`/`%edi`/`%ebp`.

**A number from either is meaningless without the compiler beside it**, exactly
as for `compare.py`. Both figures above are GCC 3.4.2 exact at `-O3`. The
inherited "one true positive in four" was measured on sarge's 3.4.4 at `-O2`
over 365 symbols and did not survive re-measurement (2402).

**Any tool here must be shown to fire.** `extcheck` printed "(none)" through
four broken versions and there was no way to tell a clean tree from a dead
detector; both aids are now validated by reintroducing a known defect and
watching it appear, then restoring and watching it go. Finding 134's argument.

**And it happened again, to both of them at once.** They kept defaulting to
`TC_OUT=/tmp/tc_out` after `build.sh` moved its output to `build/tc_out`, so
with no environment set they compared **zero** symbols and reported a clean
tree, exit 0 (finding 2400). Both now refuse to run on an empty `TC_OUT` and
print the number of symbols compared on every line that carries a verdict. **A
detector must report its denominator** — re-running the injection ritual on the
current toolchain is what found that, and two more bugs under it (2401).

**AND THEN TO `make phase` ITSELF, WHICH IS NOT A TRIAGE AID BUT THE GATE.**
Task #164, "build: split the object tree so the DEFAULT build carries our
fixes", moved the differential tier's objects from
`build-cov/src/` to `build-cov/repro/`; `tools/debugcov.py` went on reading the
old path, found no `.gcda` anywhere, and printed `suite line coverage over
src/ 0.0% (0/0)` and `0 of 0` deviation sites — which the phase boundary
aggregated into "differential, 64-bit, interop, coverage and debug sites all
OK", exit 0. Two of five tiers had measured nothing and the gate could not
tell. It now probes both layouts, **exits non-zero on a zero denominator**, and
prints the count on every line carrying a verdict; `make phase`'s closing line
quotes those denominators and refuses to be printed without them. Finding 3100,
and it is the same defect as 2400 with the gate rather than an aid behind it.

**AND A THIRD TIME, TO SEVEN TOOLS AT ONCE — SAME COMMIT, OTHER HALF OF THE
TREE.** #164 also stopped a plain `make` filling `build/src`; those objects now
go to `build/repro`. Seven tools learn what we have WRITTEN by globbing that
directory -- `closure.py`, `readyqueue.py`, `worklist.py`, `coverage.py`,
`cppstruct.py`, `callgraph.py` and `service.py` -- and they went on reading the
empty one. `coverage.py` printed `translated 0.0%, 0 bytes, 0 symbols` at exit
0: the headline number of the whole project reading zero. `service.py` reported
913 unwritten data-mode symbols against a true 267 **while its own
`MUST_BE_FAX`/`MUST_BE_DATA` self-check passed** -- that check tests
reachability in the BLOB, and reachability does not care whether anything is
written. **A self-test that cannot fail is the dead detector in its purest
form**, and it is why this one survived unnoticed the longest. Two of the seven
also advised "Run `make` first", which by then was the advice that CAUSED the
fault; `make coverage` is what fills the tree. All seven now go through
`tools/objtree.py`, which refuses on an empty tree, prints the directory it read
and the object count on stderr, and warns without refusing when the tree is
PARTIAL (fewer objects than sources -- silently 52.2% against a true 54.8%) or
STALE (a source newer than every object). The probe order is licensed by
measurement, not assumption: both object trees define the same 1457
`(name, kind)` pairs. Findings 3055, 3110 and 3111.

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
- **A bare `cd` in a compound command FAILS OPEN: everything after it runs in
  the tree you were already in.** Use `git -C <dir>` in preference, and where a
  `cd` is unavoidable write `cd <dir> || exit 1`. A worktree that had been
  auto-removed made `cd .claude/worktrees/fix164` fail; the `git rebase master`
  on the next line therefore ran in the MAIN tree, against
  `improve/v34-training` — a branch another session was committing to — and
  stopped on a `findings.md` conflict rather than at the `cd`. Aborted, and the
  branch, the working tree and that session's three commits all verified
  intact, but the window in which a concurrent write would have been lost was
  real. The `git status` check in the bullet above cannot save you here,
  because by then you are checking the wrong tree.
- **A fresh worktree is missing TWO paths outside itself, and both are now
  found for you.** Agent worktrees live under `.claude/worktrees/`, so nothing
  relative to `..` resolves.
  - `third_party/spandsp` is gitignored, so `git worktree add` does not bring
    it and the interop tier cannot link. That failed at the top of a 1,573-line
    log everybody read the tail of, so `make phase` gained a `prereq` target
    that runs FIRST, refuses if the library is absent, and symlinks the main
    tree's copy when it can find one (finding 1563).
  - `BLOB ?= ref/slmodemd/dsplibs.o` pointed at `.claude/worktrees/slmodemd` and
    every run died at `No rule to make target`. Loud, so not the same class of
    bug, but it blocked every worktree run until someone passed `BLOB=/abs/…`.
    The default is now resolved through `git rev-parse --git-common-dir` —
    the main repository's `.git` seen from inside any worktree, the same trick
    `prereq` uses. `make -s print-BLOB` says what it resolved to, and an
    explicit `BLOB=` still wins.

  What neither of them was is a reason to distrust a worktree's gate. That was
  finding 3100, and it was a branch difference and not a worktree one: the
  coverage tiers had stopped measuring on `master` and would have measured
  nothing in the main tree too, the moment it checked `master` out.
