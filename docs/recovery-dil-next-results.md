# DIL continuation from the integrated checkpoint

Baseline `d9995470` follows the completed source-recovery campaign. Its fresh
full census is 1,091/1,852 exact and 122,715/720,125 original exact bytes;
whole-object semantic and raw completion remain different. The prior DIL
report's 1,086 census belongs to its earlier opening checkpoint.

Read `recovery-dil-results.md` before using this report. Its 67-cell ceiling,
producer-mode, header, segment, declaration and induction closures remain in
force. Hindsight's *Residual compiler-stage family batch* confirms those
closures and the integrated checkpoint; the source, saved objects and native
instructions are the authority for this continuation.

## Opening five complete-TU controls

The original V.92 info copy forms one destination base at 0x50a97, indexes it
with the same counter used for the nineteen-byte info array at 0x50aa0, and
reuses that base for indexed zero padding at 0x50ae0. Only its three-byte
middle copy has a destination pointer increment. The retained source's
initial RTL computes `infoAt + i + bits + constant` in each loop; saved
`09.loop` creates separate pointer induction for the first copy and countdown
padding. This is a multi-use base/induction boundary, distinct from the closed
array declaration and segment/helper index domains.

Cross shared info-base capture versus the existing expressions with original
even-fallthrough/odd-cold final parity source order versus existing odd-first
order. Four V.92 cells preserve array homes, info bytes, every loop bound,
read age, store order, diagnostics and the compiler profile. A captured base
is formed after the before-info diagnostic, matching the original reload.
Prediction: the first copy and padding share the indexed destination base,
while the three-byte tail retains its original pointer induction. A smaller
size gap alone does not validate that prediction.

Compile one untouched V.90 control with all RTL dumps at the same time. Its
purpose is to identify the earliest stage that speculates the nonpositive
integer conversion. No V.90 source/profile alternative is included yet.

Every unchanged complete TU must raw-reproduce the snapshotted final object.
Use the shared toolchain helper, complete retained flags and final bug-compat
macro. Preserve compiler/selected assembler identity, source/header/object
hashes, full RTL, native bodies, symbol/data/relocation metadata and all
nonexact changes. No strength-reduction option, arbitrary declaration order,
register constraints, padding or forced inlining is introduced.

```sh
python3 tools/recovery_dil_next_info.py --historical-headers \
  --baseline-dir build/recovery-dil-next-baseline \
  --domain docs/recovery-dil-next-results.md
```

## First result and separate mechanism crosses

All five opening cells compile and both baseline objects raw-reproduce.
The four V.92 SIZE gaps are 26/21/9/16 for existing, even-first, shared-base,
and joint source respectively. The shared-base control restores indexed first
copy and padding, but also turns the three-byte middle pointer loop into an
indexed loop. That incomplete graph is not retained.

The original middle loop independently increments its destination pointer at
0x50acf/0x50ad1 while indexing v92Info by the counter. Declare four follow-up
cells on captured-base source: explicit pointer for only that three-byte tail
versus existing indexing, crossed with parity order. Two are repeated controls
and two are new source candidates. Require the unchanged captured-base object
to raw-reproduce its preceding cell. Prediction: the tail regains its pointer
increment without sacrificing the other two indexed loops. No array-type,
declaration-order, register or profile axis is added.

The untouched V.90 full RTL gives a new compiler-stage boundary: UID247's
`fix:SI` of the nonpositive SF product remains in its branch through
`20.combine`; `21.ce2` moves it before product predicate jump228. Original
native code converts only after the predicate. This distinguishes a pass
transformation from the already closed source-product alternatives.

Declare twelve diagnostic source/option cells. V.90 crosses retained cached
product versus the previously measured SF-test/DF-positive repeated product
with four profiles: retained, no if-conversion, no if-conversion2, both options.
V.92 tests its retained source under those same four profiles as the paired
falsifier. Save every RTL stage and execute the compiler-selected assembler.
Prediction: suppressing the responsible if-conversion pass leaves conversion
in its original branch; the source cross tests whether positive re-evaluation
and late conversion require both boundaries. These options are diagnostic;
no per-file exception or global adoption follows from a local hit. Preserve
all nonexact changes, metadata, literal pools and relocations in the audit.

The four middle-tail cells have zero exact hits. The explicit pointer restores
all three original info-loop categories (first indexed, middle pointer, final
padding indexed), with SIZE9/16 at odd-first/even-first parity order. Equal
SIZE gaps hide the changed graph. Retain that candidate for review separately
from exact gains; do not replace the exact gate with this local classifier.

The twelve profile cells also have zero exact hits. No-if-conversion suppresses
V.90's ce2 speculation but puts the positive ceiling arm on fallthrough,
where the original puts it cold; no-if-conversion2 raw-merges the retained
profile. Repeated-product source raw-merges across all four options, and
V.92's four option cells raw-merge. No option is retained. The tested profile
family closes without a shared-profile or global-original-flags conclusion.

## Separate CRC loop-limit witness

Both originals preserve the frame quotient before adding one: V.90 stores
it at 0x4daaf and V.92 at 0x50bcc, then separately increments it for the
positive guard and for the outer countdown. Retained source constructs
`frames = quotient + 1; j < frames`, and its compiler discards that quotient
lifetime. This is an integer producer/consumer boundary, distinct from the
closed fifteen-stage movement and feedback-update domains.

Declare six complete-TU cells at retained flags. V.90 tests existing versus
`frames = quotient; j <= frames`; V.92 crosses that same limit representation
with existing info source versus the complete three-loop info-graph candidate.
Prediction: quotient retention and separate increment/guard/countdown agree
with both original bodies. Do not infer recovery from register names or a
smaller score. Bounds are mathematically equivalent over this descriptor's
finite extent; no reachability or alias contract is broadened. Close after
this domain absent another independently witnessed operation.

## Final explicit seven-bit pointer producer

The complete info candidate establishes a controlled producer mechanism: an
explicit pointer restores the original middle-tail induction while preserving
indexed neighbours. The first remaining V.92 segment loop still differs in
that same category: original 0x50440..0x5044a writes through a pointer,
increments it and decrements a trip counter; retained 0x420..0x42d writes
through a base/index and increments the index. Its original seventeen later
segment/DIL fields repeat the pointer/countdown family. V.90's original
segment loops instead retain indexed relative counters.

Declare six final source cells, all at retained flags. V.90 compares its
existing helper with explicit destination-pointer advancement for seven-bit
fields. V.92 crosses those two helper representations with existing versus
complete recovered info/parity source. This is a concrete pointer producer,
transferred from the independently controlled middle tail. It differs from
the closed relative/absolute index spelling controls and has V.90 as an
independent falsifier against generalising it to both siblings. Every value,
shift, field offset, separator, descriptor read and loop count is preserved.
No register/declaration/profile axis accompanies it. Prediction: V.92 obtains
its native pointer/countdown graph; V.90 tests whether that representation
wrongly removes its indexed graph. Audit every body, metadata item, string
literal and relocation before interpreting exactness. Finish this declared
family rather than expand equivalent pointer spellings after a miss.

The explicit seven-bit pointer controls restore V.92 payload pointer/countdown
loops, and its initial info-store register pattern agrees with the original.
They remain nonexact (SIZE234 without the info recovery, SIZE227 with it).
The original separator publication gives a distinct owner witness: after the
first payload loop it reloads incoming bits at 0x5044c and stores the separator
through base+literal at 0x50458. The control instead stores through its advanced
payload pointer, so payload induction alone does not recover the publication
boundary. V.90's original indexed payload graph is lost as predicted.

Declare six separate publication-owner controls on those raw-reproduced
pointer baselines: an advancing local payload pointer while preserving the
incoming bits owner for `bits[at + 7]`, versus the existing mutated helper
pointer. V.92 crosses this with the info recovery; V.90 remains the paired
falsifier. Three cells repeat preceding controls and three are new source
variants. Prediction: payload pointer/countdown remains while separators
regain their original independent root/literal accesses. No source/flag
permutation or general pointer-syntax search accompanies this new owner
boundary. A complete native audit must reject a candidate that merely fixes
the payload but retains the wrong separator owner.

## Common base producer after the pointer/publication recovery

The publication-owner candidate restores the original payload/publishing
categories and remains nonexact (V.92 SIZE6 without info recovery, SIZE3 with
it). Full residual review shows differences throughout sequence-bound
lifetimes, segment addressing and CRC allocation; SIZE3 is a length gap, not
three differing bytes. No declaration-lifetime hypothesis follows from it.

Original V.92 forms seq1Bits+seq2Bits at 0x50383 and keeps 52 plus each field
offset as a literal at pointer formation/publication. Current source captures
52+seq1Bits+seq2Bits as segmentAt, then publishes with field-relative literals.
The closed direct-index controls tested this algebra before the explicit
payload-pointer/independent-publication producer was recovered. That producer
changes the concrete helper boundary, so test one final six-cell cross:
V.92 existing versus recovered pointer/info/parity source, each with existing
segment-start capture versus sum-only capture and literal52 at every consumer;
V.90 existing source with those two bases as the paired falsifier. All other
arithmetic, widths, reads, declarations and flags stay fixed. Prediction:
original common sum/literal-offset accesses become explicit without adding
loads or changing publication owners. A raw merge closes this producer
hypothesis; no expansion into equivalent base spellings is authorised by it.

The sum-only source cross has zero exact hits and does not restore literal
payload/field-separator offsets (V.92 SIZE70/60). The inlined helper still
captures `at = 52 + sum + fieldOffset` once and uses at+7 for its separator.
Retained source remains unchanged by this losing base hypothesis.

## Final helper-parameter boundary control

The remaining snapshot is the helper's at parameter, separate from its new
payload pointer. Original first-field pointer formation uses literal52 with
the common sum at 0x50435, and its separator uses literal59 with that same
sum at 0x50458. The helper captures the sum+literal as one dynamic at before
both consumers. Old direct-region controls used indexed payloads; this
control removes at while preserving the newly demonstrated explicit pointer
and independent publication root.

Declare exactly four complete-TU cells: V.92 recovered pointer/info/parity
helper baseline versus sixteen direct ordinary pointer regions; V.90 retained
source baseline versus the same direct-pointer region producer as falsifier.
Each region keeps the descriptor load, signed value shift, seven writes,
trip count, fixed offset and independent separator expression. Runtime DIL
fields keep their existing helper. New lexical blocks provide the ordinary
local scope formerly supplied by each inline invocation. There is no source
order, declaration, counter-spelling, register or option axis. Close this
producer domain after these cells even if the compiler folds it back; no
further nearby spellings follow from a smaller gap.

## Retained partial and finite closure

The direct-pointer control restores literal field offsets but still does not
recover the original common-sum producer. It holds bits+seq1Bits in one
carrier, reloads seq2Bits before fields, and retains segmentAt for the eight
inter-frame zero publications. Original code holds seq1Bits+seq2Bits once.
V.92's direct body is SIZE76 and V.90's is SIZE224. The prediction of original
base ownership without added loads fails. These direct-region candidates are
not retained, and the helper/base/declaration/induction spelling domains close.

The retained change is the idiomatic V.92 helper with an advancing payload
pointer and preserved publication root, together with the independently
witnessed info region and parity CFG. Its complete object is raw-identical to
`recovery-dil-next-publication/V92DILdesPCK/info-1-owner-1/candidate.o`:
`ac3dc1224a896f4f0fd1fc55bc666453e661f0482549fa6c5891a1e864636e99`.
V.92 is 4,163 bytes against 4,160 (SIZE3), versus the opening 4,134 (SIZE26).
The length gap is unrelated to the number of differing bytes. V.90 source
and its 3,326-byte SIZE48 body remain unchanged. There are no new exact bodies.

Recovered native categories are independently checked: eighteen seven-bit
pointer/countdown backedges; sixteen fixed descriptor-field payload pointer
and independent indexed post-loop publication pairs; first info copy indexed,
three-byte tail pointer, final padding indexed; final even fallthrough and
odd cold target. This is a bounded shape classifier with four positive/refusal
controls, original-body positives and the retained opening body as a real
negative. It neither proves semantic equivalence nor changes exact grading.
Source spellings, original types and unavailable original RTL remain unproved.

| Complete-TU domain | Compiles / shared verdicts |
| --- | ---: |
| Shared info base / parity, plus untouched V.90 RTL | 5 |
| Three-byte tail pointer / parity | 4 |
| Paired source / if-conversion options | 12 |
| CRC quotient-limit / info | 6 |
| Explicit seven-bit payload pointer / info | 6 |
| Preserved separator publication root / info | 6 |
| Common sequence sum / producer | 6 |
| Direct pointer regions / paired falsifier | 4 |
| Total | 49 |

The forty-nine valid compiles cover thirty-one distinct source/profile cells,
twenty-three distinct complete-object emissions and fifteen raw-reproduced
baseline controls. All forty-nine shared verdicts remain nonexact. Every
body, function inventory/export/binding, named storage, allocated nontext
content and nontext relocation is audited. Reordered diagnostic pools retain
the exact original literal multiset. The selected V.92 object's text
relocation target sequence also agrees with its opening baseline. No per-file
or global flag is retained.

The deciding final period run passes 2/2 binaries, twenty groups and 3,345
checks: t_dilpack 1,837 and t_v92dilpack 1,508. They include full-buffer
component differential checks and standards-oracle/fixed-answer groups.
Descriptor lengths stay within their 128-byte fields and DIL count covers
0..255 with valid guards; no public modem or overlapping-buffer reachability
claim is added. The final period object raw-matches the independent selected
candidate. Seven detached mutation locators are retargeted to the same
separator, info fill and parity alternatives; no cases or snapshots are added
or removed. Static checks cover three suites/68 locators with zero skips,
nonunique anchors, vacuous replacements or wrong-arm locators. No mutation
case or fuzz campaign is executed.

The full-object audit uses all 300 frozen d9995470 objects. The parent's live
FixedRC object had changed for the concurrent iteration; it is restored from
its raw-reproduced archived control before all 300 input hashes are checked
against recovery-final/object-audit.json. Only then is the selected V.92
object substituted. The other 299 objects and configuration are unchanged.
The canonical final census passes its ratchet at 1,091/1,852 exact and
122,715/720,125 exact original bytes, preserving every member with zero gains
and zero losses. Input hashes are checked again after that census.
The earlier attempt using the live directory is invalid/excluded, including
its incomplete census output; it cannot establish an exact-set loss.

Both independently measured strict semantic/raw completion gates remain
DIFFERENT/exit1. Faithful .text changes 711,836 to 711,852 bytes and positioned
allocated equality changes 70,072 to 69,984 of 943,398 reference bytes. Exact
symbol records stay 421/2,931 and target-aware relocations stay 988/18,317.
These layout results establish no aggregate byte-identity gain. The before
partial object's hash is checked against the archived final checkpoint; the
after object is linked from the complete verified 300-input manifest using
the period binutils image. Parent integration still owns its final full phase,
combined census and partial link.

Reproduction and evidence:

```sh
python3 tools/recovery_dil_next_audit.py --self-test
python3 tools/recovery_dil_next_audit.py \
  --output build/recovery-dil-next-evidence/audit.json
make period T='t_dilpack t_v92dilpack' J=8
python3 tools/anchorcheck.py dilpack dilpackstd v92dilpackstd
```

Each `recovery_dil_next_*.py` generator runs with its declared domain and
historical-header flag, using build/recovery-dil-next-baseline except tail,
publication and direct-pointer controls, whose named baseline directories
hold the preceding independently compiled cells. Their per-cell commands,
source/header/object hashes, identities, saved assembly/preprocessed source
and every RTL stage remain in build/recovery-dil-next-*/. Aggregate audits,
300-input hashes, strict comparison/status reports and the seven locator
before/after records are in build/recovery-dil-next-evidence/. One initial
static-check invocation used an unregistered suite name; its skipped result
is preserved in the invalid-selection log and excluded from validation.

The next discriminator requires evidence outside these closed spellings:
trace the initial sum producers and their GIV reductions through 09.loop,
with the original single sum plus literal publications as a constraint; for
V.90 retain the established 20.combine-to-21.ce2 conversion boundary and
original indexed payloads as separate constraints. A new source/control
needs an independent shared-use or lifetime witness explaining both sides,
not a register-name match, a smaller SIZE number or unavailable original RTL.
