# Graph-lead source recovery, 2026-11-05

This wave recovers **V34EchoAdapt EXACT79**, retains native operation/owner
boundaries in FPM_PPS_filter and v22_data, and leaves the other historical
V34 echo cursor family closed. The measured census is **1,137 EXACT / 28 REGALLOC / 71 BYTES / 616 SIZE**
over 1,852 symbols, with zero unresolved and no EXACT/REGALLOC losses.
The frozen source baseline is
`a9f9e35a98d184d406b0d0461c9a02c9576e5ad6`.

## V34: source induction closes the complete function

Native reads coefficient fraction (+0x0c), high coefficient (+0x08), then
history (+0x10) from the echo owner before testing taps (+0x1c). It copies
the count after the guard, then traverses advancing pointers. The retained
indexed loop allowed guarded pointer acquisitions. Capture those three pointers
in native order and retain ascending unsigned source induction while advancing
them. GCC's `09.loop` explicitly reports “Can reverse loop” and “Reversed loop”,
introducing the native post-guard count copy and decrementing machine loop.

| Complete-TU source control | V34EchoAdapt result |
| --- | --- |
| Baseline indexed loop | SIZE16, 95 bytes |
| Entry captures, indexed loop | SIZE7, 86 bytes |
| Historical manual countdown replay | BYTES23, 79 bytes, 33 displayed instructions |
| Ascending pointer loop | BYTES8, 79 bytes, native 34 instructions |
| Native fraction/coefficient capture order | EXACT79 |

Six source cells include five nonbaseline controls. The sixth is the historical
BackwardClean early-cursor replay: BYTES254→232 and its nomination disappears,
but it supplies no new property beyond the closed F11704 family, so it is not
selected. A detector finding alone does not reopen a prior source domain.

Among 26 TU functions, only Adapt gains EXACT; none lose EXACT or REGALLOC.
Its source change also changes two later machine bodies: BackwardClean
BYTES254→247 and TimingFilter remains SIZE100. Their parsed instruction patterns
agree through `27.flow2`; first changes at `28.peephole2` are scratch choices.
The intermediate `08.gcse` repeated-UID dump is explicitly unparsed, with equality
checked again at `09.loop`. All 48 named data objects, five allocated noncode
sections, 50 strong exports and function order remain unchanged.

The whole-TU owner diagnostic compares relations in six of 26 copies (53
anchors, 14 relations); nominations fall 4→1. Adapt's three disappear;
BackwardClean's persists. Exact energy remains a clean negative control with
three anchors/two relations. TimingFilter has zero compared relations, and
three changed-body stack analyses refuse the frame model. Complete native body
identity establishes Adapt's exactness independently of these bounded graphs.

## FPM: cross entry observations, selector lifetime and rail traversal

Twelve distinct full-TU sources test five nominated entry captures A, maps/ring
root captures B, unconditional low-byte symbol observation C, its wide index
carrier, then full-width rail counters × consumed-cursor wrapping. Native reads
the symbol byte before mapped/direct dispatch even in direct mode; the fixture
supplies a readable symbol array on both paths. `mapped` remains reread per
symbol, and `step_adj` remains read per output after publication.

A alone removes five nominations but remains SIZE12/741 bytes. Full root
captures initially increase the distance to SIZE76/829. On the selector/capture
baseline, full counters alone give SIZE42/795, cursor wrap alone SIZE19/772,
and both SIZE15/738 against native 753. Retain the combined native-backed graph,
including its three-byte distance regression from baseline. Two exact neighbours,
all allocated data and exports remain literally unchanged.

The final owner subset has 19 matched anchors and 18 compared relations,
zero nominations and zero unpaired anchors, versus 14/13 and five/five before.
Indexed effects, loop iterations and much arithmetic remain unsupported.
Baseline initial RTL reads members inside the loop; `08.gcse` PRE makes guarded
preheader captures. Explicit source captures instead exist before the guard in
initial RTL. This differs from the remaining I-rail load order: history then
coefficient survives through `31.bbro`, and `33.sched2` reverses them legally.
Tail comparison orientation already differs in `01.rtl`. Reopen with a new
source/helper/value-age witness; do not chase these residuals with operand or
helper spelling palettes. No behavioural defect follows solely from ordinary
reads moving across an empty-loop guard.

## V22: preserve ages when a closer candidate regresses the graph

Move local status initialization after V22_status. This removes the nominated
call/write timing difference, preserves all other TU bodies and remains SIZE10.
The five automatic status writes retain their widths and values.

A further native witness identifies values retained into the carrier-loss debug
message. Short captures reduce SIZE10→3; int captures give SIZE4. Both hoist the
limit read before counter publication, unlike native, and reverse comparison
operands/predicates; int also loses the native word comparison. Crossing native
carrier-absent fallthrough gives SIZE22 alone and SIZE5 with short captures,
without restoring those ages. These closer controls are rejected. Eight source
cells were tested in this parent domain; the matrix retains every endpoint.

The independent interface control reproduces two baseline TUs, then changes
Detect_Retrain's declaration and definition coherently to short. Caller output
is literally unchanged and still lacks native's dead CWTL. The callee adds
return sign extension (SIZE25→24); its source changes are excluded. Five
synthetic direct-component counts give native/int-identical full EAX values;
three high-bit results differ under the short contract. This does not uniquely
identify the original ABI, but supplies no recovery reason to change it. The
old v22ans.h claim that the casts reproduced every conversion is corrected.

## Validation, boundaries and reproducibility

Focused period tests pass five binaries: t_v22ans, t_v22modem, t_v22dp,
t_fpm_pps and t_v34ec. The V34 fixture includes 2,000 push/filter/adapt steps
and a 200-step one-tap history, comparing both coefficient halves and signed
error/carry transitions. FPM includes six initialized profiles × 48 stateful
calls, direct/mapped input, and an initialized zero-count history. Planted V22
states, FPM debt/full-scale probes and BackwardClean cursor placements remain
synthetic; none establish public modem negotiation reachability.

The [final evidence summary](evidence/graph-lead-recovery-20261105.json) records
gate logs and hashes. **Final make phase: 399 period tests passed, zero failed,
plus structural checks.** Focused binaries contain 14,555,912 checks across 45 groups.
Routine screening of all 39 shared functions in the changed TUs selects 21
inexact copies before and 20 after; nominations decrease 10→1. The surviving
BackwardClean nomination remains explicitly parked. Owner/stack actual relation
coverage is reported separately, and exact Adapt leaves the inexact selection. Fifteen of 49 FPM
mutation anchors are reanchored with the same labels and fault intent. No
mutation execution or snapshot refresh is part of this result. The first phase
attempt exposed these detached anchors; its failure is retained.

Selected production objects reproduce frozen candidates literally:

| TU | SHA-256 |
| --- | --- |
| fpm_pps | `1cd5d5647445e08b47e34eb7b6f5edf2f68c822f26248b9e1e8ff344dee95b2c` |
| v22mod | `8538952a74dbea1968f7b93e6f5d3dcc531dd57ce35b2a322e62ef31ca8b899f` |
| v34filters | `ffe7bbe3bef769c78cea53c6e3dc81809a5e4cc989de1a021be994a19c49f8a9` |

Only these three of 300 production objects change. Candidate normal/dump
compilations agree; full-TU/data/export checks remain separate from byte grades.
Frozen commands, sources, headers, compiler/image identities, parsed artifact
hashes and individual reports are under
`/mnt/zfs/dev/slmodem/scratch/graph-leads-20261105/`; parent gates and closure
reports are in the active worktree's `build/graph-leads/`. The V22 return probes
are in `v22-data-review/`. Shared-CWD compiler dumps were excluded from accepted
stage evidence and retained separately. This wave does not change flags,
equivalence grading, tolerances or fixture assertions.
