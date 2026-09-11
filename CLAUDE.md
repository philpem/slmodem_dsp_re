# Working in this tree

A source reconstruction of `slmodemd/dsplibs.o`. Read `README.md` for what the
project is and `docs/fastpass.md` for the method. This file is the part that
is about *how to work here* rather than what the work is.

## Out of scope, absolutely

**Never read, search or reference the `re/` directory.** It is work in
progress belonging to a different effort. To you it does not exist. Scope any
subagent prompt to exclude it.

## Scope: which services are open, and the one that is not

This was a conversational rule for a long time and had no home in the tree,
which is why it was hand-copied into every agent brief and drifted. It lives
here now. `tools/service.py` is the authority on which service reaches a
symbol; this is the authority on which services we work.

| service | status |
|---|---|
| V.90, V.92, V.34, **V.32**, V.22, V.23, B.103, V.8 | **OPEN** -- the data modes |
| Ring detect, Caller ID | **OPEN** -- small, and worth clearing |
| Voice | **OPEN**, lowest priority |
| **FAX** (`class1*.c`, `src/pump/v17/`) | **LAST**, deliberately |

**V.32 was fenced for most of the project and is not any more.** It is a DATA
MODE -- `service.py`'s own classifier lists it as one -- and it carries
**25,925 bytes firm and 30,306 at the ceiling, 59-68% of the unwritten data-mode
remainder**, plus 17 already-written symbols sitting in SIZE.

**DO NOT READ A SPAN NAME AS A MODULE NAME.** The first figure written here was
32,921 and it was wrong in both directions at once (F8160): it swept in 12,154
bytes of V.22 that merely sit in a span *named* `V32mod.c`, and it omitted the
9,539 bytes of V.32 that sit in a span named `Dialer.c`. The span labels come
from the blob's layout, and **a span boundary can bisect a module** -- V.32's
half-duplex machine is split across two of them, `V32TxHdxModem` and eight
`TxHdx*` states on one side and `V32RxHdxModem` with twelve `RxHdx*` on the
other, divided between two adjacent functions 0x170 apart. Schedule V.32 as
`Dialer.c +18` **and** `V32mod.c +39` together, and never as a "Dialer pass". Any goal phrased as "cover the data
modes" requires it, and while it was fenced that goal could not be reached.

**FAX WAS LAST ON PURPOSE, AND SINCE 2026-08-31 IT IS THE CURRENT PHASE.**
The order below is unchanged as history and the reason it gave was never
difficulty; what changed is the GOAL, from covering the modes to completing the
object. The data modes and the services are done, so fax is the only thing
left. `docs/remaining.md` carries the decision and the measured scope. Read the
rest of this paragraph as why it was deferred, not as a reason to defer it now.

**FAX IS LAST ON PURPOSE, AND THE REASON IS NOT DIFFICULTY.** It is 283 symbols
and 78,331 bytes -- larger than everything else remaining put together -- and
SpanDSP already implements Class 1 fax in the open-source world, so the
marginal value of reconstructing it is lower than for anything else here. It is
a project phase, not a wave.

**"LEAVES BEFORE FAX" WAS TRUE, IT PAID, AND IT IS NOW EXHAUSTED -- MEASURED,
SO DO NOT RE-DERIVE IT (F8320).**

- **Backward, what it already banked:** of the fax closure's 45 already-written
  call symbols, **42 of them -- 18,156 bytes, 18.7% of the closure's 96,874
  bytes of code -- were written because a NON-FAX path needed them.** The
  strategy worked. It happened as a side effect of the data-mode work.
- **Forward, what is left to clear in advance: 0 bytes of code**, and 9,510
  bytes of data symbols. **All 283 unwritten fax `.text` symbols are reached by
  a fax entry point and by nothing else** -- zero overlap with data, voice, CID
  or ring.
- **The 0 is not a tool artefact.** A reverse-edge probe over the whole
  no-entry-point bucket found **129 of 139 symbols have no relocation anywhere
  in the 1.2 MB pointing at them, and 0 have a FAX-reachable referrer**. They
  are exported API surface with no internal caller, not a missing graph hop --
  and that includes names that read like core fax (`GenEQTrnSequenceV27`,
  `GenEQTrnSequenceV29`, `fax_class1_status`, `cHDLCtx_off_init`).

**AND THE OBVIOUS METHOD FOR CHECKING IS VACUOUS.** `service.py`'s `none`
bucket is *defined* as `unwritten − r_data − r_fax − r_oth`, so intersecting it
with the FAX closure is empty **by construction**. Anyone who runs that
intersection gets 0 and learns nothing from it. The reverse-edge probe is what
answers the question.

So: schedule the 139 leaves (16,013 bytes) **on their own merit**, and FAX on
its own size. Neither is a reason for the other.

## The rule that is not relaxed

**Nothing is committed that has not passed a differential test**, and nothing
is committed that is wrong-but-plausible. If a function cannot be made to
pass, leave it out and record the attempt. The goal is a replacement that
behaves *identically* to the blob, so any test disagreeing with the blob is a
hard failure whatever build it came from — never a tolerance to widen.

Run `make phase`, not `make test`. `make phase` is now the period/compiler and
structural reconstruction gate by default. Modern GCC, 64-bit, interop and
coverage tiers are explicit portability checks (`make portability` or
`make phase-full`) rather than the default reconstruction authority.

**IT IS A RULE ABOUT `src/`, AND `testbench/` IS NOT `src/`.** The harness is
measurement apparatus -- it places calls, records both ends, and analyses what
came back. There is no blob to be differentially identical to, so the rule
cannot apply to it and must not be read as forbidding a commit there. What
DOES apply is the discipline those tools were built under and which cost more
to learn: a detector must report its denominator, and a tool that prints
nothing is indistinguishable from a tool that is broken (findings F134, F2400,
F2401). Show a new analysis firing on a known input before trusting a clean
run from it.

## Budget your turns, not your reading

Finding F220 measured this, so it is not a guess. **Context growth is
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
(finding F220).

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
   who references one (finding F604). This is not the Ghidra prohibition — that
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
the tree instead of believing the brief (finding F6402). **A rules file has the
same shelf-life problem as a comment and no gate behind it**: findings F6100 and
F6103 are the same defect in a source comment and in this file's own gccdiverge
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

### Gate on `make period`, and read a red modern tier with suspicion

**Gate a reconstruction commit on `make period` ALONE.** It is the only tier
with no allow-list and the only one whose verdict is about the CODE. Three
things make this a rule rather than a preference, and all three were measured
in the 2026-08-30 leaf wave (findings F8410-F8497, `docs/remaining.md`):

- **The modern compiler here may not be the one the register was built for.**
  `tools/gccdiverge.json` was calibrated against GCC 13; a machine with GCC 14
  produces failures that are the COMPILER and not the source. One such
  breakage stopped `make phase` reaching the test tier at all on a clean
  master: `t_v34rx.c` used a `ref_` name it never declared, which 13 warned
  about and 14 makes a hard error. **`make -s print-CC` and check before
  believing a modern failure.**
- **THE HAZARD IS ONE-WAY AND SILENT.** Editing `src/` to satisfy the modern
  compiler moves the reconstruction AWAY from the object while `make period`
  keeps passing, so no test can ever report it. A construct the modern build
  demands is apparatus (see the flag/shim rule above), never source. **Never
  edit `src/` to make the modern tier green.**
- **A combined gate log CANNOT be read by position.** The old `make phase`, and
  current `make phase-full`, run period, modern-`test` and coverage tiers into
  one stream at `J>1` -- three copies of `t_v90cdesign` at once on a 3-core box.
  "The PASS lines before the first `gcc -m32` line are the period tier's" is
  WRONG, and that session believed it for several turns. **Attribute a verdict
  to a compiler by running that compiler alone.**

**Compiler-portability failures get their own GitHub issue as standard
procedure.** When the deciding period differential passes but a modern
compiler fails, check existing issues and create or update a dedicated
follow-up linked to the reconstruction issue. Record the branch/commit,
compiler versions and complete flags, reproduction commands, failing checks
and denominators, tested controls, and closure criteria. Distinguish measured
compiler behavior from an unproven root cause. Do not hold a period-validated
reconstruction commit open solely to finish that separate investigation;
record and link the failed portability gate when committing. Keep the modern
failure visible: no tolerance widening, source workaround, or declaration
change merely to make the gate green. Issue #19's GCC 14 follow-up, #30, is
the precedent. Shared working-instruction changes belong on master; carry
this procedure forward when the investigation branch is integrated.

`make phase` is the default reconstruction gate: period differential plus the
structural/provenance checks that do not require a modern compiler to reproduce
period x87 behaviour. `make portability` is the opt-in modern GCC, 64-bit,
interop and coverage/debug-site gate. `make phase-full` runs both. A branch can
be reconstruction-finished with `make phase`; a portability branch or release
claim should run and report `make portability` too.

**And the gate must report its denominator like everything else here.** The
run prints `period differential: N passed, M failed`; N is the TEST COUNT, so
check it moved by the number of tests you added -- 262 where master is 258 is
a gate that ran your four, and 258 is a gate that silently ran none of them.
Findings F134 and F2401, applied to the gate.

**Operationally:** `J` defaults to `nproc/2`, so pass `J=$(nproc)` when the
machine is yours; run it under `nohup`, because an interrupted `make` leaves
its `docker run` child alive and compiling; and never end the wrapper with
`echo`/`tail`, which reports THAT command's exit status and turned a red gate
into an exit 0 in the wave above.

**IT IS THE GENTOO-PATCHED GCC 3.4.2-r2 NAMED BY THE BLOB.** The default
period image is `ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, pulled
automatically from its dedicated reproducible-toolchain repository. That
repository owns the Gentoo 2005 stage3, recovered inputs and Docker build;
this tree is an image consumer.

Stock GNU 3.4.2 (`dsplibs-tc342`) and Debian's 3.4.4 (`dsplibs-tc`) remain
available only for explicit compiler A/B measurements. Modern compilers are
portability apparatus (including newer compiler and x86_64 work), never an
authority for reconstruction source or code-generation decisions.

**`make phase` RUNS IT**, so `make phase` needs docker and the
`tools/toolchain` image. It is incremental and sound -- an object is reused
only if it is newer than its source and than every header -- so an unchanged
tree relinks rather than rebuilding: about 34 s of the run. `make one T=...`
is still the fast loop between commits.

The modern build no longer runs in default `phase`; use `make portability` or
`make phase-full` for it. It is the portability check, and `make check64`
proves the tree is 64-bit clean. Where
GCC 13 provably cannot reproduce the object from correct source, the site is
declared in `tools/gccdiverge.json` -- eight entries today, thirteen checks --
rather than papered over in `src/`. That register names CHECKS, not tests, and
a stale entry (an allow-listed test that starts passing) fails the gate.
**`make period` has no allow-list and is not getting one.**

**AN ENTRY COSTS ITS BINARY'S WHOLE MUTATION SURFACE, so the divergent check
goes in a binary of its own.** `tools/mutate.py` judges a mutant caught by a
non-zero exit, and a declared binary exits non-zero on the UNMUTATED source --
so it cannot score a mutation set against that baseline and it refuses, for
the SUITE and not the row. That silently removed nine suites and 647 verdicts
before anyone counted them (findings F2157 and F3002). Three binaries now carry
one declared check each for this reason -- `t_v90p4dnan`, `t_v92ecnan`,
`t_v90adidnan` -- and **none of them has a mutation suite**, because a
registered suite that can never be recorded reads MISSING to
`mutsnap.py --check` and fails the gate. `t_v92ecparams` is the same move the
other way round: three GREEN members lifted out of a declared parent, so it
keeps its suite. Split the VALUE where you can rather than the group -- the
blob treats a NaN and 177.0f as one input, so `t_v90leaves` lost one check of
4,570 and kept every shape. Findings F6000, F6001 and F6002.

`t_v90equproc` is declared without being a split -- it is `V90Equalizer::
process`'s own binary and its divergence is the whole test's, not one check
lifted out of a healthy group -- and the no-suite rule binds it just the same.
**Do not register a mutation suite for it.**

**The eight are two causes, and only two.** Five of them are the object's
equality tests: a single ordered `fcom` with no parity test, which GCC 13
will not emit at all -- `-mno-ieee-fp` is accepted by it and does nothing, and
`-ffinite-math-only` does the job by withdrawing NaN semantics from the whole
translation unit, which breaks eleven other sites that depend on them. So the
source is the object's, `make period` proves it, and the modern build
declares. `t_agc`, `t_v90equ`, `t_v92ecnan`, `t_v90adidnan` and `t_v90p4dnan`.
Findings F2300 and F2304.

The other three are **x87 excess precision**, where the object narrows an
intermediate the modern compiler keeps at 80 bits: `t_psd` in the FFT
butterflies reaching a decibel (1453), `t_v90equproc` on the one
subtraction inside `V90Equalizer::process` whose difference feeds the squared
error, the DFE step and the high-error test (6203), and `t_v90specproc`, which
is 1453 INHERITED rather than a third site -- `V90SpectralVerifier::process`
calls `Psd::process` and takes 1 and 2 ULP in the 32 spectrum bins, nothing
about `process` itself (5804). Neither is closable by
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

**AND THE RULE HAS A SECOND HALF NOW, FOR THE OTHER COMPILER.** A construct
the MODERN build DEMANDS is apparatus by the same argument, and it does not go
in `src/` either. **Prefer a FLAG that withdraws the demand** -- the Makefile
has been doing this unstated since `-fno-lifetime-dse`, which exists to give
GCC 13 back a semantic 3.4.2 had. Where no flag can express it, the shim goes
beside `period_compat.h` in `tools/toolchain/`, force-included by `CXXFLAGS`
and never named from `src/`. That sibling does not exist yet and should not be
created empty; the one case so far was a flag.

    a construct GCC 3.4.2 lacks     tools/toolchain/period_compat.h
    a construct GCC 13 demands      a flag if one exists, else that sibling
    either one, inside src/         nowhere -- it is not the author's

**THE CASE THAT ESTABLISHED IT.** C++14 sized deallocation makes GCC 13 emit
`_ZdlPvj` for `delete p` on a class with a destructor, undefined in a tree
that links no libstdc++, and finding F7816 answered it with a
`#if __cplusplus >= 201402L` block in each of six `src/pump/v90/` files. Every
copy was CORRECT -- inert under 3.4.2, carrying no claim about the object --
and still apparatus inside the reconstruction. `-fno-sized-deallocation`
deletes the need rather than relocating it, and `src/` now contains no C++14
text at all. Finding F7900.

**AND NOTHING ELSE MAY BE MOVED ON THE ANALOGY.** The UNSIZED
`operator delete` / `operator delete[]` keep one copy per `.cpp` because an
inline definition's POSITION in the translation unit is a lever-3 carrier --
consolidating the unsized array form into `sysdep.h` cost eight destructors
their byte identity (finding F7815). The sized form was exempt only because
the period compiler never received its tokens, and even that was measured
rather than argued: all 200 period objects byte-identical, `md5sum` against
`md5sum`. Anything the period compiler CAN see gets the same measurement or
stays where it is.

`docs/method/compilers.md` is the register of every variance found so far,
including the one that was silent: 78 files guard their offset assertions on
`__SIZEOF_POINTER__`, a GCC 4.6+ predefine, so under 3.4.2 the guard read
`#if 0` and every assertion vanished while the file compiled clean.

## The second tier: comparing code generation

`.comment` names the original's compiler 279 times over — **GCC 3.4.2**, built
22 September 2005 (finding F606). `tools/toolchain/` has a container with it,
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
run. Finding F2200, and 2201 for what the blob's Gentoo patch stack means:
stock 3.4.2 is the exact POINT RELEASE, never the exact compiler.

**AND THE GENTOO COMPILER ITSELF IS PUBLISHED.**
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest` is
`sys-devel/gcc-3.4.2-r2` built from Gentoo's own ebuild inside Gentoo's own
stage3-x86-2005.0 -- glibc 2.3.4, binutils 2.15.92.0.2-r1 -- and it prints the
blob's `.comment` back byte for byte, double space and all:

```
make tc
make period
```

**Use it by default even where a stock 3.4.2 comparison happens to agree.**
The blob's own compiler banner is stronger provenance than a tree-wide
aggregate. DCR's earlier "stack-only" interpretation was retracted:
the first rejection from `byteident.py --why` does not establish that it is
the only difference (see `docs/cid-dcr-audit.md`). `-O3` (2155) and
`-mno-ieee-fp` (1990) were re-measured on the real compiler and both survive
symbol for symbol. Findings F2320, F2500, F2501 and F10269.

The flags were derived from the object, not guessed, and are in
`tools/toolchain/period.mk` with the evidence beside each:

    -O3 -frename-registers -march=i386 -mtune=i686 -mfpmath=387
    -mno-ieee-fp -fomit-frame-pointer -maccumulate-outgoing-args
                                                        (no PIC, no SSP)

`-mtune=i686` is worth knowing about: `-march` and `-mtune` are separate
questions and only the first leaves a trace, so "no cmov in 1.2 MB" bounds the
instruction set and says nothing about scheduling. Finding it took the match
from 30 to 82 (finding F612). `-frename-registers` took it to 92 (616).

**The retained baseline is `-O3`; it is not a uniquely recovered original
command line.** The historical result overturned 616's ruling against it: on a tree
three times the size, `-O2` matches 313 and `-O3` matches 324, and the `-O3`
set gains 15 while losing 4 rather than swapping. 616 measured at 92 of 365,
where `-finline-functions` had almost nothing to inline across. `make period`
is green at both, and there is no divergence to declare. Finding F2155.

**Issue #22 keeps the original profile open.** Source/inline choices and
individual options can reproduce matches under more than one optimization
level. Compare complete translation units, including non-exact bodies and
symbol binding; an exact-set gain alone does not establish the original
profile. See `docs/cid-dcr-audit.md` and GitHub #22 for measured controls.

**Before source/flag refinement, read `docs/method/experiment-design.md`.**
It is #22's anti-loop workflow: baseline controls, crossed source/option
experiments, bounded domains, explicit stopping/reframing, and full-TU review
including non-exact bodies and exports. Current exact matches are evidence,
not a reason to freeze potentially compensating source/flag choices. Do not
require monotonic improvement during exploration or adopt a candidate merely
for a net score gain. Record the next discriminating test in the issue before
repeating a stalled line of inquiry.

**Every reconstruction experiment must enable `DSPLIB_REPRODUCE_BUGS`.**
Use the shared experiment-toolchain helpers to append the define after
configurable flags. Record and review the actual complete compiler command
against the baseline's `.build-config` before interpreting a one-option
control; a copied flag list missing `-frename-registers` is a different
experiment. Execute the assembler selected by the compiler to identify it:
printing `gcc -print-prog-name=as` alone is not its version. Preserve invalid
runs as explicitly invalid artifacts, exclude their results, and rerun the
correct controls. Two delegated matrices required this correction in #22.

`-mno-ieee-fp` is the newest and its worth is not in its +2 (302 -> 304): the object's float
compares are ordered, 406 `fcom`-family against four `fucom` that are all
inside libm's `pow`, and the default `-mieee-fp` emits `fucom` for every
comparison whatever the source says. Until it was set, **every float
comparison in every function read as a codegen mismatch** -- so a numeric
function's per-symbol diff was measuring our flags, not our source. Finding
F1990.

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

  **Before removing a narrowing cast, or changing a field to make one go
  away, answer all three questions.** (1) Does the object actually narrow the
  *computed value* before the call or store? A `cwtl`, `movswl` or narrow
  stack argument at that point is evidence for an explicit narrow
  intermediate, not a cleanup opportunity. (2) Could that intermediate come
  from a correctly-declared `short` local or field instead? Trace the load to
  a use: a live `movswl`/`movzwl`, signed division, or signed comparison is
  evidence for the declaration; do not infer an unsigned field merely from a
  cast at a call site. (3) Is the extension's upper half discarded before it
  is used? If so it is the LOCAL's code-generation carrier, not evidence for
  the field's type (7803).

  **For a confirmed narrowing bug, split reproduction from repair.** Keep the
  blob's narrow arithmetic under `DSPLIB_REPRODUCE_BUGS`; in the normal arm,
  widen the signed source values to an actually wide allocation/count type
  before arithmetic. An `unsigned short` temporary merely moves the 16-bit
  boundary and is neither evidence that the source field is unsigned nor a
  full-width fix. Prove the reproduction arm with the period compiler and
  prove the repaired arm at the boundary where the original count wrapped.
- **NOT FREE, ONLY FREE OF THE THING YOU FIRST BLAMED.** This bullet used to
  read "free, so ignore it: register allocation, instruction scheduling, the
  extension on a load whose upper half is discarded (614)", and two of those
  three have since been steered to byte identity. What is true is narrower:
  **they are not determined by the statement you are looking at.** They are
  determined from outside it, and that outside thing is a real source property.
  - **Register allocation** follows the translation unit's EMISSION ORDER.
    Reordering nine files' function definitions to the blob's own order gained
    17 symbols and lost none (7796), and it later took `loadParams` — 7,894
    bytes, 2,663 differing, the largest such symbol in the tree — to byte
    identity as a BYSTANDER of a reorder aimed at something else (7800). The
    carrier is upstream of the function, not its own index.
  - **A dead extension** follows the declared type of the LOCAL being loaded
    into — not the field, not the store destination, and not a cast; six
    spellings compiled, one matches (7803). 614 is still right that the FIELD's
    type is not what varies, and the blob proves it by using both extensions on
    one field.
  - So do not chase these INSIDE the function. Look outward.
- **In between, and it needs the strong test:** statement order. GCC does NOT
  simply preserve it — `toneiir_reset`'s source is already in the object's
  order and the compiler reorders ours (617). The acceptance test is FULL-TEXT
  identity, operands included.
  **ENUMERATE, DO NOT SEARCH.** The candidate spellings are a small finite
  family: compile ALL of them. If exactly one maps onto the object you have
  recovered an order within that declared family, even where the compiler
  reordered your source (7770). Do not claim uniqueness across untested source
  forms or compiler profiles. If several
  map, you have decoded a specific FACT and the finding must say which. If none
  does, those tested statement orders do not explain the difference under
  those controls — not a global exclusion of statement order. For example, 16 spellings of
  `packData`'s loops, maximum 516 against the object's 534 (7785).

**THE RULING ON FIT VERSUS RECOVERY (7782), because these levers all raise
it.** Byte identity is the target and it is TAKEN when the candidate space is
exhausted and exactly one element maps onto the object — the preimage is
unique within an evidence-backed domain, rather than selected by a near-match
score. This does not prove uniqueness over all possible source/flag choices.
It is DECLINED when you are hill-climbing on byte count: one pass declined
`V92Phase4Modulator::reset` at 27 differing bytes of 290 because its
14-spelling enumeration contained no match, and another declined a 2-of-387
near-miss because the enumeration was completed before any cell was read.
Closer bytes are not a grade. Record which side of the line a closure falls on
and what the domain was; "closed by reordering" without the domain is not
reviewable.

**`docs/method/refinement.md` is the playbook for all of this** — rule 0 and
nine levers, each with the measurement that established it AND the case where
it failed. Read it before a refinement pass. It is where these entries are
maintained; this section is the summary.

### The tools, and their precision

- `byteident.py` (`make byteident`) — **grade 0 and grade 1, and the only
  tool that measures either.** Grade 0 is positional byte identity: the
  same bytes in the same places, which is the first question anybody
  actually asks. Grade 1 is the same instructions and operands under a
  renaming taken PER LIVE RANGE, which is what an allocator actually
  chooses; `--self-test` carries eighteen cases and TWELVE of them are things
  it must still REJECT, because a looser check fails by calling different code
  equivalent. **433 of 1,251 at grade 0 and 486 at grade 0-or-1 today** — read
  the number from the tool, not from here. `--why SYMBOL` prints the row
  `alpha_equal` rejects on, which is usually NOT the first row that differs.
  It refuses to run when `build/tc_out` is older than `src/`, because `make
  phase` does not build that directory and nothing else does either, so a
  merge leaves it stale while every count keeps rendering as a clean, plausible
  and wrong number (7769). `docs/method/refinement.md` is the playbook: every
  lever with the measurement that established it and the counterexample that
  bounds it. Read the count from its own headings, not from here.
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
  unverified and named (finding F2402). Every hit must be traced against
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
  integer if-conversion. 69 rows classified in finding F2900, of which three
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
watching it appear, then restoring and watching it go. Finding F134's argument.

**And it happened again, to both of them at once.** They kept defaulting to
`TC_OUT=/tmp/tc_out` after the period build moved its output to
`build/tc_out`, so
with no environment set they compared **zero** symbols and reported a clean
tree, exit 0 (finding F2400). Both now refuse to run on an empty `TC_OUT` and
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
prints the count on every line carrying a verdict; the opt-in portability
boundary quotes those denominators and refuses to be printed without them.
Finding F3100, and it is the same defect as 2400 with the gate rather than an
aid behind it.

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
`(name, kind)` pairs. Findings F3055, F3110 and F3111.

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

**Track outstanding work in GitHub issues.** Tasks, defects to investigate,
and open lines of inquiry belong in issues, linked to their parent milestone
where appropriate. Check existing issues before creating another; update
them with evidence, next actions and completion criteria as work progresses.
The findings and deviations logs remain the record of measured conclusions
and original-binary behavior; they are not the task queue. Link between the
issue and the relevant finding rather than treating either as a substitute
for the other.

Shared tool improvements and these working instructions belong on `master`.
Keep active investigation branches current by merging or rebasing `master`
after shared changes land; keep experimental reconstruction changes separate.
`AGENTS.md` is a symlink to this file, so both agents read the same rules.

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

- **AN UNWRITTEN CALLEE DOES NOT "RESOLVE TO THE BLOB". IT FAILS TO LINK.**
  `symmap.py` renames EVERY defined blob symbol to `ref_*`, and every test
  binary links all of `$(OBJ_REPRO)`, so one reference from `src/` to a symbol
  this tree has not written is an undefined reference that fails the whole
  suite. The scaffold that would change this is the F214 spike, which **F215
  declined** -- so do not brief anyone with "unwritten callees resolve at link
  time", which a 2026-08-30 wave's briefs did, costing nine symbols that were
  written and then had to be withdrawn (F8492).
  **AND IT BINDS ON DATA REFERENCES, NOT ONLY CALLS.** Storing a handler's
  address -- `movl $handler, field` -- has no `call`, appears in no call
  graph, and pins the symbol at link exactly the same way; `RxNextStateV21`
  alone installs three. Check `dis.py` for BOTH relocation kinds before
  scheduling a symbol, not just `R_386_PC32` (F8493). A symbol whose referents
  are unwritten is BLOCKED, not hard: it becomes writable the moment they
  land, and the honest move is to leave it and say so.
- **A `.c` calling a `.cpp` is about the LINK LINE, and the link line has been
  fixed.** This used to read "a `.c` may not call anything defined in a
  `.cpp`", because the interop binaries linked only `$(SRC)` -- every `.c`
  under `src/`, no C++ -- so such a call compiled, linked 32-bit, passed `make
  one`, and failed `make phase` at `t_spandsp_v23` with an undefined
  reference. They now also link `$(CXXOBJ64)`, a 64-bit build of the C++ half,
  and `src/pump/v34/V34hshak.c` already calls `V34SetINFO1aBits` and
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
  Findings F333 and F344 for why `v34hstx1.cpp` is a `.cpp` at all; 711 for the
  expiry and the three conditions.
- **A relocation on a call proves nothing about the translation unit; its
  ABSENCE does.** A resolved PC-relative displacement with no relocation means
  the target is `LOCAL` and in the same TU. A relocation being present only
  means the symbol is `GLOBAL`. Findings F306 and F333.
- `tools/dis.py` shadows the standard library's `dis`, which `inspect`
  imports. A tool in `tools/` that reaches for pyelftools dies with
  `AttributeError: module 'dis' has no attribute 'COMPILER_FLAG_NAMES'`,
  naming neither the directory nor the file. See the top of
  `tools/whichfield.py` for the fix.
- **objdump swaps FDIVP/FDIVRP and FSUBP/FSUBRP.** The `DE` pop encodings
  print as their own opposite: `de f1` reads `fdivp` and IS `FDIVRP`
  (`ST(1) = ST(0)/ST(1)`). The `D8` register forms are fine. For any popping
  divide or subtract, read the bytes, not the mnemonic — finding F245.

  **Upgrading objdump does not help and `tools/dis.py` now tells you anyway.**
  Binutils 2.15 in the container and 2.42 on the host print these identically
  in AT&T syntax; it is what the syntax means, not a bug being carried.
  `objdump -M intel` renders the same bytes the architecture's way, and
  `dis.py` runs that second pass for you and appends `<== Intel: fdivp` to any
  line where the two disagree — 4 lines of 763 on a real function, silent
  elsewhere. Finding F2156, which also records how the measurement nearly went
  wrong: comparing two objdumps under different `-M` settings made them look
  exactly opposite and almost retired 245.
- A string reference is an `R_386_32` against the SECTION symbol with the
  offset as an inline addend, so searching the disassembly for a string's
  address finds nothing and proves nothing. `tools/relocscan.py --at
  .rodata.str1.1:0xNNNN` is what answers "who references this string" —
  finding F604, where not knowing that had a defect misdiagnosed for weeks.
- A per-FUNCTION count across an inlining boundary measures our factoring, not
  our completeness: where we split one of the original's functions into static
  helpers, the helpers have no blob symbol and their bytes count against
  neither side. Read `debugaudit.py --missing`'s per-file rollup first
  (finding F605), and `compare.py`'s per-object one (610).
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
  everything -- finding F700.
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
    tree's copy when it can find one (finding F1563).
  - `BLOB ?= ref/slmodemd/dsplibs.o` pointed at `.claude/worktrees/slmodemd` and
    every run died at `No rule to make target`. Loud, so not the same class of
    bug, but it blocked every worktree run until someone passed `BLOB=/abs/…`.
    The default is now resolved through `git rev-parse --git-common-dir` —
    the main repository's `.git` seen from inside any worktree, the same trick
    `prereq` uses. `make -s print-BLOB` says what it resolved to, and an
    explicit `BLOB=` still wins.

  What neither of them was is a reason to distrust a worktree's gate. That was
  finding F3100, and it was a branch difference and not a worktree one: the
  coverage tiers had stopped measuring on `master` and would have measured
  nothing in the main tree too, the moment it checked `master` out.
