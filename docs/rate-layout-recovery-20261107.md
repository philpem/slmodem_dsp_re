# getMP and rate-renegotiation recovery, 7 November 2026

This wave recovers **getMPrecvdBits EXACT895**, preserves every existing EXACT/
REGALLOC function, and closes the measured 96-byte layout debt before Phase34.
The two rate transmitters now have native extents and instruction operands,
but remain BYTES because independent operations have different schedules.

Baseline is `451530e296aa105d35821628f7a90a39180ce9b7`. The retained Gentoo
GCC3.4.2-r2 profile, mandatory `DSPLIB_REPRODUCE_BUGS`, headers, function order,
ABI and classifier are unchanged. [Compact evidence](evidence/rate-layout-recovery-20261107.json)
records parsed input hashes, controls and validation. Complete commands, dumps
and objects remain in `/mnt/zfs/dev/slmodem/scratch/rate-layout-wave-20261107`;
parent gate artifacts are in the working checkout's `build/rate-layout-wave`.

| Function | Baseline | Selected | Outcome |
| --- | --- | --- | --- |
| getMPrecvdBits | SIZE27, 922 bytes | EXACT895 | One EXACT promotion |
| v90RateRenegSilence | SIZE83, 900 bytes | BYTES14, 983 bytes | Partial recovery |
| v90RateReneg | SIZE48, 507 bytes | BYTES13, 555 bytes | Partial recovery |
| v90Phase34 | BYTES47, 1,358 bytes | BYTES46, 1,358 bytes | Native local-call distance restored |

Global census moves from **1,155 EXACT / 34 REGALLOC / 75 BYTES / 588 SIZE** to
**1,156 / 34 / 77 / 585**, over the same 1,852 symbols. There is no REGALLOC
promotion. BYTES progress is reported separately from the exact gain.

## getMP: owner, publication and helper boundaries

Native retains the engine at root+4, with receiver at engine+0x248 =
root+0x24c, width four. Received MP starts at session+0x1744: six char fields,
one rate-mask short and six coefficient shorts, twenty bytes through 0x1757.
The selected interior view starts from the complete session, derives offsets
from its actual declared members, and preserves types and alignment. It makes
no claim about an original standalone MP declaration. A separate period layout
control checks all sixteen relevant offset/width/extent obligations and is
raw-identical to the selected object.

The initial narrow word is published at root+0xaa0c before the seven coefficient/
mask words, then the rate nibble is ORed down and published again. Both DMA
initialisations and seven independently guarded diagnostics remain. The
diagnostics retain their entry session. The existing max-upstream getter
reacquires session from root+0x3548 after the second DMA opportunity and those
diagnostics. The old duplicate retained the earlier session across that boundary.
The shared getter's complete algorithm and diagnostics match the native inline
expansion; its Index name does not change its rate units. Both public getter
copies retain REGALLOC123.

Mask initialisation belongs after that getter and before the rate guard. Native
code duplicates it into the diagnostic exits before edprintf. The four-step
capability loop uses short counter/mask transport, zero-bit-first arms and
branch-local capability reads. `mask * 2 | 1` is raw-identical to the explored
shift spelling and avoids a negative signed left shift. The consumed masks are
-65,-129,-257,-513; the counter covers 0..4.

Source publishes `paa6c = &info_caps` before initialising `info_caps`. The
compiler emits the native value-before-pointer stores. The opposite source
order is BYTES27: at rnreg the pointer and constant receive different hard
registers, which then change sched2 dependencies. Ordinary/dump controls agree
literally. This explains the measured current-compiler difference without
identifying unique original source or invoking a persistent scratch cursor.

| Selected property removed | Grade |
| --- | --- |
| Engine owner | SIZE16 |
| Shared fresh getter | BYTES119 |
| Narrow MP form and first publication | SIZE16 |
| Narrow branch-local loop | SIZE24 |
| Checked MP input owner | SIZE31 |
| Zero-bit-first source arms | BYTES52 |
| Mask initialisation after getter | SIZE12 |
| Source pointer-before-value publication | BYTES27 |
| All properties retained | EXACT895 |

The helper investigation retains 72 declared family cells: 32 initial factorial,
12 final-boundary, 9 mask-boundary, 10 capability-view and 9 selected-ablation cells.
Including controls/dumps/layout checks there are 77 valid retained objects,
75 source hashes and 52 object hashes. Rejected generator/driver invocations
remain labelled apparatus failures. Native and selected have 203 decoded
instructions, 200 after removing three padding instructions. All28 relocation
fields are checked in the exact verdict.

## Rate pair: coupled owners and narrow consumption

Native independently retains TX root+0x221c, RX root+0x264 and engine root+4.
Existing owners preserve count root+0x25c0 width two; flags+0x25c2 width two;
previous/current quadrant+0x25c6/+0x25c8 widths two; scrambler+0x25cc width four;
point+0x25d0 width four; and constellation+0x382 width two. All calls continue
to receive the complete root or actual declared output member.

The eight owner combinations are measured together. Incomplete combinations
lose SetV90 EXACT; TX+engine and RX+engine also lose Jd/Dil EXACT. Actual
TX/RX/engine ownership repairs those losses together. The intermediate
regressions are preserved in the experiment matrix. All owners alone leave
both targets SIZE1. Direct narrow count consumption and Silence's actual idle
quadrant member then recover both native extents.

Native increments a register and compares AX without extending a named short
result. Direct `++tx->seg_symcount` produces that transport even though source
initially publishes before the test: compiler sinking/commonisation yields
native's compare-then-shared-store tail. A separate short local adds SI
transport and sign extension. Machine-store timing alone would have rejected
the successful source form incorrectly. First-increment spelling crossed with
direct/captured selector gives four raw-identical controls.

Silence has 215 decoded instructions and 28 matching branch/return sites;
Rate has 123 and 17. Manual complete-arm review checks both constant-pair
comparisons/resets, provider result preservation across output calls, all
completion actions and default dispatch. Silence18 emits CP before testing
done, then publishes state19, unfreezes EC and clears SAS. Silence19 retains
literal mode0/no differential encoding, its distinct fallback, fresh post-txmit
count read and previous-quadrant publication. Silence20 sets freeze before
every provider call. Existing emitters already implement the genuinely shared
operations; no new artificial helper or carrier is introduced.

With exact getMP coupled in, the complete instruction multisets agree with
native, including strict local CALL operands. Two permutations per function
remain: immutable vect4 loads move across disjoint count increments/stores,
sometimes taking the point store with them. They remain in the same basic block,
with no intervening call/observation. MOV does not change flags. Captured bbro
order 61,62,63,68,69 becomes sched2 order 61,68,62,69,63 in the first group. This
is a measured transformation, not proof of allocator impossibility or exhaustive
equivalence. The review assumes valid complete owners and corresponding opaque
callee behaviour; nested/aliased effects outside that model remain unproved.

Pair controls plus helper-only/combined ordinary/dump controls total 25 cells,
21 source hashes and 16 object hashes. The helper-only build independently
reproduces `c07b8e59e91caede06f4b3d8c25e3474eb647f63c13c0d3dfa1920a43db7433a`.
The final combined source uses defined mask multiplication; its ordinary/dump
object is `3da40203496ffaf390420ae9d1b06c05752572d9246ff083ad4e39d8d6ed44cc`.

## Actual layout debt and complete-TU effects

| Padded extent relative to native | Baseline | Selected |
| --- | ---: | ---: |
| getMP | +32 | 0 |
| Silence | -80 | 0 |
| Rate | -48 | 0 |
| Sum before Phase34 | -96 | 0 |

Strict local getMP distances now match: Silence.-896, Rate.-1888 and
Phase.-2448. They remain in the strict classifier. Phase's other same-block
permutations are unchanged; another exact function did not automatically repair
their schedules. k56FlexPhase34's resolved getMP field also changes while its
SIZE28 classification remains.

Independent parent checks retain all 56 Main functions, their order and exports,
and all 38 baseline EXACT/REGALLOC copies. Only five canonical bodies change:
getMP, both rate functions, Phase and K56Phase; the latter two changes are only
resolved local-call fields. All 225 CALLs preserve ordered identities. All 81
nontext relocation targets retain exact containing function and relative
continuation, not merely instruction-boundary membership. Data extents and
non-address payloads are retained; moved .rodata code addresses are audited.

Two independent full-TU builds reproduce unchanged baseline and selected
production objects literally. Across 300 source objects, only Main changes.
Both 300-object partial links use 279 anchored inputs and 21 explicit source-order
inputs. Both remain structurally and raw DIFFERENT from native. Native .text
is 728,304 bytes; baseline/selected are 714,489/714,585. This 96-byte growth recovers
the actual local debt; absolute positioned-byte equality remains a layout-
sensitive diagnostic, not an exact-function score or whole-object victory.

The changed-TU graph screen selects 21 inexact copies. Owner admits 19/refuses 2,
compares 251 relations across 14 copies, with 32 ambiguous and 10 unpaired anchors.
Stack admits 9/refuses 12 but compares zero relations, with 18 ambiguities. There
are zero nominations only within those supported subsets. Unsupported nested
owners, opaque callee effects, cyclic/value relationships and ambiguous matches
prevent an equivalence claim. Parsed object/transitive-helper hashes are retained.

## Differential histories and validation

The new fixture passes unchanged source first, then the final combined source.
The final **make phase J=8 passes 399 period tests, zero failures**, plus structural
checks. `t_vpcmcreate` now has 28,407 checks in 16 groups, adding 10,918 checks.
Modern portability, mutation suites and snapshots are not run or refreshed.

Rate coverage has 288 paired prefixes/2,016 target calls through native Create,
modeminit, optional public modulator setup and public SetV90. It spans both
roles/selectors/constellations, debug 0/2 and 600/2400/3429-baud setups. Six calls
retain setter-created state 11/15, count 2..12 and queue 32..230. An eight-call
exploratory grid exceeded 600-baud capacity: 96 failing bounds among 17,283 checks
are preserved as an invalid-domain result. No source was changed to fit it.
Threshold transitions and states 12..14/16..20 remain outside this new domain.

MP coverage has 16 paired packets/32 helper calls/259 checks. The fixture supplies
twenty public message bytes and group 36 to the native-owned V90MP component,
then native infoToBits/reset/bitsToInfo accepts exactly one complete 216-bit Type1
packet at its last bit. All twenty decoded bytes are compared with the supplied
payload. Native copyMpInfoForInterface and SetV90 produce the helper's session
and receiver 11 entry. The actual private ABI uses root in EAX: both fixture
declarations use regparm(1), confirmed in native/reconstruction/callsite code.

Both new groups compare all 126 live heap regions, 265,992 bytes, raw, plus debug
transcripts/completion/callback bounds; no learned exclusions apply. These are
constructed component histories. They do not establish negotiated modem
reachability, Type0, receiver<=0, sensitive ISP, low-rate capability-loop paths
or callback-driven session replacement. Existing later-arm planted probes remain
synthetic. Native instruction identity provides separate evidence for exact
getMP; the bounded runtime domain is stated independently.

Assigned routing was Sol Extra High for both source investigations and Sol High
for fixture audit; independent cross-builds and parent validation check their
outputs. Actual token/billing usage is unavailable. Hindsight was queried before
experiments and commitment; historical records supplied no current validation.

## Next investigations and reopening conditions

Start with **V34XF_IndicateTrn2dReceived**: current source is 87 bytes against
native 127 and still uses the flat root. Fresh native instructions retain
engine root+4 at 0xa397, then use engine+0x248 for the state and debug reload.
Its last arm is SETG/MOVZBL/LEA at 0xa403..0xa409. Audit owner and predicate/
dispatch boundaries together, crossing supported changes and affected neighbours.
This is a nomination, not an already demonstrated promotion.

Hindsight's Full residual-family screening and DIL/MP dataflow recovery page
reports an earlier Trn2d recovery reverted after K56RateDetermined changed from
EXACT to BYTES4. Its cited F12052 is absent from the current findings document;
that historical source variant has not been reproduced here. Current frozen
Trn2d is SIZE40 and K56Rate is EXACT64 after the preceding wave's coupled K56Jd
owner recovery. Recover the old variant and cross it with that current owner
model before deciding whether the earlier loss still applies. An isolated
neighbour regression requires analysis and coupled repair, rather than an
automatic veto. The reported historical loss supplies no current validation.

Then compare **k56FlexPhase34** with the recovered Phase/rate ownership and
narrow-consumption mechanisms. Its helper distance is repaired but SIZE28
remains. Read the historical multi-base/idle findings and current Playbook
before transferring anything; its different fallback/encoding contract prevents
blind helper substitution. Audit callable construction/queue-drain histories
before extending the rate fixture through its 128/16-symbol thresholds.

Reopen the pair's scheduling groups only with a distinct declaration, helper
boundary or liveness witness predicting their dependencies. The exact getMP
coupling and first-increment/selector family have already been tested. Preserve
the Phase/quadbit reopening conditions in the preceding report. Existing exact
neighbours may be changed by independently supported reconstruction, with losses
traced and coupled repairs tested before selection.
