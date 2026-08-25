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

Levers 0 to 9 came out of the refinement waves, in that order. **10, 11 and 12
did not: they were learned once somewhere in the older record, written into one
finding and never generalised**, and were swept up afterwards (7813). Their
measurements are as real as the rest and their yield in a refinement pass is
unknown, which is the one thing to hold in mind when a brief quotes them.

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


**OUR SOURCE ORDER IS THE ANSWER SHEET, NOT A CANDIDATE, and this is the
sentence to read first.** A reconstruction writes the statements down in the
order the object's stores come out of the disassembly, so **our source order
already IS the blob's EMISSION order** — it is the first thing anybody
transcribes. Comparing the two therefore tells you nothing, and a cell that
reproduces it is not a hit. What an enumeration is searching for is the
PREIMAGE of that order under GCC 3.4.2's scheduling, which is a different
object and is usually not an order you can read anywhere. In all four of
7805's closures our source was already the emitted order and the compiler had
permuted it. So before enumerating: if our source reads like the disassembly,
that is the ANSWER and not a guess, and the domain to enumerate is everything
else.

**AND THE HARNESS IS WHAT MAKES AN EXHAUSTED DOMAIN AFFORDABLE**, which is
what turns rule 0 from advice into something a pass can budget for. Compile the
REAL translation unit rather than a model of it, one container pass over every
variant, and score each object with **`byteident.py`'s own `body` and
`verdict`** — never a second implementation of the comparison, or the number
printed per cell can disagree with the number the tree-wide tool prints (7773's
rule). Measured: **0.03 s a cell for a 500-line file and 0.25 s for a
1,500-line one** (7805). Domains that bought at those rates: 720 cells for a
unique preimage (7806) and 5,151 for a measured NO preimage (7809) — which
file was which size is in those findings, not multiplied out here.

For a large class a stand-alone MODEL is faster still — 40,320 cells in twelve
minutes for an 8! constructor domain, 6,624 distinct emissions and thirteen
preimages (7807) — **and a model must be validated before it is believed.**
That one was checked to reproduce our committed object's thirteen instructions
exactly, from our committed source order, before a single cell of it was read.
A model nobody has seen agree with the real compiler is `gates.md`'s dead
detector with 40,320 rows of output.

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

**AND THE DELTA RUNS BOTH WAYS -- MOST OF THIS TREE'S ARE EXTRAS, NOT
ABSENCES.** Lever 2's worked example is a MISSING statement, and that shape has
been over-read since. Measured over one cluster: three of its four real deltas
were **extra code in ours**, not absences (7823) -- `calcMtoMatchKtarget` ours
71 against blob 67, `updateUref` 66 against 64, `unitePhasesInfoOfUref` 203
against 202. Only `SpectralShaper::process` was an absence. Read the sign
before reaching for "what statement is missing", and note that
`instrcount.py`'s columns are `ours, blob, delta`.

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

**AND `--why`'s BARE `False` DOES NOT ROUTE HERE RELIABLY, because it is the
one place the padding filter is missing (7823).** `alpha_why` opens with
`len(x) != len(y)` over `insns()` rows and `insns()` does not strip padding, so
a bare `False` conflates "a statement is missing" with "the two carry different
numbers of alignment nops". `printErrorHistogramAndReset` prints `False` and is
**EQUAL on code**, 86 against 86, with 2 nops against 14. **Confirm every bare
`False` against `instrcount.py` before spending lever 2 on it** -- and read its
columns as `ours, blob, delta` with the delta OURS MINUS BLOB, because reading
it the other way inverts the diagnosis. Three of this cluster's four real
deltas are EXTRAS in our code, not absences, which is the opposite of the
lever's worked example and wants a different search.

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
`-frename-registers` — compiled with and without, same result (7772). The
cause is now established and is at the end of this lever (7812).

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

**A COROLLARY THE MECHANISM MAKES OBVIOUS AND WHICH WAS MEASURED SEPARATELY:
the lever cannot reach a symbol at emission index 0** (7808). The cursor is
threaded through the TU in emission order, so a symbol emitted FIRST has
nothing ahead of it to have moved the cursor. `V90Phase4Demodulator`'s C1/C2
sit at index 0 with nothing above them but `typedef char` assertions; all six
orderings of the file's bottom blocks were compiled, **two of them achieve the
blob's `nm -n` order exactly, 15 for 15, reorder-only**, and the pair stays at
53 differing bytes in every one. Check the target's emission index before
spending a reorder on it -- and prefer the `-fno-peephole2` certificate below,
which is stronger because it answers for the symbol rather than its position.

### 3b. The mechanism, settled: a round-robin cursor in `peephole2` (7812)

**It is not the register allocator, and it is not a counter.** Swap two
definitions in `V90Phase4Modulator.cpp` and dump every RTL pass with `-da`:
the unmoved bystander `resetRRNSecondSection` is **identical through `.24.lreg`,
`.25.greg`, `.26.postreload` and `.27.flow2`** and first differs at
`.28.peephole2`. At `flow2` the two stores are still immediates straight to
memory with no register in them. `peephole2` is what puts a register there.

The pattern is `i386.md:17507` — a store of an immediate whose *encoding*
reaches `ix86_cost->large_insn` is split into `reg = imm; mem = reg`, and its
`match_scratch` is filled by `peep2_find_free_register` (`recog.c:2931`), whose
first line is

    static int search_ofs;

a round-robin cursor over `reg_alloc_order`. On success it is set to the
register after the one found (`recog.c:3018`); **nothing resets it per
function, per file or per pass** — all four references in 3.4.2 are inside that
one function. It is threaded through a translation unit in EMISSION order,
which is why the leaf block matters and the file's source order does not.

**Why a swap moves it while every counter stays put.** Each call is
`search_ofs' = f(search_ofs, the live set)`, advancing past whichever register
was free rather than by a fixed step, so composing two of them does not
commute. That is exactly 7796's measurement — `generateDataSymbolBeforeFPE` had
the identical label number (`.L212`), the identical instruction count and
different registers, and the same at `.L215` for its twin — read as a
mechanism instead of by elimination. **One discontinuity:** `recog.c:3024`
resets the cursor to 0 when no register can be found, so a function under
enough pressure to fail an allocation resynchronises everything after it.

**Three arms, each of which turns the pattern off, and each collapses the
effect to zero** (symbols compared only where address and name agree in both
objects, so nothing that moved is counted):

    the tree's flags   1 bystander differs      -fno-peephole2   0
    -mtune=i386        0                        -Os              0

`-mtune` is the one to read twice: `x86_split_long_moves = m_PPRO` (`i386.c:492`)
masked by the tune setting (`i386.h:263`), so **`-mtune=i686` — finding 612's
flag, the one that took the codegen match from 30 to 82 — is exactly and only
what enables this.**

**THE ADVANCE TEST, AND IT IS EXACT IN THE DIRECTION THAT MATTERS.** Compile the
file twice, with and without `-fno-peephole2`, and compare the candidate
function. Three outcomes and they are not the same claim:

    bytes IDENTICAL           peephole2 did nothing.  It consumed no scratch,
                              the cursor cannot reach it, reordering CANNOT
                              move this symbol.  A definitive clear.
    bytes differ, and a
    REGISTER appears that
    the -fno-peephole2 build
    never uses               a scratch was allocated.  Exposed, and you can
                              name the register.
    bytes differ, no new
    register                 peephole2 fired on a pattern that takes no
                              scratch (the xor-zeroing one, lea-to-add).
                              Undecided -- treat as exposed.

Run over the live buckets on this tree, one file compiled twice per candidate:

    REGALLOC  25 symbols   12 take a scratch   12 undecided    1 CLEARED
    BYTES     91 symbols   18 take a scratch   45 undecided   28 CLEARED

**That is 3a's missing certificate.** 3a can say a file's order is achievable
and cannot say a null result means anything; this says, per symbol and before
any permutation, that 1 of the remaining 25 REGALLOC symbols
(`V92Mapper::reset`) and 28 of the 91 BYTES are not reachable by this lever at
all. It also explains 7796 and 7801 after the fact: nearly every REGALLOC symbol
is peephole2-touched, which is why aiming whole files at that bucket paid and
aiming at BYTES did not.

**AND IT HAS BEEN WATCHED FIRE IN BOTH DIRECTIONS, because a certificate that
licenses SKIPPING work is exactly the shape `gates.md` rule 3 exists for.**
Permuting the file every way and comparing only where the symbol did not itself
move — objdump renders branch targets absolutely, and that artefact reported
the certificate BROKEN on the first run of the check:

    resetRRNSecondSection   classified "takes a scratch (ecx, edi)"
                            7 of 28 permutations changed it        FIRES
    V92Mapper::reset,       all four classified CLEARED
    freqToNearestBin,       0 of 23 comparable permutations
    ModulusEncoder C2,      changed any of them                    HOLDS
    spectralDesign
    V90Equalizer::process   classified undecided, 0 of 28

**EXPOSED IS NECESSARY AND NOT SUFFICIENT, and that is the part to hold on to.**
`V90CP`'s C1 takes three scratch registers and held over 21 comparable
permutations; `generateSymbol` held over 28. The cursor is a state machine and
most single swaps are no-ops for it — which is 7777's control 4 all over again,
and the reason 7796 had to permute whole files rather than pairs.

The twelve REGALLOC symbols that take a scratch, with the register peephole2
gave them, are the list a reordering pass should start from — `V90CP`'s C1
(`ecx`, `edx`, `edi`), both `printTitle`s, `V90Parameters`' C2, both
`setMappingParams`, `V90SignBitsExtractor`'s C2, `V34EqualizerCleanUp`,
`V90Resampler::setBllState`, `VPcmFloModem::setPcmSessionType`,
`applyPadGainToLinMapp`, `generateSymbol` and `dp_v8_exit`.

**WHY THE SHAPE, READ OFF THE OBJECT, IS NOT THE TEST.** `large_insn` is 8 for
`pentiumpro_cost` (`i386.c:251`), so `movl $imm32,disp8(%reg)` at seven bytes is
left alone and `movl $imm32,disp32(%reg)` at ten is split — visible in the
reproduction, where `+0x18`, `+0x20` and `+0x30` stay immediate stores and only
`+0x2f9c` and `+0x2fa0` split. Counting that emitted shape gives **218 of the
blob's 1,859 functions, 406 splits in all**, led by `v34handshak` at 19, and it
is a good quick read of the object. **It is also a bad predictor: over the 25
REGALLOC symbols it finds 4 where the exact test finds 12.** i386.md has 144
`match_scratch` sites and the read-modify-write group at 17685 is PPRO-tuned
too, drawing on the same cursor. Use the shape to understand what is happening;
use the two compiles to decide.

**How much of the effect this is, measured.** 177 of 184 swaps offered over the
47 C++ units in `src/pump/` with four or more definitions — the other 7 the
harness skipped, on a compile or a permutation that did not apply, and they are
counted on neither side: **16 bystanders differ with peephole2 and 8 without**,
and all
eight survivors are one instruction, `movl $imm,(%esp)`, with a different
`.rodata.str1.1` addend, which is a diagnostic string's pool offset moving with
the function order and CLAUDE.md's own trap rather than a register choice. All
sixteen were inspected. Over this sweep the cursor accounts for all of it.

**IT RETRODICTS 7772.** Two character-identical bodies each taking one scratch:
the first gets the register the cursor points at, the second the next one round
the ring. "Whichever body comes FIRST gets `%ecx`; the second gets `%eax`" is
that sentence.

**TWO DEAD HYPOTHESES. Do not re-try either.** The counters — `label_num`,
`DECL_UID`, insn UIDs — are at the same value in both compiles, and a swap
preserves them by construction. And allocation addresses: if they carried it,
changing when the collector runs would change the output, because `ggc-page`
frees pages that later allocations reuse. Five arms of `--param ggc-min-expand`
and `ggc-min-heapsize` give **one md5 for all five objects**, and the knob was
shown to fire first — `-fmem-report` reads 6040k of arena at the default
against 1520k at the aggressive setting, and the compile goes 0.131 s to
0.234 s collecting.

### 4. File-scope declaration order

GCC 3.4.2 emits file-scope objects in **reverse definition order**. Verify that
on our own object before leaning on it — it was checked ten-for-ten first.

Matching the two `.rodata` blocks **by content** paired all ten of
`v8_V21_Init`'s tables with no array differing, which proved the argument
assignment was already right and the *definition order* was the defect. Ten of
thirteen bytes (7765).

**The rule holds on BOTH compilers, which is what makes it usable.** A
three-variable scratch file gives `CCC, BBB, AAA` ascending under GCC 13 `-m32
-O3` and under GCC 3.4.2 at the tree's flags, so one source order satisfies the
period build and the modern one; had they disagreed, D392 would have closed as
unachievable rather than as fixed. The corollary is worth knowing separately:
`short[4]` gets 2-byte alignment from 3.4.2 and 4 from GCC 13 while the `.data`
section is 4 in both, so **a two-byte pad between two arrays is a
translation-unit boundary** under the object's own compiler — which is how
`COEF_DC` and `FPM_sin_sign` were placed in different units from the bytes
rather than the symbol table (3622).

**WHERE IT STOPS, AND IT IS A DIFFERENT KIND OF SYMBOL. A function-local
`static` does not record its declaration order.** `V92CP::bitsToInfo`'s two
statics come out `delta` at `.bss+0x0` and `gamma` at `+0x4` **with the
declarations in EITHER order** — both arrangements were built on the period
compiler and the layout did not move, so the blob's own order is not evidence
about the original's source and cannot be reproduced by reordering (6610).
Nothing in the tree observes those offsets, so the temptation is to write the
comment anyway; two files already carry that claim and one of them is now known
to be unfounded. **Check which kind of symbol you have before leaning on
lever 4 at all**, and 3622's own closing paragraph is the model for how far a
headline of this shape is entitled to go: it is one scratch file plus one real
one, so nothing should be built on it that a test does not check.

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

**AND CHECK THE CONVERSE BEFORE YOU WRITE THE EXPANDED SHAPE AT ALL: the
duplication may be the compiler's.** Thirteen hand-written convolutions were
about to go in because the object has thirteen. One `static` helper taking the
tap count and the shift as parameters, called from a switch with six constant
pairs, on the period compiler:

    static, -O2          1 copy, runtime shift, helper stays out of line
    static inline, -O2   6 copies, shifts $0xd $0xe $0xf $0x10
    static, -O3          6 copies, the same four shifts

which are the object's own four shift amounts (611). **But the compiler can
only do that if the source hands it constants** — reading the tap count out of
a state struct leaves nothing to fold, and `-O3` then takes our function from
454 bytes to 463 rather than towards 2,640. So the finding is not "write one
loop"; it is that the original's hand optimisation was expressed as literals at
the call site, and the expansion is downstream of that.

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
bucket and nobody was looking at; **fifteen more in wave 7** (7814), taking
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
DELETE-EXPRESSIONS (7816).** 7786's spelling C was `delete p` on a **POD**,
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
them the blob's tail-callee is not our last statement at all (7817).

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

**AND THE DEFINITION'S POSITION IN THE TU IS ITSELF A LEVER-3 CARRIER (7815).**
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
the spelling (7818).

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

**TWO MORE PLACES THIS HAS BEEN GOT WRONG, both worth checking before any
retype. The extension may belong to the ACCUMULATOR rather than to the
element.** `FPM_FSE_receive` reads `tilt_coeff[4]` and `tilt_hist[4]` `movzwl`
inside a 4-tap multiply-accumulate, and a `short` array feeding a 32-bit `imul`
normally gives `movswl`, so the naive reading is `unsigned short`. It is wrong,
and the experiment is cheap — the same `short` declaration with the
accumulation written two ways:

    int acc = 0; ... acc += c[j]*h[j]; out = (short)acc;
        -> movswl on both operands, and the `out = 0` store is DELETED
    out = 0; ... out = (short)(out + c[j]*h[j]);
        -> movzwl on both operands, and the zero store is KEPT

The kept store is `movw $0x0,0x76(%ecx)` in the object, so the accumulator is
the `short` field itself, only its low 16 bits are ever stored back, and the
extension is free (3581). **The presence or absence of a zeroing store beside
the loop is the tell**, and no differential test can separate the two spellings.

**And a CALLEE's mangled signature beats an inference from a free encoding.**
`V90SpectralShaper`'s two heap buffers were typed `unsigned short *` on the
strength of every access being `movzwl (%reg,%edx,2)`. The stride is real
evidence; the extension is not — every one of those loads has its 32-bit result
discarded by a 16-bit store. What is forced is that both pointers are handed
straight to `progress(const short *)` and `getMetric(const short *, unsigned)`
with no conversion instruction between the load and the push:
`_ZN24V90SpectralShapingFilter8progressEPKs` is `PKs`. An `unsigned short *`
would not convert silently in C++ at all — it needs a cast the object gives no
reason for. Evidence class 2 beats class 3, and `compare.py` did not move by
one symbol on the retype (5850).

### 9. Operand order — which is decided by the TREE, not by how you spell it

`return dsp->rx_energy & dsp->rx_tone;` — swapping the two operands gave byte
identity. That much has always been in this file. What was missing is that the
lever usually does not work, and why.

**THAT EXAMPLE IS THE FILE'S ONE UNCITED CLAIM, and the mechanism below does not
cover it.** No finding in the tree records it; a sweep for one found nothing.
It is also a bitwise `&` of two `COMPONENT_REF`s, *neither* of which is a DECL,
so the rule below cannot be what swapped it. It is kept because it happened,
and it is marked because the next reader will otherwise apply a comparison rule
to a bitwise expression and get nothing. **Everything that follows is measured
on COMPARISONS.**

**THE MECHANISM, AND IT IS THE ADVANCE TEST.** GCC 3.4.2's
`tree_swap_operands_p` (`fold-const.c`) returns "swap" when operand 0 is a
`DECL_P` and operand 1 is not. So `local > params->THRESHOLD` — a plain local
against a `COMPONENT_REF` — is canonicalised to `params->THRESHOLD < local`,
and **the threshold is what gets loaded into `%st(0)`**. Rewriting the source
comparison the other way round changes nothing, because both spellings fold to
one RTL. **Reading the member into a local first is the fix**: both operands
are then `DECL_P`, the first test returns 0, no swap happens. All six sites in
`checkSpecialSpectralConditions` took the object's own condition codes on that
one change, and five spellings were compiled and RUN against a real NaN before
it was believed — the plain form and a nested-`if` detect on a NaN; a local
threshold, a local array and a local struct do not (3529).

**AND UNDER `-mno-ieee-fp` THIS IS BEHAVIOUR, NOT CODEGEN.** The swap comes
with an inverted predicate, which is identical for ordered operands and
OPPOSITE for a NaN, so a comparison can be logically right, spelled every
available way, and still send a NaN down the wrong arm. `V90SdDetector::process`
was 304 failures of 32,262 and took `getV90Decision` down with it (2301);
`CalcErrorEnergyAfterEchoCancellation`'s keep-rate flag answered 0 where the
object answers 1 and was green over 24,509 checks until the accumulator was
seeded negative (4812). Only `make period` sees any of it — GCC 13 honours
IEEE for `>` whichever order it picks, so `make one`, `make test` and
`mutate.py` all pass either spelling (3529).

**WHERE IT STOPS, AND THIS IS THE HALF THAT WAS MISSING. Four measured
failures, all of them the TYPE and not the order:**

    GenericToneDetector::process   `a >= b` -> `b <= a`, and inverting the
                                   condition with the arms swapped: NEITHER
                                   moves the emitted branch.  GCC canonicalises
                                   operand order (1991)
    V90Equalizer high-error test   six probes; every `float` spelling gives
                                   `fcoms`+`jbe` and BOTH `long double`
                                   spellings give the object's
                                   `fcomp %st(1)`+`jae` (5701)
    displaySpectralParams sign     seven spellings; only the `long double`
                                   parameter emits `fldz; fcompp` (5823)
    advanceTrellis compare         six spellings; the two that give the
                                   object's `fcoms` are the two a differential
                                   test refuses (5855)

**So: vary the TYPE first and the operand order second, and compile the
candidates rather than reasoning about them** (5823, which is the second site
to say so independently). 5855 is the sharpest case — our operand 1 was a
`NOP_EXPR`, a `float` widened to `long double`, so it is not a DECL, GCC swaps,
and the `float_extend`-of-memory compare no longer applies because that pattern
wants the narrow memory operand SECOND. The order was never the free variable.

**A decoded operand order is worth a paragraph even when you decline it.**
5855's is recorded with all six cells and left alone, because the two matching
spellings are refused by a test and CLAUDE.md's rule is that the differential
tier decides.

### 10. Where a member's body is written — in-class is implicitly `inline`

A member defined inside the class body is implicitly `inline`, which moves it
from `--param max-inline-insns-auto` (100) to `max-inline-insns-single` (500),
and GCC 3.4.2 then inlines it nearly everywhere **while still emitting the weak
symbol** — so the symbol table looks right and every call site is wrong.

    Scrambler::reset      moved out of the class body   452 -> 455 identical,
                          nothing lost; V90Modulator::reset plus the two
                          constructors that call it                    (5805)
    Scrambler::process    inlined at all 19 sites the blob calls;
                          `V90Modulator::progress` +57 -> +3 instructions,
                          identical SET 488 before and 488 after        (7543)

**The tell is 7480's shape:** an EXCESS of instructions with a MISSING call.
Count `R_386_PC32` sites against the blob's for the template member; ours had
zero against nineteen.

**IT RUNS BOTH WAYS, AND THAT IS WHAT MAKES IT A LEVER.** `Descrambler`'s bulk
`process` STAYS in the class body, because the blob carries no
`Descrambler<...>::process` symbol at all and moving it out would make us emit
one the original does not have (7543). Read the object first.

**Where it stops:** `V90SpectralVerifier::printSpectrum` is already out of
line, and ours is inlined into `process` where the blob's is a real call.
Definition order was probed (moved after `process`: no change) and so was
translation-unit growth (padded with 400 unrelated functions: still inlined,
still 419 bytes, to the byte). Cause not found, recorded rather than chased
(5802). **Do not reach for `__attribute__((noinline))`** — that is fitting the
compiler, and it puts a construct in `src/` the original cannot have had.

### 11. The constant pool is a typed, per-function observable

`.rodata.cst4` against `.rodata.cst8` against `.rodata.cst16` names the literal's
type, and the load instruction says it again:

- `fldt` where the blob has `fldl` means we wrote `0.54L` and the author wrote
  `0.54` — three constants in `hamming<float>`, and `DspMath.cpp`'s own comment
  already recorded the object's operands as eight-byte slots (2903).
- `fmuls` and not `fmull` makes the multiplier single precision; a `double` 0.4
  would have gone to `.rodata.cst8` (4340).
- **A constant that is NOT there is evidence too.** The "OutputConversionFactor"
  print uses 1e5, 1e4, 2^30, 2^24, 2^20, 2^-16 and 2.0 and nothing near a
  thousand, though the format is `%03d`. The 1e3 multiply folded away, which
  happens only if the value was an `int` (2146).
- **A third x87 op where two would do puts a reciprocal in the source.**
  `fildll; fdivr %st(2),%st; fmuls` is `1.0/count * sum`, not `sum/count`, and
  the extra operation exists only because there is a constant 1.0 to divide
  (4340).

**Where it stops: the pool is emitted PER FUNCTION.** `output_constant_pool`
runs at the end of each function and `-fmerge-constants` leaves the folding to
the linker, so in a `.o` the duplicates are all still there — `0.5f` appears at
`+0x1b8` and again at `+0x1c4`, and 24804.0f twice three slots earlier.
**Two slots with the same bytes in one translation unit say nothing at all**,
and a reader who treats slot identity as expression identity will mis-read
every inlined float constant in this object (4340).

### 12. Spill width is forced; a value that never spills is not

The tree's standing rule is "the object keeps intermediates in registers and
never rounds them, so spell them `long double`" — `V90Equalizer.cpp`'s file
comment says so and 256 measured it holding for two step-size setters.
`enterPhase4` is the counterexample and it sharpens the rule:

    36b96:  fstps 0x38(%esp)      the getter's result
    36bd0:  fstps 0x38(%esp)      the DIFFERENCE, rounded to 4 bytes

Two `fstps` to a four-byte slot is two roundings to single precision. Written
with `long double` intermediates the transcript failed on EVERY non-zero offset
in the sweep and passed on zero — 48 of 1,092 checks — and two `float` locals
fixed all of them (2139).

**So the SPILL WIDTH is the evidence, not the presence of a spill.** A four-byte
`fstps` on an intermediate says the source had a `float` variable there. A value
that stays in a register between its producer and its consumer says nothing, and
that is why `long double` was right for the two setters. The first spill in
`enterPhase4` is forced by the following call clobbering the stack and carries
no information; the second is not, and it is the one that decides.

Note for `tiers.md`, not a change to it: that file's FREE column — register
allocation, scheduling — stands, and this tree now has two named exceptions to
it. A scratch-consuming `peephole2` (lever 3b) makes allocation steerable, and
a spill slot narrower than the value it holds makes a spill forced.

---

## What does not work

- **Renaming a variable.** Free for the compiler (7002). It changes nothing.
- **Reordering ONE function** on the strength of lever 3. That is what control
  4 of 7777 measured and it is right; what 7796 overturned is the conclusion
  drawn from it, so reorder a whole file's leaf block or nothing. Lever 3b says
  which files can pay: a file whose functions never store a constant into a
  field past +0x7f has no scratch to reallocate.
- **Getting closer.** See lever 1's stopping rule.
- **`__attribute__((noinline))`, `volatile`, or a cast added to make the output
  match.** Fitting the compiler. Three such shims were found and removed once
  the period build could adjudicate, one of which had changed `float`
  arithmetic to `double` — an alteration of the program and not of its
  compilation (1352, 1354, and lever 10's `printSpectrum`).

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
