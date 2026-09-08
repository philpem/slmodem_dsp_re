# Triage of the 🐛 entries in `docs/deviations.md`

The original survey counted 351 entries in `docs/deviations.md`, **232 of
them marked 🐛 — "defect in the original"**. These are historical cohort
counts, not a fresh register census. Five files in `src/` carried a
`DSPLIB_REPRODUCE_BUGS` arm.
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

The tables retain the original 232-entry cohort. The 2026-09-08 standards
audit moves D162 from undecidable to NOT A DEFECT: decided 97 to 98,
undecidable 21 to 20, and NOT A DEFECT 17 to 18. D271 is also retracted in
the register but was not a bug-marked member of this cohort, so it changes
none of these counts. Other later register changes are not a new census here.

A detector must report its denominator (`CLAUDE.md`, findings F134, F2400,
F3100), and so must a triage.

| | count |
|---|---|
| entries at the original register census | 351 |
| marked 🐛 at that census | **232** |
| of those, **examined** here | **118** |
| **decided** here | **98** |
| left **undecidable**, each with the deciding evidence named | **20** |
| not reached, and named in full at the end | **114** |

Of the 118 examined, **21 turned out not to be defects of the object** — 18
NOT A DEFECT and 3 misfiled. **About one in six.** The full breakdown is in the
closing census; it is repeated here because it is the headline: *the 🐛 count
is not a defect count, and the gap is large enough to matter.*

### 232 and 230 are both right, and that is itself a defect in the record

A census keyed on `^## D\d+` returns **230**, not 232, and the two missing
entries are real: `D-V92DEC-1` and `D-V92DEC-2`. They are the only entries
whose heading does not match that pattern — which is the pattern
`tools/refcheck.py` uses (`DEV_HEAD`) to learn what deviations exist. Widening
the key to `^## D[\w-]+` recovers both, and every count in this document is
computed with the wider key.

**The consequence is measured, not inferred, because the inferred version was
wrong.** The hazard is not that a citation of them dangles — `DEV_REF` is
`\bD(\d+[a-z]?)\b`, and the string `D-V92DEC-1` contains no `D` followed by a
digit, so a citation of them matches nothing and cannot be reported either
way. The hazard is that **such citations exist and are entirely invisible to
the gate**. Grepping the tree for `D-V92DEC` outside `docs/deviations.md`
finds four:

    src/pump/v90/V90Phase3Demodulator.cpp:1538   docs/deviations.md, D-V92DEC-1
    test/unit/t_v92dec.cpp:54                    docs/deviations.md D-V92DEC-1
    docs/findings.md:53704                       docs/deviations.md D-V92DEC-1
    docs/findings.md:53883                       (D-V92DEC-2)

plus one inside the register itself. Five live cross-references — one of them
from `src/`, one from a test — that `refcheck.py` counts as zero. If either
entry is renumbered or retitled, nothing fails. **Renumbering them into the
`D\d+` scheme is the fix**, and it is a documentation change, not a source
change — but it belongs to whoever owns the numbering, not to this pass, and
the five sites above must move with it.

### A second gap in the gate, found while using it

**`tools/refcheck.py` only scans TRACKED files.** Writing this document and
running the tool reported 6,519 references — exactly the baseline, unchanged —
because the new file was untracked. `git add` alone took it to 6,571. Nothing
warned. A new document full of `D<n>` and `finding <n>` citations can
therefore be written, checked, and reported clean while the checker has not
read a line of it: the dead-detector shape this tree has hit four times
(findings F134, F2400, F3100, F3055). The habit that defends against it is the one
`CLAUDE.md` already states — **read the denominator, not the exit code**. It
moved, so the tool was working.

### What was already dispositioned, and on which axis

The register is not undocumented — it is documented on axes that do not answer
this question.

| where | scope | the question it answers | 🐛 entries carrying a grade |
|---|---|---|---|
| Appendix A | D1–D64 | is the claim MEASURED, or is it an assertion? | 33 measured, 16 unmeasurable, 8 drivable, 7 retracted |
| Appendix B | D70–D161 | can anyone HIT it? | 14 fires today, 26 needs a caller, 31 latent, 21 cannot fire |
| Appendix C | selected | would fixing it improve CONNECT or RATE? | 8 ranked |

**121 of the 232 🐛 entries carry an appendix grade.** That counts Appendix
B's four blockquotes and the whole of Appendix A's scope: the appendix's own
arithmetic is 33 + 16 + 8 + 7 = 64 with "each entry in exactly one row", so
every D1–D64 is graded there whether the row is a blockquote or a sentence.

**The remaining 111 are not undispositioned, and establishing that is the
first useful thing this pass did.** The entries written from D163 up appear in
no appendix, but most carry their own four-field preamble in italics:
*Reachability*, *Observability*, *Status* and *Fix class*. That is a
disposition, written by the batch that found the defect, and a mechanical
count finds it:

| | 🐛 entries |
|---|---|
| appendix grade AND a full inline preamble | 87 |
| appendix grade only | 34 |
| inline preamble only (no appendix grade) | 90 |
| **neither — a bare entry** | **21** |

So **177 of 232 carry a reachability judgement and a fix class somewhere**,
and only **21 carry neither**. The register is in far better shape than a
naive count suggests, and a plan built on "115 undispositioned" would be
planning work that is already done. The 21 bare entries are:

> D65 D162 D176 D178 D179 D190 D275 D276 D277 D304 D306 D321 D470 D471
> D710 D901 D920 D923 D930 D-V92DEC-1 D-V92DEC-2

D920 and D923 are on that list only because their dispositions are written as
prose rather than as a labelled preamble; they are the two best-argued entries
in the file. The other nineteen are genuinely bare.

*(Three counting traps were hit here and are worth passing on, because each
one produced a confident wrong number. First: do not compute appendix coverage
by searching the appendices for `D<n>` mentions — entry bodies continue AFTER
Appendix C's heading, D780 and D790 sit past line 4743, so a prose-mention
count scores those bodies' internal citations as appendix coverage and reports
13 uncovered against a true 111. Second: do not stop at the appendix lists
either, or you report 115 bare entries against a true 21. Third: do not read
Appendix A's membership out of its blockquote alone — its UNMEASURABLE and
RETRACTED rows are prose, and taking only the blockquote drops sixteen graded
entries and puts D6, D9, D27 and D33 on the bare list, where they do not
belong. All three mistakes were made during this pass and caught by measuring
rather than by reading.)*

Appendix B itself was audited and **is internally consistent**: 14 + 26 + 31 +
21 = 92 listed, 92 distinct, every entry D70–D161 graded exactly once and
nothing graded from outside that range.

**And no existing axis asks whether the entry is a defect at all.** Appendix A
asks whether a claim is driven; a driven claim can still be a misreading.
Appendix B and the inline preambles ask whether a mechanism is reachable; an
unreachable mechanism can still be correct behaviour, and a *reachable* one
can still be harmless. `Status:` records whether the claim was demonstrated,
not whether what was demonstrated is a fault. **The register has no column for
"we looked again and the object is right", and that is the column this
document adds.**

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

### And a fifth, which the evidence forced

The four above were the shape this triage set out with. Family 9 does not fit
them, and forcing it would have falsified the result, so there is a fifth:

- **DEFECT, REACHABLE — NO FIX WARRANTED.** Real, reachable, faithfully
  reproduced, and **the right answer is not recoverable.**

It is not NOT A DEFECT: the object genuinely does the wrong thing. It is not
UNREACHABLE: ordinary input reaches it. And it is not FIX WARRANTED, because
**there is nothing to put in the fix.** D451 is the clean case — the metric is
never scaled and the decision collapses over the whole domain, but the shift
the author meant is not recoverable, the family using 16, 15/16, 13 and none.
Inventing one would make a defect look like an implementation, which is the
worst outcome available to a record whose only value is its accuracy.

**The discriminating test is whether the correct value can be derived
independently** — and it is the test D302 passed, D451 failed, and D250 passes
four times over. That test, not severity, is what separates a defect worth
fixing from one worth only recording. **This is the most useful thing this
document learned, and the register should adopt the category:** fourteen of
its entries currently carry a 🐛 and an implied to-do that nobody should ever
do.

### What this pass did NOT do

- It ran no build. `make phase` was deliberately not run: this changes no
  source, two other agents were building concurrently, and the check that
  matters for a documentation change is `python3 tools/refcheck.py`.
- It edited no entry in `docs/deviations.md` except where a verdict line is
  provably wrong. Appendix A's own header records why (finding F622): the file
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
- **Citation:** finding F1260. AGREES — 1260 records the reload from the
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
- **Citation:** finding F1260. AGREES.

### D190 — `V90Phase3Modulator`'s constructor stores a pointer nothing reads

**Mechanism.** Finding F1257 disassembled all nineteen `V90Phase3Modulator`
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
- **Citation:** finding F1257. AGREES.

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
- **Citation:** finding F1441. AGREES.

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
- **Citation:** finding F1464. AGREES.

### What this family costs the register

Five entries carry a 🐛 that means "defect in the original" and describe code
with no observable consequence. That is 2% of the 232, found in the first
family looked at, and the shape is common enough — a reload, a redundant
clear, a write-only field, a label out of order — that a sweep for it over the
entries this pass did not reach would likely find more. **The
recommendation is not to delete these entries.** They are worth recording; the
`🐛` is what is wrong, and `⚠`, or a new neutral mark, is what they should
carry.

---

# Family 2 — defects in diagnostics, and the gate that does not hold

**Entries: D163, D267, D291.** Triaged in this session, from the object and
from the host. **This family's shared argument overturns a reachability line
that several entries state about themselves**, so read the argument before the
rows.

## The shared question, and the answer is not the one the entries assume

Every diagnostic call site in the object is gated on `dsplibs_debug_level`.
`include/dsplib/debug.h` states the consequence plainly: *"every use is gated
on `dsplibs_debug_level`, and slmodemd ships with that at zero — so on a
working modem none of the call sites do anything at all."* Several entries
repeat it: D163's own reachability line reads *"diagnostic only, and only
above `dsplibs_debug_level > 1`"*.

**Measured against the object rather than believed** (`CLAUDE.md`: when a
paragraph states a count, check it against the tool):

| | count in `ref/slmodemd/dsplibs.o` |
|---|---|
| relocations naming `dsplibs_debug_printf` | 1,670 |
| relocations naming `dsplibs_debug_level` | 1,733 |
| `cmpl $0x1,<abs32>` in `.text` | 1,328 |
| `cmpl $0x2,<abs32>` in `.text` | 21 |

There are more references to the level than calls to the printf, so "every
call site is gated" is at least consistent with the object at this
granularity. A per-site proof was not done here, and is what would make it
certain.

**But the gate is opened by a documented command-line flag, and that is what
nobody had checked.** In the host, `slmodemd/modem_debug.c`:

    60: unsigned int modem_debug_level=0;
    61: unsigned int modem_debug_logging=0;
    62: unsigned int dsplibs_debug_level=0;
   ...
   139:         dsplibs_debug_level = modem_debug_level;
   140:         if(modem_debug_logging) {
   ...
   156:                 if(dsplibs_debug_level < 3)
   157:                         dsplibs_debug_level = 3;

and in `slmodemd/modem_cmdline.c`:

    127: {'l',"log","logging mode",OPTIONAL,INTEGER,"5"},
   ...
    281: if(opt_list[OPT_LOG].found) {
    282:         modem_debug_logging = 5;

So `slmodemd --log` — an option that appears in the program's own `--help`
output as "logging mode" — sets `modem_debug_logging` to 5, and
`modem_debug_init` then **forces `dsplibs_debug_level` to 3**, which is above
BOTH of the object's gate thresholds: the 1,328 sites gated at 1 and the 21
gated at 2. `--debug=2` reaches the first set by itself.

**Conclusion for the family: the default is zero and the gate is one flag
deep.** "Only above `dsplibs_debug_level > 1`" is true and is not a bound;
Appendix B's own wording for FIRES TODAY is "an ordinary call **or a shipped
configuration**", and this is a shipped configuration. Every diagnostic defect
in the register is therefore reachable, and what limits them is **severity,
not reachability**: they corrupt a transcript, not a call.

- **Evidence tier 1 throughout** — these entries rest on the format strings
  themselves, which are the original author's own words and the strongest
  evidence this project recognises. That is unusual in the register and worth
  saying: this small family has the best evidence in it.

### D163 — the CP class's debug line names the other class

`V90CP::printNofRecievedMpMpNot` prints `"V90MP: received %d MP, %d MPNot"`.
The entry establishes the literal at `.rodata.str1.4+0xd6b0` is byte for byte
`V90MP`'s at `+0x5a34`, so it is a copied line, not a coincidence.

- **Verdict: DEFECT, REACHABLE — documentation only.** Real (the label is
  wrong and the author's own text proves what it should say), reachable under
  `--log`, and its entire consequence is that a log line attributes a count to
  the wrong class. **No fix warranted in `src/`**: correcting it would change
  the transcript the differential tier compares, for no gain to anyone but a
  log reader, and the register's rule forbids it.
- **Test.** Run the transcript comparison at a debug level of 2 or more and
  confirm both sides emit the `V90MP:` prefix from the CP class. That is
  already how the class's other diagnostics are compared.
- **Citation:** finding F1239. AGREES.

### D267 — `evaluatePhase4` prints a `%d` with no argument behind it

**The most serious entry in this family, and it is undefined behaviour.** The
call site at `.text+0x3ffcf` pushes only the format pointer and stores nothing
to `0x4(%esp)`; `edprintf` formats with `vsnprintf`, which reads that slot as
an `int`. The sibling call forty-four bytes later at `0x3fffb` stores
`nofV90Retrains` there for a string with the same conversion — so this is one
line written without its argument, not a different convention. That comparison
is what makes the entry sound.

- **Verdict: DEFECT, REACHABLE — and correctly NOT FIXED.** Reachable on every
  delayed retrain past `MAX_NOF_V90_RETRAINS` once `--log` is passed. The
  consequence is a garbage integer in one log line.
- **This entry is also the register's best worked example of a defect the
  reconstruction cannot reproduce.** Both sides run the same code and read
  their own frame, and the frames differ because the compilers differ, so
  `test/unit/t_v90conneval.cpp` compares object state, verdict and line count
  on that path instead of the transcript text. The entry records that the
  attempt to make the slot deterministic was made and measured and failed.
  That is the right handling and it should not be revisited.
- **Test.** Already built: `t_v90conneval` compares everything except the one
  line. If a future change makes that line comparable, the verdict is wrong
  and the frame was deterministic after all.
- **Citation:** finding F1388. AGREES.

### D291 — `studyUrefHandler` prints a float through `%d`, twice

`fsts 0xa964(%ebx)` at `+0xb1f` stores the field as a `float`; `fstpl
0x4(%esp)` at `+0x12fe` pushes the same quantity as a `double` under the
format `"first update : trn1Sigma = %d"`. So the field and the report get two
different roundings of one value, and the report's is then read as an integer.

- **Verdict: DEFECT, REACHABLE — documentation only.** Real, tier 1 (the
  format string is the evidence), and confined to the transcript. The entry
  grades itself FIRES "with the debug level above 1", which this family's
  argument confirms is one flag away rather than a bound.
- **Test.** At a debug level of 2 or more, drive state 3 and state 5 to
  completion and compare the two lines: they must be byte-identical between
  the blob and the reconstruction, because unlike D267 the argument IS pushed
  and the value IS deterministic.
- **Citation:** finding F1443. AGREES.

## What this family changes about the register

Nothing in `src/`, and one thing in the record: **"only above
`dsplibs_debug_level > 1`" is not a reachability bound and should stop being
written as one.** It is one documented flag from a user's shell. The correct
statement for these entries is that they are reachable and cosmetic, which is
a different and more honest claim than unreachable.

---

# Family 3 — the bare entries, triaged one at a time

**Entries: D162, D176, D178, D179, D275, D304, D306, D710, D901.** These are
nine of the twenty-one entries carrying neither an appendix grade nor a full
inline preamble, so nothing at all had been decided about them. They do not
share a mechanism; what they share is that nobody had looked. Each is argued
on its own below.

## D176 — `V90PreFilter`'s constructor "can never select the last entry of `dataBase`"

# **NOT A DEFECT. The object is right and the entry is a misreading.**

This is the most consequential single result in this document, so it is
argued from the object rather than from our source.

**What the entry claims.** "The table is walked to its first empty name and
the count is decremented before `codecType` is compared against it, so the
highest index the constructor will accept is `count - 2`, and an index of
`count - 1` — a real, named entry — is rejected."

**What the object does**, at `.text+0x44dd3` in
`_ZN12V90PreFilterC2E23__tHardwareCodecTypes__P13V90Phase2InfoP13V90Parameters`:

    44dd0:  add    $0x24,%eax          ; stride one V90CodecEntry
    44dd3:  inc    %ebx                ; count the named entries
    44dd4:  cmpb   $0x0,(%eax)          ; ...until name[0] == 0
    44dd7:  jne    44dd0
    44dd9:  dec    %ebx                ; ebx = N - 1
    44dda:  cmp    %ebx,0x14(%esi)      ; codecType against it
    44ddd:  jle    44def                ; <= N-1 is ACCEPTED

**The `dec` is not an off-by-one; it converts a COUNT into a MAXIMUM INDEX,
and the comparison that consumes it is `jle`, not `jl`.** The loop leaves
`%ebx` at N, the number of named entries; `dec` makes it N − 1, the highest
valid index; `jle` accepts everything up to and including it.

**And the table settles what N is — read out of the blob, not out of our
source.** `nm -S` gives `_ZN12V90PreFilter8dataBaseE` in `.data` at `0x6760`,
**size 612**, and the entry stride the constructor uses is `add $0x24,%eax` =
36 bytes. 612 / 36 = **17 entries exactly**, and dumping them from the
object's own `.data`:

    0 Unknown            4 USB_STLC_7550    8 ALS300_WOLFSON   12 Panther_AD1803
    1 AD1821             5 ALS300_AD1819    9 AMR_SILABS       13 Squeezer_545A_ALC
    2 Lucent             6 ALS300_AKM4542  10 SIL3052_INTERNAL 14 Raptor_SL2800
    3 Siemens            7 ALS300_ICE      11 CodecType_SIL3054 15 Squeezer_545A_ITE
                                                               16 ""  (name[0] == 0x00)

Sixteen named entries at indices 0–15 and the empty terminator at 16. So the
loop stops at 16, `dec` gives 15, and `codecType == 15` —
`"Squeezer_545A_ITE"`, the last real entry — is accepted. **No named entry is
unreachable.**

*(This was checked against the object deliberately. The first draft of this
argument took the table from `src/pump/v90/V90PreFilter_loops.cpp`, and a
reconstruction can carry a table length as an assumption that no differential
test would catch — the argument is only worth its evidence tier if the count
comes from the blob.)*

**What the entry saw, and it is real but is somewhere else.** The diagnostic
prints the decremented value under the label "table length":

    "External Hardware Codec Index exceeds table length
     (codec inx = %d, table length = %d)"

and passes `n`, which is 15, where the table holds 16 named entries. So the
MESSAGE is off by one; the BOUND is exact. That is a tier-1 observation — the
author's own format string is the evidence — and it belongs to Family 2: a
cosmetic defect in a diagnostic, gated behind `dsplibs_debug_level`, which
`--log` opens.

- **Evidence tier 2** for the verdict (the object's own instructions and the
  object's own table), tier 1 for the residual message defect.
- **Verdict: NOT A DEFECT** as written. The 🐛 should be withdrawn and
  replaced by the mislabelled-diagnostic claim, which is true.
- **Test.** Construct a `V90PreFilter` with `codecType == 15` and confirm both
  sides keep it rather than resetting it to 0, and that the transcript emits
  `"HardwareCodecType: Squeezer_545A_ITE"` rather than the "exceeds table
  length" line. If the object resets it, this verdict is wrong.
- **Citation: finding F1233 — AGREES, AND THE FINDING IS WRONG TOO.** This is
  the outcome that matters and it is not the one that was expected. The
  finding's own heading is *"V90PREFILTER'S CONSTRUCTOR: THE REGISTRY DECIDES,
  THE ARGUMENT IS ONLY THE FALLBACK, AND THE RANGE CHECK IS ONE SHORT"*, and
  its step 4 reads: *"The check in step 4 is one short. `dataBase` is walked to
  its first empty name and the count is DECREMENTED before the comparison, so
  the last named entry of the table is unreachable through the argument path."*
  So D176 reports its source faithfully — the citation is sound — and **the
  error originates in the finding**. Correcting the register alone would leave
  the wrong claim standing in `docs/findings.md`, where D178 also cites it and
  where the next reader will find it. The finding needs the correction more
  than the deviation does.
- **What the finding gets right, and it is most of it.** The sixteen-arm switch
  is a switch and not a range test, and the jump table at `.rodata+0xd8c` is
  the evidence; the diagnostics are plain rather than `edprintf`-encoded, so
  they do not move that class's rotating key. Only step 4's sentence is wrong.
- **Note.** The configuration path cannot produce an out-of-range value
  either: `cmp $0xf,%eax; jbe` at `+0x44db6` gates a sixteen-way jump table at
  `.rodata+0xdcc`, and everything else falls to `codecType = 0`.

## D178 — the same constructor bounds `codecType` from above and not from below

**DEFECT, REACHABLE ONLY FROM A CALLER NOTHING VALIDATES.** Real, and D176's
collapse does not touch it: the two claims are about opposite ends of the
range and only one of them was wrong.

The object's single range test is the `jle` above — an upper bound and nothing
else. The configuration path is bounded below as well, because its jump table
maps only 0 to 15 and everything else to 0. **The argument path is not**: when
the parameter word at `+0x008` is negative, meaning "not configured", the
constructor stores the caller's `__tHardwareCodecTypes__` straight into
`codecType`, and a negative argument arrives intact.

What makes it serious rather than theoretical is what happens next, and the
entry has this right: `isV90WithEia6`, `autoSelection` and `selectFilter` all
index `dataBase[codecType]` with no gate of any kind, and **those reads are
not behind the debug level**. The constructor's own use of the index is —
`dataBase[codecType].name` sits inside `DSPLIB_DEBUG_ON()` — so the one read
that is gated is the harmless one and the three that are not are the ones that
subscript below the base of the table.

- **Evidence tier 2** (the callers type the index; the object's instructions
  show the single-sided compare).
- **Verdict: DEFECT, REACHABLE, FIX WARRANTED — host-side.** The fix is to
  clamp `HW_CODEC_TYPE` to 0..15 before it reaches the constructor, in the
  host, which is form one of the register's own table of fix forms. Nothing in
  `src/` should change: a floor added here would disagree with the blob for
  exactly the callers that need reproducing.
- **Test.** `t_v90prefilter.cpp`'s constructor sweep deliberately stops at
  zero, and the entry explains why — below zero each side reads beneath the
  base of its OWN table, so the comparison would be reporting the fixture. The
  test that WOULD decide it is a caller census: enumerate every construction
  of `V90PreFilter` in the object and in the host and show whether any can
  pass a negative. That is the missing evidence and it is cheap.
- **Citation:** finding F1233. AGREES.

## D179 — `V90Equalizer` with fewer than four taps allocates nothing and writes to it

**DEFECT, REACHABLE ONLY FROM A CONFIGURATION NOTHING VALIDATES.** Both halves
are the object's and neither is wrong alone: the constructor's `len & ~3`
makes any length below four zero, so `sysdep_malloc(0)`; `reset`'s clamp is
`if (linearEquLength - 1 < cursor)` on unsigned values, so `0xffffffff < 0` is
false, the clamp does not fire, and `linearEquCoefs[0] = 1.0f` lands in a
zero-length block.

- **Evidence tier 2**, and unusually strong for this register: the entry
  records the path as **DRIVEN** — `t_v90equ.cpp`'s constructor sweep includes
  lengths 0 and 1, so both sides take it in every trial that uses them.
- **Verdict: DEFECT, REACHABLE from a configuration, and correctly reproduced.**
  The entry's own note that it "survives only because glibc's smallest chunk
  has twelve usable bytes" is the right way to state it: the object is wrong
  and the allocator is covering for it, exactly as `.rodata` adjacency covered
  for D1.
- **Test.** Run the existing sweep under a checking allocator — the
  `V90Parameters` lesson in `CLAUDE.md` is that an under-allocation passes
  every test not run under one. That converts a reasoned claim into a measured
  one and is the single cheapest confirmation in this document.
- **Citation:** finding F1233. AGREES.

## D710 — `VPcmV34GetVisualDiagnostics` selectors 3 and 4 ignore `maxCount`

**DEFECT, REACHABLE, AND ALREADY CORRECTLY DISPOSITIONED.** Seven of the nine
arms load `maxCount` from `0x3c(%esp)`; these two reference it only as the
argument slot of a K56flex stub whose result is discarded, and the stores at
`0x73e9`, `0x742c` and `0x73f0` are unconditional. So
`VPcmV34GetVisualDiagnostics(obj, 3, points, 0)` writes eight bytes into an
array the caller has said holds none, and returns 1.

- **Evidence tier 2**, and the entry is **DRIVEN with a denominator**:
  `t_v34diag.cpp`'s `run_visual` sweeps `maxCount == 0` over all ten selectors
  and its `sawOverrun` flag *requires* the overrun to have been observed. That
  is the discipline `CLAUDE.md` asks for — a detector that cannot report a
  clean run it did not earn.
- **Verdict: no change.** The entry's own reasoning is correct: the fix belongs
  in the API contract, not in this function, because a caller passing a buffer
  of one is served correctly and only zero faults. It is right that this is
  NOT behind `DSPLIB_REPRODUCE_BUGS`.
- **Test.** Already built and already passing; `sawOverrun` is the guard
  against it silently ceasing to fire.
- **Citation:** none. The entry carries its own addresses, which is stronger.

## D901 — `loadParams` reads `BLL_TRN1_QC_SLOW_K2` into `SLOW_K1`

**DEFECT, UNREACHABLE IN THIS BUILD, and one of the best-argued entries in the
register.** Four independent lines converge — three complete `K1`/`K2` pairs
precede it, `setToDefault` continues the `K2` series 7e-12, 5e-12, 2e-12
across FAST/MEDIUM/SLOW, `+0x0f4` is one of the nine slots finding F878
measured as a float declared `int`, and `+0x0f0` being read twice is otherwise
unexplained — and the entry states plainly that nothing dissents.

- **Evidence tier 3 with a tier-1 exit named.** The entry declines to rename
  the field, on the ground that a usage inference must not sit among 291
  rule-1 measurements, and names what would settle it: a reader of `+0x0f4`
  printing it in a diagnostic. That is `CLAUDE.md`'s evidence rule 1 and it is
  precisely how finding F3527 retired `+0x074` and `+0x078` in the same class.
  **This is the correct handling of a strong inference and should be the model
  for the rest of the register.**
- **Verdict: no change.** Unreachable in this build because, per D900, no
  parameter file is parsed; a wrong slow-arm coefficient in any build with a
  real parser.
- **Test.** `t_v90loadparams` already compares the two logs entry for entry
  and fails if both calls do not go to `+0x0f0`.
- **Citation:** findings F861, F878 and F3527. AGREE.

## D306 — `CID_MTD_detect`'s energy accumulators wrap after 257 full-scale samples

**DEFECT, UNREACHABLE — and the bound is named in the entry itself.**
`(x*x + 32) >> 6` is 16777216 for a sample of −32768, so 256 of them are
exactly 2^32 and a 32-bit accumulator returns to zero. The bound is the only
caller: `cid_modem` builds its block in a stack array inside a 0x1dc-byte
frame, "which bounds it well below 257 samples".

- **Evidence tier 2** (the caller's frame types the block length).
- **Verdict: DEFECT, UNREACHABLE from the only caller in the object.** This is
  the D923 shape with a stack frame in place of an ITU table, and the entry
  reached it without prompting.
- **Test.** Already driven — `t_cid_mtd.c` drives 256 and 300 full-scale
  samples so both sides are compared either side of the wrap. What would
  overturn the verdict is a second caller of `CID_MTD_detect` with a longer
  block; a symbol census would settle that.
- **Citation:** none; addresses given.

## D304 — `CID_FSD_demodulate` runs 65535 times on a negative count

**DEFECT, UNDECIDABLE FROM HERE.** The mechanism is exact and shown:
`dec %eax; movswl %ax,%edx; inc %ax; je` is `while (count-- != 0)`, not
`while (count > 0)`, with the counter truncated to a short on every pass, so
−1 runs 65535 further iterations, each reading a sample past the caller's
array and each able to write a bit past the caller's bit buffer.

- **Evidence tier 2** (the object's own instructions, and the reconstruction
  reproduces the idiom with the count declared `short`).
- **Verdict: UNDECIDABLE FROM HERE**, and the entry says why: "this batch did
  not trace where `cid_modem`'s count comes from". **The evidence that would
  decide it is one trace** — where `cid_modem` obtains the sample count it
  passes, and whether any path can make it negative. If it cannot, this
  becomes DEFECT, UNREACHABLE with the caller named, exactly like D306 next to
  it. That trace is the highest value-per-effort item this pass leaves behind.
- **Test.** Not drivable as it stands, and correctly so: both sides would
  agree while scribbling over the harness, which by D561's rule is not a
  differential trial.
- **Citation:** none; addresses given.

## D162 — NOT A DEFECT: reserved Jd bit 48 is cleared by the packer

**The 2026-09-08 standards audit settles the former undecidable row.** The
sibling comparison used the wrong Recommendation's layout. V.92 Table 21
makes bit 47 the Jd/Jp identifier and bit 48 reserved; V.90 Table 13's
constellation fields do not transfer to this class. `packJdData` clears bit
48, together with bits 41..46, before computing the CRC or returning the
transmit vector. The constructor's untouched byte is therefore harmless in
the specified packing lifecycle.

- **Evidence:** [official V.92 Table 21](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-V.92-200011-I!!PDF-E&lang=e&type=items)
  and the explicit reserved-bit clears in `V92Jd::packJdData`.
- **Verdict: NOT A DEFECT.** No production behaviour change warranted.
- **Test:** preserve the seeded constructor comparison. Constructor state
  alone is not a packed message; the wire oracle belongs after packing.
- **Historical citation:** finding F1223 accurately records the untouched
  byte, but does not establish that the byte reaches the wire uninitialised.

## D275 — `V92Jd`'s two receive directions share one pair of state bytes

**DEFECT, UNDECIDABLE FROM HERE.** `unPackJdData` and `unPackJdPhaseData` both
store state to `+0x00` and `+0x01` while switching on two different state
words, `+0xd4` and `+0xd8`. The offsets are measured; that a receiver cannot
run both at once is a one-line inference from them, and the entry says so.

- **Evidence tier 3** (usage inference, declared).
- **Verdict: UNDECIDABLE FROM HERE.** **The evidence that would decide it is a
  caller census of the two unpackers in the object** — if no caller can
  interleave them, this is not a defect but a description of the design. That
  census is cheap and nobody has run it.
- **Test.** Drive both unpackers alternately over one object and compare
  against the blob; if the two sides agree, the sharing is faithful and the
  question is only whether a caller does it.
- **Citation:** finding F1397. AGREES — the finding is this entry's text.

---

# Family 4 — destructors that leave pointers dangling

**Entries: D5, D8, D101, D102, D103, D170, D181, D210, D211, D231.** All ten
reached and all ten decided.

## The shared question, and the instruction that answers it

**Does the object outlive its own destructor?** A member pointer not nulled in
storage that is released on the next instruction is unobservable, and calling
that a defect is a misreading. Three exceptions would make it real: storage the
CALLER supplied and keeps, a `reset`-then-reuse path that runs after the free,
or a second destruction.

**The discriminating instruction is not in the destructor at all — it is the
one AFTER the destructor call in its caller**, and it is per-call-site. No
class in this family has a `D0` deleting destructor (`nm` shows only `D1`/`D2`
for `V92Modem`, `V92Modulator`, `V92Transmitter`, `V92BitsToSymbol`,
`V92Phase4Modulator`, `V92Precoder`, `V92PreFilter`), which is *not* evidence
the object survives: with a non-virtual destructor GCC inlines the release at
the call site. Read that way, the entire V.92 teardown chain is
`dtor(p); sysdep_free(p);` at every link:

| caller | member | destructor call | free of the same pointer |
|---|---|---|---|
| `VPCMXF_Delete` 0xf6c0 | `V92Modem` **embedded** at `this+0x6124` (`lea`, not `mov`) | 0xf70d | **0xf723** |
| `~V92Modem` 0x13a80 | `+0xaa0` params block | 0x13ab1 / 0x13abf | **0x13b4a** |
| `~V92Modem` | `+0x00` modulator | 0x13b38 | **0x13b40** |
| `~V92Modulator` 0x14250 | `+0x48` phase-4 modulator | 0x14313 | **0x1431b** |
| `~V92Modulator` | `+0x4c` bits-to-symbol | 0x14333 | **0x1433b** |
| `~V92BitsToSymbol` 0x4e010 | `+0x00` transmitter | 0x4e03b | **0x4e043** |
| `~V92Transmitter` 0x53ae0 | `+0x50` pre-filter | 0x53b37 | **0x53b3f** |
| `~V92Transmitter` | `+0x4c` precoder | 0x53b49 | **0x53b51** |

**And no second destruction is reachable.** `_ZN12VPcmFloModemD1Ev` (0xd0a0)
and its `D2` copy have **no callers at all** in the object;
`VPCMXF_Delete`'s three call sites are `vpcm_create+0x2c8` (.text+0x3cc7),
`vpcm_create+0x3b2` (.text+0x3db1) and `vpcm_delete+0x3d` (.text+0x3e0c), and
**the two unwind sites are mutually exclusive** — 0x3dc4 jumps to 0x3ccc,
which is past the first site's call, both falling into `sysdep_free(%ebx)` at
0x3ccf and returning NULL at 0x3cd7. So a failed create never reaches
`vpcm_delete`. `V92createConstellations` / `V92createFilterCoefficients` are
called only from the two `V92Modem` constructors and the deleters only from
the two destructors, so no `reset` path touches the `+0xaa0` block;
`V92Precoder::reset(V92MappingParams*)` is reached only through
`V92Transmitter::reset` ← `V92BitsToSymbol::reset` ←
`V92Phase4Modulator::setMappingParams`/`generateSymbol`, all live-object paths
during a connection.

**Uniform test for the seven that rest on this:** re-run the `.text`
relocation scan for callers of each destructor and confirm each call site is
still followed by `sysdep_free` of the same register at the offsets above, and
that `_ZN12VPcmFloModemD1Ev` still has zero callers. If a caller ever destroys
without freeing, or destroys twice, every one of these flips.

## The verdicts

**NOT A DEFECT — D101, D102, D170, D181, D210, D211, D231 (seven).** Every
"dangling pointer" in these entries lives in storage released one or two
instructions after the destructor that failed to null it. Evidence tier 2
throughout: the callers type the lifetime.

- **D101 / D170** are the same claim about the same two functions
  (`V92deleteConstellations` 0x12de0 and `V92deleteFilterCoefficients`
  0x12e90), citing findings F831 and F1226 respectively. Confirmed: not one
  store in either. The ten slots live in the 180-byte block at
  `V92Modem+0xaa0`, and `~V92Modem` frees that block at **0x13b4a** with
  nothing reading the slots in between. **D170 is the better-argued of the
  pair** — it already names the `+0xaa0` free and stops at "documentation only
  until a caller is found that deletes without freeing the block", which is
  the correct verdict. D101's "a second delete double-frees and any later read
  is a use-after-free" describes a second delete that cannot happen and a
  later read that has no storage to occur in.
- **D102** — `CALLPROG_Delete` 0x79440 nulls `+0x70` and `+0x64` and leaves
  three. But CALLPROG is **embedded at `+0x444`** of the block `call_delete`
  frees at **0x2e1a**, and nothing reads `+0x6c`, `+0x78` or `+0x84` in
  between. The entry inherits finding F55's "the object itself is never freed",
  which is true of the FUNCTION and false of the PATH. Its second worry —
  "`CALLPROG_Delete` is a global symbol and the asymmetry is invisible from
  outside" — is true and harmless: **there is no outside.** `nm -u` shows the
  blob imports only `sysdep_*`, `modem_*` and the debug hooks, and the host
  reaches the datapump solely through `modem_dp_register`'s vtable
  (`ref/slmodemd/modem.c:1601` and `:1194`).
- **D181** — `~V92Precoder` 0x56d10 frees `+0x68`/`+0x6c`, `~V92PreFilter`
  0x573b0 frees `+0x04`/`+0x08`, neither stores. Both objects are released by
  `~V92Transmitter` at 0x53b51 and 0x53b3f. The reset-then-reuse exception the
  entry raises is closed: every `reset` path is a live-object path.
- **D210** — six released, exactly one null store (`movl $0x0,0x4c(%esi)` at
  0x53b56 in the `D1` copy), and the transmitter itself is freed by
  `~V92BitsToSymbol` at **0x4e043**.
- **D211** — fourteen pointers over three objects, no store anywhere; each
  object freed by its parent one instruction after its destructor returns
  (0x4e043, 0x1431b, 0x1433b, 0x13b40). The entry is candid that "nothing
  drives a second destruction, because a double free is what it would be
  measuring" — that is the right call and no test should be added.
- **D231** — five released, one null store (`movl $0x0,0x8(%esi)` at 0x13af8),
  and `V92Modem` is **embedded** at `VPcmFloModem+0x6124`, reached by
  `VPCMXF_Delete` with `lea` rather than a pointer load and freed whole at
  **0xf723**, five instructions later.

**DEFECT, UNREACHABLE — D5, D8, D103 (three).** These are the genuine (a)
shape: a free of storage the caller supplied, which WOULD be observable
because the harm lands outside the dying object. What bounds them is that no
caller supplies such storage.

- **D8** — `B103FP_delete` 0x8ef00 ends `jmp sysdep_free` with the object in
  `%ebx` on both exits (0x8efd0, 0x8f00b). It has exactly one caller,
  `b103_create+0x151`, and the instruction before it is `movl $0x0,(%esp)` at
  0x5549 — **a compile-time NULL**. The measured `bad_free=1` comes from a
  fixture constructing a state no caller constructs.
- **D5 / D103** — `FPM_TONE_create` 0xaaa00 sets its "I allocated this" flag
  only on the `state == NULL` branch, and `FPM_TONE_delete` 0xaad00 frees the
  four buffers on `len > 0` alone and then `jmp sysdep_free(this)` at 0xaad49
  unconditionally. The asymmetry is real.

  **D5's stated reason for unreachability is arithmetically wrong and must be
  replaced.** It says "Every call site passes NULL to `create` — the four in
  `B103FP_create` and the one in `FPM_FSM_init`." There are **20 call sites in
  14 functions**, and six pass a member read rather than a constant NULL:
  `SetToneDetect+0x65`, `V32FP_recreate+0x80b/+0x844/+0x876`,
  `V22FP_create+0x2e2`, and `v22_answer`/`v22_originate`. `SetToneDetect`
  alone has eight callers across the V.32 state machines, so the non-NULL path
  runs in every V.32 handshake.

  **It is still unreachable, for a better reason.** In every non-NULL case the
  value is a slot `FPM_TONE_create` itself filled, and the author NULLs the
  slot wherever a fresh object is wanted (`V32FP_recreate`'s
  `movl $0x0,0x2c(%ebx)` at 0x7f062, `V22FP_create`'s `movl $0x0,0x14(%eax)`
  at 0x88255). And `SetToneDetect` 0x83600 builds its config as a **copy of
  the object's own first 0x24 bytes**, patching only the frequency halfword at
  0x8365c — so `len` at `+0x14` is preserved *by construction* and the
  retained buffers stay correctly sized. That is a structural argument, not
  usage inference. **D103's own reachability line is the safer of the two** and
  needs no change.

## What this family changes about the register

- **Two duplicate pairs.** D5 ≡ D103 (D103 says so) and D101 ≡ D170 — the same
  functions and the same claim under two numbers, citing different findings.
  `refcheck.py` reads citations structurally and **cannot catch a duplicate
  ENTRY**; nothing in the tree can. Two of 232 found in one family is worth a
  sweep.
- **D231's header contradicts its own body.** The header says "**Reachability:
  FIRES** on every destruction"; the body concedes "Harmless as shipped". The
  header is what a future reader acts on. What FIRES is an omitted store, and
  an omitted store is not an event.
- **Three citations that do not support what they are cited for:**
  - **D181 — DRIFTED.** Finding F1245 establishes `+0x68`/`+0x6c` and the
    `reset` skip for `V92Precoder` exactly as cited, but D181's headline also
    names `~V92PreFilter` and attributes the same two offsets to it. The
    pre-filter's owned words are `+0x04` and `+0x08`, and its map is finding
    **F1246**, which D181 does not cite.
  - **D231 — DRIFTED.** Finding F1322 is titled "A NULL GUARD CAN BE
    UNREACHABLE IN THE OBJECT ITSELF", is entirely about a dead null guard on
    `+0xaa0`, and concludes "**It is not a deviation either.**" The claim D231
    needs — the embedding at `VPcmFloModem+0x6124` — is finding **F1333**, and
    it is uncited. (D231's other citation, finding F1272, AGREES.)
  - **D211 — PARTIAL.** Finding F1288 is a METHOD finding about how a
    null-guard sweep covers a graph it cannot poison. It confirms the eleven
    guarded pointers and "twenty guards over four destructors", but it does not
    state "null nothing at all"; that comes from 1281's contrast.
- **One adjacent thing found and deliberately not verdicted**, recorded so
  nobody rediscovers it: `V32FP_recreate`'s second `FPM_TONE_create` site
  makes its slot-clear **conditional** (`movl $0x0,0x30(%ebx)` at 0x7f09f runs
  only when `0x48(%esp)` is non-zero), so when it is zero the previous tone
  object is re-initialised in place against a freshly built stack config whose
  `len` need not match the buffers already allocated. That is buffer-size
  retention rather than a bad free, and outside this family. The first site at
  0x7f062 clears unconditionally, which on a second `V32FP_recreate` leaks the
  previous object — also outside this family.
- **Method disclosure.** All verdicts were read off the `D1` copies of each
  destructor; the `D2` copies were spot-checked as equivalent (D210 cites
  0x53aa6 in `D2`, the same store read at 0x53b56 in `D1`) but not diffed
  instruction by instruction.

---

# Family 5 — allocation results used without a null test

**Entries: D96, D171, D180, D212, D232, D235, D236, D303.** All eight reached
and decided.

## The shared question, and it does not collapse the way either answer predicted

**What does `sysdep_malloc` do on failure?** If it aborts, "no null test" is a
style observation and the family is not a defect at all.

**It returns NULL.** `slmodemd/sysdep_common.c:54` is
`void *sysdep_malloc(unsigned int size) { return malloc(size); }` — no abort,
no exit, no failure hook, no arena. `test/harness/runtime.c:204` is the same
plus a fill, and returns `p` through when `p == 0`. Neither injects failure,
which is why `t_vpcmdp.c:1132` records that "nothing in this tree can make
`sysdep_malloc` return zero".

**And the author knew.** `dp_runtime_create` at `.text+0x58f1` does
`movl $0x88; call sysdep_malloc; test %ebx,%ebx; je 59f4` and returns 0;
`vpcm_create` tests `VPCMXF_Create` at 0x3b08 and `K56FLEX_Create` at 0x3b31
and branches into an unwind. So the absence of a test elsewhere is **a real
inconsistency in the original**, not a misreading.

**So the discriminator is the SIZE OPERAND, and every one was read.** An
*immediate* size can only fail if the daemon's whole heap is exhausted, and a
process that cannot obtain 908 bytes is already dead by other means. A
*computed* size can be inflated by a degenerate input until glibc returns NULL
deterministically. The result is the finding that decides the family:

> **In this entire family, no computed-size allocation is dereferenced
> in-function.** All five in `V92Modulator` (`.text+0x154b7`, `0x154ca`,
> `0x154dd`, `0x154eb`, `0x154fb`) and the one in `V92BitsToSymbol`
> (`0x4df0c`/`0x4df7c`) are `mov %eax,off(%reg)` and nothing more. Every
> allocation that IS dereferenced immediately has an immediate size. **The two
> risk factors are disjoint across the whole family.**

**All eight grade DEFECT, UNREACHABLE, and none warrants a fix.** The bound is
named per row rather than asserted: an immediate size, in bytes, at an
address.

| entry | what is dereferenced without a test | the bound |
|---|---|---|
| D171 | nothing — ten results stored to fields and nothing more | immediates `$0x200` ×6, `$0x600` ×4 |
| D180 | three `FloatFIR` and one `FloatIIR` constructor | immediate `$0x14` ×4 |
| D212 | twelve sub-object constructors; purest is `movl $0x1,(%esp); call sysdep_malloc; movb $0x0,(%eax)` at 0x53c96 | immediates; the six argument-sized ones are merely stored |
| D232 | five constructors, incl. `V92createConstellations`'s `mov %eax,0x84(%ebx)` through NULL | immediates, largest `$0x918` |
| D236 | `VPcmFloModem`'s constructor, before the test at 0xfdd2 | immediate `$0x7f68` |
| D303 | `mov %cx,0x33c(%ebx)` at 0x90bce, rejoining after the only test | immediate `$0x38c` |

**Two of these carry tier-1 evidence, which is rare and worth naming.**
`D236`'s message at `.rodata.str1.4+0x2f7c` reads *"VPCMXF_Create: new
VPcmFloModem() fa…"* — **the author wrote a diagnostic for a case his own
instruction ordering makes unreachable**, which is as direct a statement of
intent as this object offers. `D235`'s at `+0x42d8` reads *"V90Modem
Constructor: Il…"* on the illegal-`side` arm.

**Corrections this family makes to the register:**

- **D180 — the fourth constructor is `FloatIIR`, not `FloatFIR`.** `0x572a7`
  is `call _ZN8FloatIIRC1EjPfj`. The entry says "four times across the pair".
- **D96 is misfiled and its own prose drifts.** It says "two host answers";
  one of the two is the object's own `dp_param_get` (0x58c0), a one-line
  wrapper for `modem_get_param(m, 0xa)`. The two halves are bounded
  differently and both are bounded: `MDMPRM_DSPINFO` returns
  `(long)(&m->dsp_info)` (`modem_param.c:80`) — **address-of an embedded
  member, never NULL** — and `MDMPRM_DPRUNTIME` returns `m->dp_runtime`
  (`:78`), which `modem.c:1136` sets inside `do_modem_start` with a `goto
  error` on failure, before the `op->create` at `:1051` that reaches
  `vpcm_create`. The obvious reentrancy trap was checked: `dp_runtime_create`
  does not call `vpcm_create`.
  **What could not be settled:** the entry says the SIGSEGV was "found by
  running". Since `&m->dsp_info` is never NULL under `slmodemd`, that crash
  cannot have come from the shipped host — almost certainly a harness with a
  stubbed `modem_get_param` — and the entry does not say which host it ran
  under.
- **D235 is misfiled** into this family: its mechanism is an uninitialised
  pointer consumed later, not an allocation. Its verdict is unchanged —
  DEFECT, UNREACHABLE, bounded exactly as the entry says by `VPCMXF_Create`'s
  `test %ebx,%ebx; sete %al` at 0xfd05, which yields 0 or 1 and nothing else.
- **D175 cites finding F1230 for "reproduced as written", and 1230 does not say
  that.** 1230 says the signed divide makes the length negative and the
  allocation enormous; its only *driven* claim is that `sysdep_malloc(0)`
  succeeds and `reset` writes into a zero-length block. **Nobody in this tree
  has observed `sysdep_malloc` return NULL**, and this citation is the one
  place the family's reasoning could have been taken for measured.

**Stated test for the family**, and it is one fixture: a failing allocator in
`test/harness/runtime.c`, off by default. Under it, `V92createConstellations`
must return normally with ten NULL fields and the fault must appear in a
*different* function; `V92Modulator`'s constructor driven with a symbol rate
large enough that `*this << 3` exceeds `PTRDIFF_MAX` must still return, with
six NULL fields and no fault. If either faults in-function, the disjointness
claim above is wrong and the family needs regrading.

---

# Family 6 — a value returned or read that nothing on that path wrote

**Entries: D261, D262, D281, D284, D290, D293, D321, D324, D330,
D-V92DEC-1, D-V92DEC-2.** All eleven reached.

**The shared question: does any caller CONSUME the value?** A dead value left
in a register that nothing reads is a reading artifact of the disassembly. The
family splits sharply on it, and the split is the result.

## NOT A DEFECT — D293

`studyUrefHandler`'s two local arrays are filled by six loops and read by
nothing: exactly **six** stores (`0x4254c`, `0x4257c`, `0x4281c`, `0x4284c`,
`0x42b2c`, `0x42b5c`) and **zero** reads, over the function's whole 5,335
bytes. **The misreading is one of category** — this is the inverse of the
family's mechanism, a value *written* that nothing reads rather than a value
*read* that nothing wrote. Dead stores the compiler kept because the locals
are arrays. No consumer, therefore no wrong answer and nothing observable; the
cost is six stores per call. Evidence tier 3. **Test:** a `lea` of
`0x50(%esp)` anywhere in the function would refute it — the scan proves
absence of indexed and direct references, not of a `lea`, and that limit is
stated rather than glossed.

## DEFECT, REACHABLE — D284 and D290, and these are the two to act on

**D284 — `uniteLinMappInfoOfUnsuspectedPhases` reads an uninitialised group
number, and a CLEAN LINE is the trigger.** The slot `0x1c(%esp)` has exactly
one write, `mov %ebp,0x1c(%esp)` at **0x4178c**, inside the scan; it is read
at **0x415fd** (`mov 0x1c(%esp),%ebp; cmp %bp,0x30(%esp,%edx,2)`) to steer
`je 416c0`, the pooling. The skip guard at 0x415f3 means that if all five of
phases 0..4 are suspected, the scan body never runs and the read takes stack
residue. **Four bytes of frame then decide which phases get pooled**, and the
mean written into every unsuspected phase becomes a function of the frame.

The producer is named, and it is the ordinary case rather than a corner:
finding F1427 records that `porcessFirstStudy` marks a phase suspected unless
it collects more than nine rough neighbours out of fifteen, and that "a smooth
mapping, which is what a **clean line** produces, collects none: the counter
starts at 1, ends at 1, and every phase comes out suspected."

- Evidence tier 2 (the read is a 16-bit compare against a per-phase group
  table, which types the slot).
- **The fix, named and not written, is a fix to the ORIGINAL:** give the
  group-number local a defined initial value and take the "no group formed"
  exit when the scan does not run. **`src/` should not change** — the current
  handling (declare it, leave it uninitialised, exclude the path from the
  differential test because two builds have two stack frames) is correct
  reconstruction behaviour. **What warrants changing is the entry's
  reachability field**, which reads `unmeasured` where finding F1427 already
  establishes the trigger.
- **Test:** run `porcessFirstStudy` on a smooth mapping, let it set `+0x280c`
  itself rather than forcing it, and confirm all six come out suspected and the
  next call reads the unwritten slot.
- Citation: finding F1427 AGREES, emphatically — **the finding is more definite
  than the entry citing it.**

**D290 — `findPadGain` hangs outright for five values of `byte_a954`, and the
producer is now traced.** `start = (unsigned char)(a954 - 3)` and
`bound = (int)start - 5`, and the loop test is a **signed** compare of a
zero-extended byte (`movzbl %dl,%eax; cmp %ebx,%eax; jg 43674`). Worked
through: `a954` of 0..2 wraps `start` to 253..255 and terminates; **`a954` of
3..7 puts `start` at 0..4 and `bound` at −5..−1, which a zero-extended byte
can never fall under**; 8 gives bound 0 and terminates. The body also runs
before the first test, and each pass loads `flds 0x9d48(%esi,%edi,4)`, so a
non-terminating loop sweeps all 256 indices — past the end of the object at
phase 5.

**A hang needs no consumer**, which is what makes this the most severe item in
the family. The producer the entry lacked: **`byte_a954` is written by
`V90AutoDigitalImpDetector::determineMaxUcode(short)`** at 0x4449f, 0x444d7,
0x4450a and 0x4468a, and nothing on those paths clamps it away from 3..7.

- **Fix named, not written, to the ORIGINAL:** make the loop bound and the
  counter the same signedness so the descending scan terminates for every byte
  value.
- **Test:** drive `determineMaxUcode` and record the range of `byte_a954` it
  emits. **The reconstruction's current test forces the field into
  [8, 0x9c] — precisely the range that avoids the hang**, so the existing
  fixture cannot see this and its bound should be widened deliberately or its
  narrowness recorded.
- The entry's second half (the uninitialised `0xa1(%esp)`, written only at
  0x43664 inside the accept arm and read unconditionally at 0x43695) stays
  **UNDECIDABLE**; the entry's note that an unordered compare *takes*, so one
  NaN writes the slot and it is five *ordered* non-improvements that reach the
  bug, is correct for `jb`.
- Citation: finding F1439 AGREES.

## UNDECIDABLE FROM HERE — the rest, each with the evidence that would decide it

- **D321 and D-V92DEC-1** — undecidable, but **for a far narrower reason than
  recorded, and the "dead register" reading is definitively excluded.** Both
  methods return the caller's `%edi`, never written. The consumption chain is
  fully traced: one relocation each, both from
  `V90Phase3Demodulator::getDecision` (0x258f0), whose two call sites are in
  `V90Equalizer::process` (0x39b39, 0x3a093); at both the next instructions
  are `cwtl; mov %eax,0x88(%esp)`, and `0x88(%esp)` is read seven times —
  including **`mov %dx,(%esi,%eax,2)` at 0x39268 where `%esi` is
  `0xec(%esp)`, the third parameter of
  `process(float*, unsigned, short*, short*, unsigned&)`.** So the
  uninitialised value **is written into the caller's own output array**, and
  at 0x39279–0x3928c it is subtracted, absolute-valued and compared against
  `$0x12c` to steer a branch. Both paths converge on `jmp 39250`, so the
  zeroed state does not route around the read. **What remains open is only
  whether the states are live** — and there is a lead the entries do not have:
  states 7 and 8 are written to `+0x28` by `getV92Decision` itself (0x2213e,
  0x233db), the two halves share one state word, and each function's default
  set is largely the other's live set. **Deciding evidence:** instrument
  `+0x08` and `+0x28` across a V.90 and a V.92 session and record whether
  `getV90Decision` is ever entered with `+0x28` in {7, 8, 0x12, >0x21}. If
  never, both close as DEFECT, UNREACHABLE with a clean bound.
- **D-V92DEC-2 — half NOT A DEFECT, half undecidable, and misfiled.** It is a
  narrowing question, not an uninitialised one. `isThereAnyAltRbsPhase`
  (0x40570) and `isAltRbs` (0x405b0) provably return exactly 0 or 1, so
  `test %ax,%ax` on them is exact: **the misreading is treating a narrow test
  as narrowing when the callee cannot produce a value with a zero low half and
  a nonzero upper half.** The two `V92Jd` methods are open; **deciding
  evidence:** enumerate every `mov …,%eax` reaching each `ret` in
  `unPackJdData` (0x128e0) and `unPackJdPhaseData` (0x12500). If all are
  ≤ 0xff the row closes as NOT A DEFECT.
- **D261** — consumption is PROVEN, not assumed: the two post-loop reads at
  0x571ed and 0x5720c each feed `FloatFIR::process(float)` and land in
  `+0x74` and `+0x70`, so the contamination enters two filter histories and
  two object fields and outlives the call. **Deciding evidence:** the actual
  range of the modulus field in `V92MappingParams` — a writer that can produce
  a negative value converts this to REACHABLE at once.
- **D262** — **the entry's caller survey is the most rigorous in the register
  and could not be improved on.** It is relocation-exhaustive, correctly notes
  that a relocation's *absence* is what would hide a same-TU caller (findings
  F306, F333), records that an earlier draft said CANNOT FIRE on "three arms are
  a plausible complete set" reasoning and was corrected, and names its own gap.
  **Deciding evidence:** every writer of `V92MappingParams+0x10`, read at
  0x53da6 — a fourth value there makes this REACHABLE.
- **D281** — DEFECT, UNREACHABLE from inside the object, and the bound is
  positive rather than merely absent: **finding F1427 states explicitly that
  D281 has no producer where D284 does** — it needs a caller to flag every
  phase at `+0x2800`, and `updateUref` passes the flags through untouched.
  **Deciding evidence:** any writer that can set all five
  `short_2800[0..4]` non-zero.
- **D324 — the entry's caller claim is wrong in both directions.** It says
  "`process` is the only caller and is not yet written". A relocation sweep
  finds **three** call sites and `process` **is** in the object:
  `V90Equalizer::process` at 0x3a461 and 0x3a9ea, where the very next
  instruction is **`fstp %st(0)`** — the value is popped and discarded, twice —
  and `V90Demodulator::progress` at 0x1d1f7, which does `fstps 0x44(%esp)`
  then `mov 0x44(%esp),%ebx`. **That third one is the real consumer and the
  entry does not mention it.** Consumption there is path-dependent (`%ebx` is
  reloaded with an immediate on the fall-through at 0x1d23d). **Deciding
  evidence:** a liveness trace of `%ebx` from 0x1d20c across every successor.
  If every path overwrites it before use, this closes as NOT A DEFECT.
- **D330 — the entry's reachability note rests on a miscitation.** It says
  "nothing in the object calls this member (D345)". **Both halves are wrong:**
  there are two `R_386_PC32` call sites, both in
  `V90ConstellationDesigner::adjustConstellationsToNewK` (0x4c2ea, 0x4c59d),
  and **D345 does not list this member** — its eleven names are `pow6`,
  `calcK`, `realK`, `maxK`, `calcMtoMatchKtarget`, `findMinValueIndex`,
  `findConstelMaxValueIndex`, `constelBuild`, `spectralDesign`,
  `reconstructInitialConditions` and `findNextUcodeToAdd`. The member IS
  reachable from inside the object; what is unknown is whether
  `adjustConstellationsToNewK` can present a constellation size above 255.
  **Deciding evidence:** instrument `m[phase]` at the two call sites.

## What this family changes about the register

**Four entries are misfiled** — D96 (a host pointer, not an allocation), D235
(an uninitialised pointer, not an allocation), D-V92DEC-2 (narrow-width
testing) and D293 (dead stores, the inverse mechanism). Misfiling is not
harmless in a register organised by mechanism: it is how a shared argument
gets applied to a row it does not fit.

**Two entries state caller facts the object contradicts** — D324 and D330 —
and both were stated as reasons the entry could not be decided. An entry that
cannot be decided *because nobody looked* is not the same as one that cannot
be decided at all, and the register does not currently distinguish them.

**Not verified, stated for the record.** D261's and D281's functions were not
disassembled in full — D261 only for the slot writes, reads and post-loop
consumption; D281 rests on the entry plus finding F1420. `isAltRbs`'s
zero-initialisation is inferred from its tail. The two `V92Jd` return-path
surveys are incomplete. D293's "no reads" is proven against indexed and direct
references, not against a `lea` of the slot.

---

# Family 7 — an index fed from a field that arrives ON THE WIRE

**Entries: D88, D93, D94, D107, D131, D135, D162, D256, D259, D265, D266,
D276, D277, D287, D288, D289, D323, D333, D344, D351, D352, D471.**
**Examined 22, decided 19, left undecidable 3.**

This is the D923 family — the only one where the ITU-T text decides — and it
is where this document's most consequential results are. Every bound below is
a quoted clause with its number, per the standard D923 set. Where no clause
could be found, the row says so rather than asserting one.

## THE TWO THAT ARE REACHABLE FROM A CONFORMANT PEER

### D94 — the V.8 CM/JM collector reads before it checks, and the match path never checks at all

**MECHANISM.** In `v8handshak`, with `%esi = this+0xc4c` (a
`struct v8_tx_sequence *`) and `%ebx = movswl` of the index `fdbc`:

    78070:  movzwl (%esi,%ebx,2),%ebp     ; THE READ -- no compare before it
    78074:  cmp    %ecx,%ebp ; je 780f7   ; match path
    78078:  cmp    $0xe,%dx  ; jg 78095   ; the bound, AFTER the read
    7807e:  mov    %cx,(%esi,%ebx,2)      ; it gates only the STORE
    780f7:  lea    0x1(%edx),%ebx ; mov %bx,0xdbc(%esi)   ; match path: no test

**It is worse than the entry says.** The bound gates the store only; the match
path at `0x780f7` raises `fdbc` **with no test at all**, so once the index
passes 14 it keeps climbing every time the stale word beyond the array happens
to equal the received character — and `%ebx` is a `movswl` of a `short`, so it
can walk ±32767 entries either side of the struct. The array is fifteen
entries: `include/dsplib/v8.h:173,180` puts `short word[V8_TX_SEQ_WORDS]` at
+0x00 and `short crc` at +0x1e.

**THE BOUND — there is none, and the recommendation says so twice.**

- ITU-T V.8 §5: *"A sequence consists of 10 ONEs followed by 10 bits for
  synchronization and then information-bearing octets, each octet being
  preceded by a start-bit (ZERO), and followed by a stop-bit (ONE)."* **No
  count.**
- ITU-T V.8 §5.2, extension octets: *"When 3 option bits are inadequate for a
  particular category, **any number of extension octets may follow directly
  after a category octet**."*
- ITU-T V.8 §6.6 / Table 8: the non-standard facilities field carries a length
  octet of up to 255 and *"**Multiple concatenated NS information blocks may
  be transmitted**"*, with the NS field parsed by §5.2's extension rules,
  distributing each five bits over ten. **A 255-octet NS block alone expands
  to about 408 extension octets.**
- ITU-T V.8 §6: *"a receiver shall ignore all bits, codes and octets reserved
  for such future definition"* — tolerance is **mandatory**.
- Clause 8 (§8.1.2, §8.2.2, §8.2.3) was read for a cap and has none; it speaks
  only of *"a minimum of 2 identical CM sequences"*.

The sync detector is `cmp $0xf,%cx` at 0x78056 and the marker written into
`word[0]` is `movw $0xf,(%edx)` at 0x780b5 — 15 is Table 1/V.8's ten
synchronisation bits `0 0 0 0 0 0 1 1 1 1`. `fdbc` resets to 1 at each sync,
so the index is bounded only by **how many characters a conformant peer puts
between two syncs**, and the recommendation permits hundreds.

- **Evidence tier 2** for the wire identity (`v8handshak`, and the sync
  constant is Table 1's own pattern); **tier 3** for the fifteen-entry extent,
  which rests on `crc` sitting at +0x1e.
- **VERDICT: DEFECT, REACHABLE FROM THE WIRE, FIX WARRANTED.** A conformant CM
  or JM carrying a non-standard facilities field, or enough extension octets,
  exceeds fifteen characters and drives an unbounded read. **Fix: test `fdbc`
  against the array extent before the read at 0x78070 and before the unguarded
  increment at 0x780f7, not only before the store.**
- **Test.** Feed the collector a synthetic CM of 20+ ten-bit characters whose
  16th to 20th equal the bytes at +0x1e..+0x26 of the sequence struct, and
  watch `fdbc` climb past 15 while the match counter at +0xdb6 rises. Refuted
  if `fdbc` is clamped anywhere upstream of `v8handshak`.
- **Citation:** finding F73. AGREES — its body restates the claim verbatim and
  records the fifteen-word message and the 0x00f marker.

### D256 — `addReceivedSampleToStorage` overruns on a DIL the recommendation explicitly permits

**MECHANISM.** `V90AutoDigitalImpDetector::addReceivedSampleToStorage`, blob
0x41ff0, 149 bytes, and **no compare against anything in the whole method**:

    42004:  imul $0x83e,%ecx,%esi          ; row stride 2,110 shorts
    4200e:  mov  0x9100(%edi,%ecx,4),%ebx  ; the index
    42045:  mov  %ebx,0x9100(%edi,%ecx,4)  ; written back, incremented
    42055:  mov  %ax,0x2818(%edi,%esi,2)   ; the store

**The caller check the entry says was missing is now done.** `objdump -r`
gives eight call sites, all in `V90Phase3Demodulator::getV92Decision(float)`
(0x21be7, 0x21cbb, 0x21d91, 0x223cd) and `getV90Decision(float)` (0x23d52,
0x23e2d, 0x242bf, 0x243cb) — one sample per phase per data frame, offered from
the per-symbol decision path, **with no guard at any call site**.

**THE BOUND — the recommendation does NOT keep it inside the array.**

- ITU-T V.90 §9.3.2.10: *"**Within 5000 ms** of transmitting S in 9.3.2.8 the
  analogue modem shall again transmit signal S for 128T followed by S for 16T.
  This indicates to the digital modem that the analogue modem has received
  enough of the DIL sequence."*
- ITU-T V.90 §8.4.1: *"The entire sequence, not just the last DIL-segment, is
  repeated until either the analogue modem causes it to be terminated or a
  timeout occurs."*
- ITU-T V.90 §5.3: *"Data frames in the digital modem have a six-symbol
  structure."*

At T = 1/8000 s, 5,000 ms is 40,000 symbols; at six symbols per data frame
that is **up to about 6,667 samples per phase against a 2,110-entry row**. The
row holds 12,660 symbols ≈ **1,582 ms** of DIL where the recommendation
permits 5,000. **Overrun begins at any DIL longer than about 1.6 s, which is
well inside the permitted window, and the peer is REQUIRED to keep sending
until we stop it.**

- **Evidence tier 2** — `linear2alaw` and the two decision methods type both
  arguments; the 0x83e stride is read straight from the object.
- **VERDICT: DEFECT, REACHABLE FROM THE WIRE, FIX WARRANTED.** **Fix: bound
  `int_9100[phase]` at 0x83e before the store at 0x42055 and stop accumulating
  rather than wrapping, or size the row for the 5,000 ms the recommendation
  allows.**
- **Test.** Run a DIL longer than about 1.6 s and watch `int_9100[phase]` pass
  2,110 — the next store lands in `sampleStore[phase+1][0]`, and at phase 5 it
  leaves the 43,440-byte object. Refuted if some state above `getV90Decision`
  stops offering samples before 2,110 per phase; **that is the one thing this
  trace does not cover**, since neither method's callers are reconstructed.
- **Citation:** finding F1363. AGREES.

## D107 — a conformant ZERO-LENGTH DIL request reads an uninitialised slot

`V90Phase3Modulator::resetDILGenerator` (0x2aed0) fills the `dilLevel` row only
while the index is below `dilCount`, so **`dilCount == 0` leaves `dilLevel[0]`
unwritten** — and then reads it back unconditionally at 0x2b022 and runs the
boundary search at 0x2b040..0x2b050, storing the result at `this+0x390`. The
loop reads indices 0..7 and **falls out with 8**, one past the eight-entry
row. Both callers are in `V90Phase3Modulator::reset` (0x2c3ac, 0x2c45d) and
neither guards on `dilCount`.

**THE BOUND — N = 0 IS EXPLICITLY LEGAL, which is what makes this reachable.**

- ITU-T V.90 §8.4.1: *"The DIL consists of N DIL-segments of length Lc where:
  **0 ≤ N ≤ 255** … **When N = 0, DIL is not transmitted.**"*
- ITU-T V.90 Table 12, DIL descriptor, bits **18:25 = N** — eight bits, so 0
  is representable and is the "no DIL" request.
- ITU-T V.90 §9.3.2.8: *"If the analogue modem requested a DIL of zero length
  it shall proceed with Phase 4."*

**Which side, in D920's terms:** this fires on the **DIGITAL-modem side**.
`V90Phase3Modulator` is the DIL transmitter, V.90 §8.4 is headed *"Phase 3
signals for the digital modem"*, and §8.4.1 says the parameters are *"sent to
it by the analogue modem using the DIL descriptor"* — so the descriptor is a
RECEIVED message on the side that faults.

- **Evidence tier 2** — the mangled argument type `tagV90DILdescriptor const*`
  and the `alaw2linear`/`ulaw2linear` callees.
- **VERDICT: DEFECT, REACHABLE, FIX WARRANTED.** **Fix: skip the segment
  search, or seed `dilLevel[0]`, when `dilCount == 0`, and clamp the search
  result at 7.**
- **Limit of the trace, stated rather than glossed:** what consumes
  `this+0x390` was not followed. The out-of-row 8 is an out-of-range VALUE
  today; whether `generateDIL` or `generateV90Symbol` then subscripts an
  eight-entry array with it is untraced, and that is the difference between
  "wrong segment chosen" and "read past a row".
- **Test.** Reset the modulator with a descriptor whose byte +0 is zero and
  whose `dilLevel[0]` slot has been seeded with 0x8000..0xFFFF, and read
  `+0x390` — it will be 8.
- **Citation:** finding F232. AGREES.

## D135 — REFUTED. "The A-law boundary row's last entry can never be reached"

**Both halves of the claim fail against the object.**

1. **"Can never be reached."** The two loads are `movzwl 0x188(%esi,%eax,2)`
   at 0x2aea5 and `movzwl 0x188(%esi)` at 0x2b022 — **UNSIGNED**. The search
   exits at the first boundary ≥ level, and a valid A-law level is a positive
   `alaw2linear` result, 0..32256, so **any level in 16385..32256 exits at
   index 7**. Entry 7 is the deciding comparison for the top segment and is
   reached on every top-segment level. What *would* have made index 8
   unreachable is a `movswl` load, and that is not what the object does.
2. **"Both users."** There are **seven** references to
   `codeSegmentsBoundriesLookupTable` across **five** functions:
   `updateCodeSegmentPointer`, `resetDILGenerator`, `generateDIL`,
   `generateV90Symbol` (twice) and `generateV92Symbol` (twice).

- **Evidence tier 1/2** — the table's own bytes, and the `alaw2linear` callee
  typing the level.
- **VERDICT: NOT A DEFECT as stated.** The residual true fact is D107's:
  because the load is unsigned, a slot content of 32,769 or more — any
  negative short — does fall out at index 8. **The entry should be replaced by
  that observation, which belongs to D107.**
- **Test.** Feed level 32000 with `pcmType` A-law and observe index 7; feed
  0x9000 and observe index 8.
- **Citation:** finding F231. AGREES with the entry's own caveat — **and the
  finding is where the claim originates and does not account for the
  `movzwl`.** Like D176/finding F1233, the correction belongs upstream.

## Settled against the recommendation — unreachable from a conformant peer

**D259 — the code histogram's index is unmasked.** The code argument is a full
`unsigned char` (`movzbl 0x20(%esp),%edx` at 0x4205d) into a 6 × 128 array at
0x8b00..0x9100, and **0x9100 is `int_9100[0]`, D256's own cursor** — the
adjacency is confirmed from the object. But ITU-T V.90 §3.5: *"**Uchord**:
Ucodes are grouped into eight Uchords. Uchord1 contains **Ucodes 0 to 15**; …
Uchord 8 contains **Ucodes 112 to 127**."* Every wire field carrying one is
seven bits (Table 12's `REF1`, Table 14's eight 16-bit masks), so a legal
Ucode is 0..127 against a row of exactly 128 — **zero margin, and in range.**
The object's own callers honour it: at 0x21bb4–0x21bce the sample is made
non-negative, encoded by `linear2alaw`, then `xor $0xd5` — the exact inverse
of `resetDILGenerator`'s `(byte & 0x7f) ^ 0xd5` at 0x2af40. **DEFECT,
UNREACHABLE from a conformant peer**; what would reach it is not a malformed
peer but a *caller* passing the raw eight-bit PCM codeword instead of its
Ucode, which would fire on roughly half of all samples. No such caller exists
today. **Citation: finding F1366 — DRIFTED, harmlessly.** 1366 is headed *"THE
OBJECT DIVIDES IN ONE METHOD AND MULTIPLIES BY A RECIPROCAL IN TWO"* and is
about mean/variance arithmetic; the histogram material is in **finding F1363**.
The claim is true and the number is one off.

**D276 — a preamble longer than seventeen 1 bits desynchronises Jd.** ITU-T
V.90 **Table 13**, first row: *"Jd bits LSB:MSB **0:16** — Frame Sync:
11111111111111111"*, then *"17 — Start bit: 0"* and *"68:71 — Fill bits:
0000"*; §8.4.2: *"Sequence Jd consists of a whole number of repetitions of the
bit pattern given in Table 13. Bit 0 is transmitted first."* **Every
repetition presents exactly seventeen 1 bits terminated by a 0**, and a
conformant transmitter cannot emit an eighteenth. **DEFECT, UNREACHABLE from a
conformant peer** — but reachable from **a single channel bit error flipping
the bit-17 start bit to 1**, and that is worth recording because it costs
almost nothing: the receiver needs a fresh seventeen and the next repetition
supplies them, 72 bits at 8000 sign bits/s ≈ 9 ms against §9.3.2.7's 4,500 ms
Jd-detection window. A resynchronisation delay, not a memory hazard.
**Citation: finding F1397. AGREES.**

**D471 — `setTrn2DummyConstel` has no bound against the 128-byte row.** The
only bound is a signed compare against `nofUcodesInTrn2` at 0x3cb24/0x3cb40.
The one-hop question is answered: `setNofUcodesInTrn2(short)` (0x3ca10) uses
its `short` argument **only as a gate** and takes the value from
`V90Parameters+0x80`, and a sweep of all 75 `mov …,0x80(%reg)` sites in
`.text` finds the only writer of that field is `V90Parameters::setToDefault()`
(0x29a75). **No wire field reaches this count**, so the entry is misfiled here;
both writers of +0x78 are internal constants. **DEFECT, UNREACHABLE.**
**Test:** grep the object for any store to `V90Parameters+0x78` or `+0x80`
other than the two named — a third, wire-fed one reopens it.

## Reachable, but not from a wire field — D351 and D352

`adjustConstellationsToNewK` accepts a row of exactly 128 (`cmp $0x80,%eax;
ja` at 0x4c726) and its shift loop then writes `constellation[k][128]`, which
tiles onto the neighbour; `constellation[k+1][0]` is what
`reconstructInitialConditions` searches for, whose `drop` over-decrements an
unsigned length to 0xFFFFFFFF and never terminates.

**128 is IN CONTRACT, which is exactly why the accept is right and the shift is
wrong.** ITU-T V.90 Table 14 specifies the constellation as eight 16-bit
Uchord masks — *"137:152 Constellation mask for Uchord1 (bit 137 corresponds
to Ucode 0)"* through *"256:271 … Uchord8 (bit 256 corresponds to Ucode
112)"* — 8 × 16 = **128 bits, one per Ucode**; §5.4.3: *"Mi is equal to the
number of positive levels in the constellation to be used in data frame
interval i as signalled by the analogue modem using the CP sequences."* So
Mi = 128 is representable and legal.

**But the index is not wire-fed:** `V90ConstellationDesigner` is the analogue
modem's OWN design code, producing the mask that CP later carries. The wire
influences it only through the rate the peer advertises in Jd bits 18:46.

- **Evidence tier 3**, on top of the entry's own core-dump measurement.
- **VERDICT (both): DEFECT, REACHABLE, FIX WARRANTED** — from the modem's own
  legal design space rather than from a wire field, so **misfiled in this
  family**. **Fix: bound the shift at 127 (D351) and bound `drop` by
  `constellationSize[k]` or widen `i` (D352). D352 is a total hang and D351 is
  its trigger, so they must be fixed as one.**
- **Test.** Already run: `t_v90cdadjust.cpp` at constellation lengths above 60
  reproduces the write and the core shows `n = 4294967295, k = 5`. **The open
  question is whether the add pass reaches 128 at a rate a real session
  negotiates**, and that is the deciding evidence.

## Misfiled — the index is internal, and no wire field bounds it

Traced far enough to establish that in each case, and no further. Evidence
tier 3 throughout.

| entry | where the index actually comes from | verdict |
|---|---|---|
| D131 | `max_bits` and the caller's fragment size — Bell 103, no V.8/V.34/V.90 field. `DemodDataB103` feeds 48 samples against a 64-sample/8-bit ceiling. | DEFECT, UNREACHABLE |
| D162 | reserved transmit field, explicitly cleared by the packer | NOT A DEFECT; also misfiled as an index |
| D265 | a transmit-side constant slip (0x45 where 0x55/0x56 was meant); nothing subscripted by a received value | defect stands as recorded; misfiled |
| D266 | a fixed constant; wire-*triggered* by any CRC failure, but nothing is indexed | misfiled |
| D277 | caller behaviour. Its own non-entry hazard `vec[unpack[1]]` is bounded by Table 13's 72-bit Jd plus each storing state's cap | misfiled |
| D287 | the cursor is `short_8b00[phase][code]` in scan order — the object's own histogram, filled by D259's path; the `[-2]`/`[-1]` window is internal | misfiled; the negative-index read fires on every call |
| D288, D289 | a `short` produced inside the class; `ci` walks down from it as an `unsigned char` | misfiled |
| D323 | `linearEquLength` and `dfeWindowHalf` from `reset` / `setLinearEquEdgesFadingParams` — the parameter block | misfiled |
| D344 | `(which << 7) + i` inside the constellation designer. The recommendation bounds the CONTENT (Ucodes 0..127, §3.5, exactly the 128-entry row); what is unbounded is the walk, which stops only on a `signed char` sign flip at 0x80 | misfiled |

**Correction to this family's former D162 argument.** It applied V.90 Table
13 to a V.92 Jd message. V.92 Table 21 instead reserves bit 48, and
`packJdData` clears it before transmission. The actual V.92 constellation
fields are Jp wire positions 48 and 49 in Table 22, owned by `phaseBits`.
D162 is NOT A DEFECT, as established in Family 3 above. Family 7's three
unresolved index questions remain D88, D93 and D333; D162 had already been
excluded from those three as a misfiled non-index case.

## Left undecidable — 3, with the evidence that would decide each

**D88 — `shellDemapper`, and this is the one to chase next.** The V.34 text
gives a quotable bound that does NOT settle it. ITU-T V.34 §9.4: *"the shell
mapper maps K input bits … into 8 output ring indices {mi,0,0, …, mi,3,1},
where **0 ≤ mi,j,K < M**"*; §9.2: *"The number of bits put into the shell
mapper per mapping frame is denoted by K where **0 ≤ K < 32**"*, and Table 10's
largest M is **18** (2743 baud, 14 600 bit/s, K = 31). So across the whole of
V.34:

| sum | maximum | 128-entry table? |
|---|---|---|
| g2 index ≤ 2(M−1) | 34 | inside |
| g4 index ≤ 4(M−1) | 68 | inside |
| **z8/g8 index ≤ 8(M−1)** | **136** | **OUTSIDE** |

**What would decide it: which of the tables at +0xa48 / +0xb48 / +0xc48 is
subscripted by the EIGHT-fold sum** (§9.4's `0 ≤ p ≤ 8(M−1)` domain). If any
of them is, **M = 18 makes a conformant V.34 connection at 14 400/2743 overrun
a 128-entry table by nine entries**, and this becomes the third REACHABLE in
the register. Only the first 0xe0 bytes of a ~450-byte function were read and
that site was not reached. Two secondary points: the register's "a parameter
of 200 gives an index near 1200" is a value no legal ring index or sum can
take — the maximum sum-of-four is 68 — so **the recorded segfault is an
artefact of driving the function directly rather than a wire path**; and
`movswl 0x4(%ebp),%edx` at 0x5875b loads one sub-index SIGNED, so a negative
index is representable, which is a `demapFrame` question.
**Citation: finding F129. AGREES** — it says three tables where the entry
heading says four, which is consistent: the fourth is the 529-entry grid in
`decodeDepth`.

**D93 — two halves, two answers.** `ApplyBulkDelay`'s `bulk_len` is a
configuration field with no wire path — misfiled, unreachable from this
family's question. `getbit` (0x5eaf0) reads a bit cursor over `word[10]` at
+0xaa3c with `crc` at +0xaa50, fed by `getMPrecvdBits`, so the underlying wire
object is a **V.34 MP sequence** and `v34handshak` hands the record to `getbit`
thirty-six times. **What would decide it: the identity of the message `getbit`
actually parses out of +0xaa3c.** Finding F227 describes +0xaa3c/+0xaa3e as
`info_caps`, a *rebuilt capability word*, not the raw received MP. If `getbit`
is ever pointed at a raw MP, MP is longer than 160 bits and this is reachable;
if it only ever reads the rebuilt word, ten words is a real bound.
**Citation: finding F227. AGREES.**

**D333 — `setConstellationToNoise`'s 128-byte staging array.** The span is
`arg5[k] − params->unnamed_360` with `arg5` an `unsigned char *`, so up to 256
against 128 bytes at 0xc0 in a 0x14c frame. **No bound applies because there
is no caller**: the entry records (D345) that nothing in the object calls this
member, so there is no caller range to check and no wire field reaches `arg5`.
**What would decide it: one caller.** Given one, the question reduces to
whether `arg5[k]` is a Ucode (≤ 127, and V.90 §3.5 makes it safe) or a count
or difference (up to 255, unsafe). **No test is possible until then** — driving
it directly measures the fixture, not the object.

---

# Family 8 — unguarded divisions, and loops with no bound

**Entries: D33, D113, D126, D185, D226, D228, D229, D282, D292, D297, D304,
D331, D336, D337, D338, D343, D356, D930.** Eighteen examined, seventeen
decided, one left.

## THE SHARED BOUND, and it is stronger than the one the register uses

Six entries in this family are bounded by "a parameter cannot take a bad
value". The register argues that as *"slmodemd never supplies a parameter
file"* — a claim about the host. **The object proves something stronger.**

`Vparser_read_int` (**0xb0990**) and `Vparser_read_float` (**0xb09a0**) are
three-byte stubs in the shipped object:

    b0990:  31 c0    xor %eax,%eax
    b0992:  c3       ret

**They write nothing through `*value`.** `V90Parameters::loadParams` and
`V92Parameters::loadParams` are their only callers, and
`V90Parameters::loadModemParamsData` — the *unconditional* second writer,
which runs on both arms of `if (paramFile)` — touches only
`DIGITAL_POWER_REDUCTION`, `PROBING_MODE`, `LINE_CONNECTION_TYPE` and
`TRN2D_MEAN_ERROR_STD_EVALUATION_ENABLE`. `V92Parameters::init` tail-calls
`loadParams` and has no equivalent at all.

**Therefore every other `V90Parameters` / `V92Parameters` field holds its
`setToDefault` value for the life of the process, whatever
`modemParams->paramFile` is.** That needs no claim about the host at all, and
it is the correct form of the argument. Defaults it fixes, used below:
`V92_ECHO_FILTER_LENGTH = 180`, `RATE_FORCE = 45333`, `SPECTRAL_SHAPER_SR = 1`,
`GERMAN_PBX_SPECTRAL_SHAPER_SR = 3`, `ENABLE_DIGITAL_POWER_REDUCTION = 1`,
`SPECTRAL_VERIFIER_FFT_LEN = 1024`, `SPECTRAL_VERIFIER_FFT_WINDOW = 1`.

**Integer and float divides are different severities and the register does not
separate them.** An integer divide by zero is `#DE` → SIGFPE, immediate and
fatal. An x87 float divide by zero is an infinity or the real indefinite
0xffc00000, which propagates silently into a decision. Every row below says
which it is.

## The verdicts

| entry | kind | verdict |
|---|---|---|
| D33 | integer, SIGFPE | UNDECIDABLE FROM HERE |
| D113 | integer, SIGFPE | UNDECIDABLE — duplicate of D33 |
| D126 | float, NaN | UNDECIDABLE — two of three routes closed |
| D185 | integer, SIGFPE | DEFECT, UNREACHABLE — bounded by `$0x32` |
| D226 | integer, SIGFPE | DEFECT, UNREACHABLE — **bounded host-side, not by the object** |
| D228 | unsigned wrap | DEFECT, UNREACHABLE — the shared bound |
| D229 | signed→unsigned | DEFECT, UNREACHABLE — the shared bound |
| D282 | float, NaN | DEFECT, REACHABLE, FIX WARRANTED |
| D292 | float, NaN | DEFECT, REACHABLE, FIX WARRANTED |
| D297 | loop | UNDECIDABLE — the one not closed |
| D304 | loop | DEFECT, UNREACHABLE — **now closed, see below** |
| D331 | float, infinity | DEFECT, REACHABLE — **hang claim REFUTED** |
| D336 | loop | DEFECT, UNREACHABLE — `RATE_FORCE` gives n = 29 |
| D337 | loop | DEFECT, REACHABLE, FIX WARRANTED |
| D338 | loop | DEFECT, REACHABLE, FIX WARRANTED |
| D343 | loop | DEFECT, UNREACHABLE — **no caller exists** |
| D356 | integer, SIGFPE | DEFECT, REACHABLE, FIX WARRANTED |
| D930 (loop half) | loop | DEFECT, UNREACHABLE — `shaperSR ∈ {1,3}` |

## The reachable ones, with the fix named and not written

**D356 — `calcModulusParameters`'s `__moddi3` by an empty phase. The only
entry in the family with a REPRODUCED fault**, `constellationSize = {0, 8, 0,
18, 1, 22}` and `__moddi3` as the top frame. Five `__moddi3` and six
`__divdi3` calls at 0x3dddb..0x3df13 on `remaining[i] % constellationSize[i]`;
integer, so SIGFPE. The gate chain is real — `constellationDesign` →
`adjustConstellationsPower` (0x4cb89) → `getPower` → `calcModulusParameters`
(0x3e0fd), and `process` reaches it the same way at 0x4cf57 — and **the gate
is `ENABLE_DIGITAL_POWER_REDUCTION`, whose default is 1, so it is OPEN on the
shipped configuration and cannot be closed at run time.**
**Fix, host-side or opt-in:** floor the per-phase length at one before the
power pass, or skip the pass on an empty phase. **What stays open and must
not be glossed:** whether a live session can produce an empty phase. Nothing
bounds the ucode tables a real detector fills, so **do not promote this to
FIRES TODAY on the fixture alone.**

**D337 and D338 — two non-terminating loops in
`setConstellationToNoise_forceRate`, and D338's is visible in the
fall-through.** D338 at 0x4a560–0x4a5b4: the `cmp $0x74` give-up at 0x4a56d
skips the inner `for` and lands at 0x4a5ad, and the path from 0x4a5ab to
0x4a5b4 **touches neither `%di` nor `constellation[k][0]`**, so the outer
`while` re-tests identically and repeats for ever. That is not a claim, it is
the control flow. D337 at 0x49b8e–0x49bf9: the scan skips flagged phases
(`cmpb $0x0,(%edi,%edx,1); jne 49bc0`), so **a flagged phase holding a zero
count is never incremented, the product stays 0, and `0 < 2^n` never fails.**
(D337's other candidate — `2^n` overflowing to infinity — does not survive
reading: `fadd %st(0),%st` doubles in an 80-bit register whose exponent needs
n > 16384.)
**Fixes:** make D338's give-up exit the outer `while` rather than the inner
`for`; cap D337's refinement at a turn count or refuse to enter it when an
unflagged-eligible count is zero. **D338 is coupled to D339** — an insert
writes zero into `constellation[k][0]` rather than advancing it, which is what
makes the give-up ineffective even on the insert path — so D339 must be
re-read before either is touched.

**D282 and D292 — one NaN, produced in one function and consumed in the
other.** D292: `studyUrefHandler` at 0x42260 loads the sample count and
`fildll`s it straight into a divide with **no `test` between**, where its four
sibling loops all test; float, so a NaN rather than a trap. D282:
`getAltVarThresh` at 0x406cc divides by a 16-bit count (`push %cx; filds
(%esp)`) that the selection loop increments only on the accumulate arm — so
six equal entries select nothing, the count is 0, and 0.0/0 is the real
indefinite. Finding F1442 traces the first NaN into `var[phase]`, into
`getAltVarThresh`, and out as **the threshold every phase is compared
against**, after which the ordered `>` flags nothing.
**Fix, host-side and shared:** reject a threshold that is not ordered against
itself — treat a NaN return as "no threshold yet". Reproducing the object's
NaN is required; **the caller is the place to absorb it.**
**Test, and it is already half-built:** compare the return **as a bit
pattern** against 0xffc00000. `t_v90adid` already carries the row of six zeros
and compares as bits for exactly this reason, because `==` is false for a NaN
on both sides and would pass for ever.

**D331 — real, but the severity claim is REFUTED and should be struck.** The
divide at 0x47d5a is `1.0 / (double)maxSize` with `maxSize` starting at zero
(`xor %edx,%edx`, 0x47cf4) and updated only for phases whose guard byte at
`constelTable + 0x280c + k` is zero — so it can stay 0, and x87 gives
+infinity, not a trap. **But D331 says "the two truncations that follow yield
0x80000000 — after which each of the two doubling loops runs about 2^31
times", and both are 64-bit `fistpll` (0x47e13, 0x47e4e) whose readers take
the LOW dword, which for the x87 integer indefinite is ZERO — and both are
guarded:** `test %eax,%eax; je` at 0x47e28/0x47e2a and 0x47e65/0x47e67.
**Neither doubling loop runs.** The infinity propagates as a nonsense `dmin`;
there is no hang. This is the one claim in the family a reader would act on
and that the object contradicts.
**Test:** plant a nonzero guard byte in all six of `constelTable+0x280c..
+0x2811`, call `determineDminForRrn`, and observe that it **returns**. A hang
would refute the correction.

## The unreachable ones, each with its bound named

- **D185** — the divisor is `GenericToneDetector`'s tenth argument, and its
  only producer is `ANSamToneDetector`'s seventh, which is **the immediate
  `$0x32` = 50 written into the object's own `.text`** at 0xfaef and 0x2135a.
  Those are the only two construction sites, and the `C1` entry point D185
  cites has **no relocation naming it at all**.
- **D226 — real, and the bound is HOST-SIDE, which the register states as
  though it were the object's.** `vpcm_create` bounds its fifth argument
  ABOVE (`cmpl $0x30,0x40(%esp); jg 3c7f` — arg5 ≤ 48) **and not below**, then
  computes `blk = (int)trunc(((arg5 * 1000) / 9600) * 8.0 + 0.5)`. **Any
  `arg5 ≤ 9` gives `blk = 0` and the `div %edi` at 0x11278 faults with
  SIGFPE.** So "CANNOT FIRE — finding F1188 reads it as 40" reads as an object
  bound and is not one; a host passing a block count of nine or fewer takes
  SIGFPE during construction. Finding F1188 says this explicitly and **the
  entry understates its own citation.**
- **D228, D229** — the shared bound above: `V92_ECHO_FILTER_LENGTH` is 180 in
  `setToDefault` and no run-time path can change it.
- **D336** — computed rather than asserted: `(short)(45333 × 0.00075 + 0.5)` =
  `(short)34.4998` = **34**, and `n = 34 + shaperSR − 6` is **29** at
  `SPECTRAL_SHAPER_SR = 1` or **31** at the German PBX value 3. Reaching a
  negative `n` needs `RATE_FORCE < 8000` at `shaperSR = 0`. The entry's "needs
  only a small forced rate and a `shaperSR` under 6" is right about the shape
  and wrong about the shipped configuration. *(Both guards at 0x49a39 and
  0x49a47 are equality tests, so a negative `n` does count round the whole
  32-bit range — the entry says there is one guard and there are two, which
  does not change the conclusion.)*
- **D343 — no caller exists.** An exhaustive relocation sweep over every
  `.rel.*` section finds **zero** references to
  `_ZN24V90ConstellationDesigner19calcMtoMatchKtargetEff`. The symbol survives
  only because a non-static member has external linkage. Its trip count is
  exactly `2^32 − |v|` rather than "about 2^32".
- **D930's loop half** — `blockLength = 6 / shaperSR` with an explicit zero
  guard at 0x3286c, so it is zero only for `shaperSR == 0` or `> 6`.
  `V90SpectralShaper::reset` has exactly one caller (`V90Mapper::reset`,
  0x3025c) and `process` exactly one (`V90Mapper::process`, 0x30505), and
  `mappingParams->shaperSR` has only two writers, both from the two defaults.
  **`blockLength` is 6 or 2 and cannot be 0.** The entry's closing advice —
  "anyone linking this library for real should bound `shaperSR` to 1..6 at the
  parameter block" — is right and should be kept; what should be corrected is
  "the value is reachable from the parameter block", which is true of a
  hypothetical build with a real `Vparser` and false of the shipped object.

## D304 is now CLOSED, and this supersedes Family 3

Family 3 left D304 UNDECIDABLE and named the deciding evidence: *"where
`cid_modem` obtains the sample count it passes, and whether any path can make
it negative."* **That trace was done.** `CID_FSD_demodulate` has exactly one
reference in the object, `cid_modem` at `.text+0x9207e`, whose third argument
slot `0x18(%esp)` has three writers — zero at 0x91ccd, `FPM_MRF_filter`'s
unsigned-16 return at 0x91e32, and at 0x91dfa a value that `cid_modem`
sign-extends from its own count argument (`movzwl 0x1f4(%esp),%esi` at
0x91cc5, `movswl %si,%ebx` at 0x91cf4). So a negative count IS producible, at
a count of 0x8000 or more.

**But it cannot be the FIRST failure.** `cid_modem`'s prologue copies `count`
shorts into `0x40(%esp)` at 0x91cd3–0x91ce4, inside a 0x1ec-byte frame with
room for about 214 — so a caller passing 0x8000 destroys `cid_modem`'s own
frame long before the demodulator is entered. **DEFECT, UNREACHABLE**, and the
real host-side rule is the one the register already names: bound the block
length.

## The unclosed one, and it is named rather than guessed

**D297 — two transmit loops with no iteration bound.** The K56flex arm was
read (0xbb7c–0xbbbd): nothing counts turns, and the only exit is the transmit
queue reaching `f2aa0` at `jge bf38`. **Deciding it needs a return-path audit
of four callees** — `modulatevector`, `v34handshak`, `v90RateReneg`,
`v90RateRenegSilence` — establishing for each whether a return without
enqueueing is possible. That is four functions of nontrivial size and it is
the whole of the work; it was not attempted rather than guessed at. One point
cuts *against* the hang: the exit compare is **signed** (`jge` on two 16-bit
values), so a queue count that went negative would also exit.

## Corrections this family makes to the register

1. **D33's caller list is wrong.** It names `rxinit`, `agcadapt` and
   `adaptecho`; the object contains exactly **two** references to
   `updateAlpha`, both `R_386_PC32`, **both inside `modem_serrint`**
   (`.text+0x5d200` and `+0x5d560`). `adaptecho` calls the energy *producer*
   at 0x5db99, which is probably how it got in. The error is the entry's, not
   the finding's — finding F127 agrees on mechanism, range and producer, and
   makes no caller claim.
2. **D331's "each of the two doubling loops runs about 2^31 times" must be
   struck** — both are guarded, as shown above.
3. **D336, D337 and D338 all cite D345 for "nothing calls this member", and
   `setConstellationToNoise_forceRate` is not among D345's eleven names.** An
   exhaustive sweep finds two callers: `constellationDesign` (0x4cb27) and
   `process` (0x4cf3e). **DANGLING in the sense that matters** — it is the
   difference between "no call site can constrain the input" and "two do", and
   it is the same shape as D330's mis-citation of D345 in Family 6. **D345 is
   being cited as a general licence for "uncalled" when it is a specific list
   of eleven.**
4. **D226, D228 and D229 cite finding F879 for "slmodemd never supplies a
   parameter file", and 879 says nothing of the sort** — it contains no
   occurrence of `paramFile`, `slmodemd` or `null`. The real support is the
   `Vparser_read_*` stubs (finding F860), which is a *better* bound. Re-point
   the citation.
5. **Four defects found in passing that are NOT in the register**, recorded
   here so they are not lost:
   - `updateAlpha` at `energy == 0x80000000` exactly: bit 30 is clear so the
     normalisation loop runs, `add %edx,%edx` gives 0, and **0 doubles to 0 for
     ever** — an infinite loop at 0x5d5d8–0x5d5e2, inside the function D33 is
     about.
   - **`cid_modem`'s two unguarded `idiv` by the sample count** at 0x91d5c and
     0x91db7: with `arg2 == 0` the accumulate loop is skipped, `%ecx` is 0, and
     `idiv %ebx` is **0/0 → SIGFPE**. Integer, immediate, and reached by a
     zero-length block from `cid_progress`, with no guard anywhere above it.
     **This is more urgent than D304, which is in the same function.**
   - `cid_modem`'s signed/unsigned prologue copy at 0x91cd3 (the frame smash
     that closes D304).
   - `V90SpectralVerifier::C2`'s unguarded `fdivrp` at 0x45aca by
     `(double)SPECTRAL_VERIFIER_FFT_LEN` — float, so an infinity rather than a
     trap, and bounded by the default 1024 under the shared bound.

---

# Family 9 — decision devices, tables and fixed-point conversions

**Entries: D65, D250, D298, D299, D300, D301, D302, D326, D327, D328, D335,
D348, D349, D360, D363, D370, D371, D372, D451, D470.** All twenty reached.

## This family needed a fifth disposition, and the register needs it too

The four dispositions this document opened with do not fit most of this
family, and forcing them would have falsified it. Fourteen of these twenty are:

> **DEFECT, REACHABLE — NO FIX WARRANTED.** Real, reachable, faithfully
> reproduced, and **the right answer is not recoverable**.

That is not NOT A DEFECT — the object genuinely does the wrong thing. It is not
DEFECT, UNREACHABLE — ordinary input reaches it. And it is not FIX WARRANTED,
because **there is nothing to put in the fix.** D451 is the clean example: the
metric is never scaled, the decision collapses over the whole domain, and the
shift the author meant is not recoverable — the family uses 16, 15/16, 13 and
none. Inventing one would make a defect look like an implementation, which is
the worst outcome for a record whose value is its accuracy.

**The discriminating test is whether the correct value can be derived
independently**, and it is exactly the test D302 passed and D451 failed. It is
also what separates the one entry in this family that DOES warrant a fix.

## The one fix this document recommends: D250

**`MTD7_COEF_9600`'s numerator puts the notch's zeros at 1328 Hz while its
poles stay at 1477.** `MTD7_COEF_9600` at `.data:0x78da` is
{−13271, 16384, **16751**, **−21143**, 16384}. `a1 = 16751` is 1477.04 Hz at
9600 Hz; the matching `b1` would be **−18613**, and the object has −21143. The
zeros land 150 Hz away from the poles, so the section is not a notch at either
frequency. Behaviourally: **eleven of sixteen DTMF pairs mis-decode at 9600
Hz**, all eleven being 1477 Hz chosen-when-absent or missed-when-present; all
sixteen decode correctly at 8000 Hz.

**Why this one and not the other nineteen: the right answer is recoverable
four independent ways** — from the design formula, from the matching `a1`,
from the other fifteen tables (the closed form fits all of them to within
0.1 Hz), and from the 8000 Hz twin. Nothing is invented.

- **Evidence tier 3, unusually strongly corroborated.**
- **VERDICT: DEFECT, REACHABLE, FIX WARRANTED.** **The fix, named and not
  written: replace element 3 with −18613 behind `DSPLIB_REPRODUCE_BUGS`** —
  the D302 pattern exactly, and the sixth such arm in the tree.
- **Test.** `t_dtmfrx.c` already asserts the mis-decode count at each rate: a
  fixed arm must take the 9600 Hz count from eleven to zero while the
  reproduce-bugs arm keeps eleven.
- **Register hygiene:** the entry contradicts itself. Its batch line says
  "**it is MEASURED** … Finding F1416" and the `Status:` field on the same line
  says `unmeasured`. The measurement exists; the field is stale.
- **Citations:** findings F1413 and F1416. Both AGREE.

## Two more NOT A DEFECT

**D372 — "the region tree puts the same `rq` boundary in two different
places", and the inconsistency is real while the defect is not.** All four
sites verified: arms 1 and 2 cut at `rq ≥ 0x2d41` and `rq ≥ 0x16a0`, arm 3 at
`rq ≤ 0x2d41` and `rq ≤ 0x16a0`. **But 0x16a0 = 5792 is EXACTLY the midpoint
of the Q rows 4344 and 7240, and 0x2d41 = 11585 is EXACTLY the midpoint of
10137 and 13033.** A symbol on the line is genuinely equidistant from both
rows and both cells offer the same two I values. Enumerated: of the 21,182
`ri` values at `rq = 5792, ri > 11585`, the point index differs at **all
21,182**, the Euclidean distance is equal at **20,136**, the object is
strictly nearer at **1,046**, and strictly farther at **zero**. The same
argument covers the `_64pt` half, whose cut at 8192 is the exact midpoint of
4096 and 12288.

- **The misreading:** treating a boundary that sits on an exact perpendicular
  bisector as a right/wrong choice. **The object is never worse than the
  uniform convention and is sometimes better.**
- **VERDICT: NOT A DEFECT.** A genuine tie-breaking *inconsistency* between
  arms, worth keeping recorded, and it does move `*mag` and `*angle` — but
  neither convention ever names a farther point. **Re-mark 🐛 → ⚠**, and
  rewrite "the two never agree on the point", which is true of the index and
  misleading about distance.
- **Test.** For every `ri` in 11586..32767 at `rq = 5792`, compute the true
  squared distance to the point the object names and to the point base 0x18
  names; assert equality or object-nearer at every one.
- **Citation:** finding F3217. AGREES — its headline is "the boundaries that
  are one apart", which is the inconsistency and not a distance claim.

**D363 — `Detect_v22` passes `FPM_AGC_agc` a fourth argument it does not
have.** `FPM_AGC_agc` (0xa6750) has exactly one return, `0xa6894: c3 ret` — **a
bare `ret`, not `ret $imm`.** That is cdecl: the caller cleans up. The extra
push is popped by the caller's own stack adjustment, the callee never reads
the slot, and the return value is discarded at this site.

- **Evidence tier 2** — and this is the one entry in the family where
  disassembling a *different* function decides the verdict.
- **VERDICT: NOT A DEFECT.** The misreading is calling an ABI-harmless extra
  argument a defect. It is real evidence that the author's *declaration* of
  `FPM_AGC_agc` had four parameters, which is worth recording — but that is a
  fact about the source, not a defect in behaviour. **Re-mark 🐛 → ⚠**,
  alongside D366, which is the same shape at four sites and is already ⚠ ✅.

## D327 — DEFECT, UNREACHABLE, and its reachability line conflates two things

`convertEqualizerToMmx` computes `1.0/(beta·2²⁴)` and multiplies;
`setLinearEquBeta` divides directly. *(Read from the bytes: `de fc` at
0x373e2 is **FDIVP**, which objdump prints as `fdivrp` — `CLAUDE.md`'s trap
and `dis.py`'s Intel annotation both apply, and finding F2148 notes it too.)*
Both run at x87 extended precision — the `fldcw`s touch only the rounding
control — so **the two quotients differ by at most one 64-bit-mantissa ulp,
relative 2⁻⁶⁴ ≈ 5.4 × 10⁻²⁰**, and often by zero. That reaches
`linearEquMmxShift` only if `log₂(q)` sits within about 8 × 10⁻²⁰ of the
truncation boundary.

**The entry's "every pair that is not a power of two" conflates *the reciprocal
rounds* (nearly always) with *the shift differs* (essentially never).** And the
one structured case a reader would expect to break does not: the divisor at
0x373ee is `(float)log₁₀(2)`, whose error pulls an exact power of two about
1e-6 **below** the integer — vastly more than 5e-20 — so both forms truncate to
`k−1` identically.

- **VERDICT: DEFECT, UNREACHABLE**, bounded by the arithmetic itself. **Suggest
  🐛 → ⚠** and a rewritten reachability line.

## A defect found in passing that is NOT in the register

Chasing D327 turned up two more sites of D348/D470's shape. `maxK` guards its
truncation with `fadds 1e-6f`; **`convertEqualizerToMmx` (0x373ee) and
`setLinearEquBeta` (0x36571) have no epsilon at all** — the sequence goes
straight `de f9 ; fldcw (RC=trunc) ; fistpl`. So **both return a shift one
short for every exact power of two**, always, and neither is covered by any
entry. That makes the shape **four sites, three entries, one uncovered.**

## D348 and D470 are NOT duplicates, and the register's count of two is right

Both were checked for the duplication the brief asked about. They are
instruction-for-instruction identical — including the `fstps`/`flds`
round-trip of the divisor through a 32-bit float, and the `de f9` — and both
constant pools hold the identical triple {0.0f, 2.0f, 9.99999997e-07f}. But
they are **two distinct symbols at two distinct addresses in two distinct
classes**: `_ZN24V90ConstellationDesigner4maxKEP16V90MappingParams` at 0x47a10
and `_ZN15V90TRN2Designer4maxKEP16V90MappingParams` at 0x3ca30, each with its
own constant-pool copy. One defect *shape* at two *sites* is two entries, which
is what D363 and D366 already established. **What they should gain is a
cross-reference to each other; neither has one.**

The magnitude is exact rather than approximate: `(float)log₁₀(2)` is
0.3010300099849701 against 0.3010299956639812, a relative error of
**+4.757 × 10⁻⁸**, so an exact 2^k returns as `k(1 − 4.757e-8)` and the
absolute 1e-6 guard covers it only to **k ≤ 21.02**. Computed: 20 → 20,
21 → 21, **22 → 21**, 23 → 22, **36 → 35**, **42 → 41**, 48 → 47.
Downstream in D470's case, `word_0 = maxK − shaperSR + 6` is read back as
`1LL << (shaperSR + word_0 − 6)`, so the constellation is designed one bit
small and everything below is *consistent* with the smaller count — **a lost
bit, not an inconsistency**, which is why no test sees it.

## D349 — UNDECIDABLE, and the rule that keeps it so

`calcModulusParameters` at 0x3dd96 is the canonical GCC 64-bit variable shift
with `%ebx:%esi` = 1 and **no test of any kind on the count**; above 62 the one
lands in or past the sign bit of the signed `long long codewordCount`.
**Nothing in the object calls this member (D345 — and here the citation is
correct), so no caller's range is known.** By the same rule applied to D65,
**"no caller reconstructed" is not grounds for DEFECT, UNREACHABLE.** **What
would decide it:** a reconstructed writer of `V90MappingParams::shaperSR` other
than `spectralDesign`, or a reconstructed caller whose parameter block bounds
`shaperSR + word_0` into [6, 69]. Until then `unmeasured` is the correct
status and holding the differential count in 0..62 is the correct handling — a
shift outside that is undefined in the source language and the two compilers
may legitimately differ. **Citation: finding F3050 AGREES**, and states "The
shift count has no guard of any kind; D349" explicitly.

## Already dispositioned — D302

`FSE_decision_16pt` reads `DECv32_MAG9600` (`.data:0x74f8`, **6 bytes, three
entries**) at indices 4095/8191/12287 because `sar $1` stands where the sibling
`_16Tpt` has `sar $0xd`. **Fixed at `src/pump/v32/v32fse.c:500`** — `>> 13`
restored in the default arm, flat `*mag = 0` under `DSPLIB_REPRODUCE_BUGS`. Not
re-argued. **Citation note: finding F1603 is DRIFTED for its verdict and AGREES
for its analysis**, by the register's own words — so anything elsewhere citing
1603 for "cannot be differentially tested" is now wrong.

## The fourteen that are real, reached and not fixable

Each was quantified rather than asserted; the magnitudes are the point.

| entry | magnitude, measured | why no fix |
|---|---|---|
| **D301** `_4pt` weights I twice Q | the two rules **agree for every symbol within 7,606 counts of the nearest point** — 41.5% of the 18,317 spacing; within half the spacing only 2.7% disagree, within 7,240 none. `*mag` unaffected (all four points share `0x3299`) | the intended shift is not recoverable; the family uses 16, 15/16, 13 and none |
| **D451** `_16pt` metric never scaled | **structural and total**: all coordinates are ±4096/±12288, so at any exact point every squared difference is a multiple of 2²⁶, all sixteen score 0, and point 0 always wins | same |
| **D370** outer ambiguous cell returns the farther | over the cell's 334,396,082 rotated pairs the object is Euclidean-farther at **81.8%, not 100%** — because its own `>>13` metric wraps over 61.9% of the cell; restricted to the non-wrapping sub-region it is farther at 99.99%. Cost 2,281 counts = **25.06° of carrier phase**; `*mag` unaffected (14 and 26 are mirror images, both 18881). Entry cost: the nearest cell corner is 4,578 counts from both points, 1.58× the spacing — an already-lost symbol | the `setle` sibling recovers the *operator*, not a right *answer*, because the metric has wrapped where the defect fires |
| **D371** point 26's I literal is one high | `0x3e3a` = 15930 against `DECv32_ANA_IMAP128[26]` = 15929; flips 39,703 of 334,396,082 pairs, **0.012%**. New evidence: the whole rotated ladder is the **floor** of `k·2896.31` (1448, 4344, 7240, 10137, 13033, 15929 — all six floors), while 15930 is the **rounded** value. A generated table and a hand-typed constant | the object's value is the defect; reproduced at `v32fse.c:837` |
| **D298** MRF startup window | newest samples weighted by the phase's **oldest** taps; buffer's first 30 entries are zeroed so no uninitialised read; transient is thirteen outputs per stream, then never again | reproduced; `t_v22_mrf.c` compares from sample 0 |
| **D299** PPS startup indexes at `phase` | startup uses `coeff + 2·phase`, steady uses `coeff + 2·phase·hlen`; for `p` not a multiple of 3 the three taps straddle two phases. ~40 outputs. **The two branches of one function disagree with each other**, which is the whole claim and needs no layout argument | reproduced; a measured mutation gives 26 failing checks in each of three drive patterns |
| **D360** `V22_FSE_init` zeroes 49 entries twice | **zero** — both write 0 to the same 49 shorts and the second loop covers the whole 98 | nothing can distinguish the two forms; a source change no test can see |
| **D65** `FPM_log10` reads past its table | the 129th short is `FPM_PPS_CFG[0]` = **10**, and `10 >> 3 = 1` against a correct 0: **exactly 1 LSB of Q12 too high, 0.00244 dB** — smaller than D66's own error in the same function. Domain **127 of 32,767 mantissas (0.39%)**, not 64: 64 land there unshifted and 63 arrive by normalisation | reproduced, and the reproduction is the interesting part — the 129th entry is a transcription of `FPM_PPS_CFG[0]`, so the test does not depend on our linker |
| **D300** `FPM_atan` fourth-quadrant reflection | **exactly 1 count of a 0x8000 turn = 0.011°**, uniform over one eighth of the plane, no discontinuity. Likely motive the entry does not name: `0x8000` does not fit the signed short the other three arms produce, so `0x7fff` reads as an overflow dodge — the correct dodge being `(0x8000 − t) & 0x7fff` | one count, and the only caller in the object folds at 0x4000 immediately |
| **D326** empty filter prints 65536 | **nil in computed state**; `0x50(%esp)` is read once in the whole function, immediately before `edprintf`. **Evidence tier 1** — `"V90Equalizer: short high LE coeffs min value = %d\r\n"` | transcript only, but the printed number is genuinely impossible and a tier-1 label makes that the author's own claim |
| **D328** magnitude of −32768 is −32768 | **nil in computed state** — the array element is stored *before* the branchless abs, so only the printed min/max move. **Evidence tier 1** — `"V90Equalizer: Min LE History = %d\r\n"` | transcript only |
| **D335** zero threshold prints with a minus | **nil in computed state**; `FCOM` sets C0 for `0.0 < v` strictly, so `v ≤ 0` including zero takes `'-'`. Four sites, one shape. **Evidence tier 1** | transcript only, and the entry says so |
| **D348** | 22 → 21, 36 → 35, 42 → 41, 48 → 47 | see above |
| **D470** | identical, and one bit lost consistently downstream | see above |

## Corrections this family makes to the register

- **D65's load-bearing citation is wrong, and it inverts the paragraph's
  argument.** D65 cites **D4** twice for the lucky-adjacency story — "returns
  the right answer by coincidence (D4's entry)" and "`FPM_sqrt` sets the
  precedent by adding a 193rd entry (D4)". Both should be **D1**, which is
  `FPM_sqrt` and whose overrun lands on `FPM_div_table[0]` = 32768, exactly
  right. **D4 is `FPM_div`, the UNLUCKY one**, whose overrun lands on
  `FPM_xor_table[0]` = 0 where 16384 belongs. Citing the unlucky entry as the
  precedent for luck is precisely the drift `refcheck.py` cannot catch: both
  resolve.
- **D360 cites finding F3500 and the material is in 3501.** 3500 is "THE V.22
  EQUALISER IS ITS OWN BLOCK … AND ITS `fresh` FLAG MEANS THE OPPOSITE" and
  has no account of the two clears. *(Neighbouring, outside this family: D362
  also cites 3501, whose headline is about `V22_FSE_init`'s coefficient
  reversal rather than `FSEv22_decision12` — worth a look by whoever owns
  D362.)*
- **`src/` carries a comment that understates D370.**
  `src/pump/v32/v32fse.c:765` and `:844` describe it as a tie-break — "The
  FARTHER wins on a tie". **The code at `:844` is correct**
  (`e26 >= e14 ? 26 : 14`); the comment understates a wholesale inversion over
  99.996% of the cell. Comment-only drift in `src/`, and the register and
  finding F3218 are both right — **this document changes no source, so it is
  recorded here for whoever next edits that file.**
- **D370's own "at every one of them" needs a qualifier**: true of the
  object's own metric, true of Euclidean distance at 81.8%, because the metric
  wraps over 61.9% of the cell.

---

# Family 10 — the fourteen graded FIRES TODAY

**Entries: D73, D74, D76, D77, D78, D79, D80, D81, D82, D83, D84, D85, D86,
D87.** All fourteen audited.

Appendix B put these in **FIRES TODAY** — *"these change what a user sees, on a
call nobody has to construct"* — and asked whether anyone can HIT them.
**Nobody had asked whether they are defects at all**, and this is the one place
in the register where that question is most expensive to leave open: a
FIRES-TODAY 🐛 that is not a defect is a lie at the exact spot a reader looks
first.

## The structural result: the list double-counts, by about 30%

Before any per-entry work:

- **D76 = D4**, **D85 = D6**, **D86 = D16** — each says so in its own header
  ("that entry is the authority for the mechanism").
- **D74 and D77** are declared, in D77's own text, to be **"one defect — do not
  file them separately"**, and both are in the fourteen.
- **D86's authority, D16, is marked 💤 and says "Harmless in practice".**
  Appendix B promotes it to FIRES TODAY without touching that assessment or
  saying why.

**Fourteen FIRES-TODAY slots therefore hold at most ten distinct defects.**
Appendix B's grade is sound as a per-entry statement and misleading as a count,
and the count is what a planner reads.

## The verdicts

| entry | verdict | which side |
|---|---|---|
| D73 | **MISFILED** | neither — the object cannot distinguish 32 from 132 |
| D74 | **MISFILED**, and its FIXED status is not true of either host tree | host |
| D76 | DEFECT, REACHABLE (legacy pumps only) | object |
| D77 | **UNDECIDABLE** — the threshold is contradicted by live data | object mechanism, host workaround |
| D78 | **UNDECIDABLE — mechanism REFUTED**, citation drifted | neither |
| D79 | **MISFILED** | host |
| D80 | **DEFECT, UNREACHABLE** — bounded by a 16-entry table | object |
| D81 | UNDECIDABLE on reachability | object |
| D82 | DEFECT, REACHABLE — 1 of 50 countries | object |
| D83 | **UNDECIDABLE — the name is the host's** | object behaviour, host name |
| D84 | DEFECT, REACHABLE — documentation only | object |
| D85 | DEFECT, REACHABLE — **the entry's own prose is garbled** | object |
| D86 | DEFECT, REACHABLE only against a peer omitting V.21 | object |
| D87 | DEFECT, REACHABLE on an error path | object |

**Net: three misfiled, one mechanism refuted, one bounded and unable to fire on
any shipped configuration, four undecided, and six sound object defects** — of
which three are re-registrations and two of those were graded harmless or
legacy-only where they were first filed.

## The three misfiled, and two are exactly the predicted shape

**D73 — "`DP_V32BIS` (132) never connects".** `dp_v32_init` (0x4bb3–0x4bd8)
registers **id 0x20 and id 0x84 against the same `struct dp_operations` at
`.data+0x48`**. `v32_create` (0x4560) stores the `dp_id` argument at `dp+0`
(0x45cc) and **never reads it again**; there is no `cmp $0x84` anywhere in
0x4560–0x4bb0, and the 14400 ceiling is set unconditionally at 0x45ba
(`movl $0x3840,0x18(%esi)`) with the `MDMPRM_MAX_RATE` clamp at 0x4641 the same
0x3840. **The object cannot behave differently for 32 and 132.** The bench
symptom may be real; the object is provably not where it comes from.
**Test:** call `ref_v32_create` twice, with 32 and with 132, and diff the two
0x348-byte objects — anything but `dp+0` differing refutes this.

**D74 — "`MDMCTL_IODELAY` was a hard-coded constant".** There is **no mechanism
in the object**; the entry's own *Where* field reads
*"`slmodemd/modem_main.c`, D-Modem fork"*. Per the register's own preamble,
"anything that does not fit one of those… belongs in the issue list rather
than here". It should be merged into D77 as D77 itself instructs — **and its
status must drop from FIXED**, see below.

**D79 — "`GetDialToneFilterSubindex` is hardcoded to zero".** The object asks
and the host answers zero: `modem_homolog.h:94` carries
`//u8 DialToneFilterSubindex;` **commented out**, and `modem_param.c:172-173`
is a literal `return 0;`. `cadence_create`'s dispatch then takes each bank's
own fallback, and finding F49 measured bank 3 falling back to `CP_276_504` —
*its own nearest equivalent* — which it calls "deliberate and sensible, not an
oversight". **The entry convicts itself**: *"host-side — restore the field.
Nothing in the object needs to change."*

## D74 and D77: a three-way contradiction that must not be resolved by assertion

**D74 records the IODELAY defect as "CONFIRMED, FIXED" in D-Modem `09ca128c`,
`SLMODEMD_IODELAY`, default 120, and Appendix C item 1 repeats it. The fix is
not in either host tree.**

- `grep -rn SLMODEMD_IODELAY` over `/home/philpem/dev/sip-D-modem/` returns
  **nothing**. The string does not exist.
- `d-modem/slmodemd/modem_main.c:941-953` returns a **hard-coded 48**, with a
  comment computing `filtdelay = 47` — and that file is dated **six days after
  D74 was written**.
- `slmodemd/modem_main.c:682` still returns **0**.
- `d-modem`'s `main()` installs `socket_modem_driver` by default (line 1847);
  ALSA only under `--alsa`.
- **120 appears nowhere in `docs/findings.md`.** Finding F1026 — the tree's own
  recommendation — says **240**, with 216 as the conservative alternative.

That leaves three claims that cannot all be true:

1. the live socket driver reports IODELAY 48, so
   `filtdelay = ((48+6)>>2)+34 = 47`;
2. finding F1022 measures `filtdelay >= 57` (IODELAY 86) as the V.34 connect
   threshold;
3. Appendix C records live bench data — *"Five IDENTICAL V.34 calls at one
   setting: 4 of 5 connected, at 14400, 14400, 4800, none, 14400"* — and this
   repository's recent history is dozens of live V.34 calls.

**The most likely casualty is (2).** `t_v34link` is blob-against-blob over a
simulated wire, and finding F960 itself notes the mechanism depends on a
carrierless slicer producing all-ones against a peer that is **exactly**
silent, which a real far end is not.

**So: do not record "D77 fires on this deployment" as settled, and do not
record "D74 is fixed" at all.** D77's mechanism is real and was read out of
`.text` — arm 47 must sit out `0x5f - filtdelay` four-sample blocks, with the
counter at `+0xaa78` loaded at 0x6601f, compared against `0x5f` at 0x66027, and
bit 0x200 set at 0x66045 only on `>`. What is not established is the
**threshold**. **One line of a live-call log settles it**: the blob prints
`vpcm: Delays: HW %d, DMA %d` at debug level > 1 (which Family 2 shows is one
`--log` away), and `dp_vpcm_shim.c`'s create log prints `io_delay=%ld`. If HW
is 52 and the call connects, **D77's CONFIRMED/MEASURED status and its
Appendix C rank #1 both have to come down.**

## D78 — the mechanism is refuted, and the citation drifted the day before the entry was written

D78 says `MDMPRM_MIN_RATE` and `MDMPRM_MAX_RATE` "reach nothing" — that the
pair at params `+0x30`/`+0x34` is "what one debug `printf` reads and nothing
else". **They are not write-only:**

- `VPcmV34InitiateRetrain` at 0x66b5/0x66c5 divides both by 2400 (`imul
  $0x1b4e81b5`, shift 40) into `+0x21c`/`+0x220`, orders them, and clamps the
  max to **14** at 0x66fb–0x6709 — and 14 × 2400 = 33600.
- 0x6870/0x6877 pass the pair to
  `_ZN15K56FlexFloModem14setMinMaxRatesEii`; 0x6acc/0x6acf pass it to
  `_ZN24V90ConstellationDesigner14setMinMaxRatesEjj` (0x4ac00), which stores
  them at `this+0x50`/`+0x4c`. **The mangled callee names type the field** —
  evidence tier 2.
- `VPcmV34InitiateRetrain` is called from `V34GiveINFO1dBits` (0x8429) and five
  sites in `VPcmV34Progress`.

And the literals at `+0x38`/`+0x3c` — 4800 and 33600 — are **a different
quantity, not a lost copy of the host's**: `V90Parameters::setToDefault` reads
them (`mull 0x38(%ebx)` at 0x29971, `mull 0x3c(%ebx)` at 0x2997e, same ÷2400),
and **4800…33600 is exactly the V.90 upstream rate window the Recommendation
fixes** — V.90 §5 f) and §8.1, *"4800 bit/s to 28 800 … with optional support
for 31 200 and 33 600"*, and Table 9's *"bit 36:4800; …; bit 48:33 600"*.

- **Evidence tier 1** for the label (`.rodata.str1.4+0x5f8` =
  `"vpcm: VPCM rate limits: %d-%d\n"`) and **tier 2** for the consumers.
- **VERDICT: UNDECIDABLE FROM HERE, and the entry cannot stand as written.**
  What survives is only the weaker claim that a narrowed window has no
  observable effect — a *different* claim from the one filed. Two honest
  caveats: `K56FlexFloModem::setMinMaxRates` is **one byte at 0x101f0, a bare
  `ret`**, so that arm really does discard them; and finding F824's sweep saw no
  movement, which is *probably* because its baseline runs at IODELAY 0 — a
  configuration D77 says never completes phase 2 — but that reconciliation is
  inference, not measurement.
- **Test.** Re-run finding F824's MAX_RATE sweep at IODELAY 216 instead of 0 and
  watch `+0x220`. If it moves, the window is live.
- **CITATION: DRIFTED, and it was drifted on the day it was filed.** Findings
  F823 and F824 say what D78 quotes, but **finding F1020** (commit `2c98999f`,
  **2026-08-10**) explicitly corrects them — *"`configuration.md`'s note that
  the +0x30/+0x34 rate pair 'is what `vpcm: VPCM rate limits` prints and
  nothing else reads' is wrong"* — and **D78 was written in commit `7db93c88`
  on 2026-08-11.** This is the sharpest case in the register of a citation that
  still resolves and no longer means what it is cited for, which is exactly
  what `refcheck.py` says it cannot catch.

## D80 — the mechanism is described wrongly, and the correct one bounds it

**There is no dB-to-linear conversion and nothing wraps.** `cadence_create`
fetches parameter 0x27 at 0x7d492–0x7d4ad and hands it to
`Get_Detection_Threshold_Table` (0x7d4b8), which is **seven instructions at
0x7e2c0**: `ecx = 0x2d - p`, sign-extend, `movswl ThresholdsTable(%edx,%edx,1),
%eax`, `ret`. **An unbounded signed index into a table** — that is the whole
function. `ThresholdsTable` is `.rodata:0x6900` with **`st_size` 32, sixteen
entries**: 90, 92, 96, 97, 99, 102, 185, 188, 190, 250, 280, 285, 370, 390,
470, 560. Index 16+ walks into the neighbouring table; a negative index reads
backwards. Every number in finding F60's table reproduces exactly from this, and
`cadence_create` is the **only** caller.

**And the host's own data bounds it.** Parameter 0x27 = 39 =
`GetDialToneDetectionThreshold`, and across **all fifty** shipped
`homolog_params` tables, field 18 spans **31…45** — indices **14…0**. **Every
shipped country is inside the sixteen-entry table**; only index 15 goes unused.

- **Evidence tier 2** — the symbol's own `st_size` is the bound.
- **VERDICT: DEFECT, UNREACHABLE.** The missing bound is the object's and is
  real; "a country table **can** disable detection" is hypothetical, and **no
  shipped country table does.** The grade belongs in *NEEDS A CALLER OR
  CONFIGURATION NOTHING VALIDATES*, and the fix class "validate the parameter
  to 30..50" should read **30…45**, since 46 and above index backwards.
- **CITATION:** finding F60's numbers AGREE; its and the entry's *description*
  of the mechanism — "converts it to a linear threshold exponentially",
  "wraps" — is **DRIFTED**. It is a table index, not arithmetic.

## The six sound object defects, with their grades qualified

- **D76** — `FPM_div` reads `FPM_div_table` (`.rodata:0xc6a0`, `st_size` 256 =
  128 entries) at index 128 when `m = 0xff80`, landing on the **0** at 0xc7a0,
  so the reciprocal is zero and the AGC gain collapses. Sound, tier 2.
  **Two corrections:** finding F40 says it is **already fixed by default** with
  `-DDSPLIB_REPRODUCE_BUGS` restoring the zero — that is the preamble's
  *deliberate-fix* form, not the *opt-in extension* the entry's fix class
  claims. And reachability is **legacy pumps only** (nothing in V.34/V.90/V.92
  calls it), so "FIRES TODAY" means "on an `AT+MS=103/22/23/32` call", not on
  the V.34 path being debugged. Already driven: `t_spandsp_replay`, 116,954
  agreeing checks, both implementations losing lock at the same bit.
- **D82** — the calling tone's three constants are all in `GenerateCallingTone`
  (0x7e030): step `add $0x8ab` = 2219 at 0x7e0da, off `movl $0x41a0` = 16800 at
  0x7e07e, on `movl $0x1680` = 5760 at 0x7e0fb/0x7e12f, against a fixed 8000 Hz
  caller. V.25 §2.1 requires *"1300 Hz ± 15 Hz … ON not less than 0.5 s and not
  more than 0.7 s and OFF not less than 1.5 s and not more than 2.0 s"*. **At
  8000: 1083.7 Hz, 0.72 s, 2.10 s — all three outside. At 9600: 1300.2 Hz,
  0.60 s, 1.75 s — all three inside.** Sound. **Reachability qualifier the
  entry lacks: `CallingToneFlag` is 1 in exactly ONE of the fifty tables** —
  `params014`, `CZECH_REPUBLIC` — and the default country is USA. So it needs a
  shipped-but-non-default configuration *and* an originating call.
- **D84** — read directly at 0x5c100–0x5c19d: `%edi` is seeded 0x2000 at
  0x5c111, the first product accumulates into it, **0x5c161 `neg %ecx`**, the
  second product accumulates into the negated total, `sar $0xe` at 0x5c18d. So
  the real accumulator is `Σ₂ − Σ₁ − 0x2000` — **the rounding constant enters
  with the wrong sign** — while 0x5c194 seeds a fresh `+0x2000` for the other
  axis. Half an LSB the wrong way, every symbol, at both call sites. Sound;
  documentation-only. **Test:** drive `receiver` with a conjugate-symmetric
  input pair — correct rounding gives conjugate-symmetric output and this does
  not.
- **D85** — sound, and **the entry's own prose is garbled.** The object has
  `.data:0x780c AGC_DEF_BETA = {16384, 1638}` and
  `.data:0x7810 AGC_DEF_ALPHA = {16384, 32604}`, so the slow pair is
  1638 + 32604 = 34242 and the DC gain is 1.045; the correct sibling
  `.rodata:0xa0fc AGC_DEF_BETA_v21 = {16384, 164}` gives 164 + 32604 = 32768
  exactly. **D85 says the copies of `AGC_DEF_ALPHA` carry "16384, 1638" — in
  the object 1638 lives in BETA**, and ALPHA is 32604 in all three copies
  including the correct one. **Two self-consistent repairs exist and the entry
  names one as if settled**: `alpha → 31130` (finding F30's, because
  `FPM_TONE_detect` writes `31130*x + 1638*y` longhand) or `beta → 164` (the
  object's own v21 sibling — though `relocscan` shows that pair is
  **unreferenced**). Someone acting on "opt-in extension" would write a number.
  **Test:** drive `FPM_AGC_agc` at fixed input through the b103 config; 1.045
  confirms the defect, and the settling *time constant* discriminates the two
  repairs.
- **D86** — `V8UpdateModemParameters` at 0x74bd5–0x74beb: `test $0x40,%dl` is
  the V.23 test and is correct, and **`test $0x3,%dl` is the V.21 test, which
  takes in the stop bit at word bit 0** — set in every V.21 character — so it
  is never zero and the `andb $0xdf,0x1(%edi)` at 0x74beb is dead. `& 3` where
  `& 2` was meant. Tier 2, and the framing is corroborated the strongest way
  available: finding F75 decodes the object's own sequence words back to the
  V.8 octets `0xE0 0xC1 0x05 …`. **The FIRES TODAY grade overstates it**: the
  dead `and` executes on every call, but an *observable* difference needs a far
  end that omits V.21 from its CM, which is unusual — and D16, the authority,
  grades it 💤 "harmless in practice". **Test:** `t_spandsp_v8sock` with
  SpanDSP offering V.32 alone; our `cm->b1` bit 5 must still be set after
  intersection.
- **D87** — the real `V34SetINFO0dBits` at 0x8020 loads `0x248(%edx)`, tests it
  at 0x8030 and **returns at 0x8047 if zero**, writing `movw $0x1e,0x18(%ecx)`
  only past the guard. The inlined copy compares `dsplibs_debug_level` at
  0x718fc and then **executes `movw $0x1e,0x18(%eax)` unconditionally at
  0x71903**, before the `jbe` at 0x71909 that only skips the print.
  **Evidence tier 1**, and unusually: the two copies carry *different strings
  the author wrote for the same operation* — `"SetINFO0dBits  \n"` with two
  trailing spaces at `.rodata.str1.1+0x2c58`, against
  `"V90, setINFO0dBits\n"` — which is what proves they are two hand-written
  copies rather than one inlined. The same `+0x248` guard survives intact
  elsewhere in the same function (0x71924–0x7192c), so the field is not in
  doubt. **Grade qualifier:** `v90_receiver == 0` is normal for a plain V.34
  call, but the arm needs a **CRC failure in `DET_INFO`** to be entered, so
  this is an error path and belongs one grade below FIRES TODAY. **Appendix C
  already places it there** ("affects handshake robustness on a retry path"),
  so **Appendix B and Appendix C disagree about this entry.**

## The two left undecided on reachability, and why

- **D81 — "a loud busy tone is not detected".** The cadence detector's IIR
  cascade has about 42 dB of passband gain taken back out only at the end, so
  above ~8000 amplitude it wraps internally and the interval envelope stops
  being steady (6,995…17,438 at 12,000, against 5,267…5,355 at 8,000). The
  defect is real and measured; **the FIRES TODAY grade is not established, and
  the entry says so itself** — *"whatever holds the level down is outside this
  module and is not identified."* The host is `MFMT_S16_LE`; if G.711 decodes
  at the usual scaling a −10 dBm0 busy tone lands near 10,000 and it fires, but
  that scaling is not established. **Test:** log the peak sample magnitude at
  `CALLPROG_Progress`'s input over one real busy-tone call.
- **D83 — "`modifier_validation` is tested the wrong way round", and the name
  is the HOST's.** The polarity is exactly as filed — `AnalyseDialString` at
  0x7aac0 grades flag-zero as **1 (INVALID)** and flag-non-zero as **2
  (TOLERABLE)**, and twenty of the fifty tables set field 14. But **the only
  string this function prints is `"AnalyzeDialString: LAST_DIAL…"`, and nothing
  in the object names the flag.** "Wrong way round" rests entirely on
  slmodemd's field name `DialModifierValidation` — a host word from a
  reimplemented header, **not the original author's**, and therefore outside
  the register's own evidence order. Finding F52 applies exactly the right
  discipline to the other two flags ("the names are what mislead, not the
  code") and then declines to apply it to this one. Per `CLAUDE.md` — *naming
  something wrongly is worse than leaving it padded* — **this should be a ⚠
  observation with a neutral name, not a 🐛.** **What would decide it:** an
  original vendor header or driver naming parameter 33. If it is "modifier
  validation", D83 stands; if it is anything like "modifiers already
  validated", it inverts.

---

# The closing census

The original census was computed by a script over the verdict ledger and
register headings. Its historical cohort is retained below with the single
explicit D162 disposition update described at the start, not presented as
a fresh census of today's register.

| | count |
|---|---|
| 🐛 entries in the original register census | **232** |
| **examined here** | **118** |
| **decided here** | **98** |
| **left undecidable**, each with the deciding evidence named | **20** |
| not reached | **114** |

| disposition | count |
|---|---|
| NOT A DEFECT | **18** |
| MISFILED — real, but not a deviation of the object | **3** |
| DEFECT, UNREACHABLE | **36** |
| DEFECT, REACHABLE — NO FIX WARRANTED | **14** |
| DEFECT, REACHABLE, FIX WARRANTED | **26** |
| ALREADY DISPOSITIONED by an existing fix | **1** |
| UNDECIDABLE FROM HERE | **20** |

**Twenty-one of 118 — about one in six — are not defects of the object at all.** That is
the number this document exists to produce, and it is high enough that the
register's 🐛 count should not be quoted as a defect count again without
qualification.

## What was NOT reached, and what shape it is in

The 114 are named in full here so nobody re-derives the list.

> D1 D4 D6 D9 D11 D12 D13 D14 D15 D16 D17 D19 D21 D22 D23 D24 D25 D26 D27 D28
> D29 D30 D32 D36 D39 D64 D70 D71 D72 D75 D89 D90 D91 D92 D95 D97 D98 D99
> D100 D104 D105 D106 D108 D109 D110 D111 D112 D114 D115 D116 D117 D118 D119
> D120 D121 D122 D123 D124 D125 D127 D128 D129 D130 D132 D133 D134 D136 D137
> D138 D139 D140 D141 D142 D143 D144 D145 D146 D147 D148 D149 D150 D151 D152
> D153 D154 D155 D156 D157 D158 D159 D160 D161 D230 D252 D254 D260 D272 D273
> D274 D280 D283 D286 D305 D307 D320 D322 D325 D329 D339 D340 D342 D355 D920
> D923

**D920 and D923 are inside that list, and they are NOT unexamined.** They are
this document's own standard — read at the top, quoted in the method, and used
as the template for family 7 — and they were **deliberately not re-argued**
because they are already settled, already gated behind `DSPLIB_REPRODUCE_BUGS`
in `src/pump/v90/V92CP.cpp`, and already carry a full prose disposition. They
are the only two entries in the 114 of which that is true. Anyone grepping the
list for them should read "left alone on purpose", not "missed".

The rest are not equally unattended either:

- **11 are already behind a `DSPLIB_REPRODUCE_BUGS` arm** — D1, D4, D26, D27,
  D28, D29, D30, D32, D36, and the two above. Someone decided each was real,
  decided a fix, and gated it.
- **26 are in D1–D64**, the oldest and best-argued block, every one graded by
  Appendix A and most driven by a named test. They were skipped deliberately:
  the axis Appendix A supplies is the one that was missing there.
- **66 are in D70–D161**, all graded by Appendix B — but this is where the risk
  concentrates. **They are the task #98 sweep, derived from prose in
  `docs/findings.md` rather than from a fresh reading of the object**, and
  Family 10 audited fourteen of them and found three misfiled, one mechanism
  refuted and one bounded. **There is no reason to think the remaining 66 have
  a better hit rate than the fourteen that were checked.** Extrapolating the
  audited rate would put roughly a third of that block in need of correction;
  that is an extrapolation and is offered as a reason to look, not as a
  finding.
- **22 are elsewhere** — mostly the 2026-08 batches, which carry their own
  four-field preamble and are the best-conditioned unexamined entries in the
  file.

## What I would want next, in priority order

1. **Settle D77 and D74 with one live call.** The blob prints
   `vpcm: Delays: HW %d, DMA %d` at debug level > 1, which Family 2 shows is
   one `--log` away, and `dp_vpcm_shim.c` prints `io_delay=%ld`. That single
   log line resolves a three-way contradiction between a fixture threshold, a
   host that answers 48, and this repository's own record of dozens of
   completed V.34 calls — and it decides whether Appendix C's **rank #1** is
   real. It is the cheapest high-value measurement available and it needs an
   idle machine, not an analyst.
2. **Fix the two wire-reachable defects, host-side.** D94's V.8 collector and
   D256's DIL sample store are both reachable from a *conformant* peer, both
   with a quoted clause, and both are memory-safety defects rather than
   quality ones. D94 is the sharper of the two because V.8 §5.2 and §6.6
   permit an unbounded character count by design and §6 makes tolerating
   unknown octets mandatory.
3. **Answer D88's one question**, because it is cheap and it might be the third
   wire-reachable defect. Which of the tables at `+0xa48`, `+0xb48`, `+0xc48`
   is subscripted by the eight-fold shell-mapper sum? V.34 Table 10's largest
   M is 18, so the g8 domain reaches 136 against a 128-entry table, and a
   conformant 14 400/2743 connection would overrun it by nine.
4. **Audit the remaining 66 of D70–D161 as a block**, with Family 10's method
   and its hit rate as the prior. The right output is a corrected Appendix B,
   not new entries.
5. **Correct the four findings this pass convicted**, because a register
   correction that leaves the finding standing fixes the smaller half:
   finding F1233's step 4 (D176), finding F231's `movzwl` (D135), finding F1020's
   correction being absorbed (D78), and D65's citation of D4 where D1 is meant.
6. **Adopt "DEFECT, REACHABLE — NO FIX WARRANTED" as a register grade.**
   Fourteen entries currently carry a 🐛 and an implied to-do that nobody
   should ever do, and the discriminating test — can the correct value be
   derived independently — is already the test this tree applies without
   naming it.
7. **Renumber `D-V92DEC-1` and `D-V92DEC-2`** into the `D\d+` scheme, moving
   the five cross-references with them, so the register's own gate can see
   them.

## What would make this document wrong

Stated plainly, because a triage without one is an opinion:

- **Any of the "bounded by the only caller" verdicts** falls the moment a
  second caller appears. That covers most of the 36 UNREACHABLE rows, and each
  names its bound with an address, an immediate or a clause so the check is
  mechanical.
- **The destructor family's seven NOT A DEFECT verdicts** all rest on
  `_ZN12VPcmFloModemD1Ev` having zero callers and on each destructor call
  being followed by `sysdep_free` of the same register. One caller that
  destroys without freeing flips all seven at once.
- **The allocation family's disjointness claim** — that no allocation is both
  argument-sized and immediately dereferenced — was established by reading
  every size operand in the family. A failing allocator in
  `test/harness/runtime.c` would test it directly, and nothing in this tree
  can currently make `sysdep_malloc` return NULL.
- **D176's refutation** is the strongest claim here and rests on the object's
  own `.data` and `.text`: 612 bytes at 0x6760, a 0x24 stride, `dec %ebx` then
  `jle`. If `dataBase` is ever regenerated at a different length the argument
  must be re-run, not assumed.
