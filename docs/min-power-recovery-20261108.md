# Minimum transmit-power recovery, 8 November 2026

**No EXACT or REGALLOC promotion was recovered in this wave.** The retained
source repairs measured owner/read/publication boundaries and restores the
native two beta stores. GetVPcmMinimalTxPowerReduction moves from SIZE33/current
329 bytes to BYTES29/native 362. This remains a partial recovery.

Baseline is `bcf6b2560d452294cfc0fb05bb031f885e862e4a`. The unchanged full Main
rebuilds literally to `0160fffa71e0e5877468952536a3c8878f2f543c600c1b7c1f86293304403e39`.
Final source SHA256 is `7cbe41fc4552b77482d5022851ec40ee32753ce4b488ff3aeeda6ff3a7c0e766`;
object is `f0d7e50d6ccf9c7d6472864301fba924547b6978ca869d8bf23d2b603a55c1e8`.
Gentoo GCC3.4.2-r2, selected binutils, mandatory bug reproduction, function order,
headers and classifier are unchanged. [Evidence](evidence/min-power-recovery-20261108.json)
records inputs, crossed controls, analysis/compiler identities and validation.
Full artifacts remain in `/mnt/zfs/dev/slmodem/scratch/power-reduction-wave-20261108`;
parent checks are in the isolated checkout's `build/power-reduction-wave`.

## Recovered source boundaries

Native retains engine root+4, configuration root+0xac3c and session root+0x3548.
Receiver reads remain engine+0x248/+0x24c = root+0x24c/+0x250, width four.
Config+0x44 is consumed as a signed halfword by MOVSWL and clamped to [-10, 7].
The retained model transports it as 32 bits; this does not identify a unique
original declaration. Source keeps the measured low-word read at the checked
member offset. The former comment claiming a byte-pointer cast established
alias legality is corrected.

The clamped short and final receiver-conditioned short are separate source
values. Params is acquired from session+0x610c on the native paths, with the
sensitive-ISP test at params+0x4f8. Receiver gating selects zero when neither
V.90 nor K56 conditions permit the requested reduction. Native addresses,
access widths and signed comparisons are preserved.

For a nonpositive result, native publishes HIGH_LEVEL_TX_ACTIVE=1 at 0x7bc0,
reads codecType at retained config+0x54 at 0x7bc8, then writes echo start/factor/
beta at root+0x3554/+0x3558/+0x355c. Baseline source read codec after the first
two echo publications through a fresh config-pointer expression. Capturing beta
after the high-level store and before those echo stores restores the measured
observation boundary. Native SETNE consumes codec comparison flags after two
MOV stores; those MOVs preserve flags. LEA computes 4 or 6.

The positive result publishes HIGH_LEVEL=0, echo start 2000, factor 0x7fdf and
beta 2; the nonpositive result uses factor 0x7fcb and beta 4/6. All are full-word
stores. The first diagnostic freshly reads the three echo fields. Red crosses
both calls in EBX. Native 0x7b67 reloads params from the retained session and
then reads HIGH_LEVEL for the second diagnostic. The session pointer itself
remains captured; its old source comment is corrected.

The new fixture owners are disjoint, so these read-age differences are not
presented as demonstrated reachable behaviour bugs. Original declaration and
alias evidence remain qualified. Existing helpers were compared; the similar
setup tail has a different reset contract and cannot replace this algorithm.

## High-effort source experiment

The initial complete-TU source investigation uses Sol High. It checks actual
owners, config retention, acquisition boundaries and narrow value transport
before interpreting bytes. All cells retain all 56 functions and 41 protected
copies; no other body changes.

| Declared domain | Cells | Result |
| --- | ---: | --- |
| Engine/config/params/clamp properties | 16 | Owner/config combination reaches SIZE3; no promotion. |
| Separate clamped/final values and assignment boundaries | 6 | Restores native AX→BX handoff; remains SIZE1. |
| Sensitive predicate and params acquisition | 6 | Acquisition spellings collapse pairwise. |
| Receiver-conditioned result shapes | 4 | Native extent, BYTES51; combined-OR control SIZE10. |
| Early beta read and positive store order | 4 | Measured read boundary recovered; BYTES45. |
| 3! echo-store orders in each arm | 36 | No EXACT/REGALLOC; 30 distinct object hashes. |
| Boolean capture / full codec capture | 2 | BYTES30/BYTES60; neither adopted for its score. |

There are 76 valid scored cells including repeated baseline/selected controls.
One faulty order-generator compile is preserved as invalid and excluded.
Ordinary/dump and read-only-observed objects reproduce literally. The partial
candidate still has 79 decoded instructions against native 80: peephole2 assigns
AX to both beta stores, then ce3 merges them. This first transformation occurs
before rnreg/sched2, so a final-scheduling-only explanation would be wrong.

## Extra High causal review

An independent Sol Extra High context first reproduces the same unchanged TU.
Read-only captures observe 367 scratch searches in 33 functions and calibrate the
existing model against every event. Holding observed search order and eligibility
fixed for the pre-inversion beta1p0 candidate, all 53 possible target-entry
cursors are replayed. Only cursor 15 selects
native positive-beta EBP; the current preceding SetTxScale searches deliver only
2 or 3 over that full domain. Including RequestDPNotification does not repair it.
This excludes a cursor-only predecessor change for that fixed candidate graph.
It does not bound the later arm-inversion graph or exclude other changed search
graphs, eligibility or hidden preimages of exact
machine code. TU-wide register/code-generation coupling remains established.

The final-arm inversion is a discriminating source control: authoring red<=0
first changes initial block identity and peephole2 search order. Positive beta
uses CX and false beta AX at ce3, so both stores survive. Later rnreg selects
EBP for both, after the merge pass. BBRO still emits native signed JLE and
positive fallthrough. This recovers native structure rather than selecting a
candidate merely for a smaller byte count.

The changed orientation graph is then separately calibrated and replayed over
the same three 53-start domains. SetTxScale's reachable target cursors 2/3 select
positive-beta CX/EDI, both distinct from false-beta AX before ce3. Cursor 2 has
an actual compiler observation; cursor 3 is a conditional model prediction only,
with no later-pass or whole-function proof. The pre-inversion obstruction does
not transfer to this graph; do not use it to veto a supported predecessor change.

That new graph justifies crossing inversion with the earlier 36 echo-order
controls. All 36 are measured: 32 BYTES and 4 SIZE12, no EXACT/REGALLOC. A separate
six-cell beta-width/publication cross is also negative; short beta is neutral
and shared HIGH_LEVEL publication adds 16 bytes. There are 45 scored full-TU
builds in this review. The clearest final candidate retains ordinary field order,
both beta publications and the corrected source comments, which are raw-neutral.

Native and final both have 80 decoded instructions, 78 after padding removal.
All 11 branch/return sites and operands match literally. Diagnostic calls at
function-relative 194/226 retain their sites, widths, targets, addends and string
identities; return is at 252. The strict register comparison still rejects at
nonpadding row 62: MOV versus XOR. False-arm zero/constant setup is scheduled
differently, and native SETNE CL/LEA into EDI differs from selected AL/EBP.
Matching size, instruction count and CFG do not establish REGALLOC.

Complete review covers signed clamps, receiver guards, parameter acquisitions,
field/publication widths, codec flag lifetime, diagnostic rereads, EBX across
calls, AX return and callee-saved/stack restoration. Conditional model results
and this bounded native correspondence do not establish arbitrary-alias,
concurrent or whole-object equivalence. No carrier, padding, profile change,
declaration guess or classifier relaxation is retained.

## Constructed differential coverage

The new fixture passes unchanged source first: two focused period tests, zero
failures. The intermediate candidate also passes those two before final review.
`t_vpcmcreate` adds 4,808 checks, reaching 40,659 checks in 19 groups.

The 32 cases/64 getter calls follow native Create/modeminit session types 0/1/3/4,
with public SetV90 establishing selector 11 in two configuration rows. Caller-
owned runtime fields supply full-width requests, including 0x10000 and 0x1ffff,
which native consumes as signed low words. Independent expected values cover
clamps, disabled selectors, both result signs, codec 4/other, HIGH_LEVEL and all
three echo fields. There are 10 positive/22 nonpositive cases, 10 clamp inputs,
four disabled selectors and 22 changed constructed graphs.

All 126 live regions are compared raw through `diff_eq_obj`: 4,032 region checks,
with no learned exclusions, plus paired/expected returns, transcripts, capture
completion and callback boundaries. The 14 period layout assertions pass.
These are component/configuration-boundary inputs; dp_runtime_create itself
sets request 6. Layout-zero, sensitive-ISP and pointer-alias histories remain
outside the new constructed domain. Existing planted probes stay synthetic;
no artificial alias history is added to justify the read-age repair.

## Validation and complete-TU effects

Independent parent baseline/selected builds reproduce frozen ordinary objects.
All 533 frozen inputs and 300 baseline objects are hashed. All 56 Main functions,
103 export records, 41 protected copies, function order, 225 CALL identities and
81 exact data continuations survive. Only the target body changes. The global
census is 1,159 EXACT / 33 REGALLOC / 79 BYTES / 581 SIZE over 1,852: one SIZE→BYTES,
zero EXACT/REGALLOC promotions. Across 300 source objects, only Main changes.

The changed-TU graph selects 18 inexact copies. Owner admits 16/refuses 2,
compares 245 relations across 12 copies, with 30 ambiguities/3 unpaired anchors;
stack admits 9/refuses 9, compares zero relations, with 19 ambiguous anchors.
There are zero nominations within those supported subsets; unsupported effects,
ambiguity and missing relations prevent an equivalence claim.

Both strict 300-object partial links use 279 anchored and 21 source-order inputs
and remain structurally/raw DIFFERENT. Native .text is 728,304 bytes; baseline/
selected are 714,665/714,697. Raw partial-link identity remains an open objective.
Modern portability, mutation suites, anchors and snapshots are not run/refreshed.
The final `make phase` passes 399 period tests, zero failures, plus structural checks.

## Next investigation and reopening conditions

Audit **VPcmV34GetCurrentTxBitRate** after checking its prior negative controls.
Native 213 bytes keeps root in EDX and directly reads the shared signed fallback
at root+0xaa88. Current 217 forms a ratecfg base at root+0xaa84 in EBX and saves/
restores EBX across every arm. Native 63/current 65 decoded instructions also
differ at x87: native FLDS/FADDP versus current FADDS for adding 0.5. Cross actual-
member fallback consumption with a separately supported shared-result/conversion
lifetime, preserving unsigned 32-bit multiplication and unsigned x87 conversion.
Cross a native-supported TxBitRate candidate with both minimum-power arm
orientations and remeasure the complete TU; the old graph's cursor exclusion
must not veto the changed graph. This is a read-only nomination, with no
candidate or promised gain here.

Reopen minimum power with a new native-supported false-beta live-range, actual
declaration/alias boundary, helper boundary or changed intervening search graph.
Cross it with both source-arm orientations and a native-supported predecessor
recovery where appropriate. The closed echo-order, short-beta,
shared-HIGH_LEVEL, Boolean/full-codec and fixed-search cursor domains are recorded
constraints. Their negative results do not prove a global optimum or unique
original source. Actual model usage and billing were unavailable; effort labels
are not an efficiency measurement.
