# Fixed-index arithmetic: bounded screen and receive-CRC transfer

Merged277 at548b5edd; baseline1070/1852 exact,116851 exact original bytes.
No production source/header/flags change. PR263's V34/structure paths and
issue22 writes stay excluded. All compilation uses retained Gentoo3.4.2-r2,
the executed compiler-selected assembler2.15.92.0.2 and bug define last.

## Screen denominator and scope

tools/fixed_index_screen.py scans283 C/C++ TUs outside V34:1744emitted bodies,
675nonexact original<=4096B functions. It recognizes1210 lexical for headers,
269fixed numeric-bound2..32 headers and941unsupported headers,0unmapped in
the recognized syntax. These are regex counts, not a complete C++ parser.
68eligible emitted bodies map to fixed source loops; overload ambiguity is
reported and requires manual owner/signature review. Known closed V92Precoder
cfg-reset positive fires:6/12loops,2retained/0original conditional backedges.
That source family is not retried. [Declared domain](fixed-index-screen-domain.md).

Ten body nominations arise from fewer original conditional backward branches:

| Nomination | Disposition |
|---|---|
| VTB_decoder | Both8-entry metric collection and7-entry best-state scan remain loops (ab520..ab54f, abaa0..abac3). No missing literal expansion witness. |
| getConstellationsIndex | Original fixed outer1..5 loop remains at33370..33413, with nested matching loops. Total-edge difference does not identify a missing unroll. |
| detector_progress | Original indexed TONE call ad84f and four-entry dispatch ad720..ad734 remain; no literal expansion established. Prior formal-width work is separate. |
| float2Bits | Existing closed branch/operand-owner domains; no new expansion witness. |
| V92Jd::packJdData / packJdPhaseData | Existing closed helper/member-promotion controls, left untouched. |
| V92Precoder::reset(cfg) | Known closed positive; calibration only. |
| V90Jd::unPackData / V92Jd::unPackJdData | New independently witnessed receive-CRC consumers; tested below. |
| V90PreFilter::setParamEia6 | New straight-line18-entry/predicate control; tested separately. |

Original/retained backedges for V92Jd::unPackJdPhaseData are both11, so the
whole-function screen misses it. Complete sibling graph inspection nonetheless
shows the same receive CRC promotion. Counts nominate; they cannot prove
absence. This blind spot supplies a practical next diagnostic: compare the
backedges inside the actual arithmetic loop instead of the entire function.

## Receive CRC: mechanism recovered, no complete function

Original three unpackers load16 crc member entries before the32input loop,
update private values during it, then store16 entries afterward. Reset and
absolute-difference comparison remain indexed loops. Retained inner15copy loop
prevents the promotion. Prior V90 packData lever13 supplies a related source
positive; prior V92 pack-helper experiment explicitly leaves unpackers untouched.

Six fullTUs: V90 baseline/15literal shifts, and independent V92 data/phase
expansion axes. Preserve ascending old-value copy, precomputed taps, feedback,
all state/input/return semantics, reset and checksum loops. No helper, width,
counter, declaration, slot, register or profile variations.

| Consumer | Original bytes | Retained | Expanded |
|---|---:|---:|---:|
| V90 data |879|525|861|
| V92 data |1003|590|976|
| V92 phase |987|532|944 alone /950 with data expanded |

All three expanded recurrence bodies remove the nested copy loop:2backedges
inside the32input loop become1, matching the original. None becomes EXACT.
V90's expanded body still has232non-padding instructions versus238original;
this is not solely register renaming. Phase's six-byte difference between
isolated/combined controls also requires full-TU context, not score adoption.

Audit6cells/114live grades, raw baselines and hashes, unchanged all bystanders,
bindings/imports/exports, allocated data and BSS. Changed nontext relocations
are solely existing ordered switch-table entries into changed owner bodies;
offset/type/owner fixed and all destinations decoded at instruction boundaries.
Offsets do change and are recorded; this is not final-object or semantic
switch equivalence certification. No source adoption or differential claim.
The loop detector's initial enclosing-branch ambiguity was refused; the final
tool selects the counter31 comparison feeding the actual backedge, excluding
the later checksum-failure return into the preceding reset arm.

Replay tools/jd_unpack_fixed_shift_reproduce.py with
`--domain docs/jd-unpack-fixed-shift-domain.md --baseline-dir build/production-before`,
then tools/jd_unpack_fixed_shift_audit.py. Full commands, hashes, dumps, grades
and relocation/loop observations live in build/jd-unpack-fixed-shift.

## EIA6 and limits

Four fullTU controls64live grades independently cross witnessed18scalar
copies with the original single-zero predicate. Bytes604/598/802/792 versus
original790; no gain/loss. Expanded copy removes its backedge; unequal predicate
adds one anonymous zero-float constant. Fifteen bystanders and all bindings/
BSS/nontext identities remain unchanged. The792B candidate has substantial
x87/use/home/copy differences; SIZE2 does not mean two differing bytes.
Original retains x during fistl, multiplies a loaded scale through registers
and compares with fldz/fcompp. Candidate duplicates/pops x, multiplies memory
scale and compares memory zero. [Complete evidence](batch-cpp-prefilter-expand-results.md).

Ten valid full-TU controls178live body grades,0gains/losses; production remains
1070/1852. No src/header/metadata adoption, fuzz/mutation runtime, new
differential fixture or modern portability claim. The finite domains close.
Next source controls require a new original operand/use/lifetime witness;
nearby arithmetic/declaration spellings are not justified. A distinct diagnostic
pass can trace where these preserved arithmetic/use graphs first diverge in
GCC RTL, especially EIA6's x87 lifetimes, before proposing more source.
The results do not establish an inline-budget-only wall or global ceiling.

Final make refs exit0:14383references/2944finding headings,0unresolved/held/
stale;285static suites10038anchors,0detached/nonunique/no-op/wrong-arm. No
mutation execution. Five new Python tools parse; final source/object screen
provenance covers283/283 TUs. Both full-TU audits pass after live rescoring.
