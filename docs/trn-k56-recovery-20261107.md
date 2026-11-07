# Coupled Trn2d and K56 recovery, 7 November 2026

This wave adds **three EXACT promotions**, retains every existing EXACT/
REGALLOC copy, and improves K56Phase to BYTES8. It confirms the established
working rule that register choices and code generation are coupled across the
translation unit. The Playbook and AGENTS now put that rule prominently near
the start of their recovery instructions.

Baseline is `f3bad5f10a641ca293064855df21f51b148b1aca`. All builds retain the
Gentoo GCC3.4.2-r2 profile and mandatory `DSPLIB_REPRODUCE_BUGS`; no flags,
headers, ABI, classifier or tolerances change. [Evidence](evidence/trn-k56-recovery-20261107.json)
records frozen inputs, crossed controls, compiler/helper provenance and gates.
Complete artifacts remain under `/mnt/zfs/dev/slmodem/scratch/trn-k56-wave-20261107`;
parent validation is in the isolated checkout's `build/trn-k56-wave`.

| Function | Baseline | Final |
| --- | --- | --- |
| V34XF_IndicateTrn2dReceived | SIZE40, 87 bytes | **EXACT127** |
| V34XF_IndicateK56FlexJdReceived | SIZE15, 232 bytes | **EXACT247** |
| chkForceBaudRate | REGALLOC389 | **EXACT389** |
| V34XF_IndicateK56FlexRateDetermined | EXACT64 | **EXACT64 retained** |
| k56FlexPhase34 | SIZE28, 693 bytes | BYTES8, native 721 bytes |

The global census moves from **1,156 EXACT / 34 REGALLOC / 77 BYTES / 585 SIZE**
to **1,159 / 33 / 78 / 582**, over the same 1,852 symbols. REGALLOC decreases
because chkForce becomes EXACT. K56Phase remains a partial recovery.

## Trn2d and the historical neighbour loss

> 🧠 **From Hindsight memory (Full residual-family screening and DIL/MP dataflow recovery)** — a previous Trn2d recovery was reverted after K56Rate lost EXACT.

The cited F12052 record was found in
[archived findings](https://github.com/philpem/slmodem_dsp_re/blob/1bb171a7528157bf2d43051faa74e2e4154215b4/docs/findings.md).
Its rejected source bytes were not recovered; the described properties were
reconstructed against current native instructions. Its one-debug-test claim
and alleged store at 0xa3f4 are inaccurate. Native has tests at 0xa3aa/0xa3c5
and stores at 0xa3b1/0xa3d1. The old monotonic EXACT-retention decision is
superseded by the owner's instruction to investigate and repair coupled losses.

Native Trn2d retains engine root+4 and state engine+0x248 = root+0x24c,
width four. Signed thresholds are 9/14/17; the final arm uses SETG/MOVZBL/LEA.
Actual engine ownership and per-arm stores recover the complete native body,
including its two debug tests and member reload for the shared diagnostic.

| Trn2d properties | Trn2d | K56Rate |
| --- | --- | --- |
| Baseline | SIZE40 | EXACT64 |
| Engine owner only | SIZE24 | EXACT64 |
| Per-arm stores only | BYTES37 | REGALLOC64 |
| Both | EXACT127 | REGALLOC64 |

The temporary loss is retained as evidence. It does not disqualify the
native-supported recovery.

## K56Jd repairs the coupling and becomes exact

Native retains RX root+0x264, TX root+0x221c and DC root+0x254. Constellation
is root+0x382, width two, published separately in each arm. Diagnostic fields
come from complete-root preemp/carrier/baud at 0xaa8a/0xaa94/0xaa84. The actual
DC owner has count/estimate/accumulator at 0x254/0x256/0x258, widths 2/2/4,
with existing period layout assertions. Count arithmetic preserves the signed
short input, bounded int product and low-word publication.

The complete eight-cell DC-owner × direct-config × per-arm-constellation cross
recovers native extent 247 and BYTES17. Two disjoint clear pairs then interact:
DC source order alone gives BYTES2, TX source order alone BYTES15, both EXACT247.
Source clears previous quadrant before count; scheduling emits native count
before previous quadrant. Source clears DC accumulator before estimate.

| Complete source cross | Trn2d | K56Jd | K56Rate | chkForce |
| --- | --- | --- | --- | --- |
| Baseline | SIZE40 | SIZE15 | EXACT | REGALLOC |
| Trn2d only | EXACT | SIZE15 | REGALLOC | EXACT |
| Final K56Jd only | SIZE40 | SIZE16 | REGALLOC | REGALLOC |
| Both | EXACT | EXACT | EXACT | EXACT |

This is the practical reason to preserve intermediate regressions. Each
isolated change disturbs K56Rate; the coupled source repairs it and gains Jd.

Read-only full-TU compiler captures reproduce ordinary objects literally.
Baseline has 365 scratch searches in 32 functions; Trn2d and coupled variants
have 367 in 33. The first difference in unchanged Rate/chkForce bodies is at
peephole2. Rate UID41 has identical mode/live/eligibility inputs but incoming
cursor 0/2/3 and AX/CX/AX selection in baseline/Trn2d/coupled source. Trn2d's
two added searches and Jd's changed scratch selections explain this case.
Individual function-local passes coexist with state carried across the TU;
the evidence does not imply that every register mismatch has this cause.

## K56Phase: complete residual review

Actual RX/TX/engine owners, ordered 3/4/5 dispatch, the native unmasked shift,
actual idle-quadrant consumption and earlier K56 session capture recover native
size 721. Every changed member preserves owner, offset and width: count root+
0x25c0/2, previous/current quadrant 0x25c6/2 and 0x25c8/2, scrambler 0x25cc/4,
point 0x25d0/4, pattern 0x25d6/2, index 0x2aa2/2 and state 0x250/4.

Sixteen owner/dispatch cells reach SIZE3. Shift/idle controls reach BYTES107
at native size; the coupled Trn2d/Jd context gives BYTES22; early session
capture gives BYTES8. Regrouping constant owner-address declarations is neutral.
Reversing the reset statements crossed with early capture gives BYTES22/8/29/15,
so that reversal is rejected. The exact Jd context retains BYTES8. There are 31
retained build objects, 28 source hashes and 26 object hashes, including ordinary/
dump/cleanup controls. None is an extra independent recovery claim.

Native and final K56Phase have 162 decoded instructions, frame 44, 22 blocks/
33 edges, 17 matching branch/return operands and 12 matching CALL operands,
including the strict local getMP displacement. Complete instruction multisets
agree. Only two adjacent stores reverse: native clears count at TX+0x3a4
before scrambler at TX+0x3b0; selected clears scrambler first. Both zero values,
lanes and widths match. The fields are disjoint; no call, read or branch lies
between them, and MOV changes no flags. The order is native through bbro
(UID511 before 509); sched2 reverses it. Ordinary/dump objects agree literally.

The complete review preserves provider-result AX narrowing and live transport
across emitters, actual output-member arguments, literal idle generator 1,
absence of differential encoding/previous-quadrant advance, post-txmit counter
reload, completion publications, stack and return lanes. Existing emitters have
different idle contracts and cannot replace this body. This argument assumes
valid complete owners and corresponding opaque-callee behaviour; it is not
exhaustive alias/concurrency or whole-object equivalence.

Trn2d recovers 32 padded bytes (96→128), Jd 16 (240→256), jointly closing the
48-byte getMP-to-K56 layout debt. Original function order is retained. No call
distance is masked and no caller padding or artificial carrier is introduced.

## Differential coverage and complete-object checks

The expanded fixtures first pass against unchanged source: four focused tests,
zero failures. Final **make phase J=8 passes 399 period tests, zero failures**,
plus structural checks. `t_vpcmcreate` adds 7,444 checks, reaching 35,851 in 18 groups.

Trn2d has 20 constructed histories/120 target calls/2,784 checks. Native Create,
modeminit, Jd/Dil indications and SetV90 establish entry states 0/3/6/11/15.
Three calls cover all four outputs and explicitly assert 10→14 and 18→20;
Trn2d is not generally idempotent. The old t_v34pcmif comment claiming otherwise
is corrected. K56 has 24 Ja prefixes using session types 3/4/288 calls/4,660 checks,
covering roles, debug 0/2 and 600/2400/3429-baud profiles within queue capacity.
All 44 histories change the constructed graph. Both groups compare all 126 live
regions raw with `diff_eq_obj`: 5,544 region comparisons, no learned exclusions,
plus transcripts, capture completion and callback boundaries.

These are component histories. Native Ja/MP providers are three-byte zero-return,
no-write stubs, so their completion tails cannot execute under that contract.
Case 3's 0..7 index invariant is conditional on entry and continuation; it does
not establish constructed reachability. Existing planted arm/index tests remain
synthetic. The new fixture's initial wrong guard-offset assertion and draft
ring[1] layout assumption are preserved as invalid apparatus runs; the corrected
pass checks flags at 0x386 and ten period layout obligations. Queue draining
still needs delay/lag/filter invariants: adaptecho calls EchoFilter before its
freeze guard, so freezing alone cannot validate arbitrarily long prefixes.

Independent parent builds reproduce unchanged and selected full TUs literally.
All 533 frozen inputs and 300 baseline objects are hashed. All 56 Main functions,
39 protected copies, exports and function order survive. Only Trn2d, Jd,
chkForce and K56Phase change; Create and K56Rate are literally unchanged.
All 225 CALL identities and 81 exact data continuations remain. Trn2d adds the
second native debug-level relocation. Across 300 source objects, only Main
changes. Both strict 300-object partial links remain structurally/raw DIFFERENT:
native .text 728,304 bytes, baseline 714,585, selected 714,665. Full object recovery
remains open. Modern portability and mutation/snapshot maintenance are not run.

The fresh full-corpus graph screen covers 300 objects, 693 unique inexact symbols/
696 defining copies. Owner admits 586/refuses 110, compares 2,807 relations across
267 copies, with 571 ambiguous/413 unpaired anchors. Stack admits 104/refuses 592,
compares 2,964 relations across 22 copies, with 100 ambiguities/16 unpaired anchors.
Four owner timing nominations span three copies; stack has zero. The Main
subset has 18 inexact copies: owner 16/2 and 244 relations, 32 ambiguities/9 unpaired;
stack 8/10 and zero relations, 18 ambiguities. These bounded diagnostics omit
unsupported effects and ambiguous matches; they are not equivalence verdicts.

## Next investigations

Start with **GetVPcmMinimalTxPowerReduction**, still SIZE33. Fresh native 0x7aaf
retains engine root+4; 0x7ae3/0x7af9 read engine+0x248/+0x24c while current source
uses flat fields. Native retains config/session independently, acquires params
on paths 0x7af1/0x7bf0 and reloads params after diagnostic at 0x7b67. Audit these
owners and read ages, then cross supported properties across the complete TU.
The current comment calls that params reload a session reload; verify and
correct its owner terminology during that investigation. No candidate or gain
is claimed here.

Reopen K56Phase's last pair with a distinct native-supported helper, declaration,
alias or liveness witness predicting the sched2 dependency. The owner/capture/
mask/reset families above are bounded negatives. The corpus screen's V8SetMessage
nomination is already recorded as a value-infeasible path; do not restart it
without new helper/return-boundary evidence. Audit the remaining nominations
against prior records and actual native value paths before proposing changes.

Assigned routing was Sol Extra High for both source investigations and Sol High
for fixture audit. Actual token/billing usage is unavailable. Current source,
native instructions and reproducible artifacts establish validation; memory
supplies historical decisions and their reopening conditions.
