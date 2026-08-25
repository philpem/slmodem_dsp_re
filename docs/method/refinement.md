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

### 0. What an enumeration proves depends on how many cells hit zero

Running the domain to completion is necessary; it is not the whole story. Say
which of these you have, per function (7789 does it per closure):

- **A unique preimage** — exactly one cell maps onto the object. You have
  decoded the author's ORDER. `V90MP`: 4! = 24 cells, one zero, nearest
  near-miss at 2.
- **Several preimages** — more than one cell reaches zero. You have decoded a
  specific FACT, not an order, and the finding must say which fact.
  `enterPhase4` decodes only that the clear follows the deadline, because the
  `inPhase3` slot collides; `V90Modem` decodes a zero-test and "`dil` stored
  last"; `resetLinearMapping` decodes only the NEGATIVE, that a signed 16-bit
  local is excluded.
- **No preimage** — the map is constant or simply misses. The difference is
  not what you thought it was. Four measured this way in one pass alone
  (7795), and `SpectralShaper::reset` at 10 cells with none reaching zero is
  the cleanest: by lever 1's own rule it is therefore not statement order.

**Enumerate before reading any cell.** Two passes have recorded doing that
explicitly, because stopping at a tempting near-miss is how an exhausted
enumeration turns back into a search.

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

**THE PRECONDITION: STRIP ALIGNMENT PADDING FIRST, WHEREVER IT OCCURS.**
`instrcount.py` counted intra-function padding as code and **inverted the
triage of five functions** (7793). `printErrorHistogramAndReset` read +12 and
is EQUAL; `getAT_UD` read +10 and the blob has one instruction MORE than us;
`process` read -1 and is -3. Two of the five read as "structural, decline it"
and were actually this lever's absence shape. It now imports `byteident.py`'s
own `_padding` predicate, so there is one definition of what padding is.

A trailing-only filter is not enough -- it invented one absence and hid a
real one. And counts quoted in older findings will not reproduce: 7778's three
declines were re-measured, two survive exactly and `calcMtoMatchKtarget` loses
one of its five to padding, so its verdict stands with the number read as
four.

**A SECOND OBSERVABLE, INDEPENDENT OF THE BYTE GRADE:** the order of `.rodata`
strings a function references. It agrees or disagrees without reference to any
instruction, so it corroborates a statement-order decoding that the byte grade
alone cannot distinguish (7792).

**AND A TABLE THAT SEPARATES.** Where a function has two candidate differences,
compile the CROSS PRODUCT rather than one at a time: a cell that changes one
difference and not the other proves the two are independent, which no single
comparison can (7792).

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

**THIS ENTRY SAID THE OPPOSITE UNTIL 7796 MEASURED IT, AND THE CORRECTION IS
THE MOST USEFUL THING IN IT.** 7777's control 4 moved ONE plain function,
measured no change, and 7774 was narrowed to clone pairs on that basis. The
control's observation is right and the conclusion drawn from it was wrong: a
single move usually does nothing. Reordering nine whole files to the blob's
emission order gained **17 symbols and lost none** (7796); five more files
gained **5 and lost 1** (7801).

**THE SHAPE DOES NOT PREDICT THE YIELD. THE BUCKET DOES.** Three passes have
now scored every shape at both zero and non-zero:

    7774   clone-only     the four V90Resampler constructors, 12 for 12
    7796   plain 16, twin 1, CLONE 0    over nine files
    7801   plain 2,  twin 1, CLONE 2    over five files

7796's clone column is the one to distrust, and it is why: `ModulusCoder`'s
four constructor clones were already in the blob's order and stayed at 25
differing bytes, `V90CP`'s C2 went EXACT to REGALLOC, `V90Phase4Modulator`'s
C1/C2 merely swapped their 21 and 27 -- three files' worth of clones, all in
files that also had gaps or were reverted. 7801 moved all three `Resampler`
destructors to byte identity in one file. **Do not skip a candidate for its
shape; skip it for its bucket.**

**The carrier is upstream of the function, not its own index.** Four REGALLOC
symbols already sat at the blob's emission index, so only their PREDECESSORS
could change -- and three of the four closed without moving. That is 7772's
"something ahead of both in the TU", now named. 7772's own twin
`recivedSUV` closed with neither twin moving relative to the other.

**The mechanism is not a global counter.** `-S` from both trees:
`generateDataSymbolBeforeFPE` has the identical label number (`.L212`), the
identical instruction count, and different registers -- same at `.L215` for
its twin. The label number is where `label_num` stood at expansion, so every
cross-TU counter is at the same value in both compiles. Same text, same index,
same counters, different allocation. What is left depends on the IDENTITY of
what was compiled before rather than the amount: allocation addresses and
pointer-keyed hash iteration. Do not re-try the counters.

**Aim at REGALLOC files, not BYTES files, and this is now the twice-confirmed
part.** Over 7796's nine files, **16 of 25 REGALLOC candidates closed, 0 of 9
BYTES**, and the one file aimed at a BYTES bucket paid nothing and lost a
symbol. Over 7801's five, **5 of 7 REGALLOC closed and the BYTES and SIZE
counts did not move by ONE symbol tree-wide** -- every gain and the one loss
was a REGALLOC/EXACT swap.

Re-measured at 7801: of the 165 objects sharing a `.text` symbol with the blob,
**76 already emit in the blob's order and 89 do not**, and the 89 hold **19 of
the remaining grade 1 and 63 of the BYTES**. That does not reach the tree's 30
and 94 because a COMDAT function is in its own `.gnu.linkonce.t.*` section and
is outside this ordering entirely: 3 grade 1 and 6 BYTES live there. Two
regions are not reachable by definition order at all: that head, where
templates and clones interleave, and the cgraph tail.

### 3a. The pre-check, before you permute anything (7801)

Three questions, all answered from the objects, and one of them stops a file
being permuted into a null nobody can read.

1. **Is the blob's order REACHABLE?** The emission model forces a callee ahead
   of its caller, so the order is reachable only if no call edge of ours runs
   caller-first in it. Read the edges off `objdump -dr` and **bound each
   function by its `nm -S` size**: alignment padding between functions is spelt
   `jmp <next symbol>` plus nops and reads as a call. That artefact invented
   `_iir_filter_delete -> _iir_filter_progress` and would have condemned a file
   that then gained. Check the detector against your OWN emission order -- an
   edge reading caller-first in your object is an artefact, because the same
   rule produced it.
   **It is a filter for the unreachable case, NOT a certificate.** It reads the
   final object and cannot see an INLINED call, and GCC 3.4 keeps the cgraph
   edge after inlining. `Resampler.cpp` scans as zero edges and still emits
   `reset` ahead of both constructor pairs. The only proof is the achieved
   order after the rebuild.
2. **Is the blob's span YOURS?** List the blob symbols whose address falls
   between the file's first and last and check they are all yours.
   `v34pcmif.c` is 34 of 57 -- the other 23 are in four other files of ours, or
   unwritten -- so only the RELATIVE order is recoverable there. It gained two
   anyway: a gap is a reason to discount a file's SILENCE, not to skip it.
3. **Is a datum or a macro in the way?** A file-scope table between two
   functions being swapped is lever 4's own effect. Hoist it in a SEPARATE
   commit and measure that alone. `toneiir.c`'s two `.rodata` blocks, two
   `#define`s and two file-local statics were hoisted and measured by
   themselves: every symbol identical, to the differing byte. Data moved as a
   unit keeps its own relative order, so lever 4 does not fire.

**An `#if` inside a body is safe when it BALANCES.** 7796's rule -- refuse to
move any chunk containing a `#` line -- is over-strict and freezes
`V34TimingFiltersInit`, whose `#ifdef DSPLIB_REPRODUCE_BUGS ... #endif` is
wholly inside the body. The invariant that stops the wave 5 trap is the
balance: `#if` +1, `#endif` -1, never negative, zero at the end. A bare
`#define` or `#include` still refuses.

**INDEX-FOR-INDEX AGREEMENT IS NOT SUFFICIENT.** `V34EqualizerCleanUp` sits at
the blob's index 16 of 26, in a file with no gaps, no unwritten neighbours and
all 26 symbols in place, with the blob's own predecessor ahead of it -- and it
went EXACT to grade 1. Achieving the order buys you the right to believe a null;
it does not buy byte identity.

**A FILE THAT DID NOT CLOSE MAY STILL HAVE MADE ITS RESIDUAL LEGIBLE.**
`toneiir_reset` went from 22 differing bytes of 60 to ONE, and that one is a
`movzwl` where we emit `movswl` -- lever 8, a named declaration defect that 22
bytes of register noise had been hiding (7802). Read the (b) rows, not only
the (a) ones.

**And name a check the reader can run.** 7796 could say "the `.text+0x...`
comments run upwards down the file" because both Phase4Modulator files carry
one per function. Most files carry none, so the portable check -- and the one
in 7801's five headers -- is `nm -n --defined-only` on `build/tc_out/<file>.o`
against the blob over the symbols both define, with today's score written in.

**THAT RULE IS ABOUT PICKING A FILE, AND WAVE 6 DID NOT TEST IT -- WHAT IT
CORRECTS IS HOW YOU COUNT THE YIELD AFTERWARDS.** Four files kept, **7 gained
and 0 lost**, and the buckets settle where they came from: REGALLOC 34 -> 29
and BYTES 94 -> 92, so **5 are REGALLOC and all five were targets, and the 2
BYTES closures were bystanders nobody aimed at** (`V90Parameters::loadParams`,
`V90Equalizer::enterChannelVerification`). Five of the ten targets closed, so a
ledger counting only targets reads 5 against a real 7. Keep aiming whole files
at REGALLOC; count the whole set, both directions. This wave's own per-shape yield was
**plain 7, twin 0, clone 0** -- which looked like 7796 repeating until 7801
scored clone 2 on the next five files, so read it with the table above and not
as a pattern -- and the one file whose only
targets were a C1/C2 pair (`V90Demapper.cpp`) lost a symbol and was reverted.

**A NULL RESULT CAN COST TOO MUCH TO KEEP.** `VPcmFloModem.cpp` reached 16 of
16 and gained nothing, and reverting it left the tree at 474 -- but reaching it
had taken twelve macro blocks hoisted on top of the permutation. 7796's kept
neutral files were reorder-only. The measurement is the deliverable: record
that the order is achievable and pays nothing, and do not keep the diff.
Findings 7797 and 7798.

**A FILE-SCOPE `static` THAT CALLS A MEMBER FUNCTION SETS THAT MEMBER'S
EMISSION SLOT, so it is the exception to "statics live above".** Wave 6's
`V90AutoDigitalImpDetector.cpp` stopped at 23 of 34 with `isAltRbs` emitted at
index 0 against the blob's 10; the cause was `adid_recheckAltRbs`, a helper
THIS RECONSTRUCTION introduced, sitting at the top of the file with
`o->isAltRbs(...)` in its body. Moving that one helper below its own first user
took the file to 34 of 34. The object inlines the call, so the original had no
such edge -- our factoring was setting the order. Check for it whenever a
reorder lands short by a single symbol sitting at index 0. Finding 7798.

**Two traps, both hit while doing it.** An `#endif` travelled with a moved
chunk twice and STILL COMPILED, silently enlarging an
`#if __SIZEOF_POINTER__ == 4` region over live code -- `compilers.md`'s V3,
which fails open. And a macro placed beside its first user ends up below it
after a move. Rule now in the files: macros and file-scope statics live ABOVE
the definitions. Any tool doing this must refuse to write unless the line
multiset is unchanged, which is what proves a permutation is a permutation.

**AND THE MECHANICAL CHECK FOR BOTH IS ONE THING, WITH TWO WAYS OF BEING
DEAD.** Compare the SORTED MULTISET of preprocessed non-blank lines against
`HEAD`, **under both `-D__SIZEOF_POINTER__=4` and `-D__SIZEOF_POINTER__=8`**:

- Run the period arm alone and it does not fire at all. An injected `#endif`
  moved 40 lines down `V90Equalizer.cpp` passed, exit 0 -- enlarging a region
  whose guard is TRUE swallows live code without deleting a line. V3 is that
  the predefine is absent under 3.4.2, so **only the FALSE arm shows it**, and
  the real build uses the other one.
- Compare COUNTS rather than the multiset and it misses the macro trap
  entirely: three of wave 6's files came out with a macro below its first user
  at 1781 preprocessed lines against 1781. An identifier used before its
  `#define` is not expanded, so the line survives with different text -- and
  that is not always a compile error, so wave 5's loud case is not the general
  shape.

Shown firing on both injections and clean on everything committed. Finding 7799.

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

### 7. `delete[]` versus an explicit guarded free

**The blob's global `operator delete` IS `sysdep_free`** -- it contains no
`_Znwj`, `_ZdlPv` or `_ZdaPv` at all, and its compiler-generated `D0Ev`
destructors tail-call `sysdep_free` exactly where a library `operator delete`
would call `_ZdlPv`.

The consequence is a one-instruction difference at the same byte count. At a
destructor's LAST free the object makes a plain `call sysdep_free`; our
explicit `if (p != 0) sysdep_free(p)` makes a sibling `jmp`. Eight spellings
were compiled -- guard, braces, implicit `!= 0`, trailing `return`, ternary,
short-circuit `&&`, if/else, an inlined helper carrying the guard, and scalar
`delete` -- and **all of them sibcall. Only `delete[]` does not** (7786).

Nine destructors closed on it in wave 4b, four of which were in the SIZE
bucket and nobody was looking at; **fifteen more in wave 7** (7815), taking
grade 0 from 479 to 494.

**+1 INSTRUCTION AT THE SAME BYTE COUNT IS THE SPECIAL CASE, NOT THE RULE, and
screening on it would have missed nine of wave 7's fifteen.** The sibcall does
not only swap `call`+`ret` for `jmp` -- **it deletes the frame**. Where the
destructor's only work is the free, the `sub`/`add` pair and the second `ret`
go with it:

    Scrambler<h,h>::~Scrambler   ours 7 insns / 25 bytes
                                 blob 11 insns / 29 bytes    -> EXACT

So a site is a candidate at **either** delta: +4/+4 where the free is the whole
body, +1/+0 where the destructor needs the frame anyway (`V90Equalizer` 112 vs
113 at 541 bytes both, `V90CP` 43 vs 44 at 173 both).

**THE ARITHMETIC IS ALSO THE STOPPING RULE.** Compute what the lever costs at a
site where it is confirmed, and decline anything that does not reconcile. Six
C++ destructors were declined that way in wave 7: a 4-instruction delta with a
**31-byte** gap is not this lever (`V92Precoder`, `V92PreFilter`), and
`V90SpectralVerifier` at EQUAL instruction count while the sibcall flag
disagrees is internally impossible for it. `delete[]` there would be a fit and
7782 draws that line.

**THE ENUMERATION IS NINE SPELLINGS, AND THE TWO THAT DO NOT SIBCALL ARE BOTH
DELETE-EXPRESSIONS (7817).** 7786's spelling C was `delete p` on a **POD**,
which sibcalls. On a pointer to a class **with a destructor** the same syntax
is a different construct --

    delete p    ==>    if (p) { p->~T(); operator delete(p); }

-- and GCC 3.4.2 does not sibcall that call either. So scalar `delete` sits in
BOTH columns depending on what it deletes, and **what suppresses the tail call
is the EXPRESSION, not the type**.

That closed ten more symbols over five files. **Anywhere this tree open-codes
`p->~T(); sysdep_free(p);` is a candidate, and the search is one grep** --
`V92Precoder` went from 25 instructions in 77 bytes to the blob's 29 in 108 in
a single compile, and the 31-byte gap that had been read as "register pressure"
was the expansion missing entirely.

**AND IT WILL BREAK THE MODERN BUILD'S LINK UNTIL YOU ADD THE SIZED FORM.**
C++14 sized deallocation makes GCC 13 call `operator delete(void *, size_t)`
for `delete p` on a class with a destructor, which is an undefined `_ZdlPvj`
in a tree that links no libstdc++ -- every test binary, while the period
differential is 251 passed / 0 failed. `Resampler.h` documents it and solves it
with a MEMBER operator; a global sized form guarded on
`__cplusplus >= 201402L` is inert under 3.4.2 (199711L) and does the same job.
Either way, prove the guard is inert by re-reading `byteident` across it.

**BUT A DELETE-EXPRESSION RUNS A DESTRUCTOR AND AN EXPLICIT FREE DOES NOT**, so
take it only where the OBJECT ITSELF makes the destructor call.
`V92Transmitter::modulusEncoder` is freed by the blob with no such call and was
declined for exactly that: `delete` would invent one, and nothing may add a
call the object does not make.

**MEASURE IT PAIRWISE, WITH `tools/sibcensus.py`.** A raw population ratio
mixes "the blob does not sibcall here" with "the blob has no such function".
Over the 1251 symbols both objects define, wave 7 re-derived **1207 agree, 36
where we sibcall and the object does not (32 to `sysdep_free`), 8 the other
way** -- against 7786's 1188 / 50 / 13, which is wave 4b's nine closures plus a
tool artefact: **an indirect `jmp *TABLE(,%eax,4)` is a switch and carries an
`R_386_32` relocation against `.rodata` exactly as a relocated tail call
carries one against its callee.** Seven switches were being counted as sibling
calls. Bound each function by its `nm -S` size too, or the `jmp <next symbol>`
GCC pads with reads as one -- lever 3a's artefact in another costume.

Three hypotheses are dead and should not be re-tried: the blob tail-calls
`sysdep_free` 67 times elsewhere, so it is not a declaration property; the
disagreement runs both ways, so it is not a flag; and **all eight of the other
direction are ABSENCES** -- 7 to 97 instructions of missing body, and in six of
them the blob's tail-callee is not our last statement at all (7818).

**`delete[]` IS NOT AUTOMATICALLY RIGHT, and the array test must not be our own
allocation.** Deriving "it is an array" from our `sysdep_malloc(n * sizeof(T))`
is the reconstruction arguing for itself. The non-circular witness is that the
class INDEXES the member -- `state_[i]`, `buf + size - 1`, `pLimit + c` -- which
the differential tier has already validated against the blob. And confirm the
pointee is a **POD**: on a pointer to a class with a destructor, `delete[]`
emits a destructor loop and reads an array cookie a malloc'd block does not
have, which is wrong behaviour and not merely wrong bytes. A `void *` member
cannot take a delete-expression at all.

**Only the LAST free is byte-evidence.** Away from tail position the two
spellings emit identically, so the other frees in a destructor carry `delete[]`
for uniformity, not because the object distinguishes them. Say so in the file.

**AND THE DEFINITION'S POSITION IN THE TU IS ITSELF A LEVER-3 CARRIER (7816).**
Hoisting the one inline `operator delete[]` into `dsplib/sysdep.h` -- which
every one of these files already reaches transitively -- **cost eight
destructors their byte identity, four of them wave 4b's**, while touching no
destructor and no free. The TU's declaration set was constant across the
experiment (`FloatIIR.cpp` reached `sysdep.h` under both arrangements and kept
its symbols); only the inline function's position moved. So each `.cpp` keeps
its own copy, `Scrambler.h` carries the one case that cannot (its destructor is
inline in the header), and every file reaching that header is forbidden its
own. "One type, one home" is about TYPES and undefined behaviour; this is an
inline function, the duplication is deliberate, and consolidating it needs the
SET diff re-run.

**Bounded, and the bound is now named to the byte.** `V92deleteConstellations`
(3 of 173) and `V92deleteFilterCoefficients` (3 of 106) are the purest
instances of the signature in the object -- byte-identical over 93 of 106
bytes, residual one 13-byte block -- and `delete[]` does not exist in a `.c`.
Buying them means asserting `V92ParamsInfo` was a `.cpp` of `extern "C"`
functions, which this codebase does elsewhere (`v34hstx1.cpp`). **The test is
a null**: a relocation's PRESENCE proves nothing about a TU (306, 333), there
are no unrelocated calls out of those five functions and no local text symbols
in their span to be the target of one. Declined; do not rename the file to buy
the spelling (7819).

### 8. Width and signedness — and the DESTINATION's declared type

7630's `movzwl` copied as 32 bits. Equal instruction count hides it completely.

**FIRST ASK WHETHER THE 32-BIT RESULT IS USED, because that decides which
question you are answering** (CLAUDE.md's forced-versus-free rule):

- **Used** — the extension is live and the difference is evidence about the
  loaded object's TYPE. Finding 613 is the precedent: a real defect no test
  could see, because both readings agree over every value the field holds.
- **Discarded** — a 16-bit value going straight back into a 16-bit slot. This
  is finding 614's free case, and it is NOT evidence about the field. Read on.

**WHERE THE EXTENSION IS DEAD, IT FOLLOWS THE DECLARED TYPE OF THE LOCAL BEING
LOADED INTO — not the field, not the store destination, and not a cast.**
Measured on `toneiir_reset`, six spellings compiled (7803):

    short prev = st->env_band;                  movswl
    unsigned short prev = st->env_band;         movzwl   <-- the blob
    int prev = st->env_band;                    movswl
    unsigned int prev = st->env_band;           movswl
    short prev = (unsigned short)st->env_band;  movswl
    short prev; prev = st->env_band;            movswl

A cast does not do it because the load happens first and the conversion after.
`unsigned int` does not either: the value is still fetched from a signed short
and then widened. So this is a real, recoverable source property with an
exhausted two-way domain, and 7782's ruling takes it.

**THE TRAP THIS EXISTS TO PREVENT.** 7802 read exactly this difference as a
declaration defect in the FIELD and would have retyped `short env_band` to
unsigned. The blob itself refutes that: it uses `movswl` at
`toneiir_progress+235`, where the result feeds `imul $0x3f5c,%eax,%ebx`, and
`movzwl` at three sites where the upper half is discarded. **One field, both
extensions.** Retyping it would have matched two sites, broken the third, and
asserted something the object contradicts.

So: if the object uses both extensions on one field, the field's type is not
what varies — look at each site's destination instead. And verify PER SITE
when the enclosing functions are not byte-identical: `toneiir_progress` keeping
its `movswl` is what proved the edit touched only the dead sites.

### 9. Operand order in commutative expressions

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
