# Refinement: what actually closes the last bytes

For experiment planning, source/flag interactions and stopping/reframing a
stalled search, first read [experiment-design.md](experiment-design.md).
This page supplies the individual levers; that workflow prevents a locally
successful lever from becoming an untested global assumption. A unique
preimage below is unique within its declared domain, not a claim that all
possible source/flag combinations have been excluded.

## Classify the compiler stage before searching register spellings

The [V.34 small-RTL study](../v34-small-rtl.md) separates several mechanisms
that can produce similar-looking assembly differences. Use a small control
and the corresponding GCC dump to identify the stage, then transfer the
hypothesis to the complete translation unit under the retained flags.

| Observation | Discriminating control | What the control establishes |
| --- | --- | --- |
| Incoming argument stays in a register instead of being reloaded from its stack slot | A small unsigned accumulator function with increasing register pressure; inspect global allocation dumps | Normal allocation can spill the argument home. A forced register or fabricated local spill is not recovered source. |
| Boolean result becomes a branch or setcc | Cross ordinary return/if forms with diagnostic if-conversion options and inspect ce1 | Source control flow and if-conversion are separate causes; a flag that changes many other bodies is a diagnostic. |
| A conditional store appears absent in a standalone helper | Compare the same helper after inlining and inspect GCSE/store motion | The caller may already recover the observed store. Do not adopt a standalone-only improvement. |
| Constant multiplication has the wrong instruction graph | Cross literal/const coefficients with ordinary coefficient locals, and inspect expansion then CSE | polyValue became exact at 28 bytes: late coefficient propagation folds an existing multiply, while early constants cause LEA/sub synthesis. Association alone did not recover it. |
| A comparison dispatch differs and source invents default values | Cross switch versus observed comparison chain with and without the invented initialization | preempindex became exact at 315 bytes only with descending comparisons and original unset locals. Keep supported inputs distinct from undefined caller-register state; byte identity does not define invalid inputs. |
| Two role loops appear in the blob | Compare one loop with the mode test inside against preselected taps and explicitly duplicated loops; inspect loop2 | V34scrambler became exact at 256 bytes through unswitching. Source arm order determines version layout; two binary loops do not establish two author loops. |
| A short initializer has a different loop/register shape | Cross index width, address expression, and independently observed store order | dpskDetectInfo1Init became exact at 152 bytes with a short index, root-relative clears and the blob's field order. Two address spellings matched: this is a family, not a unique original spelling. |
| A tiny coefficient loop differs from straight-line blob stores | Compare expanded cached pairs with scalar assignments preserving reloads | txrxdmainit became exact at 98 bytes only with the observed reloads; expansion alone was insufficient. Five fixed alias fixtures distinguish the old and recovered behavior. |

Keep the component boundary explicit. Those five alias fixtures are valid
inputs to a state-free leaf; they do not establish modem lifecycle coverage.
Do not invent an owner or traverse beyond a declared member array to obtain
a desired register. The FSK clear spans several fields and padding, so it
uses the complete object's byte storage rather than a fictitious array.

Audit the **purpose** of the comparison build before interpreting results.
Both make period and make tc must define DSPLIB_REPRODUCE_BUGS after
configurable flags. Without it, V.34's pre-emphasis selector changes from
initialized data to zero-initialized BSS. Canonical instruction comparisons
mask relocation addends and cannot by themselves detect this semantic
configuration mismatch. Preserve the invalid run, rebuild the unchanged
control, and report configuration gains separately from source gains.

A recovered owner can settle live register allocation while dead registers
remain compiler artifacts. V.34 FreezeEcho's real transmitter-prefix pointer
restores its SI base and frame; the last two bytes are dead pop destinations.
The peephole2 control identifies scratch selection, and evidence-backed local
definition orders leave those bytes unchanged. Keep the owner evidence and
scratch result separate; do not fabricate an aliasing view or migrate a type
merely to obtain a desired dead register.

Source widths can settle live coalescing without settling dead scratch bytes.
In txmitdibit, a real transmitter owner, direct state pointer and either a
short result or quadrant restore every live operand; one dead pop byte stays
different even after restoring the observed adjacent-wrapper order. The old
five-byte size gap concealed a copied state and non-tail call. Keep these
controls as evidence; do not adopt a broad owner migration solely to chase
that final scratch register.

A smaller size gap is not an adoption criterion. Splitting V.34 timing's
role-dependent switch into two switches brought the gap from 21 to 7 bytes,
but produced no exact function and changed another caller. It remains a
source hypothesis. Review exports, all changed bodies and the complete exact
set before adopting even a locally exact candidate. The FSK recovery kept
55 global definitions and raised the TU from 5 to 6 exact functions out of 29;
the large handshake remains non-exact.

## Reconstruct the owner before tuning its generated code

`FIELD`, `FIELD_PTR`, and width-specific offset macros are useful during
initial recovery, but they are not a completed instance model. Before
searching for a scheduling or register-allocation spelling, recover the
containing structures and their actual member types. Equal byte offsets do
not establish equivalent aliasing information for the compiler.

First distinguish missing types from unfinished migrations. V.32's sequence
code used genuinely unmodelled owners; V.22 still used raw offsets into an
owner whose constructor and shared structure had already been reconstructed.
Comments saying an instance is deliberately unmodelled can outlive that
decision. Check the current constructor and type home, not just the comment.

1. Establish allocation extent, initialization, call-site types and
   destruction together. Reuse existing embedded types and keep each owner
   definition in one shared home.
2. Preserve independently addressed aliases, pointer ownership and observed
   reloads. Replacing an offset expression with a member does not justify
   caching a pointer across a call or combining stores.
3. Name only what the evidence supports. Keep unmodelled gaps as pads and
   known-width, unknown-role fields neutral. Five accessor-visible registers
   and seven constructor clears do not prove a seven-element register bank.
4. Assert the period layout with checks that GCC 3.4.2 actually sees. Wider
   native pointers are a portability matter, not evidence against the
   original four-byte pointer layout.
5. Update mutation anchors without dropping their fault cases and run the
   period differential and structural gates before committing. An incremental
   prefix view is a migration step, not completion of the owner model.

Initializer cleanup is a separate question from arithmetic narrowing. A
cast on a hexadecimal constant assigned to a `short` array can be redundant
under the period compiler; the destination already performs that conversion.
That does not license deleting a cast that narrows a computed intermediate
before a call or store. Preserve the declared table type and validate the
cleanup with the deciding compiler.

See [the V.32 reconstruction checkpoint](../v32-structure-reconstruction.md)
for the first layout migration and its remaining owner work. This method
does not claim that modelling an owner alone guarantees byte identity.

Every lever here was measured in this tree, and every one carries its
counterexample. A lever without a known failure is a lever nobody has pushed
hard enough yet.

**The target is grade 0 — positional byte identity, per function.** Not byte
count, which moves when we emit more code rather than more of the *right*
code. Not instruction count: `qcLineVerification` measured 159 instructions
against 159 and was still wrong, a `movzwl` copied as 32 bits (finding F7630).

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

`make phase` does **not** build `build/tc_out` — it needs docker and the
toolchain image, which not every checkout has. `make byteident` and
`make similarity` DO, since both now depend on `make tc`
(`tools/toolchain/period.mk`), which is incremental: one edited source
recompiles one object. The staleness guard stays anyway, because it also
catches the directory being read by something that did not come through
make — it once reported pre-merge grades as current and they looked entirely
normal (7769).

---

## AIM A PASS AT A TRANSLATION UNIT, NOT AT A SYMBOL

The tree's own evidence says the compilation unit is the natural unit of work,
and every mechanism this file documents that acts at range does so through one:

- **Lever 3's carrier is a cursor threaded through a TU in emission order**, so
  fixing one symbol moves its successors. A symbol at emission index 0 cannot
  be reached by a reorder at all, and fixing an EXPOSED symbol early in a file
  changes every symbol after it (F7827).
- **Bystanders are the rule, not the exception.** `loadParams` -- 7,894 bytes,
  the largest such symbol in the tree -- closed as a bystander of a reorder
  aimed at something else (F7800). A pass editing `Scrambler.h` closed eleven
  symbols in a directory it had been fenced out of. `V92Modem::printTitle`
  closed with no edit to it at all, and was recorded as UNSTABLE for the same
  reason.
- **The regression runs the same way.** F8080's constructor fix cost
  `V90Jd::packData` 22 bytes as a cursor bystander, invisible to every counter
  the project prints (F8111).
- **The operator-definition position, the file's `static` helpers, and its
  declaration order are all TU-scoped**, not symbol-scoped (F7815, F7940,
  Lever 4).

**So brief a pass on a FILE and require it to score the whole file both
directions**, rather than on a list of symbols drawn from across the tree. A
symbol-drawn worklist makes every one of the effects above look like noise; a
file-drawn one makes them the signal.

**BUT SAY WHICH UNIT YOU MEAN, BECAUSE THE ANSWER DIFFERS BY WHAT YOU ARE
ASKING FOR (F8249).** The first pass deliberately scoped this way came back
with three scopes, not one:

- **The translation unit is the right unit for COST.** One container pass per
  file; that pass ran 58 cells in less wall time than a single `make phase`.
- **The INLINED HELPER is the unit for every mechanism that actually PAID.**
  `fse_rotate` has no symbol of its own and carries 196 bytes across two
  symbols; `fse_quality`'s single `&&` chain produces the same five-term
  signature in **six** symbols' censuses -- six rows of a symbol-drawn
  worklist, one construct. This is F7940 arriving from the other direction:
  the helper is invisible to a per-symbol view AND is where the defect lives.
- **A shape repeated ACROSS files is the largest thing left over.** The object
  computes `widx = next < limit ? next : 0` branchlessly in three
  `SMCv32_encoder_*` and in `TxNoCarrierV32` -- 34 bytes, five sites, two
  files -- which no single-file scope contains.

So: enumerate at the file for cost, hunt at the helper for yield, and expect
the residue to be cross-file. A brief that just says "work the TU" gets the
cheapest of the three and misses the other two.

The exception is a symbol big enough to be its own unit: a 3,000-byte function
is a file's worth of work on its own.

## The levers, in the order they have paid off

Levers 0 to 9 came out of the refinement waves, in that order. **10, 11 and 12
did not: they were learned once somewhere in the older record, written into one
finding and never generalised**, and were swept up afterwards (7813). Their
measurements are as real as the rest and their yield in a refinement pass is
unknown, which is the one thing to hold in mind when a brief quotes them.

**13 has the same caveat as 10 to 12, and it is stated here rather than
discovered later.** It closed 74 instructions in ONE function (F7940) and has
never been swept across the tree, so its yield in a refinement pass is
unknown -- exactly the thing the paragraph above says to hold in mind. The
MECHANISM is measured and the four-compile ladder is real; the GENERALITY is
not established. It is also what corrected lever 6's recorded negative, so
read the two together.

### Lever 0. What an enumeration proves depends on how many cells hit zero

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
**AND A DELTA OF ZERO IS A SUM, NOT A STATEMENT ABOUT EITHER SIDE.**
`--near`'s `+0` says the two instruction counts are equal; it does NOT say
nothing is missing. `V90Parameters::setToDefault` sat at a clean 695 against
695 and that total was **three differences cancelling** — x87 −18, integer
stores +19, clamping −1 — with the real cause six fields the header typed
`int` that are `float` (F7960). **Bucket by instruction FORM before trusting
the total**: count the `mov`-class stores, the x87 ops and the branches
separately on each side, and if those disagree while the total is zero, lever 2
does apply and is cancelling with itself.

Where the sub-counts also agree, `+0` does mean operand order, statement order,
emission order, or width and signedness. The lens is most useful on a **SIZE**
symbol, where it converts an apparently structural difference into an
arithmetic one; on a BYTES symbol it is close to redundant with the bucket.

**TWO CENSUSES EXIST AND THEY USE DIFFERENT TAXONOMIES; RECONCILE THEM BEFORE
QUOTING EITHER.** F7986 bucketed 29 rows of the V.90 cluster and found **22
with every bucket agreeing**; F8000 censused 47 rows of the DSP/V.34 span and
found the strictest class -- texts equal as a multiset, order differs -- true
of **three**. They do not disagree. F7986's "every bucket agreeing" is
F8000's `OPERANDS` + `PERMUTATION` (mnemonic multiset equal), which is 27 of
47 there against 22 of 29 here. **The rate varies by span** -- roughly
three-quarters in the V.90 cluster, a little over half in DSP/V.34 -- so
neither number is the tree's, and the census is worth re-running per worklist
rather than inherited.

Bucketing measured on one cluster: of 29 reachable `+0` rows, **22 have every
bucket agreeing** -- genuinely encoding only -- and **7 cancel**. None of the
seven had an x87 imbalance, which is the bucket that carried F7960's wrong
field types, so that defect did not repeat there (F7986). Note what that
leaves: for five of the seven the cancelling was bucketed but not TRACED, so
lever 2 is **bounded, not excluded**, on them.

**AND THE LENS DOES NOT BOUND THE MECHANISM — IT IS A WORKLIST, NOT A
DIAGNOSIS.** A `+0` row can be delta 0 by coincidence, and a symbol can LEAVE
delta 0 by being fixed. Both happened in one cluster: `V92Modem::progress` was
`+0` only by accident of two errors, and `V90Modem::progress` went from `+0` to
`-4` as its defect was corrected (F7983, F7982). The mechanism behind both --
sibling-call eligibility -- CHANGES the instruction count, so a worklist drawn
on `|delta|` would have dropped either had it been three instructions
differently wrong. Use `--near` to find cheap work, never to decide what a
symbol's defect can be.

- **No preimage** — **but FIRST ask whether the domain was drawn around the
  right code.** A no-preimage result licenses the strong conclusion "the
  difference is not what I thought it was", and that conclusion is only as good
  as the boundary you enumerated over. `packData`'s sixteen-cell domain covered
  its own four loops and was genuinely exhausted; the CRC lived in a `static`
  helper whose two loops were never in it, and the answer was there (F7940). A
  `static` helper that gets inlined has no symbol of its own, so a per-function
  enumeration cannot see it. **Enumerate over the callee set, not over the
  function that carries the symbol.** Only once the boundary is right does a
  no-preimage result mean the map is constant or simply misses. The difference is
  not what you thought it was. Four measured this way in one pass alone
  (7795), and `SpectralShaper::reset` at 10 cells with none reaching zero is
  the cleanest: by lever 1's own rule it is therefore not statement order.

**Enumerate before reading any cell.** Two passes have recorded doing that
explicitly, because stopping at a tempting near-miss is how an exhausted
enumeration turns back into a search.

### Lever 1. Statement order — enumerate, do not search

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

Closer bytes are not a grade. Finding F7782 is the ruling on this: take the
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

Closer bytes are not a grade. Finding F7782 is the ruling on this: take the
bytes when the space is exhausted and one element maps; decline when you are
searching. Record which side you are on and what the domain was — a finding
that says "closed by reordering" without saying how many spellings were
compiled is not reviewable.

### Lever 2. Instruction count at EQUAL byte size can expose a missing or extra statement

An instruction-count difference is a lead, not proof of a source statement.
Register allocation can add a return-register move; inspect operands and RTL
before assigning a source cause (see the V.34 boolean-return study above).
Once the extra instruction is an actual store, this check finds things no
functional test can:

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

**AND `--why`'s PADDING-STRIPPED COUNTS ARE THEMSELVES WRONG WHEREVER THE
FUNCTION CONTAINS A `mov %reg,%reg` (7848).** `byteident._padding` knows `nop`
and the `lea 0x0(...)` forms and **does not know the two-byte self-move**, which
is what GCC emits to align a loop head inside a function; `instrcount.py` does,
through its own `_SELFMOV` regex. So on `unitePhasesInfoOfUref` `--why` prints
**"204 against 204 with alignment padding stripped"** -- EQUAL, lever 2 does not
apply -- while the true code counts are blob 202 against ours 203, a real
**+1 EXTRA**. The blob has two self-moves there and we have one, so the error
does not even cancel. **`instrcount.py` is the authority for the count and
`--why`'s parenthesis is not**; where the two disagree, disassemble and count.
Fixing `_padding` would move grade 1 as well as the message, so it has not been
done inside a refinement pass.

**AND A DELTA OF ZERO IS A SUM, NOT AN INVENTORY -- IT LICENSES "THE ABSENCES
AND THE EXTRAS CANCEL" AND NOT "NOTHING IS MISSING" (F8000).** `--near`'s
delta-0 head reads as "same instruction count, so only the encoding differs",
and over a 47-symbol worklist that was true of **three**. Bucket each side by
instruction FORM before trusting the total:

    OPERANDS     24   mnemonic multiset EQUAL, texts differ
    CODE         20   mnemonic multiset DIFFERS while the total is zero
    PERMUTATION   3   texts equal as a multiset, order differs

`Scrambler<i,h>::process(bulk)` is `addl-1 dec-2 inc+1 jb+2 je-1 jmp+1 jne-1
mov-1 movzbl+1 xor+1` -- ten non-zero terms summing to zero -- and closing it
meant disbelieving the framing. `V90Parameters::setToDefault` is the same shape
independently: 695 against 695 was x87 -18, integer +19, clamping -1, and the
cause was six fields typed `int` that are `float`. So run the three-way census
(`Counter(text)`, then `Counter(mnemonic)`, padding AND self-moves stripped)
and read the per-mnemonic deltas; `movswl` against `movzwl` in unequal numbers
is lever 8 hiding inside a `+0` row. The framing pays on a SIZE symbol, where
it turns an apparently structural difference into an arithmetic one; on a BYTES
symbol it is close to redundant with the bucket you already have.

**A SECOND OBSERVABLE, INDEPENDENT OF THE BYTE GRADE:** the order of `.rodata`
strings a function references. It agrees or disagrees without reference to any
instruction, so it corroborates a statement-order decoding that the byte grade
alone cannot distinguish (7792).

**AND A TABLE THAT SEPARATES.** Where a function has two candidate differences,
compile the CROSS PRODUCT rather than one at a time: a cell that changes one
difference and not the other proves the two are independent, which no single
comparison can (7792).

### Lever 3. Definition order in the translation unit

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

### Lever 3a. The pre-check, before you permute anything (7801)

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

**THE RULE IS ABOUT PRICE, NOT ABOUT NULLITY** -- read the next paragraph
that way. Two pure block permutations were kept in wave 9a
(`V90ConnectionEvaluator.cpp`, `V90SpectralShaper.cpp`): each moves its file
to the blob's own emission order, changes **not one byte anywhere in the
tree**, and costs nothing beyond the permutation -- no macro hoists, no
preprocessor risk. They are kept because the order reached is a durable
measured fact that stops the next wave re-deriving it, which is 7796's
kept-neutral precedent. What follows is the case where the price was real.

**A NULL RESULT CAN COST TOO MUCH TO KEEP.** `VpcmFloModem.cpp` reached 16 of
16 and gained nothing, and reverting it left the tree at 474 -- but reaching it
had taken twelve macro blocks hoisted on top of the permutation. 7796's kept
neutral files were reorder-only. The measurement is the deliverable: record
that the order is achievable and pays nothing, and do not keep the diff.
Findings F7797 and F7798.

**A FILE-SCOPE `static` THAT CALLS A MEMBER FUNCTION SETS THAT MEMBER'S
EMISSION SLOT, so it is the exception to "statics live above".** Wave 6's
`V90AutoDigitalImpDetector.cpp` stopped at 23 of 34 with `isAltRbs` emitted at
index 0 against the blob's 10; the cause was `adid_recheckAltRbs`, a helper
THIS RECONSTRUCTION introduced, sitting at the top of the file with
`o->isAltRbs(...)` in its body. Moving that one helper below its own first user
took the file to 34 of 34. The object inlines the call, so the original had no
such edge -- our factoring was setting the order. Check for it whenever a
reorder lands short by a single symbol sitting at index 0. Finding F7798.

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

Shown firing on both injections and clean on everything committed. Finding F7799.

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

**THE CHEAP SCREEN FOR THIS LEVER IS WRONG, AND TWO PASSES FOUND IT WRONG
INDEPENDENTLY (F8042, F8067).** F8003 retired lever 3 for 21 of 22 files on the
test "does this file store a constant past `+0x7f`", the idea being that if
`i386.md:17507` never fires then no scratch is consumed. **Both halves fail:**

- **It is not the only `match_scratch` consumer.** The `add $imm,%esp` to
  `pop %reg` epilogue conversion takes one too, and the OBJECT ITSELF carries
  that conversion in all three cosine windows -- their prologue and epilogue
  counts do not balance (F8042).
- **The implementation counted the wrong side.** Counting surviving
  `mov $imm,disp>0x7f` in the FINAL object counts the sites peephole2 LEFT
  ALONE, the inverse of the intent. `V92Modulator.cpp` scores zero on it while
  the exact test finds two EXPOSED symbols in it (F8067).

**So do not screen -- run the advance test.** Compile with and without
`-fno-peephole2` and compare bytes; it is two compiles and it is the authority.
No number from the `+0x7f` screen should be quoted, and **lever 3 is NOT
retired for any file on the strength of it.**

### Lever 3b. The mechanism, settled: a round-robin cursor in `peephole2` (7812)

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
masked by the tune setting (`i386.h:263`), so **`-mtune=i686` — finding F612's
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

**DO NOT RUN THE TWO COMPARES INSIDE THE PERIOD CONTAINER (7845).** The
obvious implementation is a shell loop next to the two `g++` calls, and
**binutils 2.15 has no `objdump --disassemble=SYM`**: both sides come out
empty, `cmp` calls them equal, and every symbol in every file reads CLEARED --
the direction that licenses skipping work, so nothing downstream questions it.
One run reported **24 of 24 CLEARED over two files**; the same script over
`V92Transmitter.cpp`, whose D1 residual IS a peephole2-allocated `pop`
register, called that CLEARED too, which is what exposed it. Disassemble on the
HOST, score through `byteident.py`'s own `body()`, and have the tool print how
many symbols it found EXPOSED -- a run with zero is a run to distrust, not a
clean file. Rebuilt that way the same four files read 25 of 45 exposed.

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

**AND EXPOSED HAS A SECOND READING THE CERTIFICATE FRAMING HIDES (7827).** The
cursor is threaded in emission order and advances past whichever register each
split found free, so **changing what scratch a function consumes changes the
state that arrives at its SUCCESSOR** -- with no definition moving anywhere.
`Dtmf_Rx.c` was already 4 of 4 in the blob's `nm -n` order, so lever 3 had
nothing positional to offer; `reset_dtmf` (index 0, EXPOSED on `ecx`/`edx`)
was closed on its statement order alone and `create_cid_dtmf` (index 1,
EXPOSED on `ecx`/`edi`), which was not edited at all, went exact with it. The
file went 2 of 4 to 4 of 4 on one function's statement order. **7808's
corollary still holds -- a reorder cannot reach index 0 -- and it is about
POSITION, not about the symbol being outside the mechanism.** So: fix an
EXPOSED symbol EARLY in a file and re-measure the whole file before touching
anything below it, because its successors may have moved for free.

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

### Lever 4. File-scope declaration order

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

### Lever 4a. The ctor/dtor VARIANT DIGIT distinguishes a base from a member at offset 0

A public base subobject and a member at offset `+0` have the same address and
the same field offsets, so a header can declare either and no offset assertion
can tell them apart. `V90PreFilter.h` said as much: "nothing in the blob
distinguishes" the two.

**Something does. The Itanium ABI's variant digit.** GCC emits `C2`/`D2` for a
BASE subobject's constructor and destructor and `C1`/`D1` for a complete
object, so a member's ctor call names `D1` where an inherited base's names
`D2` — and the blob picks the base ones (F8080). That is a forced difference:
no spelling of a member produces `D2`.

Closed `V90PreFilter`'s D1 and D2 out of the RELOC bucket, which is the only
bucket where a differing relocation TARGET is the whole defect. A tree-wide
census, **shown to fire on the pre-fix tree by catching exactly the four known
sites**, then came back 0 of 1251 — and resolved `Psd` and `FloatIIR`'s
identical open notes in the MEMBER direction, which is the opposite answer and
equally forced.

Read the variant digit before deciding a base-versus-member question is
undecidable.

### Lever 5. Storage class, read off relocations

- A relocation against a **section** symbol (`.data`/`.rodata`) rather than a
  named one says the object was file-local: `static`.
- **`.data` rather than `.rodata`** says not `const`.
- Spacing between two tables says which came first.

All three at once on `RcFixed_Check_Combination` (7767). `nm` showed ours as
`R` before and `d` after.

**Where it stops:** `b103_ops` and `v23_ops` were established file-local the
same way, and adding `static` moved the relocations but **not the register
choice**, so neither reached grade 0 by it (7768).

### Lever 6. Unrolled and partially-unrolled code

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
- **`packData`'s difference is not loop shape** -- all 16 combinations of which
  of its four loops are written out were compiled; the maximum is 516 against
  the object's 534 (7785). **AND THAT EXHAUSTED DOMAIN WAS DRAWN AROUND THE
  WRONG FUNCTION.** The CRC lived in a static helper, and the helper's own two
  loops were never among the sixteen. Writing ONE of them out -- the
  fifteen-element shift -- took it from 75 instructions to 141 of the object's
  149, and lever 13 took it the rest of the way (F7940). So the reading "the
  domain is exhausted, this is lever 1's constant map" was wrong, and the
  lesson is the one lever 1 already states: **enumerate over the whole callee
  set, not over the function that carries the symbol.** A `static` helper that
  gets inlined has no symbol of its own and is invisible to a per-function
  enumeration.

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

**DUPLICATED LOOP BODIES ARE ALSO WHAT UNSWITCHING LOOKS LIKE, AND IT IS NOT
UNROLLING.** `v8_fskmodulate`'s object has a `test`/`je` at the top and then
*two* four-iteration loops with back edges -- one per value of the
loop-invariant `which` parameter -- and the earlier pass read that as "the
blob unrolls its 4-iteration loop" (F11381). It is `-funswitch-loops` (in
`-O3`), and the tell is that each copy keeps its `jle` back edge and the
bodies differ only in the invariant operand (`0x4(%ebx)` vs `0x2(%ebx)`). The
source that produces it puts the branch *inside* the loop; hoisting the
selection to a `step` local before the loop leaves nothing to unswitch and
emits one shared body. Enumerated over {`int`/`short`} x {indexed/advancing
pointer} x {two ternary polarities / if-else}, exactly two spellings map
byte-exactly (F11381). `-funroll-loops` and `-funroll-all-loops` do **not**
reproduce this shape and cost the tree 83-113 grade-0 exacts whole-tree; the
lever-6 negative for `-funroll-loops` stands and now has a second, independent
measurement beside it.

### Lever 7. `delete[]` versus an explicit guarded free

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

**THE MODERN BUILD NEEDS ITS SIZED-DEALLOCATION DEMAND WITHDRAWN.**
C++14 sized deallocation makes modern GCC call `operator delete(void *, size_t)`
for class deletion, while the period compiler uses the unsized form. The old
wave added guarded sized adapters in reconstruction source (7816); F7900
removed those apparatus blocks and recovered all200 period objects unchanged
with the modern-only `-fno-sized-deallocation` flag. Use that existing flag,
not new `__cplusplus` source shims. Preserve unsized inline adapter placement:
period-visible definitions can carry code-generation effects (7815).

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

**A declined language-dependent spelling can reopen after independent TU
recovery.** F7818's V92 deletion restriction was based on the C reconstruction
then present. F11404 recovered V92MappingParamsInt.cpp; its current fields are
typed int*/float* arrays. F11615's bounded deletion cross now closes both
173B/106B bodies, leaving all three bystanders unchanged. F11616 crosses
consistent new[] allocations: all deletion-bearing cells raw-merge, so paired
array expressions are an idiomatic supported family, not unique allocation
spelling. Keep the TU-local unsized host adapter position and original sizes,
guards, dangling slots and call order; real allocator lifecycle and whole-TU
data/export controls decide adoption. No language/layout forcing was needed.
[Recovery](../v92-array-lifecycle-recovery.md).

### Lever 8. Width and signedness — and the DESTINATION's declared type

7630's `movzwl` copied as 32 bits. Equal instruction count hides it completely.

**FIRST ASK WHETHER THE 32-BIT RESULT IS USED, because that decides which
question you are answering** (CLAUDE.md's forced-versus-free rule):

- **Used** — the extension is live and the difference is evidence about the
  loaded object's TYPE. Finding F613 is the precedent: a real defect no test
  could see, because both readings agree over every value the field holds.
- **Discarded** — a 16-bit value going straight back into a 16-bit slot. This
  is finding F614's free case, and it is NOT evidence about the field. Read on.

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

**A `switch` PROMOTES ITS CONTROLLING EXPRESSION; AN `if`/`else` CHAIN DOES
NOT — that is the width, and the FIELD's declared type does not change.**
Issue #158, `RxNextStateV17`'s IDLE-arm rate ladder. The object loads
`V17RXC_RATE_CODE` (`unsigned short`) with `movzwl` and then compares **16
bits**: `test %ax,%ax` / `cmp $0x1,%ax` / `cmp $0x2,%ax; sete; add $0x6`. A
`switch (RXCTL(modem)->rate_code)` emits `cmp $0x1,%eax` / `jle` / `cmp
$0x2,%eax` — the switch's controlling expression undergoes the integer
promotions, so the tree is SImode and the compare is 32 bits wide. Nine
spellings were compiled under the period flags and compared full-text, and the
`if`/`else if` chain **directly on the field** is the only one that maps:

    switch (RXCTL(modem)->rate_code)        32-bit cmp, SIZE 6 off
    short rate = (short)RXCTL(...)->rate_code; switch (rate)   movswl, 32-bit
    short rate = RXCTL(...)->rate_code; switch (rate)          movswl, 32-bit
    unsigned short rate = RXCTL(...)->rate_code; switch (rate) movzwl, 32-bit
    if/else if on RXCTL(modem)->rate_code                     movzwl + 16-bit  <-- the object
    short rate = (short)RXCTL(...)->rate_code; if/else chain   movswl + test %eax
    unsigned short rate = RXCTL(...)->rate_code; if/else chain movzwl + test %eax

This is not a narrowing of a computed value: the lvalue read is the same
`unsigned short` field in every cell, and the object's own `movzwl` says
unsigned. What the chain changes is where the promotion happens, and the
HImode compare is what the compiler was FORCED to encode from it. Applying it
to `RxNextStateV17` took the symbol from SIZE (6 bytes / 4 instructions) to
all bytes equal with one section relocation unresolved, and `RxHdxScramV17`'s
expiry ladder to the object's shape. **The same substitution is worth checking
wherever a narrow lvalue is `switch`ed and the object compares it at 16 bits.**

**AND THE LADDER IS NOT `RxHdxScramV17`'s WHOLE RESIDUAL.** The expiry
countdown is a separate, pre-existing difference with its own cause:
`left = (short)((unsigned short)RXCTL(modem)->countdown - 1); ... if (left > 0)
return 0;` emits `movzwl; dec; cwtl; mov %ax; test %ax,%ax; jg` where the
object has `movzwl; dec; test %cx,%cx; mov %cx; jle` — a sign-extension and a
test/store order the ladder change does not touch. It is shared with
`RxHdxBridgeV17` and `RxHdxPrtcolV17`, which carry no ladder at all, so it is
not this lever and not #158. After the ladder fix `RxHdxScramV17` is 270 bytes
against the object's 266 and the three handlers are all 210/210 in the twins
with 79 bytes differing. Recorded, not acted on.

**#160 ENUMERATED THAT COUNTDOWN AND IT IS NOT THE WHOLE RESIDUAL — MEASURED,
NOT ARGUED.** Fifteen spellings were compiled with the exact period toolchain
and compared full-text against the blob. The object's `movzwl 0x1a(%edx),%ecx;
dec %ecx; test %cx,%cx; mov %cx,0x1a(%edx); jle` IS matched in width and
signedness in isolation: a read-back or pre-decrement on the field,

    RXCTL(modem)->countdown = (short)((unsigned short)RXCTL(modem)->countdown - 1);
    if ((short)RXCTL(modem)->countdown > 0)
        return 0;

emits `movzwl 0x1a(%edx),%esi; dec %esi; test %si,%si; mov %si,0x1a(%edx)`,
removing the `cwtl` and the 32-bit test, and takes the handlers to 269/209/209.
But **no countdown spelling is full-text identical**, because the carrier arm is
laid out in the opposite order: the blob falls through from `test %eax,%eax`
into the success path (`je` to the out-of-line error arm), ours into the error
path (`jne` to the success arm). That reordering is independent of the countdown
— all fifteen spellings keep the error arm first — and predates #158, which
touched only the ladder. The register swap (`%ecx` in the blob against
`%eax`/`%esi` here) is its consequence, not a second cause. `byteident.py`
agrees: the width fix removes the `cwtl` but the handlers keep one extra
instruction each (blob 76/58, ours 79/60 at grade 0). Full-text identity needs a
second lever on the carrier `if` (a success-as-then restructure does flip the
blob's `je` and lands on the object's `%ecx`, but still hoists the return-0
`xor`); that is a different finding, not this one. **Declined, and #160 does not
close on the countdown alone.**

**#162 RECOVERED THE PAIR, AND THE MISSING LEVER WAS THE RETURN-0's SHAPE.**
The carrier `if` and the countdown are needed TOGETHER: with both right all
three handlers are grade-0 byte-identical (`RxHdxScramV17` 266 B, the twins
210 B each). The object's carrier block is:

    call CarrierDetectV17
    test %eax,%eax
    je   <error, out of line>          success falls through
    orb  $0x20,0x29(%edi)              flags |= CARRIER
    mov  0x5c(%edi),%esi               RXCTL
    movb $0x1,0x28(%edi)               status = CARRIER
    movzwl 0x1a(%esi),%ecx             countdown, loaded UNSIGNED
    dec  %ecx
    test %cx,%cx                       tested at SIXTEEN bits
    mov  %cx,0x1a(%esi)                stored as sixteen
    jle  <expiry, out of line>
    xor  %eax,%eax                     ONE return-0, shared
    <epilogue>
    ret
    <error>: ... fields ... jmp <the xor>
    <expiry>: ladder (Scram only); GetSNRV17; RxNextState;
              movswl %bp,%eax; jmp <epilogue>

The carrier order is the success-then form (`if (CarrierDetectV17(modem) != 0)
{ ... } else { ... }`), which flips the `je` and the register assignment as
#160 measured. What #160's success-as-then still hoisted is the `xor`: written
with an early `return 0` inside the then-block, GCC 3.4.2 materialises the
return value in `%edx` at the TOP of the success block and emits a SECOND `xor`
in the error arm, so the object's single shared return-0 never forms.

**A `short rc = 0;` AND A SINGLE `return rc;` AT THE END IS THE FIX.** The
success body puts the expiry behind `if ((short)RXCTL(modem)->countdown <= 0)
{ ... return (short)n; }`, the error arm sits in the `else`, and the function
returns `rc` once. That creates the shared return-0 block the object has: the
countdown's fall-through and the error arm both reach one `xor %eax,%eax`. The
countdown read-back (#160's) supplies the 16-bit `test %cx,%cx` and removes the
`cwtl`.

**THE CROSSED ENUMERATION, all three handlers compared full-text under the
period toolchain.** Carrier spellings: guard/early-return (the pre-#162
source), success-then/else-error, success-then with the error after,
`!CarrierDetectV17` guard, error-in-else-with-shared-tail, and the `goto` shared
tail. Countdown spellings: the `short left` local and #160's read-back. The
`short left` column never closes (the `cwtl` and 32-bit test remain); every
spelling except one keeps the error arm first or duplicates the return-0. The
one cell that maps onto ALL THREE is `short rc = 0;`, success-then/else-error,
read-back countdown, expiry as the then-block:

    shape               cd         Scram          Bridge         Prtcol
    guard (pre-#162)    left       SIZE 4         BYTES 79       BYTES 79
    guard               readback   SIZE 3         SIZE 1         SIZE 1
    success-then        readback   SIZE 16        SIZE 1         SIZE 1
    success-then/else   readback   SIZE 16        SIZE 1         SIZE 1
    goto shared tail    readback   EXACT          SIZE 18        SIZE 18
    rc + single return  readback   EXACT          EXACT          EXACT

Within the tested domain the preimage is unique: only the `rc` cell is
full-text identical for all three, and no other cell closes any of them. The
twins were NOT folded (F9444); each body is written out and each is exact on
its own.

**MEASURED RESULT.** `byteident.py` grade 0: the three handlers are EXACT,
`v17.c`'s own exact set 6 -> 9 with nothing lost, whole tree 829 -> 832.
`make phase`: `period differential: 375 passed, 0 failed`, `phase boundary:
period differential and structural checks all OK`. The one ratchet loss on the
tree (`_ZN13V90ParametersC2EP19_tagModemParameters` and two `V92Modulator`
constructors) is PRE-EXISTING and reproduced with the pre-#162 `v17.o`, so it
is not this change. #162 closes.

### Lever 9. Operand order — which is decided by the TREE, not by how you spell it

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

### Lever 9a. And a NULL over a whole file family is a result — prove the harness fires first

Lever 3 was run to exhaustion over eight small `fpm_*.c` files that were NOT
in the blob's emission order — seven at 3! and one at 4!, each maximal run of
`static` definitions glued to the block below it so a static never lands under
its first user. **Seven of the eight give ONE distinct emission over their
whole domain**; the eighth gives two and neither closes anything.

The null was only believed after the detector was shown to FIRE, which is
finding F134's argument applied to an enumeration: compile the
block-REVERSED file and print `nm -n` beside the byte comparison.

    fpm_sre.c   order  init,free,recover -> recover,free,init   MOVED
                bytes  every symbol IDENTICAL
    fpm_sdm.c   order  init,scram,descram -> descram,scram,init MOVED
                bytes  every symbol IDENTICAL

That is 7797's ruling with the measurement in hand: the order is achievable
and pays nothing, so record it and **do not keep the diff**. An enumeration
that reports "0 of 6 cells" without showing that any cell differed from any
other is indistinguishable from a broken generator.

### Lever 10. Where a member's body is written — in-class is implicitly `inline`

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

**RUN OVER A WHOLE FAMILY IT IS STILL THE BEST-PAYING LEVER HERE (F8001).**
`Scrambler`/`Descrambler`'s remaining in-class members -- both `process`
overloads, `processAllOnes`, `processAllZeros` and every constructor and
destructor -- screened blob 117 against ours 0, and 105 of the 117 sites were
traced to functions this tree defines rather than left as 7867's
unwritten-caller noise. Moving all of it at once gained **10 EXACT with nothing
lost**, every one a bystander in a directory the pass never edited. Two rules
came with it: move the whole family, because 7866's half-move made four bodies
worse invisibly; and APPEND the definitions at the foot of the header, because
7815 is the cost of moving anything already in it.

**IT IS LIVE, AND THE SCREENING TEST IS A RELOCATION COUNT (7867).** 7831's
measured NO above is real and is about how the lever was REACHED: that pass
chose it from a brief and went looking for somewhere it might apply. Read the
other way it paid for almost a whole pass -- `Scrambler`'s
`resetHistoryIndexes` (7862) and `copyHistoryTail` (7866) are both in-class
bodies the OBJECT calls, and finding the call is one command over both
objects:

    grep -c 'R_386_PC32.*<mangled member>'    ours 0, blob 9

Zero against nine is what `copyHistoryTail` looked like while `nm` showed all
five of its symbols present, which is 5805's warning exactly. **Do not judge
which members "look inlineable"; count the call sites for every member defined
inside a class body.** And 7866 is the trap on the way out: fixing ONE of two
helpers called from the same conditional made four `process` bodies worse and
**no bucket moved, because SIZE to SIZE is invisible to a set diff** -- read
the byte counts beside it.

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

### Lever 11. The constant pool is a typed, per-function observable

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

**FIRST MEASUREMENT IN A REFINEMENT PASS, AND IT IS A NO (7831).** A pass
briefed that its set was float-heavy and that this lever was the one most
likely to pay found nothing, in two different ways worth separating. **The
job of a file does not predict its arithmetic**: `v34filters.c` has ZERO x87
instructions over all 26 of its symbols in both objects and the TU has no
`.rodata.cst4`, `.cst8` or `.cst16` section at all -- it is fixed-point
`short`/`int`. Census the file; do not infer. **And `hamming<float>` above is
SPENT**: the pool types agree today, `fldl` 3 against 3, so an earlier pass
took it and only the first bullet was ever live there. What remains in that
symbol is the fourth bullet, the x87 ARRANGEMENT -- the blob spends
`fxch %st(3)` and divides at stack depth 3 where we divide at depth 1 -- and
it is a CONSTANT MAP: 18 spellings (where the loop's three constants are
declared x how the reciprocal is written x the multiply's operand order) give
ONE distinct emission and not one byte moves. Read the divide's BYTES, not
objdump's mnemonic (245): the blob's `de f2` prints `fdivp` and IS FDIVRP.

**Where it stops: the pool is emitted PER FUNCTION.** `output_constant_pool`
runs at the end of each function and `-fmerge-constants` leaves the folding to
the linker, so in a `.o` the duplicates are all still there — `0.5f` appears at
`+0x1b8` and again at `+0x1c4`, and 24804.0f twice three slots earlier.
**Two slots with the same bytes in one translation unit say nothing at all**,
and a reader who treats slot identity as expression identity will mis-read
every inlined float constant in this object (4340).

**FIRST TEST ON A GENUINELY x87 SYMBOL, AND IT FOUND A NEW FAILURE MODE
(F8041).** `hamming<float>` was this lever's own named example and is SPENT --
`fldl` 4 against 4, fixed by an earlier pass. `hanning<float>` did have the
defect, and fixing it **moved no grade at all**: `6.283185307179586L` puts the
constant in `.rodata.cst16` reached by `fldt` where the object has `.cst8`
reached by `fldl`, and those are **the same six bytes in the function**. So a
real recovery here can be VERDICT-INVISIBLE, and the only thing that sees it is
the constant pool's SECTION, not the instruction stream. Verified bit-exact
against the blob's own `ref_hanning` over n = 0..300.

### Lever 12. Spill width is forced; a value that never spills is not

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

**AND ITS PREMISE FAILS ON THE FIRST SYMBOL IT WAS AIMED AT (F8043).**
`blackman<float>` was chosen because it carries `sub $0x14` where the object
has `sub $0x4`, read as a spill-width difference. It is not: the object's
`sub $0x4` is **ABI alignment** -- push, push, sub 4 puts `%esp` at 0 mod 16 --
and there is no `fstps`/`flds` through `(%esp)` anywhere in its body. **A frame
size is not a spill width.** Look for an actual spill through the frame before
reading a frame delta as this lever.

### Lever 13. What the compiler can hoist out of memory depends on the SYNTACTIC FORM of the reference, not on the alias set

`V90Jd::packData` keeps `int crc[16]` -- a member -- out of memory for the
whole of its group loop: sixteen loads into stack slots before it, both groups
run on the slots, sixteen stores after, not one store to `0x4c(%edi)` between.
That is 74 of its 149 instructions and the whole of its `sub $0x48,%esp` frame.
Reproducing it needs two things in the source, and the second is the one nobody
expects.

**1. Every subscript must be a constant.** Rolled as `for (i = 0; i <= 14;
i++) crc[i] = crc[i + 1];` no element has a loop-invariant address and nothing
can be promoted at all. Written out, they all do. This is lever 6.

**2. Both sides must be MEMBER SUBSCRIPTS of the same object, not pointers.**
Through a helper's `int *crc` and `const unsigned char *in` -- the obvious
factoring -- the two arrive as indirect references that GCC 3.4.2 cannot tell
apart, so the stores have to stay in the loop in case the byte read aliases
them, and the promotion collapses to a per-group reload. Written `crc[i]` and
`bits[...]`, they are component references to different fields of one record,
GCC proves they cannot overlap, and the stores sink. The four-compile ladder,
instruction count against the object's 149:

    75   helper with pointers, shift rolled
    141  shift written out, helper unchanged
    137  helper given the object but still doing `int *crc = jd->crc;`
    149  shift written out AND both sides member subscripts

**IT IS NOT AN ALIASING PROBLEM IN THE C SENSE, and that is worth knowing
because the C answer is the intuitive one and it is wrong here.** Casting the
input read to `const int *` and then to `const float *` -- the latter genuinely
cannot alias `int` under `-fstrict-aliasing` -- moved nothing while the
pointers stayed pointers. An alias-set argument is not a substitute for the
syntactic form. Finding F7940.

**AND DO NOT HAND-WRITE THE INDUCTION VARIABLE THE OBJECT SHOWS.** The same
loop's object code is `movzbl (%ebx),%eax; inc %ebx` with a `dec %esi; jns`
countdown, which reads like a source-level pointer walk. It is not: spelling it
`*in++` costs condition 2 on the input side and lands at 136 instructions,
thirteen short. The walking pointer and the countdown are what strength
reduction MAKES of the subscript, so writing them by hand removes the
information the compiler needed and then hands back what it would have derived.
Finding F7941, and it is lever 9's rule -- act on what the compiler was forced
to encode -- applied to an induction variable.

**AND A MEMBER REACHED THROUGH A CAST HAS A THIRD FORM, where the obvious
cleanup is NOT neutral -- BUT THE NEUTRAL FORM IS NOT THE DEREF SPELLING.**
Issue #141 removed the cast-hiding macros from the fax read paths, and one site
resisted. `RXS(modem)` is `((struct v17rx *)(modem))->state`, so
`(&RXS(modem)->agc.value)->cfg.alpha++` is an address-deref through that cast.
Rewriting it to the direct-dot `RXS(modem)->agc.value.cfg.alpha++` -- dropping a
redundant parenthesis only -- grew `RxNextStateV17` by 16 bytes, and #141
recorded the address-deref as the object's form on that ground. Issue #152
enumerated the forms and the ground does not hold.

**THE 16 BYTES ARE THE COST OF RE-EVALUATING `RXS(modem)`, NOT OF THE DOT.**
The direct-dot re-loads `modem->state` between the two statements -- an extra
`mov 0x60(%ebx),%edx` and the register/alignment shift it forces -- where the
address-deref form CSEs one load across both. A cached state pointer is
neutral: `struct v17rx_state *rxs = RXS(modem); rxs->agc.value.cfg.alpha++;
rxs->agc.value.cfg.beta++;` is byte-for-byte identical to the address-deref
form over the whole function, as is `(&rxs->agc.value)->cfg.alpha++`, and so
are the address-deref, the double-address-deref and the parenthesised-dot
spellings. The object's own increments are `addl $0x2,0xc0(%edx)` /
`addl $0x2,0xc4(%edx)` -- base is the STATE pointer, offsets from `agc.value` --
and every one of those forms emits exactly that. Caching a pointer to the
SUB-object instead (`struct fpm_agc *a = &RXS(modem)->agc.value;`, or
`struct fpm_agc_cfg *c = &...->cfg;`) emits `addl $0x2,0xc(%edx)` /
`0x10(%edx)` and is excluded by the object's offsets. So the preimage is NOT
unique, and the +16 does not establish the address-deref form.

**THE WHOLE-FUNCTION CONTROL WAS UNAVAILABLE, AND THAT IS PART OF THE RESULT.**
At HEAD `RxNextStateV17` is 845 bytes against the object's 730 because
`Restore_rateV17` and `StoreCoefV17` are INLINED here where the object CALLS
both -- an inline-boundary difference unrelated to this field. The +16 and the
equality of the cached forms are therefore measured with the rest of the
function held fixed, not by full-text identity with the blob. The current form
is retained because several forms are byte-identical; it is not uniquely
recovered. Issue #141, #152.

### Lever 14. The TRANSLATION-UNIT PARTITION is a source property the object records

`src/service/voice.c` held the whole ring detector, and `RD_delete`,
`RD_process` and `RD_ring_details` could not be made exact at `-O3`: ours
inlined `RingDetector_Delete`/`RingDetector_Process`/`RingDetector_GetLastRing`
into the wrappers, while the object CALLS all three. The tempting answer was a
flag -- the global `-fno-unit-at-a-time` profile does recover those three -- but
the object says something structural. Its `STT_FILE` records are

    voice.c, fax.c, rd.c, ringDetector.c

in that input order, and the wrappers are in `rd.c` while the detector is in
`ringDetector.c`. `-O3` cannot inline across a TU, so the reference's calls are
not a compiler choice at all; they are the file split. Moving the two groups
into their own files, bodies unchanged and no flag touched, is **7 -> 10 EXACT**
in that TU and **822 -> 825** over the tree, denominator unchanged. Finding
F11351.

**What to look at, and why the usual maps will not show it.** The authority is
`readelf -sW`'s `FILE` records (or `docs/modules.md`'s source, `tools/tumap.py`),
NOT a span name and NOT `docs/attribution.md`'s prefix matching -- those merge
several input files into one `bracket` extent whenever the inner TUs have no
local symbol to anchor them, which is exactly the silent case. `ld -r` keeps
the `FILE` records in input order, so they answer both "was this two files?" and
"in what order?".
**The map generator had this wrong by a bug of its own, now fixed** (F11352,
#67): an anchored TU's `exact` extent was extended to the next anchor and the
intervening TUs' globals were reassigned to it, which is how `voice.c` came to
own `rd.c`/`ringDetector.c`. `tools/tumap.py` now reports the anchored TU's
own local-symbol envelope as `exact`, the gap as a shared `bracket` with its
candidate TUs named, and globals in that gap as ambiguous. `readelf`'s FILE
records remain the authority, but `docs/modules.md` no longer contradicts
them.

**The tell is a call boundary, not a byte count.** A wrapper that `SIZE`s small
because it inlined a helper, or a constructor that `SIZE`s large because it did
not inline one the reference contains, is the signal; so is an exact function
that gains or loses a helper call under an unrelated change. Reach for this
lever before the flags, and when you do move a function, split its `t_*`
mutation suite with it: `anchorcheck` reports the detached anchors as "matches 0
time(s)" and a suite whose anchors now live in two files has to become two
suites. The same question for the rest of the object is #6/#20.

### Lever 15. A header-only template has NO translation unit — the user instantiates it

Lever 14's FILE records answer "was this two files?"; they also answer "was
this a file AT ALL?". A class template whose members are defined in the header
has no `.cpp` of its own — its weak `.gnu.linkonce.t` symbols are emitted by
whichever TU instantiates them. Reconstructing it as an out-of-line `X.cpp`
with member-wise explicit instantiation is a plausible-looking file the object
never had, and it is detectable two ways.

**The absence of the FILE record is evidence, not an artefact.** `ld -r`
preserves a `FILE` record for an object with NO symbols at all (measured: a
partial link of our own emptied stubs still carries one), so if there is no
`X.cpp` in `readelf -sW`'s `FILE` records, there was no `X.cpp`.

**The second tell is C2/D2.** Explicit member instantiation of a constructor
and destructor emits the base-object variants C2/D2; an implicitly
instantiated class in the blob has only the referenced C1/D1 (F601). The two
spellings emit the SAME function bodies, so the exact-byte metric cannot see
the difference — the partial object's symbol set can.

**The mechanism.** Move the definitions into the header and DELETE the
invented `.cpp`. Do NOT add `inline`: with it GCC inlines five of the six away
and their out-of-line symbols vanish; without it they are emitted exactly as
the object has them. Keep the body out of the class (in-class is Lever 10 and
inlines nearly everywhere) and keep `__attribute__((noinline))` only where the
object already forces it (`Queue::reset` is called by the constructor).

**Measured (Queue, F11355).** `Queue<float>` had been a `src/dsp/Queue.cpp`
with explicit instantiation. There is no `Queue.cpp` FILE record; the only
references anywhere are `V92Modulator`'s `reset`, `progress` and
ctor/dtor. Moving the definitions into `Queue.h` without `inline` and deleting
`Queue.cpp` gives a partial object with no `Queue.cpp` FILE record and exactly
the blob's six Queue symbols, no C2/D2, where the `.cpp` form added two
sections and a FILE record. `make phase` green. The function bytes are
unchanged by the move — `Queue.cpp` and `V92Modulator.cpp` emit identical
counts — so this lever fixes WHICH TU owns a symbol and the symbol SET, not
the residual bytes.

**The family audit is DONE (F11356, issue #74).** `Agc.cpp`, `DiffCoder.cpp`,
`DspMath.cpp`, `LowPassFIR.cpp`, `Scrambler.cpp` and `SineWave.cpp` all lacked
a FILE record and all six were the same invented TU; their definitions now
live in the matching headers without `inline` and the `.cpp` files are gone.
`make phase` is green and the partial object carries no invented FILE record
and no C2/D2 for any of them. Two consequences worth carrying to the next
family:

- `operator delete[]` stays one copy per `.cpp` (F7815). `DiffCoder`'s
  parallel coders and `LowPassFIR` free with `delete[]`, so the TUs that now
  instantiate those destructors -- `Resampler.cpp`, `V90SignBitsExtractor.cpp`,
  `V90SpectralShaper.cpp` -- needed a local definition, and the direct tests
  needed theirs as apparatus.
- **A function template's codegen can depend on the instantiating TU's
  flags.** `DspMath`'s `sinc` expands `sin()` to `fsin` only under
  `-ffast-math`; a fixture that includes the header and calls the function
  instantiates it under the fixture profile and disagrees with the blob.
  `t_dspmath` therefore carries `test/unit/t_dspmath.cxxflags`, read by both
  build systems, rather than calling the source TU's symbol from the test.
  Check the flag dependency before moving a function template whose body uses
  a transcendental. See #74's F11356.

### Lever 16. A holder declared with the wrong pointer type hides casts that retyping removes

When a local, field or parameter holds one specific struct pointer but is
declared as a generic or wrong pointer type, every use site casts to the real
struct. Retyping the holder to the real struct type is **codegen-neutral** --
pointer types are not observable in the emitted code, and the symbols here are
`extern "C"`, so no mangling or DWARF recovers the declared type -- and it
lets the casts go.

Measured on `DemodDataV17` (`src/fax/v17.c`): `rxs` held `RXS(modem)`
(`struct v17rx_state *`) but was declared `unsigned char *`. Compiling the
file on the Gentoo period compiler with each of `unsigned char *`, `void *`
and `struct v17rx_state *` gives the **identical** object (`94c51dc0...`).
Retyping to `struct v17rx_state *` removed three `((struct v17rx_state *)rxs)`
casts.

The type is a fidelity CHOICE, not a recovery. Among the candidates keep the
truthful struct type over `void *` (a convenience that keeps the casts) and
over `unsigned char *` (not an established idiom). The retype also removes a
modern-compiler rejection -- GCC 14's `-Wincompatible-pointer-types` is what
stopped `make coverage` at this file.

Do NOT collapse these: a `void *`/`char *` holder cast to DIFFERENT struct
types at different sites (one generic pointer, several targets), and a
`char *`/`unsigned char *` used deliberately as a byte pointer. Decide per
holder from its USES, not from the declaration. A retype that is not
codegen-neutral is a different finding -- record it and leave the cast.

Issues #140 (tree-wide retyping audit) and #141 (replace the cast macros this
hides behind -- `RXS`, `RXS_SRE`, `RXS_FSE`, `RXSTATE`).

## Narrow where the object narrows, not earlier

`InitGenSequence` provides a small, exact discriminator. Its reconstruction
used an `unsigned short` local for `total / width - 1`, introducing a
three-byte `movzwl %ax,%eax` absent from the reference. The reference instead
keeps the promoted result full-width until two 16-bit stores.

Both `int` and `unsigned int` locals reproduce the complete reference body.
Plain `int` follows the expression's integer promotions without an extra
conversion, and is the retained spelling. The destination fields remain
unsigned shorts; this is not evidence to widen or retype them. When
`total < width`, the result is still stored as `0xffff` in both fields.
Division-by-zero and oversized-shift behavior are not repaired by this edit.

The full-TU controls gain one exact function with no other body changes.
See [the retained V.32 result](../v32seq-top-retained-result.md) for gates
and the separate, unretained floating-absolute-value investigation.

## Separate an element count from its byte-size conversion

The `Scrambler<int, unsigned char>` constructor showed a three-byte
allocation-arithmetic difference: the reference adds the guard element and
then shifts, while the reconstruction uses a scaled address with a byte
displacement. Reassociating the sum did not alter the output. An ordinary
`unsigned int count = b + c + 1` followed by `count * sizeof(T)` reproduced
the reference constructor exactly, without changing the allocation amount.

This is evidence for a source intermediate, not permission to add casts,
volatile objects, barriers or arbitrary locals to force a score. The finite
domain establishes one supported spelling, not a uniquely recovered original.
All 30 header-consumer controls reproduced their baselines; only the two
`Scrambler<int, unsigned char>` constructor copies changed. Other
specializations, including every `Descrambler` copy, remained unchanged.
See [the retained result](../scrambler-allocation-retained-result.md) for
the shared-header scope, period gates and full-object exact-set comparison.

## Distinguish a cached pointer from repeated member loads

The parallel decoder independently exhibits the same distinction: its
reference body caches `state_`, advances input/output/state pointers, saves
the input byte before writing output, then updates state. Applying that
observed structure reduces its 62-byte reconstruction to 58 bytes against
the original 57. The residual is an extra byte in the load/XOR sequence,
not evidence to add a cast, barrier, or aliasing promise. All 18 header
consumer controls reproduce their baselines; only the decoder helper changes.
See [the decoder result](../parallel-decoder-retained-result.md) for the
period gates, whole-object comparison, and explicitly rejected invalid run.


An unchanged address calculation is not the same evidence as an unchanged
memory access pattern. In `ParallelDifferentialEncoder<unsigned char>::process`,
the object reads `state_` once before the loop, advances state/input/output
pointers, and stores a local XOR result to output before state. The former
indexed reconstruction reloaded `state_` twice per iteration, mutated state
first and then reloaded it for output. Its loop bound, unlike the state pointer,
is a member read on every iteration in the reference and must remain one.

A cached state cursor and local result recover that evidenced structure.
This is not the same justification as inventing a pointer merely to keep a
preferred base register live across calls. The latter had already been rejected
for `V90SpectralShaper::reset`; do not apply that rejection indiscriminately
to a loop with demonstrably different memory accesses.

Cross source structure with a narrowly chosen pass control. Here six source
forms with and without loop strength reduction showed that explicit cursors,
not a pass toggle, explained the structure. A second staged-read experiment
did not steer the remaining load/XOR order. Stop there rather than add
`volatile`, barriers or attributes: the helper still differs in four bytes,
despite recovering the reference's 54-byte size and memory-access structure.

The full shared-header scope matters: 18 unchanged controls reproduced their
objects, and only the shaper TU changed under the retained candidate. All 830
exact names were preserved. See [the experiment](../spectral-encoder-experiment.md),
[the read-order control](../spectral-encoder-read-order.md) and
[the retained result](../spectral-encoder-retained-result.md).

## Equal-width field types still carry aliasing information

Do not normalize an original `long` to `int` merely because both occupy four
bytes in the period build. Equal size, signedness and offsets do not establish
the same source type: GCC's alias analysis can distinguish the declarations
and consequently move loads, stores and register lifetimes differently.

The worked case is `dsp_info.clock_deviation`. Smart Link's vendored host
header declares it `long`; the reconstruction had deliberately substituted
`int` to retain a four-word layout on LP64. Under Gentoo GCC 3.4.2-r2 that
substitution delayed a load in `dp_runtime_create` past several integer
stores. Restoring the independently evidenced host type reduced the function's
byte mismatch from 53 to 28 without changing its 298-byte size. All 830
baseline exact functions survived the complete-object census; among 26
header consumers, only `dp_param.c` changed.

The discriminator was a crossed experiment, not a nearest-score search:
source and destination `int`/`long` types were tested with retained flags
and `-fno-strict-aliasing`. All four no-strict-aliasing cells reproduced the
original reconstruction object. Thus the type substitution's effect was
measured as an alias-analysis interaction. The destination-type controls
lacked independent provenance and were not retained.

Use this workflow when widths and call targets agree but load/store placement
does not:

1. Check original host headers, mangled argument types and other independent
   declaration evidence before rewriting statements to match the schedule.
2. Reproduce the unchanged full TU through the experiment path, then cross
   the evidenced type alternative with an alias-analysis control. Treat the
   control flag as a diagnostic, not a proposed global exception.
3. Measure every consumer of a shared declaration, including exact-member
   losses, nonexact bodies, bindings and the correctly ordered partial link.
4. Validate retention with the period differential gate. Track a native
   64-bit ABI-layout consequence separately; do not silently normalize the
   original type for portability inside the reconstruction.

This solves an identifiable reconstruction mistake, not every remaining
scheduling difference: `dp_runtime_create` is still non-exact. A byte-distance
gain alone is not evidence for a declaration, and a function-level gain need
not improve the whole-section positional metric when other layout differences
remain. See [the experiment](../dp-param-alias-experiment.md),
[the shared-header audit](../dp-param-shared-header.md), and issues #15/#22.

## What does not work

- **Renaming a variable.** Free for the compiler (7002). It changes nothing.
- **Reordering ONE function** on the strength of lever 3. That is what control
  4 of 7777 measured and it is right; what 7796 overturned is the conclusion
  drawn from it, so reorder a whole file's leaf block or nothing. Lever 3b says
  which files can pay: a file whose functions never store a constant into a
  field past +0x7f has no scratch to reallocate.

  **RUN THAT AS A FILTER BEFORE PERMUTING ANYTHING -- it is one `objdump` pass
  and it retires whole directories (F8003).** Count `mov $imm,disp(%reg)` with
  `disp > 0x7f` in OUR object, per translation unit. Over the 22 files holding
  one pass's `src/dsp/`, `src/pump/v34/`, `src/v8/`, `src/callprog/` and
  `src/core/` worklist, **21 of 22 came back ZERO** -- `peep2_find_free_register`
  never fires, the cursor never advances, and definition order cannot pay
  anywhere in that span however far out of the blob's order a file sits. It was
  checked against a real enumeration before being believed:
  `GenericToneDetector.cpp` is 4 of 7 in the blob's order with no gaps, and four
  block permutations including the blob's own gave four distinct object
  emissions with **not one symbol's verdict or byte count moving** -- 9a's null,
  predicted in advance by the screen. The screen reads OUR objects, which is the
  right side: the threading is a property of our compilation.
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

### Byte/word aliases require an explicit endian boundary

A recovered union of named bytes and a word can faithfully model i386 partial
stores while still being nonportable. Record whether each use means a physical
object byte or a numeric slice of a word. Do not reverse members speculatively:
external byte-layout consumers may require the original offsets. Until both
kinds of consumers are audited, reject unsupported endianness rather than
silently changing flags. The modem owner headers use `period_byte_layout.h`
for this boundary; this is a restriction, not a completed big-endian port.

The owner migration also demonstrated why typed replacements must retain the
actual pointer path: an equalizer's decoder owner can be external, not the
adjacent embedded decoder. Likewise, a callback slot is not the callback's code
address, and a signed comparison cannot be inferred from an unsigned shared
member declaration. Preserve these distinctions when replacing offset macros.

### Read provenance across stores (V.34 power example)

An input pointer can alias a real output field even when observed lifecycle
callers pass disjoint buffers. Before caching two reads into one local, check
whether an intervening store reaches a compatible input lvalue. In settxlevel,
the blob reloads the MP word after its first power store. Direct reads recover
that data flow without closing the function's remaining codegen gap.

Separate the evidence: observed modem callers, fixed component probes, and
exploratory aliases are different boundaries. Report positive controls and
case/check denominators. Do not promote a synthetic alias to modem reachability,
and do not change source merely to accommodate an impossible fixture. Here
the original load/store ordering supplies independent source evidence.

A closest-size owner variant is still insufficient: crossing short carrier
signedness and assignment placement yields nine distinct objects, none exact,
and the nearest body retains live copy/scheduling differences. Record the
whole-tree set and partial-link losses as well as the local size. See
[the power study](../v34-small-rtl.md) for fixed probes and the closed domain.


### Count calls only after accounting for tail sharing

A relocation count is a static code-layout measurement, not a count of source
calls. In probeselect, expanding six bit-reversal packing helpers gives an
identical full object; -fno-crossjumping raises 14 reversal calls to 19, past
the blob's 16, and costs two exact helpers. F11533 corrects F11526's proposed
open-coded-versus-call interpretation. Preserve that historical observation,
but do not carry its untested source inference into a new reconstruction.

Counter width and signedness must be checked at each use, not selected for
nearest size. The bit reader's wide CRC with an unsigned top-bit shift and a
signed-short top-bit test compiles identically under the period compiler;
a signed-short arithmetic carrier introduces a narrowing absent from the
blob. Likewise, final-rate candidates can recover a frame/comparison shape
while retaining different live loads, copies and branch sharing. A two-byte
size gap is not stronger evidence than those discrepancies. Close the bounded
domain without adopting a candidate when the predicted source mechanism does
not reproduce it. [The study](../v34-small-rtl.md) records complete-TU controls,
exact losses and the stopping conditions; none demonstrates a global ceiling.


### Distinguish source arithmetic during expansion from register allocation

A bare arithmetic half and signed division by two differ on negative odd
inputs. In setInitialPhase the blob uses SAR at both numerator sites, while
our /2 creates sign corrections in initial RTL. Crossing the two expressions
independently removes 2 -> 1/1 -> 0 division expressions before allocation;
this is stronger original-source evidence than a reduced size gap. F11534
restores the shifts, preserves all exact names and passes the fixed period
gate, while explicitly recording partial-link layout/relocation losses.

Do not infer every local type from a halfword comparison. A short loop recovers
an induction step and halfword test, yet changes another helper's body; a short
best-error local still compares an int-promoted error at full width. Record
those crossed controls and stop when they fail the predicted whole live graph.
The remaining comparison widths and reloads are unresolved; the successful
arithmetic idiom does not establish a byte-exact function.


### Find the first duplication pass before declaring an inline-budget cause

V8 rebuildJMSequence retains seven charFlip call instructions through .30.rnreg
and acquires an eighth first in .31.bbro. A -fno-reorder-blocks control restores
seven final calls but increases its size gap from 82 to 361 bytes. This is
block-reordering duplication evidence, not a production flag recovery or proof
that the original source is already correct (F11536).

Count unique call_insn UIDs within the exact function. Late RTL uses mode-tagged
forms such as call_insn:HI, and GCSE debug text can print the same UID twice;
raw symbol-reference counts would invent an earlier doubling. Cross-check the
final count against canonical R_386_PC32 symbol targets, not dictionary keys.
The replay tool's known seven/eight/seven controls must fire before trusting it.

Next examine the duplicated block's predecessors, successors, probability
notes and live operands before/after .bbro. Here the pass explicitly clones
block 32 (call UID 334) to 166 (UID 2643), redirecting edge 17 -> 32.
The 17 -> 32 -> 42 trace starts with the incomplete-extension word reload;
that shared acceptance join is a concrete source-graph boundary to investigate. Use a source/options crossing to
test an independently supported acceptance/extension CFG hypothesis. A finite
failed family does not prove a source-recovery ceiling, and lack of a unique
preimage is not proof that all faithful source hypotheses are exhausted.
Keep SIZE length gaps distinct from counts of differing bytes.

For V34's initial-phase graph, short e plus short best recovers the halfword
comparison that short best alone missed; the coupled graph also recovers
induction and product narrowing. Its two-byte near-match still caches inputs
contrary to the blob (F11535), so it is declined. Do not remove a defined
winner default to imitate an apparently uninitialized blob slot without
proving the original first-improvement invariant.


A missing initialization can be investigated with a compiled-arithmetic
invariant rather than deleting the default to improve size. For initial-phase
winner selection, bits 13..28 of the wrapped square/error make the threshold
predicate periodic over 2^28 ratios. Exhaustive finite arithmetic proves a
first improvement by iteration 3 for every 32-bit ratio (F11537), resolving
F11535's safety question. Default removal still fails to recover the complete
object and changes another helper; it is not adopted. Report the mathematical
boundary, full denominator and known positive/negative controls separately
from protocol fixtures and source-preimage evidence.


A cached input may involve more than one pass. In V34 initial-phase setup,
GCSE PRE creates incoming-edge loads, and post-loop CSE first replaces them
with earlier value copies (F11543). Disabling the second transformation
confirms its cause without undoing the first. Check the earliest changed
RTL and the final load graph separately, including every changed TU body.
Consult the recovered compiler source and relevant patch guards, but keep
executed compiler dumps as authority when only selected source files have
been patched. A named scalar local is a finite source hypothesis only when
conversion expansion predicts a distinguishable RTL representation; names
alone do not compel new loads or register allocation.


A zero SIZE change can hide a useful arithmetic recovery: TimingV34's three
explicit halves remove six report arithmetic bytes while alignment adds six
bytes elsewhere (F11544). Use the operand widths and signedness, local opcode
landmarks and full-TU body inventory as evidence. A signed-word comparison
and sign-extended divisor can establish a carrier correction independently
of register scheduling; retain synthetic high-bit examples as arithmetic
boundaries, not claims of reachable modem states. Gate source adoption on
fixed period differential fixtures and report unchanged exact denominators.


When an earlier pass creates the later pass's input, complete the bounded
interaction rather than trying nearby source spellings. V34's eight-cell
source/GCSE/CSE2 cube restores indexed reloads with GCSE off and shows no
remaining CSE2 effect in that function (F11540), despite changes to 44 other
bodies. The restored graph costs four exact helpers. This rejects a
regalloc-only explanation of that local graph without selecting a compiler
profile; any next profile test must explain those helpers and the remaining
non-exact body together.


## Transfer recovered patterns through a bounded small-function pass

A curated 20-function shortlist produced three exact gains after testing five
families (F11541), without a new compiler profile. Prefer observable source
boundaries over residual size: cache a derived owner before a debug call
when the blob keeps it across that call; share a successful return when an
early return gives the result an extra lifetime; use an unconditional signed
minimum store where the blob stores in both outcomes. FloatIIR needed both
the nested geometry arm and minimum, while FloatFIR did not close with the
same pattern. V32's result initialization position mattered, and retaining
only its exact renegotiation function preserved the hit independently of the
non-exact retrain neighbor.

Measure full TUs and preserve nearby non-exact bodies. An idiom that closed
one family is a hypothesis elsewhere, not permission for a mechanical tree
rewrite. The FIFO experiment produced identical objects for unsigned-short
and int returns, with no caller discriminating the API: leave that signature
unchanged. Show shortlist/screen/compile denominators, known detector
controls and closed-domain dispositions. [The pass](../playbook-small-patterns.md)
records corrected compiler-path replay, all losses (none in retained source),
period validation and the still-failing complete-object comparison.


The reserve pass (F11542) adds two useful controls. For constant fill loops,
recover counter direction and output traversal together: a signed-short
post-decrement countdown plus advancing pointer closes TxNOP/RxClampV22;
changing either alone misses. Preserve exact write count and memory order,
then check sentinel boundaries in existing fixed differential fixtures.
For a float-to-int threshold comparison, naming the converted threshold
before loading the count can recover the x87 conversion/load schedule without
changing precision or rounding. These are bounded source carriers, not proof
of unique original spellings. CID caching and calling-tone early loads did
not transfer successfully; V32's closest array cell still has a reversed
store/extension pair and is left out. [The reserve ledger](../playbook-reserve-patterns.md)
records all seventeen cells and the complete-TU controls.


A narrow output parameter can carry a returned value differently from a
local of the same declared type. RxHdxSequenceE's unsigned-short local
expands into a widened SI pseudo and its count store uses the lowpart;
storing directly through count and passing *count to the next call keeps
the returned HI store before widening. Together with direct array-root
access this closes the complete339-byte function (F11545); either change
alone misses. Inspect initial RTL and cross the independent carriers before
classifying an adjacent store/extension swap as register allocation alone.
Preserve the same truncation, memory order and call boundaries; do not
mechanically eliminate locals elsewhere. [The bounded record](../v32-sequence-count.md)
includes exact neighboring-body controls and the two still-deferred V22
reserves whose previous spelling domains are already closed.


Transfer a recovered loop pattern by a declared product, then distinguish
source-graph recovery from exactness. DetSequence's countdown and separate
short shift reproduce their local graph but increase size; a newly observed
once-per-call found initializer fixes a third lifetime without closing the
spill choice (F11546). Local allocation still has the bound pseudo; global
allocation/reload spills it and retains the shift-register pseudo, opposite
the blob. That is a pass boundary for the next explanation, not permission
for register/declaration spelling searches. [The six-cell ledger](../v32-detsequence-loops.md)
reports all negatives, positive graph/spill controls and unchanged retained
objects. Include new tool/doc files in the tracked-file census before checks,
so an untracked ledger cannot silently escape cross-reference validation.


A global-allocation dump may include reload's final mapping: distinguish the
boundaries before attributing a spill to priority. F11547 traces installed
Gentoo cc1 and raw-validates both full-TU outputs; one reg is assigned ECX by
global allocation, then evicted by reload's CL requirement. A short
post-decrement temporary instead receives ECX locally and changes the earlier
allocation decision. F11548 separates decrement from extraction, removes that
local temporary and recovers the blob's spill arrangement without byte identity.
[Trace and bounded source control](../v32-detsequence-allocation.md). Machine
scheduling can put the decrement ahead of SHR even when source decrements
after extraction; don't infer expression sequencing from that order alone.


Trace spill-home allocation before inferring source declaration order. F11549
observes ascending pseudo allocation and late reload eviction; a minimal
word-scope change recovers all blob slots but remains non-exact. Optimized
compiler debug parameter locations can misreport arguments: trace the true
ABI entry and validate full-TU raw output. F11550's crossed peephole control
certifies scratch exposure but loses exact neighbors. Returning to the
non-exact predecessor reveals an independent GenSequence source property:
short post-decrement narrows before the wrapping AND, and combines with a
countdown to recover 118 bytes exactly (F11551–F11552). [Measured record](../v32-gensequence-recovery.md).

## Locate redundant-load elimination before blaming allocation

TxHdxTRN's cached subtraction initially resembles a register-lifetime issue.
Its first CSE dump still reloads the member; GCSE PRE explicitly replaces
that load with the comparison's reaching register. Cross the independently
observed unsigned input fold with diagnostic pass controls: disabling load
motion changes neither raw object, while disabling all GCSE restores memory
RMW but loses two exact neighbors. The unsigned fold itself recovers one
instruction without exact identity. Treat these as separate measured effects,
and close both bounded domains without inventing volatile declarations or
adopting local score improvements. [F11553 and replay](../v32-txhdxtrn-pass-boundary.md).

FPM_rms supplies another independently measured countdown/cursor case:
neither property alone recovers the function, and both recover all62 bytes
including its call relocation (F11554). Preserve arithmetic order and overflow
semantics; loop recovery is not permission to replace scaled products with
an algebraically similar expression. [Four-cell record](../fpm-rms-countdown.md).

An allocation failure's literal-zero early return can obscure the blob's
common pointer return. Dual_TONE_create recovers64-byte exactness by guarding
initialization and returning the allocated pointer for both outcomes; the
two-cell full-TU domain changes only that function (F11555).
[Measured record](../dualtone-create-common-return.md).

Trace x87 stack operands before calling a difference scheduling. Notch's
saved-state addition occurs before its product-sum in the blob and after it
in retained source. Recovering the addition tree still leaves load/exchange
differences and no exact gain (F11556). Algebraic equivalence does not imply
finite-precision grouping, and load order alone does not justify invented
coefficient locals. [Closed source domain](../notch-addition-tree.md).

## A constant loop bound can reveal when a helper was inlined

PCM's indexed segment search differs from the blob's pointer walk. Recovering
the cursor leaves a strict-comparison difference: fixed source emits <=7,
the blob emits <8. A helper size argument initialized to8 at the call preserves
the strict comparison under period inlining; both size-only and table/size
forms yield the same complete object. This constrains late binding without
uniquely recovering a helper signature (F11557). A separate input/magnitude
carrier domain closes linear2alaw too (F11558). Inspect initial RTL before
blaming scheduling for a comparison that was already normalized there.
[Seven-cell staged record](../pcm-segment-search-recovery.md).

## A machine countdown can be an optimizer's reversed ascending loop

Float2Linear's four-cell source domain recovers101-byte exactness with
advancing input/output pointers and the existing ascending loop. An explicit
countdown misses, alone and with cursors. Gentoo's loop dump says it reversed
the cursor loop; source index i is dead except as the bound. Check that pass
before inferring a countdown from machine decrement/test (F11559).
[Complete-TU controls](../float2linear-cursors.md).

An apparently register/layout-bound constructor can still omit a real failure
edge. toneiir_create's blob checks the allocation result; reconstruction did
not. Restore observed behavior even when its finite source domain gives no
byte-exact gain. A same-length candidate with114 differing bytes is not a
recovery. Fixed allocator-failure and one-shot recovery checks distinguish this
from a score-only rewrite (F11560). [Record](../toneiir-allocation-recovery.md).

Recovering an x87 opcode is not recovering its lifetime or evaluation boundary.
Floating RMS literal/local controls recover fld1 but either add final narrowing
or move the reciprocal before the loop; both miss the reference's two separated
count conversions (F11561). Inspect the full stack tree and conversion sites,
not just the attractive opcode. Likewise the constructor common-pointer-return
family succeeds for Dual_TONE_create and fails for silence_create (F11562).
[Closed domains](../playbook-rms-silence-controls.md).

GenerateAnsTone supplies another ascending-loop reversal control: an output
cursor recovers its clear loop; a source countdown does not. Its four remaining
comparison bytes disappear when elapsed updates use the field rather than a
common temporary. Early source field writes are coalesced into the reference's
conditional final stores by Gentoo, so infer source factoring from complete
controlled bodies rather than store placement alone (F11563).
[Six-cell record](../v32-anstone-recovery.md).

Operand-width recovery can leave a distinct condition-lowering mismatch.
FPM_TONE_filter's short carrier cross restores cmpw/incw but leaves a branch
where the blob has setl/neg/and, with no exact gain. Do not adopt carrier
narrowing solely because individual instructions agree, or infer a unique
local type from a word comparison (F11564).
[Closed width domain](../fpm-tone-width-controls.md).

A shared narrow temporary can extend a value before a store that only needs its
low half. V22 IIR recovers history-store-before-extension by narrowing at uses,
then recovers its full body only when accumulator initialization/reset match
the preheader/latch boundary. Cross both source questions: either alone misses
(F11565). Keep mixer narrowing explicit; rereading the stored history introduces
an alias-sensitive operation the reference does not perform.
[Six-cell record](../v22-iir-conversion-recovery.md).

SMCv32_encoder_abs's countdown/cursor cross restores sequential input and the
word sentinel but leaves distinct ring-return/tag conversion boundaries. A
correct recovered loop is not authority to widen ring locals or force masks;
inspect the helper signature and field reads across callers first, including
closed F8249 controls (F11566).
[Traversal domain](../v32-smc-abs-traversal-controls.md).

For a rejected conditional-zero mask, read the converter's complete predicate.
GCC3.4's noce_try_store_flag_mask needs zero versus the destination itself:
a ternary assigning a separate result temporary can fail even when both arms
are already SImode. Ordinary in-place conditional clearing recovers V32's mask
in all three encoder callers (F11568). Shifted tags, wrapped-result carriers
and comparison-use narrowing are separate axes; together they recover an exact
68-byte abs loop but leave51 bytes different in the whole function (F11567,
F11569). Do not turn an exact internal region into an adoption or declare the
remaining prologue a pure regalloc issue without examining its load ordering.
[Closed24-cell record](../v32-smc-if-conversion.md).

The destination-identity diagnostic transfers to FPM_TONE_filter: six ordinary
conditional updates convert and six ternaries do not, across a bounded carrier
cross. No function becomes exact (F11570). A verified shared compiler mechanism
can explain an instruction family while leaving scheduling and lifetime
questions open; do not equate successful transfer with recovered original
source. [Twelve-cell transfer](../fpm-tone-if-conversion.md).

A close non-exact slicer can still read its decision input after an output that
the blob reads first. FSE_decision_CD's fixed permitted count/magnitude alias
fails3/268 baseline checks; recover the read boundary before interpreting its
remaining scheduler difference (F11571). BYTES2 masks the union of relocation
fields: here four raw bytes and the successor relocation move when two stores
swap. Inspect relocation positions and RTL stages, not the byte score alone.
[Source/option cross and fixed fixture](../fse-cd-load-recovery.md).

Do not infer that reversing two independent source writes will undo a measured
scheduler reversal. The CD successor/lms two-cell control emits the same
complete object in both orders (F11572); the compiler can erase lexical-order
information before final scheduling. Close that bounded family rather than
expanding into arbitrary store permutations.


An early zero-extension can reflect a conversion boundary without being the
whole source mismatch. Caller ID pack_next_bit's seven carrier/position controls
recover selected operations but leave the complete function non-exact; review
its inlined caller too (F11573). [Closed domain](../cid-pack-conversion-controls.md).

Do not clear automatic output fields merely because the reference leaves them
uninitialized. Prove which fields the callee defines and which anyone reads.
The cosine generator's artificial clears and cached scale interact with the
loop carrier: only their removal plus short post-decrement recovers the complete
125-byte function in an eight-cell cross (F11574). The same-length cached-scale
control still differs in88 bytes. Register allocation can follow recovered
lifetimes without register-specific source. [Controls](../fpm-tone-demod-recovery.md).


A ternary counter update can merge stores/returns and change a byte input's
lifetime even when all arithmetic is already correct. cEncodeChar's complete
six-cell cross gives three exact ordinary-branch forms and zero ternary hits;
byte-helper narrowing is unnecessary in that family (F11575). Select an
independently supported form and review all callers: the direct byte form
preserves edprintf, while shared helper changes affect it. Multiple hits bound
a common source property, not a unique original spelling.
[Carrier/control cross](../encode-carrier-recovery.md).


Check siblings for a proved output-lifetime/countdown pattern, but rerun the
cross: generate2's four cells yield only the combined148-byte exact hit
(F11576), preserving its two separate scale reads and demod's existing gain.
The callee must define both outputs before use; a matching sibling is evidence
for transfer, not permission to apply it blindly to phase-reversal generators.
[Quadrature transfer](../fpm-tone-pair-recovery.md).


A register-heavy oscillator residual can still conceal a real signed-word
boundary. Fixed negative traversal and naturally reached counter32760 expose
FPM_TONE_generate discrepancies that ordinary answer-tone tests miss (F11577).
Use adequate buffers and valid history before changing source. Narrowing locals
can recover cmpw while eager Boolean lowering appears already in initial RTL;
that is not allocator evidence. Direct field predicates recover branches, then
owner-counter update plus predicate order and private-phase capture boundary
recover the full213 bytes. Early source field writes can be coalesced into
conditional final stores. [Bounded recovery](../fpm-tone-sine-recovery.md).


Primitive array-new can preserve an element-count boundary that manual byte
allocation folds into a different instruction. Psd's five-cell scaffold/control
cross recovers both82-byte constructors when the second float array uses new[];
first-only and unused allocator scaffold raw-reproduce baseline (F11578).
Two exact cells emit the same complete object: choose the consistent two-array
form without claiming unique first-allocation spelling. Verify cookies, emitted
allocator symbols, exact sizes, uninitialized buffer content and every TU body.
C++ replay must include configured CXX flags, not only the C profile.
[Allocation transfer](../psd-array-new-recovery.md).


Owned-object publication can distinguish otherwise similar constructor code.
V90SpectralVerifier publishes Psd at+4 after nested construction in the blob;
manual allocation assigned to the member before construction obscured that
lifetime. Raw-local/late assignment and direct placement-expression assignment
emit the same162-byte exact clones (F11579). Compare the call/store boundary
and preserve parameter-member reloads; do not infer allocator changes from
register colours alone. [Full-TU controls](../v90sv-publication-recovery.md).


Publication transfer has limits: Resampler history-local and conditional-owner
forms recover the blob allocation/pointer-store boundary and174-byte length
but still differ in44 bytes (F11580); complete TU exact set unchanged. Do not
adopt a size-only hit or expand arbitrary initialization-store permutations.
[Closed family](../resampler-history-publication-controls.md).


A sibling count-local transfer can recover only an arithmetic prefix.
Descrambler<int,int> becomes107B/BYTES19 across27 header consumers with
no exact gains; Scrambler's prior success is not a full-preimage proof
(F11581). Require every defining copy and full body, preserve negative
controls and close the tested arithmetic family.
[Shared-header transfer](../descrambler-count-controls.md).


Opposite-order duplicate pretests before a member cursor is initialized can
expose an outer source guard around a for loop. FloatARMA's independent
denominator/numerator cross has only one full hit: both padding guards
recover both612-byte clones (F11582). Denominator-only gets the same length
but differs171 bytes; numerator-only stays596B. Cross guards, retain full
bodies and inspect final cursor state, rather than treating size as fidelity.
[Guard recovery](../floatarma-padding-recovery.md).


An inline narrowing cast can still be folded into a table relocation, while
assignment into a short index preserves the same compiler's conversion
boundary. TONE_read needs short masked phase, bounded quadrants and stored
short reflections; the width-only cross leaves11 differing bytes, both
storage forms raw-agree on the full121-byte object (F11584). Use fixed
exhaustive input coverage and full-TU/relocation controls, not explicit
registers or assumed original spelling.
[Width/storage cross](../tone-read-width-recovery.md).

Unconditional min assignment is not universally an exactness unlock.
FloatFIR setCoefficients' four min-store/nested-update cells recover selected
boundaries but none the complete body (F11583); close the family rather than
expand arbitrary variable/store permutations.
[Closed control cross](../floatfir-coefficient-control.md).


A word load does not establish unsigned source type. RxClampV32's member
is already signed short; countdown/conversion controls close without a gain
(F11585). [Closed domain](../rxclamp-count-controls.md).

Matching loop/return shape and even total size does not establish a divider
preimage. GetFP_Value's bounded arithmetic/lifetime families leave byte
mismatches; preserve the static zero-divisor fidelity observation separately
from unrun runtime/reachability claims (F11586).
[Controls](../getfp-divider-controls.md).

Aggregate copies and short element counts are independent boundaries:
FPM_MTD_create needs both, plus short loop counter and removal of an
unsupported allocation-success guard. Only the combined16-cell cross recovers
184B exactly (F11587). Verify exceptional outcomes with fixed child processes
and successful controls; an added NULL return can hide a blob fault and alter
fidelity. [Recovery](../fpm-mtd-create-recovery.md).


After aggregate configuration copy, subsequent arithmetic may read the owner
fields rather than the incoming pointer. FPM_MRF_init also uses the original
fresh argument as an allocation carrier and a short clear counter (F11588).
An8-cell independent cross needs all three for full193B exactness; counter+
carrier alone reaches size but differs30 bytes. Keep growth ownership/debug
ordering and verify growth through valid successive initialization.
[Recovery](../fpm-mrf-init-recovery.md).


A short local inside a shared inlined helper is not a universal conversion
carrier. Phasor interpolation fraction-width transfer alone adds one byte
per oscillator without recovering their bodies (F11589). Preserve the
complete TU/table controls and close that local-width family; existing
exhaustive fixtures are not evidence for an unrun candidate.
[Controls](../phasor-fraction-controls.md).


An unsigned-word operand can lose its additional bits when its arithmetic
result is assigned short. Phasor's original-word × fraction-width cross
raw-merges unsigned+short with short-only, with no exact gains (F11590).
A separate observed register/load role is a hypothesis to test, not a source
preimage once the bounded cross refutes it.
[Controls](../phasor-consumption-controls.md).


Use-site signedness can differ between allocation and clearing: FPM_FSD_init
allocates an unsigned-word trace length but clears with signed comparisons.
Shared short counter plus that allocation conversion closes its full268-byte
body; short counter alone misses one byte (F11591). Test requested allocation
sizes and untouched allocated contents at negative component boundaries.
Do not retype the field or narrow a promoted loop bound merely to avoid the
blob's own counter wrap. [Recovery](../fpm-fsd-init-recovery.md).


Word countdowns/cached members/narrowed total can recover FSM loop structure
without its complete229-byte body (F11592). Keep the tested four-axis family
closed; upper return-register bits do not uniquely type the public API.
[Controls](../fsm-modulate-controls.md).

A duplicated positive-count pretest motivates a guard experiment, not an
exact source claim. FSE_getdiag loop-only guard reaches217B vs229B without
exactness (F11593). Inspect the branch destination instruction before inferring
an exceptional return: it moves the selected count, not zero. Preserve invalid
preimage attempts and existing negative-count fixture evidence separately.
[Controls](../fse-getdiag-guard-controls.md).


Generic helper coverage does not validate a parent's child configuration.
BwChDem_Create's ratio28996 differs from object's29000 even while existing
waveform tests pass (F11594); fixed owned-child comparison catches all five
constructors. Recover literal fidelity independently from exactness. Deferred
const-table definition raw-reproduces baseline, so initializer visibility does
not unlock this first-entry load. Keep const/data controls; no source-order
or mutable/volatile score fitting. [Controls](../bwch-constructor-controls.md).


A later readonly initializer may still be visible when unit-at-a-time parsing
precedes expansion. Cross initializer placement with that pass before attributing
constant folding to source order (F11595). Initial RTL can confirm the mechanism
without recovering the body: Bw Create regains its table load and size but still
has a fixed store-displacement mismatch, while Progress and data layout change.
Compare named data owners and relocations separately from section padding/order;
never adopt the option merely because one instruction reappears.
[Controls](../bwch-unit-visibility-controls.md).


Early loads can identify original local lifetimes even when field widths are
already right. ECC init caches near-delay and line fill before allocation and
clears; both locals recover the607-byte body (F11596). Short and promoted-int
spellings raw-merge, so claim the retained lifetime rather than a unique width.
Use fixed alias controls to expose rereads, labelled component-only when no
modem history is proved. A bystander free's dead POP may change too; record
that gain without inventing a source change in free.
[Recovery](../ecc-init-cache-recovery.md).


Do not treat early index initialization plus separated postincrement as an
exact source recovery merely because size approaches the object. V22 MRF's
four-cell cross closes without a hit (F11597): combined227Bvs228 still changes
saved-register count and sequencing. Distinguish RHS sampling before a
postincrement assignment from incrementing after its store; these have
different local lifetimes. [Controls](../v22-mrf-index-controls.md).


Sampling the RHS before work[k++] is a distinct discriminator, but V22 MRF's
four-cell follow-up also fails (F11598), growing244→245B. After two negative
batches, park nearby index/temporary synonyms and return to independent
operand or field evidence. A bounded failed family does not establish that
source recovery is globally exhausted.
[Scope review](../v22-mrf-index-controls.md).


Opposite temporary-array rail placement and early work-index lifetime motivate
an independent stack/source cross, but do not force an exact preimage.
V22 PPS's four cells restore selected boundaries yet fail the complete290B
body (F11599). Close the family, preserve arrays/data/symbol controls and
select another operand/use lead. [Controls](../v22-pps-init-controls.md).


Distinguish a sentinel countdown from a postdecrement test using initial RTL
and final flags. MTD sentinel retains CMPWffff; postdecrement reproduces the
blob's DEC/narrow/INCWoldflags but still misses the complete297B function
(F11600/F11601). Short energy locals do not by themselves reproduce its clamp.
Terminal ternary lowering is another early-stage discriminator; sharedverdict
also fails (F11602). Close these staged families, preserve data/body controls,
and do not promote a one-byte size difference into a byte preimage.
[Controls](../mtd-detect-controls.md).


Review an inlined helper's complete TU, including bystanders. Shared IIR's
counter/cursor/feed-forward narrowing eight-cell cross has no full hit
(F11603), and count changes untouched II despite unchanged size. Do not add
an uninitialized output merely to match the blob's undefined zero-section
return; preserve the fixture's exclusion and demand an independent discriminator
before more local permutations. [Controls](../iir-boundary-controls.md).


A use-site word conversion does not prove a word-width running carrier.
Block-update's promoted-position control reflects the blob's full-width
subtraction and later narrowing but fails its complete244B body (F11604).
Keep arithmetic bounds separate from service reachability, close the bounded
family and do not fit accumulator widths or stack allocation by size.
[Controls](../block-position-controls.md).


Local stack-word addresses and store/reload pairs support testing an ordinary
inlined helper, without forcing address escapes. Div32 helper×word-index
cross reproduces these boundaries, and guarded do/while restores conditional
count-store placement, but neither closes the full body (F11605/F11606).
Review error epilogues and narrowing widths, not just size. Audit fixture
cross-product claims: two marginal sweeps need not cover every pair. Keep
concrete missing fixed vectors and close declaration/frame permutations.
[Controls](../div32-normalization-controls.md).


Signed-word increment and comparison evidence can recover a loop even when
its complete initializer still misses. V34 detector short counters reproduce
its prologue and clearing loops, but two observed post-loop store-order axes
fail the full body (F11607/F11608). Close the finite family, keep the partial
recovery and select independent evidence rather than permute fields/registers.
[Controls](../v34-detector-initialization-controls.md).


An unread callee slot does not establish an absent source argument. Audit all
caller stack writes before dropping a nominally dead scalar: restoring MRF/FSD
free arguments recovers four complete callers (F11609). Cross each API change,
use isolated consistent header overlays, then rebuild every caller/callee and
adapt test consumers. Recover observed literals, not invented ownership meaning
or a unique formal width. Check unchanged helper bodies and non-exact bystanders;
API fidelity is source evidence even when only some callers become exact.
[Recovery](../free-argument-recovery.md).


Extend an ignored-argument recovery only after auditing every caller. The
FSE/SRE×ECC×PPS cross restores13 explicit literal1 slots and yields five exact
callers (F11610); two V27 bodies still miss initial setup, so preserve their
unmatched status. Ignored formal type/name/meaning remain unproved. Full
partial links may lose positioned relocation matches despite complete function
gains; report both, with all-TU data/export and differential controls.
[Recovery](../free-argument-recovery-rest.md).


Repeated owner loads after calls can distinguish a cached child local from
original direct owner expressions. B103 needs both dsp andhdx lifetimes to
recover its full deletion body (F11611); either alone misses. Retain guards,
call order and real lifecycle allocator checks. Do not infer a prior behavior
bug from a register/source lifetime recovery, or transfer it to already-matching
loads without new evidence. [Controls](../free-argument-recovery-rest.md).


Call argument evaluation and a preceding pointer assignment need not schedule
identically. When a deletion suffix already matches, compare direct owner-member
arguments against the explicit child-local statement at the first calls. V27
RX/TX recover complete bodies this way (F11612); preserve later matching owner
reloads. Review register-renamed bystanders and avoid claiming unique spelling
from setup order. [Recovery](../v27-delete-argument-recovery.md).


Keep ignored arguments and return recovery as independent axes. Audit every
caller slot, including original input counts that survive a resampler overwrite.
AGC's nineteen explicit fourth slots recover RxHdxNoSignal (F11613) with the
callee raw-identical while preserving void and field reloads. An object's EAX
consumption is evidence for a return hypothesis; a reconstruction header does
not prove the original lacked a result. Review genuine control-factoring
bystanders, not just edited callers. [Recovery](../agc-fourth-argument-recovery.md).


Stage return semantics separately from call-argument restoration. Six observed
AGC EAX consumers motivate an int return, with two signed-short conversions;
minimal and complete twelve-TU controls recover no exact bodies (F11614).
Ignored results can still affect a caller body, so audit all API consumers.
Close the finite family without asserting that zero gains disprove the original
return type. Reject an empty domain and preflight all generators before the
first compile; preserve invalid attempts outside valid result artifacts.
[Controls](../agc-return-controls.md).


Cached-pointer controls do not exhaust an ordinary delete expression. A
typed owned class pointer plus one blob pointer retained across destructor/
free supports testing scalar delete separately from its TU-local adapter.
V90Phase4Modulator's adapter-only cell raw-merges; member delete recovers both
94B clones and leaves all51 bystanders unchanged (F11617). Preserve ownership,
null guard and automatic member destruction. A recovered first-emitted body
does not establish that later register/scheduler differences must disappear.
[Recovery](../v90p4-owned-delete-recovery.md).


Cross each independently observed owned-member lifetime. V90Modulator's
three typed destructor/free pairs all retain one blob pointer across calls.
Each partial member-delete combination matches199B but still fails the
complete body; all three recover both clones with all23 bystanders unchanged
(F11618). Use complete bytes/relocations to distinguish the crossed family
from an accidental size match. Preserve primitive frees and generated member
destructors, and isolate the adapter-only control.
[Recovery](../v90-owned-delete-recovery.md).


A nonvirtual owned class deletion can emit both destructor and host free.
Do not infer explicit destructor syntax from two calls alone. V92EchoCanceller
retains the same pointer across both calls with ordinary guarded member delete,
recovering both195B clones while all13 other bodies remain unchanged (F11619).
Keep conditional pointer clears inside their observed guards; test the adapter
independently. Large layout growth can still reduce positioned partial-link
matches despite complete function recovery. [Controls](../v92ec-owned-delete-recovery.md).


A five-owner cross can reveal a bystander register effect separately from
source recovery. All31 nonzero V92Modulator crosses recover enterPhase3 by
ECX/EDX renaming, but only all-five recovers both complete503B destructors
(F11620). Every partial also503B yet fails bytes. Audit the bystander, preserve
virtual and primitive releases, and label temporarily masked-owner fixture
subsets synthetic rather than pre-release histories.
[Recovery](../v92-owned-delete-recovery.md).


Typed-owner recovery does not authorize rewriting adjacent primitive releases.
V90/V92 modem crosses recover four destructor clones plus two unchanged
register-renamed bystanders (F11621); phase2Info remains bare free, V92 mapping
helpers remain unguarded and automatic embedded destruction stays generated.
Audit the full ownership boundary and keep genuine child lifecycles separate
from temporarily masked or pre-released guard fixtures.
[Controls](../modem-owned-delete-recovery.md).


A bounded owner screen can recover source lifetime without recovering bytes.
V90Demodulator all-eight removes128B and leaves only a dead-pop mismatch,
but loses constructor identity and changes a progress jump-table layout
(F11622). Preserve that informative loss without adopting it as a gain or
calling19 cells exhaustive. [Controls](../v90dem-owned-delete-controls.md).


Revisit a C-language exclusion only with independent TU provenance. F11430
places VPCMXF_Delete and its class destructor in VpcmFloModem.cpp; ordinary
class delete then recovers109B from117B by suppressing a sibling free jump,
with all33 bystanders unchanged (F11623). Preserve the same-TU destructor
inlining boundary. [Recovery](../vpcmx-delete-recovery.md).


Store duplication in the object can motivate a control-placement experiment
without proving original duplicated source. CID reset branch-local clearing
recovers118B shape but remains BYTES8 (F11624). Both exits'EAX0 separately
motivate an ordinary zero-result return hypothesis; its crossed int cells
still miss (F11625). Close the finite family, preserve baseline controls and
ABI uncertainty, and do not expand into register or return-type spellings.
[Controls](../cid-reset-controls.md).


An equal-size output-helper candidate is still a negative result when its
complete body differs. FPM_div's helper/word-index cross reaches150B but
BYTES105; independent initialization ownership after the zero guard gives
BYTES108 (F11626). Count all emitted functions separately from blob-common
symbols, reproduce repeated controls, and close the declared family rather
than perturbing counter scope, loops or frames. [Controls](../div16-normalization-controls.md).


Separate postreload scheduling from x87 stack conversion before classifying
exchange differences as allocation. Notch's sched2 control changes54B to48B,
but neither addition tree becomes exact (F11627). Scheduled load hoisting
precedes inserted exchanges; a disabled-pass near miss supports neither a
production option change nor invented coefficient locals.
[Stage controls](../notch-addition-tree.md).


C++ TU provenance enables delete hypotheses but does not establish pointee
lifetime. K56's actual factory pointer reaches class API calls and+0xc record
accesses, yet all relevant class methods are empty (F11628). Keep receiver
use, record extent, sizeof and ownership separate; do not select scalar or
array delete solely to suppress a sibling free jump.
[Audit](../k56-owner-boundary-audit.md).


Audit added defensive guards against the actual first memory access. Removing
dp_wrapper_delete's unsupported null early return recovers85B EXACT (F11629),
while both other TU functions stay unchanged. Preserve independently observed
child guards and validate actual constructed lifecycles; do not invent invalid
null fixtures or widen the blob contract. Aligned whole-object allocation can
stay constant despite a shorter exact function.
[Recovery](../dpw-delete-guard-recovery.md).


An unsupported defensive guard is a source-contract lead, not a guarantee
of a complete byte recovery. FixedRC reset needs external memory-call
boundaries too; their cross restores270B shape but remains BYTES41 (F11630).
Explicitly review replacement imports and shared helper callers. Tone creation
loses its added allocation guard yet remains SIZE90 (F11631). Close these
finite domains and review distinct allocation/dispatch boundaries instead of
perturbing registers or accepting only a nearer size.
[Reset controls](../fixedrc-reset-controls.md),
[tone controls](../tone-allocation-guard-controls.md).


Preserve guard extent, not only the existence of a null check. FixedRC's state
check guards the state free itself, and child free operands reload the owner
between unknown external calls (F11634). Both properties recover113B Delete;
either partial cell does. Pair host allocation and deallocation, restore only
observed partial clears, and validate asymmetric created-owner lifecycles.
Full factory ownership can be source-supported without closing factory bytes;
keep those residuals and intentional import/jump-table changes visible. A new
ledger must first fire on the known baseline mismatch:162/652 failures became
652/652 passes here, with six real V22 bridge witnesses restored.
[Recovery and bounded domains](../fixedrc-factory-recovery.md).

Recover a loop's widths and traversal independently before testing helper
factoring. V34 receive queue's eight counter/output/readwidth controls miss;
direct conditional wrap then eliminates the helper's cursor temporary and
recovers70B EXACT (F11635). Audit the inlined caller too: V34agc changes only
in its queue prefix, with complete later body unchanged. An exact function
gain can still reduce whole-link positioned matches; report both.
[Recovery](../v34-rxqueue-recovery.md).

A captured child load is a valid source lifetime hypothesis, not a promised
byte match. CID string capture restores the pre-loop owner access but stays
SIZE1 (F11636). Keep capture after the external clear, audit actual allocation
and output/owner boundaries, and close the finite family rather than expanding
register or pointer synonyms. [Control](../cid-string-capture-controls.md).

Separate sample-read order from cursor-helper factoring. V34 transmit queue
needs both to recover68B EXACT (F11638). A fixed overlap witness detects the
baseline's lost highhalf sample, while ordinary separate-buffer tests cannot.
Label that allocated component probe synthetic rather than asserting modem
reachability. Preserve other helper users and report alignment-absorbed size
changes. [Recovery](../v34-txqueue-recovery.md).

Return-width and predicate hypotheses need complete bodies, not selected
instructions or equal size. FIFO8 read's wider results remove sign extension
but remainSIZE14 (F11637); bitreverse's statement predicate reaches64B but
emitsTEST/JE/MOV instead ofSETNE (F11639). Keep both negative controls,
preserve ABI uncertainty, and close each family without adjacent synonyms.
[Return controls](../fifo-read-return-controls.md),
[predicate controls](../v34-bitreverse-init-controls.md).

Inspect both comparison operands in initial RTL before inferring source
width from the final CMP. Decision's short best_dist alone still compares
SI values; both short distance locals produce HI comparison and CMPW
(F11640–F11642). A normalized unused return register can motivate a bounded
API control without uniquely recovering its declaration. Even after cursor,
comparison, guarded reads and result normalization reproduce, differing
spills and entry layout do not authorize register/declaration variants.
Preserve repeated raw controls and close the finite family.
[Controls](../v34-decision-controls.md).

A recovered countdown backedge does not prove the entry boundary is recovered.
FloatFIR's postdecrement control keeps the authentic zero guard but introduces
a second entry test, growing287B to303B with no exact gain (F11643). Preserve
the complete control and its denominator; do not remove observed guards or
expand counter synonyms to fit the partial result.
[Controls](../floatfir-countdown-controls.md).

Repeated word counter narrowing is source evidence but not a guarantee of
complete recovery. V34TimingHPFilter's int→short control restores increment
narrowing and word comparison, reaching76B yet remainingBYTES29 (F11644).
Even alpha comparison fails prologue ordering. Keep all26 function/48 data
controls, distinguish recovered operations from complete identity, and close
the width family without arithmetic or register permutations.
[Controls](../v34-hp-counter-controls.md).

Separate loop traversal from recovered arithmetic. V34 echo cursor/count
controls reach reference sizes yet fail complete/alpha bodies, with meaningful
bystander changes (F11645/F11646). Preserve original zero-count boundaries,
report all 26 function/48 data controls, and close the finite families.
[Controls](../v34-echo-cursor-controls.md).

A conventional advancing structure pointer can recover an indexed loop's
observed cursor without changing widths or arithmetic: V8 DFT energy becomes
74B EXACT (F11647). Validate all siblings/data and production raw promotion,
then report whole-link layout effects separately. Register local headers for
every experimental source directory; setup failures are invalid, not compiler
rejections (F11648). Preserve the failed setup and prove the corrected baseline
raw-reproduces before counting any candidate.
[Recovery and apparatus control](../v8-dftenergy-recovery.md).

Counter width can determine whether GCC reverses a loop. V8 queue int counters
reverse to countdowns; short counters retain the blob's forward narrowing
and recover two complete94B helpers (F11649). Cross independent helpers to
separate each gain, and audit inline consumers too: V8agc's changed prefix
includes scheduling/alignment, while its complete later suffix and relocations
stay identical. Do not infer global original flags from two source recoveries.
[Recovery](../v8-queue-width-recovery.md).

An advancing sample cursor can reproduce addressing and outer-counter spills
without recovering the complete loop. DFT update's cursor grows150→172B
against164B and fails instruction count too (F11650). Keep sample loads inside
the bin loop to preserve alias lifetime, review the whole TU, and close that
traversal domain without promoting a partial match.
[Control](../v8-dftupdate-cursor-controls.md).

Preserve ascending overlap behavior when recovering pointer-copy source.
V8 coefficient copy keeps its signed-short loop and changes only indexed
assignment to *dst++=*src++, recovering45B EXACT with all twelve siblings
unchanged (F11651). Report alignment-absorbed growth and positioned-layout
loss independently. [Recovery](../v8-copycoeff-cursor-recovery.md).

An independent word-counter/coefficient-cursor cross can recover both predicted
operations without closing a FIR body (F11652). Retain valid indexed history;
do not add an unused final before-array pointer decrement to fit disassembly.
Separate the recovered operations from merged-history and accumulator residuals,
and close finite source domains without forcing spills or registers.
[Controls](../v8-fsktx-source-controls.md).

A typed embedded-subobject owner can recover compact offsets and lifetime
across a call without changing store order. ANSam's six tone-pointer accesses
plus owner-relative envelope_phase recover85B EXACT (F11653). Review sibling
bodies and anonymous tables too: raw jump-table addends move with function
alignment, but each function-interior target must still agree. Report that
controlled raw-data change explicitly, not as unchanged nontext.
[Recovery](../v8-ansam-owner-recovery.md).

Dead-index removal can enable automatic reversal without closing call-boundary
normalization. Tonequeue's output pointer recovers ADD2/countdown but stays
SIZE4, with meaningful inlinehandshake/relocation-layout changes (F11654).
An exact byte-consuming callee does not require upper argument-slot bits to
be normalized; do not infer a unique wider prototype from their absence.
Close traversal independently of any future ABI investigation.
[Controls](../v8-tonequeue-cursor-controls.md).

Read-only reduction cursors need their own control even when neighboring FIR
cursor domains failed. Echo history-energy restores load/ADD2/countdown but
merges entry with backedge, missing the blob's distinct skip/count copy
(F11655). Use initial loop diagnostics to distinguish compiler strength
reduction from source traversal, then preserve complete negative evidence
and input boundaries. [Control](../v34-echo-energy-controls.md).

Treat observed byte-sample lifetimes as independent boundaries, including a
load between two output stores. V22 control's three capture axes still fail
full bodies despite the closest148B versus145B (F11656). A whole control-byte
value domain on disjoint objects does not cover alias timing; require valid
object/representation evidence before claiming a behavioral difference or
original API contract. Do not choose captures or retype flags by nearest size.
[Controls](../v22-control-capture-controls.md).


Cross count-capture timing with old-value postdecrement when dispatch and
loop flags independently support them. MakeTxData’s combined spelling
recovers the complete body and five function-relative table targets; neither
axis alone does (F11657). Anonymous section offsets remain UNRESOLVED in the
strict comparator. Preserve that distinction until a general resolver proves
table extent and targets with negative controls; never replace target identity
with masked addends or a named exception. [Evidence](../v22-txdata-recovery.md).


Anonymous jump-table identity needs guard-derived extent and ordered relocated
instruction destinations, not table-byte masking (F11658). Prove a separate
bounded detector on real ELF positives and explicit refusals before modifying
canonical grading. Review control transfers beyond Jcc: LOOP, far/prefixed
jumps and entries into guard instruction interiors can bypass an incomplete
parser. Preserve unsupported cases as unproved, and state the ABI-entry
boundary. [Controls](../anonymous-jumptable-proof.md).


Integrate proved table identity through the actual shared body/verdict path
and retest every refusal there (F11659). Ordinary anonymous data must stay
unresolved; changed ordered destinations must be RELOC. Known incoming-edge
checks need prefix handling outside the owner too. Review the full exact-set
diff and production hashes: MakeTxData’s 195-byte gain completes a previously
committed source recovery, while the comparator stage emits no new code.
[Integration](../anonymous-jumptable-proof.md).


Separate local type lowering from allocator state. updateAlpha’s short
quotient restores the HI predicate in initial RTL and combine but leaves
its SI arithmetic pseudo, allocation conflicts and two spills unchanged;
full body becomes205B versus169B (F11660). A recovered TESTW is not a
recovered source body or a spill improvement. Preserve compiler metadata
normalization narrowly and delimit diagnostics before allocated instructions.
[Two-cell control](../v34-alpha-width-controls.md).

Trace allocation boundaries before calling stack traffic register pressure
(F11690-F11691). The isolated updateAlpha reproducer retains the full-TU spill
graph without its debug path; read-only GDB shows global allocation evicting
the locally coalesced numerator/result to give EDX to the divide remainder.
Reload then allocates their stack slots. An in-place energy update, supported
by the blob's ADD/SAR on the same register, keeps the denominator cross-block
and restores register-held arithmetic. Crossed with the independently
observed HI quotient boundary, it recovers the complete169-byte body. Neither
axis alone is exact. Require raw objects from container/plain/GDB controls;
record missing debug variables and rejected dump options separately. Review
inline consumers and register-renamed siblings before adoption.
[Issue #246 tools and controls](../gcc3-reload-tracing.md).


Classify unresolved relocations by the actual consuming instruction before
expanding a dispatch proof. The five remaining complete-body candidates are
four jump-table consumers and one REP aggregate initializer (F11661).
A typed source literal and equal immutable payload prove read values but do
not alone prove canonical destination identity. Audit references/overlaps and
address escape; retain ordinary-section refusal while that distinction is
open. [Screen](../anonymous-jumptable-proof.md).


A post-store reload is a testable source boundary, but recovering it alone
does not recover the whole function (F11662). Minimum-level diagnostics gain
the blob's second cfg+0x60 read through initial, combine and allocated RTL,
yet remain101B versus104B. Review every sibling and nontext relocation, and
count allocated instructions separately from diagnostic listings. Do not turn
an unexplained root+4 carrier into an invented header or volatility variant;
separate-storage fixtures do not establish reachable aliasing.
[Two-cell control](../v34-minlevel-reload-controls.md).


Separate eager evaluation from Boolean-width recovery (F11663): replacing
logical OR with bitwise OR restores the unconditional parameter read, yet
integer promotions produce OR32/AND1 rather than OR8/MOVZBL. Inspect the
callee and inline consumer before drawing a return-type conclusion. Removing
an unsupported index initializer can likewise change scheduling/alignment
without closing the body (F11664). Prove first assignment mathematically,
keep synthetic fixture labels, and preserve failed diagnostic-dump baselines
as invalid; a no-dump control must still reproduce raw production.
[Controls and mandatory scope review](../v90-predicate-initializer-controls.md).


Before changing branch cost, inspect the compiler's default and every folding
gate (F11665). i686 already has cost2; fold_truthop additionally requires two
comparison trees and simple RHS operands. A captured bool predicate retains
branches, while a captured int value enables eager SI folding. A bool result
can restore QI OR yet leave an extra widening. These are distinct pre-allocation
mechanisms, and none is full-body recovery. Caller EAX tests support the int
API; narrow operations alone do not establish a narrow return declaration.
Explain moved table addends using independently checked named-function offsets
and instruction boundaries; keep that object review separate from grading.
[Closed domains](../v90-predicate-initializer-controls.md).


Cross owner lifetime with independently observed scalar boundaries before
attributing a wrapper gap to allocation (F11666). B103 reload recovery and
unsigned result extension are distinct effects; removing the explicit short
count cast emits identical code under the current prototype. Close that
conversion family rather than inventing a caller signature. Signed16 input
loads and zero-extended results bound implementation semantics, not uniquely
formal declarations; validate wider-result behavior with real initialized
components and adequate buffers. A short reference alias can hide the high
result bit even while differential tests pass.
[Full-TU controls](../b103-transmit-boundary-controls.md).


Observe full scalar results at a valid initialized boundary before treating
return-extension differences as allocation noise (F11667). A signed-short
reference alias hides results with bit15 set. Bound buffer indices separately
from input formal ranges: MRF accepts positive signed16 inputs, but its signed
output index limits this fixed 10:9 boundary to32768 outputs. Preserve and
exclude invalid over-limit probes. Audit the shared header and every caller;
unsigned-short versus narrowed-int result families can emit identical objects
without proving the original declaration. Caller-local narrowing remains
independent. [Measured controls](../mrf-result-width-controls.md).


For scalar loop widths, separate narrowing at the update from narrowing only
at function return. The MRF counter-only control restores signed truncation
after increment but does not recover the full helper. Ring-next narrowing
must precede the wrap comparison to reproduce the observed boundary. Keep
convolution indices at their separately measured width; cross independently
evidenced families instead of enumerating arbitrary local-type permutations.
[Ongoing bounded MRF controls](../mrf-counter-width-controls.md).


MRF's nine-cell countdown/conditional cross (F11669) restores all predicted
local boundaries without full identity. Input-loop old-value nonzero tests
and conditional-zero destination identity are independent source mechanisms;
equal lengths can hide different bodies. Close the declared family after
complete review, retain losing explanatory controls, and reframe before
more nearby type/order changes. [Ledger](../mrf-counter-width-controls.md).


Inspect Boolean predicate lifetime across x87 status clobbers before calling
a mismatch allocation-only (F11670). Power-first && recovers an exact72-byte
loop where index-first && skips a comparison, index-first & carries a predicate
across FNSTSW, and power-first & introduces integer-promotion zeroing. Ordinary
logical operand order can recover both access and lifetime boundaries without
casts or forced registers. Cross access/order/operator controls and compare
complete bodies, not just sizes. [Full-TU recovery](../v90-power-index-predicate-recovery.md).


For a commutative op with two memory operands, source operand order survives
to RTL and assigns which load feeds the accumulator and which stays the
memory operand; for a REG+MEM pair the same order is canonicalized away
(REG first) and no spelling reaches the other two-address form (F11671).
Test the flip on BOTH shapes before concluding anything about "operand
order" in general: the encoder's flip recovered the exact 54-byte body -
accumulator `movzbl (%edx),%eax` then memory `xor (%ebx),%al`, with the
input-pointer increment scheduled after the xor as a consequence - while the
decoder's flip emitted a byte-identical object, closing its residual as a
reload form choice rather than a spelling. Exclude re-read spellings with a
behavior argument first: reading `*in` again after the `*out` store changes
`out == in` results, and recorded aliasing parity pins the one-read temp
form. A stale ratchet floor can report a loss the census refutes; check the
symbol against the current exact set before treating it as a regression
(F11671, third documentation of the same pre-existing 810-floor
V90Parameters C2 entry).


An if/else pair whose arms are one increment and one reset can emit with
either arm as the fall-through; the blob's choice is readable from which
block the conditional branch targets, and the mirror spelling of the arms
is the source lever (F11673). Check the tree for an INLINE copy of the same
test before compiling: getV90Decision's state-9 copy already carried the
author's arm order, and the mutation anchors quote it -- corroboration that
cost one compile instead of a domain. A cast on the READ of a struct field
folds to a nop conversion and cannot change the load's extension; the
extension follows the field's declared type, and a raw displacement probe
over a TU is contaminated by any other object at that offset (F11673).


A store whose emission position matters can be unpinned from its source
position only when the scheduler is indifferent; when the blob places it
after an unrelated load, the source position is the lever and the load's
own position is the answer sheet (F11675). For two loads feeding two
stores through a pointer the compiler cannot prove disjoint from the
destination, adjacent reads are the only spelling that yields
load,load,store,store -- interleaved statements serialize on the first
store. And check row-indexed rejection reports for desync before reading
a WIDTH/operand row as a type difference: one shifted store re-numbers
every row after it.
## Cross source arithmetic boundaries with observed induction widths

F11692 transfers updateAlpha's in-place arithmetic diagnostic to
V34TimingHPFilter, without transferring its spill explanation. Short index
alone matches length but leaves BYTES29; in-place carry multiplication alone
also fails. Together they recover all 76 bytes. Inspect combine before local
and global allocation: a named carry update can change the two-address operand
and lifetime even when neither object spills. Feature screening is triage;
stack references are not spill evidence.

F11693 recovers V34TimingFiltersInit's six fixed-offset stores and short
induction, then its member-relative history clear. A byte-size expression used
as an element count reproduces the known D29 overwrite and unsigned loop bound.
A flattened pointer clearing the same bytes need not produce that address form.
Keep reproduction defects guarded and preserve the shipping initialization.
Do not claim six explicit stores uniquely recover the original array declaration.

Audit allocated nontext with canonical relocation targets: a preceding function
shrinking moves a jump table's raw addends while preserving its destination
function and relative offset. Verify that identity before masking bytes.
[Screen and 27-cell reproduction](../gcc3-candidate-screen.md).


## The last narrow consumer can select the extended load

F11694 reopens v8_crc's rejected extraction-spelling family with a different
question. Baseline expansion resembles the blob, but combine removes its
signed extension; the signed-field negative retains a shared HI load and both
extensions, then regmove folds the last extension into the load. GCC3
optimize_reg_copy_3 requires the narrow pseudo to die at that consumer and
rewrites earlier users through subregs. Signed predicate first, unsigned read
second makes that extension unsigned and recovers the entire41-byte function.
This happens before allocation: do not label the operand-role mismatch regalloc.

`-fno-regmove` is an insufficient disabling control: the extension folding
runs under expensive optimizations in the forward pass. Preserve its measured
losses instead of treating an accepted flag as proof a pass did not execute.
[Thirteen bounded controls and replay](../v8-crc-extension-promotion.md).


## Mask tests may be compiler-expanded grouped cases

F11700 recovers VPcmV34GetQuickConnectIndication's59-byte body with a grouped
switch and one initialized result. An early-return switch has identical length
but misses55 bytes. GCC3 stmt.c emit_case_bit_tests introduces the masks at
initial RTL; ordering counts merged case nodes, not represented values.
Inspect the source control's branch/result lifetime as well as the mask values.
[Three-cell switch control](../v34-accessor-boundaries.md).

## Keep a multiply input live until its later consumer

F11701's SNR loop computes product before copying old v to last; the negative
copies first and permits destructive multiplication. Combine retains the
product72/v64/last63 use boundary before allocation, recovering all99 bytes
when crossed with the independent second-loop handoff. Flat-first alone
raw-merges production; the handoff alone merely matches length. A named
intermediate is evidence-supported when the blob consumes its input later;
it is not permission to introduce temporaries for register scores.
[Bounded loop/product controls](../v34-accessor-boundaries.md).

### Cross counter width with store order while preserving argument lifetimes

V34 detectorinit's signed-short nested counters recover its entire loop region,
but the tail still differs. Keep that independently measured width correction
while crossing the bounded source orders of the remaining independent stores.
A literal copy of machine store order changes the coeff argument's load role
and misses; coeff-first source plus count/state/armed/limit/lo/hi matches186 bytes.
Initial/combine RTL separates the counter extensions from allocation and
scheduled field-store order. A machine order does not uniquely identify source
order: use controls and retain argument-load boundaries, rather than assign
register names or broaden a synonym search. One exact candidate out of twelve
permutations is evidence within that domain, not global source uniqueness.
[Full-TU controls and replay](../v34-detector-boundaries.md), F11702.

### Finish one arithmetic channel before the next product

V34 dftupdate's reference PUSH/FILD of the real product precedes imaginary
IMUL, while the reconstruction computed both products and integer updates
before either floating update. Recover the complete real channel before the
imaginary product and inspect combine: the real conversion moves before the
imaginary multiply, changing lifetime overlap before allocation. Combined with
the independently measured advancing input cursor and per-bin input load,
this recovers all200 bytes. Each axis alone misses. Keep wrapping integer adds
and x87 operations/rounding points intact; cross the source boundaries rather
than fit the ensuing registers. [Twelve full-TU cells and pass controls](../v34-dft-loop-boundaries.md), F11703.

### Trace a neighboring gain before interpreting it as source recovery

An independently observed array-base local in V34TimingPrefilter makes the
following, unchanged V34EqualizerCleanUp exact. Compare its actual RTL across
stages while excluding only compiler addresses and MEM alias annotations:
instructions agree through postreload, and the first change is peephole2's
constant scratch. This is a translation-unit scratch carrier, not evidence of
different cleanup source. Recover the address boundary on its own object
merit, report the neighboring gain separately, and keep the nonexact precursor
open. Here a late-coefficient control is closer to the precursor's product
schedule but loses the cleanup gain; that prevents claiming a unique original
source or freezing the early-coefficient choice. Bounded initialization controls
also miss. [37 full-TU controls and firing detector](../v34-prefilter-boundaries.md),
F11705. The related rollback domain is closed without adoption (F11704).


A constant-sized initializer can still reveal a field-derived loop bound.
In `v8_phase_rev_init`, setting `half=32` then comparing the loop index with
`half*2` produces an invariant register64 and the object's compare direction.
A literal64 and a cached local `full=half*2` compare immediates differently;
combine distinguishes all three before allocation. The field-bound form
closes the77-byte function. Do not assume that a binary constant establishes
a source literal, and cross cached versus direct member bounds explicitly.

Retained empty loops can be real source structure. V8's detector initializer
contains a three-iteration short empty loop nested inside its history clear,
as well as a2x2 short section/tap clear. Restoring both gives267 exact bytes;
integer empty loops disappear and removing the short empty loop misses28bytes.
Identify the counter narrowing and the CFG from the object and RTL before
retaining an empty loop; inventing no-effect code solely to move layout is
not this mechanism. These two recoveries combine for344 exact bytes without
bystander changes (F11706; [declared controls](../batch5-v8-detector-boundaries.md)).

GenericToneDetector extends the statement-boundary lever with a measured
ownership discriminator (F11707). A local quotient increment can be
if-converted to ADC even when the blob branches; storing/testing/incrementing
the member preserves its memory updates through ce2. Cross that independent
boundary with the observed reset count/accumulator order. Here both constructor
clones become exact267B while the out-of-line reset stays exact. A shared-zero
assignment loses reset, so do not treat all zero stores as freely reorderable.
Use complete-TU positive/negative controls, inspect combine→ce2, and test callers
that inline the changed method. The two process compare/accumulation boundary
hypotheses gave no gain; their residual is still open, not an invitation to
repeat F1991's closed operand swaps.

### Cross call-load lifetime with transition CFG instead of rewriting register names

Calling tone's saved amplitude is read before TONE_read in the blob and scaled
in place afterward. Current source read it after the call and consumed an
anonymous product. Recover both boundaries, then separate remaining-zero
transition/else-save from the nonzero-save/continue CFG. Source clamp and arm
order controls alone recover length, not bytes; only the combined lifetime/CFG
control is exact215B. Inspect combine's output-store source to demonstrate the
product-carrier change before allocation. Preserve the tone's measured original
period/phase bugs throughout. [13 full-TU controls](../batch5-calling-tone.md),
F11708. V34nlencoder's analogous magnitude-update family canonicalizes and
misses; do not generalize an exact gain into a universal source recipe (F11709).


An original diagnostic is executable source even when the normal debug level
hides it. Recover its exact format, unsigned gate and call boundary, and compare
complete bodies: call_delete becomes158B exact (F11730). Count debug relocation
anchors only to triage; inlining, CFG duplication and shared tail jumps can
change counts without removing a message. The new gcc3_debug_anchor_census.py
reports its object/symbol denominators and a known0→1 restoration control.
The call/Psd finite negative families are closed in their local records.


A cursor/countdown transfer may need a different value-read boundary in its
sibling. FPM_lmsupd closes150B with hist[k--] and an advancing coefficient
destination captured before arithmetic. FPM_lmsupd2 needs its first narrowed
product computed before capturing that destination; both int and short product
carriers then emit the exact182B body. Initial RTL identifies the t/dest source
order before register allocation, and the failed earlier capture produces an
extra frame slot and coefficient load. Keep the measured common property,
not a gratuitous local retype. The two gains combine without moving the third
TU body; the isolated first gain still changes its non-exact sibling through
allocation state. [72-cell record](../batch20-dsp-boundaries.md).


Separate configuration spelling from literal correctness. V23's receive
constructor retains an authentic original version print and its observed
initialization boundaries, then an independent literal control recovers29000
from the object's0x7148 (F11732). Neither a near instruction match nor old green
fixtures justify keeping28998. Audit constant/table emission order explicitly.
Original B103 case/default diagnostics close two state functions (F11733),
but later shared-prototype changes can undo that match; measure the combined
TU rather than summing isolated winners.

The switch/branch factoring lever now has a two-axis control in float2Bits
(F11734): a case0/case1 switch recovers its common no-match return, while
placing the subtraction arm under a negated whole table predicate recovers loop
fall-through and branch direction. Neither source component is exact alone;
the combined fullTU reproduces283B and leaves the enclosing CPPacker unchanged.
Preserve unordered routing with whole-predicate negation, not an ordered
opposite comparison. Inspect initial RTL dispatch and table edges before
attributing layout to postallocation scheduling. Failed DIL zero-trip/code-load,
ModulusDecoder split-product/add, and Parameters fabs/store crosses are closed
bounded families, not reasons to retest source synonyms.

Before classifying small residuals as compiler-only, compare original debug-call
anchors even for completed source. B103's four missing prints explained most
of its SIZE residual and exposed authentic CFG, field-owner and widening-loop
boundaries (FINDING_B103). The object's sole emission order plus recovered prints
and config zero predicate make create exact406B and untouched exit exact49B.
The shrinking non-exact process exposed a true count..0 versus count-1..0 error;
record it as behavioral reconstruction and run the final batch's period gate,
not as an inert expression choice. Keep failed direct-param, unsigned-local,
split-status and count-address controls: none closes process's remainingSIZE11.

A float expression assigned to a double temporary is still a source boundary.
realfft's four h1/h2 locals retain DF destinations after previous operand-cast
cleanup; changing only those destinations to float recovers its exact505B
body. Initial RTL verifies DF→SF while x87 retains excess precision without
intermediate memory stores. Do not infer source type from the absence of a
spill. Full TU remains two exact symbols with identical nontext metadata/data.
[Width control](../batch20-realfft-width-controls.md).

Direct helper output pointers and delayed feedback history acquisition can
recover member-store lifetimes without recovering a function. FloatARMA's
cross reaches scalar392B but stillBYTES152; block remains80B short. Close the
bounded family and reject size-only candidates.
[Output/load controls](../batch20-arma-output-controls.md).

A FABS mismatch can originate at two different GCC3 stages. For getTimingHistoryStd
(F11737), positive/complement ternaries fold before initial RTL, while the
negative ternary becomes ABS at21.ce2. Explicit positive/complement sign assignments
preserve the blob's compare, +/-1 load and multiply; negative-first statements
preserve the operations but reverse their layout. Cross predicate and statement
form, retaining whole-predicate semantics, before declaring arithmetic spelling
or register allocation exhausted. Exact body plus full-TU nontext/literal auditing
is required; record both initial and late folding boundaries.

Call boundaries can bound index lifetimes without permutations: V90 history
resample's original increment follows its read-only timing getter, while the
baseline carries next across the call. Late local alone misses; direct member
increment/wrap closes170B. Cross new source gain with earlier same-TU gains:
this six-cell cross retains both resample170B/Std66B and audits the14bystanders.

A signed-word callee load proves consumption width, not necessarily formal width
(F11738). Cross consistent declarations/definitions with narrowing at use and
caller return/owner lifetimes. Require the callee raw control and all consumers;
B103/V21's four transmit gains leave MRF raw unchanged. Compose same-TU winners:
the B103 Answer diagnostic-only gain is forgone at peephole2 in the combined
candidate, and is excluded from the batch count rather than fitted with padding.

Historical replay after a deliberate header change must use an explicit revision
snapshot, not suppress the default drift guard (F11739). The shared driver's
--historical-headers mode hashes Git inputs and records include paths; the raw
baseline remains mandatory. Default current-header mode still rejects drift.

A saved result register across a diagnostic can identify source result ownership
(F11740). ConnectionEvaluator retrain twins close only with an entry-owned
verdict and shared cleanup; equivalent early returns materialize constants late
and omit the saved register. Cross result definition boundary and shared/duplicated
cleanup before concluding extra saves are regalloc noise. Compile both twins and
all TU bystanders; a post-counter negative cell changes a later nonexact carrier,
while the chosen entry-verdict cell preserves it.

For a weighted update with unchanged member before arithmetic, distinguish cached
local and direct member product inputs. ConnectionEvaluator average's member
product and hot-first CFG are each insufficient; their cross reproduces113B.
Initial RTL shows the additional source member read. Preserve unsigned integer
conversion, full extended precision and the single final rounding; do not create
float temporaries or reassociate an expression to match a size. Cross independent
same-TU winners before adoption: all three gains here survive together.

Census opcode and call-count gaps require provenance before source hypotheses
(F11741). SpectralVerifier process inherits its extra FABS from inlined
printSpectrum; getTimingHistoryStd had a local conditional-sign boundary.
Similarly, two distinct addresses or debug relocations may reflect shared tails.
Trace the instruction back to the source owner before reopening a closed family.

Whole configuration capture is a separate source boundary from pointer-store
permutations. V22 FSE's bounded copy-before-assignment control recovers instruction
structure but leaves five register bytes; retain that grade-1 preimage for compiler
allocation tracing and decline production adoption until strict proof.

A short source loop counter can still have an SI pseudo in GCC3 RTL. Tone IIR's
short tap counters expose low-word sign extension and cmpw boundaries while
pseudo mode stays promoted (F11743). Literal four-section factoring plus short
counters recovers most of both progress bodies but not strict identity; a separate
shift-array capture recovers another seventeen bytes yet remains three short.
Retain the measured preimages; do not invent padding or force stack slots.

Cold diagnostic blocks: address order is not callback execution order.
Trace branch targets and rejoins before adopting source order. CALLPROG_Dial
parameter-29 debug read at higher address executes before the mainline read;
its CFG rejoins before that call. Branch-specific parameter rereads may be
observable even with constant getter values. Preserve host-read/printed
sequence and counts across valid lifecycle/debug states; require explicit
getter denominators and an old-source failing control. Equal function size
(1094B here) is insufficient: final residual remains BYTES422. F11742 and
docs/batch20-callprog-callback-order-controls.md carry complete-TU and runtime
controls, including the rejected intermediate seed rather than hiding it.

The incoming argument may be the original default result. _handle_status's
if-assignment form (F11750) keeps it live at entry and recovers44B; sparse
switch/return and switch/assignment alternatives do not. Transfer result-lifetime
hypotheses through actual dispatch forms, not a fabricated register carrier.

Compare read ownership across output stores before treating a renderer as a
register mismatch (F11751). CID's high output store separates two input reads;
eagerly cached nibbles hide that alias boundary. Predicate direction can expose
signedness even when earlier signed/unsigned source variants were raw-equivalent:
late read plus decimal-first signed-byte ternaries recover two complete bodies.
The changed assumption is the new observed branch direction, not a lower score.

Source cursor loops can expose GCC3 reversal that indexed loops block: V34 history energy compiles an ascending counter plus `*hist++` into the blob DEC/JNE. Trace `.09.loop` before inferring source count direction (F11754). For short paired history copies, flattened stores can remove genuine loop topology; restoring the two-step V8 loop recovers161B. Full-register shifts with a byte ABI may require an unsigned working copy before lookup, as charFlip proves.

Full-width masked message indices and branch orientation can interact: neither mask nor arm order alone recovers the eight modem reporters. Status stores can precede child/request sampling, and boolean clear-then-conditionally-set can preserve reads that direct boolean assignment hoists. Use actual alias/read boundaries and cross independent witnesses; do not treat store/read rearrangement as behaviorally inert (F11753). Existing union bytes can express observed byte flags without inventing a new shared type.

A common default result needs its original initialization boundary as well as its switch shape. vce_get_sreg clears its result after the external getter: moving zero before the call creates a callee-save live range and changes the prologue. Explicit guarded assignment1 instead of a boolean expression is also needed. Cross these independent instruction witnesses; the result alone is not a new general store-order lever (F11756).

CID extends callback-boundary reconstruction: an output buffer initialized after memset, combined with owner-relative stores, preserves the original call-setup scratch lifetime and reaches145B. Neither axis alone reproduces it. Capturing a child by itself did not explain the residual; trace the full owner and return-buffer lifetimes rather than fit register names (F11759).

A fullwidth cosine formal can eliminate caller byte extensions while the callee consumes exactly the lowbyte. Cross consistent callee/caller declarations with real output cursors and verify all header consumers; signed/unsigned full formal may remain indistinguishable. V8 TONEq90B needs both width and cursor, while ANSam remains nonexact (F11760).

For fully covered unsigned modulo switches, defensive result initialization can destroy the original live range. A common fullwidth result with explicit narrow negation reproduces six V90/V92 leaves when allcases are proven covered; never leave an actually reachable result uninitialized. Anonymous jump-table operands need proven ordered destinations and CFG/stack ownership, including meaningful negative controls. Measure comparator support changes on the immutable baseline before attributing source gains (F11761).

### Cross ownership with value lifetime before closing a return-spelling domain

The batch50 pulse digit separates original input from corrected count and acquires the child before the zero branch: neither alone matches, both recover146B (F11762). V90RDetector's compound member update first restores store/reload, then a positive complete-group guard retains the initialized verdict and closes four functions (F11763). Return synonyms being inert on one ownership graph does not exclude their interaction with an independently witnessed graph. Require the full finite cross and inspect unchanged exact neighbours; avoid arbitrary reordering or register padding.

### Compare expression mode and copy width, not just constant bytes

A0.5f→0.5 expression-mode change recovers V90 enterDataPhase190B while its entire constant pool stays byte-identical (F11765); don't infer source literal width from pool bytes alone. FIFO_create167B needs whole-config copy crossed with capacity zero-extension and signed cursor use; neither local signedness fix nor copy alone succeeds (F11766). Audit high-bit behavior from operands, not the signedness of one branch opcode.

### An early initialized common result can be the original debug-call lifetime

Voice DLE196B needs switch arms assigning a shared status initialized before callbacks (F11769). Literal returns remove its carrier; an equivalent if assignment graph still misses. The accompanying merged-string pool changes order but not values. Review allocated data order alongside code; exact functions are not full-object identity. Do not normalize an unrelocated local-call distance simply to bank an otherwise matching dialer: the unresolved callee/TU layout remains a separate witness.

### Snapshot callbacks only where the original acquires them

Rx voice_set_rx305B closes when callback-owner reads follow the original calls: the silence callback is acquired after detector setup and each gain callback is read from its owner at use (F11770). Capturing a function pointer at entry extends its lifetime across callbacks and can change both allocation and visible owner reloads. Cross acquisition timing with per-use owner access; preserve the callback prototype and returned-word interpretation. Neither timing nor direct gain reads alone reproduces this example.

### Reconstruct the state that lives through diagnostics

V90/V92 initiateFPE/RRN assignments before diagnostic calls reproduce four leaves; equivalent assignments after calls prevent original late tail merging (F11772). A residual attributed to crossjump freedom can still have a recoverable source carrier. Use pass dumps to locate the first actual merge: here26.postreload retains both sites and27.flow2 merges only the recovered graph.

Fax reversal requires crossings of loop-counter width, postdecrement grammar and shift-before-OR narrowing (F11771). Matching one loop opcode alone does not establish the source. Literal frame open-coding additionally needs the original unsigned header stride; inspect high-bit address extension separately from loop branches.

### Read declared union views at the original width

A byte flag write followed by a word-mask test can differ from two byte-member operations even when their values agree: GCC caches the byte value across a conditional join and adds a CFG edge before register allocation (F11773). Initial RTL distinguishes QI byte from SI word guards, and the post-GCSE loop dump first distinguishes their jump counts. The original word read plus an exact sister function establishes the union view; use existing declarations and corresponding shifted masks. This recovers RxHdxIdleV17/V27 without volatile or alias tricks. Pair arm orientation with memory countdown ownership and narrow callee-result use for related epoch states; none of the partial source controls suffices.

Whole enclosing-object ownership must sometimes cross both pre-call reads and post-call stores (SetScramblerV27, F11775). Holding the nested helper pointer on either side alone does not reproduce original92B. Preserve the observed post-call owner reread rather than caching a child across initialization; inspect the enclosing address formation, not just the final equivalent field offset.

Separate variable and fixed clearing arms may be source structure, not compiler threading (_idle_state, F11776). Check original loop bounds and adjacent exact witnesses: named existing bound160 retains CMP160/JL whereas a literal loop canonicalizes to CMP159/JLE. Cross this boundary with the observed arm graph; do not infer source from length alone.

Anonymous tables in ordinary push-save functions may be comparable even with direct calls (F11777). Prove the frame, ABI call contract, protected guard and every ordered case destination before canonicalization; do not mask anonymous relocations. Validate rejection controls through the actual comparator and audit old/new immutable baseline classifications separately. V29 transition byte writes then recover467B with zero unrelated body changes.

Return the observed owner, even on a null diagnostic path, when its lifetime is visible (create_dtmf, F11778). A literal returnNULL removes that live-through-call object and may duplicate the epilogue; a failure diagnostic or positively guarded initialization followed by common return recovers228B. Adjacent constructors can still miss under the same family: keep the complete-TU negative control rather than generalize from one win.

Arithmetic preparation can begin across a store group while completion remains afterward (_recieve_silence_state_init, F11779). Compare the whole dependency chain: signed half-count SHR+ADD before state stores and SAR after them witnesses an earlier initialization of the existing local. Transfer that observed lifetime rather than permute stores until scheduling matches.

Existing helper factoring can settle nearly an entire wrapper while leaving a dead scratch register (FDSP_DP_Run, F11781). Calls to its two conversion helpers reproduce137/138 bytes and all relocation destinations; remaining POP differs first atpeephole2, not register allocation or CFG. Cross independently proven predecessors before attributing TU history. Do not alter a return ABI or add dead locals to bank the last byte. Final scheduled RTL may place epilogue instructions before the epilogue note; use the complete function to confirm surviving instructions.


### Inspect terminal source edges before attributing a constructor frame to allocation

An explicit return in a switch's final diagnostic arm can select a sibling jump where break retains CALL and cleanup (V92Modem, F11782). Here it restores the original frame and exposes a separate retained parameter versus mutable-member reload across construction calls. Cross the two observed boundaries: neither alone reproduces either constructor, both recover both clones. Initial RTL and02.sibling distinguish source age and terminal control before register allocation. Do not generalize return synonyms without the original tail-call witness.

Inspect period libc headers and preprocessed source before assigning an FPRem to unsafe optimizer algebra (PHASOR, F11783). Glibc's __FAST_MATH__ inline wrapper may explain it even when macro-only and optimizer-flag controls are raw-identical. Keep diagnostic profile changes and exact losses visible; a wrapper preimage is not a recovered whole profile.

Duplicated cyclic scratch arrays can support affine state-index arithmetic without modulo (VTB, F11783). Prove the index range over the actual state domain rather than replacing every read with XOR. A new private emitted helper is an inventory change to review, not permission to force inline; literal source expansion is a separate bounded control. Close raw-equivalent loop spellings instead of expanding the syntax domain without new witnesses.

### Classify complete mismatches before assigning an allocator cause

F11789's immutable300-object inventory accounts for811 nonexact symbols;
only40 pass the complete existing alpha proof with proven relocation identity.
Instruction differences, size/call gaps and unresolved destination proofs are
observations, not an inlining-budget diagnosis. Use typed call destinations,
not printed relocated call-site offsets. Score every defining copy, not a
favourable COMDAT. [Inventory and witnesses](../gcc3-mechanism-classification.md).

For a dead epilogue scratch, observe the installed compiler's TU-wide search
history before changing source. F11790's read-only GDB observer preserves all
five emitted objects raw, while a captured eligibility model reproduces154
supported choices out of163 recorded searches. Removing the evidenced final
predecessor predicts and produces a different scratch, but removes an export
and cannot be adopted. The cursor required for ECX is a conditional constraint
under that captured state, not recovered original flags or TU ordering. Never
insert dummy allocations or permute definitions just to obtain the cursor.
[Reproduction tools and results](../gcc3-mechanism-results.md).

Extend the terminal-edge lever with an instruction-boundary relocation census:
F11791 finds three candidate bodies, of which explicit default return closes
V92Modem::reset138B at02.sibling. The same spelling misses V90 reset and is
inert in ADID; inspect complete bodies and independent witnesses before a
cross. Signedness can recover an original loop branch without recovering the
function: F11792's four constellation controls gain zero and close that finite
domain. Keep negative controls as evidence; do not adopt partial score gains.


### Distinguish a two-address destination from an expression of the same width

A byte XOR can differ before allocation even when both source forms already
expand in QImode. ParallelDifferentialDecoder's preserved input feeds a later
state store; a separate narrow decoded result updated with XOR supplies the
original destination identity (F11795). Combine keeps input first and folds
state memory second, letting reload emit the original byte copy/memory XOR.
This closes57B where cached-pointer and commutative spellings did not. Trace
initial/combine/reload operands, retain alias-visible store ordering and audit
all compiler-discovered header consumers. Do not add temps just to tune colour.

Expression mode transfers across independent original half-load/pop witnesses:
ADID's two mean methods need double addition although their stored0.5 constant
is still float-sized (F11794). Cross sister methods and inspect inline consumers;
constant bytes do not determine expression type. The same discriminator narrows
F7846's Uref to an extra four-byte frame allocation (F11796), not an exact
function. A complete instruction-sequence match with wrong stack offsets remains
nonexact. Require a slot-allocation/lifetime witness before more source forms;
never normalize away the frame to obtain a gain. [Controls and results](../gcc3-value-carriers-results.md).


### Separate local slots from known-callee alignment

A four-byte frame mismatch need not be another local or a global flag. Trace
assign_stack_local_1 and ix86_compute_frame_layout in the installed period
compiler, preserving emitted objects raw. Uref's four allocations agree;
the difference is padding2 propagated from its already-emitted callee through
cgraph_rtl_info. Recovering the original constant quiet NaN instead of a
library nanf call lowers that callee's known incoming boundary128→32bits;
crossing it with independently evidenced double-half addition closes240B
(F11798–F11799). Keep flags/function order fixed, inspect the callee's object,
audit inline consumers and pool/table relocations, and require valid target
observations plus refusal controls. Do not infer dead locals from stack size.
The other three NaN sites supply bounded no-gain controls, not a general
license for spelling changes. [Results and replay](../gcc3-uref-stack-results.md).


### Check publication, binding and saved-register padding separately

An emitted callee does not necessarily publish an incoming alignment value.
Installed GCC3.4.2 guards publication with binds_local; weak/template controls
return known info with boundary0, so callers keep the default16bytes (F11802).
Aligned double slots still publish4bytes in the strong-leaf controls: local
alignment is not a call requirement. Source-order pairs are raw-inert under
the retained unit-at-a-time pipeline; no production visibility/order fit.

Near-frame vector callers instead retain16-byte incoming-call alignment while
direct member indexing removes an extra saved register. The resulting12bytes
of padding disappear (F11804). Trace post-call value ownership and narrowing:
one vector uses a late conditional return, its sister uses an unsigned wide
level and one terminal short conversion. Cross the sisters independently,
audit losing single-factor cells and all bystanders. The unchanged TRN gain
is a measured peep2 cursor effect with matching eligibility, not additional
source recovery. All272 scratch choices replay; no cursor forcing or dead
carriers. [Screen, controls, audits and replay](../gcc3-alignment-results.md).

### Pair counts with their derived lengths before fitting stack sizes

GenericIIR C1's extra saved register follows source ownership, not a new
alignment flag: the original count/derived-length pairs precede coefficient
pointers. The three-cell pairing×pointer-age boundary closes107B only when
both properties agree. Installed24.lreg puts blockSize inECX with12-instruction
span rather thanESI with16; C1/C2 copies alone change (F11806). Observe the
parameter's pseudo and local allocation before interpreting its frame.
Do not generalize an exact neighbor's statement order or enumerate permutations.

### Distinguish a wide unsigned magnitude from an unsigned short carrier

MOVZWL, full-width NEG and a terminal MOVSWL can describe an unsigned32-bit
magnitude with one signed-short conversion. A signed/unsigned *short* matrix
has not tested that family. Independent original witnesses recover V92 Ja77B
and E1u54B (F11807–F11808); fields/return ABI stay unchanged. Explicit post-call
capture is raw-inert for Ja. Inspect inline copies: E1u's five-byte removal
recovers the original88B arm even while the enclosing pump's SIZE gap grows.
Keep original load, arithmetic and conversion boundaries rather than fitting
aggregate length. CPt's wide near-hit stays declined.

Opcode screening is only a candidate generator (F11809). Require operand/path
tracing: NEG may build a quality mask or modify a different stored amplitude.
Six masked-count controls and four larger-pump ownership cells do not transfer
the gain. The pump's post-call lowering is already unchanged, and its remaining
remote tails need compiler-stage CFG evidence (F11810). Do not reopen cast,
ternary, declaration-order or flag matrices without a new discriminator.
Complete domains, audits and replay: [sample transfer results](../gcc3-sample-transfer-results.md).


### Preserve a used boolean value before a zero-selection mask (F11812)

An original SETcc/NEG/AND sequence can distinguish a separately used boolean
from a direct conditional expression. In the period compiler, direct
`n & -(quality != bad)` and multiplication controls fold to branches already
in initial RTL. Capturing `int reliable = quality != bad`, then using its
negated value to mask a wide count before the terminal short return, reproduces
`RxHdxDataV17`. V27 and V29 additionally require independently witnessed public
argument/consumption widths and existing union byte flag writes. Cross these
axes: the width controls alone had previously failed. Review complete caller
and callee TUs, including non-exact bodies and every header consumer.

This is a bounded source-value ownership lever, not a reason to rewrite all
ternaries. Ring-wrap transfers recover SETL/NEG/AND but still miss complete
functions; an opcode screen must be traced to the same value/path. A widened
source carrier can be raw-inert once optimization keeps the narrow result wide,
so full-register index use alone does not establish an `int` local. Demapper's
BYTES1 SIB commutation and a same-size normalization helper remain misses.
No declaration, register, slot or equivalent-expression permutations follow
from these partial successes. Reopen a closed family only with an independent
original operand or compiler-stage witness. See
[next20-byteexact-results.md](../next20-byteexact-results.md).


### Trace writable loop memory before calling a LEA/copy difference regalloc

A count updated through an output pointer can be promoted by GCC3's loop pass,
leaving one final word store and a separate next-value pseudo. A private int
counter followed by one terminal publication is a different source family.
Original LEA1/copy/backedge/word-store operands justify crossing these two;
`09.loop`'s explicit `Hoisted regno ... r/w from (mem:HI ... count)` diagnostic
establishes the promotion in the reconstruction. FPM_div additionally needs
helper-owned count initialization after the zero guard and becomes EXACT150
(F11815). Keep initialization placement as an independent witness.

The mechanism transfers to log/sqrt/V8 normalization without closing their
complete bodies. A split increment alone does not establish a missing helper:
V8's bit helper already updates pointed structure memory. Do not force output
slots, permute declarations/parameters, remove checks, or tune late registers
from near hits. Trace output storage lifetimes or the remaining source operand
boundary before reopening a closed family. Full domains, detector controls,
audits and replay: [loop-memory results](../gcc3-loop-memory-results.md).


### Separate an aligned HI machine move from a wide source read (F11818)

GCC3's `*movhi_1` can emit MOVL for an aligned memory address while both RTL
operands remain HI. An original DWORD load therefore does not establish an
int local. Trace initial output homes and the final annotated machine pattern:
FPM_div_32's count moves from esp+18/MOVZWL to esp+16/MOVL when its two
addressed word outputs follow the independently exact sibling's declaration
boundary. Both separate/grouped forms match147B; the scalar-only control is
raw-inert. This is storage ownership plus address-dependent instruction
selection, not a late register permutation or artificial alignment fix.

Sqrt transfer restores every live instruction/operand but leaves one dead
POP byte; log result-owner controls are raw-inert. Keep those misses separate
from the reciprocal recovery and do not adopt partial bodies. A justified
output-order reconstruction requires actual initial addresses, source/storage
witnesses and complete-TU controls; it does not license permutation searches.
[Domains, causal traces and replay](../gcc3-output-storage-results.md).


### Trace run publication and captured decisions before blaming registers (F11821)

An assignment receiving a helper return can publish a remainder after a loop,
while a pointed helper publishes it before rounding and pushing. V8's original
operands distinguish these families. Guarded-do countdown restores DEC/JNE;
flag-preserving moves can intervene, so require the real flag dependency rather
than adjacent mnemonics. Unsigned bit formals independently recover zero loads.

Trace decision arithmetic before dispatch: original SUB/TEST/SETG captures a
wrapped energy difference, silence replaces it with2, then a switch dispatches
0/1/2. A direct comparison is a different family at overflow boundaries. An
unsigned decision carrier restores original CMP1/JB where signed emits JLE.
Twenty complete-TU controls recover these individual witnesses but no exact
function. Do not adopt a near size, assume algebraic values are reachable modem
histories, or force a parameter subobject from its register base. Seek an
independent typed-extent/caller witness next.
[Bounded controls, full audit and replay](../v8-remainder-owner-results.md).


### Compare predicate operands before attributing tap loops to scheduling (F11822)

V8's original tap loops use unsigned <= predicates. Changing only j to unsigned
changes their exit tests from GT/GT to LTU/GTU already in initial RTL. Final
JAEs/JBE differ in mnemonic because the first operands commute; inspect both
operands before declaring a failure to reproduce signedness. Keep signed
outer position/limit comparisons separate. A streaming input cursor with a
signed countdown independently restores the original first JNS loop while
preserving increasing sample addresses. These witnesses do not produce an
exact V8 body and are not adopted from a size fit.

Only demod forms the receive base0xc2c; mod forms transmit base0xc20, and the
initializer writes absolute fields. An explicit LEA/ADD census is bounded
instruction-form evidence, not typed-extent proof for a new subobject. Seek
independent source/caller evidence rather than fabricate aliasing structures.
[Five-cell domain, observations and replay](../v8-index-cursor-domain.md).


### Follow real load UIDs through combine before blaming source evaluation order (F11823)

V8 coefficient-first expressions and a named first coefficient both change
initial RTL capture order and emit the same complete object. First CSE removes
repeated inline sample loads. These forms remain nonexact. Trace actual HI
memory reads: combine folds a load into the extension UID, so a disappeared
word-load UID does not prove the read disappeared. A surviving register-only
extension also does not prove a memory capture.

For the first input tap, coefficient-first survives allocation/reload/renaming
and31.bbro;33.sched2 reverses the reads to sample-first. More expression
synonyms do not resolve this observed scheduling boundary. Use verbose
scheduler diagnostics with a mandatory raw-object repeat, then inspect GCC3's
dependency and ready-queue decisions. Do not invent volatile/alias dependencies
or infer allocator causality from mnemonic order. A duplicate-stream dump
must be explicitly unparsed rather than silently selecting one stream.
[Four-cell domain, UID trace and replay](../v8-product-capture-domain.md).


### Check consumed callback returns before treating EAX as incidental (F11827)

A void reconstruction adapter whose table consumer reads int status may have
lost its original return declaration. Read the caller and all count/result stores:
eight fax process adapters preserve the modem return across their stores in the
blob. Explicit int forwarding makes all eight EXACT62 and removes obsolete
callback casts without changing the dispatcher object. Audit every shared-header
consumer and table relocation; one exact wrapper alone does not settle the API.
[Return contract and ten-consumer proof](../fax-adapter-result-results.md).

### Capture one final field result when both branches publish it (F11825–F11826)

BitsToSymbol reset's if/else stores become one guarded unsigned result with a
conditional expression; both supported accumulator/conditional forms are raw
EXACT108. CID reset likewise needs final samples_fill publication after its
conditional mark update; the constructor's inline consequence is reviewed too.
Use original common-store/control-flow evidence, not arbitrary statement order.
[Combined gains and bounded negatives](../byteexact-scheduling-batch-results.md).

### Read scheduler tie-break dependencies after allocation (F11824)

Equal-priority loads need not retain initial order. V8's coefficient/sample
priority35 tie is broken by4versus5outgoing dependencies, matching the official
GCC3 scheduler ranking path. Verbose-only diagnostics must raw-repeat the object.
Trace earlier ranking gates and actual ready selection before interpreting a
fanout count; include anti/output edges created by allocation. Original hard
register reuse can constrain order differently. Do not force registers or fake
source dependencies to reproduce it. [Measured diagnostic controls](../batch-scheduler-trace-domain.md).


### Recover owner acquisition at the callback boundary (F11829)

FAXVMI_process327B becomes exact by reading the framer after processing
callbacks, at the original owner load. An entry cache extends a child lifetime
across calls even when the member is normally stable. Preserve initial scalar
snapshots separately from later owner reads. This transfers only where the
blob independently shows that boundary: V21 next-state and HDLC unframe reload
controls reproduce individual operations but remain nonexact. Trace remaining
conversion/inline graphs before further source variants. Full-TU audits must
include source-unchanged inline callers and reordered merge strings in negative
controls. [Domain/results](../fax-publication-batch-results.md).

### Read the ownership of heterogeneous arithmetic operands (F11830)

voice_modem338B closes with det_len+saved: original word-extension and full-word
memory reads feeding ADD isolate the arithmetic operand graph. Keep existing
local widths and interfaces fixed; do not assign registers or permute unrelated
statements. Promoted ushort sum is bounded and final narrowing remains unchanged.
This is separate from detector formal-width controls. The33TU/26small-symbol
screen's known input fires, but its six-instruction pattern is a bounded triage
rather than proof every remaining sum was examined. [Evidence/replay](../services-voice-count-sum-results.md).

### When the operation matches, trace the remaining compiler dependency (F11831–F11833)

Unsigned wrapper minima/remainders, original FSE copy boundary, ADD-then-SUB
reversal energy and GenericIIR member publication can each recover operations
without closing a function. Size proximity is not a next discriminator. Hold
the witnessed source graph fixed, find the first changing RTL pass, and predict
a new source or visibility boundary before reopening the finite domain. No
register/slot/type permutation or source/profile score selection follows from
these negative results. [Bounded batch and next phase](../byteexact-contract-batch-results.md).


F11832's FSE trace now measures sched2 as the first changing pass:29validated
streams preserve copy→freq→phase_acc through allocation/reload/rename, then
sched2 emits freq→phase_acc→copy. Explicitly exclude the duplicate GCSE stream.
Inspect scheduler memory dependencies and priority carriers next; changing
allocation or declaration order is not supported by this trace alone.
[Replay tool](../../tools/fse_copy_stage_trace.py).


### Observe memory alias edges before fitting scheduling or registers (F11835)

The FSE cfg copy's type-set2 has edges to short stores but not integer fields;
those integer stores priority6 exceed copy5 and issue first. Builtin memcpy
set0 adds both edges, so copy8 must issue first. Two verbose raw repeats and
actual Gentoo ready/issue tables establish this mechanism. Official alias and
scheduler sources explain it, without recovering original memory tags or the
complete Gentoo patch stack. Neither control is byte-exact. Do not invent
integer/union fields, void casts, volatile or profile options to make an edge;
require independent original field/copy/API evidence.
[Trace, source hashes and replay](../fse-scheduler-dependencies-results.md).

### Fixed-entry source expansion can restore the enclosing helper budget (F11837)

calcModulusParameters622B needs original five modulo/six division sites and
six scalar place stores; retained loops expose1/2sites. Expand both ordinary
fixed-entry sequences, then preserve the observed heterogeneous sum operand
roles. Full body becomes exact and getPower regains its original out-of-line
CALL as helper complexity increases. Audit the caller too: its574B shape still
has exchanged double homes, and all six jump-table destinations must be decoded
and checked against ordered original cases. Do not claim source-correctness or
profile uniqueness from recovering a helper call boundary. V92's CRC promotion
transfer recovers mechanism/length but no complete functions and stays unadopted.
[Complete-TU evidence](../batch-cpp-constellation-expand-results.md).

F11838/F11839 additionally distinguish measured later optimization and actual
spill migration from a blanket inline-budget diagnosis. V32 source publication
survives reload then collapses by sched2; V29's standalone arithmetic swaps an
eq spill for a distance spill. New original storage/graph evidence is needed
before reopening either family. [Batch limits and next phase](../byteexact-dependencies-batch-results.md).

### Trace late call merging and replication before inferring source repetition (F11842–F11844)

The widened C++/DSP screen nominates two branch graphs, not fixed-entry
expansions. In V92 reset, explicit length-arm diagnostics stay separate through
reload, merge in flow2 and duplicate again in bbro. V90 reset's arm-local DIL
calls merge in flow2 and stay shared; the complete object is unchanged.
Thus final relocation multiplicity cannot determine source statement count.
Use source RTL, postreload, flow2 and block-reordering dumps to distinguish
these mechanisms before expanding source. Preserve observable per-iteration
mode reloads and one diagnostic per execution. These finite source controls
gain no complete function; require a new original graph witness before reopening
them. [Screen, controls and replay](../expansion-wide-results.md).

### Compare backedges inside the witnessed arithmetic loop (F11845–F11847)

A whole-function edge count can both nominate unrelated branches and miss a
real inner-copy mismatch. V92 phase unpack has11backedges on either side, yet
the original32input CRC loop has one and the retained loop has two. Constant
member shifts remove the inner copy in all three receive CRC consumers, but
none closes completely. Identify the counter comparison feeding the specific
backedge; do not mistake a later checksum failure branch returning to reset
for an enclosing arithmetic loop. Audit all switch destinations after layout
changes and the other functions in the TU. This improves lever13 triage rather
than adding a source spelling with guaranteed gains. EIA6 also recovers18copy
operations but leaves real x87 lifetime/operand differences; SIZE2 is not two
different bytes. [Screen, graph evidence and closed controls](../fixed-index-results.md).

### A nonpopping live SI conversion can constrain the source mode (F11848–F11851)

GCC3 reg-stack strips FIX before handling its source and duplicates a live XF
value if the stack has space. It adds physical REG_DEAD, so the emitter pops
that copy. Live SF/DF sources bypass this rule. Therefore original FISTL with
proved live input and spare capacity is evidence against that XF producer;
final REG_DEAD alone is not evidence the logical value died. Exclude DI's
hardware-forced pop, full-stack cases and unproven entry/call history. Actual
Gentoo controls corroborate the hash-pinned stock rule; patch-source identity
and unique C types are not established.

The constraint is on RTL mode under the measured profile; an unknown
long-double ABI or TU option can change the declaration-to-mode mapping.

EIA6 SF/default-double math recovers FISTL/FMULP/FCOMPP, while its constant
10000 still occupies SF storage. Constant-pool width does not determine
expression precision. Those isolated controls and the two beta mode transfers
did not become exact. F11852–F11853 below close EIA6 with independent use,
lifetime and tail-reload evidence.
Trace register-zero folding at combine, register-scale folding at lreg and
later stack physical deaths separately; do not fit declarations/registers/
slots or remove a defined shift mask for a near-size match.
[Complete evidence, closed domains and tools](../eia6-x87-results.md).

### Trace argument conversions, then verify the actual tail edge (F11852–F11854)

An apparent x87 scheduling difference may begin at combine: a fractional
FIX used by a conditional argument can sink into that argument after another
conversion. EIA6's builtin integer abs and explicit earlier if keep fraction
conversion earlier and restore magnitude conversion directly to outgoing+8.
They emit identical complete objects; neither alone proves original source.
An independently observed early FABS/live result then recovers the diagnostic
prefix exactly. Follow source uses and conversion homes through the passes;
do not fit types or stack slots to partial opcode agreement.

When a remaining copy arm has fewer available registers, compare the actual
tail jump target. EIA6's original bypasses a common pointer reload; ours lands
on it, keeping this live. Moving that reload to the else path, while retaining
reloads after both callbacks, frees the original copy pipeline and makes the
complete790B body strict EXACT. Real operand/use and branch-target evidence
supports this graph; register-colored rejoin addresses alone do not. Review
both paths and all callback effects before transferring the lever.

Conversely, an explicit count mask can create a genuine preservation edge:
beta's mask is in initial RTL, and regmove coalesces it destructively with the
count. Publication must precede it to preserve the original full count.
Do not remove defined masking or retry source order without a proved domain
bound or independent redundant-mask computation.
[Bounded controls, full-TU audits and replay](../eia6-scheduler-results.md).

### Separate captured arithmetic from alias-visible publication (F11856)

Two writes to one field can use different source values. V17TX_control first
publishes request scale, multiplies that stored value, then delays the product
store across another configuration write. Re-reading request or publishing
the product earlier changes unknown-overlap behavior and the register graph.
Whole-owner addressing plus common return recovers most of the body; an
explicit product lifetime and its original delayed store close strict identity.
V27's independent original also needs the first store and fresh request flag
reads after the conditional member write. Transfer each observed use age,
not an assumption that owner/request pointers are disjoint.

### Cross common results with the original dispatch tree (F11857)

A shared return can still leave different state comparisons and duplicate
tails. fax_class1_status closes only when its common zero/one result is crossed
with ordinary switch groups4..6 and12..13, matching the original ordered
range tree. Keep callbacks and their effects intact. Extra current epilogues
are a screening clue, not proof of missing source statements.
[Full-TU controls, declines and validation](../callback-value-age-batch-results.md).

Historical floors need source/profile provenance too (F11855): V90Parameters
C2 was genuinely exact when recorded. A later supported float-field recovery
changed only its register choices, even though the changed writer stayed
byte-identical. Do not blame the comparison tool or compiler version without
crossed historical replays, and do not undo correct typing or lower a floor
merely to make the check green.


The constructor retype control now locates the first divergence precisely
(F11861): patterns agree through27.flow2; scratch selection differs first at
28.peephole2, and30.rnreg maps the later register colors. A prior writer can
retain its final bytes while changing SI-immediate scratch opportunities to
SF-register moves. Use pre-pass patterns and actual raw-preserving dumps;
final writer identity does not preserve compiler scratch-search state. This
corroborates the existing scratch-cursor lever, without measuring the full
historical dynamic cursor-event sequence or justifying literal permutations.
[Actual field-era stage proof](../v90-parameters-constructor-stage-proof.md).

### Separate assembly-layout clues from executed value ages (F11862–F11864)

A broad binary screen can find candidates missed by repeated-return/source-text
heuristics, but a load's position after a call in disassembly does not establish
that it executes after that call. Branches can lay out mutually exclusive arms
in either order. Bound streams by ELF symbol size, retain original operands and
actual branch destinations, resolve callees and trace paths before attributing
a displacement-count difference to stale values. Same displacement also does
not identify the same object. Known historical positives and current exact
negatives establish detector operation, not source recoverability.

Use matching full-TU controls to locate the earliest observed RTL-pattern or
instruction-order divergence. Preserve registers, modes and immediates; inspect
all constructor clones independently. Later rnreg can eliminate a peephole2
difference in one clone and retain it in another. Static immediate-store splits
are scratch opportunities, not counts of dynamic searches or cursor events.
The generic [stage diagnostic](../gcc3-stage-divergence-diagnostic.md) enforces
explicit clone selection and rejects incomplete/ambiguous paired stage sets.
Identical earlier emitted bytes remain insufficient to establish compiler-state
identity. Do not tune earlier source tokens to manufacture register colors.

A recovered pointer lifetime can preserve two stores and a second memory read
through allocation yet still lose that read in postreload (F11865). In SDM's
control, UID56 MEM:HI becomes an AX selfcopy at that pass; the baseline instead
loses its reread in first CSE and its first store by combine. Track the specific
read at each stage before attributing the final absence to source factoring or
register allocation. Compare value/address/mode knowledge and invalidations;
a widened original load is a discriminator, not proof that a cast prevents
forwarding. The installed compiler's actual replacement branch remains to be
observed. [Bounded controls and static rule](../fax-tail-timing-results.md).

F11866 now dynamically identifies SDM's decisive replacement as
`reload_cse_simplify_set`/cselib: the stored HI value keeps AX as a known location,
old-cursor copy preserves its address value, and register cost2 beats memory4.
A later operand fallback visits the resulting selfcopy; its successful return
alone is not evidence it performed the original memory forwarding. Observe the
pattern and value identities at the first changing visit. Preserve complete
raw/debugged/container object equality and distinguish an extracted compiler's
host runtime from its original container runtime. Read-only GDB observations
need no inferior calls or RTL writes; pin the actual binary for optimized-frame
and instruction-address observations. [Dynamic replay](../sdm-postreload-dynamic-trace.md).

An existing wide input lookup before a store does not prove a wide reread after
that store resists forwarding. Mode, address and available register locations
must be compared at the relevant use. Also audit prior identical siblings before
calling a source boundary new (F11867): the older compound/postincrement168B
control has a surviving extra early read and a removed late read. Equal or near
total load counts and size can therefore hide the very boundary being sought.
Keep old source families closed until independent original/use evidence changes
the discriminator; don't add casts or volatile to coerce a surviving load.

### A widening load opcode need not represent wide RTL (F11868)

GCC3's i686-tuned movhi_1 can emit MOVZWL for a plain HI memory-to-register
move to avoid partial-word stalls. SDM's saved UID118 and the installed emitter
prove that mapping; an explicit SI zero-extend UID38 emits the same opcode under
a different pattern. Consult the backend move rule and real -dP annotation
before inferring an explicit promotion or source width from MOVZWL/MOVZBL.
The memory access remains word-sized in both cases. Extra physical destination
bits written by an instruction do not establish which bits the RTL required.

This corrects the proposed SDM late-wide-versus-HI discriminator above: original
MOVZWL does not distinguish those modes. The candidate read is HI from initial
RTL, so there is also no combine narrowing to explain. Keep the valid dynamic
value-forwarding result, close the unsupported width family, and require an
independent original use/alias/profile witness for further source controls.
[Annotated and dynamic emitter proof](../sdm-movhi-emitter-width-refutation.md).

### Restoring one source boundary does not recover the complete function (F11869)

The V32 cleaned-sample getter's accepted-count-first arm restores the original
branch, but its owner reload is still missing. The V22 permutation counter's
early lifetime plus separated increment leaves a one-byte size difference
with different allocation and schedule. Neither is a byte-exact improvement.
Bound original coefficient capture independently; do not expand a near-size
result into declaration/register permutations.

A reciprocal's stack one is not unique evidence of an unsuffixed double
literal. The float RMS control restores FLD1 at the wrong lifetime and adds
an original-absent final narrowing; it also changes inlined callers. Audit
the complete expression graph and complete TU, including pooled constants.
The ordinary reciprocal-first reversal emits the retained raw object; close
it rather than retrying synonyms. [Eleven-cell audit](../small-counter-use-results.md).

An unchanged sibling can first diverge at peephole scratch selection after
matching allocation/reload. EpochDetectV29 transfers the constructor-stage
diagnostic: the new scratch and its compare differ at28peephole2, followed by
renaming at30rnreg. Keep the losing full-TU controls and trace the origin;
do not alter the sibling to restore a score (F11870).
[Quality and bystander stage evidence](../fax-quality-use-results.md).

Validate a nomination's complete operation stream before interpreting its
source controls. ResamplerTiming's 'missingFABS' premise was refuted by three
FABS in every control; an earlier comparison computes sign. Preserve invalid
premises and exclude their axes from source-family closure claims. A remaining
DF-versus-XF operand witness is independent, and its bounded negative does
not authorize a general type matrix (F11871).
[Corrected diagnostic-mode nomination](../v90-timing-diagnostic-math-domain.md).

A wrong callee-entry argument can be a source-fidelity gap even if the actual
callee ignores it on that arm. Fax command's uninitialized default `extra`
emits80 instead of the original0 outsideFTM; the existing real callee consumes
it onlyFTM, where both supply80. Distinguish measured argument reconstruction
from public behavior and synthetic replacement-callee probes. Its corrected
argument and original six-case dispatch still miss full identity; do not
claim a gain or invent an operational failure (F11872).
[Complete dispatch/result controls](../fax-service-values-results.md).

### Observe scratch history, including the actual allocation-order table (F11873)

The installed Gentoo C and C++ compilers now have raw-preserving dynamic
controls for the persistent scratch-search cursor. Model the actual candidate
order, class/mode restrictions, live/reserved bits, save/restore restriction
and frame protection; validate returned registers and cursor transitions.
All1410 observed searches agree. The53-slot order ends in five EAX entries,
explicitly zero-filled by the target backend; a uniform register rotation
model is insufficient.

An identical final writer can make a different number of searches. The
historical +0x434 retype removes one immediate-SI search, and that difference
reaches the unchanged C2 constructor. Correct float typing remains supported
independently and must stay. The fax bystander supplies a second dynamic
positive. No rejected enclosing matcher or failed scratch search occurred in
these controls: do not claim either mechanism was observed or broaden to
other modes/classes without controls. [Installed compiler proof](../gentoo-peep2-search-proof.md).

A directly observed call-argument value is recoverable separately from a
complete function's register/profile residual. Under fidelity work, the fax
default-zero correction is adopted after full-TU auditing and period388/0;
SIZE8 becomesSIZE5 but no exact function is claimed. Its actual callee ignores
the affected argument outsideTM. Keep these categories explicit and preserve
the negative dispatch/result experiments rather than presenting this as a
successful CFG or whole-function preimage (F11874).
tools/fax_extra_argument_audit.py.

### Separate direct scratch selection from allocation, then recover the counter boundary (F11875–F11876)

A register-only verdict does not identify a compiler pass. Current complete-TU
traces find zero searches in _iir_filter_create and SDMv27_init, one in
FloatFIR::reset and three in dp_v22_init. FloatFIR's selected scratch survives
renaming; the registration wrapper's final colors differ from its selected
scratch colors. Use the actual event stream and successive RTL stages before
attributing either to inherited scratch state. C++ clone declaration names
can repeat: do not equate them with emitted C1/C2 symbol ownership.
[Target boundary](../gentoo-scratch-target-results.md).

A separate zero guard plus while(count--) can leave a redundant test after
member loads even when the source guards the right boundary. Original DEC and
UINT_MAX tests at both boundaries justify consuming count in the existing
pre-load guard, then postdecrementing at the tail. This preserves the zero-input
boundary and recovers FloatFIR's original counter structure while leaving the
body's other work and seven bystanders unchanged. It produces no exact gain;
do not turn that measured component recovery into a register/profile claim or
broaden the domain to guard/variable permutations.
[Counter recovery](../floatfir-consumed-count-results.md).

### Share branch values before allocation, not only their final machine tail (F11877)

A REGALLOC residual can still have a source preimage. SDMv27_init makes no
scratch search and no rnreg changes. Two source stores compile to one final
machine tail but retain separate branch-result pseudos through local allocation;
both takeEAX before the owner pointer is assigned. The original common
load-result/store supports one conditional assignment. Its shared HI pseudo
spans the branches, changes allocation, and yields the full89-byte exact
initializer with both bystanders and all TU metadata unchanged. This is
source factoring, not a request for specific registers or a cursor edit.
Inspect24lreg/25greg and branch-result definitions when a common original
store has separate source assignments; merely reversing branch arms is a
separate closed domain. Do not assume every register-only function has this
cause or claim the conditional syntax is uniquely recovered.
[Control and audit](../sdmv27-common-value-results.md).

### Follow cold arms before inferring a common source result (F11878, F11880)

A normal-path field store does not prove both arms feed it. Receiver wrap arms
store immediate 0x4000 separately and jump after the normal store; CID's cold
arms really return to the same instruction. Use `tools/branch_store_paths.py`
with an independently identified branch/field/base to discriminate. The tool
refuses aliases/base changes and nested CFG instead of inventing a source
preimage. Positive and refusal controls are in receiver_counter_boundary_audit.
The 26-TU follow-up domain is closed in shared-value-followup-results.md.

### A local truncation boundary can introduce an extra extension (F11879)

For an unsigned-short field increment followed by a wrap check, a temporary
assignment/cast can add MOVZWL after INC under this compiler. Direct field ++
reproduces the original load/INC/word comparison/store in three receiver
methods. Check both cold and normal stores and prior alias-sensitive reads;
this is a component recovery, not a general permission to remove narrowing.
No whole-function exact gain follows here. Leave unsupported base-changing
paths unadopted; source syntax is not uniquely identified by this observation.

### Choose the active typed overlay at initialization too (F11881)

A reset of a packed decision point is not necessarily the original timing
history reset, even when both leave identical zero bytes. rxtiminginit's blob
clears existing dp.iir2.q/i separately in its timing-group order; choosing the
packed dp.point view hid that access-width evidence. Four full-TU controls
recover23 stores in offset/width/value order, but leave a2-byte size residual
and five changed nonexact bystanders. Review them all; do not claim exactness
from the recovered component or invent a new type/extent from a register base.
[Timing reset control](../v34-timing-reset-results.md).

### Cross entry captures with return ownership before blaming call layout (F11883–F11884)

An ordinary blob call versus a sibling jump can encode source result ownership.
VPcmV34GetCurrentRxBitRate needs BOTH its observed unconditional pointer
captures and a shared result to recover90B EXACT. Each isolated axis misses;
56 full-TU bystanders stay unchanged.01.rtl shows load lifetimes, but all cells
have call_placeholder;02.sibling reveals which alternative actually survives.
No flag, return-type guess or register constraint is needed. Crossed controls
on TX keep RX exact but fail TX, so this is not a universal recipe. Close that
negative domain and retain all full-TU data/exports and caller audits.
[Rate control evidence](../v34-rate-lifetime-results.md).

A real interior base does not uniquely determine a containing type or size.
VPcm's obj+4 accesses are witnessed, but full-object constructor clearing and
tagV34Object parameter mangling give no independent control extent. Preserve
named fields and existing maps until a typed-callee/extent witness exists;
do not fabricate a nested header solely to reproduce ADD4 (F11882).

### A pre-store load need not be a source capture (F11885–F11887)

V90Modem::setSessionFlag closes only with direct side dispatch crossed with a
switch whose digital arm returns and analog arm breaks. The original ordinary
analog call and digital sibling jump bound the terminal structure; GCC still
schedules the side load before storing the distinct sessionFlag member. An
explicit source capture introduces different register operands. Neither direct
access alone nor the previously tested switch alone closes the function.
Inspect 02.sibling: initial call_placeholder alternatives are not final calls.

Use small_call_result_screen to nominate equal named-transfer multisets with
an ordinary/sibling mismatch; this is a screen, not source or ABI evidence.
550 small nonexact emitted bodies yield five candidates, including three
closed allocator wrappers. Eight crossed full-TU controls preserve all data,
bindings and bystanders. DialerAbort's structured common exit compiles identically
to the early return and remains nonexact even with its unsigned guard; close
that domain. Do not change allocator prototypes or sweep return synonyms.
[Controls and audit](../small-call-result-results.md).

### Link mismatching rows to UIDs before attributing a register residual (F11888–F11891)

Residual_stage_inventory applies the canonical worst-copy rule; 97 REGALLOC/
BYTES nominations are not 97 diagnoses. Unchanged complete-TU reproduction and
-dP annotation cardinality/opcode checks let residual_register_uid_trace link
25/31 REGALLOC targets to RTL patterns. Repeated clone headers and unavailable
annotation/stage windows stay ambiguous or unavailable, never guessed or zero.
A changed peephole UID need not consume scratch: _iir_filter_create's zero
searches coexist with eight transformed UIDs. Inspect actual events before
attributing a row to the persistent cursor; two scored Scrambler process bodies
have no direct search. [Baseline and UID evidence](../residual-stage-family-results.md).

A failed scratch search can reset the persistent cursor even when its enclosing
peephole emits no replacement. The new V92 setEchoDelay and v8_process controls
observe three distinct failed sites (five attempts including a repeat), setting
cursor zero; subsequent searches confirm the state. Count rejected attempts,
not only surviving instructions or accepted replacements. Seven controls validate
382 searches/3784 visits; V90MP's QI/q events are explicitly outside the current
HI/SI r model. This observes current GCC behavior, not original cursor values,
and does not authorize tuning source history to manufacture a register color.
