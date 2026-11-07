# V34 quadbit, Phase34 and indication owners, 7 November 2026

This wave adds **three EXACT functions** and preserves every previous
EXACT/REGALLOC function. The final census is **1,155 EXACT / 34 REGALLOC /
75 BYTES / 588 SIZE**, over 1,852 functions. Two of 300 verified objects change.
The deciding `make phase J=8` passes **399 period differential tests, zero
failures**, plus structural checks. The
[evidence ledger](evidence/quad-phase34-recovery-20261107.json) pins the frozen
inputs, compiler controls, complete-TU audits, fixture domains and gates.
Baseline is `391728ad5333dfc9469d85b643d7e25fe0fa2da7`.

| Function | Baseline | Selected | Outcome |
| --- | --- | --- | --- |
| V34XF_IndicateJdReceived | SIZE4, 146 bytes | **EXACT150** | New EXACT |
| V34XF_IndicateDilReceived | SIZE42, 108 bytes | **EXACT150** | New EXACT |
| V34XF_IndicateK56FlexRateDetermined | SIZE1, 63 bytes | **EXACT64** | New EXACT |
| V34XF_IndicateK56FlexJdReceived | SIZE30, 217 bytes | SIZE15, 232 bytes | Supported owner recovery; coupled Rate gain |
| v90Phase34 | SIZE99, 1,259 bytes | BYTES47, native 1,358 bytes | Partial flow recovery |
| txmitquadbit | SIZE21, 432 bytes | SIZE3, 450 bytes | Partial lifetime recovery; native 453 bytes |

The last three are not additional promotions. Both complete strict partial
links remain DIFFERENT. No profile, ABI, classifier or tolerance changed.
Mutation/anchor/snapshot work and the separate modern portability gate were
not run. Assigned routing was Sol Extra High for quadbit/Phase34 and Sol High
for indications; actual usage/billing is unavailable, with no blind model
efficiency comparison claimed.

## Indications: recover actual owners and review the coupled result

Native Jd at 0xa274 and Dil at 0xa30f retain engine root+4. Their receiver-state
publication is engine+0x248 = root+0x24c, width four. Rate at 0xa6e8 similarly
uses the engine for root+0x250, width four. Dil independently retains TX root+
0x221c at 0xa316 and clears TX+0x3a4 = root+0x25c0, width two.

Jd's actual engine member alone gives EXACT150. Dil's complete crossed control
is baseline SIZE42, engine only SIZE17, TX only SIZE9 and both EXACT150. No
branch-syntax change is needed for those promotions. All changed uses preserve
their complete checked addresses and widths; no new type or owner extent is
introduced. Existing SetV90 remains EXACT274.
These checked views do not identify a unique original class or union declaration.

Rate's actual owner alone gives REGALLOC64. The minimal core containing Jd,
Dil and Rate retains that register-only result. A preceding K56Jd owner recovery
then makes Rate EXACT64. Native 0xa5e9 retains RX root+0x264 and 0xa5ef retains
TX root+0x221c across its diagnostic/setup call. The selected K56Jd uses actual
RX constellation root+0x382, width two, then resets TX count/previous/scrambler
at root+0x25c0/+0x25c6/+0x25cc, widths 2/2/4. Its other algorithms and calls stay.

K56Jd remains inexact. That is not a reason to reject independently witnessed
owner recovery and the additional exact Rate result. This wave measures the
coupled emissions and validates the complete TU; it does not attribute this
particular Rate interaction to an unobserved compiler cursor. Optional DC-owner
and native-oriented branch controls are retained separately. They are unnecessary
for the selected gain and bring additional consumer review obligations.

Twenty full-TU indication cells retain all 56 functions and 35 protected copies.
Native offsets, call boundaries and literal ordinary/dump parity support the
admission. The existing wider K56Jd and indication sweeps remain labelled
synthetic where they plant state; new constructed simple-indication histories
are described below.

## Phase34: ordered dispatch and narrow publication

Native 0x9bf3/0x9c03/0x9c10 retain RX root+0x264, TX root+0x221c and engine
root+4. Native 0x9c26..0x9c68 tests states 3..10 in order. The old source switch
created an eight-entry table. The selected if-chain restores selector/continuation
identity and the default return; it does not prove a unique authored spelling.

All TX uses stay within the measured 0x3c4-byte owner: count root+0x25c0,
previous/current quadrants +0x25c6/+0x25c8, scrambler +0x25cc and point +0x25d0,
widths 2/2/2/4/4. Bit-provider output extent is two bytes. RX constellation stays
root+0x382, width two; engine state stays root+0x24c, width four. Every call
continues to receive the complete root.

| Staged crossed control | Results |
| --- | --- |
| Baseline / TX owner / ordered flow / both | SIZE99 / SIZE95 / SIZE16 / BYTES625 |
| Actual RX only / direct counter only / both, after owner+flow | SIZE12 / SIZE12 / BYTES55 |
| Direct constellation consumption | BYTES54 |
| Actual idle-dibit member publication/consumption | BYTES47 |

Direct narrow counter consumption restores native publication and HI decisions:
signed <=127 in states 3/6 and equality16 in 4/7. The count is published before
the state3 diagnostic, then cleared after state transition. State5 consumes its
literal-mode idle scrambler result through the actual quadrant member. It cannot
share an ordinary emitter blindly: those emitters use the flags-selected
polynomial and differential encoding; the idle arm does neither.

Native and selected have **291 decoded / 289 reachable instructions**, the same
frame and all **45 branch/return sites** matching. Complete operand/instruction
multisets agree except the strict local-call immediate. Remaining permutations
stay within basic blocks: entry disjoint owner/session reads and constant-address
arithmetic, or four immutable vect4 loads moved across narrow count increments.
Call arguments, persistent publications and relevant flag/value lanes were
reviewed at every branch/call. Corresponding opaque callees must agree on the
stated valid complete-object domain; this is not an exhaustive equivalence proof.

The table/count pairs have native order through bbro; sched2 moves the table
loads. The entry schedule is also independent. No original-compiler trace or
unique original declaration is recovered by those observations.

The remaining getMP immediate is **.-2448 native versus .-2352 selected** at the
same function-relative callsite, resolving to the same local entry. Its 96-byte
layout debt is before Phase34:

| Intervening extent including padding | Source minus native |
| --- | ---: |
| getMPrecvdBits | +32 |
| v90RateRenegSilence | -80 |
| v90RateReneg | -48 |
| Total | **-96** |

Changing only the caller cannot close that immediate while preserving its native
callsite and these frozen extents. Recover the preceding source; keep the strict
grade and do not introduce padding or canonicalize away the distance.

The obsolete eight-entry Phase table is removed. Main .rodata becomes 324 from
356 bytes, with 81 remaining data fields instead of 89. Every remaining target
retains its exact function-relative continuation/case identity and instruction
boundary. Progress's six table bases move by 32 bytes; those paired tables and
selectors were checked explicitly. Valid target addresses alone would not be
sufficient. This is recovered source/data structure, separately from a byte win.

## Quadbit: two expansion lifetimes and a commoned narrow field

Fresh ELF measurement gives native size **453**, baseline 432. An initial brief
incorrectly subtracted the SIZE21 gap and assumed native411; no result was
interpreted using that assumption. The measured native object remains authoritative.

Native retains the actual TX owner and passes the actual scrambler member at
both expansions. The old source copied the state to an automatic local twice
and captured Boolean mode once. Earlier owner/result/flag-local controls had
not recovered the native frame. This wave tests pointer lifetime, actual member
reads and result consumption together, after reproducing the unchanged HS TU.

Repeated actual `tx_flags` reads form a commoned non-user HI pseudo. An explicit
short flags local had become SI after promotion. Passing the actual member
address separately at each call restores native frame44 and its four homes:
flags at SP+0, second pointer +0x14, first pointer +0x18 and original bits +0x1c.
A named shared pointer instead retained frame60 in the tested context; flat
compatibility flag reads collapsed the frame. Those are compiler observations,
not a uniquely recovered original local/type declaration.

The selected source uses the declared scrambler member at both calls and signed
current-quadrant consumption. Full first-result modulo-four masking proves its
0..3 range; the second scrambler changes a distinct four-byte field. The two
mode expressions retain the same bit-zero inversion for every flag word.
Actual first-pointer root/member spellings were raw-identical in the recovered
lifetime context, so no artificial split pointer is retained.

The selected body is **450 bytes, SIZE3**, versus native453; 147 versus149
decoded instructions, **43 blocks / 64 edges** on both sides. Native keeps
MOVZX then a separate MOVSWL at the final join; selected signed consumption
fuses the extension. Another differing instruction is alignment. First-join
LEA/ADD, register-dependent publication and scheduling differences remain
explicit in the complete path/lane review. A 451-byte diagnostic remains
unselected; the one-byte distance preference does not establish its declaration.

Both native and selected publish the same scrambler states, current/previous
quadrants and packed point at the corresponding boundaries. Complete call/value
review covers the flags, low/high lanes, both mode branches, return transport and
the sibling jump to txmit. Native-compatible frame/CFG shape alone is not
REGALLOC. The guarded shared-helper capture experiments remain unselected:
they lose the already exact public V34scrambler and do not close quadbit. This
does not rule out every legitimate helper boundary.

## Complete TU and fixture evidence

The two changed objects contain **109 emitted functions / 85 native-shared
copies**, with **47 protected EXACT/REGALLOC copies** and no losses. Nine bodies
change: the three promotions, K56Jd, Phase34, quadbit, Progress's paired table
bases/resolved call fields, k56FlexPhase34's resolved getMP field, and three
resolved handshak call fields. All other bodies remain literal-equal. Exports,
binding, visibility, function inventories and emission order remain.

Parent independently recompiles unchanged and selected full TUs, reproducing
all four frozen/production objects literally. All 533 frozen input hashes and
300 baseline object hashes were checked; current 300-object closure changes only
the two named TUs. Period Gentoo GCC3.4.2-r2, its executed assembler, complete
retained flags and mandatory bug-reproduction define govern every control.
The ledger keeps actual parsed-byte and transitive helper hashes separately.

Main retains all 226 PC-relative transfer identities, with 225 actual CALLs
counted separately. Phase's physical call order changes to native with its
dispatch layout; other ordered lists remain. One obsolete table-load relocation
disappears. HS retains 2,347 canonical relocation fields and 538 relocated plus
55 resolved ordered call targets; quadbit's ordinary call becomes the native
sibling jump. Nontext changes are the removed Phase table and validated moved
code-target addends; retained strings/data payloads agree.

The expanded `t_vpcmcreate` passes unchanged source first and has **17,489 checks
in 14 groups**, adding **2,387** this wave:

- Existing point-prefix histories gain native public 2400/3429-baud setups beside
  600 baud: 128 added prefixes / 1,152 added target calls / 2,048 added checks.
  The complete point group has 192 prefixes and 1,728 calls, all active, with
  queue capacity checked after every call.
- Simple Jd/Dil/Rate indications gain 42 constructed cases / 84 calls / 339
  checks, all active, including both debug settings and constructor roles.

These compare all 126 graph regions / 265,992 bytes raw, plus transcript bytes,
callback boundaries and capture completeness. No internal state is planted.
These are component histories, not claims of negotiated protocol reachability.
The 14-group / 112,962-check Phase34 arm fixture plants flags/state/provider
chains and remains synthetic exploratory fidelity coverage. A constructor Ja
history alone would not establish DATA/TRN_WATCH or dispatch states3..10.

The final clean-helper graph screen selects 41 inexact copies. Owner admits
31/refuses10, comparing 260 relations with48 ambiguities/23 unpaired anchors;
stack admits13/refuses28, comparing six relations with15 ambiguities/no unpaired
anchors. Zero nominations is limited to those supported subsets. The complete
manual native/operand reviews provide the stronger target-specific evidence;
unsupported effects are not silently treated as equivalent.

## Next discriminating work

Audit getMPrecvdBits, v90RateRenegSilence and v90RateReneg together against native
control/read/publication boundaries and their existing negative records. They
now have a measured dependency role in Phase34's strict local-call layout.
Recover their real algorithms instead of tuning caller placement.

For quadbit, reopen with a distinguishing source/helper/liveness constraint for
the final HI-to-SI join and first result transport. Repeating the closed flag-
local, pointer-name, result-width or Boolean-synonym families is not a new
hypothesis. K56Jd's remaining arm merge and reset scheduling likewise need a
fresh native-backed boundary. The qualified reports and all losing controls
remain under `/mnt/zfs/dev/slmodem/scratch/quad-phase34-wave-20261107`.
No complete-object equivalence, unique source preimage or global optimum is claimed.
