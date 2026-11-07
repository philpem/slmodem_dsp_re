# V34 bulk delay and TX-owner recovery, 7 November 2026

This wave adds **v34FreezeEcho EXACT222** and **ApplyBulkDelay REGALLOC467**.
The final census is **1,151 EXACT / 33 REGALLOC / 73 BYTES / 595 SIZE**, over
1,852 functions, with no previous EXACT/REGALLOC losses. All 300 source objects
were rebuilt; six change. `make phase J=8` passes **399 period differential
tests, zero failures**, and the structural checks. The
[evidence ledger](evidence/create-tx-recovery-20261107.json) records hashes,
controls, collateral, fixture domains and the final gates.

| Function | Baseline at aa2133a1d | Selected | Native bytes |
| --- | --- | --- | ---: |
| v34FreezeEcho | SIZE15, 207 bytes | **EXACT222** | 222 |
| ApplyBulkDelay | SIZE46, 421 bytes | **REGALLOC467** | 467 |
| VPcmV34Create | SIZE121, 2,255 bytes | SIZE23, 2,353 bytes | 2,376 |
| VPcmV34SetV90RateReneg | SIZE46, 228 bytes | SIZE6, 268 bytes | 274 |
| txinit | SIZE22, 223 bytes | BYTES8, 245 bytes | 245 |
| settxlevel | SIZE18, 448 bytes | SIZE2, 464 bytes | 466 |

The last four are supported partial source recoveries, not promotions. No
classifier, tolerance, option profile or public ABI changes. No mutation suite,
anchor or snapshot maintenance was executed. Modern portability was not run.

## Four interacting properties in ApplyBulkDelay

Native retains the engine owner at root+4 across memset, consumes the original
signed short delay at entry, and falls through into the longer mask/DMA arm.
Its final two-byte root+0x25c DMA update retains the preceding debug value until
a diagnostic call requires a fresh read. The selected source expresses those
three properties and uses a pointer to the actual declared short member only
around that final update. The pointer compiles away to root-relative accesses.
Capturing it at entry instead introduces an unwitnessed call-crossing EBP life.

Seventeen ordinary cells produce sixteen distinct objects, with three additional
dump controls. Baseline and selected ordinary/dump objects reproduce literally.
Removing the scoped member pointer gives SIZE6; removing the engine owner gives
BYTES99; removing the narrow entry test gives BYTES100; reversing the long-arm
fallthrough gives SIZE23. All four together give REGALLOC467. The actual-member
store restores scalar alias metadata and debug PRE from 1/2 to 2/5 substitutions
and created instructions. This does not identify unique original source syntax.

All 17 relocation fields and the complete instruction/branch sequence match
under strict register alpha-renaming. Native has 108 decoded instructions and
105 reachable instructions. The four differing bytes at relative offsets
200/203/206/209 exchange independent EAX/ECX transport and zero lanes. The low
word consumers, sign extension, flags and far-echo clear retain their values;
neither differing scratch lane crosses a call. REGALLOC is justified here by
that residual review and the deciding differential gate together.

## The measured TX owner and the regression that led to EXACT

The previous generic queue prefix stops before the scalar reset fields. Native
SetV90 at 0xa14f, txinit at 0x5d685, txmit at 0x5d7c2, modeminit at 0x5f573,
settxlevel at 0x62561 and FreezeEcho at 0x5e212 independently retain root+0x221c.
The new `v34_tx_control` models the complete measured 0x3c4-byte group, including
the 230-word queue and scalar fields through root+0x25dc. Its final two bytes
are alignment. The original name and whether its declaration continued farther
remain unresolved. The root stays 0xac4c bytes; downstream offsets stay fixed.

The type has one field home and a GNU flat compatibility view. Sixteen permanent
member-pair checks cover complete offsets and widths, plus owner base/extent.
Period GNU89/GNU++98 layout probes and all-header structural checks pass. The
three initialized words at root+0x25d6/8/a retain neutral short names. No scalar
access is added beyond the generic queue subobject.

SetV90 crosses the independently witnessed TX and engine owners. Baseline,
TX-only, engine-only and both give SIZE46 / SIZE20 / SIZE22 / SIZE6. Header-only
initially makes handshak 15 bytes worse: two inlined FreezeEcho flag stores
become alias-zero effects and block reuse of the global debug value. PRE falls
from 74/107 substitutions/created instructions to 72/105. Native FreezeEcho
provides a fresh owner witness; migrating its actual flags member restores
those substitutions, makes its public body **EXACT222**, and improves handshak
from SIZE48674 through SIZE48689 to SIZE48636. The whole handshak remains far
from the native graph. Matching PRE counts alone are not an equivalence proof.

## Create: recover the source boundaries, then explain the remaining gap

Native Create reloads the session/K56 owner at +0x3c9/+0x3d6 for rate setup,
reloads config after the minimum-level publication, and acquires a fresh session
before delay setup at +0x4ac. The old duplicated constructor algorithms used
entry-captured locals. Calling the existing rate, minimum-level and delay helpers
recovers those observed read boundaries when inlined; the detector reset also
shares its existing body. Actual engine/DC members retain checked addresses.

Native dispatch tests ==2, signed <=2, ==3 and ==4. The old comment claiming
that this ruled out a switch was wrong; the selected switch restores that
decision tree and shared Flex tail. Byte-sized extraction of quick-connect bit
4 restores native QI shift and its frame slot without changing the bit value.
These are independently witnessed source properties, not adoption on size alone.

Twenty-six initial cells yield no Create promotion. The selected body has 505
reachable instructions versus native 506. Its initial engine pseudo and the
inlined helper's engine pseudo occupy distinct GCSE expression entries; native
retains one engine base. Repeating formal-root expressions, successful in
Delays, worsens Create; using the formal just for initial engine formation is
literal-neutral. An owner screen compares only 36 relations with three ambiguous
anchors and excludes most reads/writes. It cannot establish equivalence or an
allocator-only residual. Reopen with a source/declaration or helper-boundary
constraint that distinguishes those expression identities.

One final declaration control replaces the complete `v34_object` tag with
`tagV34Object` throughout an isolated source/header closure, removing the
artificial distinction between the opaque helper formal and complete root.
Period compile-time checks establish that the types are distinct in baseline
and identical in the control. Both ordinary Main objects are literally equal,
including all 56 bodies, six native root manglings and 34 protected copies.
Create remains SIZE23. The canonical tag distinction is therefore not the cause
in this tested closure; no declaration rename is adopted. Reopen only with
additional declaration/alias evidence that changes the relevant compiler trees.

## Other residuals and collateral

SetV90 has native six blocks/seven edges/60 reachable instructions, versus
five/six/58 plus three padding bytes. Initial candidate RTL merges the two
constellation arm stores; native duplicates a seven-byte store and two-byte
jump. Subtracting candidate padding accounts for the six-byte gap. A native
zero survives preinitdigital while current source rematerializes it afterwards.
Equivalent explicit if/else controls do not restore that graph. A useful next
hypothesis must explain both the arm effects and call-crossing value lifetime.

txinit has one block, 58 instructions, five matching call targets/arguments and
a 28-byte frame on both sides. Source RTL through 31.bbro already clears the
scrambler, previous quadrant and count in native order; 33.sched2 first reorders
the independent stores. TX-only and RX-only owners each remain SIZE20; both
give BYTES8. Reopen with supported scheduling/liveness or real helper-boundary
evidence, rather than a source-store-order search.

settxlevel has 23 blocks/34 edges on both sides, with 124/123 decoded
instructions. A signed short remains narrow longer in native, and flag reuse
also differs. Preserve the root snapshot obtained before GetMinPR; a constant
member-address addition moving across that call alone does not change when a
mutable owner pointer was read. A supported consumed-value or helper-boundary
constraint is needed to explain this residual.

The six changed objects contain 130 native-shared function copies, including
50 protected EXACT/REGALLOC copies. Seventeen bodies change; all are listed in
the ledger. None ends with a worse grade or size gap. Equal-grade collateral was
reviewed for instruction effects and object assumptions: modem_serrint, txmit,
txmitdibit/quadbit, modeminit, DilReceived, Progress, k56FlexPhase34,
GetDiagnostics and initdigital. modeminit keeps its 1,012-byte body but now spills
its modulator owner, enlarging the frame from 28 to 44 bytes. GetDiagnostics
review assumes distinct modem/result objects, not arbitrary overlapping buffers.
The larger existing native flow mismatches remain explicitly open.

All global/weak export metadata, allocated section extents and local-call
destination identities remain. Strings and NUL-padding multisets match, though
their order changes. Main's 89 relocated data targets retain exact destinations;
handshak has 314 fields with 63 changed function-relative targets inside its
large state machine, all landing on instruction boundaries. Target validity is
not equality of every case-entry effect.

The final six-TU graph screen selects 73 inexact copies: owner layer admits
61/refuses 12, comparing 230 relations with 63 ambiguities and 46 unpaired
anchors; stack admits 16/refuses 57, comparing six relations with 22 ambiguities
and 23 unpaired anchors. Zero nominations describes those bounded subsets only.
Opaque calls, SBB/byte predicates, globals and large indirect graphs remain
unsupported. Manual residual review supplies the stronger function-specific
evidence above.

## Fixture and build evidence

The expanded `t_vpcmcreate` fixture passes unchanged baseline source first.
It adds 81 ApplyBulkDelay cases: factory plus native constructor histories for
both sides, debug 0/1/2, and nine nonnegative short delay values covering 29/30,
143/144 and 9599/9600 boundaries. Each checks all 126 observed regions,
265,992 bytes with no masks, plus transcript bytes and callback boundaries.
All 81 have an observable state change. These 162 calls add **813 checks**,
bringing that binary to **13,181 checks in ten groups**. Delay arguments are
component boundary probes, not evidence of negotiated round-trip delays. No
internal state is planted by this addition. Existing broad handshake synthetic
states remain separately labelled; its 43 groups contain 14,368,945 checks.

All 531 frozen source/header/compatibility inputs and 300 baseline objects were
verified. An independently rebuilt unchanged 300-object closure is literally
equal, after correcting an invalid assembly apparatus command. Gentoo period
GCC 3.4.2-r2, retained flags and mandatory DSPLIB_REPRODUCE_BUGS govern every
selected C/C++ build. Final parent objects agree literally with the independent
selected six-object closure. Before/after complete partial links use all 300
objects and strict raw/normalized comparisons; both remain **DIFFERENT**. This
wave makes no whole-object equivalence or global-optimum claim.

Create and Bulk investigations used Sol High; the TX causal investigation used
Sol Extra High. A final bounded canonical-root declaration control also uses
Sol Extra High. Usage and billing are unavailable; this is not a blind model
efficiency comparison. Retained scratch reports, controls and compiler dumps
live under `/mnt/zfs/dev/slmodem/scratch/create-tx-wave-20261107`; their hashes
are in the committed ledger.

Next, use the settled owner models to investigate SetV90's arm-store and zero
lifetime together, then settxlevel's narrow consumed value and txinit's sched2
ordering. Revisit Retrain only with a distinguishing source/alias/liveness
witness. Apply the coupled owner/inline lesson to other inexact consumers before
repeating already negative spelling families.
