# V22 transfer wave — 2026-11-05

This wave adds two EXACT functions: `Detect_v22` (375 bytes) and
`v22_org_rmloop2` (1,050 bytes). It also restores native constructor and HDX
owner boundaries. `V22FP_create` remains SIZE7 against 2,449 native bytes;
its final sharing and register choices are unresolved. No EXACT or REGALLOC
member is lost. The full census is 1,136 EXACT / 28 REGALLOC / 71 BYTES /
617 SIZE over 1,852, against baseline local commit 2a0de7e81.

## Detector: recover the loop and scalar timing together

The initial suspicion of a detector-result width defect was falsified by the
unchanged control: its first signed extension and second word test already
match native. An independent audit found eighteen native MTD references from
thirteen functions, all direct calls. Mixed full-width and low-word use does
not uniquely establish the original return declaration; no interface changed.

The retained detector initializes counters after AGC, separates the root-based
clear pointer from the MTD chunk induction variable, uses an ascending pointer
with a short countdown, preserves the native zero-result fallthrough and keeps
a short scalar subblock bound. The branch/bound cross gives SIZE8 for neither
or bound alone, BYTES4 for branch alone, and EXACT for both. Three ordinary
loop controls fail to recover the last operand/predicate distinction.

The literal bound frontend produces word comparison against 3 with a GT exit.
The scalar bound survives through CSE, then loop propagation substitutes 4
while retaining the signed LT predicate. Native has CMPW4/JL. This is a
consumed loop bound, not an unrelated scratch consumer; the original literal
`short subblocks` spelling is not uniquely recovered. Extra High review
recompiled the source literally and independently checked the native CFG,
widths and six relocations. All nine neighboring TU bodies remain unchanged.
The exact object is identical under current, previous and plain-pointer owner
headers, so this gain does not select the original owner declaration.

## Constructor: separate real epochs from backend sharing

Thirty-four constructor source cells and the paired header closure exposed
independently witnessed source properties: HDX/DSP capture groups across
allocations and initializers; allocation destination owners captured before
calls; separate tone and initialization-tone configurations; the native
0xec frame and configuration slots; byte parameter-flag accesses; and a
word status clear followed by byte flag/status publications. A separate
original input and working owner recover native register/value roles.

Native first-tone destination acquisition at 0x87c5a and argument acquisition
at 0x87c61 straddle an outgoing argument publication. A typed/opaque HDX-owner
union alone over-invalidates the scalar group (SIZE51); captures alone are
raw-inert. Both restore the native acquisition. It extends the existing
explicit alias-contract model; it does not prove an original pointer union,
opaque pointer declaration or volatile field. The final period layout probe
passes 26 checks; a deliberately wrong byte mask fails one of 26.

The minimal status model is a word/byte union. Memset, that union and an extra
status-bitfield alternative emit identical objects. The extra bitfield was
therefore dropped. Earlier delegate claims that the word view folded ORB to
MOV, and that the input-owner variant was raw-identical, were withdrawn after
object inspection. Machine audits were retained; those narrative inferences
are not evidence. Equal SIZE grades do not establish equal instructions.

The remaining cold sharing is traced precisely: peephole2 selects different
scratch registers for the two mode stores and matching registers for rate
stores; ce3 consequently shares the opposite pair from native. A three-byte
layout shift crosses the short-JE boundary and contributes another four bytes
to the seven-byte extent difference. The branch length is a consequence.
The actual retained constructor trace validates 26 scratch choices; none of
53 incoming cursors recovers native inverse-sharing with fixed sites and
eligibility. That is a bounded diagnostic, not a global allocator wall.
Matching frame size or hot-path offsets does not prove hot opcode equality.

## Follow the collateral instead of freezing exact neighbors

The broader owner model moved Demod's HDX acquisition past its rx_count word
publication. Native 0x8e3ca captures HDX before the store at 0x8e3cd. An
explicit capture restores the complete fourteen-function Demod object
literally under both headers. Its existing EXACT result was a reason to audit
the new boundary, not a reason to reject the shared model without repair.

The state-handler investigation completed twenty full-TU cells and six dump
controls. It restores native owner groups and detector call ages, including
originating RMS publications after Detect_1s, conditional retrain captures,
capture-before-gap timing and the answer tone-owner join. NODE0 retains its
owner across the substate write and skips NODE1's acquisition, as native does.
`v22_org_rmloop2` becomes EXACT under both headers. The scope is all ten
functions, with 19 owner/member layout checks and explicit old/new controls.

| Function | Baseline | Retained |
| --- | --- | --- |
| V22FP_create | SIZE40 | SIZE7 |
| Detect_v22 | SIZE29 | EXACT375 |
| v22_org_rmloop2 | BYTES90 | EXACT1050 |
| V22FP_modem | BYTES5 | SIZE8 |
| connect_1200 | BYTES34 | BYTES70 |
| v22_answer | SIZE1 | SIZE2 |
| v22_originate | SIZE136 | SIZE160 |

The four worsening rows are explicit structural tradeoffs. Native owner and
call boundaries improve; exact code is not claimed. Connect2400, retrain and
data retain their SIZE grades, with scheduling differences across headers.
Answer-remote-loop and local-loop retain their baseline bodies. Header-invariant
source does not by itself prove full native equivalence.

Eight handler bodies change against the baseline. All five allocated non-text
sections retain masked payload/layout; exports and emitted order remain intact.
Eleven of 58 code-table function-relative destinations move, with zero owner
changes. Independent review decodes all 174 native/baseline/candidate targets
at instruction boundaries and checks source-case entry identities. Raw data
and strict target-offset equality are explicitly different. This entry-edge
review and period fixtures do not establish a complete CFG equivalence proof
or public modem reachability for planted states.

## Validation and next transfer

The frozen baseline contains 300 hash-verified objects. Both original focused
fixtures pass. The retained detector passes 195,264 direct checks; constructor,
Demod and alias probes pass five focused binaries. The L predecessor passes
five state-handler binaries; final O receives the complete period gate. Fixtures mix constructor/reinitialization histories with planted
component states; the latter remain synthetic exploratory probes.

Three constructor mutation anchors were reanchored to retain their original
faults after field and local-name changes. The identical-tone substitution
remains explicitly equivalent. No snapshot refresh or mutation campaign was
run. An intermediate gate passed all 399 period tests but failed those stale
anchors; the corrected gate and the actual final source receive separate logs.
A copied object cache without dependencies was excluded from full-closure
evidence and all 300 comparison objects were rebuilt before grading.

Sol High performed the source investigations. Targeted Sol Extra High review
added independent source-to-object reproduction, native boundary checks, cold
sharing/cursor controls and table-entry review. It did not discover another
promotion independently. These were collaborative reviews, not blind model
comparisons; unavailable token and billing usage remains unknown.

Next, investigate originate's native predicate spill/frame contract: native
uses byte slot esp+0x33 and frame0x7c, while retained source uses frame0x6c.
Inspect the predicate producer, width, lifetime and consumers before changing
its type or adding a local. Constructor sharing should reopen only with a
new native-backed operation, lifetime or eligibility discriminator. Do not
retry source-order or cursor palettes. Transfer the successful capture-before-
publication lesson to another independently witnessed consumer.

Evidence is under `build/v22-transfer/` and
`/mnt/zfs/dev/slmodem/scratch/v22-transfer-20261105/{detect,create,modem,review}`.
The reports retain exact commands, captured inputs, hashes, experiment limits,
failed controls and corrections. [Issue #590](https://github.com/philpem/slmodem_dsp_re/issues/590)
tracks the unresolved owner declaration and remaining consumers.

## Final retained-source gate

`nohup make phase J=8` on the frozen O source completed with 399 period tests
passed, zero failures, and all structural checks green. This is
`build/v22-transfer/phase-retained.log`; the earlier reanchored N gate also
passed but is not the final-source verdict. All 285 mutation suites retain
10,044 anchors with zero ambiguous/missing matches. The final constructor,
Demod and detector objects reproduce their selected hashes, and the actual
retained modem object is
`0321337719b51779f24fb8852d4a2fd614adf3db61406062ff5d39b244d80fe2`.

The census records exactly two new EXACT names, Detect_v22 and
v22_org_rmloop2, totalling 1,425 native bytes. No exact/regalloc member is lost.
Original owner declarations and full constructor/originate byte identity remain
unresolved. Portability tiers were not run and are not implied by this gate.
