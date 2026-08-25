# Grade 2, and whether a machine can decide it

*An investigation, not a change. Nothing in `src/`, `include/` or `test/` was
touched. The one artefact is `tools/eqtriage.py`, a triage aid in
`extcheck.py`'s sense — never a gate.*

Every number here was measured in this worktree at `3ad9ea22`, against
`ref/slmodemd/dsplibs.o` (sha256 `1f3e56d0…`, 1,233,728 bytes) and
`build/tc_out` built by `tools/toolchain/build.sh` on `dsplibs-tc342` — GCC
3.4.2 exact, 200 objects, 0 failed. A number from either tool is meaningless
without the compiler beside it, so: **GCC 3.4.2, `-O3`, the flag set in
`build.sh`.** The denominator throughout is **1,200 symbols the blob and our
build both define**, which is `byteident.py`'s and `compare.py`'s denominator
exactly.

---

## The recommendation

**No to lift-and-solve. Yes to two bounded repairs of the tool we already
have.**

Do not build an equivalence oracle on Ghidra P-Code, RetDec LLVM IR, angr/VEX,
Miasm or ESIL. The measurement below shows the dilemma is structural rather
than a defect to wait out: **on Ghidra — the only lifter installed, and the one
this tree already pins — there is no rung on the ladder that is both faithful
and more normalised than what the tree already compares.** The faithful rung
(raw P-Code) is a 1:1 transliteration of the instruction stream and is
*harder* to compare than the instructions `byteident.py` compares today. The
comparable rung (high P-Code, and the C above it) silently discarded an entire
arithmetic operation and collapsed an 80-bit intermediate to 32 bits in a
seven-instruction function — the exact distinction findings F1453 and F6203 turn
on.

Instead, do these two things, which need no new dependency and no new trust:

1. **Fix `byteident.py`'s relocation normaliser. It has never fired once.**
   This is not a tidy-up: it means the published grade split is wrong today,
   in the direction of overstating difference. §1.
2. **Relax its register bijection from whole-function to live-range.** That
   plus (1) accounts for the entire mechanically-decidable population of the
   same-size bucket — 47 of 170 functions — with no oracle anybody has to
   believe. §2.

The honest concession is in §3: for 96 functions (8.0% of the tree) a lifted
comparison would be sound in principle and nothing today measures them. The
reason to decline anyway is in §5.

---

## F1. Before any of that: the grade split is wrong today

`byteident.py` grades in two passes. `body()`/`verdict()` decides EXACT,
UNRESOLVED, RELOC, BYTES and SIZE at the byte level; anything that fails is
then re-tested by `insns()`/`alpha_equal()` for grade 1. The two passes
normalise relocations separately, and **the second one does not work.**

`insns()` intends to rewrite a relocated operand to its relocation *target*,
so that two instructions relocated against the same thing compare equal — its
comment says so at length, citing finding F604. The patch loop is:

```python
for line in out.splitlines():
    m = RELOC.match(line)
    if m and rows:
        rows[-1][1] = re.sub(..., "@" + m.group(3), rows[-1][1])
```

`rows` is already complete when this runs, so `rows[-1]` is the **last
instruction of the whole function**, not the relocation's own instruction —
usually `ret`, whose operand is empty, so `re.sub` is a no-op. Every
relocation in the function patches the same empty string.

**Measured, over the first 400 blob symbols: 158 carry at least one
relocation, 210 relocations in all, and `insns()` emitted the `@` marker for
zero of them.** On `dp_v23_exit` — two relocations, `R_386_32 .data` on
`mov $0x60,%eax` and `R_386_PC32 modem_dp_deregister` on the call — it emits
none. `tools/eqtriage.py --selftest` prints that comparison on every run.

This is finding F134's defect inside the tool that defines grades 0 and 1, and
it is the fourth normalisation artefact in that tool's history after absolute
branch targets, section-vs-symbol relocations and relocated displacements.
Like all three of those, it inflates the difference count.

### What it costs, in both directions

`verdict()` — the correct pass — partitions the 1,200 as
**EXACT 393, UNRESOLVED 5, BYTES 170, RELOC 3, SIZE 629.** What `byteident.py`
prints today, and what it would print with the normaliser repaired:

| bucket | printed today | corrected |
|---|---|---|
| grade 0 EXACT | 393 | 393 |
| UNRESOLVED | 5 | 5 |
| grade 1 REGALLOC | **10** | **18** |
| RELOC | **0** | **3** |
| BYTES | **163** | **152** |
| SIZE | 629 | 629 |
| **grade 0 or 1** | **408 (34.0%)** | **416 (34.7%)** |

Eleven functions are wrongly *excluded* from grade 1. Three are wrongly
*included*, and those three are the worse half: `FPM_FSM_init`,
`V90PreFilter::~V90PreFilter` (D1 and D2) have a **differing relocation
target** — they reference something different — and are today certified as
"same instructions and operands under a register bijection". `RELOC` printing
`0` is not a clean tree; it is three real differences promoted to grade 1 by a
comparison that could not see their operands.

**The repair is the single-pass STRUCTURE**, not a new substitution: run the
patch in the same pass that builds the rows, so a relocation lands on the
instruction objdump printed it after. `tools/eqtriage.py`'s `insns()` does
exactly that and can be lifted across.

> **Whoever lifts it must keep the substitution literal-only.** The
> replacement text is `@<target>` in place of the operand's *numeric
> literals*, leaving registers, direction and addressing mode standing. This
> tool replaced the whole operand for one revision and thereby collapsed
> `mov %eax,glob` and `mov glob,%eax` — a store and a load — into one row.
> That over-normalisation moved REGX 29 → 33 and MIXED 24 → 20 before it was
> caught; §9. There is exactly one case where the whole operand must go: a
> **bare-hex** operand, which is a relocated `call`/`jmp` whose target objdump
> prints as the next address (addend −4), so `call foo` and `call bar`
> otherwise normalise to the same self-relative offset and compare equal. Two
> rules, and the second closes an over-normalisation sitting beside the
> under-normalisation.

> I did not apply this. `tools/toolchain/byteident.py` was out of scope for
> this investigation by instruction, and it is another session's file.

---

## F2. The ceiling, re-measured

The brief this began from said a better comparator "can only ever reclassify
the 163" same-size functions. That is true of a comparator that aligns bytes
or instructions, and **false of a lifted comparison, which is not bound by
length** — which is the whole argument for lifting. So both buckets are
measured here. Non-exact is **792 of 1,200 (66.0%)**: 163 same-size plus 629
different-size, as printed today.

> **792 and 799 are both right and they count different things.** 792 is the
> complement of grade 0 or 1 *as printed today* (1,200 − 408). `eqtriage.py`
> prints 799, which is `verdict()`'s BYTES 170 plus SIZE 629 — it excludes the
> 3 RELOC-verdict rows and includes the 7 that today's broken `alpha_equal`
> promoted out of BYTES. Anyone reproducing a run sees 799 in the banner and
> 792 in this paragraph; neither is a typo.

`tools/eqtriage.py` classifies the same-size bucket exactly, because the two
sides align instruction-for-instruction. It reports the *shape* of the
different-size bucket and classifies nothing there, because no alignment
exists.

### F2a. The same-size bucket: 170 by `verdict()`, and what it is made of

The A/B is the point. `--raw` classifies `byteident.insns`' rows unmodified;
the default classifies correctly-normalised rows. **Only the classification
changes between the two columns** — the x87 and loop columns are properties of
the function and are computed the same way in both, deliberately, because
taking them from the raw rows made the loop column read a silent zero.

| class | `--raw` | corrected | what it is |
|---|---|---|---|
| ALPHA (grade 1) | 7 | **18** | already alpha-equal; byteident's test could not see it |
| REGX | 14 | **29** | only register *names* differ, under no one consistent bijection |
| DISP | 6 | 8 | a displacement differs |
| MIXED | 52 | **24** | more than one kind |
| SCHED | 41 | 41 | same mnemonic multiset, different order (617's class) |
| SHAPE | 17 | 17 | different mnemonics, same byte count |
| LEN | 33 | 33 | same bytes, different *number* of instructions |
| **movable** | **21 (12.4%)** | **47 (27.6%)** | free or unresolvable |
| **needs reading** | 58 (34.1%) | 58 (34.1%) | SCHED + SHAPE |
| **real difference** | 91 (53.5%) | **65 (38.2%)** | DISP + MIXED + LEN |

The dead normaliser was overstating "real difference" by 91 against 65 and
shrinking the movable population by a factor of 2.2. MIXED — the largest
"real" class — collapses 52 → 24 once relocated operands are compared by
target.

**The movable 47 need no lifter, no IR and no solver.** 18 are grade 1 the
moment the normaliser works. The other 29 are pure register allocation, which
`CLAUDE.md` rules FREE, and they fail grade 1 only because `alpha_equal`
demands *one bijection for the whole function* where the allocator makes
different choices in different live ranges. Read the diffs and there is nothing
else in them:

```
_ZN9V92Mapper5resetEsh    mov $0x40a00000,%ecx      |  $0x40a00000,%eax
                          mov %ecx,0x28(%ebx)       |  %eax,0x28(%ebx)
_ZN8FloatFIR5resetEv      mov 0x8(%ebx),%edx        |  0x8(%ebx),%ecx
                          sub %edx,%eax             |  %ecx,%eax
V34EchoCleanUp            mov $@.rodata.str1.4,%ebx |  $@.rodata.str1.4,%edx
                          mov %ebx,0xc(%esp)        |  %edx,0xc(%esp)
findMinValueIndex         mov 0x604(%ecx),%ebx      |  0x604(%ecx),%esi
                          movzbl 0x4(%ecx),%esi     |  0x4(%ecx),%ebx
```

`V34EchoCleanUp` is the one that shows why §1 matters: the relocation target
is the *same* on both sides and only the destination register differs, which
is visible only once relocated operands are compared by target. The last is
two registers exchanged wholesale — a bijection, but one `alpha_equal` cannot
express because `%ebx`↔`%esi` holds over part of the body and the identity
holds over the rest. A per-live-range bijection is a bounded change to a tool
that exists, and it is the entire mechanically-decidable population of this
bucket.

That leaves **58 that need reading** (SCHED, SHAPE) — the class finding F2900
hand-classified 69 of at a smaller denominator, with 2901/2902/2903 as the
verdicts — and **65 that are real differences a checker could only be wrong
about.** No oracle changes those numbers; they are what the bucket contains.

x87 appears in 19 of 170 (11.2%); 73 of 170 (42.9%) have neither x87 nor a
loop.

### F2b. The different-size bucket: 629, and it is not "the same thing spelled
differently"

This is the half the brief's re-framing is about, and the half nothing
measures. Here is its shape.

| |blob − ours| | n | of 629 | x87 | loop | same mnemonic multiset |
|---|---|---|---|---|---|
| within 4 bytes | 137 | 21.8% | 35 | 98 | 2 |
| 5–16 | 158 | 25.1% | 32 | 119 | 0 |
| 17–64 | 183 | 29.1% | 36 | 153 | 0 |
| 65–256 | 105 | 16.7% | 16 | 97 | 0 |
| over 256 | 46 | 7.3% | 13 | 45 | 0 |

**The load-bearing number is the last column: 2 of 629 (0.3%).** If these were
mostly the same computation factored differently — different spill decisions,
a different schedule, one side inlining what the other calls — the mnemonic
multisets would agree far more often than that. They do not. These are
genuinely different instruction streams, not different spellings of one.

The rest of the shape:

- **loops in 512 of 629 (81.4%)**, x87 in 132 (21.0%). Only **96 (15.3% of the
  bucket, 8.0% of the tree)** have neither.
- ours is **smaller in 382** and larger in 247. Over the bucket the blob is
  457,492 bytes and ours 381,203 — a **76,289-byte deficit**.
- That deficit is *not* the missing diagnostics. Only **22 of 629 functions
  (3.5%)** have a debug-call deficit at all, 293 calls in total, and
  `v34handshak` alone accounts for 241 of them and **52,518 of the 76,289
  bytes**. Excluding it: **23,771 bytes over 628 functions**, about 38 bytes
  each.

So the bucket is one very large unfinished function plus a long tail of
functions that are close but not the same code. A lifted oracle asked about
this population would be asked, 81% of the time, to decide equivalence of
loops — which needs an invariant or a bound, not a solver call — and 21% of
the time to reason about x87, where §3 shows the available tool is wrong.

---

## F3. x87, measured

`CLAUDE.md` records Ghidra's floating-point modelling as weak and explicitly
**unmeasured here**; `tools/decompile.sh`'s header says the same and records
that the one attempt picked `V34EchoFilter`, which turned out to be
fixed-point shorts. This section measures it. Ghidra **11.4.2**, the pinned
version, installed and working.

### The probe, with ground truth measured rather than asserted

Finding F6203's narrowing, minimised: a product rounded to 32 bits and read
back, against the same product left on the x87 stack. Compiled by the period
compiler with the tree's own flags. One is 6203's `fstps`/`flds` pair
exactly:

```
narrow:  flds 0xc(%esp); fmuls 0x8(%esp); fstps (%esp); flds (%esp); fsubs 0x10(%esp)
wide:    flds 0x8(%esp); fmuls 0x4(%esp);                            fsubs 0xc(%esp)
```

**They differ on 1,999,998 of 2,000,000 random inputs (100.00%)**, first
witness `a=2.72150183 b=-0.844936609 c=2.26479387` giving `narrow=-4.56429005`
against `wide=-4.56429052`. The probe is a probe. (A plain `float` local does
*not* produce the narrowing on GCC 3.4.2 at `-O3` — both spellings compile to
identical bytes — which is 6203's point about neither compiler being obliged
to narrow, confirmed in passing.)

### What Ghidra returns

```c
float narrow(float param_1, float param_2)
{ return param_2 * param_1; }                       /* the fsubs is GONE.
                                                       so is param_3. */

longdouble wide(float param_1, float param_2, float param_3)
{ return (longdouble)param_2 * (longdouble)param_1 - (longdouble)param_3; }
```

A seven-instruction function, and the decompiler dropped an arithmetic
operation and a parameter, silently, with no diagnostic.

### Two controls isolate it

**One opcode byte.** Three functions identical except `d8 64` (`fsubs`),
`d8 44` (`fadds`), `d8 74` (`fdivs`):

| | decompiled |
|---|---|
| `nsub` | `return param_2 * param_1;` — **operation lost** |
| `nadd` | `(longdouble)(param_2 * param_1) + (longdouble)param_3` — correct |
| `ndiv` | `(longdouble)(param_2 * param_1) / (longdouble)param_3` — correct |

**The same shape in integers.** `inarrow`/`iwide` — same `sub $0x4,%esp`, same
volatile round trip through `(%esp)`, same `pop`, same `0x10(%esp)` third
parameter — both recover their arithmetic correctly (`inarrow` as
`CONCAT44(param_2 * param_1, param_2 * param_1 - param_3)`, a return-width
wart with the value right).

So the fault is the floating-point path, and it fires on one of three sibling
opcodes. That is worse for an oracle than a uniform weakness would be: a
uniform weakness can be bounded and declared, and this cannot be predicted.

### And the structural result, which outlives the bug

Dumping P-Code at both levels (`--- RAW ---` is the per-instruction lift,
`--- HIGH ---` is after the decompiler's analysis):

**Raw P-Code is faithful.** It models the x87 register stack as an explicit
rotation of 10-byte registers, every narrowing as `FLOAT2FLOAT`, and the
arithmetic at 80 bits:

```
FMUL  float ptr [ESP + 0x8]
      (register, 0x1100, 10) FLOAT_MULT (register, 0x1100, 10) , (unique, 0x5d500, 10)
FSTP  float ptr [ESP]
      (unique, 0x5580, 4)    FLOAT2FLOAT (register, 0x1100, 10)     <- the narrowing
FSUB  float ptr [ESP + 0x10]
      (register, 0x1100, 10) FLOAT_SUB (register, 0x1100, 10) , (unique, 0x60700, 10)
```

**High P-Code is not:**

```
nsub:  (unique, 4) FLOAT_MULT (stack, 0x8, 4) , (stack, 0x4, 4)   <- 4 bytes, not 10
       RETURN                                                      <- no FLOAT_SUB at all
nadd:  (unique, 4) FLOAT_MULT ...; FLOAT2FLOAT; FLOAT_ADD (10) ...  <- correct
```

Both failure modes the brief predicted, in one function: the 80-bit
intermediate is **normalised away** (10-byte `FLOAT_MULT` → 4-byte), and an
operation is **lost outright**.

> **This is the finding, and it survives Ghidra fixing `fsubs` tomorrow.** Raw
> P-Code is faithful *because* it is a 1:1 transliteration — nine P-Code
> operations for `SUB ESP,0x4`, flag computation included. Comparing it
> between the blob and our build is strictly harder than comparing the
> instruction streams `byteident.py` already compares: it would report a
> difference for every register the allocator chose differently, and every
> flag neither side reads. The level that is *comparable* — that has run SSA,
> type propagation and dead-code elimination, and can therefore see past
> register allocation — is the level that discarded the operation. **There is
> no rung that is both faithful and more normalised than what the tree
> already has.**

That is the general shape of the proposal's problem. A lifter's normalisation
is not tuned to "differences that change neither the result nor the
execution"; it is tuned to producing readable code, and its job is to make
different things look the same. That is the opposite of what grade 2 needs.

---

## F4. Is a lifter-as-oracle even allowed here?

`CLAUDE.md` says **"Ghidra is scaffolding, never evidence"**, and the rule as
written forbids something specific: *"No name, comment or finding is ever
written from decompiler output"*, because "a Ghidra guess recorded as a
derivation corrupts the record". Using a lifter as a comparison oracle is a
different use, and it deserves to be argued rather than assumed either way.

**It is not forbidden by that rule, and it is not exempt from it either.** The
rule is about the *provenance of recorded claims*, and a boolean is still
provenance. If an oracle's `EQUAL` closes a function, or lands in
`docs/findings.md` as the reason something was left alone, then decompiler
output has become evidence — laundered through a one-bit interface, which
makes it harder to audit than a sentence would have been, not easier.

The tree already has the right precedent and the right words for it.
`extcheck.py` is **"the signedness detector, and a triage aid, never a gate"**,
and every hit "must be traced against `dis.py` before anything is retyped" —
one report in five is real (finding F2402). The same settlement applies:

> **An equivalence oracle may narrow a list a human then reads in `dis.py`. It
> may never close a function, and no finding may cite it as a derivation.**

Under that rule a lifted oracle is *permitted*. §5 is why it is still not
worth building.

---

## F5. What it would cost, and why I still decline

### What is installed — the brief was wrong about this

| | status |
|---|---|
| **Ghidra 11.4.2** | **installed**, `~/ghidra/ghidra_11.4.2_PUBLIC`; also `12.1_DEV`, `12.2_DEV` |
| docker + `dsplibs-tc342`, `dsplibs-tc`, `dsplibs-tc342-gentoo` | installed |
| unicorn 2.0.1, pyelftools 0.30 | installed |
| angr, pyvex, archinfo, claripy, z3, miasm, capstone, keystone, pyghidra, triton | absent |
| rizin, r2, radare2, retdec-decompiler | absent |

Ghidra is simply not on `$PATH` — `tools/decompile.sh` defaults `$GHIDRA` to
its directory and finds it. **Every Ghidra measurement in this document was
taken with what is already on the machine, at zero installation cost.** That
is also why the Ghidra arm is the one that got measured and the others did not.

### What the rest would cost

- **angr / pyvex / claripy / z3** — `pip install angr` into a venv, no root,
  roughly 400–600 MB resolved. Feasible without disturbing the machine. Not
  attempted: §3 settled the question before the cost was worth paying, and
  VEX has the same structural problem as P-Code — it is faithful and therefore
  unnormalised, and angr's x87 support is thinner than Ghidra's, not thicker.
- **rizin / radare2 (ESIL)** — distro packages, root, or a container.
- **RetDec** — a multi-hundred-MB build or release tarball.
- **The right way to host any of them is a container**, and the brief is right
  about that. **It must be a separate image, not the compiler image.**
  `dsplibs-tc342`'s entire value is that its output is byte-comparable to the
  blob's and that it prints the blob's `.comment` back; adding a package to it
  invalidates every A/B in findings F2200, F2320 and F2500. A `Dockerfile.analysis`
  beside the others, driven the way `build.sh` drives the compiler, costs
  nothing and keeps the pin intact.

### The decision

The cost is not the obstacle. The obstacle is that the resulting oracle would
have to be trusted on the 8.0% where it is sound while being demonstrably
wrong on the 21% of the same bucket that contains x87 — and a tool trusted
selectively, by a rule no gate can check, is precisely what `CLAUDE.md` calls
a tolerance to widen. The concession is real and I am making it explicitly:
**for the 96 no-x87, no-loop different-size functions, a lifted comparison
would be sound in principle and nothing today measures them.** A verdict of
"our source is right and factored differently, stop chasing the codegen" would
retire real work, and `CLAUDE.md`'s own warning that 100% on `compare.py` is
not the target is currently unfalsifiable. That is the strongest argument for
building this, and it is worth 8.0% of the tree, not 66%.

Against it: the 96 are the *residue* of a bucket whose defining measurement is
that 0.3% of it shares a mnemonic multiset. They are different code. The prior
that they are equivalent-but-refactored is weak before any oracle runs, and an
oracle's job here would mostly be to confirm differences the tree can already
see.

### unicorn, named once

`unicorn` 2.0.1 is installed, and it is the only option that fits this tree's
epistemology: concrete execution of the real bytes on QEMU's soft-float x87,
producing a **witness** for DIFFERENT. Its limit is the one that matters:
**it refutes and cannot confirm.** Grade 2 is a universal claim and "same over
N inputs" is not one — that is the same gap between `narrow` and `wide`, which
agree on 2 inputs in 2,000,000. And `make period` already does concrete
differential execution, against the real linked objects with real fixtures.
Unicorn's only unique reach is functions the harness has no fixture for. Worth
remembering; not worth building now.

---

## F6. Why BinDiff and Diaphora are the wrong tool, specifically

They answer **"how similar"**, and produce a similarity score with a matching
as the real output. Two reasons that is the wrong question here:

- **The matching problem is already solved.** Both objects carry symbols;
  `byteident.py`'s denominator *is* the 1,200 names both define. The expensive
  half of what those tools do is work this tree does not need.
- **A score needs a threshold, and a threshold is a tolerance.** `CLAUDE.md`:
  any test disagreeing with the blob is "a hard failure whatever build it came
  from — never a tolerance to widen". A grade is a partition, not a ranking;
  "0.94 similar" has no reading in it.

---

## F7. If someone builds one anyway: the validation it must pass

An equivalence oracle is a detector, and finding F134's rule binds it: **it must
be shown to fire, and it must report its denominator** (findings F2400, F2401,
F3100). But an *equivalence* detector needs its controls in both directions,
because both under- and over-normalisation are live failure modes here —
`byteident.py`'s history is three under-normalisations, and §1 is a fourth,
while §3 is an over-normalisation in the same function as an outright loss.

Minimum, all with the denominator printed:

1. **SAME** on a grade-0 function against itself.
2. **SAME** on that function with registers permuted — the tool doing its job.
3. **DIFFERENT** on that function with one immediate perturbed by 1.
4. **DIFFERENT** on `narrow` against `wide` from §3, which differ on 100.00% of
   2M inputs. *Any oracle that returns SAME here is disqualified*, and this is
   the cheapest disqualifier available: it is seven instructions.
5. **DIFFERENT** on the §3 triple `nsub`/`nadd`/`ndiv`, which are one opcode
   byte apart.

Controls 4 and 5 exist as compiled objects and are reproducible from the
sources quoted in this document with `build.sh`'s flags.

**How to tell a real equivalence from a normalisation artefact**, which is the
question the brief asked and the hardest one here: by requiring that the
normalisation be *stated and separately falsifiable*, not inferred from the
verdict. Every correction in `byteident.py`'s history — absolute branch
targets, section-vs-symbol relocations, relocated displacements, and now the
dead patch loop — was found by asking "what did this tool rewrite, and can I
see it rewrite it?" `tools/eqtriage.py --selftest` answers that for its own
normaliser by printing `this tool 2, byteident.insns 0` on a function with two
relocations. An oracle whose normalisation cannot be printed and counted
cannot be audited, and the answer to "is this a real equivalence" will not be
available when it matters.

---

## F8. `tools/eqtriage.py`

The one artefact. A triage aid, never a gate; it decides nothing and grades
nothing.

```
tools/eqtriage.py                 both buckets, the tables above
tools/eqtriage.py --class REGX    one class, with its per-instruction diffs
tools/eqtriage.py --diff SYM      the aligned diff for one symbol
tools/eqtriage.py --raw           byteident's normalisation, for the §2a A/B
tools/eqtriage.py --selftest      ten classifier controls + the normaliser A/B
```

It reuses `byteident.py`'s `sizes`, `body`, `verdict` and `alpha_equal`
unchanged, so its buckets are the same buckets. It disassembles for itself
only because of §1. About 30 s over the tree.

---

## F9. What was tried and did not work, and what remains unmeasured

- **A plain `float` local as an x87 probe.** GCC 3.4.2 at `-O3` compiles
  `float t = a*b; return t-c;` and `return a*b-c;` to **identical bytes** — no
  narrowing. The probe had to use finding F6203's own `volatile float` round
  trip to produce a binary containing `fstps`/`flds`. Consistent with 6203;
  it cost a build to learn.
- **I made the over-normalisation mistake myself, in the tool written to
  measure over-normalisation.** The first `insns()` here replaced a relocated
  instruction's *whole operand* with `@<target>` instead of only its numeric
  literals. That is tidier to write and it collapses `mov %eax,glob` with
  `mov glob,%eax` — a store and a load, against the same symbol, reading as
  one row. It inflated the headline: **REGX 33 and MIXED 20, against a true 29
  and 24; movable 51 against a true 47.** The direction is the point — my
  error made the tree look *more* equivalent than it is, which is the failure
  mode a grade-2 oracle has by default and the reason this document declines
  to build one. It took one revision to introduce and was invisible to a
  ten-control self-test, because every relocated control fed operands that
  were *already* bare `@target` strings and so never exercised what the
  substitution discarded. The eleventh control — a relocated load against a
  relocated store, which must be a difference — fails on the old code and
  passes on the new. **A control that cannot fail is not a control**, which is
  the same lesson as `service.py`'s self-check in finding F3111.
- **`byteident.RELOC.search(out)` on multi-line output.** My first attempt to
  count relocations reported 0 of 400 symbols, because `^` without
  `re.MULTILINE` only matches at position 0. It looked like a confirmation of
  the bug I was chasing and was my own. Re-run per line: 158 of 400. Worth
  recording as the shape of the trap — a broken measurement that agrees with
  your hypothesis is the one you will not check.
- **Not measured: RetDec, angr/VEX, Miasm, ESIL.** §3's argument is
  structural and I expect it to carry, but it was measured on Ghidra only.
  The specific claim "raw IR is faithful, analysed IR is not, and nothing in
  between is both" is a Ghidra measurement generalised by reasoning. If anyone
  revisits this, controls 4 and 5 in §7 cost minutes and settle it per tool.
- **Not measured: whether the 58 SCHED/SHAPE rows contain a reorder our
  source's data dependencies would forbid**, which finding F614 calls the sharp
  version and the only real defect in that class. `--class SCHED` lists them;
  none of finding F2900's 22 showed one.
