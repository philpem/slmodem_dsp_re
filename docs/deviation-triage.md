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
  0x12e90), citing findings 831 and 1226 respectively. Confirmed: not one
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
  between. The entry inherits finding 55's "the object itself is never freed",
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
  - **D181 — DRIFTED.** Finding 1245 establishes `+0x68`/`+0x6c` and the
    `reset` skip for `V92Precoder` exactly as cited, but D181's headline also
    names `~V92PreFilter` and attributes the same two offsets to it. The
    pre-filter's owned words are `+0x04` and `+0x08`, and its map is finding
    **1246**, which D181 does not cite.
  - **D231 — DRIFTED.** Finding 1322 is titled "A NULL GUARD CAN BE
    UNREACHABLE IN THE OBJECT ITSELF", is entirely about a dead null guard on
    `+0xaa0`, and concludes "**It is not a deviation either.**" The claim D231
    needs — the embedding at `VPcmFloModem+0x6124` — is finding **1333**, and
    it is uncited. (D231's other citation, finding 1272, AGREES.)
  - **D211 — PARTIAL.** Finding 1288 is a METHOD finding about how a
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
- **D175 cites finding 1230 for "reproduced as written", and 1230 does not say
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
finding 1427 records that `porcessFirstStudy` marks a phase suspected unless
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
  reachability field**, which reads `unmeasured` where finding 1427 already
  establishes the trigger.
- **Test:** run `porcessFirstStudy` on a smooth mapping, let it set `+0x280c`
  itself rather than forcing it, and confirm all six come out suspected and the
  next call reads the unwritten slot.
- Citation: finding 1427 AGREES, emphatically — **the finding is more definite
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
- Citation: finding 1439 AGREES.

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
  306, 333), records that an earlier draft said CANNOT FIRE on "three arms are
  a plausible complete set" reasoning and was corrected, and names its own gap.
  **Deciding evidence:** every writer of `V92MappingParams+0x10`, read at
  0x53da6 — a fourth value there makes this REACHABLE.
- **D281** — DEFECT, UNREACHABLE from inside the object, and the bound is
  positive rather than merely absent: **finding 1427 states explicitly that
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
consumption; D281 rests on the entry plus finding 1420. `isAltRbs`'s
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
- **Citation:** finding 73. AGREES — its body restates the claim verbatim and
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
- **Citation:** finding 1363. AGREES.

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
- **Citation:** finding 232. AGREES.

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
- **Citation:** finding 231. AGREES with the entry's own caveat — **and the
  finding is where the claim originates and does not account for the
  `movzwl`.** Like D176/finding 1233, the correction belongs upstream.

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
today. **Citation: finding 1366 — DRIFTED, harmlessly.** 1366 is headed *"THE
OBJECT DIVIDES IN ONE METHOD AND MULTIPLIES BY A RECIPROCAL IN TWO"* and is
about mean/variance arithmetic; the histogram material is in **finding 1363**.
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
**Citation: finding 1397. AGREES.**

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
| D162 | not an index at all — see the correction below | UNDECIDABLE on observability |
| D265 | a transmit-side constant slip (0x45 where 0x55/0x56 was meant); nothing subscripted by a received value | defect stands as recorded; misfiled |
| D266 | a fixed constant; wire-*triggered* by any CRC failure, but nothing is indexed | misfiled |
| D277 | caller behaviour. Its own non-entry hazard `vec[unpack[1]]` is bounded by Table 13's 72-bit Jd plus each storing state's cap | misfiled |
| D287 | the cursor is `short_8b00[phase][code]` in scan order — the object's own histogram, filled by D259's path; the `[-2]`/`[-1]` window is internal | misfiled; the negative-index read fires on every call |
| D288, D289 | a `short` produced inside the class; `ci` walks down from it as an `unsigned char` | misfiled |
| D323 | `linearEquLength` and `dfeWindowHalf` from `reset` / `setLinearEquEdgesFadingParams` — the parameter block | misfiled |
| D344 | `(which << 7) + i` inside the constellation designer. The recommendation bounds the CONTENT (Ucodes 0..127, §3.5, exactly the 128-entry row); what is unbounded is the walk, which stops only on a `signed char` sign flip at 0x80 | misfiled |

**A correction worth carrying, on D162.** ITU-T V.90 Table 13 makes bits 47
and 48 **two independent one-bit fields** — *"47 Size of constellation used to
transmit CP, E and SCR **during training sequences**"* and *"48 … **during
rate renegotiation procedures**"* — not "constellation size, 2 bits" as the
class header says. **So the uninitialised byte is a whole field, not half of
one**, which strengthens D162 rather than weakening it: an unwritten
`bits[48]` is an unwritten *rate-renegotiation constellation size*, not one
bit of a two-bit number.

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
**Citation: finding 129. AGREES** — it says three tables where the entry
heading says four, which is consistent: the fourth is the 529-entry grid in
`decodeDepth`.

**D93 — two halves, two answers.** `ApplyBulkDelay`'s `bulk_len` is a
configuration field with no wire path — misfiled, unreachable from this
family's question. `getbit` (0x5eaf0) reads a bit cursor over `word[10]` at
+0xaa3c with `crc` at +0xaa50, fed by `getMPrecvdBits`, so the underlying wire
object is a **V.34 MP sequence** and `v34handshak` hands the record to `getbit`
thirty-six times. **What would decide it: the identity of the message `getbit`
actually parses out of +0xaa3c.** Finding 227 describes +0xaa3c/+0xaa3e as
`info_caps`, a *rebuilt capability word*, not the raw received MP. If `getbit`
is ever pointed at a raw MP, MP is longer than 160 bits and this is reachable;
if it only ever reads the rebuilt word, ten words is a real bound.
**Citation: finding 227. AGREES.**

**D333 — `setConstellationToNoise`'s 128-byte staging array.** The span is
`arg5[k] − params->unnamed_360` with `arg5` an `unsigned char *`, so up to 256
against 128 bytes at 0xc0 in a 0x14c frame. **No bound applies because there
is no caller**: the entry records (D345) that nothing in the object calls this
member, so there is no caller range to check and no wire field reaches `arg5`.
**What would decide it: one caller.** Given one, the question reduces to
whether `arg5[k]` is a Ucode (≤ 127, and V.90 §3.5 makes it safe) or a count
or difference (up to 255, unsafe). **No test is possible until then** — driving
it directly measures the fixture, not the object.
