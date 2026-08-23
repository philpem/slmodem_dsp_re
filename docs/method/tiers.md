# The five oracles, and what each cannot see

Every number here cites `docs/findings.md` in this tree. Nothing in this file
is advice that was not measured.

The four tiers do not form a hierarchy. **Neither of any two subsumes the
other** (finding 613), and each has a blindness that is structural rather than
a matter of writing more tests. Knowing which tier can possibly see a given
class of defect is most of the value; the rest is running them.

| tier | decides | structurally blind to |
|---|---|---|
| differential, **under GCC 3.4.2** | correctness over the reachable input domain | anything that does not change an output |
| mutation | whether a claim is tested at all | whether the claim is *right* |
| codegen vs. the object | what the compiler was forced to encode | anything the compiler was free to choose |
| transcript / diagnostics | what the two sides *say* | anything that prints nothing |

---

## 1. Differential — the only thing that decides

Reconstruction and blob linked into one binary, driven from one deterministic
input, outputs compared sample by sample. Any test disagreeing with the blob
is a hard failure, never a tolerance to widen.

**AND IT IS THE PERIOD COMPILER THAT RUNS IT.** `make period` builds `src/`,
`test/harness/` and `test/unit/` with GCC 3.4.2 in `tools/toolchain/`, links
them against the blob with binutils 2.15, and runs the suite — our source and
the object compiled by the *same* compiler, so a difference is a difference in
the code and not in the toolchain. It is part of `make phase`.

This was not always so, and the reason it changed is a failure mode worth
naming. While GCC 13 was the only gate, every place the two compilers
disagreed had to be absorbed **somewhere**, and the only place available was
the reconstruction's own source: a `volatile` here, a `(double)` cast there,
each one making the source less like what the author wrote in order to satisfy
a compiler the author never used. Every gate stayed green while the artefact
the project exists to produce drifted. Three such shims were found and removed
once the period build could adjudicate; one of them had changed `float`
arithmetic to `double`, which is an alteration of the program and not of its
compilation (findings 1352, 1354; `docs/method/compilers.md`).

The modern build still compiles and still runs, as a portability check and a
faster inner loop. Where GCC 13 provably cannot reproduce the object from
correct source, the site is declared in `tools/gccdiverge.json` — one entry
today — rather than papered over in `src/`. **`make period` has no allow-list
and is not getting one:** the period compiler has no excuse, being the one the
object was built with.

**A rejection in `src/` under GCC 3.4.2 is a finding.** The author wrote this
code for that compiler, so anything it refuses is something the author cannot
have written. A rejection in `test/` is plumbing. And where our own apparatus
needs something the old compiler lacks, the shim goes in
`tools/toolchain/period_compat.h`, outside the reconstruction.

**What it cannot see, with the cases that proved each:**

- **The declared signedness of a field whose values never go negative.**
  `struct b103_hdx.mode` was `short` here and `unsigned short` in the original.
  It holds 0, 1 or 2 — loopback, originate, answer — and over that range
  `movswl` and `movzwl` produce identical values. A difference needs 0x8000,
  at which point both readings index outside the function-pointer table
  anyway, so the divergence exists only where both are already faulty. It was
  invisible to **1,104 differential tests** and visible in **nine
  instructions** (finding 613).
- **The width of a counter that never overflows, and a guard on an input no
  caller produces.** Same argument (finding 613).
- **A diagnostic call site that is absent.** The debug level ships at zero and
  every gate is `> 1`, so a missing call and a present one behave identically
  under every test in the tree. The blob makes **1,670** `dsplibs_debug_printf`
  calls across 399 functions; this tree made **22**, and 242 were missing from
  functions that *are* reconstructed (finding 134).
- **The order of two stores that leave the same bytes behind.** See tier 4.
- **Its own resolution.** Of 1,667 `diff_eq_int` call sites, **819 passed a
  format string with no conversion in it**, so the offset the check had
  computed was formatted by nothing and discarded. Every one of the 819 was a
  real comparison that really passed — the suite was not weaker than it
  looked. But a *failing* one said `got 68, reference 136` without saying
  which byte, and the session then earned that back by re-reading the
  disassembly (finding 220). Compare whole objects with a helper that
  coalesces differing bytes into runs and reports the first difference first;
  resolve the offset to a field from your own DWARF.

**The corollary that is easy to miss.** A test that links the wrong pair of
objects reports NOT CAUGHT for everything, which is the same output an
untested claim gives and indistinguishable from it without looking. Six
mutation sets were misread that way before a manifest existed
(`tools/mutate.py`'s own header).

---

## 2. Mutation — anti-vacuity, not correctness

Break the code on purpose; check the tests notice. It exists because the code
and the test are written by the same hand at the same sitting from the same
reading, so **a green run says the two agree — it does not say either is
right** (`tools/mutate.py`).

Twice the only thing that caught an error here was deliberately introducing
one (`tools/mutate.py`), and the second time it found something better than a
bug. Six deliberate mutations were made to three hand-written call sites: five
were caught immediately — swapped arguments, a dropped call, a misspelt
string, a wrong variable — and the sixth, **moving a print to before the call
it followed, passed everything**, because the call it was ordered against
prints nothing. The test could not see call-site *order* at all (finding 149).

**What it cannot see:**

- **That the claim is correct.** A mutation proves the test would notice the
  code changing. If the code and the test are wrong in the same direction, the
  mutation is caught and both stay wrong.
- **Its own losses.** An UNUSABLE mutation does not fail a run, so a suite that
  has quietly lost mutations still prints `0 NOT caught` (finding 347). Read
  the UNUSABLE count, not the NOT-CAUGHT count.
- **Where its anchors landed.** Nine entries in one suite were mutating a
  different arm from the one their label named, and **every one was reported
  CAUGHT** — at a claim nobody made (finding 432). `src.count(find) == 1`
  cannot detect this by construction.
- **That its own equivalence arguments still hold.** A mutation marked
  `equivalent` is a claim that nothing in the harness can observe it — a claim
  about the harness, not about the object — and it expires the moment the
  harness gains an observable. Ten of one suite's recorded equivalents became
  CAUGHT when the diagnostics were turned on, with every argument still a
  correct statement about the object (finding 651, and §4 below).

**What it says when you finally write the numbers down.** Recording all 48
suites at once gave **37 mutations NOT CAUGHT across 10 of them**, plus one
unusable — including `v34k56` at 15 caught against 10 not, so **40% of what
that suite claims to check is unchecked**. Nobody could have known: the number
existed only in the scrollback of whichever session last ran it (finding 545).
**37 is the figure at findings 545 and 638, and it has since moved**: eight of
`v34hstx1`'s eleven fell when the transcript tier gained its missing lines
(finding 651). No finding records a tree-wide total after that, so this one
does not either — which is the point of recording per-suite verdicts under a
key rather than a total in prose.

### And what a recorded snapshot catches that a run cannot

A batch bound a file's offset macros to the fields they name with a
compile-time assert. `make phase` passed. The assert made one mutant —
`rxstate and txstate offsets transposed`, a mutation the suite had carried for
a long time with a recorded verdict of CAUGHT — fail to *compile*, moving it
CAUGHT → UNUSABLE; and an unusable mutation does not fail a run (finding 347),
so the suite quietly stopped measuring the thing that mutation was written to
measure. What caught it was the comparison against the record **by name**
rather than by count, which is the whole argument for the snapshot existing
(finding 637).

> **Do not put a compile-time check where a mutation already measures the same
> thing.** A tautology checked by the compiler is not worth an empirical
> guarantee given up. Split by coverage instead: the macros some mutation
> rewrites are left to the mutations, and the ones no mutation touches get the
> assert. Grep the mutation files for anything you are about to assert — here
> eleven macros are rewritten by some mutation and exactly one collided
> (finding 637).

The general shape is worse than the instance: **adding a static check to a
file that has a mutation suite is not free, and the phase gate cannot see the
cost.** It went green with the mutation broken.

---

### Running them without taking the machine hostage

`tools/mutate.py --jobs` defaults to **half the cores**, not all of them.
Every mutation is a build plus a test run in its own copy, so N jobs is N
concurrent compilers, and a full re-record is 4,653 mutations over tens of
minutes -- long enough that leaving the machine unusable for the duration is a
bad trade for the last few percent. `tools/toolchain/period.sh` halves for a
sharper reason: it NESTS, since `make phase` already runs at `-j$(nproc)` and
one of its recipes is that script asking a container for another J.

Both are defaults, not caps. Run higher deliberately when the machine is
yours -- and note mutate.py's own record that a real bug was found BECAUSE
jobs were high, so this is a load choice and not a determinism one.

## 3. Codegen against the period compiler

Not "does it behave the same" but "did the same compiler, given our source,
emit what the original's compiler emitted". It reaches exactly the class tier 1
cannot, and pays for itself the first time it does.

**Tier 1 now uses the same toolchain, and these remain different questions.**
`make period` runs the suite and asks whether the OUTPUTS agree; `make
similarity` compares the INSTRUCTIONS. A function can pass the first and match
none of the second — different factoring computes the same thing for ever —
and the reverse is possible too. Sharing a compiler removes an excuse from
both, nothing more.

It also removed a measurement error. The two builds were compiling `src/` with
different flag sets: `build.sh` passed neither `-D__SIZEOF_POINTER__=4` nor
the compat header, so it built 20 fewer translation units *and* silently
elided the 81 offset assertions guarded on that predefine. One flag set took
the ratchet from 386 compared / 105 identical to **750 / 254**, almost entirely
over C++ the codegen tier previously could not build at all.

`.comment` named the compiler 279 times over — GCC 3.4.2, built 22 September
2005 (finding 606). A container with gcc-3.4.3 and binutils 2.15 compiles 76
of 81 translation units unmodified (finding 607).

**Flags come out of the object, not out of a guess.** Each of these was read
from a specific disagreement:

| flag | what gave it away | effect |
|---|---|---|
| `-fno-pie` | zero `get_pc_thunk` in the blob, 41 in our build | finding 606 |
| `-fno-stack-protector` | no `__guard` symbol anywhere | finding 606 |
| `-fomit-frame-pointer` | `ulaw2alaw` opens `movzbl 0x4(%esp)`, ours `push %ebp` | size matches 6→29, identical 0→18 (finding 607) |
| `-maccumulate-outgoing-args` | `BwChDem_Delete` fills the outgoing area with `mov`, ours `push` | 29→37, 18→26 (finding 607) |
| `-mtune=i686` | nothing — it leaves no mark | 30→82 identical (finding 612) |
| `-frename-registers` | `Agc<float>::reset`, 7 instructions of 11 | 83→92 identical (finding 616) |

**`-mtune` is the important one methodologically.** `-march` and `-mtune` are
separate questions and only the first leaves a trace, so "no `cmov` in 1.2 MB"
bounds the *instruction set* and says nothing about scheduling.
`-march=i686` matches equally well and emits 147 instructions the object does
not contain anywhere, so the object excludes it; `-march=i386 -mtune=i686`
gives the same 82 and stays consistent with every byte (finding 612). Every
flag before that had been read directly out of the instruction stream; this
one could only be found by search against a similarity metric — a second,
weaker mode of evidence that reaches things the first cannot.

### The rule for reading a difference: FORCED vs. FREE

**Act on what the compiler was forced to encode. Ignore what it was free to
choose.** Five mistakes in one session came from getting that backwards
(finding 614).

- **Forced.** The signedness of a load *whose 32-bit result is used*. That
  qualifier is the whole rule: of the first six hits from the signedness
  detector, five were dead extensions — the value stored straight back as 16
  bits, a field copy or a filter history shifting along, where the upper half
  is discarded and the compiler was free either way. Changing four struct
  fields on that signal would have been wrong and no test would have objected
  (finding 614).
- **Free.** Register allocation. Instruction scheduling. Of the 22 same-size
  near-misses: one real defect, five dead extensions, and the remainder
  ordering or register allocation (finding 614).
- **Weak but real.** A reordering. GCC is deterministic, so a different order
  *does* mean something in the source differs — but the map from source to
  schedule is many-to-one, so a matching order confirms a candidate spelling
  and never reveals one. Searching over spellings until the output matches is
  fitting the compiler: the result matches but was not derived, which is the
  same failure mode as writing code to pass a test rather than to be right
  (finding 614).
- **The sharp version.** A reorder permitted by both sources is noise; **a
  reorder our source's data dependencies would FORBID is a defect** — a real
  difference in what the code computes, findable by inspection rather than by
  search. None of the 22 showed one (finding 614).
- **Statement order is not preserved.** `toneiir_reset`'s source was already
  in the object's order — `0x96 0x2c 0x98 0x94` — and GCC emitted ours as
  `0x96 0x94 0x98 0x2c`, hoisting the short store and sinking the `int` one
  (finding 617). So the acceptance test for acting on a store-order hint is
  **full-text identity, operands included**. Two functions of nineteen passed
  it and were changed; seventeen were left alone.

### The three grades, and why the weak one is still worth naming

A codegen result is one of three things, and saying which is not pedantry --
each grade licenses a different conclusion:

- **grade 0 -- the function's binary code is identical.** Nothing is left to
  argue about.
- **grade 1 -- the instructions are the same and only register allocation
  differs.** `compare.py` drops operands, so this already scores as a match:
  register allocation is the compiler's free choice (finding 614) and chasing
  it means permuting source until the output matches, which is fitting the
  compiler rather than recovering the source.
- **grade 2 -- minor instruction differences that change neither the result
  nor the execution.** Scheduling, an extension on a load whose upper half is
  discarded, integer if-conversion. `samesize.py` is the tool for this bucket
  and 2900 classified 69 of them: three were real defects, three were 614 and
  declined, ten were forced and named.

Below grade 2 is not a grade. It is a difference to explain.

**Grades 0 and 1 are now MEASURED, by `tools/toolchain/byteident.py`, and
`compare.py` measures neither.** `make byteident`, over the same denominator
`compare.py` uses -- the symbols the blob and `build/tc_out` both define:

| | of 1,200 | |
|---|--:|---|
| **grade 0** — same bytes in the same places | **393** | 32.8% |
| — as grade 0 but a section relocation cannot be compared by name (604) | 5 | |
| **grade 1** — same instructions and operands, renamed per live range | 38 | |
| **grade 0 or 1** | **436** | **36.3%** |
| same size, bytes differ | 132 | |
| different size | 629 | |

**Read that against `compare.py`'s 480 "identical instruction sequences" on
the same tree.** Around 140 functions have the same mnemonic sequence and are NOT
equivalent under a register bijection, because `compare.py` drops operands
entirely -- `mov $1,%eax` and `mov $2,%ebx` are one instruction to it.
`CarrierDetectB103` is the shape: same mnemonics, and the two loads are
`0x8(%edx)`/`0x4(%edx)` in the blob against `0x4(%edx)`/`0x8(%edx)` in ours.
That may still be functionally equivalent -- deciding needs the dataflow, which
is grade 2 and is a judgement -- but it is not the same instructions, and the
older number counted it as if it were.

**Grade 1's renaming is PER LIVE RANGE, not per function.** A single
bijection held across a whole function rejected 20 pairs that differ in nothing
but allocation -- the blob puts two successive values in `%eax` where ours puts
the second in `%edx`, which no function-wide map can express. A destination
written without being read ends the range and the pairing lapses there;
`add %eax,%ebx` reads and writes `%ebx` and is not a definition. That took
grade 1 from 18 to 38. **It is a LOOSENING, so its self-test is inverted:
`byteident.py --self-test` carries eight cases and SIX are things the check
must still reject** -- a different immediate, a different displacement, crossed
live ranges, a pinned `%esp`, a read-modify-write mistaken for a definition,
and an indexed memory operand mistaken for three operands. A tool that only
proves it accepts has proved nothing.

**AND THE TOOL'S OWN RELOCATION NORMALISER DID NOT FIRE FOR ITS FIRST DAY.**
It patched relocations in a second loop over the disassembly, so `rows[-1]` was
the function's LAST instruction every time -- usually a `ret` with no operands
-- and 210 relocated instructions across the tree were normalised into nothing
while the run reported cleanly. Worse, a `call`'s operand is a bare address
already rewritten function-relative, so there was no numeric literal for the
target to replace at all: `V90PreFilter`'s two destructors call
`FloatFIR::~FloatFIR` **D2** in the blob and **D1** in ours, and both were
certified grade 1, "same instructions and operands". Fixed by applying the
relocation to the instruction it belongs to, appending the target where nothing
was substituted, and refusing to let grade 1 overrule a differing relocation
target at all. The correction moved grade 1 from 10 to 18 and RELOC from 0 to
3 -- found by an independent agent and reproduced here, arriving at the same
four numbers from a different repair.

**Three further artefacts inflate a text comparison of disassembly, and each
was found by looking at what the tool called a difference.** Absolute branch targets
(`jmp 7e321` against `jmp f1` is one jump printed twice; 310 functions scored
different by that alone). Section-symbol relocations against named ones
(finding 604). And relocated displacements, where the blob's addend rides
inline and ours is a zero with a relocation beside it. `byteident.py` corrects
all three -- branch targets are made function-relative, relocated fields are
compared by TARGET rather than by value, and a section-vs-symbol pair is
reported as UNRESOLVED rather than as a difference. A tool that has not been
made to handle them is reporting its own artefacts.

### The size ratio is a completion gauge, not a codegen metric

**And the number below is not what its sentence used to call it.** This
paragraph said "the count of BYTE-IDENTICAL functions"; `compare.py` runs
`objdump --no-show-raw-insn` and cannot see a byte, so what stayed flat at 92
was the count of functions whose MNEMONIC SEQUENCES matched. Byte identity was
not measured by anything until `byteident.py` was written; see below for what
it actually says.

The size ratio is the weak number and it moves when you emit more code, not
only more of the right code. `-O3` took it from 77.3% to 89.0% while that count
stayed **flat at 92 across that entire 12-point swing** — our code grew 17 KB, we were undershooting, and the gap closed
arithmetically (finding 616). Every one of `-O3`'s extra matches came from
`-frename-registers` alone; `-finline-functions`, `-funswitch-loops` and
`-fpeel-loops` added 17 KB and **not one additional match**.

Also: a mnemonic comparison is not a byte comparison. Two functions storing
the same constants to different offsets both read as `mov mov mov`.

### And the floor only exists if it is written back

`--ratchet` compares against the recorded numbers; `--update` is what records
them. A gain measured at a merge, reported in a commit message and never
blessed leaves the floor where it was, and the next batch's twelve-symbol
*regression* then prints `ratchet OK -- gained` (finding 556). Read the number
twice before blessing it, too: a cold container reports a partial
`identical ... now 48` on its first invocation and 105 on every run after
(finding 651). Both halves are `gates.md` rule 2 and its third failure mode.

---

## 4. Transcript / diagnostics — not decoration

The diagnostic call sites are carried because the gate is real control flow and
**the format strings are the original author's own words**. Discarding a call
site discards the annotation, and this reconstruction has leaned on those
strings repeatedly: "Near"/"Far" fixed which echo canceller is which,
`V34HSHAK: Freeze EC` named a function's flag, and one string identified an
inlined function (finding 134).

**The case that makes this tier load-bearing.** Arm 54 incremented a counter,
branched, and zeroed it — in the wrong order:

```c
/* ours */                            /* the object */
n = vect_idx + 1;                     n = vect_idx + 1;
if (n != 0xa) { vect_idx = n; ... }   vect_idx = n;
vect_idx = 0;                         if (n != 0xa) ...
hs_setstate(...);                     hs_setstate(...);
                                      vect_idx = 0;
```

**Both orders leave the same bytes behind** — the intermediate 10 is never read
and the field ends at 0 either way — so no byte comparison can separate them,
and none did for six batches. Only the value printed inside the state-change
diagnostic could (finding 573).

### What this tier cannot see

Measured exhaustively, by applying each uncaught mutation and differencing one
side's transcript against itself (finding 570). Of eleven uncaught mutations,
**five** would be caught by a working transcript comparison and six never will:

- **Three are indistinguishable under the fixture's fill.** Both echo cancellers
  are empty, so both print `Nothing to report`, and a mutation that reports the
  first twice instead of first-then-second produces a byte-identical
  transcript. No routing fixes that; **a fill in which the two hold different
  coefficients would** (finding 570).
- **Two print nothing at all.** `initdigital` emits no line on that path, so
  "not called" is invisible, and its guard equally so. These had been
  miscounted as "print-only, expected" — they are not print-only, they are
  unobservable (finding 570).
- **One is a genuine oracle gap** — a transfer with no oracle, confirmed by
  name rather than inferred from "one survived" (finding 570).

### That measurement was of a tree that has since changed

Finding 570's method is sound: dump side A's transcript under each uncaught
mutation and diff it against the unmutated dump, on the argument that side B
never changes, so a mutation whose side-A text is identical cannot be caught by
any transcript comparison. The argument holds. **What it measured was the text
that tree then printed.** When the missing diagnostic lines were written, of
the eleven uncaught mutations **eight flipped to caught, not five**, and three
remain — because three of the six that finding 570 called identical are
separable now, all three being swaps between two echo cancellers that had both
printed `Nothing to report` under the fixture it had (finding 651). The
prediction being tested said one would remain and finding 570 said six; both
were wrong, in opposite directions.

> **A measurement of what a system currently emits is not a property of the
> system.** It is a property of the system *and* of what it emits today, and
> it needs re-measuring whenever the second changes. Say that in the finding,
> because on the page it reads as a permanent property and is not.

The three that remain are the ones finding 570 called identical for reasons
that have nothing to do with printing: a wrap tested before its counter is stored, a
call not made on a path that prints nothing either way, and the transfer with
no oracle (finding 651).

### When the oracle moves, the equivalence flag is what is wrong

Ten mutations in that suite were recorded `equivalent` — *this cannot fail* —
and all ten came back CAUGHT. The runner prints `RECORDED AS EQUIVALENT AND
CAUGHT ANYWAY -- the argument for these is wrong, or the code has moved under
it` and fails the run. **Neither was true.** Every one of the ten arguments is
still a correct statement about the object; what changed is that one function
now prints its state transition and another prints all five of its arguments.
Seven of the ten had swapped two functions that are the same code at debug
level 0 and are not the same code at debug level 2; the other three changed a
value the code does not *use* and the trace now *reports* (finding 651).

There is a third case the message does not name: **the oracle moved.** The
right response was to drop the flag and keep the argument, each prefixed with
what separated the two and why the argument is still true of the object.

> **An equivalence argument is only ever equivalence under the checks that
> exist.** Ten claims in one suite were being checked by nothing while reading
> as deliberate, and what exposed them was not a better argument but a new
> observable (finding 651). Every `why` in every suite is a hostage to that;
> a by-name comparison against a recorded snapshot is what makes the change
> visible when it happens — here it named all eighteen moved verdicts.

### Two operational facts

- **It needs a green baseline to run at all.** Turning the diagnostics on made
  the suite fail 322 of 23,295 checks, and the mutation runner correctly
  refuses a baseline that is not green — every mutation would report CAUGHT for
  a reason unrelated to the mutation. So the count the previous batch had asked
  for *did not exist* (finding 570). **That was the state at finding 570 and is
  no longer true** — the suite is green with the diagnostics on, the driver
  enables them unconditionally and the phase gate runs it (finding 651). The
  rule stands: a tier that cannot be run has no verdict, and "the suite goes
  red" is a result rather than a postponement.
- **The direction matters more than the count.** Of those 322 failures,
  **161 were `diagnostic lines` and 161 `transcript`, and not one was
  `bytes written`, `step signature` or the arena sweep.** 760 lines the blob
  printed and we did not; **zero** lines we printed and the blob did not. The
  object left behind was byte-identical in all 272 step cases. The
  reconstruction was not wrong about the machine; it was silent about the
  trace (finding 570). When the repair went in, spurious went 0 → 4 → 0, and
  those four *were* the defect (findings 571 and 573). The argument was made a
  third time when a digit was transcribed onto the wrong one of two adjacent
  messages of the same arm: **two missing lines and two spurious ones, which
  cancel in any count of differing cases**, so a count alone read it as two
  cases short and said nothing about why (finding 650). Count both directions;
  the one that should always be zero is the one worth watching.

### What a trace gap is actually made of

Of 760 missing lines, **204 (27%) were calls not made** — helpers that already
existed and were already correct, whose bodies three arms had inlined verbatim
instead of calling. One of them, `v34FreezeEcho`, was **written, correct and
dead**: no caller anywhere in `src/` (finding 572).

Finding 574 read the other 73% as *behaviour* nobody had written, and **that
reading was wrong.** When the lines were finally written, not one of them
needed a field, a header, a foreign file or a function this tree did not
already have: the arms were reconstructed, byte-exact and mutation-tested, and
what was missing was the reporting threaded through them (finding 650, which
closes finding 574 and says so in its header). The honest statement is
narrower and more useful than the prediction: **a trace gap is a gap in what
is said, and it is not evidence of a gap in what is done** — the placement of
each line, not its arguments and not the code around it, was the whole job.
See *Make the measurement one command*, below, for what it took to zero.

Two facts held together hid the defect for three batches: the diagnostics were
off, and the reason recorded for their being off was a harness limitation. The
check that settled it — disable the fix and count — took one build.

**When a fix is committed for a mechanism nobody has run, the first thing to
measure is not whether it helps but how much of the gap it can possibly
account for** (finding 570).

### Make the measurement one command

```
$ V34TX1_TRACE=1 ./build/test/t_v34hstx1 | grep -c '^=@= '
161
```

A gap that has to be re-derived from scratch is a gap nobody measures twice.
The binary fails under that variable **by design** — the failure *is* the
measurement — so it is not in the phase gate, and unset the file behaves
exactly as before. Shown to fire in both directions: 0 without it and a
passing run, 161 with it (finding 570). Whoever closes part of the gap watches
that number fall; 161 → 137 was predicted before anything was edited, and
landing anywhere above it would have meant a family did not fully close
(finding 571).

**It was watched all the way down.** From 137 at a later fork point: 83 after
one arm's rate-selection lines, 25 after ten more arms, then **0** — with zero
lines printed that the blob does not at any intermediate state, and the object
byte-identical in all 272 step cases throughout (finding 650). A number built
as a measurement, and taken to zero, is the strongest argument available for
building one.

Taking it to zero is also what disproved finding 574's reading of the gap, two
sections above. The placement of each line was the whole job, and placement is
the part the message text will mislead you about:

> **Place a diagnostic on the branch that reaches it, never on what the
> message says.** Two of them would have gone elsewhere on their text: one
> reads as a particular arm's report and is reached from a guard that every
> case falls back to, and placing it where it reads left the count four cases
> short; another shares a name with a message fifteen lines away in `.rodata`
> that carries one extra character, and transcribing that character onto both
> produced **two missing lines and two spurious ones** (finding 650).

## 5. Spec conformance -- the only oracle that can indict the OBJECT

**Every tier above measures agreement with the blob. None of them can tell you
whether the blob is right.** The differential tier compares our output to the
object's; the codegen tier compares our instructions to the object's; mutation
asks whether our tests can tell our code from a broken copy of it. Give all
three a function that faithfully reproduces an object which mis-implements the
standard, and all three go green.

So where an ITU-T Recommendation states how a function must WORK, that function
gets an explicit test against the SPEC, beside its differential test and not
instead of it. Use the Recommendation's own test vectors where it provides
them; where it does not, say so in the test rather than leaving it implied, and
use a published known-answer for the algorithm, deriving the variant from the
spec's stated bit order rather than assuming one.

**The CRC is the worked case.** V.90 never defines its own: every place it
mentions one says "The CRC generator used is described in 10.1.2.3.2/V.34", and
that clause fixes four things a reconstruction can get plausibly wrong ---

    polynomial x^16 + x^12 + x^5 + 1
    the shift register is loaded with ALL ONES before anything is shifted in
    the contents are output starting with bit 0, and bit 0 of the CRC is the LSB
    the CRC covers every information bit in the sequence EXCEPT the frame sync
        bits, the start bits and the fill bits

--- of which the last is the one a differential test can never see, because
both sides skip the same fields whether or not those are the right fields.

**A conformance failure is a DEVIATION, never a licence to change the
reconstruction.** The reconstruction must match the object; that rule does not
bend for a spec. Record it in `docs/deviations.md` with the clause quoted and
the side named, exactly as D920 and D923 do -- D920 was settled this way, by
reading the object's instructions and then the ITU text and concluding that the
blob was faithful at one site and defective at another. A fix goes behind
`DSPLIB_REPRODUCE_BUGS`, with `src/dsp/fpm_div.c` as the pattern.

This is a fifth oracle and not a refinement of the fourth: it is the only one
whose failure says something about the ORIGINAL rather than about us.
