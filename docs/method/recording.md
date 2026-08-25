# How to write the record

Every number cites `docs/findings.md` in this tree.

The source can be rebuilt from the object by anyone with the time. The
*reasons* cannot, and neither can the list of what was tried and did not work.
So the record is the deliverable, and a rule that keeps the record honest is
worth as much as one that keeps the code correct.

Every rule below is one this tree got wrong first.

---

## Name what the object names, and nothing more

A field at +0xaa7c is called `filtdelay` because the blob prints

```
  "On RX_PHASE1_ANS: is short=%d, bulkDelay=%d, filtDelay=%d"
```

and the `movswl 0xaa7c(%edi)` sixteen instructions earlier is what pushes the
third of those. Name, width and signedness all come out of the object
(finding F633).

Its neighbour at +0xaa78 takes **243 accesses**, more than any field in the
object except the three state words, and six microstate arms bump it. It is
called **`faa78`**, not `counter`: what it counts differs per arm, and a name
saying "counter" would read as measured when only the width and the sign are
(finding F633).

The same rule applies to a type. Two fields hold 32-bit values that are stored
with addresses inside the object and then dereferenced at offsets matching a
known structure — strong evidence, and still not taken, because the region
they point into has two readings this tree holds equally and one file's own
use sites cast it three different ways in different arms. They are `void *`,
following an existing precedent for an honestly under-claimed pointer
(finding F634).

> **A wrong evocative name is worse than an offset, because it will be
> believed.** An offset is visibly a thing nobody has explained yet.

## Record adjacency as adjacency

`sizeof(struct v34_detector)` is 0x24, the detector sits at +0x3564, and
0x3564 + 0x24 = 0x3588 — exactly where the next measured field starts. Two
independent readings meeting is worth writing down.

It is **not** a proof, and the struct was not embedded. The 0x24 is *our*
declaration's size, not anything the object states, and **a displacement is
not a size** — the largest offset reached does not end a field and neither
does the next one starting (finding F215). Embedding would have converted a
consistency check into a claim about 36 bytes nothing in that batch tested.
The padding array was shortened to `[0x3588 - 0x3564]`, the tiling was
recorded as tiling, and the claim was not made (finding F630).

## Say what you did NOT do, in the same breath

Twelve object spans became named fields and **no source file outside
`include/` changed at all**. That is not an unfinished rename: `hs_get`,
`hs_put` and `hs_setstate` take the offset as a **runtime argument**, because
one function serving all three state machines is the entire point of them, so
there is no field for a field access to name — and changing the signature to
take a member pointer would change codegen to buy a spelling (finding F632).

Written down, that is a decision. Left out, it reads as half-done and the next
batch finishes it.

The other half of the same discipline is *proving* the non-change rather than
asserting it. For that batch: 217 `.text` sections byte-identical before and
after, all 90 objects the period toolchain builds byte-identical, the codegen
comparator reporting the same figures either side, `make offsets` holding 905
annotations against the compiler, and `sizeof(struct v34_object)` identical
(finding F636) — then all 48 mutation suites re-run, 2,874 mutations, every
verdict compared **by name**, zero changes (finding F638).

## Do not quote a number you cannot reproduce

One finding said "sixty-one of the seventy-nine were left". **Both numbers
were wrong and the ratio should not have been quoted at all** (finding F639).

The denominator came from scraping every `#define <PREFIX>_NAME 0xNNNN` out of
two source files and resolving each against the struct: 73 distinct values, 24
of which resolve to a named field after that batch — and **at least five of
those 24 are not object offsets at all.** Two are counter *limits* a getter's
result is compared against, one of them `0xc7`, which is the number 199; three
are offsets into a different structure, which a field-resolver pointed at this
one will happily resolve to whatever happens to sit at that byte. The
denominator is noise, so the honest figure is the numerator alone: **twelve
spans became fields**, each of them named (finding F636).

> **An unreproducible ratio is worse than no ratio.** It is the same disease as
> the stale counts a keyed snapshot was built to cure: a number nobody can tell
> is stale is worse than no number, because it gets quoted (finding F545).

Three of that correction's four items are the same failure in different
clothes — **a number quoted in a wider scope than it was measured in.** "The
same seven NOT CAUGHT" was a subtotal over the suites pinned to one source
file; tree-wide the figure is 37, in ten suites, and always was (finding F639).

## Re-read a derived document when a finding it cites is AMENDED

New findings pull a derived document forward: somebody notices the material is
missing. **Corrections to old findings do not**, and nothing detects it. A
distillation happens once, at a moment, and freezes whatever the record said
then; amending the finding afterwards leaves the document saying the original
wrong thing, with a citation that still resolves.

This set printed a timing table taken from a finding that had been corrected
two commits before the set was merged, and the same stale number had reached
three of its files in one pass (finding F558). `refcheck.py` confirmed the
citation resolved, because it did — to a finding whose numbers had changed
underneath it. That is finding F543's shape in a second place.

Updating for new material is the obvious job. Re-reading against amendments is
the one with no trigger, and reading is the only method available for it.

## Correct forward; never rewrite the claim

Two mechanisms, one rule.

- **A new finding, when the commits carrying the error are pushed.** One entry
  is four corrections to seven earlier ones, written as its own entry for
  exactly that reason. It says what each said and what is true, and it names
  the one correction that would have misled a reader applying the project's
  own rule about reading codegen (finding F639).
- **A forward-pointing header, when the finding still needs reading.** Two
  findings whose predictions were later disproved carry `AMENDED BY ...` and
  `CLOSED, AND THE PREDICTION IN THE LAST PARAGRAPH IS WRONG` blocks at the
  top, with the original text left underneath, intact (findings F570 and F574,
  amended by findings F650 and F651).

Both leave the wrong claim legible, which matters because a branch cut before
the correction is going to be reading it.

## A finding that records a failed attempt is worth as much as one that records a success

And it has to be written as a failure, not as a postponement.

- **A tier that could not be run is a gap, not a pass.** The codegen ratchet
  was part of one batch's acceptance criterion and did not exist in that
  branch's history at all. The rule it was meant to enforce was followed by
  hand and **recorded as the weaker guarantee that is**, rather than reported
  as a passing check (finding F554).
- **A prediction disproved is the measurement.** One prediction said eleven
  uncaught mutations would fall to one. The batch asked to test it found the
  suite could not be run in that configuration at all, said so, and measured
  what was reachable instead — five of the eleven, with six unreachable by
  construction (finding F570). A later batch finally ran it: **eight flipped,
  and both predictions were wrong in opposite directions** (finding F651).
  Three findings, one question, and the record shows the whole path rather
  than the answer.
- **A detector that could not find the defect it was written for**, through
  four consecutive versions, each of which reported confidently (finding F618).
- **Four candidates, four false positives, each for a different reason**
  (finding F619).

The corollary for the tree's own instructions: *if a function cannot be made to
pass, leave it out and record the attempt.* An absence with a reason beside it
is a result. An absence without one is indistinguishable from nobody having
looked — which is `gates.md`'s pattern, arriving in the prose.

## Write citations in the form your checker parses — and show the audit fires

This tree's checker resolves `finding N`, `findings N, M and K`, and a bare
`DN`. It **cannot** resolve a bare `(N)`. Such a citation is invisible to it:
never reported dangling, and silently re-pointed by the next renumber, reading
perfectly all the while. The first version of `gates.md` listed six citations
**by bare number in a table column** — inside the table that teaches the rule
(finding F670).

Two spellings look like citations and are not, and the second is worse because
it looks *more* careful than the form that works:

- a bare `(N)` — no word, so nothing to match (finding F670);
- **the word present but separated from the digits.** The pattern is
  `\bfindings?\s+(\d+…)`, so bolding or backticking the number — writing
  `finding` followed by the digits wrapped in `**` or in a code span — reads
  to the checker exactly like the bare parenthetical. One was caught by the
  audit below, in this file set's own draft; `refcheck.py` reported zero
  dangling either way (finding F690).

The audit that missed them is the part worth copying. It searched for
`finding[s]? N` and reported every hit resolving correctly: a detector that
cannot distinguish a clean file from a broken one, which is finding F134's
argument arriving inside the tool written to enforce it. The version that
works matches **every** `\b\d{3}[a-z]?\b`, subtracts the ones already carrying
the word, and the remainder is read by hand (finding F670) — the two-pass
procedure finding F543 used, for the same reason. What is left in these files
is quantities, and they are the reason a bare-number rewrite is not available:
`1,670` diagnostic call sites, the filter coefficients `340`, `348` and `361`,
and every measured count in the set (finding F543).

Two further traps, both met:

- **The checker walks *tracked* files.** A new document that has not been
  `git add`ed is invisible to it, so a clean run over a set that excludes the
  newest file is finding F540's skipped suite arriving in the deliverable
  (finding F670). Add the file, then check.
- **A miscitation in the record propagates into briefs.** One finding cites a
  number for a measurement that is recorded in the next finding along, and
  cites it as a bare parenthetical, so nothing could report it; the brief for
  this update relayed it onward as measured (finding F690). The record is more
  reliable than a brief and is not infallible.
