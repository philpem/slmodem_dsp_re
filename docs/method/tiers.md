# The four oracles, and what each cannot see

Every number here cites `docs/findings.md` in this tree. Nothing in this file
is advice that was not measured.

The four tiers do not form a hierarchy. **Neither of any two subsumes the
other** (finding 613), and each has a blindness that is structural rather than
a matter of writing more tests. Knowing which tier can possibly see a given
class of defect is most of the value; the rest is running them.

| tier | decides | structurally blind to |
|---|---|---|
| differential | correctness over the reachable input domain | anything that does not change an output |
| mutation | whether a claim is tested at all | whether the claim is *right* |
| codegen vs. period compiler | what the compiler was forced to encode | anything the compiler was free to choose |
| transcript / diagnostics | what the two sides *say* | anything that prints nothing |

---

## 1. Differential — the only thing that decides

Reconstruction and blob linked into one binary, driven from one deterministic
input, outputs compared sample by sample. Any test disagreeing with the blob
is a hard failure, never a tolerance to widen.

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

**What it says when you finally write the numbers down.** Recording all 48
suites at once gave **37 mutations NOT CAUGHT across 10 of them**, plus one
unusable — including `v34k56` at 15 caught against 10 not, so **40% of what
that suite claims to check is unchecked**. Nobody could have known: the number
existed only in the scrollback of whichever session last ran it (finding 545).

---

## 3. Codegen against the period compiler

Not "does it behave the same" but "did the same compiler, given our source,
emit what the original's compiler emitted". It reaches exactly the class tier 1
cannot, and pays for itself the first time it does.

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

### Count matches, not bytes

The size ratio is the weak number and it moves when you emit more code, not
only more of the right code. `-O3` took it from 77.3% to 89.0% while the count
of byte-identical functions stayed **flat at 92 across that entire 12-point
swing** — our code grew 17 KB, we were undershooting, and the gap closed
arithmetically (finding 616). Every one of `-O3`'s extra matches came from
`-frename-registers` alone; `-finline-functions`, `-funswitch-loops` and
`-fpeel-loops` added 17 KB and **not one additional match**.

Also: a mnemonic comparison is not a byte comparison. Two functions storing
the same constants to different offsets both read as `mov mov mov`.

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

### Two operational facts

- **It needs a green baseline to run at all.** Turning the diagnostics on made
  the suite fail 322 of 23,295 checks, and the mutation runner correctly
  refuses a baseline that is not green — every mutation would report CAUGHT for
  a reason unrelated to the mutation. So the count the previous batch had asked
  for *did not exist* (finding 570).
- **The direction matters more than the count.** Of those 322 failures,
  **161 were `diagnostic lines` and 161 `transcript`, and not one was
  `bytes written`, `step signature` or the arena sweep.** 760 lines the blob
  printed and we did not; **zero** lines we printed and the blob did not. The
  object left behind was byte-identical in all 272 step cases. The
  reconstruction was not wrong about the machine; it was silent about the
  trace (finding 570). When the repair went in, spurious went 0 → 4 → 0, and
  those four *were* the defect (findings 571 and 573).

### What a trace gap is actually made of

Of 760 missing lines, **204 (27%) were calls not made** — helpers that already
existed and were already correct, whose bodies three arms had inlined verbatim
instead of calling. One of them, `v34FreezeEcho`, was **written, correct and
dead**: no caller anywhere in `src/` (finding 572). The other **73% was code
nobody had written**, and no amount of re-reading the file produces it
(finding 574).

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
