# Refinement: what actually closes the last bytes

Every lever here was measured in this tree, and every one carries its
counterexample. A lever without a known failure is a lever nobody has pushed
hard enough yet.

**The target is grade 0 — positional byte identity, per function.** Not byte
count, which moves when we emit more code rather than more of the *right*
code. Not instruction count: `qcLineVerification` measured 159 instructions
against 159 and was still wrong, a `movzwl` copied as 32 bits (finding 7630).

Measure with `tools/toolchain/byteident.py`:

| flag | what it gives you |
|---|---|
| *(none)* | the tree's grade counts |
| `--why SYMBOL` | **the row `alpha_equal` rejects on** |
| `--list-exact` | the exact SET, for a diff |

Three habits, each of which has cost this project real time when skipped:

1. **Diff the SET, not the count.** A count nets to zero across a gain and a
   loss, and a fix once changed three functions its author never disassembled
   (7764).
2. **Baseline before you change anything.** A number measured only after a
   change is not evidence about the change. Four separate attribution puzzles
   here came from this (7763, 7769).
3. **`--why`, not eyeballing the diff.** The first row that DIFFERS is usually
   not the row the comparison rejects on. One pass quoted the wrong one and
   had to correct itself (7778).

`make phase` does **not** build `build/tc_out`. Nothing does except
`tools/toolchain/build.sh`. The tool now refuses to print when `src/` is newer,
because it once reported pre-merge grades as current and they looked entirely
normal (7769).

---

## The levers, in the order they have paid off

### 1. Statement order — enumerate, do not search

The single most productive lever: ten of the closures across four passes.

When a difference is a statement order, the candidate source spellings form a
small finite family. **Compile all of them.** If exactly one maps onto the
object, the object's emission has a unique preimage and you have *decoded* the
author's order.

    resetBeforRRN     2 orders compiled, 1 matches   -> EXACT   (7770)
    externalReset     all 3! compiled, 6 distinct emissions, unique preimage -> EXACT (7779)
    enterRepeatedCP   all 6 positions of one store   -> EXACT   (7770)

This holds **even when the compiler reorders your source**. An earlier rule
(7766) said to act only where our emission *is* our source, because then the
object's emission is the author's; that rule is sound and too strong. In all
three of 7770's closures GCC reordered our source and the order was still
recoverable, because the domain was exhausted rather than trusted.

**The branch that would kill it, and it must be excluded by measurement:** if
the compiler emits the same order for *every* source spelling, the map is
constant, no source produces the object's bytes, and the difference is not a
store-order difference at all. Nothing before the compile distinguishes that
case from a bijection.

**Where it stops.** Hill-climbing on byte count is not evidence:

    V92Phase4Modulator::reset   14 spellings, NONE emits the object's store
                                order; best was 27 of 290 -> DECLINED (7771)

Closer bytes are not a grade. Finding 7782 is the ruling on this: take the
bytes when the space is exhausted and one element maps; decline when you are
searching. Record which side you are on and what the domain was — a finding
that says "closed by reordering" without saying how many spellings were
compiled is not reviewable.

### 2. Instruction count at EQUAL byte size means a missing or extra statement

Cheap, and it finds things no test can:

    V90Resampler (Pf) ctor   blob 69 insns, ours 68, both 271 bytes
                             -> a dead `timingHistoryIndex = 0` that reset()
                                overwrites three statements later (7774)

A dead store is invisible to every differential test by construction. This is
the only lever that finds one. Check it on every function in a batch.

### 3. Definition order in the translation unit

Two character-identical bodies in one TU compile to **different bytes**, and it
follows the position, not the text:

    whichever body comes FIRST gets %ecx; the second gets %eax   (7772)

Swapping the definitions moves the difference with the slot. It is **not**
`-frename-registers` — compiled with and without, same result. The cause is
not established.

Cashed once: `nm -n` showed the blob emitted `V90Resampler`'s `(f)` constructor
pair before its `(Pf)` pair and we emitted them the other way round, and in
*both* objects it was the first-emitted clone pair that diverged from itself.
Swapping made all four constructors byte-exact, 12 symbols for 12 (7774).

**Counterexample, and it is load-bearing:** moving a *plain* (non-clone)
function's definition changed nothing at all, and pointing the clone-pair lever
at `V90Equalizer`'s destructor did nothing (7777, controls 4 and 5). Proven on
clone pairs; disproved on at least one plain function.

Tree-wide sizing: **96 translation units emit in a different order from the
blob**, holding 40 of the 48 REGALLOC and 89 of the 109 BYTES symbols. That is
an upper bound on opportunity, not a yield estimate.

### 4. File-scope declaration order

GCC 3.4.2 emits file-scope objects in **reverse definition order**. Verify that
on our own object before leaning on it — it was checked ten-for-ten first.

Matching the two `.rodata` blocks **by content** paired all ten of
`v8_V21_Init`'s tables with no array differing, which proved the argument
assignment was already right and the *definition order* was the defect. Ten of
thirteen bytes (7765).

### 5. Storage class, read off relocations

- A relocation against a **section** symbol (`.data`/`.rodata`) rather than a
  named one says the object was file-local: `static`.
- **`.data` rather than `.rodata`** says not `const`.
- Spacing between two tables says which came first.

All three at once on `RcFixed_Check_Combination` (7767). `nm` showed ours as
`R` before and `d` after.

**Where it stops:** `b103_ops` and `v23_ops` were established file-local the
same way, and adding `static` moved the relocations but **not the register
choice**, so neither reached grade 0 by it (7768).

### 6. Unrolled and partially-unrolled code

The object is frequently more unrolled than the natural source. When you write
source in an unrolled or partially-unrolled shape to match it, **put the
rolled version in a comment above it.** The unrolled form is what the compiler
needs; the loop is what a reader needs, and without it the next person cannot
tell an intentional expansion from a transcription accident.

    /*
     * The object writes these out; the source it came from may not have.
     * Rolled, this is:
     *     for (i = 0; i <= 16; i++)
     *             bits[i] = 1;
     */
    bits[0] = 1;
    bits[1] = 1;
    ...

**Watch for Duff's Device and its relatives.** They produce a signature that
is easy to misread: partial unrolling with loop control still present, which
looks like "the compiler unrolled it" and is actually the source's own shape.
The tell is an indirect jump whose case labels land INSIDE a loop body and
fall through, rather than a plain switch whose cases do not. 79 blob functions
carry an indirect jump, all `jmp *@.rodata(,%eax,4)`; none has been checked
for this (7785).

Two negatives already recorded, so nobody repeats them:

- **`-funroll-loops` is not a missing period flag.** Whole tree rebuilt with
  it: 0 gained, 39 lost, grade 0 433 -> 394 (7783).
- **`packData`'s difference is not loop shape.** All 16 combinations of which
  of its four loops are written out were compiled; the maximum is 516 against
  the object's 534, so the domain is exhausted with no match (7785). That is
  lever 1's constant-map branch appearing for real.

### 7. Width and signedness

7630's `movzwl` copied as 32 bits. Equal instruction count hides it completely.

### 8. Operand order in commutative expressions

`return dsp->rx_energy & dsp->rx_tone;` — swapping the two operands gave byte
identity.

---

## What does not work

- **Renaming a variable.** Free for the compiler (7002). It changes nothing.
- **Reordering files** on the strength of lever 3. Control 4 of 7777 exists
  precisely to stop that.
- **Getting closer.** See lever 1's stopping rule.

---

## Grades 1 and 2 are not failure, and grade 1 is not a dead end

    grade 0  EXACT      same bytes, same places
    grade 1  REGALLOC   same instructions and operands under a per-live-range
                        register bijection
    grade 2             differences changing neither result nor execution

Grade 1 entries are **deprioritised, not written off**. Register allocation in
GCC 3.4.2 is a function of source form, and lever 3 is a demonstrated case of
steering it. Do not chase them inside a batch aimed at BYTES; do not describe
them as unreachable either.

`alpha_equal`'s own defects have twice been mistaken for real differences —
a zeroing idiom that failed exactly when a register was renamed (7762), and a
padding guard that matched no padding (7769). If `--why` reports something that
looks like a tool artefact, it may be one; say so and check.
