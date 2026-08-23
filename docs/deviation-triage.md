# Triage of the 🐛 entries in `docs/deviations.md`

`docs/deviations.md` carries 351 entries, **232 of them marked 🐛 — "defect in
the original"**. Five files in `src/` carry a `DSPLIB_REPRODUCE_BUGS` arm.
Between those two numbers is this document's reason to exist: the overwhelming
majority of the things this tree has called a bug have never been
**dispositioned**. Nobody decided whether they are real, whether anything can
reach them, or whether anything should be done.

That gap is the risk, and it cuts both ways. A 🐛 that is correct behaviour
misread is a lie in the record, and the record is this project's deliverable.
A 🐛 that is real and reachable from the wire is a defect that ships in
whatever links the blob today.

**This document decides nothing about `src/`.** It changes no code and
proposes no edit to a differential test. Where it says a fix is warranted it
NAMES the fix and stops, because the rule in `CLAUDE.md` and in the register's
own preamble is that a defect recorded there is not quietly corrected here.

---

## The denominator, before anything else

A detector must report its denominator (`CLAUDE.md`, findings 134, 2400,
3100), and so must a triage.

| | count |
|---|---|
| entries in `docs/deviations.md` | 351 |
| marked 🐛 | **232** |
| of those, examined here | *(see the closing census)* |
| decided here | *(see the closing census)* |
| left undecided, and named | *(see the closing census)* |

### 232 and 230 are both right, and that is itself a defect in the record

Every mechanical census of this file returns **230**, not 232, and the two
missing entries are real: `D-V92DEC-1` and `D-V92DEC-2`. They are the only
entries whose heading does not match `^## D\d+`, which is the pattern
`tools/refcheck.py` uses (`DEV_HEAD`) to learn what deviations exist.

The consequence is not cosmetic. `refcheck.py` cannot see those two headings,
so it cannot report a citation of them as dangling, and it cannot report one
that drifts. They are outside the register's own gate. **Renumbering them into
the `D\d+` scheme is the fix**, and it is a documentation change, not a source
change — but it belongs to whoever owns the numbering, not to this pass, and
it must be done with `refcheck.py --since` across every branch.

### What was already dispositioned, and on which axis

The register is not undocumented — it is documented on axes that do not answer
this question.

| where | scope | the question it answers | 🐛 entries carrying a grade |
|---|---|---|---|
| Appendix A | D1–D64 | is the claim MEASURED, or is it an assertion? | 33 measured, 16 unmeasurable, 8 drivable, 7 retracted |
| Appendix B | D70–D161 | can anyone HIT it? | 14 fires today, 26 needs a caller, 31 latent, 21 cannot fire |
| Appendix C | selected | would fixing it improve CONNECT or RATE? | 8 ranked |

Counting only the explicit classification lists — the `> D73 D74 …`
blockquotes, Appendix A's table rows and its retraction list — **117 of the
232 🐛 entries carry an explicit disposition on some axis, and 115 carry
none.**

*(Do not compute this by searching the appendices for `D<n>` mentions. Entry
bodies continue AFTER Appendix C's heading — D780 and D790 sit past line 4743
— so a prose-mention count silently scores those bodies' internal citations as
appendix coverage and reports 13 uncovered instead of 115. That mistake was
made and caught during this pass.)*

**And neither existing axis asks whether the entry is a defect at all.**
Appendix A asks whether a claim is driven; a driven claim can still be a
misreading. Appendix B asks whether a mechanism is reachable; an unreachable
mechanism can still be correct behaviour. The register has no column for "we
looked again and the object is right".

### Provenance: where the 🐛 marks came from

| origin | 🐛 entries |
|---|---|
| task #98 sweep of `docs/findings.md` (Part II) | 92 (D70–D161) |
| task #87, #99 and named others | 4 |
| no task named — D1–D64 and everything from D162 up | 136 |

The 92 matter as a block. Part II states its own method: it swept **every line
of `docs/findings.md`**, 37,637 lines, and turned prose into register entries.
That is a legitimate way to find defects and a **second-hand** way to
establish one. On `CLAUDE.md`'s evidence ordering it is at best tier 3, usage
inference, and one remove further out — inference from someone else's writing
about the object rather than from the object. Nothing in that block was
re-read against the blob when it was written, and its entries say so by citing
a finding rather than an address.

### Citations

- **180 of 232** 🐛 entries cite at least one `finding N`; **52 cite none** and
  rest entirely on their own account of the object.
- **Every citation resolves.** `python3 tools/refcheck.py` reports 6,519
  references checked, 0 dangling, 0 pending, 0 stale, exit 0 — the baseline
  taken before this document was written, so a clean run afterwards means
  something.
- Resolving is not agreeing. `refcheck.py`'s own docstring says it cannot
  catch a reference that still resolves but now points at the wrong finding,
  and that is the failure mode this register is most exposed to. Every triaged
  entry below therefore carries a **citation check** — AGREES, DRIFTED or
  DANGLING — done by reading the cited finding, not by running the tool.

### Reach into `src/`

**105 of the 232** are named somewhere under `src/`, `include/` or `test/`.
The five files carrying a `DSPLIB_REPRODUCE_BUGS` arm account for a
recognisable slice of them:

| file | deviations named |
|---|---|
| `src/dsp/fpm_div.c` | D1 D4 |
| `src/pump/v34/v34filters.c` | D26 D27 D28 D29 D30 D31 D32 |
| `src/pump/v34/v34hshak.c` | D35 D36 D37 D43 D51 D52 D53 D54 D59 |
| `src/pump/v32/v32fse.c` | D301 D302 D370 D371 D451 |
| `src/pump/v90/V92CP.cpp` | D503 D520 D570 D571 D920 D921 D923 |

An entry named beside a `#ifdef DSPLIB_REPRODUCE_BUGS` is already
dispositioned by construction: someone decided it was real, decided a fix, and
gated it. Those are marked ALREADY DISPOSITIONED below rather than re-argued.

---

## Method

The standard is **D920 and D923**, and it was set by the work that wrote them:
read the blob's own instructions, then read the ITU-T text, then name which
SIDE the defect is on. D923 is the worked example of the verdict this document
most often reaches — the object stores into `bits[word_11c]` with no guard at
seven sites, and ITU-T V.90 Table 14 / V.92 Table 23 bound each four-bit
constellation index to "an integer between 0 and 5", so a legal message tops
out near 1,786 of 2,000 entries and **the overflow needs a malformed peer, not
a conformant one**. Real defect; unreachable from the wire; documented, not
guarded.

Triage ran **by mechanism family, not by entry number.** Within a family the
discriminating question is shared, so it is argued once and applied N times —
which is what makes a carefully argued 60 possible where 232 one-line guesses
would not be worth writing. The families and their shared questions are the
section headings below.

**Every row states its evidence tier**, per `CLAUDE.md`:

1. a format string the original author wrote — the strongest thing in the
   object, because it is the author's own words;
2. a callee or caller that types the thing;
3. usage inference — the weakest, and said so when it is all there was.

**Every row states a test**: the concrete observation that would confirm or
refute the verdict. A disposition without one is an opinion.

### The four dispositions

- **NOT A DEFECT** — the reading was wrong, or the behaviour is required. The
  row says what the misreading was. This is the most valuable verdict in the
  document and the one held to the highest standard.
- **DEFECT, UNREACHABLE** — real, but no legal input reaches it. The row says
  what bounds the input, and where the bound is the recommendation it quotes
  the clause.
- **DEFECT, REACHABLE, FIX WARRANTED** — needs a `DSPLIB_REPRODUCE_BUGS` arm
  or a host-side clamp. The row names the fix and does not write it.
- **UNDECIDABLE FROM HERE** — the row says precisely what evidence would
  decide it, so the next person does not repeat the work.

### What this pass did NOT do

- It ran no build. `make phase` was deliberately not run: this changes no
  source, two other agents were building concurrently, and the check that
  matters for a documentation change is `python3 tools/refcheck.py`.
- It edited no entry in `docs/deviations.md` except where a verdict line is
  provably wrong. Appendix A's own header records why (finding 622): the file
  is shared, and a sweep of in-place edits conflicts where an append does not.
- It did not re-derive D1–D64. Those are the oldest and best-argued entries in
  the file, most are driven by a named test, and Appendix A already audits
  them on the axis that was missing.

---

# Family 1 — dead stores and redundant work

**Entries: D190, D195, D196, D294, D296.** Triaged in this session, from the
object.

**The shared question.** Does anything read the value, or observe the
redundancy? A store no path reads, a memset that clears bytes already zero, or
a reload the compiler was obliged to emit are all *surprising* — which is why
they were written down — but the register's 🐛 means "defect in the original",
and code with no observable consequence is not a defect. It is usually the
compiler being correct.

**The verdict for the family is NOT A DEFECT, five for five**, and each row
says what the misreading was.

### D196 — `VPcmV34Create` re-loads `sess + 0x612c` between two byte stores

*Claim: the two stores "are not guaranteed to reach the same record".*

**Mechanism.** The two loads are at `.text+0xabb6` and `+0xabca`, and
everything between them is visible:

    abb6:  mov    0x612c(%ecx),%eax
    abbc:  shr    $0x3,%edx
    abbf:  mov    %dx,0xabfc(%ebx)
    abc6:  movb   $0x0,0x10(%eax)
    abca:  mov    0x612c(%ecx),%edx
    abd2:  movb   $0x0,0x11(%edx)

**The misreading.** The reload is *aliasing*, not a hazard. GCC could not
prove that `movb $0x0,0x10(%eax)` — a byte store, which under C's aliasing
rules may touch anything — does not overwrite `0x612c(%ecx)`, so it reloaded.
For the two stores to reach different records, one of the two intervening
stores would have to overwrite the pointer itself: `%ebx`-based at `+0xabfc`,
which is a different object, or `%eax + 0x10`, which would require the record
to contain the pointer to itself at offset 0x10. Neither is possible, and the
program is single-threaded. **The two stores necessarily reach the same
record.**

- **Evidence tier 3** (usage inference over the object's own instructions);
  the instruction sequence is direct, the aliasing argument is inference.
- **Verdict: NOT A DEFECT.** Keeping both loads in `src/` remains right — it
  is what `make similarity` compares — but the 🐛 mark is wrong.
- **Test.** Fold the two loads into one in the reconstruction and run the
  differential tier: it will pass, because no input can separate them. That is
  the mutation-survivor shape, and it is the same instrument D195 already has.
- **Citation:** finding 1260. AGREES — 1260 records the reload from the
  disassembly and does not itself claim a hazard.

### D195 — the second `sysdep_memset` clears what the first already cleared

**Already proven unobservable, by the strongest instrument in the tree.**
`test/mutations/vpcmcreate.json` carries `"the redundant second memset is
dropped"` as a **deliberate recorded survivor**, and its header states the
count: *"31 entries: 29 caught, 1 equivalent and recorded as such, 1
survivor"*. A surviving mutant is a proof of unobservability — the suite
cannot distinguish the object's behaviour with the memset from its behaviour
without it, because there is nothing to distinguish.

- **Evidence tier 2** (a recorded mutation result, i.e. the suite typing the
  behaviour).
- **Verdict: NOT A DEFECT.** Redundant, not wrong.
- **Test.** Already run and recorded; re-run `tools/mutate.py` over
  `vpcmcreate` and the survivor must still survive. If it were ever *caught*,
  that would mean the two memsets are not redundant and this verdict is wrong.
- **Citation:** finding 1260. AGREES.

### D190 — `V90Phase3Modulator`'s constructor stores a pointer nothing reads

**Mechanism.** Finding 1257 disassembled all nineteen `V90Phase3Modulator`
text symbols and searched every `0x50` displacement: three hit, and only the
two constructor copies are the object at all — `reset`'s `mov 0x50(%esp),%ebp`
is a stack slot. So the field is written and never read *within the class*.

- **Evidence tier 2** (the class's own symbols type the access), and the
  entry's own status line is honest that what reads `+0x50` from OUTSIDE the
  class was not looked for.
- **Verdict: NOT A DEFECT** as recorded — a write-only member is a dead store,
  and the entry's own comparison to the V.92 sibling is a note about
  divergence between two classes, not a defect in either.
- **Test.** `tools/relocscan.py` and a displacement sweep over every symbol
  that can hold a `V90Phase3Modulator *` — if an outside reader of `+0x50`
  exists, this becomes a live field and the entry becomes a *naming* question,
  not a defect one.
- **Note on the record:** this entry was renumbered from the 162 slot on the
  coordinator's assignment and says so, which is exactly what `CLAUDE.md`
  requires of a renumber. Good practice; leave it.
- **Citation:** finding 1257. AGREES.

### D294 — the study's states are numbered out of the order it runs them in

**Two claims, and neither is behavioural.** A state *number* is a label; the
dispatch is through the jump table at `.rodata+0xd70`, and the entry's own
text says "the chain is measured from the arms and not from the numbers" —
i.e. the code does the right thing and only the labelling is surprising. The
second half, `int_a9a0` being "read by nobody", is a dead store: the entry
reports it is named in no displacement of any of the class's thirty-two
members.

- **Evidence tier 3** (usage inference).
- **Verdict: NOT A DEFECT**, both halves. Out-of-order labels change nothing a
  caller sees, and a write-only field changes nothing at all.
- **Test.** Renumber the states in the reconstruction to run order and the
  differential tier must stay green; if it does not, the numbers are load
  bearing and this verdict is wrong.
- **Citation:** finding 1441. AGREES.

### D296 — the retrain detector stores three accumulators it is about to clear

**Mechanism.** `VPcmV34Progress` stores at `+0x30b`, `+0x311` and `+0x314`
into `+0xac30`, `+0xac34` and `+0xac38`, and writes zero into all three
unconditionally eight instructions later at `+0x349`, `+0x350` and `+0x357`,
on every path out of the block.

**The entry's own text already contains the verdict**: "Nothing reads any of
the three between the store and the clear, and the compiler kept them because
they are member stores through a live pointer."

- **Evidence tier 3**, and the entry graded itself **FIRES** — which is true of
  the *store* and irrelevant, because what fires is a write nobody reads.
  **This is the clearest case in the register of the reachability axis being
  read as a severity axis.** A dead store that executes on every call is still
  a dead store.
- **Verdict: NOT A DEFECT.**
- **Test.** Delete the three stores in the reconstruction and run the
  differential tier plus `make similarity`: the first must stay green (nothing
  observes them) and the second must move (the object contains them). That
  pair of outcomes is what distinguishes "dead" from "wrong".
- **Citation:** finding 1464. AGREES.

### What this family costs the register

Five entries carry a 🐛 that means "defect in the original" and describe code
with no observable consequence. That is 2% of the 232, found in the first
family looked at, and the shape is common enough — a reload, a redundant
clear, a write-only field, a label out of order — that a sweep for it over the
entries this pass did not reach would likely find more. **The
recommendation is not to delete these entries.** They are worth recording; the
`🐛` is what is wrong, and `⚠`, or a new neutral mark, is what they should
carry.
