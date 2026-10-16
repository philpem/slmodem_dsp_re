# CP operand and lifetime follow-up

Baseline `d9995470`, following the validated 1,091/1,852 exact-name checkpoint.
Work is restricted to the complete V90CP.cpp TU. The retained CP encoder is
2,716 bytes against the original 2,785 (SIZE69). Its existing fixed payload
regions and paired-word/buffer read ages remain fixed. The closed domains in
`recovery-bitpacker-results.md` are not a licence for nearby type, declaration
or loop-spelling searches.

## First independent operand witness

The first remaining structural region in physical emission order is the
encoder's inlined CRC. Its original byte feedback is the unmasked sum until
the final stage publication. This differs from the retained early-masked
feedback even though the two are equivalent modulo two at every output.
The two standalone CRC methods independently prove the same operand graph:

| Original method | Raw byte sum | Tap masks | Final feedback mask |
| --- | --- | --- | --- |
| infoToBits | ADD at 0x523d7 | AND at 0x52413 / 0x5244e | AND at 0x5245a |
| calcCRC | ADD at 0x513c7 | AND at 0x51403 / 0x5143d | AND at 0x51449 |
| evaluateCRC | ADD at 0x5184c | AND at 0x51871 / 0x518c2 | AND at 0x518ce |

Each original keeps the same feedback byte between the two masked tap adds;
there is no earlier AND on that carrier. The retained encoder instead masks
at 0xf06 before either tap use. All three retained source copies contain the
same early-mask expression.

F7414 measured the MP calcCRC/evaluateCRC late-mask correction and described
the CP twin's *source* as early-masked. It did not demonstrate an original CP
early mask. The later `batch100-v90-crc-shift-domain.md` explicitly preserved
that CP expression while testing shift grammar in the standalone methods.
The thirty-two-cell CP/MP recovery tested fixed regions and read/publication
boundaries, preserving feedback masking. Thus this original CP operand-use
witness is distinct from those closed source families. F4601's historical
claim that the standalone CP gap is only allocation omitted this difference.

## Declared first domain

Compile exactly four complete-TU cells: unchanged; late mask in infoToBits;
late masks in calcCRC and evaluateCRC; and both axes. Each enabled method
changes only these two statements:

```c
a = (unsigned char)(crc[0] + bits[i]);
/* Existing fixed stage assignments and individually masked tap stores. */
crc[15] = (unsigned char)(a & 1);
```

The prediction is removal of the feedback mask before either tap, with an
explicit mask at the final feedback stage, matching all three original
consumers. A surviving early mask, changed tap semantics or unaccounted
collateral falsifies the proposed source recovery. Size and exact-name gains
alone do not establish a unique original spelling. No CRC polynomial, byte
width, frame skip, count, source order or compiler option changes.

Use the full retained Gentoo profile and shared experiment-toolchain helper,
with DSPLIB_REPRODUCE_BUGS appended after configurable flags. Require raw
reproduction of the unchanged complete TU against the archived production
object before interpreting candidates. Audit every emitted body, including
exact neighbours, all named/allocated nontext data, binding, visibility and
relocations. Protect the entire 1,091-name exact set with the frozen complete
300-object census. Run focused period component tests before source retention;
the parent campaign owns the final integrated phase.

The baseline under `build/recovery-cp-next/baseline/` is a real copy of the
validated complete object/config set, not hard links. Existing worktree build
artefacts remain separate. Commands and selected compiler/assembler identity,
source/header hashes, complete objects and RTL streams are recorded under
`build/recovery-cp-crc/`. Replay:

```sh
python3 tools/recovery_cp_crc.py --domain docs/recovery-cp-next-results.md \
  --baseline-dir build/recovery-cp-next/baseline
```

A distinct, untested next witness is original encoder CRC-bound ownership:
0x522df..0x522eb reloads the saved bodyLength value and subtracts 0x11 after
CRC initialization; the retained encoder reuses the earlier start cursor.
This needs its own bounded control after the first domain, rather than an
adjacent spelling sweep. Original and retained full disassemblies remain in
`build/recovery-cp-next/`. The initial bbalign invocation supplied `--ours`
outside anchor mode, where this checkout ignores it; it measured nothing and
is excluded. The corrected invocation selects the archived directory with
TC_OUT and reports 691 original / 689 retained non-padding instructions.

## First result and published-bound domain

The unchanged late-mask control raw-reproduces its full TU; all four cells pass the complete
metadata/data/binding/nontext-relocation audit: sixty shared-body verdicts,
zero exact gains and losses. Encoder size remains 2,716; calcCRC moves from
582 to 584 versus original 570, and evaluateCRC remains 684 versus 668.
The original unmasked feedback use is restored independently of those gaps.
All nine exact CP neighbours remain exact. The helper-axis controls also
change the source-unchanged, already nonexact bitsToInfo body; its complete
native instructions and relocations are retained as collateral.

Declare exactly two complete-TU cells for the independent bound witness:
all three late-mask corrections as control, and a local unsigned end captured
from bodyLength - 0x11 after CRC initialization, replacing the encoder loop's
earlier start-cursor bound. This uses the standalone provider's existing
slot/use contract and the original encoder's post-initialization subtraction
at 0x522df..0x522eb. The prediction is restoration of that subtraction/read
dependency. If the object is unchanged or reuses the earlier cursor instead,
the source expression alone does not explain the original boundary; close
this axis without adjacent declaration or spelling trials. All other source
and flags remain fixed. The unchanged TU must raw-reproduce the preceding
all-late cell before interpretation.

```sh
python3 tools/recovery_cp_bound.py --domain docs/recovery-cp-next-results.md \
  --baseline-dir build/recovery-cp-bound-baseline
```

## Published-bound result and four-capture domain

The two bound cells reproduce the preceding all-late object raw and pass
thirty complete body verdicts plus metadata/data/nontext-relocation checks.
The candidate restores the original ADD 0x11, saved body length, reload and
SUB 0x11 dependency after CRC initialization. Its encoder is 2,732 bytes
(SIZE53), with zero exact gains/losses and no changed sibling body. This is
an independently recovered operation graph, not a unique source spelling.

The remaining original count-capture region at 0x526d7..0x52701 consists of
four literal loads from this+0x48/0x4c/0x50/0x54. Each supplies a mutable
t-array entry and an independent bound for its later fixed payload region.
There is no capture backedge. Current code still emits a four-iteration loop
at 0x11f9..0x120d with eight indexed source loads. The previous topology
domain expanded the four/six *payload* regions while leaving this distinct
count-capture loop untouched.

Declare exactly two complete-TU cells: the fixed late-mask/published-bound
control and four literal `t[k] = n[k] = nof_58[k]` captures for k=0..3. Keep
the existing unsigned array element types, the following t-array shift loop,
all payload regions and all read/publication boundaries unchanged. The
prediction is four literal loads with no capture backedge, and possible
scalar replacement of the n-array whose remaining uses are all fixed indices.
The latter may explain the current frame's extra sixteen bytes; frame size
alone does not establish source or justify padding/declaration changes. An
unrecovered literal access graph or collateral failure rejects this control.
No further capture spelling or count-type alternatives are part of it.

```sh
python3 tools/recovery_cp_captures.py --domain docs/recovery-cp-next-results.md \
  --baseline-dir build/recovery-cp-captures-baseline
```

## Four-capture result and storage/use-width cross

The two capture cells raw-reproduce their control and pass thirty shared-body
verdicts plus the full nontext/metadata/relocation audit. Literal capture
loads replace the absent-in-original backedge. The encoder becomes 2,748
bytes (SIZE37), with zero exact gains/losses and no changed sibling body.
The frame remains 0x8c: the predicted automatic scalar replacement does not
occur. Initial RTL and native stores still give n a contiguous sixteen-byte
stack array. This observation rules out literal capture alone as an
explanation for the original storage graph.

Declare a four-cell complete-TU cross with the following two independent axes:

* Four unsigned scalar n0..n3 bounds in place of the n-array, retaining the
  indexed t-array and the four witnessed literal capture order. Original
  bounds have separate descending spill homes at stack+0x28/+0x24/+0x20/+0x1c,
  while only t is accessed by a runtime index. The candidate currently has
  n at stack+0x60..0x6c and t at+0x70..0x7c. The prediction is independent
  bound carriers with unchanged indexed t use. Matching the original frame
  would corroborate this source/storage hypothesis; it does not by itself
  establish it. No declaration permutation or forced stack location.
* A signed-byte use-site representation for the byte_11 switch. Original
  dispatch uses CMP AL,1 at 0x525d7 and signed JLE at 0x525df, followed by byte
  comparisons with 2/3 and TEST AL in the zero arm. Retained dispatch instead
  uses full EAX comparisons. Keep the unsigned shared field and cast only
  this controlling value to signed char. The original *predicate* is signed
  and byte-wide; that does not prove an original field type or unique source
  spelling. Both representations leave all values outside 0..3 unwritten,
  including high-bit bytes. Prediction: original byte predicate graph;
  a surviving SI comparison falsifies this representation's explanation.

The control fixes all late masks, the published CRC bound and literal captures.
The unchanged cell must raw-reproduce the control; all four must preserve every
value/store and pass complete-TU collateral and exact-set checks. Each axis has one source
representation; no adjacent width palette or ordering sweep follows a miss.

```sh
python3 tools/recovery_cp_storage.py --domain docs/recovery-cp-next-results.md \
  --baseline-dir build/recovery-cp-storage-baseline
```

## Storage result and original helper ownership

All four storage/use-width controls pass sixty body verdicts and complete
metadata/data/nontext-relocation audits, with raw unchanged control and zero
exact gains/losses. Every encoder remains SIZE37. The scalar axis restores
four independent SI pseudos and descending spill homes, but the frame remains
0x8c. The byte axis restores CMP AL and signed JLE without changing the shared
field. Neither result identifies unique original declarations.

The remaining intermediate bitIndex publication has a measured current
compiler explanation. Initial RTL UID608 stores pos+9 into bitIndex before
the seven zero bits. The `08.gcse` log explicitly says `STORE_MOTION delete
insn in BB 34`, printing that store and its replacement UID3290, a register
copy into pseudo318. The original native body retains the member store at
0x52751. This locates a current transformation; it does not recover the
original compiler's intermediate state or establish a flag correction.
`build/recovery-cp-next/cursor-store-motion.json` retains the explicit log and
positive/missing-deletion controls. A first-occurrence UID trace and duplicate
dump store counts are retained as excluded diagnostics: the gcse log prints
earlier and later streams, so neither identifies the first dropping pass.

There is independent correspondence for the existing CP helper producers:
resetCRC's original signed sixteen-one loop matches the inline reset; calcCRC
has the same byte feedback, fixed tap graph and bodyLength-0x11 bound; the
exact 101-byte calcSequenceLength has the original unsigned divide/product
graph and field inputs. Strong global binding does not prevent O3 automatic
inlining. MP's PrintBase2 is not a corresponding CP producer: its unsigned
long/SHR contract differs from CP's signed-word/SAR source.

Declare exactly eight complete-TU cells crossing direct bodies versus calls
to the existing resetCRC, calcCRC and calcSequenceLength methods, over the
fixed preceding source. These restore possible original helper-owned source
regions without adding a helper, changing any provider, forcing inlining or
altering flags. Inlining must preserve the original call-free native regions,
the exact helper exports and the original value/read-age graph. An emitted
call, wrong bound/feedback or changed nontext/binding invalidates adoption of
that cell. Unchanged complete-TU reproduction and every-body audit remain
mandatory. Missing cursor publication may survive this ownership cross;
no claim that these three tail helpers explain the earlier count-padding
region is made.

```sh
python3 tools/recovery_cp_helpers.py --domain docs/recovery-cp-next-results.md \
  --baseline-dir build/recovery-cp-helpers-baseline
```

## Closure and retained source

All eight helper cells compile and pass 120 shared-body verdicts plus the
complete metadata/data/nontext-relocation audit. All seven changed source
cells compile to the same complete object, SHA-256
`db5dab0fd4235828a08d1c1b1074347a00b75718987b370ecef517eda2970009`.
Each preserves the original call-free encoder graph and every provider export.
The encoder remains SIZE37 with zero exact gains/losses. These controls prove
that the existing global CP methods automatically inline under this profile;
they do not distinguish original helper ownership. No helper-call spelling
is retained, and no option or inline-directive control follows this miss.

The retained source is `recovery-cp-storage/V90CP/scalar-1-byte-1`, with a
documentation comment added. Acceptance follows the individually witnessed
late feedback uses, published CRC-bound dependency, four literal source
captures, independent bound carriers and byte predicate. The scalar and byte
axes are not selected for a smaller SIZE value: their four cells all have the
same 2,748-byte extent. The original field/formal types and source declarations
are not uniquely established. The sixteen-byte frame difference is unresolved.

| Body | Opening bytes / verdict | Retained bytes / verdict |
| --- | --- | --- |
| infoToBits | 2,716 / SIZE69 | 2,748 / SIZE37 |
| calcCRC | 582 / SIZE12 | 584 / SIZE14 |
| evaluateCRC | 684 / SIZE16 | 684 / SIZE16 |
| bitsToInfo | 2,388 / SIZE3 | 2,388 / SIZE3 |

The first three source bodies change. bitsToInfo's source is unchanged but it
inlines evaluateCRC; the mask change moves within that native CRC too. Its
original raw feedback sum at 0x5346e precedes tap uses and the final mask at
0x534f1, independently corroborating the inherited correction. This collateral
is an actual inherited operand change, not a claimed register-only bystander.
The other eleven emitted bodies are raw-identical, including all nine exact
CP methods. All fifteen defined/shared names remain present with the same
bindings, visibility and imports/exports. Named data and allocated nontext
metadata are identical. Fourteen anonymous .rodata jump-table addends move
forward by 32 with bitsToInfo: their relocation entries/types/targets, owning
function and function-relative destinations remain unchanged. All remaining
nontext bytes are identical after accounting for exactly those fourteen
addends. The complete opening-to-retained comparison is preserved in
`build/recovery-cp-next/whole-tu-audit.json`; this also records the literal
.rodata difference rather than generalising the final domain's unchanged
relative addends to the whole chained experiment.
Complete .text grows 8,500 to 8,532 bytes. Symbol changes are calcCRC's size,
infoToBits's size and the following bitsToInfo offset (6,112 to 6,144).

The five finite domains total **20 valid full-TU cells / 300 shared-body
verdicts**, representing sixteen distinct source byte strings and ten distinct
complete objects. The aggregate hashes and counts are preserved in
`build/recovery-cp-next/experiment-ledger.json`. There are **zero exact gains
and zero exact losses**. Each
chained unchanged full-TU control raw-reproduces its independent predecessor.
The source/object hashes and complete commands are in each domain's
`results.json`; `tools/recovery_bitpacker_audit.py --run RUN_NAME` regrades
every cell, including the rejected helper ownership controls. The run names
are recovery-cp-crc, recovery-cp-bound, recovery-cp-captures,
recovery-cp-storage and recovery-cp-helpers.

Opening CP object SHA-256:
`8bb7d6d3ec5326a256588f2a80fccc001f8e87b9e62693929e2ac1eee4137f8b`.
Retained CP object SHA-256:
`8e9d6ac8187912569b63dfbc4ec2f5836f55c054d2c3b80c22333aa004d67e6f`.

## Deciding validation and limits

Gentoo `make period T='t_v90cpinfo t_v90cpcrc t_v90cpb2i t_v90cpleaf' J=4`
passes all four requested binaries, with zero unknown or failed selections.
Their twenty groups contain 61,735 checks: info 2,177, CRC 1,364, decoder
56,281 and leaves 1,913.
The final period CP object raw-matches the selected independent candidate,
including the production comment change. Existing component-method fixtures
and the standards CRC oracle are used; no new state histories, public-modem
reachability assertion, tolerance or fixture is introduced. Logs are
`build/recovery-cp-next/focused-period.log` and `fixtures/*.run.log`.

A private complete 300-object census substitutes only the validated CP object
into the frozen d9995470 inputs. All 300 opening hashes were independently
checked against the final campaign manifest before any source edit. The
other 299 objects and complete configuration remain raw-identical. The fresh
ratcheted census remains **1,091/1,852 EXACT, 122,715/720,125 original exact
bytes**, with zero lost or gained names against the entire frozen exact set.
It reports 662 SIZE, 65 BYTES, 29 REGALLOC and five UNRESOLVED. The final
census and log are `build/recovery-cp-next/final-census.{json,log}`; these inputs
are isolated from the parent's later live FixedRC/MP experiments.

The new source detaches six existing mutation locators, as demonstrated by
the failing static pre-update check. Their find/replace strings follow the
late feedback and locally scoped bound while preserving the original six
alternatives. No mutation execution or snapshot update is part of this pass.
The final static check passes all 285 suites / 10,040 anchors, with zero
skipped, nonunique, vacuous or wrong-arm anchors. Refcheck reports 14,464
references, zero unresolved/pending/stale, and 3,100 valid finding headings.
All five new replay scripts pass py_compile; source and candidate hashes are
checked in the aggregate audit. Final static logs are anchors-final.log and
references-final.log under the same artefact directory.
The parent campaign owns the final integrated make phase and faithful partial
link. No new final-object semantic/raw or modern-portability claim is made.

The tested domains are closed. The next distinct discriminator is the
original count-padding cursor publication versus the measured current gcse
store-motion deletion, and the still unexplained frame/induction allocation
after scalar bound recovery. An independently corresponding producer or
read/alias ownership witness is required before more source controls. The
current compiler trace alone does not authorise volatile, alias barriers,
forced registers, padding, flag changes or nearby declaration permutations.
