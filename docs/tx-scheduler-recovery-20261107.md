# V34 predecessor, narrow-value and TX recovery, 7 November 2026

This wave adds **VPcmV34SetV90RateReneg EXACT274** and
**txmitdibit REGALLOC250**. The final census is **1,152 EXACT / 34 REGALLOC /
74 BYTES / 592 SIZE**, over 1,852 functions. No previous EXACT/REGALLOC function
is lost. All 300 source objects are verified; two change. The deciding
`make phase J=8` passes **399 period differential tests, zero failures**, plus
structural checks. The [evidence ledger](evidence/tx-scheduler-recovery-20261107.json)
records the frozen inputs, controls, compiler traces, complete-TU effects and
gates. Baseline is commit `679752cf12c2a6b926e635e35d58bc226fc779cf`.

| Function | Baseline | Selected | Outcome |
| --- | --- | --- | --- |
| VPcmV34SetV90RateReneg | SIZE6, 268 bytes | EXACT274 | New EXACT |
| txmitdibit | SIZE5, 255 bytes | REGALLOC250 | New REGALLOC |
| settxlevel | SIZE2, 464 bytes | BYTES43, 466 bytes | Partial native graph recovery |
| txinit | BYTES8, 245 bytes | Unchanged | Causal investigation; no source edit |
| v90Phase34 | SIZE98, 1,260 bytes | SIZE99, 1,259 bytes | Supported owner recovery; one-byte distance loss |

SIZE is a length gap; BYTES counts differing bytes at equal length. Partial
results are not additional promotions. Both strict complete partial-link
comparisons remain DIFFERENT. No profile, ABI, classifier or tolerance changes;
no deprecated mutation/anchor/snapshot work or modern portability gate.

## SetV90: all three properties are needed

The native target retains engine root+4 and TX root+0x221c, has two constellation
arm stores, and carries a zero across preinitdigital. The starting source merges
the stores and materializes the zero after the call. The preceding v90Phase34
independently retains the engine owner at native 0x9c10, alongside RX at 0x9bf3
and TX at 0x9c03. Its source used the flat engine compatibility view.

The selected source changes eight Phase34 receiver-state uses to the actual
engine member, still root+0x24c, width four. SetV90 publishes progress through
engine+0, still root+4, width four. Its unchanged unsigned-byte constellation
formal selects two arm stores through a default-first switch, preserving native
nonzero fallthrough. Original type name and unique authored switch spelling
remain unresolved. All calls retain the complete root; the receiver-state read
after txmit remains after that call.

| A: preceding engine owner | B: actual progress member | C: native-oriented arms | SetV90 |
| --- | --- | --- | --- |
| No | No | No | SIZE6 |
| No | No | Yes | SIZE6 |
| No | Yes | No | SIZE6 |
| No | Yes | Yes | SIZE6 |
| Yes | No | No | SIZE6 |
| Yes | No | Yes | BYTES19 |
| Yes | Yes | No | SIZE6 |
| Yes | Yes | Yes | **EXACT274** |

All 56 native/current function positions have the same emission order. The
successful control changes a real preceding algorithm's owner, without adding
a scratch carrier or moving functions. Phase34's own distance grows one byte;
the coupled result is retained after instruction and differential review.

## Observe the compiler mechanism beyond initial RTL

Initial RTL was an incomplete discriminator. A diagnostic switch keeps both
stores through peephole2, then ce3 merges them because they use the same scratch
register. Thus a missing native arm store can result from a late pass even when
an alternative source expansion already contains the two effects.

For A0+B+C versus A1+B+C, the target instruction UID/pattern streams agree before
peephole2. Read-only captures of the actual compiler each record **360 searches
across 32 functions**, including ten target searches. Both captures reproduce
ordinary objects, repeated outputs and assembly literally. The final Phase34
search leaves incoming SetV90 cursor 49 versus 50. With the same target UIDs,
eligibility and live sets, arm stores 123/116 choose AX/AX versus AX/DX. ce3
can merge only the first pair. Vector-clear 98 chooses CX versus BX with the
same live set SI/DI/SP; BX can survive preinitdigital, allowing sched2 to hoist
the zero before that call. Later renaming gives the native final arm lanes.

This directly observes the predecessor interaction in the tested compiler.
It does not reconstruct the original compiler's unseen cursor or uniquely
identify original declarations. B's actual member metadata supplies the remaining
publication schedule. The final six blocks, seven edges, 60 instructions and
four canonical relocation fields are exact; no residual lane argument is needed.
Ordinary and dump objects reproduce literally.

## txmitdibit: an actual member pointer and narrow result

Native 0x5e620 addresses the actual scrambler at root+0x25cc while retaining the
TX owner at root+0x221c. The old source copied the scrambler into an automatic
local and copied it back. The selected source passes the declared member's
address and consumes the modulo-four quadrant as a short.

All changed accesses retain checked full addresses: flags root+0x25c2, previous
quadrant +0x25c6, current quadrant +0x25c8, scrambler +0x25cc and point +0x25d0,
widths 2/2/2/4/4. The corresponding signed/unsigned int access is permitted;
no new owner extent or type is introduced. Masking proves quadrant 0..3 before
short conversion and indexing; this does not establish a unique original local.

The full eight-cell owner/member-pointer/short-result cross gives baseline
SIZE5; short only SIZE6; pointer only SIZE27; pointer+short SIZE26; owner only
SIZE6; owner+short SIZE7; owner+pointer SIZE8; all three REGALLOC250. Native and
selected each have 83 decoded instructions. The only differing byte is offset
0xaf: native POP EDX versus selected POP EAX discards the four-byte frame.
Neither lane is consumed afterwards; the remaining restoration and sibling
jump to txmit agree, including argument and relocation targets.

## settxlevel and txinit: different remaining causes

Direct consumption of the declared reduction short removes settxlevel's promoted
local, restoring the narrow predicate and separate negative-continuation widening.
Native and selected now have 124 instructions, 23 blocks, 34 edges and nine
relocation fields, with matching complete branch heads/targets. The selected
466-byte body remains BYTES43. Valid complete modem/MP owners, call boundaries,
the second MP read after publication, short up-loop truncation and 32-bit down-loop
arithmetic remain part of the reviewed domain.

The remaining initial MP load uses AX through greg/peephole2, then rnreg changes
it to CX. sched2 can move that load before consumption of GetMinPR's AX result;
native's AX MP load instead depends on that consumption. An explicit MP capture
changes another outgoing-area dependency but does not recover this lane. Late
owner formation, typed formals, direct formal use, actual engine access and
dead-view removal do not close the graph. Twenty-five valid builds yield eight
distinct objects; the minimal direct-field source is retained on its native
value/flag evidence, separately from the two promotions.

txinit already has native scrambler/previous/count clear order through bbro.
Read-only compiler observation records 21 rename decisions and reproduces the
assembly/object literally. An absent-target control reports zero, rejects the
empty observation and preserves emitted output. The three zero chains receive
DX, AX and CX using local use-age ticks; their hard-register dependencies make
sched2 choose count, scrambler, address LEA, previous. Both functions reach the
first echo cleanup with the same fields zero and identical arguments. This
straight-line correspondence does not upgrade BYTES8 or prove original syntax.
There is no supported txinit source change or predecessor-cursor explanation.

## Collateral and complete object closure

The two changed TUs contain **109 emitted functions / 85 native-shared copies**,
including **45 protected EXACT/REGALLOC copies**. Nine bodies change:

| Body | Measured change and review |
| --- | --- |
| SetV90 | EXACT274 |
| txmitdibit | REGALLOC250 |
| settxlevel | BYTES43; narrow graph recovered, initial scheduling remains |
| v90Phase34 | SIZE98->99; native engine owner, independent temporary/TX store schedules and exit/padding reviewed; larger switch/flow remains open |
| IndicateDilReceived | SIZE49->42; unchanged source now retains native-style separate short arm stores/epilogues |
| K56FlexRateDetermined | SIZE1 unchanged; independent ECX/EDX constant transport after debug |
| k56FlexPhase34 | Same body except resolved local-call displacement to getMPrecvdBits |
| v34handshak | Same body except three resolved call displacements after settxlevel alignment growth |
| local probe_preemp_shape | Same 1,273-byte extent; greg lane change and independent LEA scheduling preserve low-byte flag consumers, calls and effects |

No other object changes. Parent independently recompiles the HS baseline,
direct-field only, dibit only and both, reproducing each isolated object and the
combined production object literally. Main likewise matches its frozen selected
object. All source/header/compiler hashes and full inventories are in the ledger.

Exports, bindings, visibility and function inventories remain. Main's 439 function
relocation fields and 226 ordered call fields retain targets. All allocated
nontext metadata and masked payloads agree. Of 89 data targets, 88 keep exact
function-relative offsets; Phase34 case4 moves +481->+480 but still enters its
vect4[2] arm under the same subtract-three/CMP7 selector. HS nontext payloads
remain raw-equal except resolved moved code targets; all 54 local-call identities
and 314 data targets retain destinations. Target validity alone is not the
admission argument: changed continuations and field effects were also reviewed.

## Fixtures and graph-screen limits

The expanded `t_vpcmcreate` has **15,102 checks in 13 groups**, adding **1,921**:

- TX reset/power: 108 cases, 216 calls, 651 checks on factory/native-created
  owners, optionally initialized by native modeminit. All 90 power cases change
  output; the 18 reset cases are idempotence coverage, not sensitivity evidence
  for nonzero resets. Existing planted-state reset probes remain synthetic.
- TX points: 64 bounded prefixes, 576 calls, 1,027 checks after native Create and
  modeminit, both roles and debug 0/2. All prefixes change output; queue capacity
  is checked after every call. No internal state is planted.
- SetV90: 24 cases, 48 calls, 243 checks on constructed owners. All change output.
  The four installed shell aliases are checked by referent; every other learned
  exclusion remains bytewise visible. This masks only those 16 address bytes.

The first two groups pass unchanged source before candidate edits. SetV90 passes
with unchanged Main first, then selected Main in the complete phase. Component
calls do not establish that these inputs/prefixes occur during negotiated PCM.
Each observes all 126 graph regions / 265,992 bytes, plus transcripts, callback
boundaries and capture completeness. The first TX fixture run had six failed
expectations about reset diagnostics; native echo cleanup emits them at level
two. The expectation was corrected, preserving the failed apparatus log; all
source/native graph and transcript comparisons already agreed.

The baseline full-corpus screen covers 701 unique inexact symbols / 704 copies.
Owner admits 593/refuses 111, comparing 2,727 relations with 567 ambiguities and
416 unpaired anchors. Stack admits 106/refuses 598, comparing 2,964 relations
with 100 ambiguities and 39 unpaired anchors. Four owner nominations span three
functions. BackwardClean's old capture replay and V8's infeasible NULL join stay
within their existing qualified negative records; the SpectralShaper nomination
pairs different loop guards across encoder calls and is not a demonstrated
observation-age defect. No source change follows from those nominations alone.

> 🧠 **From Hindsight memory (Null datapump signed-count and V8 message graph recovery)** — the V8 helper-null nomination was already reviewed as a value-infeasible continuation.

The current selector code and Playbook record were checked before retaining that
qualified conclusion; the recurring static nomination does not reopen it alone.

The final two-TU screen selects 44 inexact copies. Owner admits 33/refuses 11,
comparing 178 relations with 40 ambiguities and 23 unpaired anchors. Stack admits
14/refuses 30, comparing six relations with 20 ambiguities and 23 unpaired
anchors. Zero nominations describes those supported subsets only. Parent uses
the committed worktree helper closure. Separate child graph/stage comparisons
initially imported captured dirty-main helpers; their distinct hashes are
retained and are not mixed with the parent denominators. The compiler captures
have independently checked helper parity and literal output controls.

## Bounded negatives and next work

Quadbit owner/scalar/capture controls remain nonexact; native keeps full flags
and different frame/pointer transport. A shared V34scrambler capture loses the
already EXACT public helper and changes the zero-iteration store graph, so that
specific diagnostic is rejected. EchoFilter pointer/capture/count controls also
remain nonexact despite a small byte gap; its owner/lag spill and guard graph
still differ. Neither negative closes every legitimate helper or type hypothesis.

The suggested bitreverse Boolean-production lead was checked against F11639 and
the [historical report](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-bitreverse-init-controls.md)
before final recommendations. That same statement
predicate family already reached native size but stayed BYTES40 with TEST/JE/MOV
instead of TEST/SETNE. It stays closed without a new independent constraint.

Next investigate quadbit's two-scrambler-call lifetime and Phase34's remaining
TX-owner/flow structure. Reopen settxlevel/txinit with a distinguishing helper or liveness
witness, not another root/store-order palette. Preserve the new exact SetV90 as
a measured coupled constraint while investigating subsequent native-supported
source changes; a temporary loss still requires analysis, not automatic veto.

SetV90 and narrow/scheduler investigations used Sol Extra High; owner-consumer
transfer used Sol High. Usage/billing are unavailable and this is not a blind
model comparison. Scratch controls, compiler observations and reports live under
`/mnt/zfs/dev/slmodem/scratch/tx-scheduler-wave-20261107`; the ledger pins their
actual bytes. No whole-object equivalence or global-optimum claim is made.
