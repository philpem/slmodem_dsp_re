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
