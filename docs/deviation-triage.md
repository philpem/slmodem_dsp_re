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
