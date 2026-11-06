# V34 Delays and owner transfer, 6 November 2026

This wave adds **two EXACT and three REGALLOC functions**, with no existing
EXACT/REGALLOC losses. The final census is **1,150 EXACT / 32 REGALLOC /
72 BYTES / 598 SIZE**, over 1,852 functions. All 300 source objects were rebuilt;
299 remain literally unchanged. The independently rebuilt main object agrees
literally with the frozen selected candidate. The
[evidence ledger](evidence/delays-retrain-recovery-20261106.json) records the
source/header/compiler hashes, crossed controls, fixture counts and gates.

| Function | Baseline at 4be0be421 | Selected |
| --- | --- | --- |
| VPcmV34SetDelays | SIZE26, 233 bytes | **EXACT259** |
| VPcmV34SetMaxBlockLength | SIZE3, 39 bytes | **EXACT42** |
| VPcmV34SetTimeOut | SIZE3, 27 bytes | **REGALLOC30** |
| VPcmV34GetMaxUpstreamRateIndex | SIZE3, 120 bytes | **REGALLOC123** |
| V34XF_GetMaxUpstreamRateIndex | SIZE3, 120 bytes | **REGALLOC123** |
| VPcmV34InitiateRetrain | SIZE11, 1,395 bytes | BYTES31, native 1,406 bytes |

Retrain remains incomplete and is not a sixth promotion. There are 34 EXACT /
REGALLOC functions in this 56-function TU after the change, previously 29.
The exact count is 30 and the REGALLOC count is four. These dated measurements
are not permanent counts.

## Delays: one root identity and a real scalar member

Native Delays retains root ESI and session EDI, acquired at 0x63ff. It never
retains engine+4. Filtdelay at root+0xaa7c and dmadelay at root+0x25c remain
short accesses. No field type, arithmetic, diagnostic, public ABI or call
boundary changes.

The flat compatibility union makes dmadelay references conservative alias set
0. A short reference to the actual declared member restores scalar metadata,
alias set 7, and debug PRE across its store. That alone reaches BYTES116.
A second property is needed: the initial config read through a cached cast
pointer and later reads propagated through the formal occupy different GCSE
expression entries. Repeating the root-member expression through the formal
unifies them. Direct root access alone remains SIZE26; both give EXACT259.

| Complete-TU control | Grade | PRE substitutions / created instructions |
| --- | --- | --- |
| Baseline | SIZE26 | 1 / 2 |
| Actual dmadelay reference | BYTES116 | 2 / 4 |
| Direct root expression | SIZE26 | 2 / 3 |
| Both | EXACT259 | 3 / 5 |

Four ordinary/dump builds reproduce literally. Fourteen declared source cells
produce eight distinct objects; filter reference, root reference and named
embedded dmadelay controls do not recover the missing combination. Manual config
caching removes native reloads and is rejected. The instruction match validates
the observed ownership/alias/lifetime behavior, not unique original reference
syntax.

The native debug value is sampled at 0x63e3. Config survives the filter store
on the first quiet edge. After the filter diagnostic, config and debug are
reloaded at 0x6475/0x6480. The second debug decision therefore uses either the
retained value or that new value, according to the preceding edge. After a DMA
diagnostic, config is reread at 0x64b5; both continuations reread dmaDelay before
adding 0x68 and calling the captured session's echo canceller. The exact body
preserves both cold/hot calls, returns and all eight relocation fields.

Inlining propagates this recovered debug/config reuse into Retrain. That is a
measured shared-algorithm effect. It does not establish that a persistent
allocator cursor caused the improvement.

## Transfer to the small functions

SetMaxBlockLength native 0x6508 retains root+4, then publishes ptc at engine+4
(root+8, width four). Actual engine membership restores the entire 42-byte
function without changing another body.

SetTimeOut native 0x6e90 computes the wrapped deadline before acquiring its
engine owner and clearing the sample count. Owner alone gives BYTES20; early
product alone remains SIZE3; both give REGALLOC30. Root+0x238 is cleared and
root+0x23c receives the 32-bit product, each width four. The formal remains
int, as its mangled name specifies; multiplication stays unsigned to preserve
all 32-bit wrapping. A later formal-identity control is literal-neutral here,
unlike Retrain; no variable-name or declaration palette is retained.

Both MaxUpstream copies have independently checked native root+4 witnesses.
Only the v90_receiver read changes to the actual engine view, still root+0x24c,
width four, signed >1. Config/session sampling, the sensitive-ISP bound and
both diagnostics stay intact. Each function reaches REGALLOC123 independently
and together. Their register assignments remain different from native.

The combined five promotions preserve every other body except the inline
Retrain consumer. They do not automatically promote another neighbour. A
separate RateReneg control is whole-object neutral for typed engine access;
session-before-receiver order improves its own BYTES39→31 but changes no later
function. Neither control is retained. This tests the possible TU interaction
without presuming that every byte improvement changes persistent allocation.

## Retrain: recovered constraints and remaining bytes

The retained model groups the existing DC count16, estimate16 and accumulator32
at engine+0x250/+0x252/+0x254, root+0x254/+0x256/+0x258. All three accesses use
one actual eight-byte POD member. Header-only is literal-neutral on the tested
complete TU. Actual common ownership restores native DC instructions
+0x2d1..+0x30f, including arithmetic and the independently scheduled clears.
Mixing flat count access with typed clears does not recover that block.

The fields have one textual home. A guarded type header supplies their complete
type to both the engine fragment and the root header. GNU C89/C++98 probes and
permanent assertions cover flat/typed root offsets, group offsets 0/2/4,
widths 2/2/4, extent eight, engine extent 0x260 and root extent 0xac4c. This is
an inferred owner/alias model; the original type name and compatibility union
remain unresolved.

Removing a redundant byte-formal copy restores the native saved request and
entry diagnostic transport. The formal remains unsigned char, independently
of the first switch's retained int conversion for its numeric diagnostic.
The default diagnostic keeps its independent conversion. Native session-before-
receiver acquisition restores session+0x14 and receiver+0x10 frame homes.
The receiver selector is captured before short/verification publications at
the native boundary; other selector reads remain at their own uses.

The investigation measures 59 complete-TU cells and 3,304 per-function grades.
New DC ownership plus the formal identity is an interaction: the earlier raw
group with formal reuse remains BYTES172; the actual group reaches BYTES42,
and session/receiver order reaches BYTES31. A further engine declaration-order
control is neutral. Losing controls are retained; they do not establish a
universal source optimum.

The final comparison covers 64 reachable blocks, 97 typed edges, 35 guards,
14 calls and 305 reachable instructions. All 30 relocation fields have strict
site/type/width/addend/target validation, with positive and negative controls
in both Python modes. This is complete CFG and residual review, not exhaustive
execution of every value-feasible path. The bounded graph screen compares
185 owner relations and zero stack relations; its clean subset is not an
equivalence verdict.

Twenty-four paired instruction rows account for the remaining 31 bytes.
Config guards, K56 constants, V90 arguments and handshake/reset work use
different regional scratch lanes. Native status CMP and receiver publication
are reversed, but occupy disjoint root fields, so this reordering alone cannot
change the read value within a valid complete object. Detector y2/x1 clears are
also disjoint, and MOV preserves the role-comparison flags. Public reset is
already EXACT118. Status snapshots, alternate joins, actual-member references
and direct-root spelling do not recover the full native graph. No store-order
grid or relaxed REGALLOC classification is justified.

A new compiler-dependency or original alias/liveness witness is needed to
resolve those constraints. The retained partial source supplies clearer
ownership and native transport; it is not accepted merely for the smaller gap.

## Complete objects, fixtures and gates

Only the main TU changes among 300 objects. Its 56 functions remain present,
with six changed bodies, no missing/new definitions and no protected losses.
The selected object SHA256 is
`33ef73f9052e399907b5dd8f200545640be7ce6f4ca0c6412c9606e8972cd940`.
Exports, bindings, function order, allocated relocation-zeroed data and all
89 ordered code-data destinations retain their reviewed identities and
instruction boundaries. Relocation counts/addends are recorded separately
from that target identity; complete relocation tables are not raw-identical.

Both the old endpoint and selected source pass the existing meaningful focused
period checks: t_v34pcmapi 30,536 checks in nine groups; t_v34pcmif 63,365 in 38;
t_v34retrain 22,645 in ten; t_vpcmcreate 12,368 in nine. State-free timer/length
and readonly getter tests include boundary values. Retrain's patterned states
remain synthetic; the constructor-driven cases establish status-0 component
histories, not negotiated PCM reachability. The previously added direct Delays
helper cases compare the whole constructed graph immediately after each call.
No fixture, tolerance or retired mutation suite is changed in this wave.

The five-promotion intermediate passes make phase with 399/0 and structural
checks. The first DC-model endpoint also passes 399/0, but the all-header
structural compile exposes an incomplete-type dependency in its field fragment.
The guarded DC type header fixes that dependency without a gate exception.
All 300 repaired objects are literally equal to the preceding selected objects.
Offset checking then reports 2,225/2,225 and the period assertion audit sees
3,898 live assertions. The final `make phase J=8` passes 399 period tests, zero failures, and all
structural checks; its log and exit status are in the evidence ledger.

Strict whole partial-link comparisons are DIFFERENT before and after this
wave, including raw bytes; full-object completion remains open. Both partial
links contain all 300 inputs, ordered by the recorded attribution procedure.
Modern portability is not claimed. Sol High handled Delays and small transfers;
Sol Extra High handled interacting Retrain source/type constraints. Usage and
billing were unavailable; this is not a blind model-efficiency comparison.

The Playbook records the successful crosses and negative transfers. AGENTS now
separates address identity from alias metadata and formal/local identity, and
corrects its stale type-duplicate count against onedef's empty debt register.
The old active instruction to maintain mutation anchors was removed from the
owner-modelling section of the Playbook, consistent with retirement policy.

## Next discriminating work

1. Transfer the Delays root-identity/member-reference lesson to Create and
   ApplyBulkDelay, where the earlier header wave exposed measured GCSE effects.
   Require fresh native read-age witnesses and validate their entire consumers.
2. Investigate the real transmitter-control owner used by SetV90RateReneg.
   Its engine-only transfer remains SIZE22, and the existing txq ends before
   the native scalar-reset fields. Recover the full owner from constructor,
   allocation and native consumers before another source experiment.
3. Trace Retrain's remaining status/detector scheduling dependencies and regional
   live ranges against that settled TU context. Retain the explicit residual
   list and exact reset helper; do not restart a store-order or type palette.
