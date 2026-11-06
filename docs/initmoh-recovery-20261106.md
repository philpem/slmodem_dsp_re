# InitMOH source recovery, 6 November 2026

`VPcmV34InitMOH` advances from SIZE32 (412 bytes) to BYTES55 at the native
444-byte extent. This is a partial source-graph recovery: there is no new
EXACT or REGALLOC function. The census is 1,147 EXACT / 29 REGALLOC / 72 BYTES /
604 SIZE over 1,852 functions, with no protected loss. The
[evidence ledger](evidence/initmoh-recovery-20261106.json) retains the source
crosses, compiler stages, fixture audit and full-TU checks.

## Source properties and escalation

Native retains `obj+4` from `0x6cf2` across handshake initialization and publishes
progress 7 through it at `0x6d89`. The message==1 arm clears the flag register
at `0x6e3b` and joins the ordinary arm at the common path-selector store,
`0x6d27`. Message publications remain separate. The selected source restores
that common flag continuation and retains the complete-object interior owner
using the existing member-derived access helper.

| Control | Result |
| --- | --- |
| Baseline | SIZE32, 412 bytes |
| A: common flag publication | SIZE32, 412 bytes |
| B: retained interior progress owner | SIZE6, 450 bytes |
| A+B | BYTES228, 444 bytes |
| A+B plus widened consumed-byte value | BYTES56, 444 bytes |
| Above plus byte-width predicate consumption | BYTES55, 444 bytes |

The High investigation froze the A/B matrix and located the late-byte split at
reload. Extra High then crossed a supported value/conversion boundary through
the A/B graph, retaining losing and no-gain cells. Its 15 ordinary compilations
produce 12 distinct objects, with no target EXACT/REGALLOC hit. These are
successive investigations, not a blind model-efficiency comparison. Requested
model/effort and unavailable usage measurements are recorded explicitly.

The native incoming byte is saved across the diagnostic path. At `0x6d32` it
is zero-extended once, then its low byte feeds both TESTB at `0x6d3d` and the
publication at `0x6d48`. The earlier candidate instead compares its spilled
byte in memory and reloads it separately for publication. The selected local
`unsigned int late_value = late` carries one widened value to both consumers;
the predicate explicitly consumes its low byte. Widening alone emits TESTL;
adding byte consumption changes exactly that opcode byte and recovers TESTB.

The formal remains unsigned char. Zero/nonzero behavior is unchanged for all
256 formal values; upper ABI argument bits are not consumed. This recovers a
conversion/use graph without uniquely identifying the original local type.
Caller publications, the writing handshake initializer and tone reset remain
in their original boundaries. The derived progress address is stable across
those calls; it is not a cached mutable pointer field.

The ending reset duplicates the existing `resetRetrainDetector` algorithm.
Sharing that helper is whole-object neutral in the tested baseline, A+B and
widened-value combinations. It consolidates supported code without claiming a
byte gain or proving unique original call syntax.

## Residual compiler boundary

The selected local allocator already assigns late-value pseudo 74 to EAX in
`24.lreg`. Read-only observation of the actual period compiler confirms SI
mode, Q_REGS, all four Q registers available, no suggestions and no crossed
calls; `find_free_reg` chooses register 0. Native reloads through ECX. Ordinary,
dump and observed assemblies/objects agree literally, so the observer does
not create this allocation.

`peephole2` subsequently introduces constant/zero-store scratch registers;
register renaming changes lanes and `sched2` schedules independent definitions
and stores. Native and source differ in some receiver/retrain zero-store
orders. Their memory owners, widths, values and opaque-call boundaries agree;
native scheduling order alone does not establish original source order.
No store-order grid, scratch consumer, flag or barrier was added.

Reopen the residual with a new native-supported lifetime, copy, owner or
conversion property that changes local allocation constraints while preserving
the byte formal and predicate. Reopen store groups only with evidence that
distinguishes source grouping from legal scheduling. The current boundary is
a bounded result, not a global-minimum claim.

## Full object and graph checks

The selected TU object SHA256 is
`d65158766842b9ae3b999e9ff8db038a962e51ad0e03247e0fa7d52c34a81425`.
All 56 emitted functions have native counterparts; only InitMOH changes, with
55 neighboring bodies literal-identical. All 27 EXACT and one REGALLOC TU
neighbors remain protected. All 300 reconstructed objects were checked: 299
are byte-identical and only `VPcmV34Main.cpp` changes.

Exports, bindings, function order and allocated section layouts remain stable.
All 89 rodata code targets retain their ordered owner-relative identity and
instruction boundaries. 75 raw addends move by 32 bytes with later code. The
other allocated payloads remain raw-identical. There are 439 text plus 89 rodata
relocations, 528 total, separate from the 300-object corpus denominator.

The owner graph target compares five anchors and six relations cleanly before
and after recovery. The stack graph changes from refusal to three matched
anchors with zero compared relations. Overall owner coverage remains 49/56
admitted; stack coverage becomes 13/56 from 12/56. These diagnostic subsets do
not establish full equivalence.

## Fixture audit and validation

The new fixture reuses the registered-create graph and exported reinitialization
for its three histories. Messages 0/1/2, independent late 0/1 and flag 0/1, and
debug levels 0/1/2 give 108 initialized-component scenarios and 432 calls, including
repeated requests. It compares the 126-region graph after the second call and
checks state/mode/progress and a native-generated MOH word after each call.
The word depends on late, independently of flag. The scenario block has 2,918 checks;
43 period layout assertions independently verify relevant addresses and widths.
No negotiated MOH or first-call whole-graph reachability claim follows.

The first draft expected twelve alias-word installations. The old-source run
failed 108 of 2,918 checks, one alias assertion per scenario. Native mode 4 installs
ten words; it leaves timing pointers +0x620/+0x624 unchanged. The corrected
fixture checks ten alias pairs and compares those eight bytes, plus the other
excluded bytes, directly. Existing all-twelve and HangUp four-word checks stay
intact. The failed apparatus run and corrected old-green control are retained;
no reconstruction source or tolerance was changed to accommodate it. AGENTS
now requires method-specific installed/untouched mask denominators.
Four additional snapshot-only calibrations show the raw check detecting a
changed untouched byte, ignoring an installed alias, and returning clean after
restoration; live DSP state is never altered by those controls.

Old and selected sources pass the expanded suites: `t_v34pcmapi` 9 groups/30,536
checks, `t_v34conn` 4 groups/186, and `t_vpcmcreate` 7 groups/8,448 in the old-source control and 8,452
after adding the four comparator calibrations. The previous
77 synthetic InitMOH scenarios remain. The new cases establish initialized
component behavior, not negotiated protocol or audio reachability. The full
`make phase J=8` passes 399 period tests with 0 failures and all structural
checks. Staged citation checks are rerun. Mutation infrastructure is not used.

## Next transfer

`VPcmV34InitiateRetrain` has a fresh retained-owner witness: native `0x6667`
acquires `obj+4`, uses it across the rate/diagnostic/switch work and handshake
call at `0x68d8`, then publishes progress 7 through it at `0x68dd`. Current
source uses direct members and an unused complete-object byte pointer. Audit
the complete rate/flag/DC/progress owner group, preserving every resolved
offset and width, and cross it with its duplicate reset-helper graph. Helper
consolidation alone is not a predicted win. No such experiment was run here.

> 🧠 **From Hindsight memory (V34 residual source and lifetime recovery)** — the
> older INFO0 alias/declaration investigation did not establish an original
> declaration. Its parked spelling palette is separate from this fresh retained
> owner-group hypothesis.
