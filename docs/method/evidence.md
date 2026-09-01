# What our evidence PROVES

*An audit, not a change. Nothing in `src/` or `include/` was touched. The
artefacts are `tools/eqproof.py` and `tools/eqproof-checks.sh`, triage aids in
`extcheck.py`'s sense — never gates.*

The question this answers, asked directly:

> **Where we've reimplemented something — if it's not byte-exact, prove it
> implements the exact same function.**

That is a claim about SEMANTICS. This tree measures BYTES well and has never
asked it systematically. `docs/method/equivalence.md` is the sibling document
and it settled the other route: on Ghidra, the only lifter installed, **there
is no rung that is both faithful and more normalised than the instruction
streams `byteident.py` already compares**, so an equivalence oracle is declined
and this document does not relitigate it. What is left is coverage of the
differential tier, and that is what is measured here.

Every number is from this worktree at the commit this document lands in,
against `ref/slmodemd/dsplibs.o` (sha256 `1f3e56d0…`, 1,233,728 bytes) and
`build/tc_out` from `tools/toolchain/build.sh` on `dsplibs-tc342` — **GCC 3.4.2
exact, `-O3`, 206 objects, 0 failed**. The denominator throughout is
**1,297 symbols the blob and our period build both define**, which is
`byteident.py`'s denominator exactly.

---

## The answer in four numbers

| | | |
|---|---|---|
| **591** | 45.6% | **grade 0** — byte-identical. The question does not arise. |
| **32** | 2.5% | **grade 1** — and it is a **PROOF**, verified against four named holes, 32 of 32 clean. §1. |
| **674** | 52.0% | **neither**, so each needs a coverage argument. §2. |
| **2** | 0.3% of the 674 | are exhaustive over a whole input domain. **666 are SAMPLED.** |

**The weak class dominates, it will go on dominating, and that is the answer
rather than a defect to fix.** §4 is why, and it is a wall rather than a
gradient.

> `CLAUDE.md` says "433 of 1,251 at grade 0 and 486 at grade 0-or-1 today". The
> tool says 591 and 623 of 1,297. The paragraph carrying the stale numbers also
> says "read the number from the tool, not from here", so the rule was written
> and the numbers under it drifted anyway — F6100/F6103's family, fifth member.
> Finding F9495.

---

## 1. Grade 1 is a proof — for these 32, and not as a property of the grade

Grade 1 is "the same instructions and the same operands under a register
bijection taken **per live range**". A consistent renaming of registers
preserves semantics, so the standing assumption has been that a grade-1 symbol
is proven by construction rather than by testing. **That assumption is false in
general and true here**, and the difference between those two sentences is the
whole of this section.

### What the grade already gets right, and it is more than it looks

- `%esp` and `%ebp` are **pinned to themselves**, so every stack address and
  every frame offset agrees literally.
- Every immediate and every displacement is compared **literally**.
- A relocated operand is compared by its **relocation target**, so two
  instructions naming different symbols are never equal.
- `%st(N)` survives the register substitution as `%r(N)`, so **x87 stack
  indices are compared literally**: `fsub %st(1),%st` is never equal to
  `fsub %st(2),%st`. **Grade 1 makes no floating-point concession at all**, and
  `equivalence.md` §3 — where Ghidra's high P-Code dropped an arithmetic
  operation outright and collapsed an 80-bit intermediate to 32 bits — does not
  touch it.

### The four holes

Each is a place where two instruction streams are identical under a consistent
renaming and still compute different functions. Each is a synthetic control in
`tools/eqproof.py --selftest`, and **every control is a pair `alpha_equal`
ACCEPTS** — a hazard reported on a pair byteident rejects would be no news.

| | what it is |
|---|---|
| **RET** | `ret` has no operands, so nothing pins the binding at the one point where a value LEAVES the function. `mov 0x4(%esp),%eax; ret` against `mov 0x4(%esp),%edx; ret` is a clean rebind returning a different value. |
| **SAVECLASS** | the bijection may map a callee-saved register onto a caller-saved one. `push %ebx … pop %ebx` against `push %ecx … pop %ecx` matches instruction for instruction, and the second preserves `%ecx` while **destroying the caller's `%ebx`**. |
| **CALLLIVE** | that crossing with a `call` standing in it and the register read afterwards. The blob's `%ebx` survives the call; our `%eax` does not. |
| **LOOPREBIND** | `alpha_why` is a **linear scan with no control-flow graph**. It validates one pass. A back edge means the second pass begins with the bindings the first pass ENDED with. |

### The census

| shape | fires | and |
|---|---|---|
| RET | 16 of 32 | **all 16 return `void`**, so the shape is vacuous. 0 return a value. |
| SAVECLASS | 9 of 32 | none of the nine is live across a call, so none is a defect. |
| CALLLIVE | **0** | |
| LOOPREBIND | **0** | |
| REPLAY | **0** | the replay never once disagreed with `alpha_equal`. |
| **clean** | **32 of 32** | |

**`SAVECLASS` on its own is a precondition and not a defect**, and is reported
because a reader should see how much of the population is even eligible. A
callee-saved register used as scratch inside one live range with no call in it
is interchangeable with a caller-saved one, and both sides save whatever their
own ABI obliges them to — which `alpha_equal` already checks by matching the
`push`/`pop` instructions.

**`REPLAY 0 of 32` is the guard that makes the rest readable.** `eqproof.py`
re-expresses `alpha_why`'s binding loop in order to instrument it, and a second
copy of a comparison is a second answer to the same question. So the replay
reports whether it reached the end without a conflict on every pair byteident
accepted, and a disagreement is a failure of this tool rather than a hazard.

### And all 32 also have differential evidence

**Every one of the 32 is in the DIRECT population too.** The only three symbols
in the whole 1,297 that no test names are `_ZN5V90JdD2Ev`, `_ZN5V92JdD2Ev` and
`GetNextDigitAndReturnNextState`, and none of them is grade 1. So the
by-construction proof and the differential evidence are independent and both
present for all 32.

That matters most where the census leans on something outside the instruction
stream. The sixteen RET hits are cleared by the **return type declared in our
own headers**, which is a source fact rather than an object fact; if that
lookup were wrong for any of them, the differential test on that symbol is a
second line of defence rather than nothing.

### How to quote it

> **Grade 1 is a proof for the current 32, verified against four named holes.**

Not "grade 1 is a proof". The argument is population-specific: the next symbol
that lands at grade 1 has to be re-censused, which is `tools/eqproof.py
--grade1`. The result also rests on a linear replay with a textual map
comparison at loop heads — sound for the hazards as defined, over-approximating
on control flow, which is the direction that can only ever call a proof
incomplete.

### Two of the four probes over-reported first, and the over-reports were mine

The first census read **5 of 32 clean**. Recorded in F9491 because every one of
the four errors made the tree look *less* proven than it is:

- **LOOPREBIND** asked "was there a rebind inside a loop", which fires on an
  identity rebind — the commonest thing in a loop. The sharp question is
  whether the MAP differs between the loop head and the back edge for a
  register the body reads before writing. 16 → 1.
- **And that last one was a back edge that is not a loop.** GCC 3.4.2 puts a
  cold path at the end of a function and jumps back into the main line.
  `_iir_filter_create`: `je +0xb0` at the top, `jmp +0x23` at `+0xbe` once
  `sysdep_malloc` has returned. Textually a backward branch across the
  function; in the CFG a MERGE that runs at most once. `eqproof.py` now builds
  basic blocks and computes dominators and takes only edges `u -> v` where `v`
  dominates `u`. 1 → 0.
- **CALLLIVE** asked "is there a call anywhere". 7 → 0.
- **RET** was folded when it should have been split by return type. 16 → 0.

---

## 2. The 674, and where their evidence comes from

| | | |
|---|---|---|
| **DIRECT** | **673** (99.9%) | a compiled test object references `ref_SYMBOL`, so it is compared **at its own boundary**, on inputs the test chose |
| **COMPOSITE** | **0** | no test names it; our build reaches it from one that is |
| **NONE** | **1** (0.1%) | `GetNextDigitAndReturnNextState` |

**The tree's own rule is upheld, and this is the measurement of it.** "Nothing
is committed that has not passed a differential test" — over the whole 1,297,
**1,294 are DIRECT and 3 are not**. The other two are `_ZN5V90JdD2Ev` and
`_ZN5V92JdD2Ev`, the two F8326 already names and declines at one byte each.

**COMPOSITE reading 0 is a property of the tree and not a dead detector.** The
graph carries **2,919 edges over 1,304 functions**, and `--selftest` takes a
real caller/callee pair out of it, requires the callee to be reachable from the
caller, and requires it NOT to be reachable from no root at all.

> This is also F8326 measured. That finding warns that `coverage.py`'s 100.0%
> counts a symbol tested when a test NAMES its `ref_` alias, so a member
> reached only through a tested dispatcher reads untested. True, and the
> population it costs is **three symbols**, all named above.

Membership is decided from `nm -u` on the compiled test objects, never by
grepping the sources: every test file opens with a block of `extern ref_*`
declarations, so a grep counts a symbol as driven the moment it is *declared*.

---

## 3. What that evidence proves

A symbol lands in the first class it qualifies for, so these partition the 674.

| | | |
|---|---|---|
| **EXHAUSTIVE, whole domain** | **2** (0.3%) | `FPM_div`, `charFlip` |
| **EXHAUSTIVE, reduced domain** | **5** (0.7%) | `FPM_phasor`, `FPM_phasor_demod`, `FPM_phasor_dp`, `RxTrained1200`, `RxTrained2400` |
| **SAMPLED** | **666** (98.8%) | driven, ≥100 checks, no exhaustiveness claimed |
| AD-HOC VECTORS | 0 | **empty by construction** — see the band note below |
| **UNTESTED** | **1** (0.1%) | |

### The five claims were read, and they are two different things

Exhaustiveness cannot be computed. Nothing in this tree can derive a function's
input domain — a function taking a `struct *` has a reachable state space, not
a domain — so it is **asserted by a test and verified by reading it**. All five
`diff_begin` labels claiming it were opened:

| | |
|---|---|
| `charFlip` | 256 of 256 values of one `unsigned char`. **Whole domain.** |
| `FPM_div` | 65,536 of 65,536 values of one `unsigned short`. **Whole domain.** |
| `FPM_phasor` | all 65,536 phases against **fourteen chosen** increments of 65,536. Reduced. |
| `FPM_phasor_dp` | exhaustive over phase; `frac_phase × frac_inc` is 2^32 and is expressly NOT swept — the carry boundary is. Its header says so at length. |
| `RxTrained` | all 3^n sequences over an alphabet of **three** shorts of 65,536: "the two symbols that matter plus one that matters to neither". |

**Every one of the five is honest in its own prose, and none of them can be
told apart from its LABEL.** That is structural rather than sloppiness: a label
names a sweep, and whether a sweep covers a whole domain is a fact about the
SIGNATURE, which no label carries.

A claim is credited only where **its label names the symbol**. Crediting every
symbol a claiming binary drives put 15 in the whole-domain class off the back
of two sweeps — `t_v8util` sweeps `charFlip` over all 256 bytes and drives
fourteen other v8 utilities it does not sweep at all. **18 further test sources
mention exhaustiveness in prose; they are listed by the tool and counted
nowhere**, because such a mention is about one sweep inside a binary driving
many symbols, and crediting them all put 110 of 674 above SAMPLED on the
strength of a comment.

### How many inputs — 628,543,812 checks over 263 binaries

| band | DIRECT symbols | |
|---|---|---|
| under 100 | 0 | |
| 100 – 999 | 19 | 2.8% |
| 1k – 99k | 302 | 44.9% |
| **100k and up** | **352** | **52.3%** |

Per BINARY and never per symbol — one binary may drive twenty and this credits
its whole total to each — so every band is an **upper** bound.

**Which is also why AD-HOC VECTORS and the "under 100" band both read 0, and
neither is a measured absence.** A symbol reached by three hand-written calls
inside a binary that sweeps 500,000 checks elsewhere is credited with all
500,000 and lands in the top band. The class can only ever catch a symbol whose
WHOLE binary is ad-hoc. Telling the two apart needs per-symbol attribution,
which nothing here has, and `--sym` is the manual substitute.

**So SAMPLED is not weak for want of effort.** It is weak because a sample is
not a universal claim, which is the gap `equivalence.md` §5 names for unicorn
in the same words: `narrow` and `wide` differ on 1,999,998 of 2,000,000 random
inputs, and **agree on 2**.

### Discriminating power, which is the only axis that speaks to whether a test could TELL

F8163: a non-vacuity guard proves a branch was **reached**, not that the test
can tell it from its alternative. `mutate.py` judges exactly that.

| | |
|---|---|
| a mutation suite covers a binary that drives it | **517 of 673** (76.8%) |
| …and nothing in those suites went uncaught | **415 of 673** (61.7%) |
| no mutation suite reaches it at all | **156 of 673** (23.2%) |

Over the recorded snapshot: **9,252 mutations, 146 UNCAUGHT.** The metric is
`NOT caught` and not `caught == total`: **266 mutations carry
`"equivalent": true`** and are expected to survive, and scoring their survival
against the test marked 105 of 210 suites as imperfect for being right.

---

## 4. What it would cost to move `SAMPLED` up

**A wall, not a gradient**, and the wall is the INPUT DOMAIN rather than the
parameter list. Of the 666 SAMPLED, by our own declarations:

| | | |
|---|---|---|
| no pointer parameter at all | **201** | 30.2% — the domain IS the parameters; a sweep is conceivable and costs their total width |
| at least one pointer parameter | **392** | 58.9% — **read it** |
| our source does not settle it | **73** | 11.0% |

**A pointer parameter is not automatically an input, and this tree's own
worked example says so.** `FPM_div(unsigned short, unsigned short *, unsigned
short *)` takes two pointers and `t_fpm_div` still sweeps its whole domain in
65,536 trials — both pointers are OUT parameters and the input is the one
short. So a pointer means "read this one", not "no".

**And conceivable is not affordable: the width decides.** One 16-bit argument
is 65,536 trials and `t_fpm_div` runs it in under a second. Two is 2^32, and
`t_fpm_phasordp` declined exactly that — it swept the carry boundary instead
and wrote down which half it had not covered. **The ceiling on "make it
exhaustive" is a SUBSET of 201, against a population of 674.**

### The cheap axis is the other one

**156 DIRECT symbols are reached by no mutation suite.** A mutation suite needs
no new input domain: it asks whether the test could TELL, which is exactly the
question a sample leaves open. It is the only axis in this report that moves at
a cost the tree already pays, and 23.2% of the population is unexamined on it.

---

## 5. The tools

Both are triage aids. Neither grades anything, neither closes anything, and no
finding may cite a class from either as a derivation.

```
tools/eqproof.py                    the classification, with denominators
tools/eqproof.py --grade1           the grade-1 proof census
tools/eqproof.py --class DIRECT     one class listed; also COMPOSITE, NONE,
                                    EXHAUSTIVE, WHOLE, REDUCED, SAMPLED,
                                    ADHOC, SCALAR, POINTER
tools/eqproof.py --sym NAME         one symbol, its grade and its evidence
tools/eqproof.py --selftest         15 controls, every probe shown firing
sh tools/eqproof-checks.sh          the per-binary check counts it reads
```

`eqproof.py` reuses `byteident.py`'s `sizes`, `body`, `verdict`, `insns` and
`alpha_equal` unchanged, so its buckets are the same buckets. **`byteident.py`
was not modified**: it is another session's file and it defines the headline
number.

`eqproof-checks.sh` exists because the harness prints
`PASS <section> <n> checks` and **never names its own binary**, so a `-jN`
`make phase` log cannot be attributed — three binaries' sections interleave and
no line says whose is whose.

### Three things this cannot do, stated so nobody reads more into it

- **It cannot decide EXHAUSTIVE.** §3. Inferring it from a check count would be
  the dead detector this tree keeps rediscovering.
- **Check counts are per binary.** Every band is an upper bound.
- **Reachability is an over-approximation.** An edge is any relocation out of a
  function's body, so an address stored in a table counts. That direction moves
  a symbol from NONE into COMPOSITE rather than the reverse, which reports
  evidence as marginally stronger for one symbol but never invents an absence.

### What went wrong while building it, because the shapes recur

- **`scalar_only` emptied its own cache.** `got.pop()` on the cached set, where
  `got` IS the set: the first call for a symbol answered and every call after
  returned `None`. `classify` asks three times, once per class, and the three
  columns came to **201 + 130 + 349 = 680 over a population of 666**. **What
  caught it was adding the column up**, and `classify` now asserts the
  partition. A cache that answers once is worse than no cache, because a cold
  run and a warm run disagree and no denominator moves. F9496.
- **The check counter read a FAILING section as zero checks.** It matched
  `PASS … n checks` only, and a failure prints `FAIL … a/b checks failed`.
  Eight binaries carry a check `tools/gccdiverge.json` declares, so they exit
  non-zero under modern GCC while `make period` passes them; `t_v90specproc`
  runs 35,450 checks and read as 0, which put `V90SpectralVerifier::process`
  alone in the report's weakest class. F9494. **And `gccdiverge.json` has
  EIGHT entries, not the seven `CLAUDE.md` states.**
- **`tools/dis.py` shadows the standard library's `dis`.** Run from `tools/`, a
  traceback cannot even be FORMATTED: the crash handler dies with
  `module 'dis' has no attribute 'COMPILER_FLAG_NAMES'` and prints that instead
  of the real error. `whichfield.py`'s fix, applied at the top of the file.
