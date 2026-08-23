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
(findings 134, 2400, 3100, 3055). The habit that defends against it is the one
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
- **Citation:** finding 1239. AGREES.

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
- **Citation:** finding 1388. AGREES.

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
- **Citation:** finding 1443. AGREES.

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
- **Citation: finding 1233 — AGREES, AND THE FINDING IS WRONG TOO.** This is
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
- **Citation:** finding 1233. AGREES.

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
- **Citation:** finding 1233. AGREES.

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
across FAST/MEDIUM/SLOW, `+0x0f4` is one of the nine slots finding 878
measured as a float declared `int`, and `+0x0f0` being read twice is otherwise
unexplained — and the entry states plainly that nothing dissents.

- **Evidence tier 3 with a tier-1 exit named.** The entry declines to rename
  the field, on the ground that a usage inference must not sit among 291
  rule-1 measurements, and names what would settle it: a reader of `+0x0f4`
  printing it in a diagnostic. That is `CLAUDE.md`'s evidence rule 1 and it is
  precisely how finding 3527 retired `+0x074` and `+0x078` in the same class.
  **This is the correct handling of a strong inference and should be the model
  for the rest of the register.**
- **Verdict: no change.** Unreachable in this build because, per D900, no
  parameter file is parsed; a wrong slow-arm coefficient in any build with a
  real parser.
- **Test.** `t_v90loadparams` already compares the two logs entry for entry
  and fails if both calls do not go to `+0x0f0`.
- **Citation:** findings 861, 878 and 3527. AGREE.

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

## D162 — `V92Jd`'s constructor leaves one constellation bit unwritten

**DEFECT, UNDECIDABLE FROM HERE.** `V90Jd`'s constructor, otherwise the same
function, fills both `bits[47]` and `bits[48]` — from
`V34_PHASE4_CONSTELLATION` and `V34_RRN_CONSTELLATION` — and the header's bit
map calls the pair "constellation size, 2 bits". `V92Jd` writes a literal 0
into `bits[47]` and never writes `bits[48]`. A two-bit field with one bit
initialised and one inherited is the shape of a slip, and the sibling
comparison is the same instrument that made D920 convincing.

- **Evidence tier 2** (the sibling class and the header's bit map type the
  field).
- **Verdict: UNDECIDABLE FROM HERE.** **The evidence that would decide it is
  `packJdData`**, which is not written yet and may fill `bits[48]` before
  anything transmits the vector. If it does, this is not a defect at all; if
  it does not, an uninitialised bit reaches the wire and the entry becomes one
  of the most serious in the register.
- **Test.** Already correctly guarded: `t_v92jd.cpp` seeds the slot with
  varied bytes and compares the whole object, so a reconstruction that
  helpfully cleared `bits[48]` fails rather than passes. Leave it.
- **Citation:** finding 1223. AGREES.

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
- **Citation:** finding 1397. AGREES — the finding is this entry's text.
