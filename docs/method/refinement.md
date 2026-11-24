> **Historical mutation evidence:** this log preserves results from the removed
> mutation system. Mentions of suites, anchors or mutation runs describe past
> measurements only and do not prescribe current maintenance.

# Refinement: what actually closes the last bytes

For experiment planning, source/flag interactions and stopping/reframing a
stalled search, first read [experiment-design.md](experiment-design.md).
This page supplies the individual levers; that workflow prevents a locally
successful lever from becoming an untested global assumption. A unique
preimage below is unique within its declared domain, not a claim that all
possible source/flag combinations have been excluded. Unique authorship is not
an acceptance gate: several natural source spellings can satisfy the same
complete native constraints. Prefer a clear supported spelling, report the
ambiguity, and retain the full differential and code-generation obligations.

Before reusing a closure, identify the observation it actually establishes.
Equal projected instruction counts do not establish dataflow equivalence;
an exact out-of-line body does not fix its frontend inline cost; a refused
table proof does not establish a source defect; and the worst emitted copy
is distinct from the body selected in a particular partial link. See the
[proof-boundary review](../apparatus-unblock-review.md) for measured controls
and the remaining proof obligations. Keep diagnostic and production flags
separate when locating the compiler pass responsible for a difference.

## Screen beyond the historical candidate filter

Inventory every residual function and every defining copy before applying a
size or source-family filter. A filter used for one experiment is not the
remaining-work boundary. Likewise, a detector that nominates physical
backedges says nothing about functions outside that detector's domain.
Report both denominators and inspect helper and tail-call closures before
calling an operation missing.

Shared calls can expose a source-lifetime discrepancy. In `_init_receiver`,
the original's three modulation-specific request buffers survive to one
common `FAXVMI_control` call. Branch-local request objects and three copies
of the control path obscure that structure. Keep the typed requests alive
in the enclosing scope, initialise the selected request and control template
in the witnessed order, then join the common rate and control path. Dispatch
after the call using the saved modulation value, as the original does;
rereading the configuration would introduce a different value source.

This is a supported partial recovery, not a byte-identity claim. Check the
complete translation unit, the exact-name set and period component histories
before retaining it. Neither a smaller size gap nor a green differential
test alone establishes the original source structure.

## The translation unit is the recovery boundary

**Register allocation and code generation are coupled across the complete TU.**
Earlier functions can change later register choices, scheduling and instruction
layout even when the later source is untouched. Period GCC's persistent
peephole2 scratch-search cursor is one measured mechanism. Individual passes
can work per function while sharing state across emitted functions; do not
interpret function-local analysis as allocator independence.

This is established project knowledge. Getting a function to EXACT can recover
later REGALLOC/BYTES functions, or temporarily regress an EXACT neighbour.
Rebuild the complete TU in original function order, score all emitted bodies,
preserve gains and losses, and cross independently supported source changes.
Trace and repair collateral before selecting a candidate. An EXACT neighbour
is not frozen, and a temporary regression alone cannot disqualify a recovery.
The [SetV90 wave](../tx-scheduler-recovery-20261107.md) and
[Trn2d/K56 follow-up](../trn-k56-recovery-20261107.md) are concrete examples.

## Consult, investigate, record, transfer

Consult this Playbook at the start of every recovery turn. Read the applicable
mechanisms, counterexamples and recent additions before choosing experiments.
Compare historical results with the current source, headers and compiler inputs.

Shared declarations, interfaces and neighboring source can compensate for one
another. An EXACT neighbor is not frozen. Explore evidence-backed intermediate
states that make bytes worse, then test the coupled correction. When collateral
is a small set of functions, trace their native boundaries and first compiler
divergences before rejecting or parking the proposal. Widen the investigation
to affected callees, callers and preceding functions where the evidence leads.
Preserve the baseline and report all gains/losses; the completed candidate still
requires period fidelity. A finite search does not establish a global optimum.

Before parking a function, complete a bounded deep dive: identify its native
observation boundaries, trace the first relevant compiler transformation, and
run a discriminating control. Record the outcome here, including negative
results, tested combinations, full-TU collateral, fixture domain, unresolved
questions and the evidence that would reopen it. A smaller byte gap or an
unexplained compiler-stage label is not a completed investigation. Deep dives
have declared limits; this rule does not require endless source permutations.

**Escalate to Astra when Sol High and Extra High have produced no EXACT/REGALLOC
recovery before parking the function** (owner instruction, 2026-11-08). Partial
SIZE/BYTES progress alone does not satisfy this escalation condition. Preserve
the frozen evidence and tested negative domains, give Astra room for a fresh
causal/source hypothesis, and validate its output against the same period and
complete-TU obligations. Record what the escalation added, even if no recovery
results. Model identity does not establish correctness or efficiency.

On the next recovery turn, review the additions from the preceding deep dive
and screen other functions for the same mechanism. Require a fresh native
witness before transferring a source change. Record where the lesson applies,
where it fails, and the next discriminating test. Transfer a negative lesson
as a constraint too; do not repeat the parked function's closed palette.

## Mutation maintenance is retired (2026-11-05)

The owner reiterated that mutation tests are deprecated. Remove them from
routine gates and CI; do not reanchor suites or refresh snapshots unless the
owner explicitly requests a named legacy investigation. Earlier maintenance
accounts below are historical outcomes, not current work requirements. The
[retirement policy](mutation-policy.md) supersedes the older retained-anchor
rule. Period differential, valid-history regression tests and apparatus
negative controls remain the reconstruction authority.

## V22, V92CP and HDX deep-dive outcomes (2026-11-05)

Local commit `4246de2a4`
retains V22 control REGALLOC at 145 bytes and V92 `float2Bits` EXACT at 125
bytes. The coupled HDX/Demod follow-up adds `DemodDataV22` EXACT at 510 bytes
and restores two HDX byte scores. It passes 399 period tests plus structural
checks; the final measured census is 1,134 EXACT / 28 REGALLOC / 73 BYTES /
617 SIZE out of 1,852, with no EXACT/REGALLOC losses. These are dated
measurements, not permanent counts. The complete
source and fixture qualifications are in the [V22 report](../v22-control-recovery-20261105.md)
and [V92 report](../v92cp-inline-recovery-20261105.md).

| Lesson | Measured outcome | Transfer condition or limit |
| --- | --- | --- |
| Cross request sampling with the owner alias boundary. | V22 needs the high request before the scrambler store, a conditional reread after RETRAIN, and the late DSP-owner acquisition. The fixed-context alias-view × recapture matrix gives BYTES 54 / 50 / 72 / REGALLOC. | Track owner, offset, width and value age separately. Isolated captures did not establish a negative result for the combined graph. |
| Preserve consumed coefficient and predicate ownership together. | V92 switch dispatch, one weight per iteration, negated predicate and counter/cursor lifetime recover `float2Bits`; other 14 TU bodies remain unchanged. | Preserve unordered floating-point behavior. The three `bitsToInfo` byte/quantum cells produced no grade gain; do not repeat them without new scalar-home/lifetime evidence. |
| A matching reload does not identify its alias cause. | An alternative pointer-union view plus explicit native captures reproduces the retained control body and status EXACT with ordinary HDX shorts. | It does not establish original HDX bitfields or an original pointer union. Trace both the publication and the owner acquisition. |
| Distinguish a pointer union from a plain opaque pointer. | Plain `void *` plus typed casts loses control's late owner read; the union is the active alias mechanism. | Do not infer an original generic-pointer declaration from a union diagnostic. An unused member that changes alias sets needs independent declaration/owner evidence before retention. |
| Compare the entire header closure, then investigate small collateral sets. | Header alone restores ans_rmloop2 BYTES 18→7 and org_rmloop2 101→90 but gives DemodDataV22 SIZE16. Its coupled interface/lifetime correction makes Demod EXACT510. | Do not reject the isolated regression before tracing and crossing the affected source; all 300 production objects were compared. |
| Distinguish interface transport from local interpretation. | Native Demod passes zero-extended counts; SRE/FSE consume signed low words. Unsigned-short formals plus signed locals retain both callee objects literally. | A callee sign extension does not uniquely establish its formal parameter type. Check all callers and full callee use; FSE must publish n_in from the existing signed local. |
| Cross count declarations with RMS-result lifetime. | Counts alone give BYTES29; RMS short capture alone BYTES202; both EXACT510. An int RMS local does not recover it. | Keep signedness, conversion width and value lifetime separate, then test their interaction. |
| Place owner recaptures on the native CFG edge. | With the alternative, unconditional post-RMS recapture gives BYTES25; recapture only on the surviving RMS path restores EXACT. | Protocol!=0 bypasses the native reload. Blanket freshening can hide a wrong source graph. |
| Recover status value ages explicitly when testing a broader owner alias. | Native status reads protocol, captures DSP, then retains that owner through the initial report stores and first scrambler read. Capturing protocol before DSP restores status EXACT in both diagnostic owner variants. | Original source spelling remains non-unique. A native capture in one function does not justify a global alias declaration. |

The [HDX report](../v22-hdx-deep-dive-20261105.md) records nine full-stc
cells and seven paired consumer builds. The [Demod report](../demodv22-recovery-20261105.md)
adds fifteen full-V22int cells and six callee controls. Only the selected
coherent source has the 399-test period validation; that verdict does not cover
every diagnostic cell. The pointer-union view is an explicit alias-contract
model, not a uniquely recovered original declaration. It replaces the earlier
HDX state view after the coupled repair and full closure review.
[Issue #590](https://github.com/philpem/slmodem_dsp_re/issues/590) tracks the
remaining original-declaration and consumer questions.

For the next recovery turn, transfer the new interface/value-age lesson to
`Detect_v22`: the alternative exposed owner-related changes there. Map native
DSP acquisitions around calls and publications before proposing captures.
Inspect `V22FP_create` if its changed group supplies an independent discriminator.
Record successful and failed transfers. These are admission checks, not promised
gains. Keep control's remaining register choices separate from structural work.


## V22 transfer: scalar bounds, owner groups and sharing (2026-11-05)

The [V22 transfer wave](../v22-transfer-wave-20261105.md) recovers Detect_v22
EXACT375 and v22_org_rmloop2 EXACT1050. Constructor improves SIZE40→7 but
remains non-exact. Final census: 1,136 EXACT / 28 REGALLOC / 71 BYTES /
617 SIZE over 1,852. Preserve the report's non-exact regressions and fixture
limits; the count alone does not justify the source.

| Transfer lesson | Positive evidence | Boundary or counterexample |
| --- | --- | --- |
| Cross scalar lifetime with branch orientation. | Detector branch alone gives BYTES4, bound alone SIZE8, both EXACT. A consumed short bound propagates after CSE and retains native CMPW4/LT. | This establishes a source family, not the author's unique local declaration. Return-width suspicion was false; the baseline already matched it. |
| Capture the owner before the native publication. | Demod captures HDX before rx_count; answer captures before gap and retains the owner across its tone-state publication. | Matching static owner-load counts does not establish the right paths or ages. |
| Analyze newly exposed collateral. | Broader HDX view alone worsens constructor; native groups recover its first destination acquisition. Handler repairs then yield another EXACT function. | The pointer unions model an observed alias contract; original declarations remain unresolved. |
| Distinguish access evidence from declaration evidence. | Word/byte status view produces native DWORD-clear/ORB/byte-store. | Memset and extra bitfield produce identical objects; no extra bitfield evidence exists. Same SIZE is not raw equality. |
| Trace sharing into branch-length feedback. | Peephole2 choices feed ce3 crossjumping; a three-byte shift changes a near JE into short JE, adding four bytes to the extent gap. | Do not tune that branch or assume a shared machine tail was a shared source tail. Fixed-input cursor replay is not source recovery. |

All 300 comparison objects were rebuilt for closure. Eleven of 58 table target
offsets move with unchanged owners; their source-case entry mapping is reviewed,
not described as unchanged raw data or complete CFG proof. Source/RTL narrative
claims must be checked against actual object bytes and parsed input hashes.
Extra High independent review added validation, not a measured efficiency win.

The originate predicate/frame follow-up is recorded below. Reopen constructor
sharing with a new operation/lifetime/eligibility witness, not another spelling
or incoming-cursor palette. Read these results and their negative controls first.

## Native graph timing and the V22 originate follow-up (2026-11-05)

The [originate deep dive](../v22-originate-recovery-20261105.md) retains the
native early gap clear, absent-arm branch orientation and direct NODE3 output
word consumption. The size distance improves SIZE160→150, with no new exact
function.
C × D and E × CD show why neither a flat isolated grade nor a smaller quotient
variant settles the source graph. The candidate passes 399 period tests plus
structural checks; only originate changes among ten TU bodies and only its
object among 300. Code-table offset changes remain explicitly qualified.

Native SETE-to-memory followed immediately by MOVZBL does not establish a
source char. Its separate DIV numerator/quotient homes also prevent attributing
the larger frame solely to the predicate. Gap width/consumer controls and two
quotient families are now closed; reopen only with a new source/ABI/def-use
witness. A closer quotient candidate was rejected for the wrong producer and
comparison graph.

Run [native/reconstruction graph screening](native-graph-screen.md) routinely
on changed translation units and periodically over the inexact corpus. Compare
owner, access width, predicates, value ages and branch/call relationships before
assuming an allocation-only residual. Preserve input hashes, positive and
negative controls, compared-relation denominators, ambiguous correspondence and
unsupported effects. The screen nominates investigations; a clean bounded
subset is not an equivalence verdict. Audit each nomination against native
instructions, source and valid fixtures before changing reconstruction code.
Keep period differential and full-TU/data/closure checks as the deciding gates.

The initial 716-symbol/719-copy screen nominates 25 timing/predicate differences
across 13 copies. Owner relations are compared in 275 copies and local-stack
relations in 23; these are overlapping subsets. Do not describe 605 admitted
owner graphs as 605 fully compared functions. The V22 gap positive disappears
with the retained repair. The next section records the completed v22_data,
FPM PPS and V34 echo entry-read follow-up. Review scheduling and owner identity
before interpreting a nomination as a defect. The separate
`null_process` signed-word predicate is a concrete native discrepancy whose
high-bit count domain still needs a fixture audit.

## Graph-lead transfer: induction, capture and observation age (2026-11-05)

The [graph-lead wave](../graph-lead-recovery-20261105.md) recovers
V34EchoAdapt EXACT79. Early owner captures alone give SIZE7; an ascending
pointer loop gives BYTES8; native fraction-before-coefficient capture order
closes the body. `09.loop` explicitly reverses the ascending source loop and
introduces the native post-guard count copy. A manually written countdown
misses that copy. Machine DEC/JNE does not establish original source induction.
Two later nonexact bodies change first at peephole2; record that collateral
rather than treating their source as changed or their grades as frozen.

FPM's five nominated captures alone remain SIZE12. Broader native root captures
first give SIZE76. Crossing full-width rail counters and the dependency on the
consumed history cursor reduces that to SIZE15 and restores the native selector
observation. Retain this supported operation graph despite the three-byte
baseline distance regression. The root-owner comparison grows from 14 anchors /
13 relations to 19 / 18; zero nominations is qualified by unsupported indexed
and loop effects. Separate PRE-created guarded captures from explicit entry
captures and sched2 load ordering before proposing another source spelling.

V22 status initialization after V22_status removes its timing nomination at
unchanged SIZE10. Diagnostic captures get as close as SIZE3 but move a limit
read ahead of native publication and reverse the native predicate. The
absent-fallthrough interaction still fails those ages. A coherent short-return
interface leaves the caller literally unchanged and changes callee EAX values
for three synthetic high-bit cases. Keep low-word consumption, full-EAX
transport and a uniquely recovered interface declaration separate.

The BackwardClean early-cursor replay removes its old nomination but supplies
no new source property beyond the closed F11704 family; it is not selected.
On the next turn, transfer ascending-loop reversal and exact capture-order
lessons to fresh native-backed leads. The following section records the VPCM rate-change
graph follow-up; do not reopen the parked FPM
comparison/load-order or V22 quotient/return-type palettes without new evidence.

## VPCM and cadence: owner views and shared rejoins (2026-11-05)

The [rate/receiver/cadence wave](../rate-receiver-cadence-20261105.md) recovers
VPcmV34SetMinMaxBitRates EXACT348 by crossing native interior-owner access,
early session/K56 capture and scalar low/high capture. The complete eight-cell
matrix matters: only the combined cell closes it. A member-derived offset view
preserves named-field evidence, but every use still needs full-owner offset and
width checks; it does not establish a unique original engine declaration.
Initiate reaches native259-byte extent/BYTES39. Its inert absent-clamp control
and first-RTL/sched2 differences do not justify another predicate palette.

A read-age nomination can instead expose a missing control-flow rejoin.
Cadence's first-cycle failure arm must still reach the common matcher guard;
else-if incorrectly skips it. Three constructor/audio histories demonstrate
native first-cycle detection and parameter reads, with a three-cycle negative
control. Preserve that admission even when its isolated byte grade worsens.
The native finite empty loop is a local countdown, not an asynchronous poll;
ascending source induction recovers its loop-pass reversal.

setupreceiver's post-rxinit rates and detector-call descriptor reads recover
native observation age at SIZE43. Callee-write auditing establishes no tested
numerical defect there. Distinguish operation placement, callee effects,
missing paths and fixture reachability before calling a nomination a bug.
Changed-TU graph coverage and allocated-data targets remain separately bounded.

Mutation maintenance is retired under the policy above; no reanchoring is
required for these source changes. The following section records the completed
null_process and V8SetMessage follow-up. Keep the recorded negative domains
closed until a new native witness changes their source hypothesis.

## Signed mutable counts and value-infeasible joins (2026-11-05)

The [null/V8 wave](../null-v8-recovery-20261105.md) recovers null_process
EXACT59. Its common callback still transports an unsigned-short pointer;
native CMPW/JLE and CMPW/JG consume the loaded word as signed. Preserve the
interface and cast at each use, rather than snapshot the bound or retag one
callback declaration. Native reloads count after each destination store.

Cross that signed-consumption correction with post-guard destination acquisition.
Baseline is SIZE2, signed consumption alone BYTES14, late destination alone
SIZE2, and both EXACT. All five neighbouring bodies, exports and allocated data
remain unchanged. A no-gain isolated owner change was necessary to the combined
recovery; unsigned transport did not establish unsigned consumption.

The fixture adds ten constructed-link component cases / 190 assertions: five
with disjoint counts through 32767 and five explicitly synthetic count/dst[0]
alias probes. Positive aliases discriminate repeated reload from a snapshot;
high-bit aliases safely expose four old-source failures and pass after recovery.
Normal VMI destination storage is only 200 words. No high-bit protocol history
or ordinary FAXVMI alias is established, and the source-reconstruction phase
remains closed. These are boundary fidelity tests, not new fax reconstruction.

> 🧠 **From Hindsight memory (Residual compiler-stage family batch)** — the
> unsigned V8 switch-expression recipe and parked helper-null investigation were
> historical leads, independently replayed here against the current inputs.

V8SetMessage's six-cell selector-signedness × helper-boundary experiment retains
only the unsigned switch-expression change: SIZE20/412 bytes becomes
SIZE11/403 against native392. All six neighbouring bodies, exports, five
allocated-data sections and 34 data relocations remain unchanged. The native-
sized direct-rejection diagnostic still differs in 104 bytes, moves rc's
initialization and changes neighbouring scheduling; it is rejected. Both pointer
forms retain the helper-null join through GCSE and bypass. Reopening requires
independent helper/return-boundary evidence explaining the native rc boundary,
not another size match or the same static graph nomination.

The graph lead is a value-infeasible continuation. For an invalid selector,
current source sets seq=NULL, then necessarily rejects at the shared NULL test.
The raw graph explores the impossible nonnull continuation and reaches charFlip.
Both native and current feasible paths reject before encoding. Before calling a
nomination an operation-age defect, establish selector/iteration identity and
carry guard facts across the path. Preserve independently witnessed differences
(the native lacks that pointer-null test) without upgrading them to a behavior
bug. Zero supported nominations still does not prove equivalence.

The selected pair passes the period and structural gate, with no protected
grade losses. The following section records the completed silence-state audit
and small-loop transfer. Keep the bounded V8 helper palette closed without
new evidence.

## Entry snapshots, post-guard owners and cursor publication (2026-11-05)

The [silence/loop wave](../silence-loop-recovery-20261105.md) recovers
_recieve_silence_state EXACT274 and faxvmi_write_fifo EXACT157. Both expose
source observation errors at legal component aliases; the probes remain
explicitly synthetic and do not establish ordinary protocol histories.

A call/read nomination can expose a caller publication. Native silence handling
captures word8 before clearing TX, publishing tx_count and calling FPM_rms.
RMS and its sqrt callee write only private stack storage, but word8 and tx_count
can designate the same int. Entry snapshot alone gives BYTES2; a named native
loop bound alone gives SIZE1; both close the body. Sixteen real-initializer
component cases / 193 assertions expose four old-source failures and pass with
the correction. Disjoint and nonzero-entry controls remain green. Replace the
old source comment claiming strict aliasing justified the late read.

Investigate collateral through actual pass decisions. Twelve EXACT class1
neighbours remain EXACT. Two unchanged-source neighbours first differ at
peephole2; fax_class1_command subsequently loses shared initialization tails
at ce3 and moves BYTES49→SIZE11. Complete preceding normalized RTL is unchanged.
Do not add a source carrier to regain that allocation accident. Review command
case destinations and pooled string identities separately from raw section
bytes: six switch targets retain case identity despite three offset shifts;
37 string relocations retain their target strings.

The small-loop screen covers 300 objects / 1,886 shared defining copies:
1,302 native bodies at most 250 bytes, 244 inexact copies, 183 with backward
jumps, seven source/native inspections. Backward layout is not a loop proof.
Only two TUs received source matrices. Uninspected rows remain unreviewed.

For FIFO writing, A is short postdecrement, B post-guard framer acquisition,
C old-wr capture plus increment publication before the input read, D underrun
capture before source-pointer acquisition. Baseline SIZE36, A24, B20, AB14;
ABC is native-sized BYTES22; ABCD is EXACT157. Removing A/B/C/D gives BYTES43 /
SIZE4 / SIZE25 / BYTES22 respectively. The final D control changes initial RTL
capture homes and closes the remaining bytes. All three TU bodies are
compared, including the exact frame_reverse neighbour; both neighbours remain
unchanged.

Do not collapse cursor advancement into a later publication. Native increments
wr before loading the input sample. A one-element source can legally be &fr.wr
while the actual FIFO is a separate two-element array. Sixteen constructed-ring
cases / 225 assertions expose two old-source failures; zero requests, full
rings, disjoint sources and wrap cases provide controls. All fields, buffers,
guards and source advances are checked. Neither test aliases a FIFO buffer to
undersized scalar storage nor claims an ordinary constructor-generated alias.

FPM_iir_filt_II remains unchanged. Its short-outer-induction × inner-postdecrement
matrix gives SIZE19/15/20/16, with no complete recovery or demonstrated behavior
defect. These four spellings are closed; reopening requires a new native value
observation or compiler cause. TxNoCarrierV17/V27/V29 counts already agree with
native unsigned predicates: do not transfer signed consumption to them merely
because another function uses signed low words.

The following section records the completed framed-writer and 251..600-byte
follow-up. Keep the IIR and V8 negative domains closed until their reopening
condition holds.

## Recover helper boundaries before duplicating algorithms (2026-11-05)

The [framed-writer/medium-loop wave](../frame-loop-recovery-20261105.md)
recovers _send_hdlc_buffer_state EXACT282. Native copies the immutable FAXVMI_STS
template after FAXVMI_process and before FAXVMI_status. Moving the assignment
from entry restores that placement. No template-mutating fixture or behavior
bug is inferred from a source-placement correction. Seven neighbours, including
two REGALLOC bodies, stay unchanged.

faxvmi_write_frame's initial SIZE132 gap includes a missing inline boundary:
native contains the existing FIFO writer and two-nibble CRC algorithm inline.
Outer postdecrement alone gives SIZE116; post-header framer source acquisition
alone SIZE132; both SIZE106. Native-backed store publication and CRC controls
reach SIZE17; calling the existing exact FIFO helper with ignored return and
sharing a private CRC implementation reaches SIZE1/543 against native542.
The public CRC wrapper remains in its original TU, with one implementation in
a private header used by exactly two TUs. Its SIZE1/107 against native108
persists; frame_reverse, write_fifo and byte_reverse remain literally EXACT.
The original inline declaration/visibility is not uniquely recovered.

The large byte gap was not missing algorithmic source. Compare repeated native
walks with existing helpers before manually rebuilding them. The native frame
contains the helper's partial/full/countdown/underrun graph, including the
full-ring return that bypasses normal underrun publication. Reusing that helper
restores the per-store owner reloads without artificial captures. Validate its
public entry, all inlined consumers and the complete header closure together.

A one-byte size distance can hide several instruction/observation differences.
Here .07.addressof retains the second FCS FIFO-pointer load (UID290), but .08.gcse
reuses the first load (UID270); native reloads after the first FCS store. Current
pointer/ushort alias sets permit that reuse. A bounded Sol Extra High rescue
confirmed that direct-postfix controls have the same tail and supplied no new
source control or gain. Do not infer a particular original union, qualifier or
alias annotation from the reload. Reopen with independent original access/alias
visibility evidence or a natural source boundary changing GCSE availability;
close the CRC type/expression/postfix palettes on this evidence. No compiler
impossibility or allocator-only conclusion follows.

Eight union-backed frame component probes / 113 assertions expose two old-source
failures. A declared ushort array overlaps only same-type rd/wr/count fields;
the destination FIFO is a separate real array. Native CRC observes payload
before later cursor/count publications, while transmission observes the updated
values. The probes preserve that distinction, with disjoint and zero-frame
controls. They establish no ordinary allocation/protocol alias history.

The extended screen covers 300 objects / 1,886 shared defining copies, 316 native
251..600-byte copies, 223 inexact and 216 with backward jumps. Eight receive
native/source inspection; 215 remain uninspected. Backward cold rejoins are
not loops. SIMP/ASYC packers publish rd before loading FIFO[oldrd]; restoring
this makes SIZE28→11 and SIZE35→9. Sixteen synthetic union-backed component
cases / 177 assertions expose eight old-source failures and pass after repair.
The FIFO's declared backing really accommodates capacity16; pointer retention,
consumed word, cursor/occupancy, output/guards and complete normalized owners
are checked. Disjoint and zero-output controls stay green.

Mask-width × rd-publication controls give SIMP28/28/11/11 and ASYC35/35/9/9.
Both mask spellings already expand to SI-mode locals and SHR in initial RTL;
DWORD instructions do not uniquely justify widening the source local. Keep only
the independently observed publication repair. Reopen the residual with a new
consumed-value/owner/helper witness, not another width or expression palette.

The following section records the completed coupled pulse recovery. Prioritise
uninspected medium-screen rows with matched predicate/publication witnesses;
keep the frame and packer closed palettes recorded above.

## Unsigned interpretation, callback ages and native joins (2026-11-05)

The [pulse wave](../pulse-recovery-20261105.md) recovers IsPulseDialerReady
EXACT373 and unchanged-source SetPulseBreakTime EXACT88. The latter improves
from BYTES19 through a measured peephole2 scratch choice; nine other emitted
TU bodies remain unchanged. No field declaration or interface changes are needed.

Keep isolated no-gain controls. Unsigned consumption alone leaves SIZE68;
callback ages alone and A+B give SIZE26. Native shared-advance/source-join
controls give SIZE22/17, then native-sized BYTES145/4. Only direct unsigned
member-consuming comparisons close the last four bytes. Initial RTL already
shows the difference: local consumption compares break against elapsed with
LEU, direct consumption elapsed against break with GEU. This is a source
expression/observation boundary, not allocation. Three unsigned field-home
diagnostics all retain BYTES4; they are unretained, TU-only controls, not
whole-header validation. A no-gain isolated change can be required by the
completed source graph; trace collateral before rejecting an intermediate.

Cast before arithmetic. Native break+make is modulo-2^32 ADD followed by an
unsigned comparison. Casting the result of a signed sum retains a possible
signed-overflow expression. The selected source casts both members before ADD,
and consumes elapsed as unsigned only at comparison/advance sites. Signed int
setter transport and overlay declarations do not uniquely determine consumption.

Preserve asymmetric callback ages. Native retains dp->self from before the
entry diagnostic; hook-on diagnostics use entry remaining, while hook-off
reads remaining after modem_set_param. Both transition arms join the same
elapsed reread/advance; the returned field predicate also permits the native
late read. Nontransition paths retain their existing values. On pulse expiry,
clear elapsed, decrement remaining, then share the increment by five. Do not
freshen every owner/read on entry or blindly retain every local snapshot.

Country make/break values are u8; ordinary sums are at most510, with hook-flash
u8×10 also small. Setters accept unclamped int but that does not establish
normal high-bit configurations. Current host modem_set_param forwards hook
requests to driver ioctl; inspected ALSA/modemap/socket/test paths establish
no synchronous pulse API reentry. The ten new groups therefore separate two
normal/disjoint controls from synthetic public-PulseDialDigit reentry and
exploratory high-bit/modular-duration probes. They initialise component storage
and use public setters/load/poll, not a full call_create lifecycle.

The retained old-source control keeps all18 existing groups green and fails
seven new groups;330 assertions include downstream trace/state consequences,
not330 distinct defects. Compare full normalized objects, parameter-event
prefixes and exact diagnostics at each public boundary, and assert exactly one
synthetic callback on each side. Negative controls cover no callback, disjoint
host activity and arithmetic wrap. Source-dependent trace lengths are reported
with separate old/selected denominators. The fixture's own INTMAX+1 setup is
repaired with unsigned arithmetic; apparatus overflow must not decide a source
finding. No mutation infrastructure is used.

The complete300-object/header check, period gate and final measured census are
in the report. New source predictions require independently witnessed operand,
owner, width or value-age differences. A finite crossed matrix is not a global
optimum claim, and final evidence includes every protected neighbour rather
than only aggregate gains.

The Dialer transfer below completes this recommendation; its return-domain audit
finds only grades0..3 and establishes no negative-grade behavior bug.

## Dialer recovery: joins, local calls and original register identity (2026-11-05)

The [Dialer wave](../dialer-recovery-20261105.md) recovers AnalyseDialString
EXACT436 and IsDialStringInvalid EXACT149. Unsigned consumption alone does not
improve the wrapper. Preguard grade-name selection alone reaches BYTES4; both
reach BYTES2. Those last bytes are an unrelocated local-call displacement,
requiring the producer, GetNext and native function order together. Do not
normalize a real static-call distance away to manufacture EXACT.

Keep a promising intermediate through collateral repair. An all-break switch
initially loses the wrapper's EXACT grade. Crossing the native space/default
continuation graph with successful mixed-mode arm ownership restores EXACT
and takes GetNext from SIZE118 to SIZE1. Native selector range 32..119 and its
88-entry table distinguish this from the earlier87-entry source graph. The
space/default source spelling remains nonunique; supported emitted control
flow is the evidence, not a claim to know every original break/continue.

Analyse needs the native signed selector, early loop index, direct unsigned
grade lowering and modifier-validation arm ownership. Its complete 436-byte
body and all 88 ordered targets match. DialerCreate retains unsigned rejection,
preguard lookup and two distinct post-call modem-owner reloads, reaching BYTES5.
The producer returns only 0..3; int storage and unsigned consumption coexist.
Do not invent a negative-grade bug or change a shared declaration to explain it.

Audit the whole TU even when a local body is unchanged. Progress retains its
2,876-byte body/instruction sequence; its only nonrelocated differences are
four local-call distance bytes. A later Abort source correction changes
GetNext 894 to 896 bytes with literally identical GetNext assembly and normalized
RTL: assembly relaxation/alignment context is a different boundary from source
or allocator changes. Both preserve the wrapper span. No padding carrier,
store-order grid or compiler flag was added.

Table proof must distinguish an original callee-save value from that register's
later repurposed contents. The analyser saves EBX, redefines it, then publishes
it as a call argument; that argument is not another ABI save. Aligned fullword
frame locals can remain unknown if initialized on every admitted path. Neither
permits assuming their values survive a call. A higher-address clobber followed
by a backward jump to the first lexical save exposed a pre-existing unsound
acceptance: CFG identity must accompany save/restore tracking. The retained
real-ELF controls accept 134 cases and refuse 55; rebasing stays EXACT, swapping
ordered table targets is RELOC. Normal and -O runs use explicit checks.

Remaining bounded dives are recorded, not silently abandoned: constructor
constant stores become scratch-register operations first at peephole2; Abort's
error diagnostic remains a tail call despite an object-identical explicit-return
control; GetNext still has unmatched case/return-sharing relationships. Reopen
with independent predecessor, continuation/type or join evidence, not another
closed spelling palette. The baud-cap transfer below completes that next step.

## Baud cap: initialization order and an interior owner (2026-11-06)

The [baud-cap wave](../baud-cap-recovery-20261106.md) recovers chkForceBaudRate
from SIZE5 to REGALLOC389. Unsigned consumption alone gives BYTES272; early
session capture alone leaves SIZE5; both give BYTES264. The existing
member-derived interior view acquired at the native boundary improves to
BYTES55. Reading configuration after array initialization, together with that
late owner acquisition, closes the instruction graph to REGALLOC. The isolated
controls were required to distinguish the lifetime interactions.

Native witnesses are the session capture at 0xa479, array initialization before
configuration loading, obj+4 acquisition at 0xa490, and unsigned JBE/JA cap
branches at 0xa4e7..0xa50b. Keep original field homes: receiver accesses resolve
0x24c=4+0x248 and 0x250=4+0x24c, both 32-bit. A member-derived interior view
preserves named fields without asserting a unique original engine type.
The byte-derived selector is 0..7; use-site unsigned consumption establishes no
negative-input bug or unsigned declaration. Existing code already reproduces
the byte shift transport, so another byte-local declaration is unnecessary.

The full 56-body audit retains 54 literal bodies and all exports/order. The
sole other changed body is k56FlexPhase34's local-call distance after its
address moves 16 bytes; its target and SIZE28 grade remain. Raw rodata also
changes:67/89 relocated words move 16 bytes, while all owner/offset targets
remain identical. Do not call relocation-normalized data raw-identical.

Before stopping at REGALLOC, trace the residual: its scratch forms first
appear at peephole2 and 14 observed renaming decisions explain current lanes,
with plain/dump/observer/replay/integrated object parity. Reopen with new native
source/lifetime/predecessor evidence, not a register-fitting spelling grid.
The graph screen's target result pairs only one owner anchor and compares no
relations; its clean result is not evidence against the source discrepancies.

The 144 new component cases/1,873 checks cover all ordinary receiver arms with
active and inactive diagnostics and producer-supported allow values. Old and
selected source both pass. The existing 800 synthetic cases remain labelled;
partial session backing and absence of diagnostic reentry prevent lifecycle
or pointer-replacement claims. Retain those limits alongside the full period gate.

The HangUp transfer below completes that next step.

## HangUp: status capture among publications and explained collateral (2026-11-06)

The [HangUp wave](../hangup-recovery-20261106.md) recovers EXACT258 from
SIZE24. Early session capture and the existing interior-owner view each reach
SIZE4; both give BYTES36 at native size. Crossing native session-before-receiver
acquisition with explicit status capture after rate_want and before min/max
clears closes the target. Keep all isolated and combined controls. Status capture alone leaves HangUp's
body literal-identical but changes InitMOH, so unchanged target bytes do not
establish an inert whole-TU control. An emitted
load/store age can depend on the complete ownership graph rather than a missing
source ordering statement.

Native witnesses are p3548 capture at 0x6be1, the obj+4 rate owner, and status
read between the rate publications. Source already writes clr before loading
the demodulator; baseline sched2 reverses them and the selected graph restores
the native order without a barrier. Genuine field offsets are disjoint; no
behavioral old-fail or aliasing bug follows from these instruction ages alone.

Keep the complete byte-view extent explicit: selected engine is complete obj
plus offsetof(progress), not a member-derived byte pointer walked beyond that
member. Both spellings give the same complete TU object. Nineteen period layout
assertions preserve the resolved offsets and widths; no original engine type is
uniquely established. AGENTS now records this extent check alongside address
and width preservation.

The full 56-body audit preserves 54 literal bodies and every protected function.
Unchanged-source InitMOH remains SIZE32 but changes lanes. Its normalized RTL
agrees through flow2; peephole2 first differs. Two 362-event observations show
identical inputs for its 13 events but incoming cursor 50 versus 3; conditional
replay explains the selected choices. The cursor later reconverges. Trace and
retain this collateral instead of rejecting the recovered predecessor or
inventing operations to fit the neighbor. Raw rodata 75/89 addends shift 32 bytes;
all 89 ordered owner-relative targets and boundaries remain unchanged.

The new 18 paired status-0 observations/524 checks reuse the existing 126-region
registered-create graph, including exported reinit and repeated requests. They
cover a constructed component boundary, not negotiated status 1/2 or diagnostic
reentry. Mode 2's four static aliases are checked; other learned masked bytes
are compared raw, while old all-twelve checks remain. Old and selected source
both pass. Owner/stack target screens each pair only two anchors and compare
zero relations, so their clean subset is not an equivalence conclusion.

The InitMOH transfer below records the next completed investigation.

## InitMOH: shared byte transport, partial recovery and method masks (2026-11-06)

The [InitMOH wave](../initmoh-recovery-20261106.md) reaches native 444-byte
BYTES55 from SIZE32. A common flag publication alone remains SIZE32; B retained
interior progress owner gives SIZE6 (450 bytes); A+B gives BYTES228. Keep the
larger B control. Native 0x6cf2/0x6d89 supports the retained obj+4 address across
handshake initialization; 0x6e3b/0x6d27 supports the shared flag continuation.
Message publications remain separate. This is address retention, not a cached
mutable pointer field.

Extra High adds a conversion/use boundary. Native zero-extends the saved byte
once at 0x6d32, tests CL and publishes that same CL. The earlier spilled QI range
became a memory comparison plus a separate reload at greg. A common unsigned
int consumed-byte value restores sharing (BYTES56); narrowing its predicate
consumption restores TESTB (BYTES55). The formal remains unsigned char; all 256
byte values keep the same behavior and ABI upper bits stay unconsumed. Retain
D/E crossed with the earlier owner/continuation graph, including no-gain cells.
No unique original local declaration is established.

The existing reset helper matches the repeated algorithm and is whole-object
neutral in the tested combinations. Share it without scoring that as a gain.
The residual is narrower than the first reload diagnosis: actual local
allocation assigns quantity 74 to EAX at lreg with all Q registers available.
Read-only observer parity establishes that decision; peephole2/rnreg/sched2
explain later scratch lanes and independent-store scheduling. Reopen with a
new native-backed quantity/lifetime/owner/conversion constraint, not a type,
initializer or store-order grid. This wave adds no EXACT or REGALLOC function.

All 55 TU neighbors are literal unchanged, including protected HangUp and baud
cap. All 89 ordered table targets remain equal relative to their owners, while
75 raw addends move 32 bytes. The owner target screen compares five anchors/six
relations; selected stack screen pairs three anchors/zero relations. Report
those limits separately from differential and byte-category evidence.

The 108 initialized component scenarios/432 calls cross late and flag independently
on the existing 126-region registered-create graph. Native-generated words
separate their effects. Mode 4 installs ten static aliases, not the constructor's
twelve; initial old-source failure 108/2918 exposed that apparatus assumption.
The corrected check validates ten pairs and compares the untouched timing
pointers and all other excluded bytes raw. Preserve old twelve/four-word checks,
failed-run evidence and old/selected green controls. Four saved-snapshot
calibrations exercise both untouched-byte detection and installed-alias exclusion,
with restoration checks. AGENTS now makes masks
method-specific. No negotiated MOH or first-call full-graph claim is made.

Next audit VPcmV34InitiateRetrain's complete retained engine group: native 0x6667
acquires obj+4 for rate bounds, receiver flags, DC fields and progress; 0x68dd uses
it after handshakinit 0x68d8. Source uses direct fields and an unused byte owner.
Cross that owner lifetime with the existing reset helper after complete
owner/offset/width and caller-publication audits. Sharing alone does not predict a byte gain.

## V34 engine owner and complete caller-flow comparison (2026-11-06)

The [follow-up report](../flow-retrain-recovery-20261106.md) adds one actual
promotion: `VPcmV34SetMinimumSigLevel` is EXACT104. InitMOH remains BYTES55 and
Retrain remains SIZE11; those are separate partial results. Report completed
EXACT/REGALLOC functions first rather than presenting equal extents as recovery.

The minimum-level helper needs both the retained root+4 address and typed
component metadata. Raw integer access retains the address but delays the
debug comparison across the floor publication; flat typed access restores the
comparison age but uses the root base. A real embedded POD control-prefix view
supplies both. Its fields have one textual home and 29 permanent offset/width
checks for both views. The flat GNU compatibility view conservatively broadens
alias sets. This is a qualified ownership/alias model, not a recovered original
union or type name. Validate C89/C++98 acceptance and every affected TU.

The header change exposes six changed objects among 300 and 16 changed bodies
among 145 shared functions. Existing EXACT/REGALLOC functions survive. Delays,
Create, ApplyBulkDelay and descrambleGPC regress within the inexact population.
Trace these effects rather than treating them as a veto: debug PRE/HOIST first
changes executable RTL at gcse in the first three; descrambleGPC first adds a
reload at postreload after an alias-0 store. Native public Delays retains root,
not root+4, so an invented interior owner is not a supported repair.

Retrain's existing rate/floor/delay and reset helper algorithms match native
inline regions and are shared. The complete diagnostic comparison pairs 64
reachable blocks, 97 edges, 35 guards and 14 calls. Eliminating the extra
conversion and restoring independent owner groups can match the native extent
and instruction count while remaining BYTES225. That diagnostic spelling is
not adopted. Equal CFG topology and byte length do not establish allocator-only
residue; debug sampling, publications and live values still discriminate it.

InitMOH now has a [replayable complete caller audit](../../tools/native_graph/initmoh_flow_case/README.md):
95 reachable instructions, nine feasible paths and 21 call boundaries. All
3,603 relational obligations discharge within its explicit normal-return ABI,
address and opaque-callee model. Distinct late-byte/constant quantities explain
why no single global register substitution applies; disjoint store schedules
preserve complete memory at the checked boundaries. This does not promote
BYTES55 to REGALLOC or identify the missing original allocation constraint.

Parent review reproduced two false cleans in the first auditor: CALL addend
-4→0 reaches entry+4, and a global addend 0→4 reads the next word. Checking only
target names lost both differences. The corrected reader validates exact field
sites/types/addends and refuses overlapping fields and alternate entries.
A separate control reproduces solver AST-ID reuse; retaining live AST references
keeps the serialized DAG faithful. Require intended refusal reasons and normal/
optimized controls, not arbitrary exceptions counted as successful rejection.

Transfer next to Delays' root/session/config captures and Retrain's two debug
sampling boundaries. Carry the typed owner model through any evidence-backed
coupled repair, preserve every intermediate loss and rerun all 300 objects.
Use complete flow comparison where the bounded screen leaves important effects
uncompared; do not restart a type or store-order palette after a clean bounded
subset. Component-created Retrain/direct-helper histories supplement the older
synthetic probes without claiming negotiated PCM reachability.

## Delays and typed-owner transfer: five promotions (2026-11-06)

The [next wave](../delays-retrain-recovery-20261106.md) recovers SetDelays
EXACT259, SetMaxBlockLength EXACT42, SetTimeOut REGALLOC30 and both
MaxUpstreamRateIndex entry points REGALLOC123. Those five promotions are
separate from Retrain's remaining BYTES31. The five-promotion source passes
399 period tests plus structural checks before the final DC-model combination.
Use the report's final gate/closure evidence rather than that intermediate run.

Delays needs two independent properties. A reference to its actual dmadelay
short restores narrow alias metadata across the store and the debug PRE edge.
Repeated root-member expressions through the formal unify the first config
read with later reads in GCSE's expression table. The cached cast pointer's
initial owner pseudo had split that identity. Baseline, member-reference only,
direct-root only and both give SIZE26 / BYTES116 / SIZE26 / EXACT259. Four
ordinary/dump controls reproduce literally. Manual caching across diagnostics
instead removes native config reloads and is rejected. A root reference or
named embedded dmadelay spelling alone is neutral. Do not reduce this to an
alias-set number or infer a unique original C++ reference declaration.

Transfer the existing typed engine only with a fresh native root+4 witness.
SetMaxBlockLength then becomes EXACT42 without changing any neighbour.
SetTimeOut needs that owner *and* deadline computation before the sample clear:
owner alone BYTES20, early product alone SIZE3, both REGALLOC30. Preserve the
unsigned wrapped multiply. Both MaxUpstream copies become REGALLOC123, retaining
the signed receiver comparison, fresh config/session reads and diagnostics.
The combined five promotions do not automatically recover another neighbour.
The exact Delays helper does repair its inline Retrain debug reuse; measure
that effect separately from a persistent allocator-state hypothesis.

Retrain exposes one genuine eight-byte DC group: count16, estimate16 and
accumulator32 at engine+0x250/+0x252/+0x254. A real nested POD owner with flat
compatibility retains both the interior address and field metadata. Accessing
all three through that owner restores the native DC block; mixed flat/typed
views do not. Header-only is literal-neutral on the tested full TU. Removing
the redundant byte-formal copy and using the native session-before-receiver
capture order restores request transport and frame homes. Neither is a formal
type change. The first switch shares one widened diagnostic conversion; its
default arm keeps the independent native conversion. All properties are crossed
with the exact Delays helper and the five promoted functions.

The final native/current Retrain pair has 64 reachable blocks, 97 edges,
35 guards, 14 calls and 305 reachable instructions, with 30 strictly validated
relocation fields. This is a complete CFG and residual review, not exhaustive
value-domain equivalence. Scratch lanes and two independent scheduling groups
remain. Root status and receiver publication occupy disjoint fields of the
same valid complete object; swapping their compare/store order alone cannot
change the read value. Detector y2/x1 clears are likewise disjoint. Do not call
these demonstrated behavior defects or weaken REGALLOC to absorb scheduling.
Snapshot/conditional/reference controls and a direct-root status control do not
recover the native graph; the exact public reset helper does not justify a
caller store-order palette. Reopen with a distinguishing source/alias/liveness
constraint, not merely the remaining byte count.

Two transfer boundaries matter. RateReneg's actual typed engine migration is
whole-object neutral; session/receiver order narrows its own bytes but changes
no later function. SetV90RateReneg's engine owner alone remains SIZE22; the
existing txq ends before its native scalar-reset group and cannot safely stand
in for that owner. These are recorded negative/partial controls, not retained
promotions. Preserve complete owner extents rather than walking past a queue
subobject or manufacturing a TX type for scratch registers.

## Bulk delay and the coupled TX-owner repair (2026-11-07)

The [follow-up](../create-tx-recovery-20261107.md) adds ApplyBulkDelay
REGALLOC467 and v34FreezeEcho EXACT222, preserving all previous EXACT/REGALLOC
functions. Its 300-object closure changes six objects; the deciding phase
passes 399 period tests and structural checks. Create SIZE23, SetV90 SIZE6,
txinit BYTES8 and settxlevel SIZE2 remain explicitly partial recoveries.

ApplyBulkDelay needs four native-supported properties together: the actual
engine root+4 retained across memset, the original short formal at the entry
comparison, the long mask/DMA arm as fallthrough, and an actual short-member
pointer scoped to the final DMA update. Each ablation loses REGALLOC. Capturing
that pointer at entry instead creates an unwitnessed call-crossing lifetime.
The scoped pointer restores narrow alias metadata and native debug reuse;
four differing bytes are independently reviewed EAX/ECX scratch lanes.

The old TX queue prefix does not contain the scalar tail. Independent txinit,
txmit, modeminit, settxlevel and FreezeEcho instructions witness one complete
measured control group at root+0x221c, size 0x3c4, including all fields through
root+0x25dc. This supports a checked owner model, not a unique original type
name, union spelling or ultimate declaration boundary. Cross TX and engine
owners: SetV90's baseline / TX / engine / both are SIZE46 / SIZE20 / SIZE22 /
SIZE6. The last gap includes a native duplicate arm-store absent from initial
candidate RTL; do not call it allocator-only or retry equivalent if/switch
spellings without a distinguishing constraint.

Header-only makes handshak 15 bytes worse. Two inline FreezeEcho stores acquire
alias set zero and prevent debug PRE. Actual member use in the independently
witnessed FreezeEcho owner restores those inline substitutions, promotes its
public entry EXACT222 and makes handshak 38 bytes closer than baseline. This
is a measured coupled repair across a temporary regression. Matching PRE
counts do not establish whole-handshak equivalence.

Create shares existing rate, minimum-level and delay helpers to recover native
fresh session/config reads; it also recovers the DC owner and byte-sized quick
extraction. A binary decision tree alone cannot distinguish an authored switch
from if/else: the old comment ruling out a switch was wrong. The retained
switch reproduces the native tree, but repeated formal-root access that fixed
Delays does not recover Create's remaining engine-expression split. Test a
transfer on its own compiler evidence rather than treating a successful spelling
as a general rule.

A bounded canonical-root control also makes the complete `v34_object` and
native-mangled `tagV34Object` one type throughout an isolated closure. It is
whole-Main-object neutral, with compiler type-identity checks confirming that
the intended distinction really changed. Do not repeat this tag-rename family
as an explanation for Create's CSE split without new declaration evidence.

txinit's source through 31.bbro already has native reset-store order; 33.sched2
first reorders the three independent clears. Another source-order palette is
unsupported. For settxlevel, preserve the root binding captured before GetMinPR;
moving its constant TX-member address calculation across the call alone does
not change a mutable pointer observation. The partial residuals, full flow
counts, fixture domains and concrete reopening conditions are in the report.

## SetV90: predecessor recovery changes the later branch graph (2026-11-07)

The [follow-up](../tx-scheduler-recovery-20261107.md) promotes SetV90RateReneg
EXACT274 and txmitdibit REGALLOC250, with 399 period tests plus structural
checks passing and no prior EXACT/REGALLOC losses. Two of 300 objects change.
settxlevel's native-sized BYTES43 is separately labelled partial progress;
txinit remains BYTES8. These are dated measurements, not permanent counts.

SetV90 needs three properties together: the independently witnessed engine
owner in preceding v90Phase34, actual engine->progress in SetV90, and separate
constellation arms with native nonzero fallthrough. All eight ablations are
measured; only all three close it. Phase34 becomes SIZE99 from SIZE98 while
gaining its native owner. Preserve and trace that one-byte loss: rejecting it
would discard the exact SetV90 combination. Native/current emission order is
already identical, so no function-order permutation is needed.

Early expansion alone was an incomplete discriminator. A switch control keeps
two stores through peephole2, but ce3 merges them when both use AX. Paired
read-only captures of the complete TU each observe 360 scratch searches across
32 functions and reproduce ordinary assembly/objects literally. The owner
change leaves the target's pre-peephole instruction streams equal but changes
its incoming cursor 49->50. At equal target UID/live/eligibility sets, two arm
stores receive AX/AX versus AX/DX. Only the former can merge. The vector-clear
scratch changes CX->BX; sched2 can then carry its zero across preinitdigital.
No explicit zero carrier is needed. These measured current-compiler states
explain the interaction; they do not uniquely identify the author's declaration.

txmitdibit separately needs actual TX ownership, a pointer to its declared
scrambler member and a short quadrant result. Eight ablations give no promotion
until all three combine. The native argument is the real field, not a copied
automatic scrambler; modulo-four bounds make the short result transport safe.
The sole residual is a dead EDX/EAX frame-discard pop before the sibling txmit
jump. An unchanged local pre-emphasis helper also changes at greg; its complete
flag/value/call effects were reviewed rather than hidden by its unchanged size.

settxlevel's direct short-field consumption recovers the native 23-block,
34-edge graph and separate narrow test/widening, but initial hard-register
dependencies still reorder the MP read. Captures, late owner formation and
typed-formal variants do not close it. txinit's source already has native store
order through bbro; read-only rnreg tracing shows local use-age ticks choose
DX/AX/CX zero lanes, then sched2 reorders the independent clears. Do not transfer
the persistent-cursor explanation indiscriminately to that function.

Quadbit and EchoFilter remain bounded negatives with recorded reopening
conditions. A shared-scrambler capture loses the existing exact public helper
and changes the zero-iteration store graph; that particular diagnostic is not
selected. A failed control does not exclude every legitimate helper boundary.
The reobserved bitreverse Boolean-production difference is already covered by
F11639's closed statement-predicate control; it is not a fresh reopener. Check
the cited historical report as well as symbol search before repeating it.
Continue with quadbit's two-call lifetime and Phase34's remaining owner/flow
constraints; the target-specific reopeners are in the report.

## Quadbit, Phase34 and indication-owner closure (2026-11-07)

The [next wave](../quad-phase34-recovery-20261107.md) adds JdReceived EXACT150,
DilReceived EXACT150 and K56FlexRateDetermined EXACT64. Jd needs its actual
engine owner. Dil's complete baseline/engine/TX/both cross gives SIZE42 /
SIZE17 / SIZE9 / EXACT150. Rate is REGALLOC64 with its owner alone, then EXACT64
when the earlier K56Jd's independently witnessed RX/TX owners are recovered.
K56Jd itself remains SIZE15; inexactness alone is not grounds to discard that
supported change and its coupled exact gain. The larger DC/branch experiments
remain separate, unnecessary controls.

Phase34 recovers actual TX/RX member use, the native ordered 3..10 dispatch,
narrow counter publication/consumption and idle-dibit member transport. Its
staged crosses matter: owner/flow alone reaches native extent but BYTES625;
actual RX and direct count together recover native instruction count, then
the actual idle member closes the supported family at BYTES47. It remains
partial. The obsolete eight-entry table disappears, and the remaining Progress
tables retain exact case identities despite shifted bases. A valid instruction
target alone would not establish those identities.

The native/selected Phase pair has 291 decoded instructions and matching 45
branch/return sites. Remaining permutations are inside basic blocks, involving
disjoint owner reads or immutable table reads and count stores. A strict local
getMP call still differs because preceding source occupies 96 fewer bytes:
getMPrecvdBits contributes +32 including padding, RateRenegSilence -80 and
RateReneg -48. Recover these actual functions; do not canonicalize away the
distance or add caller padding to claim REGALLOC/EXACT.

Quadbit's missing lifetime was not another explicit flags snapshot. Repeated
actual flag-member reads yield a commoned non-user HI pseudo; an explicit
short local had become SI. Direct actual-member arguments at both scrambler
expansions restore native frame44 and its four spill homes. Actual signed
quadrant consumption keeps the clear source property; the selected 450-byte
body remains SIZE3 against native453. Both have 43 blocks/64 edges, but native
keeps a separate extension at the final join and another alignment instruction.
Frame size/CFG agreement alone does not complete this function.

The expanded initialized point histories add native public 2400/3429-baud
modulator setups beside the existing 600-baud path, comparing full graph bytes
and checking queue capacity. New indication histories exercise constructed
owners too. These are component histories, not negotiated protocol reachability;
the broad Phase34 planted-state arm sweep stays synthetic. The period/structural
gate passes 399/0. The report records complete-TU effects, input/helper hashes,
qualified negatives and exact stopping/reopening conditions.

## getMP and rate-renegotiation layout recovery (2026-11-07)

The [follow-up](../rate-layout-recovery-20261107.md) recovers getMPrecvdBits
EXACT895. RateRenegSilence and RateReneg reach native extents 983/555 but remain
BYTES14/13. Their real source recovery closes the preceding 96-byte extent debt;
Phase34's strict local getMP distance now agrees with native, with BYTES46
remaining. No additional REGALLOC/EXACT neighbour is recovered automatically.

Eight interacting getMP properties matter: actual engine ownership; the existing
max-upstream getter with a fresh post-DMA session acquisition; a checked twenty-
byte received-MP view; narrow initial MP publication before coefficient copies;
branch-local capability reads with short loop transport; zero-bit-first arms;
mask initialisation between getter and rate guard; and source pointer publication
before capability value initialisation. Every individual ablation loses EXACT.
The input view starts at the complete session and derives each address from
declared members; it does not walk beyond the first char subobject or establish
an original standalone MP type. The shared getter returns a rate despite its
historical Index name; compare its full algorithm and units before reuse.

Machine store order does not uniquely determine source order. Native compares
the narrow count before its final store; direct member preincrement nevertheless
reproduces that graph after compiler store sinking. A named short local instead
creates SI transport and an extra extension. In getMP, source pointer-before-
value initialisation emits native value-before-pointer order. Its final losing
BYTES27 control differs at rnreg and then sched2; no persistent-cursor cause is
inferred from that function-local observation. Use defined mask multiplication
and OR rather than left-shifting a negative signed value; the checked objects
are literally equal.

The pair's eight owner combinations preserve temporary EXACT losses, including
SetV90 and Jd/Dil, until actual TX/RX/engine ownership repairs them together.
All-owner × narrow-count × actual idle-member controls recover native operands
and sizes. Crossing the independently exact helper then closes strict call
distances. First-increment/selector spellings are raw-identical controls; they
do not reopen the residual. Complete pair review leaves two independent vect4/
count scheduling groups per function. Reopen with a new helper, declaration or
liveness witness predicting those dependencies; neither same-size code nor the
bounded graph screen establishes equivalence or allocator impossibility.

The final gate passes 399 period tests plus structural checks. All 56 Main
functions, 38 protected copies, 225 CALL identities and 81 data continuations are
retained. New constructed fixtures add 10,918 checks: 288 bounded rate prefixes
in setter-created states 11/15 and 16 accepted Type1 MP packet histories. They
compare all 126 heap regions raw. Threshold transitions, closing CP, sensitive
ISP and low-rate capability-loop histories remain outside the new domain;
existing planted-arm coverage stays synthetic. The report records controls,
complete-TU and 300-object evidence, compiler/helper hashes and next targets.

The Trn2d follow-up is now completed below. Its historical F12052 record was
located in archived Git history, and the described source properties were
reconstructed against current native instructions. The old rejected source
bytes were not recovered. Its K56Rate loss is repaired by the coupled K56Jd
owner recovery; the earlier isolated rejection does not apply to that result.

## Trn2d, K56Jd and the coupled Phase34 follow-up (2026-11-07)

The [follow-up](../trn-k56-recovery-20261107.md) adds Trn2d EXACT127,
K56Jd EXACT247 and chkForceBaudRate EXACT389, preserving K56Rate EXACT64 and
every previous EXACT/REGALLOC copy. K56Phase reaches native 721 but remains
BYTES8. These are three exact promotions plus one partial recovery.

Trn2d needs actual engine ownership and per-arm state stores together. The
owner-only result is SIZE24; stores alone BYTES37; both EXACT127. Trn2d alone
also promotes chkForce from REGALLOC to EXACT, but moves K56Rate to REGALLOC.
K56Jd alone likewise loses K56Rate EXACT. Both recover it. Read-only full-TU
peephole2 captures show identical Rate scratch eligibility with incoming cursor
0/2/3 and AX/CX/AX selection at UID41. This confirms the established TU coupling;
it does not mean every register mismatch has that cause.

K56Jd restores its real eight-byte DC owner, complete-root diagnostic reads and
per-arm constellation stores. Two disjoint clear pairs then interact: baseline
combined BYTES17, DC source order only BYTES2, TX source order only BYTES15,
both EXACT247. Native machine order is not necessarily source order. Preserve
ordinary/dump parity and the complete bystander ledger before attributing an
effect to a declaration or compiler cursor.

K56Phase needs actual RX/TX/engine owners, ordered 3/4/5 dispatch, the native
unmasked shift, actual idle quadrant consumption and earlier session capture.
The bounded declaration/reset/context crosses leave only two adjacent disjoint
zero stores reversed at sched2. Native and selected have 162 decoded instructions,
17 matching branch/return operands and 12 CALL operands. Reopen with a new
native-supported helper/declaration/alias/liveness constraint predicting those
dependencies, not another mask/order/owner palette or an allocation-only label.

The constructed fixtures add 7,444 checks: 20 Trn2d histories assert all four
outputs and successive 10→14 / 18→20 transitions; 24 actual K56 session Ja prefixes
stay within queue capacity. Native Ja/MP stubs return zero and write nothing,
so their completion tails remain unreachable under that callee contract.
Case 3's bounded-index induction is not a constructed entry history; planted
arm/index probes remain synthetic. All 126 live regions are compared raw.
The final period/structural gate passes 399/0. Only four Main bodies and one of
300 source objects change; exports, order, 225 CALL identities and 81 data
continuations are preserved. The report records the full corpus screen and
the next native-backed power-reduction owner/acquisition investigation.

## Minimum power: observation boundaries and arm identity (2026-11-08)

The [deep dive](../min-power-recovery-20261108.md) adds no EXACT/REGALLOC
promotion. GetVPcmMinimalTxPowerReduction reaches native 362/BYTES29 from
SIZE33/current 329. Retain it as partial source recovery: actual engine/config
owners, path-specific params acquisition, distinct clamped/final short values,
and native codec-read timing. No other Main body changes.

Native publishes HIGH_LEVEL_TX_ACTIVE, then reads captured config+0x54, then
publishes the echo fields. Baseline source read codecType after the first two
echo stores through a fresh config-pointer expression. Constructor fixtures
have disjoint owners and do not demonstrate an alias-sensitive behaviour bug;
the native observation boundary is independently measured. The post-diagnostic
reload is params from the retained session, not a session-pointer reload.
Keep config+0x44's signed low-word consumption despite its 32-bit transport.
Neither the writer width nor a byte intermediate proves an original declaration
or legitimises the final typed dereference.

High-effort owner/acquisition/clamp and 36 echo-order controls reach a partial
candidate whose two beta stores merge at ce3 after peephole2 assigns both AX.
Extra High review tests a different source-arm identity: writing red<=0 first
changes block/search order, keeps distinct scratch lanes until ce3, and preserves
both native stores. BBRO still emits native positive fallthrough/JLE. The graph
change justifies crossing the previously negative 36-order family again; all 36
remain negative for EXACT/REGALLOC. A short-beta/shared-HIGH_LEVEL six-cell
family also fails. This is an interaction result, not a licence to repeat an
unchanged spelling palette.

The final pair has 80 decoded/78 nonpadding instructions, all 11 branch/return
sites and both diagnostic CALL sites equal. MOV/XOR scheduling and register
transport still fail strict REGALLOC. Matching extent and CFG are insufficient.
Read-only full-TU captures reproduce ordinary objects literally. A calibrated
53-cursor replay of the pre-inversion beta1p0 graph permits native positive-beta
EBP only at target entry 15; the current SetTxScale searches map every incoming
cursor to 2 or 3. This rules out cursor-only repair of that fixed graph. It is
not a negative result for the changed arm-inversion graph. TU-wide
coupling remains established: changing the actual search graph or eligibility
is outside that conditional negative, even for an already exact predecessor.
The final orientation's separate calibrated replay confirms that target entries
2/3 keep distinct positive/false beta scratch lanes; entry 2 is compiler-observed,
entry 3 model-only. This is not a later-pass or whole-function proof. Bind every
conditional exclusion to its exact source/search graph before transferring it.

The new fixture supplies 32 constructed component cases/64 calls/4,808 checks,
with 22 changed graphs, full 126-region comparisons and independent expected
returns/flag/echo values. Sensitive-ISP/layout-zero and pointer-alias histories
remain outside that new domain; old planted probes stay synthetic. The report
records both effort denominators, complete-TU/300-object checks and the gate.
Reopen with a new native-supported false-beta live-range, declaration, alias or
helper boundary and cross it with both source-arm orientations. Do not repeat
the closed echo-order, short-beta, shared-publication or Boolean-capture domains
without such a constraint. Transfer the actual-member/result-lifetime lesson
next to GetCurrentTxBitRate after its prior-control audit, crossing supported
changes with both minimum-power arm orientations across the complete TU.

## TxBitRate: Astra closes the shared-result cross (2026-11-08)

The [follow-up](../txrate-recovery-20261108.md) recovers
VPcmV34GetCurrentTxBitRate EXACT213 in both minimum-power orientations. High's
44 scored cells and Extra High's76 had no promotion; mandatory Astra escalation
adds the missing interacting source cross. All41 protected Main copies survive.
Minimum power remains BYTES29/current and BYTES45/alternate: the earlier exact
getter does not automatically recover that later function.

Inspect the mode of each arithmetic operation. Native FMULS keeps a float
reciprocal, while FLDS/FADDP performs the addition wider than float. A four-byte
constant pool entry does not establish the sum's mode. The existing
V90Demodulator::getBitRate/F8060 product-only widening is the internal source
control; operand commutation and whole-expression widening are not substitutes.
Double/XF-compatible spellings do not recover a unique original declaration.

Direct fallback access and wider addition alone remain inexact. The PCM status
switch removes status/result interference; a shared integer result changes the
return-edge predictor and places K56 before V92. The final source-role order
then prevents rnreg moving fallback EAX to ECX. With all arithmetic/direct-
fallback/switch properties fixed: direct returns with either role order give
BYTES135; shared result/non-PCM-first gives BYTES2; shared result/PCM-first
gives EXACT213. BBRO still emits native non-PCM-first machine order. Trace
branch prediction, live ranges and source block identity before attributing
placement to source case order alone.

Astra audits54 scored cells, with original-order56 functions,103 exports,
225 CALLs and81 data continuations preserved. Ordinary/dump controls reproduce
literally. Float-pool raw reorders are recorded separately from canonical
constant-target equality; they are not whole-object raw identity.

The separate Astra minimum-power review has24 scored cells and no promotion.
It does recover a better declared-member spelling: `short want =
cfg->unnamed_0044` is raw-neutral in both orientations and eliminates the
incompatible short-pointer read. The model stays int for32-bit transport;
period output still consumes the signed low word. The old raw-access requirement
in the header comment is retired. This is a source cleanup, not another grade win.

The valid construction survey reaches only status0/ratecfg.txbits0, so no
redundant zero-only permanent fixture is added. Existing arithmetic/gate probes
remain explicitly synthetic; an actual protocol history into ready PCM is still
unproved. The report records current period validation and full-TU/300-object
effects, the escalation's added evidence and the next native-backed target.

## Reconcile independent recoveries before refreshing a floor (2026-11-08)

The November 8 integration combines two independently developed histories.
Audit the named protected sets from both, including stale historical entries;
an aggregate gain does not explain a missing function. Preserve source-backed
recoveries, trace losses, and test supported combinations before deciding which
source to retain. Do not silently lower a floor or weaken the classifier.

`V90MP::evaluateCRC` is the concrete coupled repair. Holding the complete merged
TU fixed, moving the loop increment to the `for` header leaves SIZE8; using
`abs(d)` alone leaves SIZE16. Together they recover EXACT651, with the other
15 emitted copies unchanged. The baseline is SIZE15. The native branchless
absolute-value graph supports the arithmetic choice; the exact combination
does not identify unique original syntax.

`V92Jd` retains its single low-byte lookahead capture. Placing the bit-0 source
assignment before bit 1 recovers both EXACT211 constructors; the compiler still
emits the native bit-1-before-bit-0 publication order. All other 19 copies are
unchanged. A scope-only control is neutral; reversing source order with either
scope recovers both constructors. This supersedes the older constructor SIZE
status below. Machine publication order does not force source statement order.
Do not attribute the improvement to an inherited scratch cursor without the
corresponding compiler trace.

The V.21 control merge supplies a smaller control. Explicit early locals and
the direct assignments emit the same native three-load-before-two-store epoch.
Restoring the direct source form recovers EXACT94; the separate status body
and its later request observation remain unchanged. Preserve the witnessed
observation boundary, rather than treating one source spelling as mandatory.

One real upstream loss is retained explicitly: `V92Modulator` C2 is BYTES7
instead of EXACT734. A current-input cross identifies `Queue::write`'s existing
`isFull()` predicate as the cause. Reverting only that predicate restores the
constructor but changes the native EXACT85 queue writer to SIZE14 (71 bytes
against 85 native) and worsens `progress` from BYTES55 to BYTES70. Both complete source bodies reproduce this header
effect. Retain the supported queue implementation and investigate the coupled
constructor recovery next; do not restore the older expression solely to protect
its bystander. The specific scratch/renaming cause still requires compiler dumps.
This supersedes the historical Queue no-adoption status recorded later here.

Two other floor entries were classifier migration errors. Fresh upstream
objects for `V90Jd` C1 and `recivedRt` are identical to their merged counterparts;
the upstream classifier accepts REGALLOC where the stricter integrated one
refuses it. Preserve those NONMATCH results and record the correction rather
than weakening the register proof or describing them as new source regressions.

The integration checkpoint explicitly refreshes the named floor to 1,231 EXACT,
1,266 matched and 1,234 link-selected EXACT functions across 1,852 names.
The [review ledger](../evidence/master-integration-20261108.json) preserves every
lost historical membership, its cause and reopening condition. The two maximum-
upstream getters retain REGALLOC with bodies unchanged from the local parent;
`dp_vpcm_exit` retains the upstream-reviewed REGALLOC result after the source
order correction. Its former displacement entry is archived in that ledger
because the refreshed matched floor now protects the actual REGALLOC result.
The retained constructor tradeoff remains NONMATCH. This is an explicit source
review, not a classifier relaxation or an unreported net-count replacement.
The refreshed detector still rejects a known lost getter match.

The integration also exposed apparatus closure defects: frozen graph helpers
must include new lazy classifier dependencies, and host-side fixture preparation
must read the same `.ldcontext` declarations as the link stage. The full default
test list uses newlines; space-only substring selection silently omitted both
context objects. Keep the failure evidence, repair apparatus, and rerun the
period gate without changing the oracle or source to accommodate it.

## V92: block copies, captured inputs and shared instantiation (2026-11-08)

At baseline `9eb32e79a`, V92Modulator emits 30 shared function copies, 25 EXACT.
The recovered Queue block read is 197/native 197 bytes (previously 215); block
write is 211/native 211 (previously 213); progress is 1075/native 1075, from BYTES55.
The paired template boundary also recovers C2 EXACT734. Scalar Queue write
remains EXACT85. These are four whole-function promotions; the mk partial below
is recorded separately.

Both block entries reject capacity using the unsigned ABI argument, then consume
a signed length for their copy comparisons and loops. Native acquires the
contiguous bound after the guard, lays out the non-wrapping arm first and
publishes the owner cursor after each element in both arms. Combining those
properties and an ordinary consumed signed local recovers both complete bodies.
The unsigned formal and object layout are unchanged. Native signed consumption
does not identify a unique original local declaration.

The intermediate losses mattered. Moving only write's bound after the guard
recovers C2 while making block write's gap larger. The completed write repair
keeps C2 EXACT too; adding the independently exact read changes C2 back to
BYTES7. Preserve these controls and repair the coupled boundary rather than
freezing either exact helper or restoring a less faithful queue algorithm.

Progress needs two independently supported captures. A count-value snapshot
between `eventCode = 0` and `symbolCount += n` admits the native queue-load group.
Capturing `txFilter` before the buffer pointer admits the native shaping-call
producer group. Separately these leave BYTES27 and BYTES28; together progress
is EXACT. Whole native correspondence bounds each root, offset and width; the
source order does not require every machine read to remain at that position.

The remaining C2 cause is the local bulk Scrambler instantiation. Native has an
out-of-line call to `Scrambler<int,unsigned char>::process` and strong-method
order mk→progress→reset→C1→C2. Current local instantiation instead postpones
progress until after the constructors despite the textual definitions' order.
A per-member `extern template` declaration suppresses process and its otherwise
local copyHistoryTail dependency and restores the complete native strong order.
The existing V90Modulator TU explicitly provides the member; that provider's
complete object remains literally unchanged. This is a documented G++ 3.4.2
forward-instantiation extension, not an added assembly or compiler option.

The V92 copy denominator changes 30→28: two duplicate weak bodies disappear
locally and remain supplied, EXACT and weak/default-visible, by V90. Across
both TUs there are 53 emitted copies and 49 unique names; the unique program
closure must still be measured. Do not count omitted copies as promotions.
Native coalesced weak sections do not prove uniquely original instantiation
ownership or spelling. The specialization-declaration control is diagnostic
only: it has no established matching specialization definition. Use the paired
instantiation declaration/definition and check both provider and consumer.

Repeated ordinary, dump, native-host and read-only-observer builds reproduce
these objects. Constructor streams agree through flow2 after resolving emitted
constant labels by payload, then differ at peephole2. Suppressing process moves
progress's real searches before reset; C2 enters cursor 0 instead of 2 and becomes
EXACT. C1 reconverges by renaming. Do not erase unresolved early constant labels
or describe a register-renamed contiguous run as a literal suffix. A diagnostic
no-unit-at-a-time profile changes order but leaves C2 wrong and loses exact
neighbours; it is not retained.

The mk partial restores native early FABS lifetime, full-width sign transport
and the late float read at root+0x24. Direct argument expressions preserve the
existing explicit extended subtraction/multiplication, fabsf whole conversion,
integer absolute value and format. Helper-call argument trees can be precomputed
before outgoing argument publications even when they later inline; CSE then
reuses a field that native reloads after those stores. Matching calls.c explains
that boundary. No volatile, new alias type or artificial use is introduced.
The retained candidate is 648/native 662 bytes, from 653/native 662: its SIZE gap
worsens 9→14. Its restored native operations justify the partial; it is not an
EXACT/REGALLOC gain or a uniquely recovered macro. The residual comparison-tree
and block-placement choices remain open. If/else controls recover CMP2/DEC1 but
leave SIZE40; repeating case-order or helper/local/sign palettes adds no witness.
Reopen with a discriminator for the native register-loaded scale or remaining
block-placement/lifetime decisions, carrying the argument boundary forward.

Sol High recovered the block methods and progress. Extra High's 13 scored cells
closed no remaining target; the required Astra escalation recovered C2 and the
mk feature boundary. Neither model identity nor these samples establish cost
efficiency; actual token and billing measurements are unavailable. Retained
objects are independently rebuilt and the paired boundary receives independent
ABI, compiler and ownership review. The evidence ledger records separate trial
and validation denominators.

Focused constructor/Queue/state checks cover constructed component graphs,
wrap/drain and signalling-NaN copies, plus labelled planted protocol states.
The new 13-case diagnostic probe uses constructed buffers, phase 0.5 and a pending
offset request, comparing native transcripts around signed zero and adjacent
fractional/whole truncation boundaries. It is a synthetic component probe, not
negotiated V.92 reachability. No tolerance or oracle is weakened.

The periodic 300-object graph screen also exposes two SpectralShaper nominations
that pair different loop guards. Audit flag origin and iteration identity before
transferring an observation-age hypothesis; bounded graph matches are not an
operation-age proof. The read-only next source nominee is V90Modulator progress:
three native symbol-buffer loads at root+0x68 occur after generateSymbol calls,
while the current assignment expressions acquire that pointer before the calls.
Test the two mixed-loop captures, the fixed phase-4 capture and their combination
in the full TU; retain its remaining event-tail and x87 obligations explicitly.

The final `make phase J=8` passes 404 period tests with zero failures. The
focused fixtures pass 797,129 checks across 20 sections, including 2,250 new
diagnostic-boundary checks. Fresh classification records 1,235 EXACT, 35 REGALLOC
and 582 NONMATCH across 1,852 symbols, zero unresolved, and 1,238 link-selected
EXACT. No previous EXACT or matched name is lost. Only one of 300 production
objects changes; the explicit V90 provider remains raw-neutral. The strict
semantic/raw partial-object comparisons both remain DIFFERENT.

The [wave ledger](../evidence/v92mod-coupling-20261108.json) records frozen
inputs, individual model efforts, crossed controls, complete closure and final
validation. Full-object byte identity remains a separate measured goal.

## Resolving an owner declaration after an alias-contract recovery

Seek independent original type evidence first: matching original headers or
source releases, debug types in another object build, type-bearing mangled
symbols, callback/API contracts, or initialized union/aggregate consumers.
A plain C symbol name and an instruction displacement do not supply that type.
The current reference has neither DWARF nor STABS sections.

Then use consumers such as Detect_v22 and V22FP_create to predict a declaration's
behavior before compiling it. Map owner reads around publications and calls,
compare candidate RTL alias sets and complete native/TU obligations, and test
supported combinations across the full header closure. Preserve temporarily
worse controls. Matching control/status/Demod does not select a unique owner
view: the two tested models already reproduce the same relevant objects.

If independent evidence never distinguishes the surviving declarations, record
an observational equivalence class and retain an explicitly qualified alias
model. Full byte identity can validate emitted code without recovering unique
source syntax. Do not invent uses for an unused union member to make it appear
better evidenced. Keep the unresolved declaration separate from measured gains.

## Use whole-TU evidence and interacting candidates

The initial [V22 control recovery](../v22-control-recovery-20261105.md)
combines request-byte ages, conditional recapture and an HDX alias boundary
to reach REGALLOC. Its two neighboring BYTES regressions are restored by the
[coupled HDX/Demod recovery](../demodv22-recovery-20261105.md). Keep both the
original tradeoff and the later interaction when evaluating this mechanism.

The [recovery pilot](recovery-pilot.md) adds a practical loop: retrieve prior
attempts, pin inputs, compare baseline/A/B/A+B, inspect all changed bodies and
data targets, then run period fixtures at a stated boundary. Use
`tools/recovery_experiment.py` for the immutable ledger and freshness queries;
`tools/gcc3_search_compare.py` analyzes stable content snapshots so a concurrent
write-and-restore cannot silently grade different bytes under the recorded hash.
A failed A or B alone is not evidence against A+B: CrossDataLinks required both
conversion helpers for its historical REGALLOC result.

SIZE and BYTES can both hide wrong reconstruction. REGALLOC can also hide
width/lane and dataflow mistakes; width and bit-position checks now reject
AX/AL and AH/AL substitutions, but CFG, implicit registers and ABI limitations
remain. A stricter heuristic demotion does not establish a new source defect.
Use `tools/gcc3_owner_trace.py` to nominate bounded owner/width cases and read
its supported/unknown/ambiguous denominator before interpreting silence.

A shared machine tail need not have been shared source. Duplicating CPt's
unchanged return operations into its two source branches made the standalone
function exact and retained native redundant conversions in its inline caller.
It also moved a later, unchanged reset function's registers: the observed
scratch cursor changed in CPt and persisted across the TU. Reset's dumps first
differed at peephole2, not initial RTL. Review that collateral explicitly;
matching local source or unchanged function event counts do not isolate a
function from earlier allocation history. Do not reorder functions or add
artificial state carriers merely to obtain a preferred dead register.

Use `tools/gcc3_stage_divergence.py` to find anchored structural/operand changes
and unresolved alignment, not to erase operands and manufacture equality.
Its cgraph summary retains printed clone records; it does not prove which
record maps to a constructor ABI symbol. `tools/gcc3_alloc_trace.py` observes
local choices and global-pass entry on a pinned captured compiler, verifying
unchanged assembly/object output. It is instrumentation, not a complete model
of global allocation or reload. Earliest observed divergence is a location to
investigate, not automatically the cause.

## Classify the compiler stage before searching register spellings

The [six-step follow-up](../six-step-recovery-wave.md) adds bounded
entry-memory owner paths and explicit pseudo-register def/use windows. Read
their supported/unknown denominators: an unchanged immediate definition
pattern does not prove unchanged input values, and a captured pointer is
different evidence from a reload after a call or aliasing write.

The [V.34 small-RTL study](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-small-rtl.md) separates several mechanisms
that can produce similar-looking assembly differences. Use a small control
and the corresponding GCC dump to identify the stage, then transfer the
hypothesis to the complete translation unit under the retained flags.

| Observation | Discriminating control | What the control establishes |
| --- | --- | --- |
| Incoming argument stays in a register instead of being reloaded from its stack slot | A small unsigned accumulator function with increasing register pressure; inspect global allocation dumps | Normal allocation can spill the argument home. A forced register or fabricated local spill is not recovered source. |
| Boolean result becomes a branch or setcc | Cross ordinary return/if forms with diagnostic if-conversion options and inspect ce1 | Source control flow and if-conversion are separate causes; a flag that changes many other bodies is a diagnostic. |
| A conditional store appears absent in a standalone helper | Compare the same helper after inlining and inspect GCSE/store motion | The caller may already recover the observed store. Do not adopt a standalone-only improvement. |
| Constant multiplication has the wrong instruction graph | Cross literal/const coefficients with ordinary coefficient locals, and inspect expansion then CSE | polyValue became exact at 28 bytes: late coefficient propagation folds an existing multiply, while early constants cause LEA/sub synthesis. Association alone did not recover it. |
| A comparison dispatch differs and source invents default values | Cross switch versus observed comparison chain with and without the invented initialization | preempindex became exact at 315 bytes only with descending comparisons and original unset locals. Keep supported inputs distinct from undefined caller-register state; byte identity does not define invalid inputs. |
| Two role loops appear in the blob | Compare one loop with the mode test inside against preselected taps and explicitly duplicated loops; inspect loop2 | V34scrambler became exact at 256 bytes through unswitching. Source arm order determines version layout; two binary loops do not establish two author loops. |
| A short initializer has a different loop/register shape | Cross index width, address expression, and independently observed store order | dpskDetectInfo1Init became exact at 152 bytes with a short index, root-relative clears and the blob's field order. Two address spellings matched: this is a family, not a unique original spelling. |
| A tiny coefficient loop differs from straight-line blob stores | Compare expanded cached pairs with scalar assignments preserving reloads | txrxdmainit became exact at 98 bytes only with the observed reloads; expansion alone was insufficient. Five fixed alias fixtures distinguish the old and recovered behavior. |

Keep the component boundary explicit. Those five alias fixtures are valid
inputs to a state-free leaf; they do not establish modem lifecycle coverage.
Do not invent an owner or traverse beyond a declared member array to obtain
a desired register. The FSK clear spans several fields and padding, so it
uses the complete object's byte storage rather than a fictitious array.

Audit the **purpose** of the comparison build before interpreting results.
Both make period and make tc must define DSPLIB_REPRODUCE_BUGS after
configurable flags. Without it, V.34's pre-emphasis selector changes from
initialized data to zero-initialized BSS. Canonical instruction comparisons
mask relocation addends and cannot by themselves detect this semantic
configuration mismatch. Preserve the invalid run, rebuild the unchanged
control, and report configuration gains separately from source gains.

A recovered owner can settle live register allocation while dead registers
remain compiler artifacts. V.34 FreezeEcho's real transmitter-prefix pointer
restores its SI base and frame; the last two bytes are dead pop destinations.
The peephole2 control identifies scratch selection, and evidence-backed local
definition orders leave those bytes unchanged. Keep the owner evidence and
scratch result separate; do not fabricate an aliasing view or migrate a type
merely to obtain a desired dead register.

Source widths can settle live coalescing without settling dead scratch bytes.
In txmitdibit, a real transmitter owner, direct state pointer and either a
short result or quadrant restore every live operand; one dead pop byte stays
different even after restoring the observed adjacent-wrapper order. The old
five-byte size gap concealed a copied state and non-tail call. Keep these
controls as evidence; do not adopt a broad owner migration solely to chase
that final scratch register.

A smaller size gap is not an adoption criterion. Splitting V.34 timing's
role-dependent switch into two switches brought the gap from 21 to 7 bytes,
but produced no exact function and changed another caller. It remains a
source hypothesis. Review exports, all changed bodies and the complete exact
set before adopting even a locally exact candidate. The FSK recovery kept
55 global definitions and raised the TU from 5 to 6 exact functions out of 29;
the large handshake remains non-exact.

## Reconstruct the owner before tuning its generated code

`FIELD`, `FIELD_PTR`, and width-specific offset macros are useful during
initial recovery, but they are not a completed instance model. Before
searching for a scheduling or register-allocation spelling, recover the
containing structures and their actual member types. Equal byte offsets do
not establish equivalent aliasing information for the compiler.

First distinguish missing types from unfinished migrations. V.32's sequence
code used genuinely unmodelled owners; V.22 still used raw offsets into an
owner whose constructor and shared structure had already been reconstructed.
Comments saying an instance is deliberately unmodelled can outlive that
decision. Check the current constructor and type home, not just the comment.

1. Establish allocation extent, initialization, call-site types and
   destruction together. Reuse existing embedded types and keep each owner
   definition in one shared home.
2. Preserve independently addressed aliases, pointer ownership and observed
   reloads. Replacing an offset expression with a member does not justify
   caching a pointer across a call or combining stores.
3. Name only what the evidence supports. Keep unmodelled gaps as pads and
   known-width, unknown-role fields neutral. Five accessor-visible registers
   and seven constructor clears do not prove a seven-element register bank.
4. Assert the period layout with checks that GCC 3.4.2 actually sees. Wider
   native pointers are a portability matter, not evidence against the
   original four-byte pointer layout.
5. Run the period differential and structural gates before committing;
   mutation suites and anchor maintenance are retired. An incremental prefix
   view is a migration step, not completion of the owner model.

Initializer cleanup is a separate question from arithmetic narrowing. A
cast on a hexadecimal constant assigned to a `short` array can be redundant
under the period compiler; the destination already performs that conversion.
That does not license deleting a cast that narrows a computed intermediate
before a call or store. Preserve the declared table type and validate the
cleanup with the deciding compiler.

See [the V.32 reconstruction checkpoint](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-structure-reconstruction.md)
for the first layout migration and its remaining owner work. This method
does not claim that modelling an owner alone guarantees byte identity.

Every lever here was measured in this tree, and every one carries its
counterexample. A lever without a known failure is a lever nobody has pushed
hard enough yet.

**The target is grade 0 — positional byte identity, per function.** Not byte
count, which moves when we emit more code rather than more of the *right*
code. Not instruction count: `qcLineVerification` measured 159 instructions
against 159 and was still wrong, a `movzwl` copied as 32 bits (finding F7630).

Measure with `tools/toolchain/byteident.py`:

| flag | what it gives you |
|---|---|
| *(none)* | the tree's grade counts |
| `--why SYMBOL` | **the row `alpha_equal` rejects on** |
| `--list-exact` | the exact SET, for a diff |

Three habits, each of which has cost this project real time when skipped:

1. **Diff the SET, not the count.** A count nets to zero across a gain and a
   loss, and a fix once changed three functions its author never disassembled
   (7764).
2. **Baseline before you change anything.** A number measured only after a
   change is not evidence about the change. Four separate attribution puzzles
   here came from this (7763, 7769).
3. **`--why`, not eyeballing the diff.** The first row that DIFFERS is usually
   not the row the comparison rejects on. One pass quoted the wrong one and
   had to correct itself (7778).

`make phase` does **not** build `build/tc_out` — it needs docker and the
toolchain image, which not every checkout has. `make byteident` and
`make similarity` DO, since both now depend on `make tc`
(`tools/toolchain/period.mk`), which is incremental: one edited source
recompiles one object. The staleness guard stays anyway, because it also
catches the directory being read by something that did not come through
make — it once reported pre-merge grades as current and they looked entirely
normal (7769).

---

## AIM A PASS AT A TRANSLATION UNIT, NOT AT A SYMBOL

The tree's own evidence says the compilation unit is the natural unit of work,
and every mechanism this file documents that acts at range does so through one:

- **Lever 3's carrier is a cursor threaded through a TU in emission order**, so
  fixing one symbol moves its successors. A symbol at emission index 0 cannot
  be reached by a reorder at all, and fixing an EXPOSED symbol early in a file
  changes every symbol after it (F7827).
- **Bystanders are the rule, not the exception.** `loadParams` -- 7,894 bytes,
  the largest such symbol in the tree -- closed as a bystander of a reorder
  aimed at something else (F7800). A pass editing `Scrambler.h` closed eleven
  symbols in a directory it had been fenced out of. `V92Modem::printTitle`
  closed with no edit to it at all, and was recorded as UNSTABLE for the same
  reason.
- **The regression runs the same way.** F8080's constructor fix cost
  `V90Jd::packData` 22 bytes as a cursor bystander, invisible to every counter
  the project prints (F8111).
- **The operator-definition position, the file's `static` helpers, and its
  declaration order are all TU-scoped**, not symbol-scoped (F7815, F7940,
  Lever 4).

**So brief a pass on a FILE and require it to score the whole file both
directions**, rather than on a list of symbols drawn from across the tree. A
symbol-drawn worklist makes every one of the effects above look like noise; a
file-drawn one makes them the signal.

**BUT SAY WHICH UNIT YOU MEAN, BECAUSE THE ANSWER DIFFERS BY WHAT YOU ARE
ASKING FOR (F8249).** The first pass deliberately scoped this way came back
with three scopes, not one:

- **The translation unit is the right unit for COST.** One container pass per
  file; that pass ran 58 cells in less wall time than a single `make phase`.
- **The INLINED HELPER is the unit for every mechanism that actually PAID.**
  `fse_rotate` has no symbol of its own and carries 196 bytes across two
  symbols; `fse_quality`'s single `&&` chain produces the same five-term
  signature in **six** symbols' censuses -- six rows of a symbol-drawn
  worklist, one construct. This is F7940 arriving from the other direction:
  the helper is invisible to a per-symbol view AND is where the defect lives.
- **A shape repeated ACROSS files is the largest thing left over.** The object
  computes `widx = next < limit ? next : 0` branchlessly in three
  `SMCv32_encoder_*` and in `TxNoCarrierV32` -- 34 bytes, five sites, two
  files -- which no single-file scope contains.

So: enumerate at the file for cost, hunt at the helper for yield, and expect
the residue to be cross-file. A brief that just says "work the TU" gets the
cheapest of the three and misses the other two.

The exception is a symbol big enough to be its own unit: a 3,000-byte function
is a file's worth of work on its own.

## The levers, in the order they have paid off

Levers 0 to 9 came out of the refinement waves, in that order. **10, 11 and 12
did not: they were learned once somewhere in the older record, written into one
finding and never generalised**, and were swept up afterwards (7813). Their
measurements are as real as the rest and their yield in a refinement pass is
unknown, which is the one thing to hold in mind when a brief quotes them.

**17 and 18 are the newest and their scope is stated with them**: both come
from one 116-byte V.8 function (2026-10-19), 17's domain is two cells and 18's
is four, and neither has been swept across the tree. 17 did convert a SIZE
function to grade-1 ACCEPT, so it is worth trying first on any small
pointer-walk loop whose size is short by a few instructions.

**14 carries a measured null as well as a success:** the `V34.c` merge was
bit-identical and recovered no call boundary. That does not exclude emission
or data-layout effects in other partitions. Read the complete source and
linkage evidence before planning a TU move.

**13 has the same caveat as 10 to 12, and it is stated here rather than
discovered later.** It closed 74 instructions in ONE function (F7940) and has
never been swept across the tree, so its yield in a refinement pass is
unknown -- exactly the thing the paragraph above says to hold in mind. The
MECHANISM is measured and the four-compile ladder is real; the GENERALITY is
not established. It is also what corrected lever 6's recorded negative, so
read the two together.

### Lever 0. What an enumeration proves depends on how many cells hit zero

Complete a declared finite enumeration before claiming a unique preimage or
excluding its domain. Full-function acceptance does not require uniqueness
across equivalent source spellings. Report the tested cells, remaining cells
and which outcome you have, per function (7789 does it per closure):

- **A unique preimage within the declared family** — exactly one cell maps
  onto the object. That identifies an order under the tested source and
  compiler controls. `V90MP`: 4! = 24 cells, one zero, nearest
  near-miss at 2.
- **Several preimages** — more than one cell reaches zero. You have decoded a
  shared source properties, and the finding must say which ones. A complete,
  natural, validated candidate can be retained without claiming its particular
  order is uniquely original.
  `enterPhase4` decodes only that the clear follows the deadline, because the
  `inPhase3` slot collides; `V90Modem` decodes a zero-test and "`dil` stored
  last"; `resetLinearMapping` decodes only the NEGATIVE, that a signed 16-bit
  local is excluded.
**AND A DELTA OF ZERO IS A SUM, NOT A STATEMENT ABOUT EITHER SIDE.**
`--near`'s `+0` says the two instruction counts are equal; it does NOT say
nothing is missing. `V90Parameters::setToDefault` sat at a clean 695 against
695 and that total was **three differences cancelling** — x87 −18, integer
stores +19, clamping −1 — with the real cause six fields the header typed
`int` that are `float` (F7960). **Bucket by instruction FORM before trusting
the total**: count the `mov`-class stores, the x87 ops and the branches
separately on each side, and if those disagree while the total is zero, lever 2
does apply and is cancelling with itself.

Where the sub-counts also agree, the bucket projection agrees. The cause is
still open: different predicates, dependencies or value/member associations
can have equal counts. Use the lens to nominate operand and control-flow
comparisons on a **SIZE** symbol, rather than to close its source structure.

**TWO CENSUSES EXIST AND THEY USE DIFFERENT TAXONOMIES; RECONCILE THEM BEFORE
QUOTING EITHER.** F7986 bucketed 29 rows of the V.90 cluster and found **22
with every bucket agreeing**; F8000 censused 47 rows of the DSP/V.34 span and
found the strictest class -- texts equal as a multiset, order differs -- true
of **three**. They do not disagree. F7986's "every bucket agreeing" is
F8000's `OPERANDS` + `PERMUTATION` (mnemonic multiset equal), which is 27 of
47 there against 22 of 29 here. **The rate varies by span** -- roughly
three-quarters in the V.90 cluster, a little over half in DSP/V.34 -- so
neither number is the tree's, and the census is worth re-running per worklist
rather than inherited.

Bucketing measured on one cluster: of 29 reachable `+0` rows, **22 have every
bucket agreeing**, historically labelled encoding-only, and **7 cancel**.
The equal-bucket observation alone does not establish that label. None of the
seven had an x87 imbalance, which is the bucket that carried F7960's wrong
field types, so that defect did not repeat there (F7986). Note what that
leaves: for five of the seven the cancelling was bucketed but not TRACED, so
lever 2 is **bounded, not excluded**, on them.

**AND THE LENS DOES NOT BOUND THE MECHANISM — IT IS A WORKLIST, NOT A
DIAGNOSIS.** A `+0` row can be delta 0 by coincidence, and a symbol can LEAVE
delta 0 by being fixed. Both happened in one cluster: `V92Modem::progress` was
`+0` only by accident of two errors, and `V90Modem::progress` went from `+0` to
`-4` as its defect was corrected (F7983, F7982). The mechanism behind both --
sibling-call eligibility -- CHANGES the instruction count, so a worklist drawn
on `|delta|` would have dropped either had it been three instructions
differently wrong. Use `--near` to find cheap work, never to decide what a
symbol's defect can be.

- **No preimage** — **but FIRST ask whether the domain was drawn around the
  right code.** A no-preimage result excludes the tested family under its
  controls; check that the family covers the relevant helpers.
  `packData`'s sixteen-cell domain covered
  its own four loops and was genuinely exhausted; the CRC lived in a `static`
  helper whose two loops were never in it, and the answer was there (F7940). A
  `static` helper that gets inlined has no symbol of its own, so a per-function
  enumeration cannot see it. **Enumerate over the callee set, not over the
  function that carries the symbol.** Only once the boundary is right does a
  no-preimage result exclude the tested source family under that profile.
  Other source properties and interactions remain open. Four measured this
  way in one pass alone
  (7795), and `SpectralShaper::reset` at 10 cells with none reaching zero is
  the cleanest: those ten statement-order cells do not recover it.

**Enumerate before reading any cell.** Two passes have recorded doing that
explicitly, because stopping at a tempting near-miss is how an exhausted
enumeration turns back into a search.

### Lever 1. Statement order — enumerate, do not search

The single most productive lever: ten of the closures across four passes.
**Lever 17 is this same question one level in** — the order of the two
statements inside a loop body — and it is worth checking first when a small
pointer-walk loop is short by a handful of instructions.

When a difference is a statement order, the candidate source spellings form a
small finite family. **Compile all of them.** If exactly one maps onto the
object, the object's emission has a unique preimage within that family. Keep
the source representation and compiler profile in the claim.

    resetBeforRRN     2 orders compiled, 1 matches   -> EXACT   (7770)
    externalReset     all 3! compiled, 6 distinct emissions, unique preimage -> EXACT (7779)
    enterRepeatedCP   all 6 positions of one store   -> EXACT   (7770)

This holds **even when the compiler reorders your source**. An earlier rule
(7766) said to act only where our emission *is* our source, because then the
object's emission is the author's; that rule is sound and too strong. In all
three of 7770's closures GCC reordered our source and the order was still
recoverable, because the domain was exhausted rather than trusted.

**The branch that would kill it, and it must be excluded by measurement:** if
the compiler emits the same order for *every* source spelling, the map is
constant within that family. If its output differs from the reference, those
order changes cannot recover it under the fixed controls. This does not
exclude a source/flag interaction or another source representation.

**Where it stops.** Hill-climbing on byte count is not evidence:

    V92Phase4Modulator::reset   14 spellings, NONE emits the object's store
                                order; best was 27 of 290 -> DECLINED (7771)

Closer bytes are not a grade. Finding F7782 rejects near-match selection as
recovery evidence. A complete supported match can be retained even if several
natural spellings match; state the shared properties and the limits of any
original-order claim. Record the domain and actual compilation count. A finding
that says "closed by reordering" without that evidence is not reviewable.


If a reconstruction transcribes stores in disassembly order, that order is
the desired emitted sequence, not evidence of the author's statement order.
Keep the unchanged source as the baseline control and test complete emitted
bodies, including operands and relocations. In all four of F7805's closures,
the source already followed the desired emitted order but the compiler
permuted it. An enumeration searches for a source preimage under the declared
controls; matching a handwritten order alone is not a byte-identity result.

**AND THE HARNESS IS WHAT MAKES AN EXHAUSTED DOMAIN AFFORDABLE**, which is
what turns rule 0 from advice into something a pass can budget for. Compile the
REAL translation unit rather than a model of it, one container pass over every
variant, and score each object with **`byteident.py`'s own `body` and
`verdict`** — never a second implementation of the comparison, or the number
printed per cell can disagree with the number the tree-wide tool prints (7773's
rule). Measured: **0.03 s a cell for a 500-line file and 0.25 s for a
1,500-line one** (7805). Domains that bought at those rates: 720 cells for a
unique preimage (7806) and 5,151 for a measured NO preimage (7809) — which
file was which size is in those findings, not multiplied out here.

For a large class a stand-alone MODEL is faster still — 40,320 cells in twelve
minutes for an 8! constructor domain, 6,624 distinct emissions and thirteen
preimages (7807) — **and a model must be validated before it is believed.**
That one was checked to reproduce our committed object's thirteen instructions
exactly, from our committed source order, before a single cell of it was read.
A model nobody has seen agree with the real compiler is `gates.md`'s dead
detector with 40,320 rows of output.

### Lever 2. Instruction count at EQUAL byte size can expose a missing or extra statement

An instruction-count difference is a lead, not proof of a source statement.
Register allocation can add a return-register move; inspect operands and RTL
before assigning a source cause (see the V.34 boolean-return study above).
Once the extra instruction is an actual store, this check finds things no
functional test can:

    V90Resampler (Pf) ctor   blob 69 insns, ours 68, both 271 bytes
                             -> a dead `timingHistoryIndex = 0` that reset()
                                overwrites three statements later (7774)

A dead store is invisible to every differential test by construction. This is
the only lever that finds one. Check it on every function in a batch.

**AND THE DELTA RUNS BOTH WAYS -- MOST OF THIS TREE'S ARE EXTRAS, NOT
ABSENCES.** Lever 2's worked example is a MISSING statement, and that shape has
been over-read since. Measured over one cluster: three of its four real deltas
were **extra code in ours**, not absences (7823) -- `calcMtoMatchKtarget` ours
71 against blob 67, `updateUref` 66 against 64, `unitePhasesInfoOfUref` 203
against 202. Only `SpectralShaper::process` was an absence. Read the sign
before reaching for "what statement is missing", and note that
`instrcount.py`'s columns are `ours, blob, delta`.

**THE PRECONDITION: STRIP ALIGNMENT PADDING FIRST, WHEREVER IT OCCURS.**
`instrcount.py` counted intra-function padding as code and **inverted the
triage of five functions** (7793). `printErrorHistogramAndReset` read +12 and
is EQUAL; `getAT_UD` read +10 and the blob has one instruction MORE than us;
`process` read -1 and is -3. Two of the five read as "structural, decline it"
and were actually this lever's absence shape. It now imports `byteident.py`'s
own `_padding` predicate, so there is one definition of what padding is.

A trailing-only filter is not enough -- it invented one absence and hid a
real one. And counts quoted in older findings will not reproduce: 7778's three
declines were re-measured, two survive exactly and `calcMtoMatchKtarget` loses
one of its five to padding, so its verdict stands with the number read as
four.

**AND `--why`'s BARE `False` DOES NOT ROUTE HERE RELIABLY, because it is the
one place the padding filter is missing (7823).** `alpha_why` opens with
`len(x) != len(y)` over `insns()` rows and `insns()` does not strip padding, so
a bare `False` conflates "a statement is missing" with "the two carry different
numbers of alignment nops". `printErrorHistogramAndReset` prints `False` and is
**EQUAL on code**, 86 against 86, with 2 nops against 14. **Confirm every bare
`False` against `instrcount.py` before spending lever 2 on it** -- and read its
columns as `ours, blob, delta` with the delta OURS MINUS BLOB, because reading
it the other way inverts the diagnosis. Three of this cluster's four real
deltas are EXTRAS in our code, not absences, which is the opposite of the
lever's worked example and wants a different search.

**AND `--why`'s PADDING-STRIPPED COUNTS ARE THEMSELVES WRONG WHEREVER THE
FUNCTION CONTAINS A `mov %reg,%reg` (7848).** `byteident._padding` knows `nop`
and the `lea 0x0(...)` forms and **does not know the two-byte self-move**, which
is what GCC emits to align a loop head inside a function; `instrcount.py` does,
through its own `_SELFMOV` regex. So on `unitePhasesInfoOfUref` `--why` prints
**"204 against 204 with alignment padding stripped"** -- EQUAL, lever 2 does not
apply -- while the true code counts are blob 202 against ours 203, a real
**+1 EXTRA**. The blob has two self-moves there and we have one, so the error
does not even cancel. **`instrcount.py` is the authority for the count and
`--why`'s parenthesis is not**; where the two disagree, disassemble and count.
Fixing `_padding` would move grade 1 as well as the message, so it has not been
done inside a refinement pass.

**AND A DELTA OF ZERO IS A SUM, NOT AN INVENTORY -- IT LICENSES "THE ABSENCES
AND THE EXTRAS CANCEL" AND NOT "NOTHING IS MISSING" (F8000).** `--near`'s
delta-0 head reads as "same instruction count, so only the encoding differs",
and over a 47-symbol worklist that was true of **three**. Bucket each side by
instruction FORM before trusting the total:

    OPERANDS     24   mnemonic multiset EQUAL, texts differ
    CODE         20   mnemonic multiset DIFFERS while the total is zero
    PERMUTATION   3   texts equal as a multiset, order differs

`Scrambler<i,h>::process(bulk)` is `addl-1 dec-2 inc+1 jb+2 je-1 jmp+1 jne-1
mov-1 movzbl+1 xor+1` -- ten non-zero terms summing to zero -- and closing it
meant disbelieving the framing. `V90Parameters::setToDefault` is the same shape
independently: 695 against 695 was x87 -18, integer +19, clamping -1, and the
cause was six fields typed `int` that are `float`. So run the three-way census
(`Counter(text)`, then `Counter(mnemonic)`, padding AND self-moves stripped)
and read the per-mnemonic deltas; `movswl` against `movzwl` in unequal numbers
is lever 8 hiding inside a `+0` row. The framing pays on a SIZE symbol, where
it turns an apparently structural difference into an arithmetic one; on a BYTES
symbol it is close to redundant with the bucket you already have.

**A SECOND OBSERVABLE, INDEPENDENT OF THE BYTE GRADE:** the order of `.rodata`
strings a function references. It agrees or disagrees without reference to any
instruction, so it corroborates a statement-order decoding that the byte grade
alone cannot distinguish (7792).

**AND A TABLE THAT SEPARATES.** Where a function has two candidate differences,
compile the CROSS PRODUCT rather than one at a time: a cell that changes one
difference and not the other proves the two are independent, which no single
comparison can (7792).

### Lever 3. Definition order in the translation unit

Two character-identical bodies in one TU compile to **different bytes**, and it
follows the position, not the text:

    whichever body comes FIRST gets %ecx; the second gets %eax   (7772)

Swapping the definitions moves the difference with the slot. It is **not**
`-frename-registers` — compiled with and without, same result (7772). The
cause is now established and is at the end of this lever (7812).

Cashed once: `nm -n` showed the blob emitted `V90Resampler`'s `(f)` constructor
pair before its `(Pf)` pair and we emitted them the other way round, and in
*both* objects it was the first-emitted clone pair that diverged from itself.
Swapping made all four constructors byte-exact, 12 symbols for 12 (7774).

**THIS ENTRY SAID THE OPPOSITE UNTIL 7796 MEASURED IT, AND THE CORRECTION IS
THE MOST USEFUL THING IN IT.** 7777's control 4 moved ONE plain function,
measured no change, and 7774 was narrowed to clone pairs on that basis. The
control's observation is right and the conclusion drawn from it was wrong: a
single move usually does nothing. Reordering nine whole files to the blob's
emission order gained **17 symbols and lost none** (7796); five more files
gained **5 and lost 1** (7801).

**THE SHAPE DOES NOT PREDICT THE YIELD. THE BUCKET DOES.** Three passes have
now scored every shape at both zero and non-zero:

    7774   clone-only     the four V90Resampler constructors, 12 for 12
    7796   plain 16, twin 1, CLONE 0    over nine files
    7801   plain 2,  twin 1, CLONE 2    over five files

7796's clone column is the one to distrust, and it is why: `ModulusCoder`'s
four constructor clones were already in the blob's order and stayed at 25
differing bytes, `V90CP`'s C2 went EXACT to REGALLOC, `V90Phase4Modulator`'s
C1/C2 merely swapped their 21 and 27 -- three files' worth of clones, all in
files that also had gaps or were reverted. 7801 moved all three `Resampler`
destructors to byte identity in one file. **Do not skip a candidate for its
shape; skip it for its bucket.**

**The carrier is upstream of the function, not its own index.** Four REGALLOC
symbols already sat at the blob's emission index, so only their PREDECESSORS
could change -- and three of the four closed without moving. That is 7772's
"something ahead of both in the TU", now named. 7772's own twin
`recivedSUV` closed with neither twin moving relative to the other.

**Aim at REGALLOC files, not BYTES files, and this is now the twice-confirmed
part.** Over 7796's nine files, **16 of 25 REGALLOC candidates closed, 0 of 9
BYTES**, and the one file aimed at a BYTES bucket paid nothing and lost a
symbol. Over 7801's five, **5 of 7 REGALLOC closed and the BYTES and SIZE
counts did not move by ONE symbol tree-wide** -- every gain and the one loss
was a REGALLOC/EXACT swap.

Re-measured at 7801: of the 165 objects sharing a `.text` symbol with the blob,
**76 already emit in the blob's order and 89 do not**, and the 89 hold **19 of
the remaining grade 1 and 63 of the BYTES**. That does not reach the tree's 30
and 94 because a COMDAT function is in its own `.gnu.linkonce.t.*` section and
is outside this ordering entirely: 3 grade 1 and 6 BYTES live there. Two
regions are not reachable by definition order at all: that head, where
templates and clones interleave, and the cgraph tail.

### Lever 3a. The pre-check, before you permute anything (7801)

Three questions, all answered from the objects, and one of them stops a file
being permuted into a null nobody can read.

1. **Is the blob's order REACHABLE?** The emission model forces a callee ahead
   of its caller, so the order is reachable only if no call edge of ours runs
   caller-first in it. Read the edges off `objdump -dr` and **bound each
   function by its `nm -S` size**: alignment padding between functions is spelt
   `jmp <next symbol>` plus nops and reads as a call. That artefact invented
   `_iir_filter_delete -> _iir_filter_progress` and would have condemned a file
   that then gained. Check the detector against your OWN emission order -- an
   edge reading caller-first in your object is an artefact, because the same
   rule produced it.
   **It is a filter for the unreachable case, NOT a certificate.** It reads the
   final object and cannot see an INLINED call, and GCC 3.4 keeps the cgraph
   edge after inlining. `Resampler.cpp` scans as zero edges and still emits
   `reset` ahead of both constructor pairs. The only proof is the achieved
   order after the rebuild.
2. **Is the blob's span YOURS?** List the blob symbols whose address falls
   between the file's first and last and check they are all yours.
   `v34pcmif.c` is 34 of 57 -- the other 23 are in four other files of ours, or
   unwritten -- so only the RELATIVE order is recoverable there. It gained two
   anyway: a gap is a reason to discount a file's SILENCE, not to skip it.
3. **Is a datum or a macro in the way?** A file-scope table between two
   functions being swapped is lever 4's own effect. Hoist it in a SEPARATE
   commit and measure that alone. `toneiir.c`'s two `.rodata` blocks, two
   `#define`s and two file-local statics were hoisted and measured by
   themselves: every symbol identical, to the differing byte. Data moved as a
   unit keeps its own relative order, so lever 4 does not fire.

**An `#if` inside a body is safe when it BALANCES.** 7796's rule -- refuse to
move any chunk containing a `#` line -- is over-strict and freezes
`V34TimingFiltersInit`, whose `#ifdef DSPLIB_REPRODUCE_BUGS ... #endif` is
wholly inside the body. The invariant that stops the wave 5 trap is the
balance: `#if` +1, `#endif` -1, never negative, zero at the end. A bare
`#define` or `#include` still refuses.

**INDEX-FOR-INDEX AGREEMENT IS NOT SUFFICIENT.** `V34EqualizerCleanUp` sits at
the blob's index 16 of 26, in a file with no gaps, no unwritten neighbours and
all 26 symbols in place, with the blob's own predecessor ahead of it -- and it
went EXACT to grade 1. Achieving the order buys you the right to believe a null;
it does not buy byte identity.

**A FILE THAT DID NOT CLOSE MAY STILL HAVE MADE ITS RESIDUAL LEGIBLE.**
`toneiir_reset` went from 22 differing bytes of 60 to ONE, and that one is a
`movzwl` where we emit `movswl` -- lever 8, a named declaration defect that 22
bytes of register noise had been hiding (7802). Read the (b) rows, not only
the (a) ones.

**And name a check the reader can run.** 7796 could say "the `.text+0x...`
comments run upwards down the file" because both Phase4Modulator files carry
one per function. Most files carry none, so the portable check -- and the one
in 7801's five headers -- is `nm -n --defined-only` on `build/tc_out/<file>.o`
against the blob over the symbols both define, with today's score written in.

**THAT RULE IS ABOUT PICKING A FILE, AND WAVE 6 DID NOT TEST IT -- WHAT IT
CORRECTS IS HOW YOU COUNT THE YIELD AFTERWARDS.** Four files kept, **7 gained
and 0 lost**, and the buckets settle where they came from: REGALLOC 34 -> 29
and BYTES 94 -> 92, so **5 are REGALLOC and all five were targets, and the 2
BYTES closures were bystanders nobody aimed at** (`V90Parameters::loadParams`,
`V90Equalizer::enterChannelVerification`). Five of the ten targets closed, so a
ledger counting only targets reads 5 against a real 7. Keep aiming whole files
at REGALLOC; count the whole set, both directions. This wave's own per-shape yield was
**plain 7, twin 0, clone 0** -- which looked like 7796 repeating until 7801
scored clone 2 on the next five files, so read it with the table above and not
as a pattern -- and the one file whose only
targets were a C1/C2 pair (`V90Demapper.cpp`) lost a symbol and was reverted.

**THE RULE IS ABOUT PRICE, NOT ABOUT NULLITY** -- read the next paragraph
that way. Two pure block permutations were kept in wave 9a
(`V90ConnectionEvaluator.cpp`, `V90SpectralShaper.cpp`): each moves its file
to the blob's own emission order, changes **not one byte anywhere in the
tree**, and costs nothing beyond the permutation -- no macro hoists, no
preprocessor risk. They are kept because the order reached is a durable
measured fact that stops the next wave re-deriving it, which is 7796's
kept-neutral precedent. What follows is the case where the price was real.

**A NULL RESULT CAN COST TOO MUCH TO KEEP.** `VpcmFloModem.cpp` reached 16 of
16 and gained nothing, and reverting it left the tree at 474 -- but reaching it
had taken twelve macro blocks hoisted on top of the permutation. 7796's kept
neutral files were reorder-only. The measurement is the deliverable: record
that the order is achievable and pays nothing, and do not keep the diff.
Findings F7797 and F7798.

**A FILE-SCOPE `static` THAT CALLS A MEMBER FUNCTION SETS THAT MEMBER'S
EMISSION SLOT, so it is the exception to "statics live above".** Wave 6's
`V90AutoDigitalImpDetector.cpp` stopped at 23 of 34 with `isAltRbs` emitted at
index 0 against the blob's 10; the cause was `adid_recheckAltRbs`, a helper
THIS RECONSTRUCTION introduced, sitting at the top of the file with
`o->isAltRbs(...)` in its body. Moving that one helper below its own first user
took the file to 34 of 34. The object inlines the call, so the original had no
such edge -- our factoring was setting the order. Check for it whenever a
reorder lands short by a single symbol sitting at index 0. Finding F7798.

**Two traps, both hit while doing it.** An `#endif` travelled with a moved
chunk twice and STILL COMPILED, silently enlarging an
`#if __SIZEOF_POINTER__ == 4` region over live code -- `compilers.md`'s V3,
which fails open. And a macro placed beside its first user ends up below it
after a move. Rule now in the files: macros and file-scope statics live ABOVE
the definitions. Any tool doing this must refuse to write unless the line
multiset is unchanged, which is what proves a permutation is a permutation.

**AND THE MECHANICAL CHECK FOR BOTH IS ONE THING, WITH TWO WAYS OF BEING
DEAD.** Compare the SORTED MULTISET of preprocessed non-blank lines against
`HEAD`, **under both `-D__SIZEOF_POINTER__=4` and `-D__SIZEOF_POINTER__=8`**:

- Run the period arm alone and it does not fire at all. An injected `#endif`
  moved 40 lines down `V90Equalizer.cpp` passed, exit 0 -- enlarging a region
  whose guard is TRUE swallows live code without deleting a line. V3 is that
  the predefine is absent under 3.4.2, so **only the FALSE arm shows it**, and
  the real build uses the other one.
- Compare COUNTS rather than the multiset and it misses the macro trap
  entirely: three of wave 6's files came out with a macro below its first user
  at 1781 preprocessed lines against 1781. An identifier used before its
  `#define` is not expanded, so the line survives with different text -- and
  that is not always a compile error, so wave 5's loud case is not the general
  shape.

Shown firing on both injections and clean on everything committed. Finding F7799.

**A COROLLARY THE MECHANISM MAKES OBVIOUS AND WHICH WAS MEASURED SEPARATELY:
the lever cannot reach a symbol at emission index 0** (7808). The cursor is
threaded through the TU in emission order, so a symbol emitted FIRST has
nothing ahead of it to have moved the cursor. `V90Phase4Demodulator`'s C1/C2
sit at index 0 with nothing above them but `typedef char` assertions; all six
orderings of the file's bottom blocks were compiled, **two of them achieve the
blob's `nm -n` order exactly, 15 for 15, reorder-only**, and the pair stays at
53 differing bytes in every one. Check the target's emission index before
spending a reorder on it -- and prefer the `-fno-peephole2` certificate below,
which is stronger because it answers for the symbol rather than its position.

**THE CHEAP SCREEN FOR THIS LEVER IS WRONG, AND TWO PASSES FOUND IT WRONG
INDEPENDENTLY (F8042, F8067).** F8003 retired lever 3 for 21 of 22 files on the
test "does this file store a constant past `+0x7f`", the idea being that if
`i386.md:17507` never fires then no scratch is consumed. **Both halves fail:**

- **It is not the only `match_scratch` consumer.** The `add $imm,%esp` to
  `pop %reg` epilogue conversion takes one too, and the OBJECT ITSELF carries
  that conversion in all three cosine windows -- their prologue and epilogue
  counts do not balance (F8042).
- **The implementation counted the wrong side.** Counting surviving
  `mov $imm,disp>0x7f` in the FINAL object counts the sites peephole2 LEFT
  ALONE, the inverse of the intent. `V92Modulator.cpp` scores zero on it while
  the exact test finds two EXPOSED symbols in it (F8067).

**So do not screen -- run the advance test.** Compile with and without
`-fno-peephole2` and compare bytes; it is two compiles and it is the authority.
No number from the `+0x7f` screen should be quoted, and **lever 3 is NOT
retired for any file on the strength of it.**

### Lever 3b. The mechanism, settled: a round-robin cursor in `peephole2` (7812)

**It is not the register allocator, and it is not a counter.** Swap two
definitions in `V90Phase4Modulator.cpp` and dump every RTL pass with `-da`:
the unmoved bystander `resetRRNSecondSection` is **identical through `.24.lreg`,
`.25.greg`, `.26.postreload` and `.27.flow2`** and first differs at
`.28.peephole2`. At `flow2` the two stores are still immediates straight to
memory with no register in them. `peephole2` is what puts a register there.

The pattern is `i386.md:17507` — a store of an immediate whose *encoding*
reaches `ix86_cost->large_insn` is split into `reg = imm; mem = reg`, and its
`match_scratch` is filled by `peep2_find_free_register` (`recog.c:2931`), whose
first line is

    static int search_ofs;

a round-robin cursor over `reg_alloc_order`. On success it is set to the
register after the one found (`recog.c:3018`); **nothing resets it per
function, per file or per pass** — all four references in 3.4.2 are inside that
one function. It is threaded through a translation unit in EMISSION order,
which is why the leaf block matters and the file's source order does not.

**Why a swap moves it while every counter stays put.** Each call is
`search_ofs' = f(search_ofs, the live set)`, advancing past whichever register
was free rather than by a fixed step, so composing two of them does not
commute. That is exactly 7796's measurement — `generateDataSymbolBeforeFPE` had
the identical label number (`.L212`), the identical instruction count and
different registers, and the same at `.L215` for its twin — read as a
mechanism instead of by elimination. **One discontinuity:** `recog.c:3024`
resets the cursor to 0 when no register can be found, so a function under
enough pressure to fail an allocation resynchronises everything after it.

**Three arms, each of which turns the pattern off, and each collapses the
effect to zero** (symbols compared only where address and name agree in both
objects, so nothing that moved is counted):

    the tree's flags   1 bystander differs      -fno-peephole2   0
    -mtune=i386        0                        -Os              0

`-mtune` is the one to read twice: `x86_split_long_moves = m_PPRO` (`i386.c:492`)
masked by the tune setting (`i386.h:263`), so **`-mtune=i686` — finding F612's
flag, the one that took the codegen match from 30 to 82 — is exactly and only
what enables this.**

**THE ADVANCE TEST, AND IT IS EXACT IN THE DIRECTION THAT MATTERS.** Compile the
file twice, with and without `-fno-peephole2`, and compare the candidate
function. Three outcomes and they are not the same claim:

    bytes IDENTICAL           peephole2 did nothing.  It consumed no scratch,
                              the cursor cannot reach it, reordering CANNOT
                              move this symbol.  A definitive clear.
    bytes differ, and a
    REGISTER appears that
    the -fno-peephole2 build
    never uses               a scratch was allocated.  Exposed, and you can
                              name the register.
    bytes differ, no new
    register                 peephole2 fired on a pattern that takes no
                              scratch (the xor-zeroing one, lea-to-add).
                              Undecided -- treat as exposed.

Run over the live buckets on this tree, one file compiled twice per candidate:

    REGALLOC  25 symbols   12 take a scratch   12 undecided    1 CLEARED
    BYTES     91 symbols   18 take a scratch   45 undecided   28 CLEARED

**That is 3a's missing certificate.** 3a can say a file's order is achievable
and cannot say a null result means anything; this says, per symbol and before
any permutation, that 1 of the remaining 25 REGALLOC symbols
(`V92Mapper::reset`) and 28 of the 91 BYTES are not reachable by this lever at
all. It also explains 7796 and 7801 after the fact: nearly every REGALLOC symbol
is peephole2-touched, which is why aiming whole files at that bucket paid and
aiming at BYTES did not.

**DO NOT RUN THE TWO COMPARES INSIDE THE PERIOD CONTAINER (7845).** The
obvious implementation is a shell loop next to the two `g++` calls, and
**binutils 2.15 has no `objdump --disassemble=SYM`**: both sides come out
empty, `cmp` calls them equal, and every symbol in every file reads CLEARED --
the direction that licenses skipping work, so nothing downstream questions it.
One run reported **24 of 24 CLEARED over two files**; the same script over
`V92Transmitter.cpp`, whose D1 residual IS a peephole2-allocated `pop`
register, called that CLEARED too, which is what exposed it. Disassemble on the
HOST, score through `byteident.py`'s own `body()`, and have the tool print how
many symbols it found EXPOSED -- a run with zero is a run to distrust, not a
clean file. Rebuilt that way the same four files read 25 of 45 exposed.

**AND IT HAS BEEN WATCHED FIRE IN BOTH DIRECTIONS, because a certificate that
licenses SKIPPING work is exactly the shape `gates.md` rule 3 exists for.**
Permuting the file every way and comparing only where the symbol did not itself
move — objdump renders branch targets absolutely, and that artefact reported
the certificate BROKEN on the first run of the check:

    resetRRNSecondSection   classified "takes a scratch (ecx, edi)"
                            7 of 28 permutations changed it        FIRES
    V92Mapper::reset,       all four classified CLEARED
    freqToNearestBin,       0 of 23 comparable permutations
    ModulusEncoder C2,      changed any of them                    HOLDS
    spectralDesign
    V90Equalizer::process   classified undecided, 0 of 28

**AND EXPOSED HAS A SECOND READING THE CERTIFICATE FRAMING HIDES (7827).** The
cursor is threaded in emission order and advances past whichever register each
split found free, so **changing what scratch a function consumes changes the
state that arrives at its SUCCESSOR** -- with no definition moving anywhere.
`Dtmf_Rx.c` was already 4 of 4 in the blob's `nm -n` order, so lever 3 had
nothing positional to offer; `reset_dtmf` (index 0, EXPOSED on `ecx`/`edx`)
was closed on its statement order alone and `create_cid_dtmf` (index 1,
EXPOSED on `ecx`/`edi`), which was not edited at all, went exact with it. The
file went 2 of 4 to 4 of 4 on one function's statement order. **7808's
corollary still holds -- a reorder cannot reach index 0 -- and it is about
POSITION, not about the symbol being outside the mechanism.** So: fix an
EXPOSED symbol EARLY in a file and re-measure the whole file before touching
anything below it, because its successors may have moved for free.

**EXPOSED IS NECESSARY AND NOT SUFFICIENT, and that is the part to hold on to.**
`V90CP`'s C1 takes three scratch registers and held over 21 comparable
permutations; `generateSymbol` held over 28. The cursor is a state machine and
most single swaps are no-ops for it — which is 7777's control 4 all over again,
and the reason 7796 had to permute whole files rather than pairs.

The twelve REGALLOC symbols that take a scratch, with the register peephole2
gave them, are the list a reordering pass should start from — `V90CP`'s C1
(`ecx`, `edx`, `edi`), both `printTitle`s, `V90Parameters`' C2, both
`setMappingParams`, `V90SignBitsExtractor`'s C2, `V34EqualizerCleanUp`,
`V90Resampler::setBllState`, `VPcmFloModem::setPcmSessionType`,
`applyPadGainToLinMapp`, `generateSymbol` and `dp_v8_exit`.

**WHY THE SHAPE, READ OFF THE OBJECT, IS NOT THE TEST.** `large_insn` is 8 for
`pentiumpro_cost` (`i386.c:251`), so `movl $imm32,disp8(%reg)` at seven bytes is
left alone and `movl $imm32,disp32(%reg)` at ten is split — visible in the
reproduction, where `+0x18`, `+0x20` and `+0x30` stay immediate stores and only
`+0x2f9c` and `+0x2fa0` split. Counting that emitted shape gives **218 of the
blob's 1,859 functions, 406 splits in all**, led by `v34handshak` at 19, and it
is a good quick read of the object. **It is also a bad predictor: over the 25
REGALLOC symbols it finds 4 where the exact test finds 12.** i386.md has 144
`match_scratch` sites and the read-modify-write group at 17685 is PPRO-tuned
too, drawing on the same cursor. Use the shape to understand what is happening;
use the two compiles to decide.

**How much of the effect this is, measured.** 177 of 184 swaps offered over the
47 C++ units in `src/pump/` with four or more definitions — the other 7 the
harness skipped, on a compile or a permutation that did not apply, and they are
counted on neither side: **16 bystanders differ with peephole2 and 8 without**,
and all
eight survivors are one instruction, `movl $imm,(%esp)`, with a different
`.rodata.str1.1` addend, which is a diagnostic string's pool offset moving with
the function order and CLAUDE.md's own trap rather than a register choice. All
sixteen were inspected. Over this sweep the cursor accounts for all of it.

**IT RETRODICTS 7772.** Two character-identical bodies each taking one scratch:
the first gets the register the cursor points at, the second the next one round
the ring. "Whichever body comes FIRST gets `%ecx`; the second gets `%eax`" is
that sentence.

**TWO DEAD HYPOTHESES. Do not re-try either.** The counters — `label_num`,
`DECL_UID`, insn UIDs — are at the same value in both compiles, and a swap
preserves them by construction. And allocation addresses: if they carried it,
changing when the collector runs would change the output, because `ggc-page`
frees pages that later allocations reuse. Five arms of `--param ggc-min-expand`
and `ggc-min-heapsize` give **one md5 for all five objects**, and the knob was
shown to fire first — `-fmem-report` reads 6040k of arena at the default
against 1520k at the aggressive setting, and the compile goes 0.131 s to
0.234 s collecting.

### Lever 4. File-scope declaration order

GCC 3.4.2 emits file-scope objects in **reverse definition order**. Verify that
on our own object before leaning on it — it was checked ten-for-ten first.

Matching the two `.rodata` blocks **by content** paired all ten of
`v8_V21_Init`'s tables with no array differing, which proved the argument
assignment was already right and the *definition order* was the defect. Ten of
thirteen bytes (7765).

**The rule holds on BOTH compilers, which is what makes it usable.** A
three-variable scratch file gives `CCC, BBB, AAA` ascending under GCC 13 `-m32
-O3` and under GCC 3.4.2 at the tree's flags, so one source order satisfies the
period build and the modern one; had they disagreed, D392 would have closed as
unachievable rather than as fixed. The corollary is worth knowing separately:
`short[4]` gets 2-byte alignment from 3.4.2 and 4 from GCC 13 while the `.data`
section is 4 in both, so **a two-byte pad between two arrays is a
translation-unit boundary** under the object's own compiler — which is how
`COEF_DC` and `FPM_sin_sign` were placed in different units from the bytes
rather than the symbol table (3622).

**WHERE IT STOPS, AND IT IS A DIFFERENT KIND OF SYMBOL. A function-local
`static` does not record its declaration order.** `V92CP::bitsToInfo`'s two
statics come out `delta` at `.bss+0x0` and `gamma` at `+0x4` **with the
declarations in EITHER order** — both arrangements were built on the period
compiler and the layout did not move, so the blob's own order is not evidence
about the original's source and cannot be reproduced by reordering (6610).
Nothing in the tree observes those offsets, so the temptation is to write the
comment anyway; two files already carry that claim and one of them is now known
to be unfounded. **Check which kind of symbol you have before leaning on
lever 4 at all**, and 3622's own closing paragraph is the model for how far a
headline of this shape is entitled to go: it is one scratch file plus one real
one, so nothing should be built on it that a test does not check.

### Lever 4a. The ctor/dtor VARIANT DIGIT distinguishes a base from a member at offset 0

A public base subobject and a member at offset `+0` have the same address and
the same field offsets, so a header can declare either and no offset assertion
can tell them apart. `V90PreFilter.h` said as much: "nothing in the blob
distinguishes" the two.

**Something does. The Itanium ABI's variant digit.** GCC emits `C2`/`D2` for a
BASE subobject's constructor and destructor and `C1`/`D1` for a complete
object, so a member's ctor call names `D1` where an inherited base's names
`D2` — and the blob picks the base ones (F8080). That is a forced difference:
no spelling of a member produces `D2`.

Closed `V90PreFilter`'s D1 and D2 out of the RELOC bucket, which is the only
bucket where a differing relocation TARGET is the whole defect. A tree-wide
census, **shown to fire on the pre-fix tree by catching exactly the four known
sites**, then came back 0 of 1251 — and resolved `Psd` and `FloatIIR`'s
identical open notes in the MEMBER direction, which is the opposite answer and
equally forced.

Read the variant digit before deciding a base-versus-member question is
undecidable.

### Lever 5. Storage class, read off relocations

- A relocation against a **section** symbol (`.data`/`.rodata`) rather than a
  named one says the object was file-local: `static`.
- **`.data` rather than `.rodata`** says not `const`.
- Spacing between two tables says which came first.

All three at once on `RcFixed_Check_Combination` (7767). `nm` showed ours as
`R` before and `d` after.

**Where it stops:** `b103_ops` and `v23_ops` were established file-local the
same way, and adding `static` moved the relocations but **not the register
choice**, so neither reached grade 0 by it (7768).

### Lever 6. Unrolled and partially-unrolled code

The object is frequently more unrolled than the natural source. When you write
source in an unrolled or partially-unrolled shape to match it, **put the
rolled version in a comment above it.** The unrolled form is what the compiler
needs; the loop is what a reader needs, and without it the next person cannot
tell an intentional expansion from a transcription accident.

    /*
     * The object writes these out; the source it came from may not have.
     * Rolled, this is:
     *     for (i = 0; i <= 16; i++)
     *             bits[i] = 1;
     */
    bits[0] = 1;
    bits[1] = 1;
    ...

**Watch for Duff's Device and its relatives.** They produce a signature that
is easy to misread: partial unrolling with loop control still present, which
looks like "the compiler unrolled it" and is actually the source's own shape.
The tell is an indirect jump whose case labels land INSIDE a loop body and
fall through, rather than a plain switch whose cases do not. 79 blob functions
carry an indirect jump, all `jmp *@.rodata(,%eax,4)`; none has been checked
for this (7785).

Two negatives already recorded, so nobody repeats them:

- **`-funroll-loops` is not a missing period flag.** Whole tree rebuilt with
  it: 0 gained, 39 lost, grade 0 433 -> 394 (7783).
- **`packData`'s difference is not loop shape** -- all 16 combinations of which
  of its four loops are written out were compiled; the maximum is 516 against
  the object's 534 (7785). **AND THAT EXHAUSTED DOMAIN WAS DRAWN AROUND THE
  WRONG FUNCTION.** The CRC lived in a static helper, and the helper's own two
  loops were never among the sixteen. Writing ONE of them out -- the
  fifteen-element shift -- took it from 75 instructions to 141 of the object's
  149, and lever 13 took it the rest of the way (F7940). So the reading "the
  domain is exhausted, this is lever 1's constant map" was wrong, and the
  lesson is the one lever 1 already states: **enumerate over the whole callee
  set, not over the function that carries the symbol.** A `static` helper that
  gets inlined has no symbol of its own and is invisible to a per-function
  enumeration.

**AND CHECK THE CONVERSE BEFORE YOU WRITE THE EXPANDED SHAPE AT ALL: the
duplication may be the compiler's.** Thirteen hand-written convolutions were
about to go in because the object has thirteen. One `static` helper taking the
tap count and the shift as parameters, called from a switch with six constant
pairs, on the period compiler:

    static, -O2          1 copy, runtime shift, helper stays out of line
    static inline, -O2   6 copies, shifts $0xd $0xe $0xf $0x10
    static, -O3          6 copies, the same four shifts

which are the object's own four shift amounts (611). **But the compiler can
only do that if the source hands it constants** — reading the tap count out of
a state struct leaves nothing to fold, and `-O3` then takes our function from
454 bytes to 463 rather than towards 2,640. So the finding is not "write one
loop"; it is that the original's hand optimisation was expressed as literals at
the call site, and the expansion is downstream of that.

**DUPLICATED LOOP BODIES ARE ALSO WHAT UNSWITCHING LOOKS LIKE, AND IT IS NOT
UNROLLING.** `v8_fskmodulate`'s object has a `test`/`je` at the top and then
*two* four-iteration loops with back edges -- one per value of the
loop-invariant `which` parameter -- and the earlier pass read that as "the
blob unrolls its 4-iteration loop" (F11381). It is `-funswitch-loops` (in
`-O3`), and the tell is that each copy keeps its `jle` back edge and the
bodies differ only in the invariant operand (`0x4(%ebx)` vs `0x2(%ebx)`). The
source that produces it puts the branch *inside* the loop; hoisting the
selection to a `step` local before the loop leaves nothing to unswitch and
emits one shared body. Enumerated over {`int`/`short`} x {indexed/advancing
pointer} x {two ternary polarities / if-else}, exactly two spellings map
byte-exactly (F11381). `-funroll-loops` and `-funroll-all-loops` do **not**
reproduce this shape and cost the tree 83-113 grade-0 exacts whole-tree; the
lever-6 negative for `-funroll-loops` stands and now has a second, independent
measurement beside it.

### Lever 7. `delete[]` versus an explicit guarded free

**The blob's global `operator delete` IS `sysdep_free`** -- it contains no
`_Znwj`, `_ZdlPv` or `_ZdaPv` at all, and its compiler-generated `D0Ev`
destructors tail-call `sysdep_free` exactly where a library `operator delete`
would call `_ZdlPv`.

The consequence is a one-instruction difference at the same byte count. At a
destructor's LAST free the object makes a plain `call sysdep_free`; our
explicit `if (p != 0) sysdep_free(p)` makes a sibling `jmp`. Eight spellings
were compiled -- guard, braces, implicit `!= 0`, trailing `return`, ternary,
short-circuit `&&`, if/else, an inlined helper carrying the guard, and scalar
`delete` -- and **all of them sibcall. Only `delete[]` does not** (7786).

Nine destructors closed on it in wave 4b, four of which were in the SIZE
bucket and nobody was looking at; **fifteen more in wave 7** (7814), taking
grade 0 from 479 to 494.

**+1 INSTRUCTION AT THE SAME BYTE COUNT IS THE SPECIAL CASE, NOT THE RULE, and
screening on it would have missed nine of wave 7's fifteen.** The sibcall does
not only swap `call`+`ret` for `jmp` -- **it deletes the frame**. Where the
destructor's only work is the free, the `sub`/`add` pair and the second `ret`
go with it:

    Scrambler<h,h>::~Scrambler   ours 7 insns / 25 bytes
                                 blob 11 insns / 29 bytes    -> EXACT

So a site is a candidate at **either** delta: +4/+4 where the free is the whole
body, +1/+0 where the destructor needs the frame anyway (`V90Equalizer` 112 vs
113 at 541 bytes both, `V90CP` 43 vs 44 at 173 both).

**THE ARITHMETIC IS ALSO THE STOPPING RULE.** Compute what the lever costs at a
site where it is confirmed, and decline anything that does not reconcile. Six
C++ destructors were declined that way in wave 7: a 4-instruction delta with a
**31-byte** gap is not this lever (`V92Precoder`, `V92PreFilter`), and
`V90SpectralVerifier` at EQUAL instruction count while the sibcall flag
disagrees is internally impossible for it. `delete[]` there would be a fit and
7782 draws that line.

**THE ENUMERATION IS NINE SPELLINGS, AND THE TWO THAT DO NOT SIBCALL ARE BOTH
DELETE-EXPRESSIONS (7816).** 7786's spelling C was `delete p` on a **POD**,
which sibcalls. On a pointer to a class **with a destructor** the same syntax
is a different construct --

    delete p    ==>    if (p) { p->~T(); operator delete(p); }

-- and GCC 3.4.2 does not sibcall that call either. So scalar `delete` sits in
BOTH columns depending on what it deletes, and **what suppresses the tail call
is the EXPRESSION, not the type**.

That closed ten more symbols over five files. **Anywhere this tree open-codes
`p->~T(); sysdep_free(p);` is a candidate, and the search is one grep** --
`V92Precoder` went from 25 instructions in 77 bytes to the blob's 29 in 108 in
a single compile, and the 31-byte gap that had been read as "register pressure"
was the expansion missing entirely.

**THE MODERN BUILD NEEDS ITS SIZED-DEALLOCATION DEMAND WITHDRAWN.**
C++14 sized deallocation makes modern GCC call `operator delete(void *, size_t)`
for class deletion, while the period compiler uses the unsized form. The old
wave added guarded sized adapters in reconstruction source (7816); F7900
removed those apparatus blocks and recovered all200 period objects unchanged
with the modern-only `-fno-sized-deallocation` flag. Use that existing flag,
not new `__cplusplus` source shims. Preserve unsized inline adapter placement:
period-visible definitions can carry code-generation effects (7815).

**BUT A DELETE-EXPRESSION RUNS A DESTRUCTOR AND AN EXPLICIT FREE DOES NOT**, so
take it only where the OBJECT ITSELF makes the destructor call.
`V92Transmitter::modulusEncoder` is freed by the blob with no such call and was
declined for exactly that: `delete` would invent one, and nothing may add a
call the object does not make.

**MEASURE IT PAIRWISE, WITH `tools/sibcensus.py`.** A raw population ratio
mixes "the blob does not sibcall here" with "the blob has no such function".
Over the 1251 symbols both objects define, wave 7 re-derived **1207 agree, 36
where we sibcall and the object does not (32 to `sysdep_free`), 8 the other
way** -- against 7786's 1188 / 50 / 13, which is wave 4b's nine closures plus a
tool artefact: **an indirect `jmp *TABLE(,%eax,4)` is a switch and carries an
`R_386_32` relocation against `.rodata` exactly as a relocated tail call
carries one against its callee.** Seven switches were being counted as sibling
calls. Bound each function by its `nm -S` size too, or the `jmp <next symbol>`
GCC pads with reads as one -- lever 3a's artefact in another costume.

Three hypotheses are dead and should not be re-tried: the blob tail-calls
`sysdep_free` 67 times elsewhere, so it is not a declaration property; the
disagreement runs both ways, so it is not a flag; and **all eight of the other
direction are ABSENCES** -- 7 to 97 instructions of missing body, and in six of
them the blob's tail-callee is not our last statement at all (7817).

**`delete[]` IS NOT AUTOMATICALLY RIGHT, and the array test must not be our own
allocation.** Deriving "it is an array" from our `sysdep_malloc(n * sizeof(T))`
is the reconstruction arguing for itself. The non-circular witness is that the
class INDEXES the member -- `state_[i]`, `buf + size - 1`, `pLimit + c` -- which
the differential tier has already validated against the blob. And confirm the
pointee is a **POD**: on a pointer to a class with a destructor, `delete[]`
emits a destructor loop and reads an array cookie a malloc'd block does not
have, which is wrong behaviour and not merely wrong bytes. A `void *` member
cannot take a delete-expression at all.

**Only the LAST free is byte-evidence.** Away from tail position the two
spellings emit identically, so the other frees in a destructor carry `delete[]`
for uniformity, not because the object distinguishes them. Say so in the file.

**AND THE DEFINITION'S POSITION IN THE TU IS ITSELF A LEVER-3 CARRIER (7815).**
Hoisting the one inline `operator delete[]` into `dsplib/sysdep.h` -- which
every one of these files already reaches transitively -- **cost eight
destructors their byte identity, four of them wave 4b's**, while touching no
destructor and no free. The TU's declaration set was constant across the
experiment (`FloatIIR.cpp` reached `sysdep.h` under both arrangements and kept
its symbols); only the inline function's position moved. So each `.cpp` keeps
its own copy, `Scrambler.h` carries the one case that cannot (its destructor is
inline in the header), and every file reaching that header is forbidden its
own. "One type, one home" is about TYPES and undefined behaviour; this is an
inline function, the duplication is deliberate, and consolidating it needs the
SET diff re-run.

**A declined language-dependent spelling can reopen after independent TU
recovery.** F7818's V92 deletion restriction was based on the C reconstruction
then present. F11404 recovered V92MappingParamsInt.cpp; its current fields are
typed int*/float* arrays. F11615's bounded deletion cross now closes both
173B/106B bodies, leaving all three bystanders unchanged. F11616 crosses
consistent new[] allocations: all deletion-bearing cells raw-merge, so paired
array expressions are an idiomatic supported family, not unique allocation
spelling. Keep the TU-local unsized host adapter position and original sizes,
guards, dangling slots and call order; real allocator lifecycle and whole-TU
data/export controls decide adoption. No language/layout forcing was needed.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v92-array-lifecycle-recovery.md).

### Lever 8. Width and signedness — and the DESTINATION's declared type

7630's `movzwl` copied as 32 bits. Equal instruction count hides it completely.

**FIRST ASK WHETHER THE 32-BIT RESULT IS USED, because that decides which
question you are answering** (CLAUDE.md's forced-versus-free rule):

- **Used** — the extension is live and the difference is evidence about the
  loaded object's TYPE. Finding F613 is the precedent: a real defect no test
  could see, because both readings agree over every value the field holds.
- **Discarded** — a 16-bit value going straight back into a 16-bit slot. This
  is finding F614's free case, and it is NOT evidence about the field. Read on.

**WHERE THE EXTENSION IS DEAD, IT FOLLOWS THE DECLARED TYPE OF THE LOCAL BEING
LOADED INTO — not the field, not the store destination, and not a cast.**
Measured on `toneiir_reset`, six spellings compiled (7803):

    short prev = st->env_band;                  movswl
    unsigned short prev = st->env_band;         movzwl   <-- the blob
    int prev = st->env_band;                    movswl
    unsigned int prev = st->env_band;           movswl
    short prev = (unsigned short)st->env_band;  movswl
    short prev; prev = st->env_band;            movswl

A cast does not do it because the load happens first and the conversion after.
`unsigned int` does not either: the value is still fetched from a signed short
and then widened. So this is a real, recoverable source property with an
exhausted two-way domain, and 7782's ruling takes it.

**THE TRAP THIS EXISTS TO PREVENT.** 7802 read exactly this difference as a
declaration defect in the FIELD and would have retyped `short env_band` to
unsigned. The blob itself refutes that: it uses `movswl` at
`toneiir_progress+235`, where the result feeds `imul $0x3f5c,%eax,%ebx`, and
`movzwl` at three sites where the upper half is discarded. **One field, both
extensions.** Retyping it would have matched two sites, broken the third, and
asserted something the object contradicts.

So: if the object uses both extensions on one field, the field's type is not
what varies — look at each site's destination instead. And verify PER SITE
when the enclosing functions are not byte-identical: `toneiir_progress` keeping
its `movswl` is what proved the edit touched only the dead sites.

**TWO MORE PLACES THIS HAS BEEN GOT WRONG, both worth checking before any
retype. The extension may belong to the ACCUMULATOR rather than to the
element.** `FPM_FSE_receive` reads `tilt_coeff[4]` and `tilt_hist[4]` `movzwl`
inside a 4-tap multiply-accumulate, and a `short` array feeding a 32-bit `imul`
normally gives `movswl`, so the naive reading is `unsigned short`. It is wrong,
and the experiment is cheap — the same `short` declaration with the
accumulation written two ways:

    int acc = 0; ... acc += c[j]*h[j]; out = (short)acc;
        -> movswl on both operands, and the `out = 0` store is DELETED
    out = 0; ... out = (short)(out + c[j]*h[j]);
        -> movzwl on both operands, and the zero store is KEPT

The kept store is `movw $0x0,0x76(%ecx)` in the object, so the accumulator is
the `short` field itself, only its low 16 bits are ever stored back, and the
extension is free (3581). **The presence or absence of a zeroing store beside
the loop is the tell**, and no differential test can separate the two spellings.

**And a CALLEE's mangled signature beats an inference from a free encoding.**
`V90SpectralShaper`'s two heap buffers were typed `unsigned short *` on the
strength of every access being `movzwl (%reg,%edx,2)`. The stride is real
evidence; the extension is not — every one of those loads has its 32-bit result
discarded by a 16-bit store. What is forced is that both pointers are handed
straight to `progress(const short *)` and `getMetric(const short *, unsigned)`
with no conversion instruction between the load and the push:
`_ZN24V90SpectralShapingFilter8progressEPKs` is `PKs`. An `unsigned short *`
would not convert silently in C++ at all — it needs a cast the object gives no
reason for. Evidence class 2 beats class 3, and `compare.py` did not move by
one symbol on the retype (5850).

**A `switch` PROMOTES ITS CONTROLLING EXPRESSION; AN `if`/`else` CHAIN DOES
NOT — that is the width, and the FIELD's declared type does not change.**
Issue #158, `RxNextStateV17`'s IDLE-arm rate ladder. The object loads
`V17RXC_RATE_CODE` (`unsigned short`) with `movzwl` and then compares **16
bits**: `test %ax,%ax` / `cmp $0x1,%ax` / `cmp $0x2,%ax; sete; add $0x6`. A
`switch (RXCTL(modem)->rate_code)` emits `cmp $0x1,%eax` / `jle` / `cmp
$0x2,%eax` — the switch's controlling expression undergoes the integer
promotions, so the tree is SImode and the compare is 32 bits wide. Nine
spellings were compiled under the period flags and compared full-text, and the
`if`/`else if` chain **directly on the field** is the only one that maps:

    switch (RXCTL(modem)->rate_code)        32-bit cmp, SIZE 6 off
    short rate = (short)RXCTL(...)->rate_code; switch (rate)   movswl, 32-bit
    short rate = RXCTL(...)->rate_code; switch (rate)          movswl, 32-bit
    unsigned short rate = RXCTL(...)->rate_code; switch (rate) movzwl, 32-bit
    if/else if on RXCTL(modem)->rate_code                     movzwl + 16-bit  <-- the object
    short rate = (short)RXCTL(...)->rate_code; if/else chain   movswl + test %eax
    unsigned short rate = RXCTL(...)->rate_code; if/else chain movzwl + test %eax

This is not a narrowing of a computed value: the lvalue read is the same
`unsigned short` field in every cell, and the object's own `movzwl` says
unsigned. What the chain changes is where the promotion happens, and the
HImode compare is what the compiler was FORCED to encode from it. Applying it
to `RxNextStateV17` took the symbol from SIZE (6 bytes / 4 instructions) to
all bytes equal with one section relocation unresolved, and `RxHdxScramV17`'s
expiry ladder to the object's shape. **The same substitution is worth checking
wherever a narrow lvalue is `switch`ed and the object compares it at 16 bits.**

**AND THE LADDER IS NOT `RxHdxScramV17`'s WHOLE RESIDUAL.** The expiry
countdown is a separate, pre-existing difference with its own cause:
`left = (short)((unsigned short)RXCTL(modem)->countdown - 1); ... if (left > 0)
return 0;` emits `movzwl; dec; cwtl; mov %ax; test %ax,%ax; jg` where the
object has `movzwl; dec; test %cx,%cx; mov %cx; jle` — a sign-extension and a
test/store order the ladder change does not touch. It is shared with
`RxHdxBridgeV17` and `RxHdxPrtcolV17`, which carry no ladder at all, so it is
not this lever and not #158. After the ladder fix `RxHdxScramV17` is 270 bytes
against the object's 266 and the three handlers are all 210/210 in the twins
with 79 bytes differing. Recorded, not acted on.

**#160 ENUMERATED THAT COUNTDOWN AND IT IS NOT THE WHOLE RESIDUAL — MEASURED,
NOT ARGUED.** Fifteen spellings were compiled with the exact period toolchain
and compared full-text against the blob. The object's `movzwl 0x1a(%edx),%ecx;
dec %ecx; test %cx,%cx; mov %cx,0x1a(%edx); jle` IS matched in width and
signedness in isolation: a read-back or pre-decrement on the field,

    RXCTL(modem)->countdown = (short)((unsigned short)RXCTL(modem)->countdown - 1);
    if ((short)RXCTL(modem)->countdown > 0)
        return 0;

emits `movzwl 0x1a(%edx),%esi; dec %esi; test %si,%si; mov %si,0x1a(%edx)`,
removing the `cwtl` and the 32-bit test, and takes the handlers to 269/209/209.
But **no countdown spelling is full-text identical**, because the carrier arm is
laid out in the opposite order: the blob falls through from `test %eax,%eax`
into the success path (`je` to the out-of-line error arm), ours into the error
path (`jne` to the success arm). That reordering is independent of the countdown
— all fifteen spellings keep the error arm first — and predates #158, which
touched only the ladder. The register swap (`%ecx` in the blob against
`%eax`/`%esi` here) is its consequence, not a second cause. `byteident.py`
agrees: the width fix removes the `cwtl` but the handlers keep one extra
instruction each (blob 76/58, ours 79/60 at grade 0). Full-text identity needs a
second lever on the carrier `if` (a success-as-then restructure does flip the
blob's `je` and lands on the object's `%ecx`, but still hoists the return-0
`xor`); that is a different finding, not this one. **Declined, and #160 does not
close on the countdown alone.**

**#162 RECOVERED THE PAIR, AND THE MISSING LEVER WAS THE RETURN-0's SHAPE.**
The carrier `if` and the countdown are needed TOGETHER: with both right all
three handlers are grade-0 byte-identical (`RxHdxScramV17` 266 B, the twins
210 B each). The object's carrier block is:

    call CarrierDetectV17
    test %eax,%eax
    je   <error, out of line>          success falls through
    orb  $0x20,0x29(%edi)              flags |= CARRIER
    mov  0x5c(%edi),%esi               RXCTL
    movb $0x1,0x28(%edi)               status = CARRIER
    movzwl 0x1a(%esi),%ecx             countdown, loaded UNSIGNED
    dec  %ecx
    test %cx,%cx                       tested at SIXTEEN bits
    mov  %cx,0x1a(%esi)                stored as sixteen
    jle  <expiry, out of line>
    xor  %eax,%eax                     ONE return-0, shared
    <epilogue>
    ret
    <error>: ... fields ... jmp <the xor>
    <expiry>: ladder (Scram only); GetSNRV17; RxNextState;
              movswl %bp,%eax; jmp <epilogue>

The carrier order is the success-then form (`if (CarrierDetectV17(modem) != 0)
{ ... } else { ... }`), which flips the `je` and the register assignment as
#160 measured. What #160's success-as-then still hoisted is the `xor`: written
with an early `return 0` inside the then-block, GCC 3.4.2 materialises the
return value in `%edx` at the TOP of the success block and emits a SECOND `xor`
in the error arm, so the object's single shared return-0 never forms.

**A `short rc = 0;` AND A SINGLE `return rc;` AT THE END IS THE FIX.** The
success body puts the expiry behind `if ((short)RXCTL(modem)->countdown <= 0)
{ ... return (short)n; }`, the error arm sits in the `else`, and the function
returns `rc` once. That creates the shared return-0 block the object has: the
countdown's fall-through and the error arm both reach one `xor %eax,%eax`. The
countdown read-back (#160's) supplies the 16-bit `test %cx,%cx` and removes the
`cwtl`.

**THE CROSSED ENUMERATION, all three handlers compared full-text under the
period toolchain.** Carrier spellings: guard/early-return (the pre-#162
source), success-then/else-error, success-then with the error after,
`!CarrierDetectV17` guard, error-in-else-with-shared-tail, and the `goto` shared
tail. Countdown spellings: the `short left` local and #160's read-back. The
`short left` column never closes (the `cwtl` and 32-bit test remain); every
spelling except one keeps the error arm first or duplicates the return-0. The
one cell that maps onto ALL THREE is `short rc = 0;`, success-then/else-error,
read-back countdown, expiry as the then-block:

    shape               cd         Scram          Bridge         Prtcol
    guard (pre-#162)    left       SIZE 4         BYTES 79       BYTES 79
    guard               readback   SIZE 3         SIZE 1         SIZE 1
    success-then        readback   SIZE 16        SIZE 1         SIZE 1
    success-then/else   readback   SIZE 16        SIZE 1         SIZE 1
    goto shared tail    readback   EXACT          SIZE 18        SIZE 18
    rc + single return  readback   EXACT          EXACT          EXACT

Within the tested domain the preimage is unique: only the `rc` cell is
full-text identical for all three, and no other cell closes any of them. The
twins were NOT folded (F9444); each body is written out and each is exact on
its own.

**MEASURED RESULT.** `byteident.py` grade 0: the three handlers are EXACT,
`v17.c`'s own exact set 6 -> 9 with nothing lost, whole tree 829 -> 832.
`make phase`: `period differential: 375 passed, 0 failed`, `phase boundary:
period differential and structural checks all OK`. The one ratchet loss on the
tree (`_ZN13V90ParametersC2EP19_tagModemParameters` and two `V92Modulator`
constructors) is PRE-EXISTING and reproduced with the pre-#162 `v17.o`, so it
is not this change. #162 closes.

### Lever 9. Operand order — which is decided by the TREE, not by how you spell it

`return dsp->rx_energy & dsp->rx_tone;` — swapping the two operands gave byte
identity. That much has always been in this file. What was missing is that the
lever usually does not work, and why.

**THAT EXAMPLE IS THE FILE'S ONE UNCITED CLAIM, and the mechanism below does not
cover it.** No finding in the tree records it; a sweep for one found nothing.
It is also a bitwise `&` of two `COMPONENT_REF`s, *neither* of which is a DECL,
so the rule below cannot be what swapped it. It is kept because it happened,
and it is marked because the next reader will otherwise apply a comparison rule
to a bitwise expression and get nothing. **Everything that follows is measured
on COMPARISONS.**

**THE MECHANISM, AND IT IS THE ADVANCE TEST.** GCC 3.4.2's
`tree_swap_operands_p` (`fold-const.c`) returns "swap" when operand 0 is a
`DECL_P` and operand 1 is not. So `local > params->THRESHOLD` — a plain local
against a `COMPONENT_REF` — is canonicalised to `params->THRESHOLD < local`,
and **the threshold is what gets loaded into `%st(0)`**. Rewriting the source
comparison the other way round changes nothing, because both spellings fold to
one RTL. **Reading the member into a local first is the fix**: both operands
are then `DECL_P`, the first test returns 0, no swap happens. All six sites in
`checkSpecialSpectralConditions` took the object's own condition codes on that
one change, and five spellings were compiled and RUN against a real NaN before
it was believed — the plain form and a nested-`if` detect on a NaN; a local
threshold, a local array and a local struct do not (3529).

**AND UNDER `-mno-ieee-fp` THIS IS BEHAVIOUR, NOT CODEGEN.** The swap comes
with an inverted predicate, which is identical for ordered operands and
OPPOSITE for a NaN, so a comparison can be logically right, spelled every
available way, and still send a NaN down the wrong arm. `V90SdDetector::process`
was 304 failures of 32,262 and took `getV90Decision` down with it (2301);
`CalcErrorEnergyAfterEchoCancellation`'s keep-rate flag answered 0 where the
object answers 1 and was green over 24,509 checks until the accumulator was
seeded negative (4812). Only `make period` sees any of it — GCC 13 honours
IEEE for `>` whichever order it picks, so `make one`, `make test` and
`mutate.py` all pass either spelling (3529).

**WHERE IT STOPS, AND THIS IS THE HALF THAT WAS MISSING. Four measured
failures, all of them the TYPE and not the order:**

    GenericToneDetector::process   `a >= b` -> `b <= a`, and inverting the
                                   condition with the arms swapped: NEITHER
                                   moves the emitted branch.  GCC canonicalises
                                   operand order (1991)
    V90Equalizer high-error test   six probes; every `float` spelling gives
                                   `fcoms`+`jbe` and BOTH `long double`
                                   spellings give the object's
                                   `fcomp %st(1)`+`jae` (5701)
    displaySpectralParams sign     seven spellings; only the `long double`
                                   parameter emits `fldz; fcompp` (5823)
    advanceTrellis compare         six spellings; the two that give the
                                   object's `fcoms` are the two a differential
                                   test refuses (5855)

**So: vary the TYPE first and the operand order second, and compile the
candidates rather than reasoning about them** (5823, which is the second site
to say so independently). 5855 is the sharpest case — our operand 1 was a
`NOP_EXPR`, a `float` widened to `long double`, so it is not a DECL, GCC swaps,
and the `float_extend`-of-memory compare no longer applies because that pattern
wants the narrow memory operand SECOND. The order was never the free variable.

**A decoded operand order is worth a paragraph even when you decline it.**
5855's is recorded with all six cells and left alone, because the two matching
spellings are refused by a test and CLAUDE.md's rule is that the differential
tier decides.

### Lever 9a. And a NULL over a whole file family is a result — prove the harness fires first

Lever 3 was run to exhaustion over eight small `fpm_*.c` files that were NOT
in the blob's emission order — seven at 3! and one at 4!, each maximal run of
`static` definitions glued to the block below it so a static never lands under
its first user. **Seven of the eight give ONE distinct emission over their
whole domain**; the eighth gives two and neither closes anything.

The null was only believed after the detector was shown to FIRE, which is
finding F134's argument applied to an enumeration: compile the
block-REVERSED file and print `nm -n` beside the byte comparison.

    fpm_sre.c   order  init,free,recover -> recover,free,init   MOVED
                bytes  every symbol IDENTICAL
    fpm_sdm.c   order  init,scram,descram -> descram,scram,init MOVED
                bytes  every symbol IDENTICAL

That is 7797's ruling with the measurement in hand: the order is achievable
and pays nothing, so record it and **do not keep the diff**. An enumeration
that reports "0 of 6 cells" without showing that any cell differed from any
other is indistinguishable from a broken generator.

### Lever 10. Where a member's body is written — in-class is implicitly `inline`

A member defined inside the class body is implicitly `inline`, which moves it
from `--param max-inline-insns-auto` (100) to `max-inline-insns-single` (500),
and GCC 3.4.2 then inlines it nearly everywhere **while still emitting the weak
symbol** — so the symbol table looks right and every call site is wrong.

    Scrambler::reset      moved out of the class body   452 -> 455 identical,
                          nothing lost; V90Modulator::reset plus the two
                          constructors that call it                    (5805)
    Scrambler::process    inlined at all 19 sites the blob calls;
                          `V90Modulator::progress` +57 -> +3 instructions,
                          identical SET 488 before and 488 after        (7543)

**The tell is 7480's shape:** an EXCESS of instructions with a MISSING call.
Count `R_386_PC32` sites against the blob's for the template member; ours had
zero against nineteen.

**RUN OVER A WHOLE FAMILY IT IS STILL THE BEST-PAYING LEVER HERE (F8001).**
`Scrambler`/`Descrambler`'s remaining in-class members -- both `process`
overloads, `processAllOnes`, `processAllZeros` and every constructor and
destructor -- screened blob 117 against ours 0, and 105 of the 117 sites were
traced to functions this tree defines rather than left as 7867's
unwritten-caller noise. Moving all of it at once gained **10 EXACT with nothing
lost**, every one a bystander in a directory the pass never edited. Two rules
came with it: move the whole family, because 7866's half-move made four bodies
worse invisibly; and APPEND the definitions at the foot of the header, because
7815 is the cost of moving anything already in it.

**IT IS LIVE, AND THE SCREENING TEST IS A RELOCATION COUNT (7867).** 7831's
measured NO above is real and is about how the lever was REACHED: that pass
chose it from a brief and went looking for somewhere it might apply. Read the
other way it paid for almost a whole pass -- `Scrambler`'s
`resetHistoryIndexes` (7862) and `copyHistoryTail` (7866) are both in-class
bodies the OBJECT calls, and finding the call is one command over both
objects:

    grep -c 'R_386_PC32.*<mangled member>'    ours 0, blob 9

Zero against nine is what `copyHistoryTail` looked like while `nm` showed all
five of its symbols present, which is 5805's warning exactly. **Do not judge
which members "look inlineable"; count the call sites for every member defined
inside a class body.** And 7866 is the trap on the way out: fixing ONE of two
helpers called from the same conditional made four `process` bodies worse and
**no bucket moved, because SIZE to SIZE is invisible to a set diff** -- read
the byte counts beside it.

**IT RUNS BOTH WAYS, AND THAT IS WHAT MAKES IT A LEVER.** `Descrambler`'s bulk
`process` STAYS in the class body, because the blob carries no
`Descrambler<...>::process` symbol at all and moving it out would make us emit
one the original does not have (7543). Read the object first.

**Where it stops:** `V90SpectralVerifier::printSpectrum` is already out of
line, and ours is inlined into `process` where the blob's is a real call.
Definition order was probed (moved after `process`: no change) and so was
translation-unit growth (padded with 400 unrelated functions: still inlined,
still 419 bytes, to the byte). Cause not found, recorded rather than chased
(5802). **Do not reach for `__attribute__((noinline))`** — that is fitting the
compiler, and it puts a construct in `src/` the original cannot have had.

### Lever 11. The constant pool is a typed, per-function observable

`.rodata.cst4` against `.rodata.cst8` against `.rodata.cst16` names the literal's
type, and the load instruction says it again:

- `fldt` where the blob has `fldl` means we wrote `0.54L` and the author wrote
  `0.54` — three constants in `hamming<float>`, and `DspMath.cpp`'s own comment
  already recorded the object's operands as eight-byte slots (2903).
- `fmuls` and not `fmull` makes the multiplier single precision; a `double` 0.4
  would have gone to `.rodata.cst8` (4340).
- **A constant that is NOT there is evidence too.** The "OutputConversionFactor"
  print uses 1e5, 1e4, 2^30, 2^24, 2^20, 2^-16 and 2.0 and nothing near a
  thousand, though the format is `%03d`. The 1e3 multiply folded away, which
  happens only if the value was an `int` (2146).
- **A third x87 op where two would do puts a reciprocal in the source.**
  `fildll; fdivr %st(2),%st; fmuls` is `1.0/count * sum`, not `sum/count`, and
  the extra operation exists only because there is a constant 1.0 to divide
  (4340).

**FIRST MEASUREMENT IN A REFINEMENT PASS, AND IT IS A NO (7831).** A pass
briefed that its set was float-heavy and that this lever was the one most
likely to pay found nothing, in two different ways worth separating. **The
job of a file does not predict its arithmetic**: `v34filters.c` has ZERO x87
instructions over all 26 of its symbols in both objects and the TU has no
`.rodata.cst4`, `.cst8` or `.cst16` section at all -- it is fixed-point
`short`/`int`. Census the file; do not infer. **And `hamming<float>` above is
SPENT**: the pool types agree today, `fldl` 3 against 3, so an earlier pass
took it and only the first bullet was ever live there. What remains in that
symbol is the fourth bullet, the x87 ARRANGEMENT -- the blob spends
`fxch %st(3)` and divides at stack depth 3 where we divide at depth 1 -- and
it is a CONSTANT MAP: 18 spellings (where the loop's three constants are
declared x how the reciprocal is written x the multiply's operand order) give
ONE distinct emission and not one byte moves. Read the divide's BYTES, not
objdump's mnemonic (245): the blob's `de f2` prints `fdivp` and IS FDIVRP.

**Where it stops: the pool is emitted PER FUNCTION.** `output_constant_pool`
runs at the end of each function and `-fmerge-constants` leaves the folding to
the linker, so in a `.o` the duplicates are all still there — `0.5f` appears at
`+0x1b8` and again at `+0x1c4`, and 24804.0f twice three slots earlier.
**Two slots with the same bytes in one translation unit say nothing at all**,
and a reader who treats slot identity as expression identity will mis-read
every inlined float constant in this object (4340).

**FIRST TEST ON A GENUINELY x87 SYMBOL, AND IT FOUND A NEW FAILURE MODE
(F8041).** `hamming<float>` was this lever's own named example and is SPENT --
`fldl` 4 against 4, fixed by an earlier pass. `hanning<float>` did have the
defect, and fixing it **moved no grade at all**: `6.283185307179586L` puts the
constant in `.rodata.cst16` reached by `fldt` where the object has `.cst8`
reached by `fldl`, and those are **the same six bytes in the function**. So a
real recovery here can be VERDICT-INVISIBLE, and the only thing that sees it is
the constant pool's SECTION, not the instruction stream. Verified bit-exact
against the blob's own `ref_hanning` over n = 0..300.

### Lever 12. Spill width is forced; a value that never spills is not

The tree's standing rule is "the object keeps intermediates in registers and
never rounds them, so spell them `long double`" — `V90Equalizer.cpp`'s file
comment says so and 256 measured it holding for two step-size setters.
`enterPhase4` is the counterexample and it sharpens the rule:

    36b96:  fstps 0x38(%esp)      the getter's result
    36bd0:  fstps 0x38(%esp)      the DIFFERENCE, rounded to 4 bytes

Two `fstps` to a four-byte slot is two roundings to single precision. Written
with `long double` intermediates the transcript failed on EVERY non-zero offset
in the sweep and passed on zero — 48 of 1,092 checks — and two `float` locals
fixed all of them (2139).

**So the SPILL WIDTH is the evidence, not the presence of a spill.** A four-byte
`fstps` on an intermediate says the source had a `float` variable there. A value
that stays in a register between its producer and its consumer says nothing, and
that is why `long double` was right for the two setters. The first spill in
`enterPhase4` is forced by the following call clobbering the stack and carries
no information; the second is not, and it is the one that decides.

Note for `tiers.md`, not a change to it: that file's FREE column — register
allocation, scheduling — stands, and this tree now has two named exceptions to
it. A scratch-consuming `peephole2` (lever 3b) makes allocation steerable, and
a spill slot narrower than the value it holds makes a spill forced.

---

**AND ITS PREMISE FAILS ON THE FIRST SYMBOL IT WAS AIMED AT (F8043).**
`blackman<float>` was chosen because it carries `sub $0x14` where the object
has `sub $0x4`, read as a spill-width difference. It is not: the object's
`sub $0x4` is **ABI alignment** -- push, push, sub 4 puts `%esp` at 0 mod 16 --
and there is no `fstps`/`flds` through `(%esp)` anywhere in its body. **A frame
size is not a spill width.** Look for an actual spill through the frame before
reading a frame delta as this lever.

### Lever 13. What the compiler can hoist out of memory depends on the SYNTACTIC FORM of the reference, not on the alias set

`V90Jd::packData` keeps `int crc[16]` -- a member -- out of memory for the
whole of its group loop: sixteen loads into stack slots before it, both groups
run on the slots, sixteen stores after, not one store to `0x4c(%edi)` between.
That is 74 of its 149 instructions and the whole of its `sub $0x48,%esp` frame.
Reproducing it needs two things in the source, and the second is the one nobody
expects.

**1. Every subscript must be a constant.** Rolled as `for (i = 0; i <= 14;
i++) crc[i] = crc[i + 1];` no element has a loop-invariant address and nothing
can be promoted at all. Written out, they all do. This is lever 6.

**2. Both sides must be MEMBER SUBSCRIPTS of the same object, not pointers.**
Through a helper's `int *crc` and `const unsigned char *in` -- the obvious
factoring -- the two arrive as indirect references that GCC 3.4.2 cannot tell
apart, so the stores have to stay in the loop in case the byte read aliases
them, and the promotion collapses to a per-group reload. Written `crc[i]` and
`bits[...]`, they are component references to different fields of one record,
GCC proves they cannot overlap, and the stores sink. The four-compile ladder,
instruction count against the object's 149:

    75   helper with pointers, shift rolled
    141  shift written out, helper unchanged
    137  helper given the object but still doing `int *crc = jd->crc;`
    149  shift written out AND both sides member subscripts

**IT IS NOT AN ALIASING PROBLEM IN THE C SENSE, and that is worth knowing
because the C answer is the intuitive one and it is wrong here.** Casting the
input read to `const int *` and then to `const float *` -- the latter genuinely
cannot alias `int` under `-fstrict-aliasing` -- moved nothing while the
pointers stayed pointers. An alias-set argument is not a substitute for the
syntactic form. Finding F7940.

**AND DO NOT HAND-WRITE THE INDUCTION VARIABLE THE OBJECT SHOWS.** The same
loop's object code is `movzbl (%ebx),%eax; inc %ebx` with a `dec %esi; jns`
countdown, which reads like a source-level pointer walk. It is not: spelling it
`*in++` costs condition 2 on the input side and lands at 136 instructions,
thirteen short. The walking pointer and the countdown are what strength
reduction MAKES of the subscript, so writing them by hand removes the
information the compiler needed and then hands back what it would have derived.
Finding F7941, and it is lever 9's rule -- act on what the compiler was forced
to encode -- applied to an induction variable.

**AND A MEMBER REACHED THROUGH A CAST HAS A THIRD FORM, where the obvious
cleanup is NOT neutral -- BUT THE NEUTRAL FORM IS NOT THE DEREF SPELLING.**
Issue #141 removed the cast-hiding macros from the fax read paths, and one site
resisted. `RXS(modem)` is `((struct v17rx *)(modem))->state`, so
`(&RXS(modem)->agc.value)->cfg.alpha++` is an address-deref through that cast.
Rewriting it to the direct-dot `RXS(modem)->agc.value.cfg.alpha++` -- dropping a
redundant parenthesis only -- grew `RxNextStateV17` by 16 bytes, and #141
recorded the address-deref as the object's form on that ground. Issue #152
enumerated the forms and the ground does not hold.

**THE 16 BYTES ARE THE COST OF RE-EVALUATING `RXS(modem)`, NOT OF THE DOT.**
The direct-dot re-loads `modem->state` between the two statements -- an extra
`mov 0x60(%ebx),%edx` and the register/alignment shift it forces -- where the
address-deref form CSEs one load across both. A cached state pointer is
neutral: `struct v17rx_state *rxs = RXS(modem); rxs->agc.value.cfg.alpha++;
rxs->agc.value.cfg.beta++;` is byte-for-byte identical to the address-deref
form over the whole function, as is `(&rxs->agc.value)->cfg.alpha++`, and so
are the address-deref, the double-address-deref and the parenthesised-dot
spellings. The object's own increments are `addl $0x2,0xc0(%edx)` /
`addl $0x2,0xc4(%edx)` -- base is the STATE pointer, offsets from `agc.value` --
and every one of those forms emits exactly that. Caching a pointer to the
SUB-object instead (`struct fpm_agc *a = &RXS(modem)->agc.value;`, or
`struct fpm_agc_cfg *c = &...->cfg;`) emits `addl $0x2,0xc(%edx)` /
`0x10(%edx)` and is excluded by the object's offsets. So the preimage is NOT
unique, and the +16 does not establish the address-deref form.

**THE WHOLE-FUNCTION CONTROL WAS UNAVAILABLE, AND THAT IS PART OF THE RESULT.**
At HEAD `RxNextStateV17` is 845 bytes against the object's 730 because
`Restore_rateV17` and `StoreCoefV17` are INLINED here where the object CALLS
both -- an inline-boundary difference unrelated to this field. The +16 and the
equality of the cached forms are therefore measured with the rest of the
function held fixed, not by full-text identity with the blob. The current form
is retained because several forms are byte-identical; it is not uniquely
recovered. Issue #141, #152.

### Lever 14. The TRANSLATION-UNIT PARTITION is a source property the object records

`src/service/voice.c` held the whole ring detector, and `RD_delete`,
`RD_process` and `RD_ring_details` could not be made exact at `-O3`: ours
inlined `RingDetector_Delete`/`RingDetector_Process`/`RingDetector_GetLastRing`
into the wrappers, while the object CALLS all three. The tempting answer was a
flag -- the global `-fno-unit-at-a-time` profile does recover those three -- but
the object says something structural. Its `STT_FILE` records are

    voice.c, fax.c, rd.c, ringDetector.c

in that input order, and the wrappers are in `rd.c` while the detector is in
`ringDetector.c`. `-O3` cannot inline across a TU, so the reference's calls are
not a compiler choice at all; they are the file split. Moving the two groups
into their own files, bodies unchanged and no flag touched, is **7 -> 10 EXACT**
in that TU and **822 -> 825** over the tree, denominator unchanged. Finding
F11351.

**What to look at, and why the usual maps will not show it.** The authority is
`readelf -sW`'s `FILE` records (or `docs/modules.md`'s source, `tools/tumap.py`),
NOT a span name and NOT `docs/attribution.md`'s prefix matching -- those merge
several input files into one `bracket` extent whenever the inner TUs have no
local symbol to anchor them, which is exactly the silent case. `ld -r` keeps
the `FILE` records in input order, so they answer both "was this two files?" and
"in what order?".
**The map generator had this wrong by a bug of its own, now fixed** (F11352,
#67): an anchored TU's `exact` extent was extended to the next anchor and the
intervening TUs' globals were reassigned to it, which is how `voice.c` came to
own `rd.c`/`ringDetector.c`. `tools/tumap.py` now reports the anchored TU's
own local-symbol envelope as `exact`, the gap as a shared `bracket` with its
candidate TUs named, and globals in that gap as ambiguous. `readelf`'s FILE
records remain the authority, but `docs/modules.md` no longer contradicts
them.

**The tell is a call boundary, not a byte count.** A wrapper that `SIZE`s small
because it inlined a helper, or a constructor that `SIZE`s large because it did
not inline one the reference contains, is the signal; so is an exact function
that gains or loses a helper call under an unrelated change. Reach for this
lever before the flags. (Splitting a TU used to require moving its `t_*`
mutation suite; that system is retired, so there is no anchor bookkeeping to
do. The same question for the rest of the object is #6/#20.)

**The tested V.34 TU merge was null (2026-10-19).** The object records a
`V34.c` FILE
record that this tree does not have; its `.text` is `0x57860..0x5AF10`, 20
global functions, which this tree spreads over `v34scram.c` (4), `v34shell.c`
(13) and `V34RX.c` (3). Renaming the two `.c` files to `.inc` and adding one
`V34.c` that includes both -- the whole merge, one compile -- left the census
**bit-identical**: same sizes, same exact set, `diff` of `--list-exact` empty.
The call-boundary observation is measurable: the object has **no relocations at all**
to `scrambleGPC`/`scrambleGPA`/`descrambleGPC`/`descrambleGPA`/`agcadapt`/
`decision` (every call site inlined them), so there was no boundary for the
merge to restore, and the 908-byte deficit sits inside functions that already
shared a TU. Count cross-TU relocations when testing a call-boundary
hypothesis. Zero references exclude that particular explanation, not every
effect of a partition: emission state, constant/data layout and selected
COMDAT copies have separate observables (Levers 3, 11 and 15). Do not move a
TU without an ownership witness and a predicted discriminator. The same
screen would
have saved the `v8seq.c` question: the source comments annotate seven
`V8Interface.c` functions with `(v8seq.c)`, but the object has no `v8seq.c`
FILE record at all.

### Lever 15. A header-only template has NO translation unit — the user instantiates it

Lever 14's FILE records answer "was this two files?"; they also answer "was
this a file AT ALL?". A class template whose members are defined in the header
has no `.cpp` of its own — its weak `.gnu.linkonce.t` symbols are emitted by
whichever TU instantiates them. Reconstructing it as an out-of-line `X.cpp`
with member-wise explicit instantiation is a plausible-looking file the object
never had, and it is detectable two ways.

**The absence of the FILE record is evidence, not an artefact.** `ld -r`
preserves a `FILE` record for an object with NO symbols at all (measured: a
partial link of our own emptied stubs still carries one), so if there is no
`X.cpp` in `readelf -sW`'s `FILE` records, there was no `X.cpp`.

**The second tell is C2/D2.** Explicit member instantiation of a constructor
and destructor emits the base-object variants C2/D2; an implicitly
instantiated class in the blob has only the referenced C1/D1 (F601). The two
spellings emit the SAME function bodies, so the exact-byte metric cannot see
the difference — the partial object's symbol set can.

**The mechanism.** Move the definitions into the header and DELETE the
invented `.cpp`. Do NOT add `inline`: with it GCC inlines five of the six away
and their out-of-line symbols vanish; without it they are emitted exactly as
the object has them. Keep the body out of the class (in-class is Lever 10 and
inlines nearly everywhere) and keep `__attribute__((noinline))` only where the
object already forces it (`Queue::reset` is called by the constructor).

**Measured (Queue, F11355).** `Queue<float>` had been a `src/dsp/Queue.cpp`
with explicit instantiation. There is no `Queue.cpp` FILE record; the only
references anywhere are `V92Modulator`'s `reset`, `progress` and
ctor/dtor. Moving the definitions into `Queue.h` without `inline` and deleting
`Queue.cpp` gives a partial object with no `Queue.cpp` FILE record and exactly
the blob's six Queue symbols, no C2/D2, where the `.cpp` form added two
sections and a FILE record. `make phase` green. The function bytes are
unchanged by the move — `Queue.cpp` and `V92Modulator.cpp` emit identical
counts — so this lever fixes WHICH TU owns a symbol and the symbol SET, not
the residual bytes.

**The family audit is DONE (F11356, issue #74).** `Agc.cpp`, `DiffCoder.cpp`,
`DspMath.cpp`, `LowPassFIR.cpp`, `Scrambler.cpp` and `SineWave.cpp` all lacked
a FILE record and all six were the same invented TU; their definitions now
live in the matching headers without `inline` and the `.cpp` files are gone.
`make phase` is green and the partial object carries no invented FILE record
and no C2/D2 for any of them. Two consequences worth carrying to the next
family:

- `operator delete[]` stays one copy per `.cpp` (F7815). `DiffCoder`'s
  parallel coders and `LowPassFIR` free with `delete[]`, so the TUs that now
  instantiate those destructors -- `Resampler.cpp`, `V90SignBitsExtractor.cpp`,
  `V90SpectralShaper.cpp` -- needed a local definition, and the direct tests
  needed theirs as apparatus.
- **A function template's codegen can depend on the instantiating TU's
  flags.** `DspMath`'s `sinc` expands `sin()` to `fsin` only under
  `-ffast-math`; a fixture that includes the header and calls the function
  instantiates it under the fixture profile and disagrees with the blob.
  `t_dspmath` therefore carries `test/unit/t_dspmath.cxxflags`, read by both
  build systems, rather than calling the source TU's symbol from the test.
  Check the flag dependency before moving a function template whose body uses
  a transcendental. See #74's F11356.

### Lever 16. A holder declared with the wrong pointer type hides casts that retyping removes

When a local, field or parameter holds one specific struct pointer but is
declared as a generic or wrong pointer type, every use site casts to the real
struct. Retyping the holder to the real struct type is **codegen-neutral** --
pointer types are not observable in the emitted code, and the symbols here are
`extern "C"`, so no mangling or DWARF recovers the declared type -- and it
lets the casts go.

Measured on `DemodDataV17` (`src/fax/v17.c`): `rxs` held `RXS(modem)`
(`struct v17rx_state *`) but was declared `unsigned char *`. Compiling the
file on the Gentoo period compiler with each of `unsigned char *`, `void *`
and `struct v17rx_state *` gives the **identical** object (`94c51dc0...`).
Retyping to `struct v17rx_state *` removed three `((struct v17rx_state *)rxs)`
casts.

The type is a fidelity CHOICE, not a recovery. Among the candidates keep the
truthful struct type over `void *` (a convenience that keeps the casts) and
over `unsigned char *` (not an established idiom). The retype also removes a
modern-compiler rejection -- GCC 14's `-Wincompatible-pointer-types` is what
stopped `make coverage` at this file.

Do NOT collapse these: a `void *`/`char *` holder cast to DIFFERENT struct
types at different sites (one generic pointer, several targets), and a
`char *`/`unsigned char *` used deliberately as a byte pointer. Decide per
holder from its USES, not from the declaration. A retype that is not
codegen-neutral is a different finding -- record it and leave the cast.

Issues #140 (tree-wide retyping audit) and #141 (replace the cast macros this
hides behind -- `RXS`, `RXS_SRE`, `RXS_FSE`, `RXSTATE`).

### Lever 17. Inside a loop body, the ORDER of the accumulate and the store is a source property

Lever 1 is about statements in a function; this is the same question one level
in, and it is cheap because the domain is two cells. Measured on
`v8_fsktxfilter` (V.8, F-2026-10-19, 116-byte function): the object walks three
moving pointers and, in one pass, both copies the delay line and multiplies
into an accumulator:

    movzwl (%edi),%ecx      ; x = *src--      (zero-extending 16-bit load)
    movswl %cx,%eax         ; (short)x
    imul %edx,%eax          ; * tap++
    add %eax,(%esp)         ; acc +=
    mov %cx,(%esi)          ; *dst-- = x

With the same declarations, the same `short k` and the same bounds, the two
statement orders in the body compile differently and only one is the object:

    acc += (short)x * *tap++;   *dst-- = x;     36 instructions, the object's exact 116 bytes
    *dst-- = x;   acc += (short)x * *tap++;     33 instructions, 94 bytes

So GCC 3.4.2 does not move an accumulate across a store here, and a 33-vs-36
instruction gap in a small pointer-walk loop is a body-order question before it
is an allocation question. Result: grade 0 `BYTES (1 byte)` -- the discarded
`pop %ecx` versus `pop %edx` -- and **grade 1 `ACCEPT`**. `make period` 394/0.

**A SIZE -> REGALLOC conversion is a win, and it is worth stating as a
success criterion.** The census has four buckets and the reconstruction
question is "is the function's source right", not "is its register assignment
right". When the object's size is reached and grade 1 accepts, the SIZE
question is answered; the residual is allocation and belongs to levers 3/5.
Do not keep hill-climbing a `BYTES (1..4)` residual -- that is how a
`pop %ecx`/`pop %edx` rename becomes an afternoon. Here the conversion moved
REGALLOC 32 -> 33 and SIZE 631 -> 630 with the exact set unchanged.

### Lever 18. A 16-bit store does not pin the pointee type -- prefer the spelling with the FEWEST casts

Lever 16 removes casts by retyping a holder to its truthful type. This is the
mirror case, and it BOUNDS what byte identity can claim: when a value's only
observable use is a 16-bit store, the declaration that produced it is not in
the object at all.

Measured on `v8_fsktxfilter`, four spellings of the two moving pointers, same
body and same local:

    unsigned short *src, *dst, with casts       116 B, byte-identical
    short          *src, *dst, no casts         116 B, byte-identical
    short *src, unsigned short *dst             116 B, byte-identical
    unsigned short *src, short *dst             116 B, byte-identical

A 16-bit store cannot distinguish them, and `unsigned short x = *short_ptr`
is the low 16 bits, which the compiler loads with `movzwl` anyway. **So the
object proves nothing about the pointer types here, and the reconstruction
should take the spelling with the fewest casts** (the declared `short
rx_scratch[]` needed no cast at all). Treat "this declaration is unobservable"
as a reason to prefer the truthful, cast-free spelling -- not as a licence to
pick a type the header does not support.

**And the cast that IS forced is worth recognising.** The object's pair

    movzwl (%edi),%ecx      zero-extending load
    movswl %cx,%eax         sign-extended for the product

is reachable only from an unsigned 16-bit local re-signed for the multiply:
`unsigned short x = *p; acc += (short)x * t;`. Both directions of removing it
were measured and both are wrong:

    (short)x present      116 B, movzwl + movswl + imul   (the object)
    x * *tap++            93 B, movzwl + imul             UNSIGNED multiply
    short x              93 B, movswl + imul              signed load, no zero-extension

So a load extension that disagrees with the product's signedness is evidence
for a narrow unsigned local plus one explicit conversion, not for a field
retype (compare Lever 8) and not for a stray cast.

### Lever 19. Test frontend inline eligibility separately from the native body

The installed Gentoo compiler has defaults of 100 for
`max-inline-insns-auto` and 500 for `max-inline-insns-single`, decoded from
its own parameter records. These are frontend cost budgets, not final
machine-instruction counts. Record the actual profile and the complete
callgraph decision before testing an `inline` declaration as a hypothesis.
Native body length alone does not establish the original frontend cost or
keyword.

PR #333 supplies a counterexample to inferring cost from the standalone
body. Two ordinary scalar declaration/assignment forms of `inverseMap`
both emit the exact 440-byte helper, but estimate at 98 and 102. Under the
same retained profile they respectively inline and retain the original call
in `process`. The [fresh full-TU replay](../apparatus-unblock-review.md)
preserves every other function body. No parameter override or artificial
operation is needed. Multiple spellings recover the cost boundary, so the
machine code does not identify one original spelling.

Measured on `src/service/ringDetector.c` (2026-10-19). `RingDetector_Reset` is
449 bytes / 124 instructions and has ONE call site, in `RingDetector_Create`.
With a plain definition our `Create` was 41 bytes:

    malloc(0x54); call RingDetector_Reset; ret

while the object's `Create` is 452 bytes and contains the whole reset body.
Adding `inline` to the DEFINITION in the .c (the header's non-inline
declaration is left alone) inlined it:

    Create   41 B / 9 insns  ->  435 B / 121 insns   (object 452 / 125)
    structsim  longest run 0 -> 14,  LCS 0% -> 77.6%

Two observations support the helper-operation interpretation:

- A `difflib.SequenceMatcher` of the object's `RingDetector_Reset` instruction
  list against the object's `RingDetector_Create` list scores 0.787, and every
  non-equal opcode is the malloc/return prologue, a branch displacement, or a
  pure register rename (`%ecx` <-> `%edx`, `%esi` vs `%edi`). This supports
  shared operations; it does not distinguish an inline call from duplicated
  source, since the enclosing context can alter register allocation in both.
- The standalone symbol is still emitted in both, which is what the language
  says should happen: the header declares the function without `inline`, so
  the inline definition is an external definition too.

**The bound.** A callee inside the auto budget may inline with or without
the keyword. A current-source cost above that budget is a useful control,
not proof of the unknown original source cost. Verify definition visibility,
other eligibility constraints and the actual decision; compare the complete
TU before retaining an explicit-inline hypothesis. It also does not fix the
callee: `RingDetector_Create`
stayed SIZE because `Reset`'s own body still differs by 4 instructions of
register allocation (the object spills `c` into a third callee-saved register
and reloads it), so the inline decision and the body's residual are separate
questions.

### Lever 20. A `cwtl` the object does not have is a 16-bit TEMPORARY, not allocation

When our code emits `cwtl` (`movswl %ax,%eax`), `movzwl %ax,%eax` or a
`movswl %ax,%reg` and the object's corresponding site has no extension at all,
the first suspect is not register allocation. It is a **short local that exists
only to be compared**: the source narrows a value into a `short`, the compiler
sign-extends it back to a word for the compare, and the compare then narrows it
again. The object never builds the temporary -- it increments or reads the
FIELD and tests the field.

Measured on `v90RateReneg` (`src/pump/v34/VPcmV34Main.cpp`, 2026-10-19):

    source:   short n = (short)((unsigned short)t->seg_symcount + 1);
              if (n <= 0x7f) ...

    object:   movzwl 0x3a4(%esi),%eax / inc %eax / cmp $0x7f,%ax / jle <store>
    ours:     movzwl 0x3a4(%esi),%eax / inc %eax / cwtl / cmp $0x7f,%ax / jle
    fixed:    movzwl 0x3a4(%esi),%eax / inc %eax / cmp $0x7f,%ax / jle <store>

Nine spellings were enumerated, and the result is a clean discrimination:

| spelling | emitted |
|---|---|
| `if (t->seg_symcount <= 0x7f)`, `(short)` of it, or `< 0x80` | identical, correct -- no extension |
| `short n` or `int n` + `(short)` compare | keeps the `cwtl` |
| `unsigned short n` + `(short)` compare | `movzwl %ax,%eax` instead |
| plain `int n` | 32-bit `cmp $0x7f,%eax` |

So the plainest field test is the preimage. The change also restored the exact
instruction count (121 vs 121, was 123) and took structsim to LCS 118 (97.5%).

**Read the fingerprint before trusting it.** A cast at the declaration is the
tell: `(short)((unsigned short)x + 1)` says the author was fighting a width, and
the object usually shows which width he settled on. But the object legitimately
contains `cwtl` where a `short`-returning call feeds wider arithmetic (the same
function has one at 0x9a74 after `getV90CpBits`, and ours matches it there), so
the test is per SITE: ours has the extension, the object's corresponding
instruction does not.

**A `short` RETURN does the same thing as a `short` local**, and it is easy to
miss because the narrowing lives in the callee's signature rather than in the
expression. `V92Phase3Modulator` (`src/pump/v90/V92Phase3Modulator.cpp`, same
day) had five `cwtl` the object does not have, at sites reading

    movzwl 0x4(%esi),%eax / neg %eax / cwtl / movswl %ax,%ebx

where the object reads `movzwl 0x4(%esi),%eax / neg %eax / movswl %ax,%ebx` --
one widening, not two. The causes were a `(short)` cast on the negation AND
`static short jaSymbol(...)` / `static short trn1uSymbol(...)` returning `short`,
so the value was narrowed on return and widened again by the caller. With the
helpers returning `int` and the casts gone the symbol's instruction count became
the object's exactly (391 vs 391, was 394); the common run stayed 78 and LCS
went 299 of 394 (75.9%) to 299 of 391 (76.5%), which is a count result and not a
run result -- quote it that way. The temporary itself is still a `short`: with
`int sample` the same function measured 18 differing bytes instead of 3.

So when the fingerprint is present, check three places, cheapest first: the
compare (test the field), the cast on the expression, and the helper's return
type.

### Lever 21. Between two locals of the same class, DECLARATION ORDER is the source property

`V34XF_GetMaxUpstreamRateIndex` was five bytes and grade 1 `ACCEPT`, and the five
bytes were one swap: the object loads the session pointer into `%ecx` and forms the
engine base in `%edx`, ours the other way round.

    object   mov 0x3548(%ebx),%ecx     lea 0x4(%ebx),%edx
    ours     mov 0x3548(%ebx),%edx     lea 0x4(%ebx),%ecx

Nothing about the statements changed it. What changed it was moving
`const VPcmFloModem *sess = ...;` above `struct v34_engine *e = ...;`, i.e. the
order the two locals are DECLARED in. With session first, GCC gives it `%ecx`, the
later engine pointer `%edx`, and the function is byte-exact (34 of 34 instructions,
longest run 34). The twin `VPcmV34GetMaxUpstreamRateIndex` has the same body and
took the same fix, so the pair closed together: census 1116 -> 1118 exact.

The domain was enumerated rather than searched — four orderings of the block's four
declarations:

    e, sess, rate (as written)   BYTES 5    run 34   ACCEPT
    sess, e, rate (landed)       EXACT      run 34   LCS 100.0%
    sess, rate, e                BYTES 19   run 28   LCS 94.1%
    rate, sess, e                BYTES 19   run 27   LCS 94.1%

Only one of the four maps onto the object, and the other three lose six or seven
instructions of contiguous run — so this is a recovered declaration order within
the tested family, not a hill-climb on differing bytes.

**When to reach for it.** A grade 0 `BYTES` verdict with grade 1 `ACCEPT` whose
`--why` row is a register name on two instructions that read two DIFFERENT members
of the same object is the fingerprint: nothing is wrong with the statements, two
live ranges of the same class are competing for one register pair, and the
allocator resolves it in declaration order. Enumerate the permutations of that
declaration block (they are few) and take the one the object matches. Do not do
this where the two locals are of different classes or where the row also changes a
mnemonic or a displacement — that is a different defect and the order will not fix
it.

### Lever 22. A base that survives a CALL forces the FRAME -- and a repeated base offset is a NESTED TYPE

Two findings that arrived together and belong together, because the second is what
makes the first available.

**A. The prologue's shape is about what stays live across a call.**
`VPcmV34InitMOH` was 32 differing bytes and four instructions short. The object
builds two bases and writes progress through the second one AFTER a call:

    sub $0x1c,%esp
    mov %ebx,0xc(%esp)  ... mov %ebp,0x18(%esp)      four mov-to-stack saves
    lea 0x264(%ebx),%esi                             receiver
    lea 0x4(%ebx),%edi                               engine
    call v34handshakinit
    movl $0x7,(%edi)                                 progress, through the engine base

Ours wrote `movl $0x7,0x4(%ebx)` with the full displacement, so nothing had to
survive the call and GCC chose the push prologue (`push` x3 + `sub $0x10`). One
declaration —

    struct v34_engine *e = (struct v34_engine *)(void *)&obj->progress;

— plus `e->progress = 7;` took the function to the object's instruction count
(95 of 95), run 8 -> 18 and LCS 68.4% -> 86.3%, and 32 differing bytes to 6.

**When to reach for it.** Ours is push-based where the object is
`sub $0xNN,%esp` with `mov %reg,K(%esp)` saves. Then look for a field written on
BOTH SIDES of a call through a base: that is what makes the base live across the
call and forces the frame. A base used only before the call, or only after it,
will not do it, and adding a pointer where the object really does push makes the
function worse.

**B. A base formed at a constant offset in many functions is an inner struct.**
A prologue scan for `lea 0xK(%reg)` / `add $0xK,%reg` (`/tmp/opencode/obj4.py`)
found **39 to 51 functions** forming a base at `obj+4`, with a displacement
histogram of `0x248` x30, `0x220` x14, `0x24c` x12, `0x21c` x11 and so on. Adding
the base offset back maps every one of those displacements onto a real member, so
the object's outer type was `{ int status; struct body body; }` and those
functions were reaching the body directly. Modelling the body as a struct and
taking its pointer in each of those functions is what produced `struct v34_engine`
and `struct v34_tx`, and with them the exact symbols `V34XF_IndicateK56FlexRateDetermined`,
`V34SetINFO0dBits`, `V34XF_IndicateJdReceived`, `VPcmV34SetMaxBlockLength`,
`V34XF_GetMaxUpstreamRateIndex` and `VPcmV34GetMaxUpstreamRateIndex`.

Two rules that keep it honest. The FLAT members stay: where the DSP core reads the
same words with full displacements, that spelling is the original too, so the
choice is per FUNCTION and the flat accessor is not wrong. And the base is taken
only where the object forms one — it is a property of the function, not of the
field, and each candidate must be measured (`byteident --why` plus the run and
LCS) rather than assumed from the scan.

### Lever 23. A single scratch register is not evidence -- measure first

`byteident`'s REGALLOC bucket is the tempting one: grade 0 `BYTES` and grade 1
`ACCEPT` means the two sequences are the same instructions with the operands
renamed, so it looks like one register away from an EXACT symbol. It is 34 of
the tree's 1852 symbols, and a census of what the differing register CARRIES
says most of them cannot be moved by source at all.

| what the register carries | symbols |
| --- | --- |
| a value the function discards (a `pop %reg`) | 8: `V92Jd::getJdBitVector`, `Scrambler<h,h>::reset`, `Scrambler<h,i>::reset`, `ParallelDifferentialDecoder<unsigned char>::reset`, `v8_fsktxfilter`, `V90AutoDigitalImpDetector::applyPadGainToLinMapper`, `V90MP::evaluateInfo`, `V90Phase4Modulator::setRdRtSymbols` |
| a constant or a symbol ADDRESS carried into a stack argument slot | 9: `dp_v8_init`, `dp_v8_exit`, `dp_v22_init`, `dp_vpcm_init`, `V90SignBitsExtractor` ctor, `V92Phase3Modulator::generateTRN1u`, `V90Phase3Modulator` ctor (`$0x63`), `V90CP` ctor (`$0x12`), `V92Phase4Modulator::recivedRt` |
| a field loaded, tested and then printed | 6: `V90Phase4Modulator::recivedE2u`/`recivedFirstRrnE2u`/`recivedCPtag`, `V92Phase4Modulator::recivedEd`/`recivedEd`, `RxDetMarkB103` |
| two values swapped between two pointers | 4: `FloatFIR::reset`, `Scrambler<h,h>::process`, `V22_FSE_init`, `_iir_filter_create` (the two ARGUMENTS swap registers) |
| a zero or small constant kept for a store | 4: `V90Parameters` ctor, `V92Jd` ctor, `V90Phase4Modulator::setMappingParams` and its V.92 twin |

**The worked negative is `V90SignBitsExtractor::V90SignBitsExtractor()`**
(`src/pump/v90/V90SignBitsExtractor.cpp:91`), 13 instructions, 2 bytes:

```
object 31931:  push %ebx / mov $0x6,%ecx / sub $0x8,%esp / mov 0x10(%esp),%ebx ...
ours   371:    push %ebx / mov $0x6,%edx / sub $0x8,%esp / mov 0x10(%esp),%ebx ...
```

Instruction for instruction identical otherwise -- the constant is hoisted to
the same place, before the same frame and the same loads. The source is an
empty constructor whose only content is `decoder(V90SBE_DECODER_SIZE)`, so
there is no declaration pair to order (Lever 21), no expression to reshape, and
nothing live across the call (Lever 22A). The 2 bytes are the translation
unit's `-frename-registers` state, not a source property.

Three earlier attempts in this bucket failed the same way: the Jd zero register
(`xor %eax` against `xor %ebx`) was invariant under three further spellings, and
`V8GetMessage`'s field/local pair never reproduced the object.

So: chase a REGALLOC symbol only when the two registers carry **different
members**, when the swapped value **survives a call**, or when the pair is two
locals of the same class. A bare scratch carrier is rejected, and the census
above is the denominator to say so.

### Lever 24. Taking a member's ADDRESS creates a base the object does not have

`setfinalrate` opened with eight locals that were addresses of members:

```c
	short *tx_baud = &obj->ratecfg.baud;
	short *tx_preemp = &obj->ratecfg.preemp;
	const short **tx_scale = &obj->ratecfg.divtab;
	short *tx_carrier = &obj->ratecfg.carrier;
	...
```

and wrote through the pointers (`*tx_baud = 0x960;`). GCC 3.4.2 materialised
**eight** `lea` bases in the prologue and spilled every one of them to
`0x1c..0x34(%esp)`: 194 instructions against the object's 175, longest common
run 4, LCS 52 (26.8%). The object has no bases there at all -- it stores with
full displacements, `0xaa84`, `0xaa8a`, `0xaa90`, `0xaa94`, `0xaa96`, `0xaaa8`,
`0xaaac`, `0xaab0(%ebx)`. Writing the members inline (`obj->ratecfg.baud = 0x960;`)
took the same function to 174 instructions, run 8 and LCS 89 (50.9%) in one
edit, and two further corrections (below) reached 171 instructions and LCS 101
(57.7%).

The fingerprint is a prologue full of `lea` **and** a spilled pointer per
member, with the body storing through `(%reg)` at displacement zero. The object
proves the source did not take the address: a `lea` the object does not have is
a local it did not declare. It is the inverse of Lever 22B -- there we ADDED a
base because the object forms one, here we remove locals because it does not.

Two companion findings from the same function, both worth reusing:

- **A hoist the object does not make.** `unsigned short a9e2 = *(unsigned short
  *)(m + 0xa9e2);` was read once and used twice. The object loads only `0xa9de`
  and `0xa9e0` in its prologue and **re-reads** `0xa9e2` at each use site, which
  is the source reading the field where it is needed. Reading it inline took 174
  to 169 instructions and LCS 50.9% to 56.6%. A local that exists to shorten two
  expressions is visible as a prologue load the object lacks.
- **A rate code is 16-bit.** `int code; int high;` against every object test
  being `cmp $0x2,%ax`, `cmp $0x5,%ax`, `testb $0x4,0xa9de(%ebx)`. `short code;
  short high;` took 169 to 171 and LCS 56.6% to 57.7% -- this is Lever 20's
  shape at a place where it costs two instructions rather than saving them.

Bound: taking an address is legitimate when the pointer is passed to a callee
(`rx_predict(rx, &rx->pred_err_re, ...)`) or genuinely re-seated; the lever
applies where the pointer only abbreviates a store.


### Lever 25. Use unmatched hunks to nominate operations for inspection

Lever 2 says a size difference is "an absence or an extra". That is true of the
verdict and useless as a plan: a function can be short by eighty instructions
because four arms are absent, or because the compiler scheduled the same
statements differently. Measure the SHAPE of the gap before reading any source.

    python3: canonicalise each side (registers -> R, relocation class -> RELOC,
    absolute addresses -> A), align with difflib.SequenceMatcher, then list the
    blob-only hunks sorted by size with the address of each.

Three calibrations from the same day (`/tmp/opencode/missingHunks.py`, which needs
`--start-address=0x%x` and `hi = addr + size`, not `size`):

- `modulatevector` (V.34): blob 820, ours 739, hunks **62, 61, 56, 47, 37, 20,
  19, 18** -- a handful of large blocks separated by long matched runs. That is
  a reason to inspect those arms; hunk size alone does not establish absent
  source.
- `v8_getbit` (V.8): blob 129, ours 121, hunks **21, 17, 8, 6, 5, 4, 3, 2** --
  many small hunks interleaved with matches, and the largest ones are a 21-line
  bit-fetch block whose logic ours has in a different arrangement. That is
  scheduling/if-conversion and register state, the class that has been declined
  every time it was chased (`adaptecho`, `RxHdxTone`, `SetAdaptEqV32`).
- The counter-example that justifies the rule: `setfinalrate` before its fix was
  blob 175 vs ours 194 with run 4 and eight member-pointer `lea`s in the
  prologue -- ONE large cause, visible in the head, and the fix moved LCS
  26.8% -> 57.7%.

One or two large hunks can nominate a source block; many interleaved small
hunks can suggest scheduling or operand differences. Neither pattern decides
whether source recovery is needed. Quote the hunk sizes in the finding either way,
because "SIZE (48 bytes)" alone cannot tell you which of the two you have.

### The hunk list alone over-reports content: PAIR the two sides

Measured 19 Oct 2026 on the V.34 body family, and it changes how the list above
must be read. `missingHunks.py` prints only the BLOB-only hunks, so a merely
REARRANGED block shows up looking exactly like a missing one. Count both sides
instead (`/tmp/opencode/gapshape.py` does: blob-only total, ours-only total,
number of hunks >= 12 and their sum):

| symbol | blob | ours | net | blob-only | ours-only | hunks>=12 | sum | largest |
|---|---|---|---|---|---|---|---|---|
| `modulatevector` | 829 | 748 | -81 | 760 | 679 | 17 | 641 | 112 |
| `decodeDepth` | 529 | 524 | -5 | 461 | 456 | 11 | 371 | 101 |
| `demapFrame` | 416 | 364 | -52 | 358 | 306 | 11 | 284 | 73 |
| `initdigital` | 318 | 304 | -14 | 270 | 256 | 8 | 187 | 62 |
| `getFrame` | 274 | 260 | -14 | 242 | 228 | 8 | 200 | 45 |
| `initV34` | 245 | 239 | -6 | 203 | 197 | 4 | 149 | 86 |
| `initG248` | 86 | 81 | -5 | 67 | 62 | 3 | 48 | 20 |
| `agcadapt` | 72 | 70 | -2 | 27 | 25 | 0 | 0 | 7 |

Every one of them has blob-only ~= ours-only, and a hunk sum that is three to
twenty times the net gap. This suggests investigating arrangement, but equal
hunk totals do not establish that the statements or their operands agree. The 73-
instruction `demapFrame` hunk at `0x594d3`, the largest in that function, is a
loop body whose spilled counter lives in different stack slots on each side --
`0x34(%esp)`/`0x38(%esp)` in the object against ours' own frame -- so nothing is
missing from it.

Print both sides' hunks and pair their operations before classifying a gap.
Similar hunk sizes are a triage clue, not evidence that a source block exists
elsewhere. Compare control flow, operands and value lifetimes as well.

### Even ONE large deletion can be arrangement: test the INSTRUCTION MULTISET

The pairing above compares hunk SIZES, and a single pure deletion in the object
can still be a block ours has somewhere else. The sharper test is the
instruction MULTISET, and it is now a repository tool:

    make contentgap S=SYM            # one symbol
    python3 tools/toolchain/contentgap.py --worst 40

It aligns the two canonicalised instruction lists and reports the surplus --
instructions present more times on one side than the other -- at THREE levels,
because each answers a different question:

    kinds       projected opcode      is an instruction KIND absent after
                                      branch opcodes become classes?
    structure   operands masked       is an instruction absent once constants
                                      and displacements are folded?
    operands    operands kept         how much of the operand text moved?

Measured on the V.34 body family (`kinds` / `structure` / `operands`, blob ours):

| symbol | blob | ours | kinds | structure | operands |
|---|---|---|---|---|---|
| `initdigital` | 292 | 279 | 20 / 7 | 36 / 23 | 45 / 32 |
| `V34SetINFO1aBits` | 326 | 317 | 13 / 4 | 16 / 7 | 36 / 27 |
| `demapFrame` | 397 | 353 | 61 / 17 | 91 / 47 | 150 / 106 |
| `modulatevector` | 807 | 725 | 119 / 37 | 149 / 67 | 324 / 242 |
| `getFrame` | 256 | 253 | 17 / 14 | 25 / 22 | 99 / 96 |
| `initV34` | 235 | 228 | 29 / 22 | 32 / 25 | 65 / 58 |
| `agcadapt` | 71 | 69 | 3 / 1 | 3 / 1 | 3 / 1 |

**A zero proves equality of the projection only.** It cannot establish source
equivalence: register relationships, constants, offsets, branch conditions
and dependent operation order can disappear from the comparison. A NONZERO
count is also a QUESTION, not a verdict: GCC legitimately
substitutes `lea` for `add` and `xor` for `mov $0`, so a kinds surplus can be
codegen with no source difference at all.

**CORRECTION, and the reason this subsection was rewritten the same day.** The
first version of this test (an ad-hoc script, `/tmp/opencode/contentgap.py`)
took only the alignment's `delete` opcodes and ignored `replace`, and on that
measurement `initdigital` reported **0 unmatched from 110 deleted
instructions**, which was read as "the whole family is arrangement". The
production tool includes `replace` and reports kinds 20 / 7 for the same
symbol. Both numbers are internally consistent; they answer different
questions, and the flat claim "every deleted instruction exists elsewhere, so
it is arrangement" was **too strong**. What survives is the narrower rule
above needs further narrowing: neither zero nor surplus proves the source
cause. Both are prompts for operand and control-flow inspection.

**And the surplus list is where the lead usually is.** `initdigital`'s surplus
is `movzwl N(R),R` thirteen times in the blob against `movswl R,R` ten times in
ours -- a zero-extending LOAD against a sign-extending register read. That is
not missing content and not arrangement: it is the Lever 20 shape, a 16-bit
temporary or a signedness choice, and the next pass on this function should
start there rather than at the hunks.

The multiset screen does not close `initdigital`, `V34SetINFO1aBits`,
`demapFrame`, `modulatevector`, `getFrame`, `initV34` or `agcadapt` as
reconstruction targets. Their published structural surpluses are nonzero in
any case. Reopen individual operations only with a native operand, control-flow
or lifetime witness; the screen alone establishes neither a defect nor its
absence.

Three known bad controls now demonstrate this boundary through the actual
canonicalisation pipeline: reversed dependent add/multiply order, a different
value stored to a different member, and an opposite conditional branch. All
have zero structural surplus. The first produces 8 versus 7 at input 3.
`python3 tools/toolchain/contentgap.py --self-test` reports these blind spots
alongside its positive surplus controls; diagnostic output states what remains
unchecked. No grading or normalisation rule is relaxed.

Earlier recoveries in this family used declaration order (Lever 21), a `short`
temporary or return (Lever 20), taking a member's address (Lever 24), and the
frame a base forces (Lever 22). Those results do not exclude further source
control-flow, ownership or arithmetic recovery.

### Lever 26. Test explicit source repetition against compiler duplication

`V90CP::evaluateInfo` had four arms 1,003 bytes short of the object across a
1,024-byte gap. The tested loop representation did not recover those
operations. Under the
retained flags, the tested loop forms did not emit the native repetitions;
explicit longhand copies recovered more of the enclosing graph. This is
per-site source evidence. Repeated final blocks alone do not establish
source repetition: inlining, unswitching, peeling and late block duplication
must be distinguished with compiler-stage controls.

    state   blob   ours   short by
      7      220     80     140     four 9-bit reads, one per nof_58[k]
      8      425    136     289     four per-frame loops, frames 0x58/0x358/0x658/0x958
     11      578    120     458     six guarded per-buffer blocks
     10      385    269     116     KEEPS its loops -- the control case

Writing states 7, 8 and 11 out longhand took ours 275 -> 483 instructions
against the object's 510 and the grade 0 difference from `SIZE (925)` to
`SIZE (131)` (LCS 309, 60.6%). State 10 is the control that proves the rule is
per-site evidence and not a policy: the object kept its loops there, and
unrolling it as well overshot to 572 instructions (a 1,139-byte arm against
511). A local accumulator is also NOT the source shape: `n = (n << 1) | bit`
in a loop over the nine bits gives 479 instructions but a WORSE LCS (301).

Fingerprint: an arm that is a fixed multiple of a smaller arm's size, with the
same instruction multiset repeated and the cursor advanced by a constant
between copies. Read the arm extents from the JUMP TABLE, not from byte deltas
between neighbouring targets (block placement reorders those).

**And gate it on the affected fixtures FIRST.** The first full run of this
change failed 392/2 because three of state 7's four blocks had lost their
`p += 0x11;` and re-read the same nine bits: `t_v90cpinfo` and `t_v90cpb2i`
reported `short_58[1][0]` reading `ff ff` against `00 00`. A hand-unrolled
block sequence must keep the cursor advance in EVERY block, and
`make period T=<test> J=8` on the two fixtures that read the field is a
95-second check that the full eight-minute gate would only find later.

### Lever 28. A `? :` does NOT if-convert — a conditional assignment must name the variable itself

When the object has a branchless mask

    cmp  %bp,%ax
    setl %cl
    neg  %ecx
    and  %eax,%ecx

and ours has the branch (`cmp; jl; xor`), the residual is not allocation: the
source form decides whether `if_convert` can fire at all.

`noce_try_store_flag_mask` (`ifcvt.c`, the LAST of the store-flag tries in
`if_convert`) is entered only when

    (!no_new_pseudos
     && (BRANCH_COST >= 2 || STORE_FLAG_VALUE == -1)
     && ((if_info->a == const0_rtx
          && rtx_equal_p (if_info->b, if_info->x))
         || ((reversep = reversed_comparison_code (if_info->cond, if_info->insn)) != UNKNOWN
             && if_info->b == const0_rtx
             && rtx_equal_p (if_info->a, if_info->x))))

so the conditional assignment's TARGET has to BE one of the two arms. A
`COND_EXPR` (`c ? a : b`) reaches the backend through a TEMPORARY: `if_info->x`
is the temporary and neither arm is `rtx_equal_p` with it, so the branch
survives no matter how the test is spelled. Written as an `if` statement that
assigns the variable itself, the untouched arm IS `x` and the mask is emitted.

Both senses work, because the two alternatives cover them: `if (c) x = 0;`
matches the first (`a == const0`, `b == x`), `if (!c) x = 0;` the second (via
`reversed_comparison_code`). What does NOT work is any `?:` spelling —
`x = c ? x : 0` and `x = c ? 0 : x` both fail, which is what makes this a test
of the STATEMENT FORM and not of the comparison's direction.

Measured on one shared helper, `(widx + 1) mod len`, inlined at eleven sites
across five translation units (F12153). Total canonical multiset symmetric
difference, baseline -> `if` form:

    SMCv17_encoder_dif    17 -> 15        TxNoCarrierV17   41 -> 19
    SMCv17_encoder_abs    26 -> 24        TxNoCarrierV29   26 -> 17
    SMCv17_encoder_tcm    15 -> 11        TxNoCarrierV32   40 -> 35
    FPM_MRF_filter        61 -> 51        TxNoCarrierV27   23 -> 23  (not a site)

**Fingerprint.** Count the object's `setcc` before theorising:
`objdump -d --disassemble=SYM ref/slmodemd/dsplibs.o | grep -c 'setl'`. Here the
object held one `setl` at every inlining site and our build held none anywhere,
which both localises the difference and proves the direction.

**Screen the whole corpus, and read BOTH directions.**
`scratch/cd/setcc.py <census.json> [--all]` counts every setcc mnemonic per
symbol, reference against candidate — the generalisation of the one-line
`grep`. Its control (all 1237 EXACT rows must agree) has already caught an
inverted control message in the screen itself, so do not skip `--all`.

79 non-EXACT rows differ, and the SIGN is the diagnosis:

- `ours < reference`: the object masks and we branch.  Our source holds a
  conditional expression where the original had an `if` statement — this
  lever.  32 rows.
- `ours > reference`: we mask and the object branches.  Our source holds an
  `if` statement where the original must have held a conditional expression
  (or some other form that does not if-convert).  47 rows, not yet worked.
  The largest are `V8.c rebuildJMSequence` (48 against 26), `dtmf_test`,
  `V8.c initTxSequence`, `CALLPROG_Progress`, `VTB_decoder`, `CID_MTD_detect`.

Then find the remaining sites directly in the source rather than in the
objects: `grep -rn "? next : 0\|? 0 : next\|< limit ?\|< len ?" src/`.  Two
in-place shapes both if-convert and both match the object, so use whichever
the source already affords:

    x = (short)(next < lim ? next : 0);     ->  if (next >= lim) next = 0;
                                                x = next;
    x = (short)(x + 1 < lim ? x + 1 : 0);   ->  x = (short)(x + 1);
                                                if (x >= lim) x = 0;

Applied corpus-wide (F12154) this closed five more sites in three files
(`V32SMC_TX.c`'s `ring_advance` inlined twice, `fpm_pps.c`, and four in
`fpm_ecc.c`), taking `FPM_PPS_filter` from -15 bytes to 0 and
`FPM_ECC_cancel`'s symmetric difference from 105 to 98.

**Expect the candidate to grow.** The mask build costs about two instructions
more than the branch it replaces, so a correct conversion moves the candidate
AWAY from a smaller object. If the object's total is smaller than even the
pre-fix candidate, the conversion is still right and there is a further,
separate source difference in that function: `FPM_MRF_filter` is 167 against
our 168 before the fix and 172 after, so its size row went 0 -> +7 while its
symmetric difference fell 61 -> 51. Record that trade; do not undo the
conversion to protect a byte count.

#### Addendum: when the arm TRANSFORMS the variable, hoist the transform

`noce_try_store_flag_mask` needs `if_info->x` itself to be one of the arms, so
a conditional expression whose kept arm is a *computation* of the variable
cannot reach it:

    i = (fifo_size > i + 1) ? i + 1 : 0;     // never a mask -- the arm is `i+1`

Hoist the computation onto the variable and the arm becomes the variable:

    i = i + 1;
    if ((int)fifo_size <= (int)i)
	i = 0;                                  // -> xor; cmp; setg; neg; and

Same for the constant form (`small = 2 - (cond ? 1 : 0)`, F12162): the `2 -`
has to be applied to the materialised 0/1 rather than distributed over the
ternary.  The tell in the object is `setcc` immediately followed by
`neg`+`and` whose `and` source is a register computed just before the compare.

### Lever 29. A `&&`/`||` chain of comparisons is CONTROL FLOW; the object's mask chain is a value

The partner of Lever 28, and the same screen.  Count `setcc` per symbol and read
BOTH directions (`scratch/cd/setcc.py <census.json> [--all]`).  Where we have
ZERO and the object has some, the original evaluated the predicate **as a
value** and our source left it as control flow:

| our source | what GCC emits | the object wants |
|---|---|---|
| `a && b && c` | short-circuit `jb`/`jae` chain | `setcc` per comparison, ANDed |
| `x = c ? 1 : 0;` | branch (Lever 28) | `setcc` |
| `!(a && b)` | short-circuit | `test; sete` |

Two spellings reach the object, and which one is original is not always
decidable from the bytes:

    /* 1. bitwise: every operand is a value, so every comparison is masked */
    ok = (a >= b) & (c >= d) & (e >= f);

    /* 2. the flag lives in a variable, so `!` sees a value and cannot
          short-circuit; `&&` is fine here because the value is needed */
    int ok = (a >= b) && (c >= d) && (e >= f);
    return (short)!ok;

Measured: `dtmf_test` (five float comparisons) went from four `jb` branches to
the object's five `setae` with the same branch sequence once the chain was
written `&`; `CID_MTD_detect` reproduced the object's `seta`/`seta`/`test`/`sete`
tail only once the flag was materialised in an `int`.

**`!` is the trap.**  `!(A & B)` never keeps the `&`: `fold` rewrites it to
`(!A | !B)` and the comparisons come out as `setbe` + `or`.  Measured on
`CID_MTD_detect`: `!(A & B)` scores WORSE (symmetric difference 16) than the
untouched baseline (15).  Materialise the flag first (`int ok = A & B;` then
`return !ok;`) and the `&` survives.

**Operand order is a second, separable lever.**  The object's own evaluation
order is visible in the mask order and in which comparison is recomputed at the
end; with `CID_MTD_detect`'s order (`wide / 2 > narrow` first) the symmetric
difference is 7 against 8 for the other order.

**Two other spellings, both measured, both rejected for `dtmf_test`:** an
`ok = 1;` followed by `if (!(c)) ok = 0;` chain (`noce_try_store_flag_mask`
gives the object's `x = x & -(cond)` accumulator, `neg` and all) scores the same
28 but overshoots the byte count the other way (173 against the object's 170),
because our build folds the first two comparisons into a plain `and` before
if-conversion runs.  Both `&` (167) and the `if` chain (173) straddle 170, so
the accumulator form alone does not decide it.

**A materialised loop test often carries a SIGNEDNESS that the loop's neighbours
do not.**  `initTxSequence`'s `emit_extension` (`src/v8/V8.c`) inlines twice, and
each inlining carries the object's

    test %cl,%cl ; setne %dl ; xor %eax,%eax ; cmp $0x3,%k ; setbe %al
    test %eax,%edx ; jne

while the same `k <= V8_CM_EXT_MAX - 1` bound in that file's other eight loops
emits `setle` -- signed -- in both the object and our build.  So the counter is
unsigned *in that helper*, and `setbe` against `$0x3` is what says so.  Measured
on the complete TU: `int k` gives `setle`; `unsigned int k`, `(unsigned)k <=
V8_CM_EXT_MAX - 1` and `(unsigned)k < V8_CM_EXT_MAX` all give one identical
object with `setbe` (GCC canonicalises the last to `k <= 3`).  Read the
comparison, not the bound expression, when the two disagree across a file.
`F12157`.

#### Caution: test each chain separately

Four short-circuit chains in the same function are not four instances of the
same source property.  Measured on `faxvmi_hdlc_unframe` (F12163): converting
all four dropped the canonical match from 88 to 98; tested one at a time only
one (line 266) improved it (88 -> 84) and the other three regressed or were
inert (271 -> 94, 254 -> 90, 245 -> unchanged).  Convert a chain when the
OBJECT's bytes show a materialised comparison combined by `test`; an `&&`
adjacent to a converted one is not evidence for itself.

#### Addendum: the loop test, and how to FIND these sites

The same rule applies to a loop condition.  `while (v92me_mul(a, b >> n) > 0
&& n <= 7)` compiled to a branch; the object's back edge materialises both
halves (`cmpl $0x7,%n ; setle %bl` beside the product's sign, then
`test %al,%bl`) -- so the source was `&`.  One operator, 93 bytes and 18
canonical entries (F12165).

Finding them does not need the object: **`scratch/cd/setcc.py <census.json>`
lists, per symbol, the `setcc` multiset of the reference against ours, and a
NEGATIVE difference means the object masks where our build branches.**  Sort by
that difference and read each symbol's source for a `&&`, a `||` or a `?:`
whose operands are comparisons.  Then check the object's bytes at the site --
it will show two `setcc`s and a `test`, which is the confirmation.

Two traps, both measured:

- `bool ok = (a && b);` / `(a || b)` written as a VALUE does not help if the
  operands fold; and `!(a & b)` NEVER keeps the `&` -- `fold` rewrites it to
  `(!a | !b)` and the `setcc` names come out inverted (F12155).  Materialise
  first, then negate.
- The reverse direction is real too: an object that BRANCHES where we mask
  means our source has a `&` (or a hand-written mask) the original did not, or
  a `?:` that should be an `if`.  Both directions are in the same screen; the
  sign of the difference is the diagnosis.

### Lever 30. A conditional MOVE needs the value MATERIALISED -- `x += c ? A : B` distributes into a branch

Lever 28's counterweight, and the same screen and direction
(`scratch/cd/setcc.py <census.json>`; here `ours < reference`, usually
`ours == 0`).  GCC has two unrelated ways to make a conditional assignment
branchless, and they need opposite source shapes:

- `ifcvt`'s `noce_try_store_flag_mask` -- Lever 28 -- needs the assignment's
  TARGET to be one of the arms, so it wants an `if` statement.
- the i386 `mov<mode>cc` pattern, reached whenever a `COND_EXPR` result is a
  VALUE.  `ix86_expand_int_movcc` (config/i386/i386.c) expands it without cmov
  when BOTH arms are `CONST_INT`:
  - sign-bit and carry-flag compares take the `sbb` path;
  - `diff = ct - cf` in {1, 2, 3, 4, 5, 8, 9} takes the LEA path --
    `setcc; lea cf(,dest,(diff & ~1)) [; add dest]`;
  - otherwise the general case -- `xor; cmp; setcc; dec; and (cf-ct) [; add ct]`.

The trap: `base += (rq > 0) ? 0x00 : 0x18;` NEVER reaches the movcc.  The front
end distributes the `+` into the arms (`(rq>0) ? base+0x00 : base+0x18`), the
RTL is a diamond, and `noce_try_addcc` cannot fold it (`emit_conditional_add`
has no i386 pattern and its setcc fallback needs an addend of +-1).  The result
is a branch.  Materialising the value first keeps the movcc:

    int q = ((short)rq > 0) ? 0x00 : 0x18;
    base += q;

gives the object's `xor %eax,%eax; test %bx,%bx; setg %al; dec %eax;
and $0x18,%eax`, and the other arm gives `setle %cl; lea 0x8(,%ecx,8)`.

The same rule applies to a boolean MULTIPLY.  `cq + (cq > ci) * grid` folds to
`cq + (cq>ci ? grid : 0)` and then distributes into a branch; storing the
comparison in a variable first keeps the real multiply:

    int sel = (cq > ci);
    idx = (short)(cq + sel * grid);

emits `xor; cmp; setg; imul; add` -- which no ternary, no if, and no inline
multiply spelling reproduces (all measured).

Fingerprints to grep for in the object: a `setcc` followed by `dec` and then
`and $C`, a `setcc` followed by `imul`, and a `setcc` followed by
`lea C(,dest,scale)`.  Measured on `VTB_decoder` (src/dsp/fpm_vtb.c): our
baseline held ZERO `setcc` against the object's three (`setg`, `setg`,
`setle`); after materialising both values our build reproduces all three sites
instruction for instruction.  Expect the candidate to GROW in bytes while the
canonical multiset improves -- the same trade as Lever 28, and the reason the
census `size` row is not the metric to optimise.

### Lever 31. A cached pointer local the object does not have is RESIDUE: it spills and blocks addressing-mode folding

A local that names an address the reference folds is not harmless.  `struct T
*p = &v->arr[i];` gives the compiler a value it must keep somewhere; if the
function calls anything, that value spills and every later use becomes a load
of the pointer followed by a load through it.  The reference, written with the
expression inline, folds the constant offset into the operand instead.

The fingerprint is a base-pointer RELOAD.  Compare the two objects for
`mov 0xNN(%esp),%r32` (a spilled pointer) and `mov 0xNN(%r32),%r32` (a pointer
re-derived from a live root).  `evaluateRxJMSequence` (src/v8/V8.c) held
`mov 0x20(%esp),%r32` twenty-one times where the reference held NONE -- and the
same accesses in the reference were single folded operands,
`movzwl 0xcd4(%r32,%r32,2),%r32`, i.e. `&v->seq[2].word[i]` with the offset as
a displacement.

Check the declaration list first.  An *unused* local is pure residue: the
declaration is evidence the original had a name there, but if no statement in
the function uses it, the accesses elsewhere were meant to be spelled inline.
`evaluateRxJMSequence` declared `struct v8_tx_sequence *seq = &v->seq[2];` and
`struct v8_cm *cm = v->cm;` and then used NEITHER -- all seven word accesses
were `seq->word[...]` and all twelve cm accesses were `v->cm->...`.

Measure the two halves separately, because they are independent:

| spelling | instructions | canonical symmetric difference | byte delta |
|---|---|---|---|
| `v->cm->` + `seq->` (baseline) | 256 | 86 | +61 |
| `cm->` + `seq->` | 246 | 76 | -- |
| `v->cm->` + `v->seq[2].` | 226 | 40 | -34 |
| `cm->` + `v->seq[2].` | 222 | 36 | -- |

The last row scores four units better on the aggregate, and it is the WRONG
answer for this function: it makes the compiler cache `cm` in a register, and
the object demonstrably re-derives it (`mov 0xa58(%r32),%r32`, the `v->cm`
field, seven times more than the cached build).  **When two spellings differ,
decide with the native reload pattern, not with the aggregate score** -- the
aggregate is a sum over instructions whose weights are not equal.

Adopted: drop both unused locals and spell the accesses inline
(`v->cm->...`, `v->seq[2].word[i]`).  Result `evaluateRxJMSequence`: candidate
1039 -> 944 bytes, byte delta +61 -> -34, longest run 15/244 -> 29/244,
symmetric difference 86 -> 40, exactly one census row moved and no grade
changed.

### Lever 31 addendum: a cached POINTER the object re-SELECTS is residue too

Lever 31 is about a local naming an address the reference folds.  The same rule
runs the other way when the local is a SELECTION:

    const short *taps = v->side != 0 ? ANSWER_Entrance_Filter
                                     : CALLER_Entrance_Filter;

hoisted above the prologue call is a value we keep once; the reference evaluates
it per loop iteration and spills the choice (`test; je; mov ANSWER; mov
%r32,0x10(%esp)`, reloaded inside the loop).  Measured on `V8agc` (F12196):
moving the declaration into the loop body took the byte delta -72 -> -23.

The tell is a ternary feeding a spilled slot that the object reloads per
iteration.  Do not confuse it with lever 31's original direction: there the
LOCAL is extra, here the local is right but its SCOPE is wrong.

### Lever 8/41 refinement: the truncated countdown is a FORM, not just a TYPE

Levers 8 and 41 say a value's or a counter's declared width is forced by the
object's own extend/compare.  For a loop counter that is only half of it.  GCC
3.4.2 emits the truncating countdown

    lea -0x1(%r32),%r32 | inc %r16 | jne <T>          (or movzwl/movswl between)

ONLY for a countdown written as

    T x = n;
    while (x--) { ... }

with `T` 16-bit.  Measured:

| source | emitted |
|---|---|
| `short i; for (i = n; i != 0; i--)` | `test`/`jne`, no truncating extend |
| `int i; for (i = 0; i < n; i++)` | `inc %r32; cmp %r32,%r32` |
| `unsigned short i; i = n; while (i--)` | the object's `lea; movzwl; inc %r16; jne` |

So when the object shows the countdown, the source is the `while (x--)` form,
and the object's extend names the signedness (`movzwl` + `jne` = unsigned,
`movswl` = signed).  Recovered three symbols from one screen
(`scratch/cd/loop16.py`, 33 rows) in F12199.

### Lever 50. The prologue SAVE-FORM is a step function of `nregs` -- read the predicate, it is not a spelling

Under `-maccumulate-outgoing-args` the i386 prologue saves callee-saved
registers one of two ways, and the choice is NOT stylistic:

    PUSH    : push %ebp; push %edi; push %esi; push %ebx; sub $0xN,%esp
    MOVSAVE : sub $0xN,%esp; mov %edi,K(%esp); mov %esi,K(%esp); mov %ebx,K(%esp)

Screen it with `python3 scratch/cd/prologue.py <census.json>` (90 mismatches in
this tree, 10 of them reference-MOVSAVE / ours-PUSH -- the corpus catalogue's
most repeated unresolved idiom, 30-50 bodies).

The predicate, verbatim from the extracted GCC 3.4.2 tree:

    config/i386/i386.c:5014-5018
      if (TARGET_PROLOGUE_USING_MOVE && cfun->machine->use_fast_prologue_epilogue)
        frame->save_regs_using_mov = true; else frame->save_regs_using_mov = false;

    config/i386/i386.c:4988-5013     -- use_fast_prologue_epilogue
      count = frame->nregs; if (count) count = (count - 1) * FAST_PROLOGUE_INSN_COUNT;
      false when function_frequency < NORMAL, else !expensive_function_p(count);
      FAST_PROLOGUE_INSN_COUNT = 20 (i386.c:531)

    predict.c:1071-1102              -- expensive_function_p
      limit = ENTRY_BLOCK_PTR->frequency * threshold;
      for each bb, for each active_insn_p insn:  sum += bb->frequency;
      return sum > limit;

The period line has no profile flags, so `function_frequency` stays `NORMAL`
(`predict.c:1213-1214`, `function.c:6429`) and the whole thing collapses to

    MOVSAVE  <=>  SUM_b (active_insns_b * freq_b)  <=  ENTRY_freq * (nregs-1) * 20

Two consequences worth holding:

1. **`-mtune=i686` does NOT disable the mov-save form.**  `TARGET_PROLOGUE_USING_MOVE
   = x86_prologue_using_move & TUNEMASK` (`i386.c:508`), the set is
   `m_ATHLON_K8|m_PENT4|m_PPRO`, and i686 maps to `PROCESSOR_PENTIUMPRO`
   (`i386.c:1101`) -- so it is TRUE.  `-maccumulate-outgoing-args` only feeds
   `to_allocate` and cannot switch the form.
2. **It is a STEP FUNCTION of `nregs`** (2 -> threshold 20, 3 -> 40, 4 -> 60)
   and a knife-edge on the weighted instruction sum.  `nregs` moves only
   through whole-TU register allocation -- i.e. a value whose LIVE RANGE SPANS
   A CALL.  So the impulse is "keep one more value live across a call" (or, in
   the other direction, stop caching a value the object re-derives), and the
   inert directions are every spelling that does not change a live range.

Recovered `enterFPE` this way: the reference keeps its return value in `%edi`
across the body (one common exit, `int ret = 0; if (state != S) { body }
return ret;`), `nregs` 2 -> 3, delta -28 -> 0, prologue reproduced
byte-for-byte.

**CAUTION -- the neighbour trap.**  The same edit on the twin `enterRRN` is
also a win for that symbol but flips the EXACT neighbour
`enterChannelVerification` to NONMATCH through the TU's allocation state.  Only
`enterFPE` is retained.  **After any single-function edit, re-grade EVERY
symbol of the TU** -- a delegated "no neighbour regression" is a claim, not a
measurement.

### Lever 31 addendum: the object CACHES a field read our source re-reads

The mirror of lever 31's original direction.  When the object loads a field ONCE
before a region and keeps it in a register across it, while our source names the
field inside the region, the fix is a local at the object's load point.

`enterFPE`/`enterRRN` (F12201): the reference reads `mmxMode` (offset 0xb0)
immediately after `state = ...` and caches it across two clearing loops
(`mov 0xb0(%esi),%ebx` before the German-PBX test, used after it), reloading it
only for the later restore arm because the setters intervene.  Our source named
`mmxMode` inside the test.  `int mmx = mmxMode;` before the test, `mmx` inside
it, `mmxMode` still read in the restore arm: run 38 -> 40 on `enterFPE` and
23 -> 40 on `enterRRN`.

**CAUTION -- fan-out by pattern is not free.**  The same pattern appears a third
time (`enterPhase4`, line 1036) and hoisting THERE regressed that function's run
19 -> 14.  Each site needs its own native witness: read the object at that site,
do not extend a fix because the source text matches.

### Lever 31 addendum: the SAME file can carry the defect twice -- sweep a file, not a function

F12158 removed `struct v8_tx_sequence *seq = &v->seq[2];` and
`struct v8_cm *cm = v->cm;` from `evaluateRxJMSequence` (src/v8/V8.c) and gained
a large delta.  The SAME pair of cached locals sat uncorrected at the top of
`initTxSequence` in the same file; removing them (F12202) took it from delta
-120 to **-14** -- 106 bytes and 22 instructions, the largest single recovery of
the session.

So after fixing this defect in one function, grep the whole FILE for a cached
pointer whose target the object re-reads:

    grep -nE "^\s*struct [a-z_]+ \*[a-z_]+ = (v|s|st|ctx|modem|obj)->" src/<file>.c

and check each one against the object (does the reference re-load the root
before each use?).  A file-level sweep is cheap; a function-level one leaves the
twins behind.

### Lever 50 addendum: the threshold has TWO sides -- when nregs is equal, the weighted SUM decides

Lever 50's predicate is

    MOVSAVE  <=>  SUM_b (active_insns_b * freq_b)  <=  ENTRY_freq * (nregs-1) * 20

The first three rows recovered with it moved `nregs` (a live range spanning a
call).  `dp_wrapper_create` shows the OTHER side: `nregs` was already 4 on both
sides, and the form still differed because our build's weighted sum was
**606898 against a 600000 limit** -- 1 % over.  Two separate `if` guards each
materialised a shared return value before their branch; the reference had one
combined guard and materialised the value once.  Folding the guards dropped the
sum under the limit, flipped PUSH -> MOVSAVE, and made the object
**byte-identical** (694 = 694 bytes, 178/178 instructions, symdiff 0).

So when you have a reference-MOVSAVE / ours-PUSH row and `nregs` is EQUAL on
both sides, do not stop: count the weighted instruction mix.  The impulse is to
make the HOT blocks hold FEWER instructions -- fold duplicate guards, share a
tail, materialise a constant once instead of twice.  It is the counter-intuitive
direction (making the code simpler in a hot block moves the save form), and it
is measurable: `scratch/cd/prologue.py` classifies the form, and the agent
recipe is to compute the sum before and after the candidate edit.

### Lever 32. A hand-written mask (`x & -(cond)`) does NOT if-convert — only a conditional ASSIGNMENT does

The corollary of lever 28, and the reason a `?:`-free source can still miss the
branchless form.  `noce_try_store_flag_mask` (ifcvt.c:1041) is reached through
`if_convert`'s store analysis, which needs an `if_info` — a conditional
STORE.  Written as a value:

    return idx & -(int)((unsigned)len > (unsigned)idx);

the mask is a `COND_EXPR` on a temporary, no `if_info` is built, and the
comparison stays a `ja` branch.  Written as a statement whose target is one of
the arms, the mask is emitted.  Measured on `bulk_next` (src/pump/v34/V34TX.c,
inlined 3x into `txmit`), complete-TU, reading the object's `setcc` multiset:

| spelling | setcc emitted |
|---|---|
| `idx & -(int)((unsigned)len > (unsigned)idx)` | (none — a `ja` branch) |
| `if (idx >= len) idx = 0;` | `setl` |
| `if ((unsigned)idx >= (unsigned)len) idx = 0;` | `setb` |
| `if (!((unsigned)len > (unsigned)idx)) idx = 0;` | `seta` |
| `if ((unsigned)len <= (unsigned)idx) idx = 0;` | `seta` (identical object) |
| `unsigned u = (unsigned)idx; if (u >= (unsigned)len) idx = 0;` | `seta` (identical) |

Two separable facts fall out.  First, the SIGNEDNESS is a second property: the
object compares UNSIGNED (`seta`), and all three unsigned spellings are
byte-identical to each other, so the source can be recovered from the object's
`setcc` alone.  Second, a hand-written mask is a plausible-looking source form
that is nevertheless wrong — an earlier reconstruction wrote exactly that
because the object showed the `and`, and it cost a branch.  Before believing a
mask, check the object for `setcc`; before believing a `setcc`, check its
signedness against the same comparison elsewhere in the file.

Companion case (same batch, F12159): the `&&` of lever 29 needs its operands
MATERIALISED, and a bare `short` operand promotes back into the arithmetic
unless it is compared explicitly — `allocated & (t->fir_len > 0)` emits no
`setcc`, `(allocated != 0) & (t->fir_len > 0)` emits the object's
`setne`/`setg` pair.

### Lever 33. `(cond) << K` folds back into a ternary; a named local keeps the `setcc`

The object computed a flag as a shift of a 0/1 comparison -- `sete %al ;
shl $0x6,%al` -- and the obvious reconstruction (`cond ? K : 0`) produced a
BRANCH instead.  The reason is a front-end fold, and it defeats the obvious
attempts to spell the shift too:

| source | result |
|---|---|
| `cond ? K : 0` | branch |
| `(cond) << K` inline | identical object (folds to the ternary) |
| `(cond) * K` inline | identical object |
| `(unsigned char)((cond) << K)` inline | identical object |
| `int b = (cond); ... (b << K)` | **`sete` + `shl`** |

`fold` rewrites `(cond) << K` and `(cond) * K` into `cond ? K : 0`, after which
the movcc/if-conversion path decides the shape.  A NAMED LOCAL holding the 0/1
comparison blocks the fold, the shift survives, and the `setcc` is emitted.

Practical consequences.  First, if the object shows `setcc` followed by a shift
or an imul, the source almost certainly holds the 0/1 in a variable -- an
inline shift/multiply of the comparison will NOT reproduce it, however many
times you re-spell it (measured: four distinct inline spellings, one object).
Second, the local must be a wide integer for the best allocation: an
`unsigned char` local still emits the `setcc` but scores worse.  Third, a
`cond ? K : 0` ternary is the WRONG hypothesis at such a site even though it
looks equivalent -- the i386 movcc for a two-constant `COND_EXPR` emits
`xor; cmp; setcc; dec; and (cf-ct) [; add ct]`, which contains no `shl` at all,
so the shift in the object rules the ternary out before you compile anything.

Recorded as F12160 (`DemodDataV32`, src/pump/v32/V32int.c).  The remaining rows
there are allocation: the object loads the compared field into a register and
tests it where ours compares memory, and it then shifts the byte where ours
shifts the word.

#### Addendum: the same fold catches `C - (cond ? 1 : 0)`

`(cond) << K` is not the only cheap fold.  `2 - (cond ? 1 : 0)` folds to
`cond ? 1 : 2` -- a two-constant `COND_EXPR` -- and that takes the i386
`movcc` sign-flag path (`cmp; sbb %edx,%edx; add $0x2`), so the comparison's
VALUE again never survives and there is no `setcc`.  The object wants
`cmpw $0x8 ; setbe %al ; sub %eax,%ecx`, i.e. a plain subtraction of a
materialised 0/1.  Named local, again:

    int g8 = ((unsigned short)s->group_count < 9);
    small = 2 - g8;

Measured on `putFrame` (F12162): setcc none -> `setbe`, canonical symmetric
difference 31 -> 24, `|byte delta|` 7 -> 3.

**A CAUTION about the conservative run.**  With the local alone the object
scored *worse* on `byteident`'s longest contiguous run (17 -> 14 of 162), and
swapping two same-typed local declarations (`int small_last` before
`int small`, lever 21) restored it to 17 while also improving the symmetric
difference 25 -> 24.  A run regression that follows a correct source fix is not
by itself evidence against the fix; check the neighbouring declarations
before rejecting it.

### Lever 34. The mask needs its LIMIT to be a DECL too — and screen by SHAPE, not by names

Two addenda to levers 28/29/33, both from the same site.

**1. A named limit stops the operand swap.**  Since F12151 we know
`tree_swap_operands_p` (fold-const.c) swaps a comparison whose right operand is
not a `DECL_P` when its left one is:

    if (DECL_P (arg1)) return 0;
    if (DECL_P (arg0)) return 1;

That rule applies to the compare INSIDE a mask or a movcc, not just to one that
becomes a branch -- and its effect there is not a different branch but a
different `setcc` NAME for the same predicate:

| source | emitted |
|---|---|
| `idx = (short)(idx + 1); if (idx >= 2 * lag) idx = 0;` | `cmp %eax,0x1c(%esp); setg %bl` |
| `int wrap = 2 * lag;` then `if (idx >= wrap) idx = 0;` | `cmp 0x1c(%esp),%eax; setl %bl` |

Same truth value, both branchless, so **no differential can see the
difference** -- only the object's `setcc` name can.  When the object's mask
uses `setl` and yours uses `setg`, the fix is a named local for the right-hand
side, not a different comparison operator.  (Measured repeatedly this session:
FPM_TONE_find_rev's wrap, FPM_TONE_detect's tail threshold, and `isAltRbs` in
F12151.)

**2. Screen by the shape, not by the identifier names.**  The sweep that found
the first seven sites (F12154) grepped for `limit` and `len` by name:

    grep -rn "? next : 0\|? 0 : next\|< limit ?\|< len ?" src/

and therefore missed `((short)(idx + 1) < taps) ? ... : 0`,
`< 2 * lag ?`, `(rd < size) ? rd : 0`.  Five more sites, all still spelled as
`?:` because the author had *seen* the object's `setl/neg/and` and hand-written
the branchless form -- which is worse than the `if`, not better.  Use

    grep -rnE '\?[^:]*: *0;' src/ --include=*.c --include=*.cpp

and read every hit whose condition is a comparison.  A hand-written
`x & -(cond)` or a `?:` at such a site is a reconstruction error, not a
faithful copy.

### Lever 35. With no ELSE, ifcvt takes the kept value from the instruction BEFORE the compare — assign the VARIABLE, not a copy

The third sub-rule of lever 28, and the one that took two symbols to EXACT
(F12164).  `noce_process_if_block` (ifcvt.c:1804) documents the shape it
handles as

    (3) if (...) x = a;   // as if with an initial x = x.

With no else block it does not invent that initial assignment -- it reaches
into the instruction stream (ifcvt.c:1882-1901):

    insn_b = prev_nonnote_insn (if_info.cond_earliest);
    if (! insn_b || GET_CODE (insn_b) != INSN
        || (set_b = single_set (insn_b)) == NULL_RTX
        || ! rtx_equal_p (x, SET_DEST (set_b)) ... )
      insn_b = set_b = NULL_RTX;

and then (ifcvt.c:1917)

    b = (set_b ? SET_SRC (set_b) : x);

`noce_try_store_flag_mask` wants `a == const0 && rtx_equal_p (b, x)`.  So a
COPY of the source operand sitting immediately before the compare is fatal:

| source | b | mask? |
|---|---|---|
| `unsigned int c = (unsigned int)which; if (which >= 6) c = 0;` | `which` | NO -- a branch |
| `int c = which; if (c >= 6) c = 0;` | `which` | NO |
| `if (which >= 6) which = 0;` | `x` itself | **YES** -- `setl %al; neg %eax; and %ebp,%eax` |

The copy is the natural way to write it when the source variable is a
parameter you do not want to disturb -- and it is exactly the mistake to look
for.  When the object shows a mask on a variable and the source clamps through
a local, ask whether the clamp can assign the ORIGINAL variable; parameters are
locals, so it usually can, and the extra local is the tell.

Recipe at the site: the compare in the object names the variable the mask ANDs
(`and %ebp,%eax` with `cmp $0x6,%ebp`), so the variable that reaches the mask is
the one whose value must survive — not a copy initialised from it.

### Lever 36. An if/else that only chooses between constant VALUES is two conditional expressions

When the arms of an if/else assign the SAME variables from the SAME predicate,
and each assignment chooses between constants, the object will hold no branch
at all — and the SCREEN is the `setcc` count:

    action = (in[0] == 0) ? V90SBE_INVERT_ODD : V90SBE_INVERT_EVEN;   /* 3 : 2 */
    state  = (in[0] == 0);

Against

    if (in[0] == 0) { action = V90SBE_INVERT_ODD; state = 1; }
    else            { action = V90SBE_INVERT_EVEN; state = 0; }

the object has four `setcc` (`cmp $0x1,%dl; sbb %ebx,%ebx; not %ebx;
lea 0x3(%ebx),%edi` for the action, `test %dl,%dl; sete %cl` for the state) and
the if/else has three — it branches and loses both.  The two mechanisms are
already known separately: the action's two constants are lever 30 (a ternary
reaches `ix86_expand_int_movcc`, an if statement does not), and the state's 0/1
is the predicate itself, materialised.

**The screen, in order:** (1) `scratch/cd/setcc.py <census.json>` — a NEGATIVE
difference means the object masks where we branch; (2) in that symbol's source,
look for an if/else whose two arms assign the same set of variables; (3) if each
assignment is `x = K1` / `x = K2`, rewrite it as `x = (cond) ? K1 : K2` — or, for
`1`/`0`, as `x = (cond)`.

**A near-zero byte delta is not evidence the body is right.**  The if/else
baseline sat one byte from the reference with a canonical symmetric difference of
22 and a run of 60/142; the correct source moved the delta to -16 while halving
the symmetric difference (22 -> 7) and lifting the run to 73/142.  Two wrong
things can cancel in the size.

### Lever 37. The object RE-COMPARES a boolean it already has — the source normalised it

The same family as levers 34/35: the object contains a comparison our source
could not produce, so the source must be written the way the object's
instruction names it.  Here the object compares a value it just computed:

    8775f: 0f 9f c2      setg   %dl        ; space_hi
    ...
    87774: 66 85 d2      test   %dx,%dx    ; and then compares it AGAIN
    87777: 0f 95 c0      setne  %al

`space_hi` is `space > 1`, so it is 0/1 and `return space_hi;` is
behaviour-identical — and GCC keeps the raw `setcc` byte, masks it later with
`and $0x1`, and no `setne` is emitted.  Writing the normalisation the object
performs --

    return space_hi != 0;        /* or  space_hi ? 1 : 0  */

-- produces the object's `test %dx,%dx; setne %al`.  Measured on `decide_bit`
(F12167): the setcc multiset becomes an exact match (`setg,setg,setle,setne`),
the symmetric difference 46 -> 30 and the byte delta +38 -> +20.  Three
spellings (`x != 0`, `(int)(x != 0)`, `x ? 1 : 0`) are byte-identical; `x & 1`
is NOT (it stays a mask and no `setne` appears).

**When to reach for this:** the object has a `setcc` your build lacks, the
branch is already there, and your value is provably 0/1.  The tell is the
`and $0x1` (or a `movzbl` of a byte flag) in your build standing where the
object has a `test`/`cmp` followed by `setcc` -- a redundant-looking
normalisation in the source is what puts the comparison back.

### Lever 38. The SIGN of the `setcc` difference names the direction

`scratch/cd/setcc.py <census.json>` gives, per symbol, the reference's `setcc`
multiset against ours.  Read the sign as an instruction about the SOURCE:

| sign | what the object does | what our source should become |
|---|---|---|
| ours < reference | the object masks where we branch | our `if` should be a conditional EXPRESSION (levers 28/29/33/36) |
| ours > reference | the object branches where we mask | our conditional expression should be a control-flow CHAIN (this lever) |

The second direction was untouched for a long time.  `FPM_MTD_detect`'s tail had
one `setl` we should not have: the object has three branches there and no
`setcc` anywhere.  `if (c1) return A; return c2 ? B : C;` is a `COND_EXPR` over
two constants, so it reaches the i386 `movcc` path; the object's branch structure
comes back with

    if (c1)      result = A;
    else if (c2) result = B;
    else         result = C;
    return result;

Measured: byte delta +6 -> +2, conservative run 53 -> 57 of 94, candidate
instruction count 95 -> 94 (the reference's).  Three equivalent spellings are
byte-identical, so the shape is pinned, not the spelling.

**Do not over-claim the converse of lever 36.**  Lever 36 says an if/else that
merely chooses between constants becomes conditional expressions; this lever
says the reverse when the object BRANCHES.  Which one applies is decided by the
object's bytes at the site, not by taste -- and the `setcc` count is how you
read it off without guessing.

### Lever 39. A single `ret` in the object is a hard constraint — `-O3`'s `-funswitch-loops` duplicates the loop

`-O3` implies `-funswitch-loops`, which hoists a loop-invariant test out of a
loop and **duplicates the loop**.  Our source can therefore emit two loops (two
`ret`s, two copies of the body) where the object has one.  Check this FIRST on
any function whose candidate is much larger than the reference:

    objdump -d --disassemble=SYM ref/slmodemd/dsplibs.o | grep -c $'\tret'
    objdump -d --disassemble=SYM build/tc_out/<obj>.o   | grep -c $'\tret'

Measured on `FPM_SMC_encoder` (F12170):

| source shape | candidate insns | `ret` | byte delta |
|---|---|---|---|
| `word = *data++` before the branch | 160 | 2 | +127 |
| the load moved into each arm | 160 (byte-identical) | 2 | +127 |
| load + `q` + `index` in each arm | 160 | 2 | +127 |
| the WHOLE body duplicated in both arms | 144 | **1** | **+96** |

Three lessons:

1. **The body has to be big for the unswitcher to decline.**  Partial duplication
   does not survive -- GCC reinstates the shared load (identical object) and
   still unswitches.  If the object has one loop and yours has two, look for an
   if/else inside the loop whose whole body was written twice.
2. **`-fno-unswitch-loops` is a diagnostic, not a fix.**  It gives a single loop
   here, but at 110 instructions against the reference's 118 -- shorter, so the
   reference is not simply "our source with the unswitcher off".  Recovering the
   loop shape does not, by itself, recover the body.
3. **A much larger candidate is a structural clue, not allocation.**  Two `ret`s
   where the object has one is a source-structure difference; do not spend time
   on register allocation until it is resolved.

### Lever 40. A `&&` can be if-converted into a materialised mask — then the source is NESTED `if`s

Lever 29's rule read backwards, and the reason the source cannot be guessed from
the operator.  GCC 3.4.2's if-conversion turned

    if (b->crc_on != 0 && folded != 0) { ... }

into a materialised mask (`setne %bl; xor %eax,%eax; test %esi,%esi; setne %al;
test %eax,%ebx; je`) where the object has two plain branches and no `setcc`
anywhere.  Nested ifs restore it, and are behaviour-identical:

    if (b->crc_on != 0)
	if (folded != 0) { ... }

Read the object's bytes (lever 38) and pick the spelling:

| our build at the guard | object at the guard | source |
|---|---|---|
| branches | `setcc` + `test` mask | `&` (lever 29) |
| `setcc` + `test` mask | branches | nested `if`s (this lever) |

Measured (F12171, two TUs holding the same algorithm): `getbit` byte delta
+18 -> +3 and `v8_getbit` -70 -> -49, with the two spurious `setne` gone from
both — the reference holds zero.

**Watch the inlining collateral.**  `getbit` is a `static inline` inlined twice
inside `v34handshak`, so a header edit moves that function too (-3477 -> -3505,
a 28-byte cost in a function ~3.5 kB short of the reference).  When the
corrected body is a header inline, re-grade every function that inlines it
before keeping the change.

### Lever 41. Where you write the DECREMENT decides whether the loop is rotated

GCC rotates a `for` loop whose increment/decrement is a separate statement --
the body's first test moves to the bottom and the loop is entered through a
guard.  The object may keep the body in SOURCE ORDER instead, and the way to
get that back is to fold the update into the subscript:

    for (i = n - 1; ; ) {
        ...
        if (symbols[i] != SYM)          /* our build: rotated -- test first */
            break;
        i = (short)(i - 1);
    }

    for (i = n - 1; ; ) {
        ...
        if (symbols[i--] != SYM)        /* the object: run++, test, decrement, compare */
            break;
    }

Measured on `RxTrained2400` (src/pump/v22/v22prc.c, F12172): longest contiguous
run 3/43 -> **7/43**, canonical symmetric difference 30 -> 19, byte delta
-59 -> **-5**, and the reference's `mov idx,tmp` copy reappears.  The tell is
the object's loop body reading in the same order as your source statements while
your build's does not.

Note the interaction with levers 1 and 21: this is an ORDERING lever, so it is
worth trying before concluding a rotated loop is allocation.  It is also
behaviour-identical -- the decrement happens at the same point either way.

#### Addendum to lever 8: a 32-bit STORE with a 16-bit zero-extended LOAD names an `unsigned short`

`FPM_FSD_demodulate`'s counter is stored 32-bit but read back `movzwl 0x54(%esp)`
and incremented `movzwl %ax,%edi` -- zero-extended (F12172).  That combination
is only reachable from a 16-bit UNSIGNED variable: `int`/`unsigned` give a
32-bit `inc`/`mov`, and a signed `short` gives `movswl`.  The store width says
nothing (the compiler widens the slot); the LOAD's extension does.

#### Addendum to lever 31: a cached VALUE the object re-reads is residue too

Lever 31 covers a cached pointer.  The same applies to a cached value:
`int variant = session_int(sess, SESSION_VARIANT);` used three times becomes one
load, while the object performs the load at each use (`mov 0x6120(%r32),%r32`
four times).  Dropping the local improved `V34GiveINFO0dBits`'s run 67/188 ->
70/188 and its symmetric difference 21 -> 19 (F12172).  A cached value changes
the register budget for the whole function, so the effect can be larger than the
one expression.

### Addendum to lever 40: name the EMITTER -- the front end folds `&&`, cse merges `||`

When our build masks where the object branches, it is worth knowing WHICH pass
made the mask, because the two cases look identical in the final object and the
fix is the same but the diagnostic is not:

1. **The C front end folds `a && b` into a bitwise `&`** when both operands are
   comparisons (each yields 0/1).  The mask is then already in `.01.rtl` --
   `fpm`-era examples show `flags>>3; xor 1; and 1; cmp; setle; and` before any
   if-conversion -- and `-fno-if-conversion` does not remove it.  This is also
   why a `&&` can mask with no if-conversion at all.
2. **cse merges a `||` chain of equality tests.**  `state == 10 || state == 13`
   becomes `cmp; je L; cmp; je L`, and cse merges the two jumps to the same
   label into `sete; sete; or; test; jne`.  `-da` confirms the mask first
   appears at the `cse` dump; `.01.rtl` and `.04.jump` have none.

Both are defeated the same way -- NESTED `if`s or an `else if` chain, never the
operator.  Two shapes that work in practice:

    if (state == 5) { ... } else if (state == 10) { ... } else if (...) { ... }
    if (u > thresh) continue;
    if (a > thresh) continue;

and one warning: a chain whose arms each assign the SAME variable is the safest
rewrite (it stays behaviour-identical), which is exactly the shape levers 36/40
already describe.

### Addendum to lever 40: the tell is the `setcc` COUNT, and it usually comes in threes

The SRE pair (F12175) shows the shape of a complete recovery at one of these
sites.  Our build had **7** `setcc` where the object had **2**, and all three of
these had to change together -- any one alone leaves a `setcc` the object does
not have:

1. the `&&` guards -> nested `if`s (lever 40);
2. the magnitude carrier `int a` -> `short a`, because the object's own compare
   is 16-bit (`cmp $0x7ff,%dx`) -- the LOAD's width, not the store's;
3. a `cond ? 0 : 1` assigned to a variable and stored -> an if/else that assigns
   the variable AND the field in each arm.

Two lessons worth carrying: the count of `setcc` at ONE site can be used as the
target (get it to match exactly, then stop), and a function whose `setcc` now
matches but whose instruction COUNT is still short is a lever-2 missing
statement -- do not keep editing the guards.

### Lever 42. The object's bytes choose the conditional-store form: the mask needs the target to BE an arm

`noce_try_store_flag_mask` (ifcvt.c:1041) builds the branchless mask only when

    (if_info->a == const0_rtx && rtx_equal_p (if_info->b, if_info->x))
    || (reversed_comparison_code (...) != UNKNOWN
        && if_info->b == const0_rtx && rtx_equal_p (if_info->a, if_info->x))

so the conditional assignment's TARGET must be one of the two arms.  That single
fact decides the source form, and the object's bytes tell you which side you are
on:

| the object | the source must be | example |
|---|---|---|
| has the mask (`setcc; neg; and`) | an `if` STATEMENT whose target IS the variable | `cursor++; if (cursor >= size) cursor = 0;` -- F12153, F12161 |
| has a plain branch (`cmp; jb`) | a `?:` whose kept arm is a DIFFERENT object | `unsigned short next = cursor + 1; cursor = next < size ? next : 0;` -- F12180 |

Lever 28's "a `?:` never if-converts" and "an `if` statement does" are two
halves of this one fact, so neither is a rule on its own: writing the `if` when
the object branches (`ours > reference` on the `setcc` screen) is as wrong as
writing the `?:` when it masks.  Read the direction from the screen first.

Two riders that are separate, separable properties:

- **Truncate explicitly when the object's compare is narrow.**  `cursor + 1` is
  an `int`, so `cursor + 1 < size` cannot be narrowed to HImode; an explicit
  `unsigned short` local is what produces the object's 16-bit unsigned `jb`
  (without it GCC emits a signed 32-bit `jl`, 13 bytes apart).
- **The counter form is its own lever** (lever 41): the object's countdown
  (`inc %ax; je`) comes from `while (i--)`, not from
  `for (i = n; i != 0; i--)`.

### Run variant sweeps with `tools/toolchain/variant_run.py`, not a per-target harness

A lever hunt is a sweep: 4-15 source spellings of ONE property, each a complete
rebuild, ranked on the same table.  Do not write a new `tryXXX.py` for each
target -- build the sweep out of tree and in parallel:

    python3 tools/toolchain/variant_run.py --spec SPEC.json [--jobs 8]

The spec names the `source`, the `symbols` to grade, and the variants (a name,
`[find, replace]` edits, optional per-variant `extra_flags`).  It reads the real
per-TU compile line from `make -n`, so the C/C++ split and the per-TU flag sets
are always right, and it builds every variant in seconds.

Three properties make its output usable as evidence, and all three are checked
by `--self-test`:

- **The baseline must reproduce the in-tree object by md5.**  The runner prints
  this as a CONTROL line; a failing control means the out-of-tree path is not
  faithful and NO verdict may be drawn from that run.
- **A variant whose `find` string is absent is INVALID, never measured.**  The
  silent version of this bug -- patching the wrong string and re-measuring the
  unchanged object -- has cost real time twice in this project.
- **`extra_flags` must actually reach the compiler.**  A flagged variant whose
  object is byte-identical to the baseline is a runner bug, not a negative.

Read the table's `dmn` column (the lane-free `setcc` multiset) before the
byte delta: when `dmn` is 0 and `dset` is not, the count and mnemonics match the
object and only the register lanes differ -- that is allocation, and it is not
a reason to keep hunting spellings.

### Lever 43. `x + 1 - (a < b)` in an object that holds `sbb $-1` is an UNSIGNED comparison

`sbb $-1` (AT&T: `sbb $0xffffffff,%eax`) computes `dest - (-1) - CF` =
`dest + 1 - CF`, i.e. `x + 1 - (a < b)` with the borrow as its `CF`.  A SUBTRACT
with borrow only exists for the **carry flag**, so the comparison must be an
UNSIGNED one (`jb`/`setb`); with signed operands the compiler emits `setl` and an
`add`/`sub` instead, or a branch.

Measured on `v8_fskdemodulate`'s `drain_run` (F12183):

| source | emitted |
|---|---|
| `int n = whole + 1 - (rem < need);` (signed) | two `setl` -- the object has none |
| `unsigned run` + `int n = whole; if (rem >= need) n++;` | the object's two `sbb $0xffffffff` |

The tell is the object's own instruction: **if the object holds `sbb $0xffffffff`
or `sbb $-1`, the surrounding comparison is unsigned** -- the same rule as levers
34 and 41, read off an instruction instead of a `setcc` name.  (Careful with the
other direction: `setl`/`setg` in the object says SIGNED, and `jb`/`jbe` says
unsigned even where no `setcc` survives.)

The full `n = whole; if (rem >= need) n++;` rewrite is also what removes the
`setl` pair -- writing the increment as a statement rather than a subtracted
predicate is the shape that lets the branchless form disappear.

### Lever 44. Read the allocator's INPUTS from the dumps before touching a lane target

A register-lane difference is not a spelling problem; it is the output of two
priority functions whose inputs are printed by `-da`.  Quote them before testing
anything (F12185):

**Global** -- `global.c:1900` `dump_conflicts` prints the allocnos in
`allocno_order`, sorted by `allocno_compare` (`global.c:628`):

    pri = ((floor_log2 (n_refs) * freq) / live_length) * (10000/REG_FREQ_MAX) * size;

then `find_reg` (`global.c:1064`) takes the first free, mode-OK, call-OK register
in `reg_alloc_order`, which for i386 is the IDENTITY
(`config/i386/i386.h:988`): **eax, ecx, edx, ebx, esi, edi, ebp**.

**Local** -- `local-alloc.c:1198` `block_alloc` sorts the block's qtys by
`QTY_CMP_PRI` (`local-alloc.c:1672`):

    pri = ((floor_log2 (n_refs) * freq * size) / (death - birth)) * (10000/REG_FREQ_MAX);

and its `find_reg` scans `reg_alloc_order` **reordered by
`x86_order_regs_for_local_alloc`** (`config/i386/i386.c`): call-clobbered first,
call-preserved after -- **eax, ecx, edx, ebx, esi, edi, ebp**.  So a block-local
qty refused eax/ecx/edx lands on **ebx**, a callee-saved register.

**The instrument is already emitted** (no gcc rebuild):

    global.c:1900  ";; N regs to allocate: <allocno_order>"
                   ";; <reg> conflicts: <pseudos> <hard regs>"
                   ";; <reg> preferences: <hard regs>"
    cfg.c:502      "Register N used <REG_N_REFS> times across <REG_LIVE_LENGTH> insns; set ..; pref <class>"

`global.c:470` shows how to combine them: `n_refs += REG_N_REFS(i)`,
`freq += REG_FREQ(i)`, `live_length = max(REG_LIVE_LENGTH(i))`.  A register printed
"in block N" is a local-alloc candidate; one without it is global.

**Worked case (F12185).**  `V90Jd::C1` has four global allocnos, dumped as
`;; 4 regs to allocate: 62 88 61 58`; the ref/live numbers reproduce that order,
so the model is checkable on a build you already have.  The three differing lanes
came from ONE local-alloc colour: the block-local byte `look` has `pref Q_REGS`
and took **eax** (ours) against **ebx** (the object's), and because it lives in eax
the global constant gets a hard conflict on eax and falls to edx.  **Look for the
colour that CASCADES, not the lane that differs.**  A `pref` of a restricted class
(Q_REGS, AREG, CREG, INDEX_REGS) plus "in block N" is the signature.

### Lever 45. A shift takes its SIGNEDNESS from the shifted operand -- and a mask literal sets it

`sar` against `shr` in the object is not a spelling of the shift: it is the TYPE of
the value being shifted.  An `&` result takes `common_type` of its operands
(`c-typeck.c:7023`), and a RSHIFT takes the type of the shifted operand
(`c-typeck.c:6805`), so

    ((unsigned short)x & 0xf0) >> 4      ->  the mask is a SIGNED int    -> sar
    ((unsigned short)x & 0xf0u) >> 4     ->  the mask is UNSIGNED int    -> shr

Measured on `V34GiveINFO1aBits` (F12188): writing `0xf0u` reproduced the object's
`shr $0x4,%edx` with delta unchanged and the canonical symmetric difference 4 -> 2.

**The tell:** find the object's `shr`/`div`/`cmp` against `sar`/`idiv` and decide
the OPERAND's signedness, then look for the mask or constant that set it.  A hex
literal without `u` is `int`; so is an unmasked `short` after promotion.  The same
rule covers the object's `sbb $-1` (lever 43) and the mixed-load idioms (an
`unsigned short` element read `movzwl` and sign-extended only at the multiply --
F12187's `rxtiming`).

### Triage a `byte_delta == 0` row with the sibling/ablation screen FIRST

A row whose byte delta is 0 and whose contiguous run is high is, by that very
signature, a same-size same-skeleton row: its residual is a LANE or a scheduling
ORDER, not a missing statement.  Do not queue it as a "near promotion" on the byte
delta alone.  Two one-measurement screens classify it:

1. **Sibling screen (F12186/F12188).**  If the symbol has a `C1`/`C2` clone and
   the sibling is EXACT, dump the TU and compare the two clones' `01.rtl` sections
   (normalise insn UIDs and pseudo numbers) and their `25.greg` allocno lists.  If
   both are identical and the clones first diverge at `28.peephole2`, the cause is
   `peep2_find_free_register`'s cross-function `search_ofs` (`recog.c:2935`,
   advanced `:3018`) -- TU state, not source.
2. **Flag ablation.**  One `variant_run.py` spec with `extra_flags` of
   `-fno-rename-registers`, `-fno-schedule-insns2`, `-fno-peephole2`,
   `-fno-schedule-insns`, `-fno-reorder-blocks`.  A flag that moves the target
   while leaving its sibling exact names the pass.  A statement-order variant that
   changes the bytes but not the contiguous run says the same thing.

The three post-alloc emitters this project keeps meeting, all with a named
predicate: `peephole2` (`recog.c`), `regrename` (`regrename.c:325` min-tick
choice), `sched2` (`haifa-sched.c:989` `rank_for_schedule`).  When one of them is
named, the target is a typed negative -- park it with the TU-state reopening
condition and change nothing.

### Lever 46. A `ret`-count difference is a TWO-WAY test

Lever 39's rule -- a single `ret` in the object where our build has two means
`-funswitch-loops` duplicated a loop -- has a converse that costs nothing to
check and is easy to miss:

    objdump -d --disassemble=SYM ref/slmodemd/dsplibs.o | grep -c $'\tret'

**Two `ret`s in the object where ours has ONE** means the object's two tails
were NOT identical, so `cfgcleanup`'s cross-jumping could not merge them and
`bb-reorder` duplicated the epilogue instead.  The thing to look for is the
register that differs between two otherwise-equivalent stores:

    ours   : mov %ax,0x4f5e(%ecx)   (both paths -- so ce3 cross-jumps them)
    object : mov %di,0x4f5e(%ecx)   (n == 0 path)   and   mov %ax,... (the other)

with `insns_match_p` (cfgcleanup.c:1011-1013, `rtx_renumbered_equal_p`) the
predicate that decides it, and `copy_bb_p` (bb-reorder.c:1006) the duplicator.
`-fno-crossjumping` reproduces the object exactly (2 `ret`s) -- a diagnostic,
never retained.  Reopen only via a supported change that moves the register.

### Lever 47. A `break` out of a `switch` defeats a sibling call — and bb-reorder then duplicates the epilogue

Two facts that combine into a screen and a fix, both measured corpus-wide
(F12191; 53 rows have a `ret`-count difference, and the extras have one cause).

**The screen.**  A `ret`-count difference in the *extra* direction is
`bb-reorder`'s trace-connection duplication.  Its fingerprint is that the
`.ce3` dump has N `(return)` and the `.31.bbro` dump has more.  Duplicator:
`copy_bb` from `find_traces_1_round` (bb-reorder.c:970), gated by `copy_bb_p`
(`bb-reorder.c:1002`).  **The predictor: the number of epilogue copies equals
the number of PREDECESSORS of the return block** (`bb->pred->pred_next`) --
`v23FP_tx_progress` has three byte-identical epilogues in the object because
three source paths return.

    objdump -d --disassemble=SYM ref/slmodemd/dsplibs.o | grep -c $'\tret'
    objdump -d --disassemble=SYM build/tc_out/<obj>.o      | grep -c $'\tret'

**The trigger, and why a `switch` matters.**  `sibcall.c:679-687` makes a call a
sibling call only if `call_block->succ->dest == EXIT_BLOCK_PTR` (or the
alternate exit).  A `break` out of a `switch` leaves the arm's trailing call
falling into a *shared epilogue* instead of tail-calling -- so that epilogue
acquires a second predecessor, which is exactly what `copy_bb_p` needs, and
bb-reorder then duplicates it.  Rewriting the dispatch as an **`if`/`else`
chain** puts the call back in tail position (`jmp` rather than `call`) and the
`ret` count returns to the object's.  Measured: `return;` instead of `break;` is
byte-identical to the baseline, so it is the SHAPE, not the exit statement.

**Do not confuse this with lever 46.**  There, the OBJECT has more `ret`s and
*our* ce3 cross-jump is what merged them (look for the register that differs
between two equivalent stores).  Here, *our* build has more and bb-reorder
duplicated (look for the second predecessor).

### Addendum to lever 47: three ways the epilogue's PREDECESSOR COUNT moves

`copy_bb_p` duplicates the return block once per extra predecessor, so the
question is always "what gives the return block its second predecessor?".
Three answers are now measured, and they are independent:

1. **A `break` out of a `switch`** (lever 47 proper): the arm's trailing call
   leaves tail position (`sibcall.c:679-687`), so it falls into the epilogue.
   Fix: the `if`/`else` chain.
2. **Several `return <field>;` statements that CSE can fold separately.**  Each
   fold makes its own path reach the epilogue.  Fix: ONE labelled shared exit
   (`goto done;` ... `done: return f->status;`), which puts the load in a single
   block and collapses the copies to one.  Measured on `v23FP_rx_progress`
   (delta -15 -> +4, symdiff 24 -> 11) and `BwChDem_Progress` (+20 -> +12,
   38 -> 24).  The object's own byte is the tell: a single
   `movswl <off>(%reg),%eax` epilogue entry reached by several jumps.
3. **A final call that is a SIBLING CALL in the object and a normal call in
   ours.**  A tail call jumps, so it adds NO predecessor; `call` + `jmp` adds
   one.  Witness for `findPadGain`: deleting the final debug print gives the
   reference's 628 instructions and ret 1 -- the trigger is the last call's tail
   position, not the print.
4. (Separately, a NEGATIVE axis with the same symptom: `beepgen_sample`'s
   difference is the `out` PARAMETER's register, and `-fno-crossjumping` does
   not restore its count -- so check WHICH pass moves the count before ranking
   spellings.)

### Lever 48. A float negation as a conditional EXPRESSION gives `xor $0x80000000`; as a statement it gives `fchs`

    shaperAx = bits[word_124] != 0 ? -f : f;      ->  xor $0x80000000,%reg
    if (bits[word_124] != 0) f = -f;              ->  fchs

The object's `xor $0x80000000` is GCC 3.4.2's spelling of the TERNARY, not of a
hand-written sign-bit flip; the statement form cannot produce it.  Measured on
`evaluateInfo` (F12193), where switching to the ternary took the delta from -94
to -54 before the companion member-accumulation fix took it to +2.

The same shape reads the other way too: if the object has `fchs`, the source is
the statement; if it has `xor $0x80000000`, the source is the ternary.  This is
lever 36's rule (an if/else over two VALUES is two conditional expressions)
applied to a float negation, where the two arms differ only in sign.

### Lever 49. A FULLY-unrolled constant-trip loop in the object means the SOURCE was unrolled

Complete unrolling of a loop with a constant trip count is done only by
`decide_unroll_stupid` (loop-unroll.c:1160), gated at `loop-unroll.c:1164`:

    if (!(flags & UAP_UNROLL_ALL))
      return;

and `UAP_UNROLL_ALL` is set only by `-funroll-all-loops` (toplev.c:3086).  `-O3`
sets only `flag_unroll_loops` -> `UAP_UNROLL` (toplev.c:3083), which opens just
`decide_unroll_constant_iterations` (loop-unroll.c:458) -- and that produces a
PARTIAL unroll with a rolled remainder, never a flat sequence.  The old unroller
is inert at `-O3`: `do_unroll = LOOP_AUTO_UNROLL` (toplev.c:3027) and its
predicate at `loop.c:5380-5384` fails because `unrolled_insn_copies =
(insn_count - 2) * n_iterations - 1` (loop.c:5354-5364) far exceeds
`insn_count`.

**So when the object shows 24 stores at 24 constant displacements, no loop
counter and no back-edge, no loop spelling in your source can reach it: the
original source was written out.**  Unroll it by hand (behaviour-preserving),
grouped however the object's groups fall.  Measured on `V92Precoder::reset`
(F12195): delta -109 -> **0**, instructions 34 -> 67 (the object's), symdiff
63 -> **0**.

The tell is the absence of the counter: if the object has no back-edge and no
comparison, the loop is not a loop.  A `-funroll-all-loops` diagnostic build
unrolls but to a different order, which is how you tell a compiler unroll from a
hand-written one.

### Lever 51. The reference INLINES libm; a C profile without `-ffast-math` calls it -- a two-row gap, not a spelling

When a C function whose object body contains an x87 transcendental
(`fsin`/`fcos`/`fprem`/`fyl2x`...) shows no `R_386_PC32` to its libm callee, the
reference build emitted the INLINE expansion, and the source is just the libm
call.  The predicate is `-funsafe-math-optimizations` (which `-ffast-math`
implies): without it GCC treats the call as observable and emits the out-of-line
call; with it the `expand_builtin` path emits the x87 sequence.

Measured on `MTK_phasor` (`fmodf`): the reference is the x87 `fmodsf3` loop
(`fprem` / `fnstsw` / `sahf` / `jp`, and a `fld %st(0)` / `fstp %st(0)`
duplicate of the remainder across the `if (x < 0)`); our build emits `call
fmodf`.  `-fno-math-errno` alone changes nothing; only the full `-ffast-math`
inlines it, and even then the residual is -7 because fast-math drops the
`fld`/`fstp` duplicate the reference keeps.  Adding
`-fno-unsafe-math-optimizations` removes the inline again, so unsafe-math is
necessary but not sufficient.

**Scope, measured, before you reach for a flag.**  A scan of the whole corpus
finds EXACTLY two non-EXACT rows missing an x87 transcendental: `MTK_phasor`
(`fprem`) and `beepgen_sample` (`fsin`+`fcos`, canonical symdiff 48 -> 27 under
fast-math).  No row is missing `fyl2x`/`f2xm1`/`fpatan`/`fscale`.  `period.mk`
keeps `-fno-math-errno -ffast-math` in `TC_CXXONLY` (C++ only) for exactly this
reason; the C profile has no fast-math.  Because UNsafe-math is required and
neither row reaches EXACT even with it, a global C fast-math flag moves two rows
part-way and recovers neither -- do not add one for these.  F12205.

### Trade two neighbours, and RECORD the loss: the codegen ratchet needs a review

Moving one function can take an EXACT neighbour with it, because the period
compiler allocates registers for a whole translation unit at a time. The trade
can still be right, but it is not free and it is not silent.

The worked case is `v90RateRenegSilence` in `VPcmV34Main.cpp`. Writing it with
the object's three bases took it from 83 differing bytes to 22 (LCS 97.7%), and
in the same translation unit `V34XF_IndicateJdReceived` fell from byte-identical
to `BYTES (3)`. The three bytes follow one register choice: `xor %eax,%eax` in the object
against `xor %ebx,%ebx` in ours, its store operand and the shared tail selected
by JNE. Mnemonics still match (normalized longest run 37 of 37, LCS 100%),
but successor rows differ; see F12127. Four
spellings of the zero failed to move it, so it is allocation rather than source.

That loss is acceptable, and it is *recorded*:

- `tools/ci/period_reviewed_losses.json` holds one entry per reviewed loss with
  `base_sha` (the pull request's base revision), `symbol`, `accepted_commit`
  (where the trade was made), `evidence_commit` and `evidence` (a path plus
  `#anchor` into this documentation), and `head_body_sha256`.
- The digest comes from
  `python3 tools/ci/period_metrics.py --fingerprint SYMBOL --head-objects build/tc_out`
  and covers the function BODY and its relocation identity, so editing that
  function again invalidates the review rather than letting it ride.
- The Gentoo Period check FAILS on any loss without a matching, current review,
  even when `make period` is green and the census counts are unchanged. `make
  period` alone cannot see it: the differential asks whether behaviour matches,
  and this loss is behavioural nothing; it is a codegen regression in a
  neighbouring symbol.

So the discipline is: make the trade deliberately, quote the neighbour's before
and after in the commit message, add the review entry in the same batch, and
expect the ratchet to keep failing until you do. A loss with no review is not a
smaller problem than a red differential -- it is the same problem with a quieter
alarm.

### Lever 52. Work a TU in ASCENDING EMISSION ORDER; the peephole2 cursor makes a downstream loss a victim, not a defect

The one cross-function carrier is the peephole2 round-robin cursor
(`recog.c`'s `static int search_ofs`), advanced only on a successful scratch
find and reset on failure, threaded through a translation unit in EMISSION
order (Lever 3b; `global_alloc`, `sched2`, `bb-reorder` and `rnreg` are all
per-function).  So a function's codegen is (its own source) x (the cursor state
it RECEIVES), and that state is fixed entirely by the functions emitted before
it.  That is a DAG that resolves in ONE ascending pass: fix f1, then f2, ... and
each function receives the native cursor once its predecessors are native.

Two rules follow:

- **Do not spend on a downstream function while an upstream one is non-native.**
  Its grade is contaminated by the cursor it inherited, and chasing it is
  chasing a symptom.  The next target is the EARLIEST function whose own
  canonical diff is not explained by a predecessor.
- **But DO re-measure the whole TU after each fix.**  A predecessor fix can
  promote a downstream function for free (the `Dtmf_Rx.c` bystander, Lever 3b),
  so a downstream difference is a victim until its predecessors are native --
  not a separate work item.

That makes a per-TU cascade loss a reviewable event, not a defect.  The
partial-link floor ratchet accepts a `cascade` entry in `tools/ci/displaced.json`
(`metric: "cascade"`, with `from` in EXACT/REGALLOC/MATCHED, the current `to`,
the `upstream` function, and a `body_sha256` pinning the current worst per-TU
body).  It is refused if the aggregate floor drops, and the entry goes stale and
must be removed when the symbol returns -- so the loss stays visible and forced
to be explained, rather than silently re-blessed.

The worked case is F12208: `enterChannelVerificationEv` (V90Equalizer.cpp) and
`exitDILEv` (V90Phase3Modulator.cpp), both with bodies byte-identical to the
floor baseline yet moved out of EXACT by the 11-19 changes to functions emitted
before them.

### Lever 53. A same-count NONMATCH is not a source error until its class is read; the recoverable class is a statement ORDER that induces a copy

The grader's `alpha_why` reports the *first differing row*, but that row is a
symptom.  Classify the whole same-count set first (`scratch/cd/reasonbreak.py`
over `alpha_why`; `scratch/cd/tier1_rootcause.py` for the finer split).  On the
2026-11-22 corpus (131 same-count rows) the classes are 39 mnemonic, 23
allocation, 22 frame/stack, 16 branch-offset, 9 struct-disp, 8 section-addend,
5 type/width, 5 frame-size, 2 symbol, 2 immediate -- and sampling each shows
them to be *orderings* of one instruction multiset, not absent operations.

Two tests tell them apart cheaply:

- **A statement reorder that leaves the object byte-identical is INERT.**  Try
  it (`variant_run.py`): `txinit`'s three zero-stores, `DialerCreate`'s two, and
  `getConstellationsIndex`'s preamble are all inert -- the period compiler owns
  the order.  Do not keep reordering those.
- **A missing `mov %reg,%reg` in the reference (a copy) is the signature of an
  order the compiler CAN see.**  The worked case is F12224:
  `getConstellationMask`/`getCodecConstellationMask` were one copy short because
  the reference read `table = constellation[k]` BEFORE
  `length = constellationSize[k]`; the reversed source coalesced `k` and dropped
  the copy.  Reading them in the reference order restores `mov %edx,%ebx` and
  both are EXACT.  Screen for the pattern with `scratch/cd/copyscreen.py`
  (fraction of reference-only `mov %r,%r`).

Do NOT reach for the classifier when a row stalls: a bounded probe that unpins
`%ebp` (`scratch/cd/ebp_probe.py`) promotes **zero** rows, so the `%ebp` pin is
not a hidden cost -- the ebp-involved rows merely move their conflict.  The
retargeted residue is the count-differers whose missing block is an OPERATION.

### Lever 54. Diff the RELOCATION SET to find genuinely-missing source; a folded constant is a fidelity signal

Alignment screens mis-fire on low-ratio bodies (`spanrep.py` reported a phantom
291-instruction span on `evaluateInfo`, F12233).  The relocation set does not:
`scratch/cd/symdiff.py` diffs the named CALL targets and data/merge-string
targets each NONMATCH row relocates against, and is immune to register choice,
frame size and instruction order.  The worked result (F12234) is that only 2 of
559 NONMATCH rows have a reference-only CALL target and 3 a reference-only data
symbol -- the corpus carries almost no missing calls or globals.

The recoverable class is the **folded constant**.  `BwChDem_Create` wrote
`bw->remaining = block_size_table[0]`, so the `static const` table folded to the
immediate and our row carried no relocation; the object LOADS the table and
relocates against the symbol, which only happens for a NON-constant index.  The
original indexes it (`block_size_table[bw->block_index]`, after
`bw->block_index = 0`).  When the object relocates against a table the source
must keep the access un-foldable -- the original's index EXPRESSION is part of
the recovered source, not an optimisation to be simplified away.  Two further
coupled defects in the same function (zero-store order and `tone.ratio` before
`tone.freq`) took it to EXACT.  Check the relocation diff BEFORE spending on an
alignment-based screen.

## A detector names a hypothesis; the gate names the answer

`tools/toolchain/extcheck.py` pairs a `movzwl` against a `movswl` **by offset
only**. It does not check the base register, so two different structs at the
same displacement are paired freely, and one hit in five is real (F2402).

Measured false positive (V.34, 2026-10-19): `receiver` reported
`mem 0x214`/`mem 0x216` with object `movzwl` versus ours `movswl`, on
`include/dsplib/v34recv.h:360-361` (`short pred_err_re`/`pred_err_im`). The
object's only two loads of those displacements in the whole object were indeed
`movzwl`, and the change made our sites `movzwl` -- and the period gate
**rejected it: 392 passed, 2 failed** (`t_v34datapump` 11/18753 checks;
`t_v34rx` 244054/120452400 plus 602/8451852 transcript checks). Reverted;
394/0 restored.

The rule that follows: **a detector hit is a hypothesis until the period
differential corroborates it.** A clean codegen argument is not stronger
evidence than the gate -- in this case the object's `movzwl 0x214(%ebp)` was
evidently not that field at all, and the tree's sign-extending source is what
matches. Run the gate before recording a retype as recovered.

## A findings entry is a historical measurement, not a state

Findings record what was true when they were written. Before acting on one,
re-measure the claim it makes. F7817 listed `v8_fskmodulate` among eight
functions where "the blob sibcalls and we do not", and stated that our source
does not call `v8_fsktxfilter`; measured today it is **grade 0 EXACT**, calls
`v8_fsktxfilter` at `V8Fsk.c:190`, and tail-calls `v8_txwritequeue` exactly as
the object does. The only visible difference was NOP padding placement, which
`byteident` strips. Reading F7817 as a work item would have "fixed" an exact
function.

`python3 tools/toolchain/byteident.py --why SYMBOL` is the two-line answer.

## Narrow where the object narrows, not earlier

`InitGenSequence` provides a small, exact discriminator. Its reconstruction
used an `unsigned short` local for `total / width - 1`, introducing a
three-byte `movzwl %ax,%eax` absent from the reference. The reference instead
keeps the promoted result full-width until two 16-bit stores.

Both `int` and `unsigned int` locals reproduce the complete reference body.
Plain `int` follows the expression's integer promotions without an extra
conversion, and is the retained spelling. The destination fields remain
unsigned shorts; this is not evidence to widen or retype them. When
`total < width`, the result is still stored as `0xffff` in both fields.
Division-by-zero and oversized-shift behavior are not repaired by this edit.

The full-TU controls gain one exact function with no other body changes.
See [the retained V.32 result](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32seq-top-retained-result.md) for gates
and the separate, unretained floating-absolute-value investigation.

## Separate an element count from its byte-size conversion

The `Scrambler<int, unsigned char>` constructor showed a three-byte
allocation-arithmetic difference: the reference adds the guard element and
then shifts, while the reconstruction uses a scaled address with a byte
displacement. Reassociating the sum did not alter the output. An ordinary
`unsigned int count = b + c + 1` followed by `count * sizeof(T)` reproduced
the reference constructor exactly, without changing the allocation amount.

This is evidence for a source intermediate, not permission to add casts,
volatile objects, barriers or arbitrary locals to force a score. The finite
domain establishes one supported spelling, not a uniquely recovered original.
All 30 header-consumer controls reproduced their baselines; only the two
`Scrambler<int, unsigned char>` constructor copies changed. Other
specializations, including every `Descrambler` copy, remained unchanged.
See [the retained result](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/scrambler-allocation-retained-result.md) for
the shared-header scope, period gates and full-object exact-set comparison.

## Distinguish a cached pointer from repeated member loads

The parallel decoder independently exhibits the same distinction: its
reference body caches `state_`, advances input/output/state pointers, saves
the input byte before writing output, then updates state. Applying that
observed structure reduces its 62-byte reconstruction to 58 bytes against
the original 57. The residual is an extra byte in the load/XOR sequence,
not evidence to add a cast, barrier, or aliasing promise. All 18 header
consumer controls reproduce their baselines; only the decoder helper changes.
See [the decoder result](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/parallel-decoder-retained-result.md) for the
period gates, whole-object comparison, and explicitly rejected invalid run.


An unchanged address calculation is not the same evidence as an unchanged
memory access pattern. In `ParallelDifferentialEncoder<unsigned char>::process`,
the object reads `state_` once before the loop, advances state/input/output
pointers, and stores a local XOR result to output before state. The former
indexed reconstruction reloaded `state_` twice per iteration, mutated state
first and then reloaded it for output. Its loop bound, unlike the state pointer,
is a member read on every iteration in the reference and must remain one.

A cached state cursor and local result recover that evidenced structure.
This is not the same justification as inventing a pointer merely to keep a
preferred base register live across calls. The latter had already been rejected
for `V90SpectralShaper::reset`; do not apply that rejection indiscriminately
to a loop with demonstrably different memory accesses.

Cross source structure with a narrowly chosen pass control. Here six source
forms with and without loop strength reduction showed that explicit cursors,
not a pass toggle, explained the structure. A second staged-read experiment
did not steer the remaining load/XOR order. Stop there rather than add
`volatile`, barriers or attributes: the helper still differs in four bytes,
despite recovering the reference's 54-byte size and memory-access structure.

The full shared-header scope matters: 18 unchanged controls reproduced their
objects, and only the shaper TU changed under the retained candidate. All 830
exact names were preserved. See [the experiment](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/spectral-encoder-experiment.md),
[the read-order control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/spectral-encoder-read-order.md) and
[the retained result](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/spectral-encoder-retained-result.md).

## Equal-width field types still carry aliasing information

Do not normalize an original `long` to `int` merely because both occupy four
bytes in the period build. Equal size, signedness and offsets do not establish
the same source type: GCC's alias analysis can distinguish the declarations
and consequently move loads, stores and register lifetimes differently.

The worked case is `dsp_info.clock_deviation`. Smart Link's vendored host
header declares it `long`; the reconstruction had deliberately substituted
`int` to retain a four-word layout on LP64. Under Gentoo GCC 3.4.2-r2 that
substitution delayed a load in `dp_runtime_create` past several integer
stores. Restoring the independently evidenced host type reduced the function's
byte mismatch from 53 to 28 without changing its 298-byte size. All 830
baseline exact functions survived the complete-object census; among 26
header consumers, only `dp_param.c` changed.

The discriminator was a crossed experiment, not a nearest-score search:
source and destination `int`/`long` types were tested with retained flags
and `-fno-strict-aliasing`. All four no-strict-aliasing cells reproduced the
original reconstruction object. Thus the type substitution's effect was
measured as an alias-analysis interaction. The destination-type controls
lacked independent provenance and were not retained.

Use this workflow when widths and call targets agree but load/store placement
does not:

1. Check original host headers, mangled argument types and other independent
   declaration evidence before rewriting statements to match the schedule.
2. Reproduce the unchanged full TU through the experiment path, then cross
   the evidenced type alternative with an alias-analysis control. Treat the
   control flag as a diagnostic, not a proposed global exception.
3. Measure every consumer of a shared declaration, including exact-member
   losses, nonexact bodies, bindings and the correctly ordered partial link.
4. Validate retention with the period differential gate. Track a native
   64-bit ABI-layout consequence separately; do not silently normalize the
   original type for portability inside the reconstruction.

This solves an identifiable reconstruction mistake, not every remaining
scheduling difference: `dp_runtime_create` is still non-exact. A byte-distance
gain alone is not evidence for a declaration, and a function-level gain need
not improve the whole-section positional metric when other layout differences
remain. See [the experiment](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/dp-param-alias-experiment.md),
[the shared-header audit](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/dp-param-shared-header.md), and issues #15/#22.

## What does not work

- **Renaming a variable.** Free for the compiler (7002). It changes nothing.
- **Using the `+0x7f` immediate-store filter to rule out scratch effects.**
  F8003's screen was disproved independently by F8042 and F8067. Other
  patterns consume scratch registers, including epilogue stack-release
  conversions, and counting stores surviving in the final object misses the
  stores transformed by peephole2. A zero count cannot exclude this mechanism.
  Use Lever 3b's controlled period builds and actual scratch observations;
  report unsupported cases explicitly. A single unproductive function swap
  also excludes only that swap, not definition order throughout the TU.
- **Getting closer.** See lever 1's stopping rule.
- **`__attribute__((noinline))`, `volatile`, or a cast added to make the output
  match.** Fitting the compiler. Three such shims were found and removed once
  the period build could adjudicate, one of which had changed `float`
  arithmetic to `double` — an alteration of the program and not of its
  compilation (1352, 1354, and lever 10's `printSpectrum`).

---

## Grades 1 and 2 are not failure, and grade 1 is not a dead end

    grade 0  EXACT      same bytes, same places
    grade 1  REGALLOC   same instructions and operands under a per-live-range
                        register bijection
    grade 2             differences changing neither result nor execution

Grade 1 entries are **deprioritised, not written off**. Register allocation in
GCC 3.4.2 is a function of source form, and lever 3 is a demonstrated case of
steering it. Do not chase them inside a batch aimed at BYTES; do not describe
them as unreachable either.

`alpha_equal`'s own defects have twice been mistaken for real differences —
a zeroing idiom that failed exactly when a register was renamed (7762), and a
padding guard that matched no padding (7769). If `--why` reports something that
looks like a tool artefact, it may be one; say so and check.

### Measure how much of a function's shape a recovery restored

`tools/toolchain/structsim.py` reports, per symbol, the longest common
contiguous run and the longest common subsequence (LCS) between the object's
copy and ours. Registers are canonicalised to `R`; a call keeps WHICH function
it calls but loses its displacement; a per-TU `.rodata`/`.bss` relocation
addend is masked because it is not comparable with the merged object's; small
displacements and immediates are kept. Alignment padding is stripped, and the
number stripped is printed.

It exists because grade 1 rejects any length difference: a function two
instructions out and one that shares nothing both read REJECT, and neither
says how far from the shape we are. `compare.py` is coarser still -- mnemonics
with operands dropped -- and its report is per-symbol identical-or-not.

    make structsim S=v8_tone_detect
    python3 tools/toolchain/structsim.py v8_tone_detect v8_getbit
    python3 tools/toolchain/structsim.py --worst 40

**Controls, measured 2026-10-19.** `v8_fsktxfilter` (grade 1 REGALLOC) and
`v8_fskmodulate` (grade 0 EXACT) both score **100%**, so the metric does not
punish a pure renaming. `v8_tone_detect` moved from 227 instructions, longest
run 14, LCS 48.9% to 182 instructions, longest run 50, LCS 85.2% when the
biquad was inlined and a pointer-taking helper removed (its four pointer
arguments had been hoisted out of the loop and spilled), while remaining
grade-0 SIZE. That is the case this is for: size alone said 760 bytes against
the object's 603 and could not tell a structural recovery from a stylistic
one, and the byte gap afterwards (605 vs 603) is smaller than the shape
improvement warrants quoting.

**It is NOT a gate and NOT a ratchet.** LCS falls when the compiler schedules
two independent statements differently even though the source is right, which
is why `compare.py --ratchet` is deliberately outside `make phase`; and a
whole-tree scoreboard invites hill-climbing a number, which the
fit-versus-recovery rule declines. Use `--worst` to CHOOSE a target and the
single-symbol form to record what a recovery restored. Denominator: the
symbols the blob and `TC_OUT` both define; the tool refuses on an empty or
stale object tree, and `--self-test` carries ten cases including the ones it
must NOT equate (different displacement, different call target, symbol addend
versus section addend).

### Byte/word aliases require an explicit endian boundary

A recovered union of named bytes and a word can faithfully model i386 partial
stores while still being nonportable. Record whether each use means a physical
object byte or a numeric slice of a word. Do not reverse members speculatively:
external byte-layout consumers may require the original offsets. Until both
kinds of consumers are audited, reject unsupported endianness rather than
silently changing flags. The modem owner headers use `period_byte_layout.h`
for this boundary; this is a restriction, not a completed big-endian port.

The owner migration also demonstrated why typed replacements must retain the
actual pointer path: an equalizer's decoder owner can be external, not the
adjacent embedded decoder. Likewise, a callback slot is not the callback's code
address, and a signed comparison cannot be inferred from an unsigned shared
member declaration. Preserve these distinctions when replacing offset macros.

### Read provenance across stores (V.34 power example)

An input pointer can alias a real output field even when observed lifecycle
callers pass disjoint buffers. Before caching two reads into one local, check
whether an intervening store reaches a compatible input lvalue. In settxlevel,
the blob reloads the MP word after its first power store. Direct reads recover
that data flow without closing the function's remaining codegen gap.

Separate the evidence: observed modem callers, fixed component probes, and
exploratory aliases are different boundaries. Report positive controls and
case/check denominators. Do not promote a synthetic alias to modem reachability,
and do not change source merely to accommodate an impossible fixture. Here
the original load/store ordering supplies independent source evidence.

A closest-size owner variant is still insufficient: crossing short carrier
signedness and assignment placement yields nine distinct objects, none exact,
and the nearest body retains live copy/scheduling differences. Record the
whole-tree set and partial-link losses as well as the local size. See
[the power study](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-small-rtl.md) for fixed probes and the closed domain.


### Count calls only after accounting for tail sharing

A relocation count is a static code-layout measurement, not a count of source
calls. In probeselect, expanding six bit-reversal packing helpers gives an
identical full object; -fno-crossjumping raises 14 reversal calls to 19, past
the blob's 16, and costs two exact helpers. F11533 corrects F11526's proposed
open-coded-versus-call interpretation. Preserve that historical observation,
but do not carry its untested source inference into a new reconstruction.

Counter width and signedness must be checked at each use, not selected for
nearest size. The bit reader's wide CRC with an unsigned top-bit shift and a
signed-short top-bit test compiles identically under the period compiler;
a signed-short arithmetic carrier introduces a narrowing absent from the
blob. Likewise, final-rate candidates can recover a frame/comparison shape
while retaining different live loads, copies and branch sharing. A two-byte
size gap is not stronger evidence than those discrepancies. Close the bounded
domain without adopting a candidate when the predicted source mechanism does
not reproduce it. [The study](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-small-rtl.md) records complete-TU controls,
exact losses and the stopping conditions; none demonstrates a global ceiling.


### Distinguish source arithmetic during expansion from register allocation

A bare arithmetic half and signed division by two differ on negative odd
inputs. In setInitialPhase the blob uses SAR at both numerator sites, while
our /2 creates sign corrections in initial RTL. Crossing the two expressions
independently removes 2 -> 1/1 -> 0 division expressions before allocation;
this is stronger original-source evidence than a reduced size gap. F11534
restores the shifts, preserves all exact names and passes the fixed period
gate, while explicitly recording partial-link layout/relocation losses.

Do not infer every local type from a halfword comparison. A short loop recovers
an induction step and halfword test, yet changes another helper's body; a short
best-error local still compares an int-promoted error at full width. Record
those crossed controls and stop when they fail the predicted whole live graph.
The remaining comparison widths and reloads are unresolved; the successful
arithmetic idiom does not establish a byte-exact function.


### Find the first duplication pass before declaring an inline-budget cause

V8 rebuildJMSequence retains seven charFlip call instructions through .30.rnreg
and acquires an eighth first in .31.bbro. A -fno-reorder-blocks control restores
seven final calls but increases its size gap from 82 to 361 bytes. This is
block-reordering duplication evidence, not a production flag recovery or proof
that the original source is already correct (F11536).

Count unique call_insn UIDs within the exact function. Late RTL uses mode-tagged
forms such as call_insn:HI, and GCSE debug text can print the same UID twice;
raw symbol-reference counts would invent an earlier doubling. Cross-check the
final count against canonical R_386_PC32 symbol targets, not dictionary keys.
The replay tool's known seven/eight/seven controls must fire before trusting it.

Next examine the duplicated block's predecessors, successors, probability
notes and live operands before/after .bbro. Here the pass explicitly clones
block 32 (call UID 334) to 166 (UID 2643), redirecting edge 17 -> 32.
The 17 -> 32 -> 42 trace starts with the incomplete-extension word reload;
that shared acceptance join is a concrete source-graph boundary to investigate. Use a source/options crossing to
test an independently supported acceptance/extension CFG hypothesis. A finite
failed family does not prove a source-recovery ceiling, and lack of a unique
preimage is not proof that all faithful source hypotheses are exhausted.
Keep SIZE length gaps distinct from counts of differing bytes.

For V34's initial-phase graph, short e plus short best recovers the halfword
comparison that short best alone missed; the coupled graph also recovers
induction and product narrowing. Its two-byte near-match still caches inputs
contrary to the blob (F11535), so it is declined. Do not remove a defined
winner default to imitate an apparently uninitialized blob slot without
proving the original first-improvement invariant.


A missing initialization can be investigated with a compiled-arithmetic
invariant rather than deleting the default to improve size. For initial-phase
winner selection, bits 13..28 of the wrapped square/error make the threshold
predicate periodic over 2^28 ratios. Exhaustive finite arithmetic proves a
first improvement by iteration 3 for every 32-bit ratio (F11537), resolving
F11535's safety question. Default removal still fails to recover the complete
object and changes another helper; it is not adopted. Report the mathematical
boundary, full denominator and known positive/negative controls separately
from protocol fixtures and source-preimage evidence.


A cached input may involve more than one pass. In V34 initial-phase setup,
GCSE PRE creates incoming-edge loads, and post-loop CSE first replaces them
with earlier value copies (F11543). Disabling the second transformation
confirms its cause without undoing the first. Check the earliest changed
RTL and the final load graph separately, including every changed TU body.
Consult the recovered compiler source and relevant patch guards, but keep
executed compiler dumps as authority when only selected source files have
been patched. A named scalar local is a finite source hypothesis only when
conversion expansion predicts a distinguishable RTL representation; names
alone do not compel new loads or register allocation.


A zero SIZE change can hide a useful arithmetic recovery: TimingV34's three
explicit halves remove six report arithmetic bytes while alignment adds six
bytes elsewhere (F11544). Use the operand widths and signedness, local opcode
landmarks and full-TU body inventory as evidence. A signed-word comparison
and sign-extended divisor can establish a carrier correction independently
of register scheduling; retain synthetic high-bit examples as arithmetic
boundaries, not claims of reachable modem states. Gate source adoption on
fixed period differential fixtures and report unchanged exact denominators.


When an earlier pass creates the later pass's input, complete the bounded
interaction rather than trying nearby source spellings. V34's eight-cell
source/GCSE/CSE2 cube restores indexed reloads with GCSE off and shows no
remaining CSE2 effect in that function (F11540), despite changes to 44 other
bodies. The restored graph costs four exact helpers. This rejects a
regalloc-only explanation of that local graph without selecting a compiler
profile; any next profile test must explain those helpers and the remaining
non-exact body together.


## Transfer recovered patterns through a bounded small-function pass

A curated 20-function shortlist produced three exact gains after testing five
families (F11541), without a new compiler profile. Prefer observable source
boundaries over residual size: cache a derived owner before a debug call
when the blob keeps it across that call; share a successful return when an
early return gives the result an extra lifetime; use an unconditional signed
minimum store where the blob stores in both outcomes. FloatIIR needed both
the nested geometry arm and minimum, while FloatFIR did not close with the
same pattern. V32's result initialization position mattered, and retaining
only its exact renegotiation function preserved the hit independently of the
non-exact retrain neighbor.

Measure full TUs and preserve nearby non-exact bodies. An idiom that closed
one family is a hypothesis elsewhere, not permission for a mechanical tree
rewrite. The FIFO experiment produced identical objects for unsigned-short
and int returns, with no caller discriminating the API: leave that signature
unchanged. Show shortlist/screen/compile denominators, known detector
controls and closed-domain dispositions. [The pass](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/playbook-small-patterns.md)
records corrected compiler-path replay, all losses (none in retained source),
period validation and the still-failing complete-object comparison.


The reserve pass (F11542) adds two useful controls. For constant fill loops,
recover counter direction and output traversal together: a signed-short
post-decrement countdown plus advancing pointer closes TxNOP/RxClampV22;
changing either alone misses. Preserve exact write count and memory order,
then check sentinel boundaries in existing fixed differential fixtures.
For a float-to-int threshold comparison, naming the converted threshold
before loading the count can recover the x87 conversion/load schedule without
changing precision or rounding. These are bounded source carriers, not proof
of unique original spellings. CID caching and calling-tone early loads did
not transfer successfully; V32's closest array cell still has a reversed
store/extension pair and is left out. [The reserve ledger](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/playbook-reserve-patterns.md)
records all seventeen cells and the complete-TU controls.


A narrow output parameter can carry a returned value differently from a
local of the same declared type. RxHdxSequenceE's unsigned-short local
expands into a widened SI pseudo and its count store uses the lowpart;
storing directly through count and passing *count to the next call keeps
the returned HI store before widening. Together with direct array-root
access this closes the complete339-byte function (F11545); either change
alone misses. Inspect initial RTL and cross the independent carriers before
classifying an adjacent store/extension swap as register allocation alone.
Preserve the same truncation, memory order and call boundaries; do not
mechanically eliminate locals elsewhere. [The bounded record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-sequence-count.md)
includes exact neighboring-body controls and the two still-deferred V22
reserves whose previous spelling domains are already closed.


Transfer a recovered loop pattern by a declared product, then distinguish
source-graph recovery from exactness. DetSequence's countdown and separate
short shift reproduce their local graph but increase size; a newly observed
once-per-call found initializer fixes a third lifetime without closing the
spill choice (F11546). Local allocation still has the bound pseudo; global
allocation/reload spills it and retains the shift-register pseudo, opposite
the blob. That is a pass boundary for the next explanation, not permission
for register/declaration spelling searches. [The six-cell ledger](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-detsequence-loops.md)
reports all negatives, positive graph/spill controls and unchanged retained
objects. Include new tool/doc files in the tracked-file census before checks,
so an untracked ledger cannot silently escape cross-reference validation.


A global-allocation dump may include reload's final mapping: distinguish the
boundaries before attributing a spill to priority. F11547 traces installed
Gentoo cc1 and raw-validates both full-TU outputs; one reg is assigned ECX by
global allocation, then evicted by reload's CL requirement. A short
post-decrement temporary instead receives ECX locally and changes the earlier
allocation decision. F11548 separates decrement from extraction, removes that
local temporary and recovers the blob's spill arrangement without byte identity.
[Trace and bounded source control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-detsequence-allocation.md). Machine
scheduling can put the decrement ahead of SHR even when source decrements
after extraction; don't infer expression sequencing from that order alone.


Trace spill-home allocation before inferring source declaration order. F11549
observes ascending pseudo allocation and late reload eviction; a minimal
word-scope change recovers all blob slots but remains non-exact. Optimized
compiler debug parameter locations can misreport arguments: trace the true
ABI entry and validate full-TU raw output. F11550's crossed peephole control
certifies scratch exposure but loses exact neighbors. Returning to the
non-exact predecessor reveals an independent GenSequence source property:
short post-decrement narrows before the wrapping AND, and combines with a
countdown to recover 118 bytes exactly (F11551–F11552). [Measured record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-gensequence-recovery.md).

## Locate redundant-load elimination before blaming allocation

TxHdxTRN's cached subtraction initially resembles a register-lifetime issue.
Its first CSE dump still reloads the member; GCSE PRE explicitly replaces
that load with the comparison's reaching register. Cross the independently
observed unsigned input fold with diagnostic pass controls: disabling load
motion changes neither raw object, while disabling all GCSE restores memory
RMW but loses two exact neighbors. The unsigned fold itself recovers one
instruction without exact identity. Treat these as separate measured effects,
and close both bounded domains without inventing volatile declarations or
adopting local score improvements. [F11553 and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-txhdxtrn-pass-boundary.md).

FPM_rms supplies another independently measured countdown/cursor case:
neither property alone recovers the function, and both recover all62 bytes
including its call relocation (F11554). Preserve arithmetic order and overflow
semantics; loop recovery is not permission to replace scaled products with
an algebraically similar expression. [Four-cell record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fpm-rms-countdown.md).

An allocation failure's literal-zero early return can obscure the blob's
common pointer return. Dual_TONE_create recovers64-byte exactness by guarding
initialization and returning the allocated pointer for both outcomes; the
two-cell full-TU domain changes only that function (F11555).
[Measured record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/dualtone-create-common-return.md).
**Update (F12174, 2026-11-16): `cid_create` recovers the same way and this tree
now holds three measured outcomes of the family.**  The deciding evidence is a
COPY the object performs before the test:

    mov 0x10(%esp),%eax   ; ctx
    test %eax,%eax
    mov %eax,%ebx         ; <-- the copy, before the `je` to the malloc arm
    je  ...

A body that keeps working on the parameter cannot produce it; a separate result
local with `else { ret = ctx; }` can.  So the rule is not "constructors copy the
argument" but "read the allocation test: if the object copies, write the result
local".  `silence_create` (EXACT here) and `Dual_TONE_create` (F11555) are the
other two members; F11562 closed the family only for the spelling it tested.

Trace x87 stack operands before calling a difference scheduling. Notch's
saved-state addition occurs before its product-sum in the blob and after it
in retained source. Recovering the addition tree still leaves load/exchange
differences and no exact gain (F11556). Algebraic equivalence does not imply
finite-precision grouping, and load order alone does not justify invented
coefficient locals. [Closed source domain](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/notch-addition-tree.md).

## A constant loop bound can reveal when a helper was inlined

PCM's indexed segment search differs from the blob's pointer walk. Recovering
the cursor leaves a strict-comparison difference: fixed source emits <=7,
the blob emits <8. A helper size argument initialized to8 at the call preserves
the strict comparison under period inlining; both size-only and table/size
forms yield the same complete object. This constrains late binding without
uniquely recovering a helper signature (F11557). A separate input/magnitude
carrier domain closes linear2alaw too (F11558). Inspect initial RTL before
blaming scheduling for a comparison that was already normalized there.
[Seven-cell staged record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/pcm-segment-search-recovery.md).

## A machine countdown can be an optimizer's reversed ascending loop

Float2Linear's four-cell source domain recovers101-byte exactness with
advancing input/output pointers and the existing ascending loop. An explicit
countdown misses, alone and with cursors. Gentoo's loop dump says it reversed
the cursor loop; source index i is dead except as the bound. Check that pass
before inferring a countdown from machine decrement/test (F11559).
[Complete-TU controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/float2linear-cursors.md).

An apparently register/layout-bound constructor can still omit a real failure
edge. toneiir_create's blob checks the allocation result; reconstruction did
not. Restore observed behavior even when its finite source domain gives no
byte-exact gain. A same-length candidate with114 differing bytes is not a
recovery. Fixed allocator-failure and one-shot recovery checks distinguish this
from a score-only rewrite (F11560). [Record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/toneiir-allocation-recovery.md).

Recovering an x87 opcode is not recovering its lifetime or evaluation boundary.
Floating RMS literal/local controls recover fld1 but either add final narrowing
or move the reciprocal before the loop; both miss the reference's two separated
count conversions (F11561). Inspect the full stack tree and conversion sites,
not just the attractive opcode. Likewise the constructor common-pointer-return
family succeeds for Dual_TONE_create and fails for silence_create (F11562).
[Closed domains](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/playbook-rms-silence-controls.md).

GenerateAnsTone supplies another ascending-loop reversal control: an output
cursor recovers its clear loop; a source countdown does not. Its four remaining
comparison bytes disappear when elapsed updates use the field rather than a
common temporary. Early source field writes are coalesced into the reference's
conditional final stores by Gentoo, so infer source factoring from complete
controlled bodies rather than store placement alone (F11563).
[Six-cell record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-anstone-recovery.md).

Operand-width recovery can leave a distinct condition-lowering mismatch.
FPM_TONE_filter's short carrier cross restores cmpw/incw but leaves a branch
where the blob has setl/neg/and, with no exact gain. Do not adopt carrier
narrowing solely because individual instructions agree, or infer a unique
local type from a word comparison (F11564).
[Closed width domain](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fpm-tone-width-controls.md).

A shared narrow temporary can extend a value before a store that only needs its
low half. V22 IIR recovers history-store-before-extension by narrowing at uses,
then recovers its full body only when accumulator initialization/reset match
the preheader/latch boundary. Cross both source questions: either alone misses
(F11565). Keep mixer narrowing explicit; rereading the stored history introduces
an alias-sensitive operation the reference does not perform.
[Six-cell record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v22-iir-conversion-recovery.md).

SMCv32_encoder_abs's countdown/cursor cross restores sequential input and the
word sentinel but leaves distinct ring-return/tag conversion boundaries. A
correct recovered loop is not authority to widen ring locals or force masks;
inspect the helper signature and field reads across callers first, including
closed F8249 controls (F11566).
[Traversal domain](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-smc-abs-traversal-controls.md).

For a rejected conditional-zero mask, read the converter's complete predicate.
GCC3.4's noce_try_store_flag_mask needs zero versus the destination itself:
a ternary assigning a separate result temporary can fail even when both arms
are already SImode. Ordinary in-place conditional clearing recovers V32's mask
in all three encoder callers (F11568). Shifted tags, wrapped-result carriers
and comparison-use narrowing are separate axes; together they recover an exact
68-byte abs loop but leave51 bytes different in the whole function (F11567,
F11569). Do not turn an exact internal region into an adoption or declare the
remaining prologue a pure regalloc issue without examining its load ordering.
[Closed24-cell record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-smc-if-conversion.md).

The destination-identity diagnostic transfers to FPM_TONE_filter: six ordinary
conditional updates convert and six ternaries do not, across a bounded carrier
cross. No function becomes exact (F11570). A verified shared compiler mechanism
can explain an instruction family while leaving scheduling and lifetime
questions open; do not equate successful transfer with recovered original
source. [Twelve-cell transfer](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fpm-tone-if-conversion.md).

A close non-exact slicer can still read its decision input after an output that
the blob reads first. FSE_decision_CD's fixed permitted count/magnitude alias
fails3/268 baseline checks; recover the read boundary before interpreting its
remaining scheduler difference (F11571). BYTES2 masks the union of relocation
fields: here four raw bytes and the successor relocation move when two stores
swap. Inspect relocation positions and RTL stages, not the byte score alone.
[Source/option cross and fixed fixture](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fse-cd-load-recovery.md).

Do not infer that reversing two independent source writes will undo a measured
scheduler reversal. The CD successor/lms two-cell control emits the same
complete object in both orders (F11572); the compiler can erase lexical-order
information before final scheduling. Close that bounded family rather than
expanding into arbitrary store permutations.


An early zero-extension can reflect a conversion boundary without being the
whole source mismatch. Caller ID pack_next_bit's seven carrier/position controls
recover selected operations but leave the complete function non-exact; review
its inlined caller too (F11573). [Closed domain](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/cid-pack-conversion-controls.md).

Do not clear automatic output fields merely because the reference leaves them
uninitialized. Prove which fields the callee defines and which anyone reads.
The cosine generator's artificial clears and cached scale interact with the
loop carrier: only their removal plus short post-decrement recovers the complete
125-byte function in an eight-cell cross (F11574). The same-length cached-scale
control still differs in88 bytes. Register allocation can follow recovered
lifetimes without register-specific source. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fpm-tone-demod-recovery.md).


A ternary counter update can merge stores/returns and change a byte input's
lifetime even when all arithmetic is already correct. cEncodeChar's complete
six-cell cross gives three exact ordinary-branch forms and zero ternary hits;
byte-helper narrowing is unnecessary in that family (F11575). Select an
independently supported form and review all callers: the direct byte form
preserves edprintf, while shared helper changes affect it. Multiple hits bound
a common source property, not a unique original spelling.
[Carrier/control cross](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/encode-carrier-recovery.md).


Check siblings for a proved output-lifetime/countdown pattern, but rerun the
cross: generate2's four cells yield only the combined148-byte exact hit
(F11576), preserving its two separate scale reads and demod's existing gain.
The callee must define both outputs before use; a matching sibling is evidence
for transfer, not permission to apply it blindly to phase-reversal generators.
[Quadrature transfer](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fpm-tone-pair-recovery.md).


A register-heavy oscillator residual can still conceal a real signed-word
boundary. Fixed negative traversal and naturally reached counter32760 expose
FPM_TONE_generate discrepancies that ordinary answer-tone tests miss (F11577).
Use adequate buffers and valid history before changing source. Narrowing locals
can recover cmpw while eager Boolean lowering appears already in initial RTL;
that is not allocator evidence. Direct field predicates recover branches, then
owner-counter update plus predicate order and private-phase capture boundary
recover the full213 bytes. Early source field writes can be coalesced into
conditional final stores. [Bounded recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fpm-tone-sine-recovery.md).


Primitive array-new can preserve an element-count boundary that manual byte
allocation folds into a different instruction. Psd's five-cell scaffold/control
cross recovers both82-byte constructors when the second float array uses new[];
first-only and unused allocator scaffold raw-reproduce baseline (F11578).
Two exact cells emit the same complete object: choose the consistent two-array
form without claiming unique first-allocation spelling. Verify cookies, emitted
allocator symbols, exact sizes, uninitialized buffer content and every TU body.
C++ replay must include configured CXX flags, not only the C profile.
[Allocation transfer](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/psd-array-new-recovery.md).


Owned-object publication can distinguish otherwise similar constructor code.
V90SpectralVerifier publishes Psd at+4 after nested construction in the blob;
manual allocation assigned to the member before construction obscured that
lifetime. Raw-local/late assignment and direct placement-expression assignment
emit the same162-byte exact clones (F11579). Compare the call/store boundary
and preserve parameter-member reloads; do not infer allocator changes from
register colours alone. [Full-TU controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v90sv-publication-recovery.md).


Publication transfer has limits: Resampler history-local and conditional-owner
forms recover the blob allocation/pointer-store boundary and174-byte length
but still differ in44 bytes (F11580); complete TU exact set unchanged. Do not
adopt a size-only hit or expand arbitrary initialization-store permutations.
[Closed family](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/resampler-history-publication-controls.md).


A sibling count-local transfer can recover only an arithmetic prefix.
Descrambler<int,int> becomes107B/BYTES19 across27 header consumers with
no exact gains; Scrambler's prior success is not a full-preimage proof
(F11581). Require every defining copy and full body, preserve negative
controls and close the tested arithmetic family.
[Shared-header transfer](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/descrambler-count-controls.md).


Opposite-order duplicate pretests before a member cursor is initialized can
expose an outer source guard around a for loop. FloatARMA's independent
denominator/numerator cross has only one full hit: both padding guards
recover both612-byte clones (F11582). Denominator-only gets the same length
but differs171 bytes; numerator-only stays596B. Cross guards, retain full
bodies and inspect final cursor state, rather than treating size as fidelity.
[Guard recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/floatarma-padding-recovery.md).


An inline narrowing cast can still be folded into a table relocation, while
assignment into a short index preserves the same compiler's conversion
boundary. TONE_read needs short masked phase, bounded quadrants and stored
short reflections; the width-only cross leaves11 differing bytes, both
storage forms raw-agree on the full121-byte object (F11584). Use fixed
exhaustive input coverage and full-TU/relocation controls, not explicit
registers or assumed original spelling.
[Width/storage cross](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/tone-read-width-recovery.md).

Unconditional min assignment is not universally an exactness unlock.
FloatFIR setCoefficients' four min-store/nested-update cells recover selected
boundaries but none the complete body (F11583); close the family rather than
expand arbitrary variable/store permutations.
[Closed control cross](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/floatfir-coefficient-control.md).


A word load does not establish unsigned source type. RxClampV32's member
is already signed short; countdown/conversion controls close without a gain
(F11585). [Closed domain](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/rxclamp-count-controls.md).

Matching loop/return shape and even total size does not establish a divider
preimage. GetFP_Value's bounded arithmetic/lifetime families leave byte
mismatches; preserve the static zero-divisor fidelity observation separately
from unrun runtime/reachability claims (F11586).
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/getfp-divider-controls.md).

Aggregate copies and short element counts are independent boundaries:
FPM_MTD_create needs both, plus short loop counter and removal of an
unsupported allocation-success guard. Only the combined16-cell cross recovers
184B exactly (F11587). Verify exceptional outcomes with fixed child processes
and successful controls; an added NULL return can hide a blob fault and alter
fidelity. [Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fpm-mtd-create-recovery.md).


After aggregate configuration copy, subsequent arithmetic may read the owner
fields rather than the incoming pointer. FPM_MRF_init also uses the original
fresh argument as an allocation carrier and a short clear counter (F11588).
An8-cell independent cross needs all three for full193B exactness; counter+
carrier alone reaches size but differs30 bytes. Keep growth ownership/debug
ordering and verify growth through valid successive initialization.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fpm-mrf-init-recovery.md).


A short local inside a shared inlined helper is not a universal conversion
carrier. Phasor interpolation fraction-width transfer alone adds one byte
per oscillator without recovering their bodies (F11589). Preserve the
complete TU/table controls and close that local-width family; existing
exhaustive fixtures are not evidence for an unrun candidate.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/phasor-fraction-controls.md).


An unsigned-word operand can lose its additional bits when its arithmetic
result is assigned short. Phasor's original-word × fraction-width cross
raw-merges unsigned+short with short-only, with no exact gains (F11590).
A separate observed register/load role is a hypothesis to test, not a source
preimage once the bounded cross refutes it.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/phasor-consumption-controls.md).


Use-site signedness can differ between allocation and clearing: FPM_FSD_init
allocates an unsigned-word trace length but clears with signed comparisons.
Shared short counter plus that allocation conversion closes its full268-byte
body; short counter alone misses one byte (F11591). Test requested allocation
sizes and untouched allocated contents at negative component boundaries.
Do not retype the field or narrow a promoted loop bound merely to avoid the
blob's own counter wrap. [Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fpm-fsd-init-recovery.md).


Word countdowns/cached members/narrowed total can recover FSM loop structure
without its complete229-byte body (F11592). Keep the tested four-axis family
closed; upper return-register bits do not uniquely type the public API.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fsm-modulate-controls.md).

A duplicated positive-count pretest motivates a guard experiment, not an
exact source claim. FSE_getdiag loop-only guard reaches217B vs229B without
exactness (F11593). Inspect the branch destination instruction before inferring
an exceptional return: it moves the selected count, not zero. Preserve invalid
preimage attempts and existing negative-count fixture evidence separately.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fse-getdiag-guard-controls.md).


Generic helper coverage does not validate a parent's child configuration.
BwChDem_Create's ratio28996 differs from object's29000 even while existing
waveform tests pass (F11594); fixed owned-child comparison catches all five
constructors. Recover literal fidelity independently from exactness. Deferred
const-table definition raw-reproduces baseline, so initializer visibility does
not unlock this first-entry load. Keep const/data controls; no source-order
or mutable/volatile score fitting. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/bwch-constructor-controls.md).


A later readonly initializer may still be visible when unit-at-a-time parsing
precedes expansion. Cross initializer placement with that pass before attributing
constant folding to source order (F11595). Initial RTL can confirm the mechanism
without recovering the body: Bw Create regains its table load and size but still
has a fixed store-displacement mismatch, while Progress and data layout change.
Compare named data owners and relocations separately from section padding/order;
never adopt the option merely because one instruction reappears.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/bwch-unit-visibility-controls.md).


Early loads can identify original local lifetimes even when field widths are
already right. ECC init caches near-delay and line fill before allocation and
clears; both locals recover the607-byte body (F11596). Short and promoted-int
spellings raw-merge, so claim the retained lifetime rather than a unique width.
Use fixed alias controls to expose rereads, labelled component-only when no
modem history is proved. A bystander free's dead POP may change too; record
that gain without inventing a source change in free.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/ecc-init-cache-recovery.md).


Do not treat early index initialization plus separated postincrement as an
exact source recovery merely because size approaches the object. V22 MRF's
four-cell cross closes without a hit (F11597): combined227Bvs228 still changes
saved-register count and sequencing. Distinguish RHS sampling before a
postincrement assignment from incrementing after its store; these have
different local lifetimes. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v22-mrf-index-controls.md).


Sampling the RHS before work[k++] is a distinct discriminator, but V22 MRF's
four-cell follow-up also fails (F11598), growing244→245B. After two negative
batches, park nearby index/temporary synonyms and return to independent
operand or field evidence. A bounded failed family does not establish that
source recovery is globally exhausted.
[Scope review](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v22-mrf-index-controls.md).


Opposite temporary-array rail placement and early work-index lifetime motivate
an independent stack/source cross, but do not force an exact preimage.
V22 PPS's four cells restore selected boundaries yet fail the complete290B
body (F11599). Close the family, preserve arrays/data/symbol controls and
select another operand/use lead. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v22-pps-init-controls.md).


Distinguish a sentinel countdown from a postdecrement test using initial RTL
and final flags. MTD sentinel retains CMPWffff; postdecrement reproduces the
blob's DEC/narrow/INCWoldflags but still misses the complete297B function
(F11600/F11601). Short energy locals do not by themselves reproduce its clamp.
Terminal ternary lowering is another early-stage discriminator; sharedverdict
also fails (F11602). Close these staged families, preserve data/body controls,
and do not promote a one-byte size difference into a byte preimage.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/mtd-detect-controls.md).


Review an inlined helper's complete TU, including bystanders. Shared IIR's
counter/cursor/feed-forward narrowing eight-cell cross has no full hit
(F11603), and count changes untouched II despite unchanged size. Do not add
an uninitialized output merely to match the blob's undefined zero-section
return; preserve the fixture's exclusion and demand an independent discriminator
before more local permutations. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/iir-boundary-controls.md).


A use-site word conversion does not prove a word-width running carrier.
Block-update's promoted-position control reflects the blob's full-width
subtraction and later narrowing but fails its complete244B body (F11604).
Keep arithmetic bounds separate from service reachability, close the bounded
family and do not fit accumulator widths or stack allocation by size.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/block-position-controls.md).


Local stack-word addresses and store/reload pairs support testing an ordinary
inlined helper, without forcing address escapes. Div32 helper×word-index
cross reproduces these boundaries, and guarded do/while restores conditional
count-store placement, but neither closes the full body (F11605/F11606).
Review error epilogues and narrowing widths, not just size. Audit fixture
cross-product claims: two marginal sweeps need not cover every pair. Keep
concrete missing fixed vectors and close declaration/frame permutations.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/div32-normalization-controls.md).


Signed-word increment and comparison evidence can recover a loop even when
its complete initializer still misses. V34 detector short counters reproduce
its prologue and clearing loops, but two observed post-loop store-order axes
fail the full body (F11607/F11608). Close the finite family, keep the partial
recovery and select independent evidence rather than permute fields/registers.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-detector-initialization-controls.md).


An unread callee slot does not establish an absent source argument. Audit all
caller stack writes before dropping a nominally dead scalar: restoring MRF/FSD
free arguments recovers four complete callers (F11609). Cross each API change,
use isolated consistent header overlays, then rebuild every caller/callee and
adapt test consumers. Recover observed literals, not invented ownership meaning
or a unique formal width. Check unchanged helper bodies and non-exact bystanders;
API fidelity is source evidence even when only some callers become exact.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/free-argument-recovery.md).


Extend an ignored-argument recovery only after auditing every caller. The
FSE/SRE×ECC×PPS cross restores13 explicit literal1 slots and yields five exact
callers (F11610); two V27 bodies still miss initial setup, so preserve their
unmatched status. Ignored formal type/name/meaning remain unproved. Full
partial links may lose positioned relocation matches despite complete function
gains; report both, with all-TU data/export and differential controls.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/free-argument-recovery-rest.md).


Repeated owner loads after calls can distinguish a cached child local from
original direct owner expressions. B103 needs both dsp andhdx lifetimes to
recover its full deletion body (F11611); either alone misses. Retain guards,
call order and real lifecycle allocator checks. Do not infer a prior behavior
bug from a register/source lifetime recovery, or transfer it to already-matching
loads without new evidence. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/free-argument-recovery-rest.md).


Call argument evaluation and a preceding pointer assignment need not schedule
identically. When a deletion suffix already matches, compare direct owner-member
arguments against the explicit child-local statement at the first calls. V27
RX/TX recover complete bodies this way (F11612); preserve later matching owner
reloads. Review register-renamed bystanders and avoid claiming unique spelling
from setup order. [Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v27-delete-argument-recovery.md).


Keep ignored arguments and return recovery as independent axes. Audit every
caller slot, including original input counts that survive a resampler overwrite.
AGC's nineteen explicit fourth slots recover RxHdxNoSignal (F11613) with the
callee raw-identical while preserving void and field reloads. An object's EAX
consumption is evidence for a return hypothesis; a reconstruction header does
not prove the original lacked a result. Review genuine control-factoring
bystanders, not just edited callers. [Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/agc-fourth-argument-recovery.md).


Stage return semantics separately from call-argument restoration. Six observed
AGC EAX consumers motivate an int return, with two signed-short conversions;
minimal and complete twelve-TU controls recover no exact bodies (F11614).
Ignored results can still affect a caller body, so audit all API consumers.
Close the finite family without asserting that zero gains disprove the original
return type. Reject an empty domain and preflight all generators before the
first compile; preserve invalid attempts outside valid result artifacts.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/agc-return-controls.md).


Cached-pointer controls do not exhaust an ordinary delete expression. A
typed owned class pointer plus one blob pointer retained across destructor/
free supports testing scalar delete separately from its TU-local adapter.
V90Phase4Modulator's adapter-only cell raw-merges; member delete recovers both
94B clones and leaves all51 bystanders unchanged (F11617). Preserve ownership,
null guard and automatic member destruction. A recovered first-emitted body
does not establish that later register/scheduler differences must disappear.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v90p4-owned-delete-recovery.md).


Cross each independently observed owned-member lifetime. V90Modulator's
three typed destructor/free pairs all retain one blob pointer across calls.
Each partial member-delete combination matches199B but still fails the
complete body; all three recover both clones with all23 bystanders unchanged
(F11618). Use complete bytes/relocations to distinguish the crossed family
from an accidental size match. Preserve primitive frees and generated member
destructors, and isolate the adapter-only control.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v90-owned-delete-recovery.md).


A nonvirtual owned class deletion can emit both destructor and host free.
Do not infer explicit destructor syntax from two calls alone. V92EchoCanceller
retains the same pointer across both calls with ordinary guarded member delete,
recovering both195B clones while all13 other bodies remain unchanged (F11619).
Keep conditional pointer clears inside their observed guards; test the adapter
independently. Large layout growth can still reduce positioned partial-link
matches despite complete function recovery. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v92ec-owned-delete-recovery.md).


A five-owner cross can reveal a bystander register effect separately from
source recovery. All31 nonzero V92Modulator crosses recover enterPhase3 by
ECX/EDX renaming, but only all-five recovers both complete503B destructors
(F11620). Every partial also503B yet fails bytes. Audit the bystander, preserve
virtual and primitive releases, and label temporarily masked-owner fixture
subsets synthetic rather than pre-release histories.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v92-owned-delete-recovery.md).


Typed-owner recovery does not authorize rewriting adjacent primitive releases.
V90/V92 modem crosses recover four destructor clones plus two unchanged
register-renamed bystanders (F11621); phase2Info remains bare free, V92 mapping
helpers remain unguarded and automatic embedded destruction stays generated.
Audit the full ownership boundary and keep genuine child lifecycles separate
from temporarily masked or pre-released guard fixtures.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/modem-owned-delete-recovery.md).


A bounded owner screen can recover source lifetime without recovering bytes.
V90Demodulator all-eight removes128B and leaves only a dead-pop mismatch,
but loses constructor identity and changes a progress jump-table layout
(F11622). Preserve that informative loss without adopting it as a gain or
calling19 cells exhaustive. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v90dem-owned-delete-controls.md).


Revisit a C-language exclusion only with independent TU provenance. F11430
places VPCMXF_Delete and its class destructor in VpcmFloModem.cpp; ordinary
class delete then recovers109B from117B by suppressing a sibling free jump,
with all33 bystanders unchanged (F11623). Preserve the same-TU destructor
inlining boundary. [Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/vpcmx-delete-recovery.md).


Store duplication in the object can motivate a control-placement experiment
without proving original duplicated source. CID reset branch-local clearing
recovers118B shape but remains BYTES8 (F11624). Both exits'EAX0 separately
motivate an ordinary zero-result return hypothesis; its crossed int cells
still miss (F11625). Close the finite family, preserve baseline controls and
ABI uncertainty, and do not expand into register or return-type spellings.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/cid-reset-controls.md).


An equal-size output-helper candidate is still a negative result when its
complete body differs. FPM_div's helper/word-index cross reaches150B but
BYTES105; independent initialization ownership after the zero guard gives
BYTES108 (F11626). Count all emitted functions separately from blob-common
symbols, reproduce repeated controls, and close the declared family rather
than perturbing counter scope, loops or frames. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/div16-normalization-controls.md).


Separate postreload scheduling from x87 stack conversion before classifying
exchange differences as allocation. Notch's sched2 control changes54B to48B,
but neither addition tree becomes exact (F11627). Scheduled load hoisting
precedes inserted exchanges; a disabled-pass near miss supports neither a
production option change nor invented coefficient locals.
[Stage controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/notch-addition-tree.md).


C++ TU provenance enables delete hypotheses but does not establish pointee
lifetime. K56's actual factory pointer reaches class API calls and+0xc record
accesses, yet all relevant class methods are empty (F11628). Keep receiver
use, record extent, sizeof and ownership separate; do not select scalar or
array delete solely to suppress a sibling free jump.
[Audit](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/k56-owner-boundary-audit.md).


Audit added defensive guards against the actual first memory access. Removing
dp_wrapper_delete's unsupported null early return recovers85B EXACT (F11629),
while both other TU functions stay unchanged. Preserve independently observed
child guards and validate actual constructed lifecycles; do not invent invalid
null fixtures or widen the blob contract. Aligned whole-object allocation can
stay constant despite a shorter exact function.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/dpw-delete-guard-recovery.md).


An unsupported defensive guard is a source-contract lead, not a guarantee
of a complete byte recovery. FixedRC reset needs external memory-call
boundaries too; their cross restores270B shape but remains BYTES41 (F11630).
Explicitly review replacement imports and shared helper callers. Tone creation
loses its added allocation guard yet remains SIZE90 (F11631). Close these
finite domains and review distinct allocation/dispatch boundaries instead of
perturbing registers or accepting only a nearer size.
[Reset controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fixedrc-reset-controls.md),
[tone controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/tone-allocation-guard-controls.md).


Preserve guard extent, not only the existence of a null check. FixedRC's state
check guards the state free itself, and child free operands reload the owner
between unknown external calls (F11634). Both properties recover113B Delete;
either partial cell does. Pair host allocation and deallocation, restore only
observed partial clears, and validate asymmetric created-owner lifecycles.
Full factory ownership can be source-supported without closing factory bytes;
keep those residuals and intentional import/jump-table changes visible. A new
ledger must first fire on the known baseline mismatch:162/652 failures became
652/652 passes here, with six real V22 bridge witnesses restored.
[Recovery and bounded domains](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fixedrc-factory-recovery.md).

Recover a loop's widths and traversal independently before testing helper
factoring. V34 receive queue's eight counter/output/readwidth controls miss;
direct conditional wrap then eliminates the helper's cursor temporary and
recovers70B EXACT (F11635). Audit the inlined caller too: V34agc changes only
in its queue prefix, with complete later body unchanged. An exact function
gain can still reduce whole-link positioned matches; report both.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-rxqueue-recovery.md).

A captured child load is a valid source lifetime hypothesis, not a promised
byte match. CID string capture restores the pre-loop owner access but stays
SIZE1 (F11636). Keep capture after the external clear, audit actual allocation
and output/owner boundaries, and close the finite family rather than expanding
register or pointer synonyms. [Control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/cid-string-capture-controls.md).

Separate sample-read order from cursor-helper factoring. V34 transmit queue
needs both to recover68B EXACT (F11638). A fixed overlap witness detects the
baseline's lost highhalf sample, while ordinary separate-buffer tests cannot.
Label that allocated component probe synthetic rather than asserting modem
reachability. Preserve other helper users and report alignment-absorbed size
changes. [Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-txqueue-recovery.md).

Return-width and predicate hypotheses need complete bodies, not selected
instructions or equal size. FIFO8 read's wider results remove sign extension
but remainSIZE14 (F11637); bitreverse's statement predicate reaches64B but
emitsTEST/JE/MOV instead ofSETNE (F11639). Keep both negative controls,
preserve ABI uncertainty, and close each family without adjacent synonyms.
[Return controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fifo-read-return-controls.md),
[predicate controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-bitreverse-init-controls.md).

Inspect both comparison operands in initial RTL before inferring source
width from the final CMP. Decision's short best_dist alone still compares
SI values; both short distance locals produce HI comparison and CMPW
(F11640–F11642). A normalized unused return register can motivate a bounded
API control without uniquely recovering its declaration. Even after cursor,
comparison, guarded reads and result normalization reproduce, differing
spills and entry layout do not authorize register/declaration variants.
Preserve repeated raw controls and close the finite family.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-decision-controls.md).

A recovered countdown backedge does not prove the entry boundary is recovered.
FloatFIR's postdecrement control keeps the authentic zero guard but introduces
a second entry test, growing287B to303B with no exact gain (F11643). Preserve
the complete control and its denominator; do not remove observed guards or
expand counter synonyms to fit the partial result.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/floatfir-countdown-controls.md).

Repeated word counter narrowing is source evidence but not a guarantee of
complete recovery. V34TimingHPFilter's int→short control restores increment
narrowing and word comparison, reaching76B yet remainingBYTES29 (F11644).
Even alpha comparison fails prologue ordering. Keep all26 function/48 data
controls, distinguish recovered operations from complete identity, and close
the width family without arithmetic or register permutations.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-hp-counter-controls.md).

Separate loop traversal from recovered arithmetic. V34 echo cursor/count
controls reach reference sizes yet fail complete/alpha bodies, with meaningful
bystander changes (F11645/F11646). Preserve original zero-count boundaries,
report all 26 function/48 data controls, and close the finite families.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-echo-cursor-controls.md).

A conventional advancing structure pointer can recover an indexed loop's
observed cursor without changing widths or arithmetic: V8 DFT energy becomes
74B EXACT (F11647). Validate all siblings/data and production raw promotion,
then report whole-link layout effects separately. Register local headers for
every experimental source directory; setup failures are invalid, not compiler
rejections (F11648). Preserve the failed setup and prove the corrected baseline
raw-reproduces before counting any candidate.
[Recovery and apparatus control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-dftenergy-recovery.md).

Counter width can determine whether GCC reverses a loop. V8 queue int counters
reverse to countdowns; short counters retain the blob's forward narrowing
and recover two complete94B helpers (F11649). Cross independent helpers to
separate each gain, and audit inline consumers too: V8agc's changed prefix
includes scheduling/alignment, while its complete later suffix and relocations
stay identical. Do not infer global original flags from two source recoveries.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-queue-width-recovery.md).

An advancing sample cursor can reproduce addressing and outer-counter spills
without recovering the complete loop. DFT update's cursor grows150→172B
against164B and fails instruction count too (F11650). Keep sample loads inside
the bin loop to preserve alias lifetime, review the whole TU, and close that
traversal domain without promoting a partial match.
[Control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-dftupdate-cursor-controls.md).

Preserve ascending overlap behavior when recovering pointer-copy source.
V8 coefficient copy keeps its signed-short loop and changes only indexed
assignment to *dst++=*src++, recovering45B EXACT with all twelve siblings
unchanged (F11651). Report alignment-absorbed growth and positioned-layout
loss independently. [Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-copycoeff-cursor-recovery.md).

An independent word-counter/coefficient-cursor cross can recover both predicted
operations without closing a FIR body (F11652). Retain valid indexed history;
do not add an unused final before-array pointer decrement to fit disassembly.
Separate the recovered operations from merged-history and accumulator residuals,
and close finite source domains without forcing spills or registers.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-fsktx-source-controls.md).

A typed embedded-subobject owner can recover compact offsets and lifetime
across a call without changing store order. ANSam's six tone-pointer accesses
plus owner-relative envelope_phase recover85B EXACT (F11653). Review sibling
bodies and anonymous tables too: raw jump-table addends move with function
alignment, but each function-interior target must still agree. Report that
controlled raw-data change explicitly, not as unchanged nontext.
[Recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-ansam-owner-recovery.md).

Dead-index removal can enable automatic reversal without closing call-boundary
normalization. Tonequeue's output pointer recovers ADD2/countdown but stays
SIZE4, with meaningful inlinehandshake/relocation-layout changes (F11654).
An exact byte-consuming callee does not require upper argument-slot bits to
be normalized; do not infer a unique wider prototype from their absence.
Close traversal independently of any future ABI investigation.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-tonequeue-cursor-controls.md).

Read-only reduction cursors need their own control even when neighboring FIR
cursor domains failed. Echo history-energy restores load/ADD2/countdown but
merges entry with backedge, missing the blob's distinct skip/count copy
(F11655). Use initial loop diagnostics to distinguish compiler strength
reduction from source traversal, then preserve complete negative evidence
and input boundaries. [Control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-echo-energy-controls.md).

Treat observed byte-sample lifetimes as independent boundaries, including a
load between two output stores. V22 control's three capture axes still fail
full bodies despite the closest148B versus145B (F11656). A whole control-byte
value domain on disjoint objects does not cover alias timing; require valid
object/representation evidence before claiming a behavioral difference or
original API contract. Do not choose captures or retype flags by nearest size.
[Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v22-control-capture-controls.md).


Cross count-capture timing with old-value postdecrement when dispatch and
loop flags independently support them. MakeTxData’s combined spelling
recovers the complete body and five function-relative table targets; neither
axis alone does (F11657). Anonymous section offsets remain UNRESOLVED in the
strict comparator. Preserve that distinction until a general resolver proves
table extent and targets with negative controls; never replace target identity
with masked addends or a named exception. [Evidence](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v22-txdata-recovery.md).


Anonymous jump-table identity needs guard-derived extent and ordered relocated
instruction destinations, not table-byte masking (F11658). Prove a separate
bounded detector on real ELF positives and explicit refusals before modifying
canonical grading. Review control transfers beyond Jcc: LOOP, far/prefixed
jumps and entries into guard instruction interiors can bypass an incomplete
parser. Preserve unsupported cases as unproved, and state the ABI-entry
boundary. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/anonymous-jumptable-proof.md).


Integrate proved table identity through the actual shared body/verdict path
and retest every refusal there (F11659). Ordinary anonymous data must stay
unresolved; changed ordered destinations must be RELOC. Known incoming-edge
checks need prefix handling outside the owner too. Review the full exact-set
diff and production hashes: MakeTxData’s 195-byte gain completes a previously
committed source recovery, while the comparator stage emits no new code.
[Integration](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/anonymous-jumptable-proof.md).


Separate local type lowering from allocator state. updateAlpha’s short
quotient restores the HI predicate in initial RTL and combine but leaves
its SI arithmetic pseudo, allocation conflicts and two spills unchanged;
full body becomes205B versus169B (F11660). A recovered TESTW is not a
recovered source body or a spill improvement. Preserve compiler metadata
normalization narrowly and delimit diagnostics before allocated instructions.
[Two-cell control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-alpha-width-controls.md).

Trace allocation boundaries before calling stack traffic register pressure
(F11690-F11691). The isolated updateAlpha reproducer retains the full-TU spill
graph without its debug path; read-only GDB shows global allocation evicting
the locally coalesced numerator/result to give EDX to the divide remainder.
Reload then allocates their stack slots. An in-place energy update, supported
by the blob's ADD/SAR on the same register, keeps the denominator cross-block
and restores register-held arithmetic. Crossed with the independently
observed HI quotient boundary, it recovers the complete169-byte body. Neither
axis alone is exact. Require raw objects from container/plain/GDB controls;
record missing debug variables and rejected dump options separately. Review
inline consumers and register-renamed siblings before adoption.
[Issue #246 tools and controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-reload-tracing.md).


Classify unresolved relocations by the actual consuming instruction before
expanding a dispatch proof. The five remaining complete-body candidates are
four jump-table consumers and one REP aggregate initializer (F11661).
A typed source literal and equal immutable payload prove read values but do
not alone prove canonical destination identity. Audit references/overlaps and
address escape; retain ordinary-section refusal while that distinction is
open. [Screen](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/anonymous-jumptable-proof.md).


A post-store reload is a testable source boundary, but recovering it alone
does not recover the whole function (F11662). Minimum-level diagnostics gain
the blob's second cfg+0x60 read through initial, combine and allocated RTL,
yet remain101B versus104B. Review every sibling and nontext relocation, and
count allocated instructions separately from diagnostic listings. Do not turn
an unexplained root+4 carrier into an invented header or volatility variant;
separate-storage fixtures do not establish reachable aliasing.
[Two-cell control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-minlevel-reload-controls.md).


Separate eager evaluation from Boolean-width recovery (F11663): replacing
logical OR with bitwise OR restores the unconditional parameter read, yet
integer promotions produce OR32/AND1 rather than OR8/MOVZBL. Inspect the
callee and inline consumer before drawing a return-type conclusion. Removing
an unsupported index initializer can likewise change scheduling/alignment
without closing the body (F11664). Prove first assignment mathematically,
keep synthetic fixture labels, and preserve failed diagnostic-dump baselines
as invalid; a no-dump control must still reproduce raw production.
[Controls and mandatory scope review](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v90-predicate-initializer-controls.md).


Before changing branch cost, inspect the compiler's default and every folding
gate (F11665). i686 already has cost2; fold_truthop additionally requires two
comparison trees and simple RHS operands. A captured bool predicate retains
branches, while a captured int value enables eager SI folding. A bool result
can restore QI OR yet leave an extra widening. These are distinct pre-allocation
mechanisms, and none is full-body recovery. Caller EAX tests support the int
API; narrow operations alone do not establish a narrow return declaration.
Explain moved table addends using independently checked named-function offsets
and instruction boundaries; keep that object review separate from grading.
[Closed domains](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v90-predicate-initializer-controls.md).


Cross owner lifetime with independently observed scalar boundaries before
attributing a wrapper gap to allocation (F11666). B103 reload recovery and
unsigned result extension are distinct effects; removing the explicit short
count cast emits identical code under the current prototype. Close that
conversion family rather than inventing a caller signature. Signed16 input
loads and zero-extended results bound implementation semantics, not uniquely
formal declarations; validate wider-result behavior with real initialized
components and adequate buffers. A short reference alias can hide the high
result bit even while differential tests pass.
[Full-TU controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/b103-transmit-boundary-controls.md).


Observe full scalar results at a valid initialized boundary before treating
return-extension differences as allocation noise (F11667). A signed-short
reference alias hides results with bit15 set. Bound buffer indices separately
from input formal ranges: MRF accepts positive signed16 inputs, but its signed
output index limits this fixed 10:9 boundary to32768 outputs. Preserve and
exclude invalid over-limit probes. Audit the shared header and every caller;
unsigned-short versus narrowed-int result families can emit identical objects
without proving the original declaration. Caller-local narrowing remains
independent. [Measured controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/mrf-result-width-controls.md).


For scalar loop widths, separate narrowing at the update from narrowing only
at function return. The MRF counter-only control restores signed truncation
after increment but does not recover the full helper. Ring-next narrowing
must precede the wrap comparison to reproduce the observed boundary. Keep
convolution indices at their separately measured width; cross independently
evidenced families instead of enumerating arbitrary local-type permutations.
[Ongoing bounded MRF controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/mrf-counter-width-controls.md).


MRF's nine-cell countdown/conditional cross (F11669) restores all predicted
local boundaries without full identity. Input-loop old-value nonzero tests
and conditional-zero destination identity are independent source mechanisms;
equal lengths can hide different bodies. Close the declared family after
complete review, retain losing explanatory controls, and reframe before
more nearby type/order changes. [Ledger](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/mrf-counter-width-controls.md).


Inspect Boolean predicate lifetime across x87 status clobbers before calling
a mismatch allocation-only (F11670). Power-first && recovers an exact72-byte
loop where index-first && skips a comparison, index-first & carries a predicate
across FNSTSW, and power-first & introduces integer-promotion zeroing. Ordinary
logical operand order can recover both access and lifetime boundaries without
casts or forced registers. Cross access/order/operator controls and compare
complete bodies, not just sizes. [Full-TU recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v90-power-index-predicate-recovery.md).


For a commutative op with two memory operands, source operand order survives
to RTL and assigns which load feeds the accumulator and which stays the
memory operand; for a REG+MEM pair the same order is canonicalized away
(REG first) and no spelling reaches the other two-address form (F11671).
Test the flip on BOTH shapes before concluding anything about "operand
order" in general: the encoder's flip recovered the exact 54-byte body -
accumulator `movzbl (%edx),%eax` then memory `xor (%ebx),%al`, with the
input-pointer increment scheduled after the xor as a consequence - while the
decoder's flip emitted a byte-identical object, closing its residual as a
reload form choice rather than a spelling. Exclude re-read spellings with a
behavior argument first: reading `*in` again after the `*out` store changes
`out == in` results, and recorded aliasing parity pins the one-read temp
form. A stale ratchet floor can report a loss the census refutes; check the
symbol against the current exact set before treating it as a regression
(F11671, third documentation of the same pre-existing 810-floor
V90Parameters C2 entry).


An if/else pair whose arms are one increment and one reset can emit with
either arm as the fall-through; the blob's choice is readable from which
block the conditional branch targets, and the mirror spelling of the arms
is the source lever (F11673). Check the tree for an INLINE copy of the same
test before compiling: getV90Decision's state-9 copy already carried the
author's arm order, and the mutation anchors quote it -- corroboration that
cost one compile instead of a domain. A cast on the READ of a struct field
folds to a nop conversion and cannot change the load's extension; the
extension follows the field's declared type, and a raw displacement probe
over a TU is contaminated by any other object at that offset (F11673).


A store whose emission position matters can be unpinned from its source
position only when the scheduler is indifferent; when the blob places it
after an unrelated load, the source position is the lever and the load's
own position is the answer sheet (F11675). For two loads feeding two
stores through a pointer the compiler cannot prove disjoint from the
destination, adjacent reads are the only spelling that yields
load,load,store,store -- interleaved statements serialize on the first
store. And check row-indexed rejection reports for desync before reading
a WIDTH/operand row as a type difference: one shifted store re-numbers
every row after it.
## Cross source arithmetic boundaries with observed induction widths

F11692 transfers updateAlpha's in-place arithmetic diagnostic to
V34TimingHPFilter, without transferring its spill explanation. Short index
alone matches length but leaves BYTES29; in-place carry multiplication alone
also fails. Together they recover all 76 bytes. Inspect combine before local
and global allocation: a named carry update can change the two-address operand
and lifetime even when neither object spills. Feature screening is triage;
stack references are not spill evidence.

F11693 recovers V34TimingFiltersInit's six fixed-offset stores and short
induction, then its member-relative history clear. A byte-size expression used
as an element count reproduces the known D29 overwrite and unsigned loop bound.
A flattened pointer clearing the same bytes need not produce that address form.
Keep reproduction defects guarded and preserve the shipping initialization.
Do not claim six explicit stores uniquely recover the original array declaration.

Audit allocated nontext with canonical relocation targets: a preceding function
shrinking moves a jump table's raw addends while preserving its destination
function and relative offset. Verify that identity before masking bytes.
[Screen and 27-cell reproduction](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-candidate-screen.md).


## The last narrow consumer can select the extended load

F11694 reopens v8_crc's rejected extraction-spelling family with a different
question. Baseline expansion resembles the blob, but combine removes its
signed extension; the signed-field negative retains a shared HI load and both
extensions, then regmove folds the last extension into the load. GCC3
optimize_reg_copy_3 requires the narrow pseudo to die at that consumer and
rewrites earlier users through subregs. Signed predicate first, unsigned read
second makes that extension unsigned and recovers the entire41-byte function.
This happens before allocation: do not label the operand-role mismatch regalloc.

`-fno-regmove` is an insufficient disabling control: the extension folding
runs under expensive optimizations in the forward pass. Preserve its measured
losses instead of treating an accepted flag as proof a pass did not execute.
[Thirteen bounded controls and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-crc-extension-promotion.md).


## Mask tests may be compiler-expanded grouped cases

F11700 recovers VPcmV34GetQuickConnectIndication's59-byte body with a grouped
switch and one initialized result. An early-return switch has identical length
but misses55 bytes. GCC3 stmt.c emit_case_bit_tests introduces the masks at
initial RTL; ordering counts merged case nodes, not represented values.
Inspect the source control's branch/result lifetime as well as the mask values.
[Three-cell switch control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-accessor-boundaries.md).

## Keep a multiply input live until its later consumer

F11701's SNR loop computes product before copying old v to last; the negative
copies first and permits destructive multiplication. Combine retains the
product72/v64/last63 use boundary before allocation, recovering all99 bytes
when crossed with the independent second-loop handoff. Flat-first alone
raw-merges production; the handoff alone merely matches length. A named
intermediate is evidence-supported when the blob consumes its input later;
it is not permission to introduce temporaries for register scores.
[Bounded loop/product controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-accessor-boundaries.md).

### Cross counter width with store order while preserving argument lifetimes

V34 detectorinit's signed-short nested counters recover its entire loop region,
but the tail still differs. Keep that independently measured width correction
while crossing the bounded source orders of the remaining independent stores.
A literal copy of machine store order changes the coeff argument's load role
and misses; coeff-first source plus count/state/armed/limit/lo/hi matches186 bytes.
Initial/combine RTL separates the counter extensions from allocation and
scheduled field-store order. A machine order does not uniquely identify source
order: use controls and retain argument-load boundaries, rather than assign
register names or broaden a synonym search. One exact candidate out of twelve
permutations is evidence within that domain, not global source uniqueness.
[Full-TU controls and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-detector-boundaries.md), F11702.

### Finish one arithmetic channel before the next product

V34 dftupdate's reference PUSH/FILD of the real product precedes imaginary
IMUL, while the reconstruction computed both products and integer updates
before either floating update. Recover the complete real channel before the
imaginary product and inspect combine: the real conversion moves before the
imaginary multiply, changing lifetime overlap before allocation. Combined with
the independently measured advancing input cursor and per-bin input load,
this recovers all200 bytes. Each axis alone misses. Keep wrapping integer adds
and x87 operations/rounding points intact; cross the source boundaries rather
than fit the ensuing registers. [Twelve full-TU cells and pass controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-dft-loop-boundaries.md), F11703.

### Trace a neighboring gain before interpreting it as source recovery

An independently observed array-base local in V34TimingPrefilter makes the
following, unchanged V34EqualizerCleanUp exact. Compare its actual RTL across
stages while excluding only compiler addresses and MEM alias annotations:
instructions agree through postreload, and the first change is peephole2's
constant scratch. This is a translation-unit scratch carrier, not evidence of
different cleanup source. Recover the address boundary on its own object
merit, report the neighboring gain separately, and keep the nonexact precursor
open. Here a late-coefficient control is closer to the precursor's product
schedule but loses the cleanup gain; that prevents claiming a unique original
source or freezing the early-coefficient choice. Bounded initialization controls
also miss. [37 full-TU controls and firing detector](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-prefilter-boundaries.md),
F11705. The related rollback domain is closed without adoption (F11704).


A constant-sized initializer can still reveal a field-derived loop bound.
In `v8_phase_rev_init`, setting `half=32` then comparing the loop index with
`half*2` produces an invariant register64 and the object's compare direction.
A literal64 and a cached local `full=half*2` compare immediates differently;
combine distinguishes all three before allocation. The field-bound form
closes the77-byte function. Do not assume that a binary constant establishes
a source literal, and cross cached versus direct member bounds explicitly.

Retained empty loops can be real source structure. V8's detector initializer
contains a three-iteration short empty loop nested inside its history clear,
as well as a2x2 short section/tap clear. Restoring both gives267 exact bytes;
integer empty loops disappear and removing the short empty loop misses28bytes.
Identify the counter narrowing and the CFG from the object and RTL before
retaining an empty loop; inventing no-effect code solely to move layout is
not this mechanism. These two recoveries combine for344 exact bytes without
bystander changes (F11706; [declared controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/batch5-v8-detector-boundaries.md)).

GenericToneDetector extends the statement-boundary lever with a measured
ownership discriminator (F11707). A local quotient increment can be
if-converted to ADC even when the blob branches; storing/testing/incrementing
the member preserves its memory updates through ce2. Cross that independent
boundary with the observed reset count/accumulator order. Here both constructor
clones become exact267B while the out-of-line reset stays exact. A shared-zero
assignment loses reset, so do not treat all zero stores as freely reorderable.
Use complete-TU positive/negative controls, inspect combine→ce2, and test callers
that inline the changed method. The two process compare/accumulation boundary
hypotheses gave no gain; their residual is still open, not an invitation to
repeat F1991's closed operand swaps.

### Cross call-load lifetime with transition CFG instead of rewriting register names

Calling tone's saved amplitude is read before TONE_read in the blob and scaled
in place afterward. Current source read it after the call and consumed an
anonymous product. Recover both boundaries, then separate remaining-zero
transition/else-save from the nonzero-save/continue CFG. Source clamp and arm
order controls alone recover length, not bytes; only the combined lifetime/CFG
control is exact215B. Inspect combine's output-store source to demonstrate the
product-carrier change before allocation. Preserve the tone's measured original
period/phase bugs throughout. [13 full-TU controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/batch5-calling-tone.md),
F11708. V34nlencoder's analogous magnitude-update family canonicalizes and
misses; do not generalize an exact gain into a universal source recipe (F11709).


An original diagnostic is executable source even when the normal debug level
hides it. Recover its exact format, unsigned gate and call boundary, and compare
complete bodies: call_delete becomes158B exact (F11730). Count debug relocation
anchors only to triage; inlining, CFG duplication and shared tail jumps can
change counts without removing a message. The new gcc3_debug_anchor_census.py
reports its object/symbol denominators and a known0→1 restoration control.
The call/Psd finite negative families are closed in their local records.


A cursor/countdown transfer may need a different value-read boundary in its
sibling. FPM_lmsupd closes150B with hist[k--] and an advancing coefficient
destination captured before arithmetic. FPM_lmsupd2 needs its first narrowed
product computed before capturing that destination; both int and short product
carriers then emit the exact182B body. Initial RTL identifies the t/dest source
order before register allocation, and the failed earlier capture produces an
extra frame slot and coefficient load. Keep the measured common property,
not a gratuitous local retype. The two gains combine without moving the third
TU body; the isolated first gain still changes its non-exact sibling through
allocation state. [72-cell record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/batch20-dsp-boundaries.md).


Separate configuration spelling from literal correctness. V23's receive
constructor retains an authentic original version print and its observed
initialization boundaries, then an independent literal control recovers29000
from the object's0x7148 (F11732). Neither a near instruction match nor old green
fixtures justify keeping28998. Audit constant/table emission order explicitly.
Original B103 case/default diagnostics close two state functions (F11733),
but later shared-prototype changes can undo that match; measure the combined
TU rather than summing isolated winners.

The switch/branch factoring lever now has a two-axis control in float2Bits
(F11734): a case0/case1 switch recovers its common no-match return, while
placing the subtraction arm under a negated whole table predicate recovers loop
fall-through and branch direction. Neither source component is exact alone;
the combined fullTU reproduces283B and leaves the enclosing CPPacker unchanged.
Preserve unordered routing with whole-predicate negation, not an ordered
opposite comparison. Inspect initial RTL dispatch and table edges before
attributing layout to postallocation scheduling. Failed DIL zero-trip/code-load,
ModulusDecoder split-product/add, and Parameters fabs/store crosses are closed
bounded families, not reasons to retest source synonyms.

Before classifying small residuals as compiler-only, compare original debug-call
anchors even for completed source. B103's four missing prints explained most
of its SIZE residual and exposed authentic CFG, field-owner and widening-loop
boundaries (FINDING_B103). The object's sole emission order plus recovered prints
and config zero predicate make create exact406B and untouched exit exact49B.
The shrinking non-exact process exposed a true count..0 versus count-1..0 error;
record it as behavioral reconstruction and run the final batch's period gate,
not as an inert expression choice. Keep failed direct-param, unsigned-local,
split-status and count-address controls: none closes process's remainingSIZE11.

A float expression assigned to a double temporary is still a source boundary.
realfft's four h1/h2 locals retain DF destinations after previous operand-cast
cleanup; changing only those destinations to float recovers its exact505B
body. Initial RTL verifies DF→SF while x87 retains excess precision without
intermediate memory stores. Do not infer source type from the absence of a
spill. Full TU remains two exact symbols with identical nontext metadata/data.
[Width control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/batch20-realfft-width-controls.md).

Direct helper output pointers and delayed feedback history acquisition can
recover member-store lifetimes without recovering a function. FloatARMA's
cross reaches scalar392B but stillBYTES152; block remains80B short. Close the
bounded family and reject size-only candidates.
[Output/load controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/batch20-arma-output-controls.md).

A FABS mismatch can originate at two different GCC3 stages. For getTimingHistoryStd
(F11737), positive/complement ternaries fold before initial RTL, while the
negative ternary becomes ABS at21.ce2. Explicit positive/complement sign assignments
preserve the blob's compare, +/-1 load and multiply; negative-first statements
preserve the operations but reverse their layout. Cross predicate and statement
form, retaining whole-predicate semantics, before declaring arithmetic spelling
or register allocation exhausted. Exact body plus full-TU nontext/literal auditing
is required; record both initial and late folding boundaries.

Call boundaries can bound index lifetimes without permutations: V90 history
resample's original increment follows its read-only timing getter, while the
baseline carries next across the call. Late local alone misses; direct member
increment/wrap closes170B. Cross new source gain with earlier same-TU gains:
this six-cell cross retains both resample170B/Std66B and audits the14bystanders.

A signed-word callee load proves consumption width, not necessarily formal width
(F11738). Cross consistent declarations/definitions with narrowing at use and
caller return/owner lifetimes. Require the callee raw control and all consumers;
B103/V21's four transmit gains leave MRF raw unchanged. Compose same-TU winners:
the B103 Answer diagnostic-only gain is forgone at peephole2 in the combined
candidate, and is excluded from the batch count rather than fitted with padding.

Historical replay after a deliberate header change must use an explicit revision
snapshot, not suppress the default drift guard (F11739). The shared driver's
--historical-headers mode hashes Git inputs and records include paths; the raw
baseline remains mandatory. Default current-header mode still rejects drift.

A saved result register across a diagnostic can identify source result ownership
(F11740). ConnectionEvaluator retrain twins close only with an entry-owned
verdict and shared cleanup; equivalent early returns materialize constants late
and omit the saved register. Cross result definition boundary and shared/duplicated
cleanup before concluding extra saves are regalloc noise. Compile both twins and
all TU bystanders; a post-counter negative cell changes a later nonexact carrier,
while the chosen entry-verdict cell preserves it.

For a weighted update with unchanged member before arithmetic, distinguish cached
local and direct member product inputs. ConnectionEvaluator average's member
product and hot-first CFG are each insufficient; their cross reproduces113B.
Initial RTL shows the additional source member read. Preserve unsigned integer
conversion, full extended precision and the single final rounding; do not create
float temporaries or reassociate an expression to match a size. Cross independent
same-TU winners before adoption: all three gains here survive together.

Census opcode and call-count gaps require provenance before source hypotheses
(F11741). SpectralVerifier process inherits its extra FABS from inlined
printSpectrum; getTimingHistoryStd had a local conditional-sign boundary.
Similarly, two distinct addresses or debug relocations may reflect shared tails.
Trace the instruction back to the source owner before reopening a closed family.

Whole configuration capture is a separate source boundary from pointer-store
permutations. V22 FSE's bounded copy-before-assignment control recovers instruction
structure but leaves five register bytes. The initial decision to withhold it
pending strict identity is superseded by the completed period and mutation
validation below; complete-function identity is not a prerequisite for retaining
an independently supported source boundary.

A short source loop counter can still have an SI pseudo in GCC3 RTL. Tone IIR's
short tap counters expose low-word sign extension and cmpw boundaries while
pseudo mode stays promoted (F11743). Literal four-section factoring plus short
counters recovers most of both progress bodies but not strict identity; a separate
shift-array capture recovers another seventeen bytes yet remains three short.
Retain the measured preimages; do not invent padding or force stack slots.

Cold diagnostic blocks: address order is not callback execution order.
Trace branch targets and rejoins before adopting source order. CALLPROG_Dial
parameter-29 debug read at higher address executes before the mainline read;
its CFG rejoins before that call. Branch-specific parameter rereads may be
observable even with constant getter values. Preserve host-read/printed
sequence and counts across valid lifecycle/debug states; require explicit
getter denominators and an old-source failing control. Equal function size
(1094B here) is insufficient: final residual remains BYTES422. F11742 and
https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/batch20-callprog-callback-order-controls.md carry complete-TU and runtime
controls, including the rejected intermediate seed rather than hiding it.

The incoming argument may be the original default result. _handle_status's
if-assignment form (F11750) keeps it live at entry and recovers44B; sparse
switch/return and switch/assignment alternatives do not. Transfer result-lifetime
hypotheses through actual dispatch forms, not a fabricated register carrier.

Compare read ownership across output stores before treating a renderer as a
register mismatch (F11751). CID's high output store separates two input reads;
eagerly cached nibbles hide that alias boundary. Predicate direction can expose
signedness even when earlier signed/unsigned source variants were raw-equivalent:
late read plus decimal-first signed-byte ternaries recover two complete bodies.
The changed assumption is the new observed branch direction, not a lower score.

Source cursor loops can expose GCC3 reversal that indexed loops block: V34 history energy compiles an ascending counter plus `*hist++` into the blob DEC/JNE. Trace `.09.loop` before inferring source count direction (F11754). For short paired history copies, flattened stores can remove genuine loop topology; restoring the two-step V8 loop recovers161B. Full-register shifts with a byte ABI may require an unsigned working copy before lookup, as charFlip proves.

Full-width masked message indices and branch orientation can interact: neither mask nor arm order alone recovers the eight modem reporters. Status stores can precede child/request sampling, and boolean clear-then-conditionally-set can preserve reads that direct boolean assignment hoists. Use actual alias/read boundaries and cross independent witnesses; do not treat store/read rearrangement as behaviorally inert (F11753). Existing union bytes can express observed byte flags without inventing a new shared type.

A common default result needs its original initialization boundary as well as its switch shape. vce_get_sreg clears its result after the external getter: moving zero before the call creates a callee-save live range and changes the prologue. Explicit guarded assignment1 instead of a boolean expression is also needed. Cross these independent instruction witnesses; the result alone is not a new general store-order lever (F11756).

CID extends callback-boundary reconstruction: an output buffer initialized after memset, combined with owner-relative stores, preserves the original call-setup scratch lifetime and reaches145B. Neither axis alone reproduces it. Capturing a child by itself did not explain the residual; trace the full owner and return-buffer lifetimes rather than fit register names (F11759).

A fullwidth cosine formal can eliminate caller byte extensions while the callee consumes exactly the lowbyte. Cross consistent callee/caller declarations with real output cursors and verify all header consumers; signed/unsigned full formal may remain indistinguishable. V8 TONEq90B needs both width and cursor, while ANSam remains nonexact (F11760).

For fully covered unsigned modulo switches, defensive result initialization can destroy the original live range. A common fullwidth result with explicit narrow negation reproduces six V90/V92 leaves when allcases are proven covered; never leave an actually reachable result uninitialized. Anonymous jump-table operands need proven ordered destinations and CFG/stack ownership, including meaningful negative controls. Measure comparator support changes on the immutable baseline before attributing source gains (F11761).

### Cross ownership with value lifetime before closing a return-spelling domain

The batch50 pulse digit separates original input from corrected count and acquires the child before the zero branch: neither alone matches, both recover146B (F11762). V90RDetector's compound member update first restores store/reload, then a positive complete-group guard retains the initialized verdict and closes four functions (F11763). Return synonyms being inert on one ownership graph does not exclude their interaction with an independently witnessed graph. Require the full finite cross and inspect unchanged exact neighbours; avoid arbitrary reordering or register padding.

### Compare expression mode and copy width, not just constant bytes

A0.5f→0.5 expression-mode change recovers V90 enterDataPhase190B while its entire constant pool stays byte-identical (F11765); don't infer source literal width from pool bytes alone. FIFO_create167B needs whole-config copy crossed with capacity zero-extension and signed cursor use; neither local signedness fix nor copy alone succeeds (F11766). Audit high-bit behavior from operands, not the signedness of one branch opcode.

### An early initialized common result can be the original debug-call lifetime

Voice DLE196B needs switch arms assigning a shared status initialized before callbacks (F11769). Literal returns remove its carrier; an equivalent if assignment graph still misses. The accompanying merged-string pool changes order but not values. Review allocated data order alongside code; exact functions are not full-object identity. Do not normalize an unrelocated local-call distance simply to bank an otherwise matching dialer: the unresolved callee/TU layout remains a separate witness.

### Snapshot callbacks only where the original acquires them

Rx voice_set_rx305B closes when callback-owner reads follow the original calls: the silence callback is acquired after detector setup and each gain callback is read from its owner at use (F11770). Capturing a function pointer at entry extends its lifetime across callbacks and can change both allocation and visible owner reloads. Cross acquisition timing with per-use owner access; preserve the callback prototype and returned-word interpretation. Neither timing nor direct gain reads alone reproduces this example.

### Reconstruct the state that lives through diagnostics

V90/V92 initiateFPE/RRN assignments before diagnostic calls reproduce four leaves; equivalent assignments after calls prevent original late tail merging (F11772). A residual attributed to crossjump freedom can still have a recoverable source carrier. Use pass dumps to locate the first actual merge: here26.postreload retains both sites and27.flow2 merges only the recovered graph.

Fax reversal requires crossings of loop-counter width, postdecrement grammar and shift-before-OR narrowing (F11771). Matching one loop opcode alone does not establish the source. Literal frame open-coding additionally needs the original unsigned header stride; inspect high-bit address extension separately from loop branches.

### Read declared union views at the original width

A byte flag write followed by a word-mask test can differ from two byte-member operations even when their values agree: GCC caches the byte value across a conditional join and adds a CFG edge before register allocation (F11773). Initial RTL distinguishes QI byte from SI word guards, and the post-GCSE loop dump first distinguishes their jump counts. The original word read plus an exact sister function establishes the union view; use existing declarations and corresponding shifted masks. This recovers RxHdxIdleV17/V27 without volatile or alias tricks. Pair arm orientation with memory countdown ownership and narrow callee-result use for related epoch states; none of the partial source controls suffices.

Whole enclosing-object ownership must sometimes cross both pre-call reads and post-call stores (SetScramblerV27, F11775). Holding the nested helper pointer on either side alone does not reproduce original92B. Preserve the observed post-call owner reread rather than caching a child across initialization; inspect the enclosing address formation, not just the final equivalent field offset.

Separate variable and fixed clearing arms may be source structure, not compiler threading (_idle_state, F11776). Check original loop bounds and adjacent exact witnesses: named existing bound160 retains CMP160/JL whereas a literal loop canonicalizes to CMP159/JLE. Cross this boundary with the observed arm graph; do not infer source from length alone.

Anonymous tables in ordinary push-save functions may be comparable even with direct calls (F11777). Prove the frame, ABI call contract, protected guard and every ordered case destination before canonicalization; do not mask anonymous relocations. Validate rejection controls through the actual comparator and audit old/new immutable baseline classifications separately. V29 transition byte writes then recover467B with zero unrelated body changes.

Return the observed owner, even on a null diagnostic path, when its lifetime is visible (create_dtmf, F11778). A literal returnNULL removes that live-through-call object and may duplicate the epilogue; a failure diagnostic or positively guarded initialization followed by common return recovers228B. Adjacent constructors can still miss under the same family: keep the complete-TU negative control rather than generalize from one win.

Arithmetic preparation can begin across a store group while completion remains afterward (_recieve_silence_state_init, F11779). Compare the whole dependency chain: signed half-count SHR+ADD before state stores and SAR after them witnesses an earlier initialization of the existing local. Transfer that observed lifetime rather than permute stores until scheduling matches.

Existing helper factoring can settle nearly an entire wrapper while leaving a dead scratch register (FDSP_DP_Run, F11781). Calls to its two conversion helpers reproduce137/138 bytes and all relocation destinations; remaining POP differs first atpeephole2, not register allocation or CFG. Cross independently proven predecessors before attributing TU history. Do not alter a return ABI or add dead locals to bank the last byte. Final scheduled RTL may place epilogue instructions before the epilogue note; use the complete function to confirm surviving instructions.


### Inspect terminal source edges before attributing a constructor frame to allocation

An explicit return in a switch's final diagnostic arm can select a sibling jump where break retains CALL and cleanup (V92Modem, F11782). Here it restores the original frame and exposes a separate retained parameter versus mutable-member reload across construction calls. Cross the two observed boundaries: neither alone reproduces either constructor, both recover both clones. Initial RTL and02.sibling distinguish source age and terminal control before register allocation. Do not generalize return synonyms without the original tail-call witness.

Inspect period libc headers and preprocessed source before assigning an FPRem to unsafe optimizer algebra (PHASOR, F11783). Glibc's __FAST_MATH__ inline wrapper may explain it even when macro-only and optimizer-flag controls are raw-identical. Keep diagnostic profile changes and exact losses visible; a wrapper preimage is not a recovered whole profile.

Duplicated cyclic scratch arrays can support affine state-index arithmetic without modulo (VTB, F11783). Prove the index range over the actual state domain rather than replacing every read with XOR. A new private emitted helper is an inventory change to review, not permission to force inline; literal source expansion is a separate bounded control. Close raw-equivalent loop spellings instead of expanding the syntax domain without new witnesses.

### Classify complete mismatches before assigning an allocator cause

F11789's immutable300-object inventory accounts for811 nonexact symbols;
only40 pass the complete existing alpha proof with proven relocation identity.
Instruction differences, size/call gaps and unresolved destination proofs are
observations, not an inlining-budget diagnosis. Use typed call destinations,
not printed relocated call-site offsets. Score every defining copy, not a
favourable COMDAT. [Inventory and witnesses](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-mechanism-classification.md).

For a dead epilogue scratch, observe the installed compiler's TU-wide search
history before changing source. F11790's read-only GDB observer preserves all
five emitted objects raw, while a captured eligibility model reproduces154
supported choices out of163 recorded searches. Removing the evidenced final
predecessor predicts and produces a different scratch, but removes an export
and cannot be adopted. The cursor required for ECX is a conditional constraint
under that captured state, not recovered original flags or TU ordering. Never
insert dummy allocations or permute definitions just to obtain the cursor.
[Reproduction tools and results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-mechanism-results.md).

Extend the terminal-edge lever with an instruction-boundary relocation census:
F11791 finds three candidate bodies, of which explicit default return closes
V92Modem::reset138B at02.sibling. The same spelling misses V90 reset and is
inert in ADID; inspect complete bodies and independent witnesses before a
cross. Signedness can recover an original loop branch without recovering the
function: F11792's four constellation controls gain zero and close that finite
domain. Keep negative controls as evidence; do not adopt partial score gains.


### Distinguish a two-address destination from an expression of the same width

A byte XOR can differ before allocation even when both source forms already
expand in QImode. ParallelDifferentialDecoder's preserved input feeds a later
state store; a separate narrow decoded result updated with XOR supplies the
original destination identity (F11795). Combine keeps input first and folds
state memory second, letting reload emit the original byte copy/memory XOR.
This closes57B where cached-pointer and commutative spellings did not. Trace
initial/combine/reload operands, retain alias-visible store ordering and audit
all compiler-discovered header consumers. Do not add temps just to tune colour.

Expression mode transfers across independent original half-load/pop witnesses:
ADID's two mean methods need double addition although their stored0.5 constant
is still float-sized (F11794). Cross sister methods and inspect inline consumers;
constant bytes do not determine expression type. The same discriminator narrows
F7846's Uref to an extra four-byte frame allocation (F11796), not an exact
function. A complete instruction-sequence match with wrong stack offsets remains
nonexact. Require a slot-allocation/lifetime witness before more source forms;
never normalize away the frame to obtain a gain. [Controls and results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-value-carriers-results.md).


### Separate local slots from known-callee alignment

A four-byte frame mismatch need not be another local or a global flag. Trace
assign_stack_local_1 and ix86_compute_frame_layout in the installed period
compiler, preserving emitted objects raw. Uref's four allocations agree;
the difference is padding2 propagated from its already-emitted callee through
cgraph_rtl_info. Recovering the original constant quiet NaN instead of a
library nanf call lowers that callee's known incoming boundary128→32bits;
crossing it with independently evidenced double-half addition closes240B
(F11798–F11799). Keep flags/function order fixed, inspect the callee's object,
audit inline consumers and pool/table relocations, and require valid target
observations plus refusal controls. Do not infer dead locals from stack size.
The other three NaN sites supply bounded no-gain controls, not a general
license for spelling changes. [Results and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-uref-stack-results.md).


### Check publication, binding and saved-register padding separately

An emitted callee does not necessarily publish an incoming alignment value.
Installed GCC3.4.2 guards publication with binds_local; weak/template controls
return known info with boundary0, so callers keep the default16bytes (F11802).
Aligned double slots still publish4bytes in the strong-leaf controls: local
alignment is not a call requirement. Source-order pairs are raw-inert under
the retained unit-at-a-time pipeline; no production visibility/order fit.

Near-frame vector callers instead retain16-byte incoming-call alignment while
direct member indexing removes an extra saved register. The resulting12bytes
of padding disappear (F11804). Trace post-call value ownership and narrowing:
one vector uses a late conditional return, its sister uses an unsigned wide
level and one terminal short conversion. Cross the sisters independently,
audit losing single-factor cells and all bystanders. The unchanged TRN gain
is a measured peep2 cursor effect with matching eligibility, not additional
source recovery. All272 scratch choices replay; no cursor forcing or dead
carriers. [Screen, controls, audits and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-alignment-results.md).

### Pair counts with their derived lengths before fitting stack sizes

GenericIIR C1's extra saved register follows source ownership, not a new
alignment flag: the original count/derived-length pairs precede coefficient
pointers. The three-cell pairing×pointer-age boundary closes107B only when
both properties agree. Installed24.lreg puts blockSize inECX with12-instruction
span rather thanESI with16; C1/C2 copies alone change (F11806). Observe the
parameter's pseudo and local allocation before interpreting its frame.
Do not generalize an exact neighbor's statement order or enumerate permutations.

### Distinguish a wide unsigned magnitude from an unsigned short carrier

MOVZWL, full-width NEG and a terminal MOVSWL can describe an unsigned32-bit
magnitude with one signed-short conversion. A signed/unsigned *short* matrix
has not tested that family. Independent original witnesses recover V92 Ja77B
and E1u54B (F11807–F11808); fields/return ABI stay unchanged. Explicit post-call
capture is raw-inert for Ja. Inspect inline copies: E1u's five-byte removal
recovers the original88B arm even while the enclosing pump's SIZE gap grows.
Keep original load, arithmetic and conversion boundaries rather than fitting
aggregate length. CPt's wide near-hit was initially declined; the 5 November
tooling hour below replays and retains its independently supported conversion
boundary as a partial recovery, without claiming register closure.

Opcode screening is only a candidate generator (F11809). Require operand/path
tracing: NEG may build a quality mask or modify a different stored amplitude.
Six masked-count controls and four larger-pump ownership cells do not transfer
the gain. The pump's post-call lowering is already unchanged, and its remaining
remote tails need compiler-stage CFG evidence (F11810). Do not reopen cast,
ternary, declaration-order or flag matrices without a new discriminator.
Complete domains, audits and replay: [sample transfer results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-sample-transfer-results.md).


### Preserve a used boolean value before a zero-selection mask (F11812)

An original SETcc/NEG/AND sequence can distinguish a separately used boolean
from a direct conditional expression. In the period compiler, direct
`n & -(quality != bad)` and multiplication controls fold to branches already
in initial RTL. Capturing `int reliable = quality != bad`, then using its
negated value to mask a wide count before the terminal short return, reproduces
`RxHdxDataV17`. V27 and V29 additionally require independently witnessed public
argument/consumption widths and existing union byte flag writes. Cross these
axes: the width controls alone had previously failed. Review complete caller
and callee TUs, including non-exact bodies and every header consumer.

This is a bounded source-value ownership lever, not a reason to rewrite all
ternaries. Ring-wrap transfers recover SETL/NEG/AND but still miss complete
functions; an opcode screen must be traced to the same value/path. A widened
source carrier can be raw-inert once optimization keeps the narrow result wide,
so full-register index use alone does not establish an `int` local. Demapper's
BYTES1 SIB commutation and a same-size normalization helper remain misses.
No declaration, register, slot or equivalent-expression permutations follow
from these partial successes. Reopen a closed family only with an independent
original operand or compiler-stage witness. See
[next20-byteexact-results.md](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/next20-byteexact-results.md).


### Trace writable loop memory before calling a LEA/copy difference regalloc

A count updated through an output pointer can be promoted by GCC3's loop pass,
leaving one final word store and a separate next-value pseudo. A private int
counter followed by one terminal publication is a different source family.
Original LEA1/copy/backedge/word-store operands justify crossing these two;
`09.loop`'s explicit `Hoisted regno ... r/w from (mem:HI ... count)` diagnostic
establishes the promotion in the reconstruction. FPM_div additionally needs
helper-owned count initialization after the zero guard and becomes EXACT150
(F11815). Keep initialization placement as an independent witness.

The mechanism transfers to log/sqrt/V8 normalization without closing their
complete bodies. A split increment alone does not establish a missing helper:
V8's bit helper already updates pointed structure memory. Do not force output
slots, permute declarations/parameters, remove checks, or tune late registers
from near hits. Trace output storage lifetimes or the remaining source operand
boundary before reopening a closed family. Full domains, detector controls,
audits and replay: [loop-memory results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-loop-memory-results.md).


### Separate an aligned HI machine move from a wide source read (F11818)

GCC3's `*movhi_1` can emit MOVL for an aligned memory address while both RTL
operands remain HI. An original DWORD load therefore does not establish an
int local. Trace initial output homes and the final annotated machine pattern:
FPM_div_32's count moves from esp+18/MOVZWL to esp+16/MOVL when its two
addressed word outputs follow the independently exact sibling's declaration
boundary. Both separate/grouped forms match147B; the scalar-only control is
raw-inert. This is storage ownership plus address-dependent instruction
selection, not a late register permutation or artificial alignment fix.

Sqrt transfer restores every live instruction/operand but leaves one dead
POP byte; log result-owner controls are raw-inert. Keep those misses separate
from the reciprocal recovery and do not adopt partial bodies. A justified
output-order reconstruction requires actual initial addresses, source/storage
witnesses and complete-TU controls; it does not license permutation searches.
[Domains, causal traces and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-output-storage-results.md).


### Trace run publication and captured decisions before blaming registers (F11821)

An assignment receiving a helper return can publish a remainder after a loop,
while a pointed helper publishes it before rounding and pushing. V8's original
operands distinguish these families. Guarded-do countdown restores DEC/JNE;
flag-preserving moves can intervene, so require the real flag dependency rather
than adjacent mnemonics. Unsigned bit formals independently recover zero loads.

Trace decision arithmetic before dispatch: original SUB/TEST/SETG captures a
wrapped energy difference, silence replaces it with2, then a switch dispatches
0/1/2. A direct comparison is a different family at overflow boundaries. An
unsigned decision carrier restores original CMP1/JB where signed emits JLE.
Twenty complete-TU controls recover these individual witnesses but no exact
function. Do not adopt a near size, assume algebraic values are reachable modem
histories, or force a parameter subobject from its register base. Seek an
independent typed-extent/caller witness next.
[Bounded controls, full audit and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-remainder-owner-results.md).


### Compare predicate operands before attributing tap loops to scheduling (F11822)

V8's original tap loops use unsigned <= predicates. Changing only j to unsigned
changes their exit tests from GT/GT to LTU/GTU already in initial RTL. Final
JAEs/JBE differ in mnemonic because the first operands commute; inspect both
operands before declaring a failure to reproduce signedness. Keep signed
outer position/limit comparisons separate. A streaming input cursor with a
signed countdown independently restores the original first JNS loop while
preserving increasing sample addresses. These witnesses do not produce an
exact V8 body and are not adopted from a size fit.

Only demod forms the receive base0xc2c; mod forms transmit base0xc20, and the
initializer writes absolute fields. An explicit LEA/ADD census is bounded
instruction-form evidence, not typed-extent proof for a new subobject. Seek
independent source/caller evidence rather than fabricate aliasing structures.
[Five-cell domain, observations and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-index-cursor-domain.md).


### Follow real load UIDs through combine before blaming source evaluation order (F11823)

V8 coefficient-first expressions and a named first coefficient both change
initial RTL capture order and emit the same complete object. First CSE removes
repeated inline sample loads. These forms remain nonexact. Trace actual HI
memory reads: combine folds a load into the extension UID, so a disappeared
word-load UID does not prove the read disappeared. A surviving register-only
extension also does not prove a memory capture.

For the first input tap, coefficient-first survives allocation/reload/renaming
and31.bbro;33.sched2 reverses the reads to sample-first. More expression
synonyms do not resolve this observed scheduling boundary. Use verbose
scheduler diagnostics with a mandatory raw-object repeat, then inspect GCC3's
dependency and ready-queue decisions. Do not invent volatile/alias dependencies
or infer allocator causality from mnemonic order. A duplicate-stream dump
must be explicitly unparsed rather than silently selecting one stream.
[Four-cell domain, UID trace and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v8-product-capture-domain.md).


### Check consumed callback returns before treating EAX as incidental (F11827)

A void reconstruction adapter whose table consumer reads int status may have
lost its original return declaration. Read the caller and all count/result stores:
eight fax process adapters preserve the modem return across their stores in the
blob. Explicit int forwarding makes all eight EXACT62 and removes obsolete
callback casts without changing the dispatcher object. Audit every shared-header
consumer and table relocation; one exact wrapper alone does not settle the API.
[Return contract and ten-consumer proof](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fax-adapter-result-results.md).

### Capture one final field result when both branches publish it (F11825–F11826)

BitsToSymbol reset's if/else stores become one guarded unsigned result with a
conditional expression; both supported accumulator/conditional forms are raw
EXACT108. CID reset likewise needs final samples_fill publication after its
conditional mark update; the constructor's inline consequence is reviewed too.
Use original common-store/control-flow evidence, not arbitrary statement order.
[Combined gains and bounded negatives](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/byteexact-scheduling-batch-results.md).

### Read scheduler tie-break dependencies after allocation (F11824)

Equal-priority loads need not retain initial order. V8's coefficient/sample
priority35 tie is broken by4versus5outgoing dependencies, matching the official
GCC3 scheduler ranking path. Verbose-only diagnostics must raw-repeat the object.
Trace earlier ranking gates and actual ready selection before interpreting a
fanout count; include anti/output edges created by allocation. Original hard
register reuse can constrain order differently. Do not force registers or fake
source dependencies to reproduce it. [Measured diagnostic controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/batch-scheduler-trace-domain.md).


### Recover owner acquisition at the callback boundary (F11829)

FAXVMI_process327B becomes exact by reading the framer after processing
callbacks, at the original owner load. An entry cache extends a child lifetime
across calls even when the member is normally stable. Preserve initial scalar
snapshots separately from later owner reads. This transfers only where the
blob independently shows that boundary: V21 next-state and HDLC unframe reload
controls reproduce individual operations but remain nonexact. Trace remaining
conversion/inline graphs before further source variants. Full-TU audits must
include source-unchanged inline callers and reordered merge strings in negative
controls. [Domain/results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fax-publication-batch-results.md).

### Read the ownership of heterogeneous arithmetic operands (F11830)

voice_modem338B closes with det_len+saved: original word-extension and full-word
memory reads feeding ADD isolate the arithmetic operand graph. Keep existing
local widths and interfaces fixed; do not assign registers or permute unrelated
statements. Promoted ushort sum is bounded and final narrowing remains unchanged.
This is separate from detector formal-width controls. The33TU/26small-symbol
screen's known input fires, but its six-instruction pattern is a bounded triage
rather than proof every remaining sum was examined. [Evidence/replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/services-voice-count-sum-results.md).

### When the operation matches, trace the remaining compiler dependency (F11831–F11833)

Unsigned wrapper minima/remainders, original FSE copy boundary, ADD-then-SUB
reversal energy and GenericIIR member publication can each recover operations
without closing a function. Size proximity is not a next discriminator. Hold
the witnessed source graph fixed, find the first changing RTL pass, and predict
a new source or visibility boundary before reopening the finite domain. No
register/slot/type permutation or source/profile score selection follows from
these negative results. [Bounded batch and next phase](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/byteexact-contract-batch-results.md).


F11832's FSE trace now measures sched2 as the first changing pass:29validated
streams preserve copy→freq→phase_acc through allocation/reload/rename, then
sched2 emits freq→phase_acc→copy. Explicitly exclude the duplicate GCSE stream.
Inspect scheduler memory dependencies and priority carriers next; changing
allocation or declaration order is not supported by this trace alone.
[Replay tool](../../tools/fse_copy_stage_trace.py).


### Observe memory alias edges before fitting scheduling or registers (F11835)

The FSE cfg copy's type-set2 has edges to short stores but not integer fields;
those integer stores priority6 exceed copy5 and issue first. Builtin memcpy
set0 adds both edges, so copy8 must issue first. Two verbose raw repeats and
actual Gentoo ready/issue tables establish this mechanism. Official alias and
scheduler sources explain it, without recovering original memory tags or the
complete Gentoo patch stack. Neither control is byte-exact. Do not invent
integer/union fields, void casts, volatile or profile options to make an edge;
require independent original field/copy/API evidence.
[Trace, source hashes and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fse-scheduler-dependencies-results.md).

### Fixed-entry source expansion can restore the enclosing helper budget (F11837)

calcModulusParameters622B needs original five modulo/six division sites and
six scalar place stores; retained loops expose1/2sites. Expand both ordinary
fixed-entry sequences, then preserve the observed heterogeneous sum operand
roles. Full body becomes exact and getPower regains its original out-of-line
CALL as helper complexity increases. Audit the caller too: its574B shape still
has exchanged double homes, and all six jump-table destinations must be decoded
and checked against ordered original cases. Do not claim source-correctness or
profile uniqueness from recovering a helper call boundary. V92's CRC promotion
transfer recovers mechanism/length but no complete functions and stays unadopted.
[Complete-TU evidence](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/batch-cpp-constellation-expand-results.md).

F11838/F11839 additionally distinguish measured later optimization and actual
spill migration from a blanket inline-budget diagnosis. V32 source publication
survives reload then collapses by sched2; V29's standalone arithmetic swaps an
eq spill for a distance spill. New original storage/graph evidence is needed
before reopening either family. [Batch limits and next phase](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/byteexact-dependencies-batch-results.md).

### Trace late call merging and replication before inferring source repetition (F11842–F11844)

The widened C++/DSP screen nominates two branch graphs, not fixed-entry
expansions. In V92 reset, explicit length-arm diagnostics stay separate through
reload, merge in flow2 and duplicate again in bbro. V90 reset's arm-local DIL
calls merge in flow2 and stay shared; the complete object is unchanged.
Thus final relocation multiplicity cannot determine source statement count.
Use source RTL, postreload, flow2 and block-reordering dumps to distinguish
these mechanisms before expanding source. Preserve observable per-iteration
mode reloads and one diagnostic per execution. These finite source controls
gain no complete function; require a new original graph witness before reopening
them. [Screen, controls and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/expansion-wide-results.md).

### Compare backedges inside the witnessed arithmetic loop (F11845–F11847)

A whole-function edge count can both nominate unrelated branches and miss a
real inner-copy mismatch. V92 phase unpack has11backedges on either side, yet
the original32input CRC loop has one and the retained loop has two. Constant
member shifts remove the inner copy in all three receive CRC consumers, but
none closes completely. Identify the counter comparison feeding the specific
backedge; do not mistake a later checksum failure branch returning to reset
for an enclosing arithmetic loop. Audit all switch destinations after layout
changes and the other functions in the TU. This improves lever13 triage rather
than adding a source spelling with guaranteed gains. EIA6 also recovers18copy
operations but leaves real x87 lifetime/operand differences; SIZE2 is not two
different bytes. [Screen, graph evidence and closed controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fixed-index-results.md).

### A nonpopping live SI conversion can constrain the source mode (F11848–F11851)

GCC3 reg-stack strips FIX before handling its source and duplicates a live XF
value if the stack has space. It adds physical REG_DEAD, so the emitter pops
that copy. Live SF/DF sources bypass this rule. Therefore original FISTL with
proved live input and spare capacity is evidence against that XF producer;
final REG_DEAD alone is not evidence the logical value died. Exclude DI's
hardware-forced pop, full-stack cases and unproven entry/call history. Actual
Gentoo controls corroborate the hash-pinned stock rule; patch-source identity
and unique C types are not established.

The constraint is on RTL mode under the measured profile; an unknown
long-double ABI or TU option can change the declaration-to-mode mapping.

EIA6 SF/default-double math recovers FISTL/FMULP/FCOMPP, while its constant
10000 still occupies SF storage. Constant-pool width does not determine
expression precision. Those isolated controls and the two beta mode transfers
did not become exact. F11852–F11853 below close EIA6 with independent use,
lifetime and tail-reload evidence.
Trace register-zero folding at combine, register-scale folding at lreg and
later stack physical deaths separately; do not fit declarations/registers/
slots or remove a defined shift mask for a near-size match.
[Complete evidence, closed domains and tools](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/eia6-x87-results.md).

### Trace argument conversions, then verify the actual tail edge (F11852–F11854)

An apparent x87 scheduling difference may begin at combine: a fractional
FIX used by a conditional argument can sink into that argument after another
conversion. EIA6's builtin integer abs and explicit earlier if keep fraction
conversion earlier and restore magnitude conversion directly to outgoing+8.
They emit identical complete objects; neither alone proves original source.
An independently observed early FABS/live result then recovers the diagnostic
prefix exactly. Follow source uses and conversion homes through the passes;
do not fit types or stack slots to partial opcode agreement.

When a remaining copy arm has fewer available registers, compare the actual
tail jump target. EIA6's original bypasses a common pointer reload; ours lands
on it, keeping this live. Moving that reload to the else path, while retaining
reloads after both callbacks, frees the original copy pipeline and makes the
complete790B body strict EXACT. Real operand/use and branch-target evidence
supports this graph; register-colored rejoin addresses alone do not. Review
both paths and all callback effects before transferring the lever.

Conversely, an explicit count mask can create a genuine preservation edge:
beta's mask is in initial RTL, and regmove coalesces it destructively with the
count. Publication must precede it to preserve the original full count.
Do not remove defined masking or retry source order without a proved domain
bound or independent redundant-mask computation.
[Bounded controls, full-TU audits and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/eia6-scheduler-results.md).

### Separate captured arithmetic from alias-visible publication (F11856)

Two writes to one field can use different source values. V17TX_control first
publishes request scale, multiplies that stored value, then delays the product
store across another configuration write. Re-reading request or publishing
the product earlier changes unknown-overlap behavior and the register graph.
Whole-owner addressing plus common return recovers most of the body; an
explicit product lifetime and its original delayed store close strict identity.
V27's independent original also needs the first store and fresh request flag
reads after the conditional member write. Transfer each observed use age,
not an assumption that owner/request pointers are disjoint.

### Cross common results with the original dispatch tree (F11857)

A shared return can still leave different state comparisons and duplicate
tails. fax_class1_status closes only when its common zero/one result is crossed
with ordinary switch groups4..6 and12..13, matching the original ordered
range tree. Keep callbacks and their effects intact. Extra current epilogues
are a screening clue, not proof of missing source statements.
[Full-TU controls, declines and validation](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/callback-value-age-batch-results.md).

Historical floors need source/profile provenance too (F11855): V90Parameters
C2 was genuinely exact when recorded. A later supported float-field recovery
changed only its register choices, even though the changed writer stayed
byte-identical. Do not blame the comparison tool or compiler version without
crossed historical replays, and do not undo correct typing or lower a floor
merely to make the check green.


The constructor retype control now locates the first divergence precisely
(F11861): patterns agree through27.flow2; scratch selection differs first at
28.peephole2, and30.rnreg maps the later register colors. A prior writer can
retain its final bytes while changing SI-immediate scratch opportunities to
SF-register moves. Use pre-pass patterns and actual raw-preserving dumps;
final writer identity does not preserve compiler scratch-search state. This
corroborates the existing scratch-cursor lever, without measuring the full
historical dynamic cursor-event sequence or justifying literal permutations.
[Actual field-era stage proof](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v90-parameters-constructor-stage-proof.md).

### Separate assembly-layout clues from executed value ages (F11862–F11864)

A broad binary screen can find candidates missed by repeated-return/source-text
heuristics, but a load's position after a call in disassembly does not establish
that it executes after that call. Branches can lay out mutually exclusive arms
in either order. Bound streams by ELF symbol size, retain original operands and
actual branch destinations, resolve callees and trace paths before attributing
a displacement-count difference to stale values. Same displacement also does
not identify the same object. Known historical positives and current exact
negatives establish detector operation, not source recoverability.

Use matching full-TU controls to locate the earliest observed RTL-pattern or
instruction-order divergence. Preserve registers, modes and immediates; inspect
all constructor clones independently. Later rnreg can eliminate a peephole2
difference in one clone and retain it in another. Static immediate-store splits
are scratch opportunities, not counts of dynamic searches or cursor events.
The generic [stage diagnostic](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-stage-divergence-diagnostic.md) enforces
explicit clone selection and rejects incomplete/ambiguous paired stage sets.
Identical earlier emitted bytes remain insufficient to establish compiler-state
identity. Do not tune earlier source tokens to manufacture register colors.

A recovered pointer lifetime can preserve two stores and a second memory read
through allocation yet still lose that read in postreload (F11865). In SDM's
control, UID56 MEM:HI becomes an AX selfcopy at that pass; the baseline instead
loses its reread in first CSE and its first store by combine. Track the specific
read at each stage before attributing the final absence to source factoring or
register allocation. Compare value/address/mode knowledge and invalidations;
a widened original load is a discriminator, not proof that a cast prevents
forwarding. The installed compiler's actual replacement branch remains to be
observed. [Bounded controls and static rule](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fax-tail-timing-results.md).

F11866 now dynamically identifies SDM's decisive replacement as
`reload_cse_simplify_set`/cselib: the stored HI value keeps AX as a known location,
old-cursor copy preserves its address value, and register cost2 beats memory4.
A later operand fallback visits the resulting selfcopy; its successful return
alone is not evidence it performed the original memory forwarding. Observe the
pattern and value identities at the first changing visit. Preserve complete
raw/debugged/container object equality and distinguish an extracted compiler's
host runtime from its original container runtime. Read-only GDB observations
need no inferior calls or RTL writes; pin the actual binary for optimized-frame
and instruction-address observations. [Dynamic replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/sdm-postreload-dynamic-trace.md).

An existing wide input lookup before a store does not prove a wide reread after
that store resists forwarding. Mode, address and available register locations
must be compared at the relevant use. Also audit prior identical siblings before
calling a source boundary new (F11867): the older compound/postincrement168B
control has a surviving extra early read and a removed late read. Equal or near
total load counts and size can therefore hide the very boundary being sought.
Keep old source families closed until independent original/use evidence changes
the discriminator; don't add casts or volatile to coerce a surviving load.

### A widening load opcode need not represent wide RTL (F11868)

GCC3's i686-tuned movhi_1 can emit MOVZWL for a plain HI memory-to-register
move to avoid partial-word stalls. SDM's saved UID118 and the installed emitter
prove that mapping; an explicit SI zero-extend UID38 emits the same opcode under
a different pattern. Consult the backend move rule and real -dP annotation
before inferring an explicit promotion or source width from MOVZWL/MOVZBL.
The memory access remains word-sized in both cases. Extra physical destination
bits written by an instruction do not establish which bits the RTL required.

This corrects the proposed SDM late-wide-versus-HI discriminator above: original
MOVZWL does not distinguish those modes. The candidate read is HI from initial
RTL, so there is also no combine narrowing to explain. Keep the valid dynamic
value-forwarding result, close the unsupported width family, and require an
independent original use/alias/profile witness for further source controls.
[Annotated and dynamic emitter proof](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/sdm-movhi-emitter-width-refutation.md).

### Restoring one source boundary does not recover the complete function (F11869)

The V32 cleaned-sample getter's accepted-count-first arm restores the original
branch, but its owner reload is still missing. The V22 permutation counter's
early lifetime plus separated increment leaves a one-byte size difference
with different allocation and schedule. Neither is a byte-exact improvement.
Bound original coefficient capture independently; do not expand a near-size
result into declaration/register permutations.

A reciprocal's stack one is not unique evidence of an unsuffixed double
literal. The float RMS control restores FLD1 at the wrong lifetime and adds
an original-absent final narrowing; it also changes inlined callers. Audit
the complete expression graph and complete TU, including pooled constants.
The ordinary reciprocal-first reversal emits the retained raw object; close
it rather than retrying synonyms. [Eleven-cell audit](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/small-counter-use-results.md).

An unchanged sibling can first diverge at peephole scratch selection after
matching allocation/reload. EpochDetectV29 transfers the constructor-stage
diagnostic: the new scratch and its compare differ at28peephole2, followed by
renaming at30rnreg. Keep the losing full-TU controls and trace the origin;
do not alter the sibling to restore a score (F11870).
[Quality and bystander stage evidence](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fax-quality-use-results.md).

Validate a nomination's complete operation stream before interpreting its
source controls. ResamplerTiming's 'missingFABS' premise was refuted by three
FABS in every control; an earlier comparison computes sign. Preserve invalid
premises and exclude their axes from source-family closure claims. A remaining
DF-versus-XF operand witness is independent, and its bounded negative does
not authorize a general type matrix (F11871).
[Corrected diagnostic-mode nomination](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v90-timing-diagnostic-math-domain.md).

A wrong callee-entry argument can be a source-fidelity gap even if the actual
callee ignores it on that arm. Fax command's uninitialized default `extra`
emits80 instead of the original0 outsideFTM; the existing real callee consumes
it onlyFTM, where both supply80. Distinguish measured argument reconstruction
from public behavior and synthetic replacement-callee probes. Its corrected
argument and original six-case dispatch still miss full identity; do not
claim a gain or invent an operational failure (F11872).
[Complete dispatch/result controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/fax-service-values-results.md).

### Observe scratch history, including the actual allocation-order table (F11873)

The installed Gentoo C and C++ compilers now have raw-preserving dynamic
controls for the persistent scratch-search cursor. Model the actual candidate
order, class/mode restrictions, live/reserved bits, save/restore restriction
and frame protection; validate returned registers and cursor transitions.
All1410 observed searches agree. The53-slot order ends in five EAX entries,
explicitly zero-filled by the target backend; a uniform register rotation
model is insufficient.

An identical final writer can make a different number of searches. The
historical +0x434 retype removes one immediate-SI search, and that difference
reaches the unchanged C2 constructor. Correct float typing remains supported
independently and must stay. The fax bystander supplies a second dynamic
positive. No rejected enclosing matcher or failed scratch search occurred in
these controls: do not claim either mechanism was observed or broaden to
other modes/classes without controls. [Installed compiler proof](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gentoo-peep2-search-proof.md).

A directly observed call-argument value is recoverable separately from a
complete function's register/profile residual. Under fidelity work, the fax
default-zero correction is adopted after full-TU auditing and period388/0;
SIZE8 becomesSIZE5 but no exact function is claimed. Its actual callee ignores
the affected argument outsideTM. Keep these categories explicit and preserve
the negative dispatch/result experiments rather than presenting this as a
successful CFG or whole-function preimage (F11874).
tools/fax_extra_argument_audit.py.

### Separate direct scratch selection from allocation, then recover the counter boundary (F11875–F11876)

A register-only verdict does not identify a compiler pass. Current complete-TU
traces find zero searches in _iir_filter_create and SDMv27_init, one in
FloatFIR::reset and three in dp_v22_init. FloatFIR's selected scratch survives
renaming; the registration wrapper's final colors differ from its selected
scratch colors. Use the actual event stream and successive RTL stages before
attributing either to inherited scratch state. C++ clone declaration names
can repeat: do not equate them with emitted C1/C2 symbol ownership.
[Target boundary](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gentoo-scratch-target-results.md).

A separate zero guard plus while(count--) can leave a redundant test after
member loads even when the source guards the right boundary. Original DEC and
UINT_MAX tests at both boundaries justify consuming count in the existing
pre-load guard, then postdecrementing at the tail. This preserves the zero-input
boundary and recovers FloatFIR's original counter structure while leaving the
body's other work and seven bystanders unchanged. It produces no exact gain;
do not turn that measured component recovery into a register/profile claim or
broaden the domain to guard/variable permutations.
[Counter recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/floatfir-consumed-count-results.md).

### Share branch values before allocation, not only their final machine tail (F11877)

A REGALLOC residual can still have a source preimage. SDMv27_init makes no
scratch search and no rnreg changes. Two source stores compile to one final
machine tail but retain separate branch-result pseudos through local allocation;
both takeEAX before the owner pointer is assigned. The original common
load-result/store supports one conditional assignment. Its shared HI pseudo
spans the branches, changes allocation, and yields the full89-byte exact
initializer with both bystanders and all TU metadata unchanged. This is
source factoring, not a request for specific registers or a cursor edit.
Inspect24lreg/25greg and branch-result definitions when a common original
store has separate source assignments; merely reversing branch arms is a
separate closed domain. Do not assume every register-only function has this
cause or claim the conditional syntax is uniquely recovered.
[Control and audit](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/sdmv27-common-value-results.md).

### Follow cold arms before inferring a common source result (F11878, F11880)

A normal-path field store does not prove both arms feed it. Receiver wrap arms
store immediate 0x4000 separately and jump after the normal store; CID's cold
arms really return to the same instruction. Use `tools/branch_store_paths.py`
with an independently identified branch/field/base to discriminate. The tool
refuses aliases/base changes and nested CFG instead of inventing a source
preimage. Positive and refusal controls are in receiver_counter_boundary_audit.
The 26-TU follow-up domain is closed in shared-value-followup-results.md.

### A local truncation boundary can introduce an extra extension (F11879)

For an unsigned-short field increment followed by a wrap check, a temporary
assignment/cast can add MOVZWL after INC under this compiler. Direct field ++
reproduces the original load/INC/word comparison/store in three receiver
methods. Check both cold and normal stores and prior alias-sensitive reads;
this is a component recovery, not a general permission to remove narrowing.
No whole-function exact gain follows here. Leave unsupported base-changing
paths unadopted; source syntax is not uniquely identified by this observation.

### Choose the active typed overlay at initialization too (F11881)

A reset of a packed decision point is not necessarily the original timing
history reset, even when both leave identical zero bytes. rxtiminginit's blob
clears existing dp.iir2.q/i separately in its timing-group order; choosing the
packed dp.point view hid that access-width evidence. Four full-TU controls
recover23 stores in offset/width/value order, but leave a2-byte size residual
and five changed nonexact bystanders. Review them all; do not claim exactness
from the recovered component or invent a new type/extent from a register base.
[Timing reset control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-timing-reset-results.md).

### Cross entry captures with return ownership before blaming call layout (F11883–F11884)

An ordinary blob call versus a sibling jump can encode source result ownership.
VPcmV34GetCurrentRxBitRate needs BOTH its observed unconditional pointer
captures and a shared result to recover90B EXACT. Each isolated axis misses;
56 full-TU bystanders stay unchanged.01.rtl shows load lifetimes, but all cells
have call_placeholder;02.sibling reveals which alternative actually survives.
No flag, return-type guess or register constraint is needed. Crossed controls
on TX keep RX exact but fail TX, so this is not a universal recipe. Close that
negative domain and retain all full-TU data/exports and caller audits.
[Rate control evidence](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-rate-lifetime-results.md).

A real interior base does not uniquely determine a containing type or size.
VPcm's obj+4 accesses are witnessed, but full-object constructor clearing and
tagV34Object parameter mangling give no independent control extent. Preserve
named fields and existing maps until a typed-callee/extent witness exists;
do not fabricate a nested header solely to reproduce ADD4 (F11882).

### A pre-store load need not be a source capture (F11885–F11887)

V90Modem::setSessionFlag closes only with direct side dispatch crossed with a
switch whose digital arm returns and analog arm breaks. The original ordinary
analog call and digital sibling jump bound the terminal structure; GCC still
schedules the side load before storing the distinct sessionFlag member. An
explicit source capture introduces different register operands. Neither direct
access alone nor the previously tested switch alone closes the function.
Inspect 02.sibling: initial call_placeholder alternatives are not final calls.

Use small_call_result_screen to nominate equal named-transfer multisets with
an ordinary/sibling mismatch; this is a screen, not source or ABI evidence.
550 small nonexact emitted bodies yield five candidates, including three
closed allocator wrappers. Eight crossed full-TU controls preserve all data,
bindings and bystanders. DialerAbort's structured common exit compiles identically
to the early return and remains nonexact even with its unsigned guard; close
that domain. Do not change allocator prototypes or sweep return synonyms.
[Controls and audit](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/small-call-result-results.md).

### Link mismatching rows to UIDs before attributing a register residual (F11888–F11891)

Residual_stage_inventory applies the canonical worst-copy rule; 97 REGALLOC/
BYTES nominations are not 97 diagnoses. Unchanged complete-TU reproduction and
-dP annotation cardinality/opcode checks let residual_register_uid_trace link
25/31 REGALLOC targets to RTL patterns. Repeated clone headers and unavailable
annotation/stage windows stay ambiguous or unavailable, never guessed or zero.
A changed peephole UID need not consume scratch: _iir_filter_create's zero
searches coexist with eight transformed UIDs. Inspect actual events before
attributing a row to the persistent cursor; two scored Scrambler process bodies
have no direct search. [Baseline and UID evidence](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/residual-stage-family-results.md).

A failed scratch search can reset the persistent cursor even when its enclosing
peephole emits no replacement. The new V92 setEchoDelay and v8_process controls
observe three distinct failed sites (five attempts including a repeat), setting
cursor zero; subsequent searches confirm the state. Count rejected attempts,
not only surviving instructions or accepted replacements. Seven controls validate
382 searches/3784 visits; V90MP's QI/q events are explicitly outside the current
HI/SI r model. This observes current GCC behavior, not original cursor values,
and does not authorize tuning source history to manufacture a register color.

### Capture before publish when the ordinary inline loses that lifetime (F11892–F11895)

V92's block-size setter needs pre-store symbolsDone capture and explicit query
dataflow to recover108B. Expanding the helper normally is raw-identical and
argument use alone misses; both early-capture forms match, so original syntax
is not uniquely identified. Preserve multiply-before-divide on the divisible
arm: simplifying rounding changes unsigned overflow behavior. Nine bystanders
and standalone query stay exact. Conversely echo-history reset closes by
REMOVING its unnecessary params capture, leaving the member-valued bound.
Pointer reg/v/f lifetime and scalar pre-publish lifetime are different axes;
inspect01rtl and24lreg/25greg rather than always adding or always removing locals.
The SDM cross misses both initializers and is closed. Do not transfer to already
exact siblings or reopen SpectralShaper's previously fitted owner-anchor domain.
[Full-TU controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/residual-value-owner-results.md).

Mutation fn verifies trailing function ownership AFTER unique substring matching;
it does not scope the find. Use unique context for duplicated arithmetic and
correct unsupported function metadata. Show detached/nonunique locators firing,
repair the same labelled operations, and keep static checks separate from any
claim that a mutation harness was executed.

### Match clone labels and printed RTL before using a repeated heading (F11896–F11897)

A C++ dump heading can name both C1 and C2, with repeated instruction UIDs.
Identify an assembly symbol by its boundary, then match emitted .L numbers to
CODE_LABEL numbers and check its printed -dP patterns against35.mach. Do not
use heading order as ownership. Earlier stages require unique shared label
witnesses independently; a final-stage match does not make every earlier stage
available. Missing/ambiguous witnesses are explicit refusals. This resolves nine
of twelve prior ambiguous nominations and increases register-row links25/31
to27/31. Three nominations were substring collisions, not constructor clones.

A helper query's exact leaf can still alter a remote constructor's scratch
history. Current Queue replay again gains85B and loses734B; no adoption. Its
argument constant loads first appear atpeephole2, then change at renaming.
Use those pass boundaries to investigate the history; do not rewrite the
constructor to repair a color without an independent source witness.
[Replay and limits](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/residual-queue-context-results.md).

### Trace scratch → renaming before attributing a captured local (F11898–F11899)

Jd's two constructor clones allocate the same lookahead byte to AX and retain
identical patterns throughflow2. A different zero-store scratch atpeephole2 then
changes which register renaming chooses for that byte. Thus an original final
register versus our allocation-stage register does not locate the divergence.
Compare independently owned clone streams and actual emitted-name search events.
One scratch difference can spread to nine late patterns without a different local.

An exact helper leaf can remove a dead-stack cleanup search and change a later
constructor's inherited cursor. The Queue85B gain still loses734B: observe the
complete event history, including all predecessors, before source fitting.
No original cursor or new source spelling follows from this mechanism alone.
When comparing dumps, strip explicit string_cst heap-pointer annotations only;
keep pool names, real constants and all operand structure. Three refusal/value
controls verify that boundary. [Replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/residual-constructor-scratch-results.md).

### Shared machine cleanup does not distinguish source exits (F11900–F11901)

Echo append's advancing-input/countdown and preincrement length witnesses recover
individual original shapes, but their combination still misses the complete body.
Do not adopt a one-byte size gap: count instructions and inspect complete paths.
An early return and common if/else exit produce raw-identical complete objects
at both remaining-counter timings. Shared machine cleanup is therefore compatible
with both source forms in this domain. A post-guard register copy likewise does
not uniquely establish a guard-local declaration. Close the bounded cross on its
misses; require a new independent witness before another spelling experiment.
[Nine-cell full-TU audit](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/residual-echo-append-results.md).

### Check memory alias identities before locating a stage difference (F11902–F11905)

V.22's literal25greg differences first looked like a global/reload boundary.
Direct traces refute that: mappings and every observed reload choice agree;
only printed memory alias IDs differ. Require a bijection preserving all alias
relationships, universal class0, addresses, offsets, registers, sizes and
alignments before treating renumbering as equivalent. Never erase alias data.
The checked streams agree throughflow2; two scratch choices first differ at
peephole2 and register renaming spreads them. This is the existing inherited
cursor mechanism, not a new allocator lever. Six controls show harmless identity
renumbering, real register/offset differences and refused alias split/merge.
ACK's finite source domains remain closed without adoption.
[Correction and read-only raw controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/residual-v22-allocator-results.md).

### Bound a captured sample by every observable reload (F11906–F11907)

Tone reversal retains current and outgoing samples for correlation/energy, but
reloads current input after rev_age is written for the history store. Extending
one local through that store removes a possible alias observation, even though
arithmetic still agrees on disjoint buffers and size nearly matches. Check all
uses across intervening writes before proposing capture. InitialRTL read/age
boundaries detect the two unsupported controls; arithmetic-only captures keep
that boundary and still miss, closing the finite source domains without adoption.
[Nine-cell audit](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/residual-tone-reversal-results.md).

### Distinguish rejected experiments and static paths from production bugs (F11908–F11909)

Tone's lost reload occurs only in an unadopted control; retained source and period
object already read input after age publication, like the blob. Audit production
before reporting a deviation. Likewise V8's two static ring stores are alternative
paths, not two writes per sample. A local publication recovers the original loop
layout but misses bytes; symbol publication is a different axis because its two
initial RTL stores already merge in machine RTL. Count both source-stage and
machine-stage publications rather than inferring missing behavior from code size.
[Four complete-TU controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/residual-v8-cursor-results.md).

### Screen indirect callbacks and validate their iteration graph (F11910–F11911)

Direct helper-call counts omit indirect callbacks. Pair literal source-loop
inventory with original/retained graph review: putFrame's four-group loop hides
sixteen fixed-offset original calls. Its static head-call count differs from
initial RTL because alternative paths share a call; neither count is the number
executed on one path. Expansion restores the group operations without recovering
the complete body. Width, capture and typed-owner crosses still miss, and unchanged
neighbors change through TU history. Count all bystanders; do not adopt the nearest
size or claim a uniquely recovered substructure from one base-pointer instruction.
[Uncapped screen and22-cell full-TU audit](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/helper-caller-screen-results.md).

### A narrow loop counter is an independent witness, not a whole-function fix (F11912)

Original snapshot index increments sign-extend from16 bits; the six-entry loops
remain rolled, so expanding them would contradict the graph. Test the local width
without changing the inlining/profile domain, and distinguish raw-inert narrowing
already expressed by assignment casts from changes to target/unchanged neighbors.
The four short-local controls miss complete identity. Reopen from a new operand
or ownership witness, not from another declaration spelling.
[Bounded domain](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-snapshot-width-domain.md).

### Expand macros with the deciding compiler and retain owner uncertainty (F11913)

A decimal-only lexical scan misses hexadecimal bounds and macros. Period
preprocessing with complete per-file flags recovers those without guessing
constant values. Preserve line provenance and use balanced for headers with
checked start/update, then report unsupported bounds and missing/ambiguous
emitted owners. Inline header helpers do not acquire a parent by name alone.
Fixed iteration counts and aggregate backedges remain nominations for graph
review. [537-loop inventory and controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/callback-preprocessed-loop-results.md).

### Recover callback identity separately from repeated call sites (F11914)

getFrame's original captures one callback before all six calls; repeating four
groups and retaining that pointer are distinct axes. Verify both the machine
site count and member read count. Their crossed forms can reproduce these
boundaries without recovering the whole function, so neither size closeness nor
matching callback counts establishes original source or a functional lifecycle.
[Full-TU cross](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/getframe-callback-expansion-domain.md).

### Check reads across a combined loop's stores before adopting fusion (F11915–F11916)

A source break/post-check and an explicit success jump can differ even when
both retain the search loop. Count-consumer synonyms may remain raw-inert across
those graph forms. Likewise, combining FSK multiplication and history transfer
must preserve coefficient read before the store; the coefficient table is
writable. Arithmetic and loop counts alone miss that boundary. Keep rejected
controls separate from corrected ones and production, and do not mistake equal
sizes for raw identity. [Controls and read-age audit](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/callback-preprocessed-loop-results.md).

### Separate guard joins, rounded consumers and spill homes (F11917–F11919)

Extra aggregate backedges can come from replicated loop increments rather than
missing expansion. ADID phase grouping retains every original loop; positive
guards recover its shared joins and original length. Cross this graph evidence
with initialization age and expression/consumer mode independently. A short
destination does not force a local rounded carrier to receive the same promotion:
direct conversion at the store recovers the original reload where signed and
unsigned carriers retain an extra extension. Check the emitted conversion age
before claiming behavior at guarded floating-exception boundaries.

The closest complete control leaves eight exchanged spill displacements. Same-
boundary first definitions can fix scheduling without fixing those homes; original
machine stack offsets do not uniquely imply source declaration order. Close the
finite source domain and seek observational allocator evidence before reopening.
Compare raw bodies: equal original-relative byte counts hid a one-opcode change
between signed/unsigned controls. Audit every bystander and data owner even when
the target is the only body expected to change.
[22-cell control and positive residual detector](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/adid-phase-grouping-results.md).

### Observe reload-created slots before interpreting local declaration order (F11920–F11921)

ADID's eight displaced operands were best/count spill homes. Read-only installed
cc1plus tracing identifies `alter_reg` during reload assigning those slots in
ascending pseudo order. Assignment timing alone did not change the pseudo order.
This new evidence justified a bounded phase-state declaration control: count,
best and bestGroup declared at the start of grouping after clearing the array.
Crossing it with the recovered guard/rounded-consumer form closes all601 bytes.
The complete TU gains one exact body with no loss; original source remains
nonunique. A closed assignment-order experiment did not exhaust declaration
scope, but reopening required a concrete allocation witness, not a larger
permutation search.

Keep independent partial reconstruction improvements eligible for validation:
supported call boundaries, operand order and joins need not close a whole body
to be useful. Separate their acceptance from nearest-size source fitting and
cross them with later independently evidenced controls. Here guard recovery
was supported and period-green; the allocation cross achieved full identity
before retention. Always audit other-function context: moving an intact exact
helper changed five raw bodies only through relocated addends. Compare canonical
targets and masked bytes before claiming inherited allocator/scratch effects.
[Source/context cross and observational replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/adid-spill-context-results.md).

### Cross floating expression modes at their actual consumers (F11923–F11925)

ADID first-study's explicit mean conversion used0.5f while its already exact
helper used0.5. The double expression recovers the original inner variance/mean
operations even though the complete function grows16 bytes. Helper reuse emits
the same object, so it does not uniquely recover factoring. Retain the supported
operation recovery after period validation; byte-exact function count is not
the only progress measure.

Consumer matters: changing threshold2.5f to2.5 at a float destination is raw-inert,
but the diagnostic multiplication feeding a short conversion changes initial
RTL from SF to DF and restores the opening evaluation sequence. An explicit
upper-clamp guard independently restores the original branch/pop selection;
crossing them recovers66 opening rows. EqualSIZE16 scores initially hid these
differences: full hash/body checks rejected the false erasure interpretation.
Separate n's initialization placement from merged byte publication: moving n
inside the admitted scan restores placement; duplicating source arms still
emits a shared store. Bound and close that domain without register forcing.
[22-cell controls, positive block detectors and retained partial recovery](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/adid-first-study-results.md).

### Separate scratch-created tail sharing from source joins (F11926–F11927)

Follow immediate stores through reload, peephole2 and ce3. In ADID first-study,
separate zero/one stores survive reload, then peephole2 chooses ax for both;
ce3's CFG cleanup crossjumps the identical register stores. The blob's distinct
byte registers keep the tails apart. This is evidence about scratch selection
and late cleanup, not proof that source arms need more duplication.

Test whether preceding TU context can actually influence the target before
moving helpers. Read-only installed tracing plus a validated scalar search
model shows all53 incoming scratch cursors converge after this target's first
forced bp selection. Incoming cursor alone cannot change its later byte picks
with fixed eligibility/order/sites. This bounded negative result does not rule
out independently evidenced lifetime or traversal changes. Also localize store
order separately: ADID count/sum remains count-first through block reorder and
reverses at sched2. Its source already has the original order; repeating source
permutations without a new scheduling witness would not test the explanation.
[Raw-equal eleven-stage replay and six-search observation](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/adid-first-study-stage-results.md).

### Inspect target packet reordering after scheduler priority sorting (F11928–F11929)

The highest-priority ready operation need not issue first. First-study's integer
xor has priority6 against the float store's1, and ready_sort ranks xor first.
The i686/PentiumPro target then moves the float store ahead to fill its complex
decoder and packs the xor alongside it. Verbose ready-list/packet/issue tables
show this directly. Follow target sched.reorder and actual issue order before
explaining a mismatch with priority or source order alone.

Read age must recover the surrounding owner/operation graph too: capturing the
high-loop reference before clearing produces count-first stores but adds an
extra index load and discards the original maintained index. Three capture/
counter-ownership forms emit the same unsupported complete object; decline them
and close that domain. A partial recovery is not merely one desired store order
surrounded by invented operations.
[Packet trace and bounded capture domain](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/adid-first-study-scheduling-results.md).

### Reconsider retention separately from a closed exactness experiment (F11930)

Unsigned constellation clearing counters already measured two original JBE
predicates replacing JLE, with just the two inline copies changing in their
caller. The source family correctly failed to close either130-byte function.
That does not refute its supported counter-type recovery. Under partial-recovery
scope, replay against current production, audit isolated controls and all
bystanders, then period-validate and retain the operation improvement. Do not
reopen nearby index/declaration/spelling permutations or claim a whole-body
gain. Count partial functions and inline operations separately from exactness.
[Current four-cell revalidation and integration](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/constellation-counter-retention-results.md).

### Retain evidenced path joins while keeping their independent read residuals open (F11931–F11933)

V32 accepted-count fallthrough restores the original unsigned JA/accepted-store
arm while its owner reload remains missing. V8 per-sample ring local restores
the original one-publication wrap join while its filter-load order and stack
homes remain different. Retain these supported path recoveries after complete
TU audit and period validation; do not force the independent missing reads or
claim a full exact gain. Local cursors must respect original capture/publication
boundaries and cannot silently cross a callback or loop iteration.

A read-age source change can survive to block reorder and still fail in the
final object. V8's pole snapshot is before input through31.bbro; sched2 reverses
the pair. That cross changes the object even at an equal BYTES233 score. Close
the tested source read-boundary domain and seek a scheduling/lifetime witness,
rather than retrying names or asserting that the compiler erased the source.
[Current path retention and nine-stage read trace](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/partial-path-retention-results.md).


### Account for proved table identity before screening spill homes (F11934–F11936)

An otherwise exact instruction graph can be rejected by anonymous section
relocations. getPower's six ordered dispatch targets agree, but the old screen
compared table offsets3300/24 and missed ten exchanged double stack operands.
Use a shared conservative ELF proof; never blank arbitrary relocation targets.
The induction fallback tracks entry index/flags/CFG and validates every table
entry, ABI call, stack width/save slot and incoming relocation. Known negative
ELF controls must refuse partial stack writes, word pushes, alternate interior
entries and overlapping CALL fields. CI cache keys include the proof module.

After table proof, unchanged-production exact count stays fixed. A separate
source change puts fraction before power; observed ascending-pseudo reload
allocation exchanges the original-opposite homes and makes getPower574B exact.
Lifting scope with power still first is raw-inert. Declaration scope/order is
only a bounded lever when named allocation requests and original homes supply
an independent witness. [Trace/replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/getpower-stack-results.md).

### A local SIZE window nominates homes, not their meaning (F11934)

Extend stack screening beyond equal-sized bodies, using matching local graphs
with all real values/registers/relocations preserved. getConstellationsIndex and
Psd::process expose consistent named local-home exchanges despite independent
CFG differences. V32 ring response instead exchanges outgoing argument slots
and corresponding defining registers: it is a false local-spill nomination.
Inspect reaching definitions/consumers before allocation experiments. Window
identity cannot establish whole-function or CFG recovery.

### Successive extraction and refreshed buffers are independent of group expansion (F11937)

V34 getFrame's group fields use SHR1, SHRsmall, SHRw destructively, and retain
unshifted input between groups while refreshing it after callbacks. Source
absolute shifts and per-group member reads can be equivalent over normal widths
yet miss both original boundaries. Cross these witnessed graphs separately from
fixed groups/callback capture/word widths. Static read counts do not locate
refreshes. Retain supported operations after full-TU/period validation even
when another incomplete graph has a smaller byte distance. Do not hold a
snapshot across a callback without the original refresh boundary.
[Domain/results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-frame-retention-results.md).

### Diagnose alias-driven reload loss without forcing a source read (F11938)

V32 getter already repeats the owner expression at return. With type-based
alias analysis, the post-count-store read disappears between07.addressof and
first available09.loop; withdrawing strict alias analysis restores the exact
53B getter while also changing constructors. Count actual load SETs rather
than duplicated REG_EQUIV text; preserve unavailable duplicate snapshots.
The option demonstrates an interaction, not original field typing or profile.
Seek an independently supported access/API type or crossed full-TU profile
explanation before source adoption. [Diagnostic](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-cleaned-alias-results.md).

### Follow the surviving pseudo when recovering spill order (F11940)

A named local can disappear while its defining temporary owns the spill.
PSD's frames local copies the unsigned division-result pseudo; declaring only
nearby loop locals after that result leaves its home unchanged. Declaring
frame/pos before division moves their pseudo creation before the surviving
quotient and recovers all three original homes. Mapping's i/count exchange
also propagates into its inlined caller. Six bounded cells/72 body verdicts
and read-only raw-equal compiler observations establish both exchanges.
Scope/adjacency controls are inert. Validate initial SET ownership, reload
requests and final frame adjustment; require all altered operands to be the
identified home permutation. Local recovery need not close a SIZE body.
Do not continue declaration permutations after the independently witnessed
homes agree. [Domain/results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/size-spill-results.md).

### Ordered enum dispatch can identify switch lowering (F11941)

An if-chain's cmp1/cmp2/test0 differed from PSD's original cmp1/je/jle/cmp2/je.
A bounded three-case switch preserves the case bodies and default no-op while
recovering that ordered dispatch. Review destination blocks as well as compare
mnemonics. Full-TU audit may support the structural recovery even when overall
SIZE distance grows; do not mistake independent x87/loop/register differences
for a failure of the recovered dispatch. Close case-order spelling searches
once the original category agrees. [Control](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/psd-dispatch-domain.md).

### Normalize inside the loop when the blob guards evaluation (F11944–F11946)

PSD's precomputed scale evaluated even with no bins; original guards evaluation
in each arm. In-loop division hoists into a guarded preheader. Cross this lifetime
with peak's scalar mode and arithmetic operands instead of freezing explicit
long-double casts: natural float division still stays extended in period x87
until the original float narrowing. Require emitted rounding/operand graph and
period validation, not a modern precision intuition. Window*input also restores
which value is loaded versus used as a memory operand. Initial overlap/length
read order is independent. All60 floating rows/27 branches can agree while six
integer conversion register rows remain. [Replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/search-psd-results.md).

### Search-entry and publication order are independent of compare spelling (F11943)

Place a value initialization after an evidenced guard when original loads occur
there. Mapping's guarded length and count postincrement recover distinct
boundaries. Reversing source equality operands is inert, but length>n instead
of n<length preserves cmp0/JBE and CMP/JA in GCC3. Cross only independently
witnessed boundaries; audit inlined callers. Matching branch orientation does
not justify local/register permutations to close remaining allocation.
[Control/results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/search-psd-results.md).

### Trace conversion temporaries before changing precision (F11948–F11949)

A spare stack word can be an expansion temporary, even when final assembly has
no narrowing instruction at that word. PSD's explicit long-double getter creates
SF4 through gen_truncxfsf2. Ordinary scalar arithmetic removes it while period
x87 retains the original extended operation graph. Cross the independently
observed comparison orientation: scalar mode alone changes many rows; bins>i
alone leaves SIZE1; both yield EXACT77B. Review the full TU and narrowing sites,
then run the period differential; this is not a blanket precision rewrite.

Separately distinguish persistent peephole scratch selection from function-local
register renaming. PSD's remaining conversion high words exist before peephole2;
read-only installed-compiler observation attributes final substitutions to local
use-age ticks. Earlier-function cursor interventions do not transfer directly.
[Results and replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/psd-conversion-results.md).

### Expansion-word removal is transferable, arithmetic narrowing is not (F11951–F11953)

Trace assign_386_stack_local requests and their returned homes, including
zero-size reload/alignment requests. SineWave's scalar producers remove the
same gen_truncxfsf2 SF4 word as PSD and restore the original8B frame; delayed
phase acquisition recovers the original operand multiset while cold-exit layout
remains nonexact. FloatFIR's scalar method already has the original4B reservation,
so a similar request alone does not warrant changing its return arithmetic.

AGC falsifies a broad scalar rewrite: its gen_truncdfsf2 request survives, and
an apparently closer control narrows fsqrt through memory where original does
not. Audit the operation graph, narrowing sites and coupled inlined callers,
not just frame size or SIZE score. Record assembler names in read-only method
traces to distinguish overloads and unrelated process methods in one TU.
[Bounded domains](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/expansion-temporary-transfer-domain.md).

### Separate branch prediction, trace placement and x87 edge repair (F11955–F11956)

A floating function can match every nonbranch operand yet differ in cold-exit
placement. Trace prediction12.bp, ordering31.bbro and stack34.stack separately:
SineWave's entry50/50 and exit10% place its zero-trip block before wrap; edge
pops appear afterward. Compare complete CFG paths, not just operand multisets.
A natural first-iteration owner can change prediction but retain hot tests and
cause block copying. Record copies created during trace connection explicitly;
require known copy-source and final-order coverage rather than silently ignoring
new blocks. Positive4→8 and malformed99→8 controls exercise the reader.

Inspect allocated constants too: this control adds QNaN and an entry fld only
at stack conversion. Live-at-entry x87 initialization can explain a constant
absent from source; it does not by itself establish reachable undefined behavior.
Reject the control when its original graph is lost. The92-copy small-floating
screen nominates only SineWave for this layout-only family; it is not evidence
that all other source recovery is exhausted. [Replay/results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/sine-cold-layout-results.md).

### Missing diagnostics can conceal the original return contract (F11958–F11960)

Compare original diagnostic relocations and value arguments before declaring a
small SIZE gap allocator-only. V23 glue had comments where original debug>1
branches/calls belonged. Restoring raw-status printf exposes an unjustified HI
return: original consumes EAX, and initial RTL shows our extra sign extension
before scheduling. Recover shared declarations and definitions together, audit
callees/callers and include every affected TU; do not use local cast workarounds.

Compare debug transcripts on valid lifecycle boundaries at levels1/2/3, with
callback markers and overflow checks. Matching outputs at debug0 misses original
state/tone/timeout messages and their effects on codegen. Nine recovered sites
bring five functions closer while all remain nonexact. Follow original newline,
argument and publication order. A SIZE2 body can still have a different frame,
dispatch and sibling-call graph. Inspect complete bodies before choosing next
controls. [Replay/results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/missing-debug-boundary-results.md).

Audit the fixture's temporal claims too: V23's acquisition deadline is one minute,
and the tested host retains carrier through24 seconds of silence. Preserve those
measured histories and exact comparisons; never repair source to meet a guessed
modern timeout. Distinguish object-recovered sites from those observed firing.

### Trace returned values and word operands independently (F11962–F11963)

A callee can publish the right field while returning the wrong register value;
a field-reading substitute can hide that reconstruction defect. Compare the
original epilogue with callers' EAX/AX captures, recover the shared return
boundary, and test explicit results as well as samples/state. Include ignored
callers in the complete-TU audit: changing AGC's return declaration exchanges
argument-store order in unchanged B103 source. A closer body is not necessarily
an exact gain.

For countdown loops, distinguish narrowing only the initial int index from
re-narrowing every decrement. Original MOVSWL AX plus TEST SI can independently
support a short counter; signed word compares can support short measured/cached
gate operands. Cross these witnesses separately from pointer/countdown factoring
and gain-local widths. AGC recovers the first two categories, but the remaining
gain graph is not established by its quiet loop or a SIZE3 gap. Retain informative
losing controls, and stop the finite domain absent another witness.
[Replay and limits](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v23-agc-return-results.md).

### Cross arm ownership with sample-loop operands (F11965)

A common advance in original disassembly can justify explicit if/else factoring
when reconstruction uses an early continue. Cross it with independently
witnessed pointer/countdown loops: AGC's indexed if/else makes initial CFG
smaller but later STC duplicates three blocks; the combined source keeps one
copy and restores quiet-before-return-before-gain plus the shared exit. Initial
RTL's two-versus-one advance assignments separate source ownership from later
allocation/layout. Inspect saved CFG and bbro transformations and use known
negative controls; do not call equal block counts equal graphs.

Adopt original graph/operand recovery on evidence and period validation, even
when SIZE gets worse. A restored arm boundary can expose pointer spills or
remaining arithmetic/shift differences; the smallest-size control may retain
wrong source structure. This is a partial lever, not a promise of exactness.
[AGC controls and limits](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/agc-arm-ownership-results.md).

### A byte instruction can be a narrow use/output choice (F11967–F11968)

Do not infer an unsigned-char declaration or cast from MOVZBL alone. AGC's
raw cached int shift expands to a QI subreg use; reload loads a memQI home into
CL, and GCC3's movqi_1 output selects MOVZBL under i686 TARGET_MOVX. The QI
load persists through saved late stages; it is not an explicit zero-extension
node. An explicit byte cast instead introduces a temporary and changes its
lifetime/hoisting. Cross raw count, byte cast and masking while preserving
value arithmetic, and inspect output-selection rules as well as RTL.

Count-width evidence and C semantics are separate: initialized nonnegative
unity-pair AGC histories give counts0..14, but initial config relocations cannot
bound later pointer selections or frozen/planted states. Document original
period hardware behavior outside C's count range, demonstrate finite count
identities with a wrong-mask control, and never present invalid shifts as
portable C or fixture reachability. [Replay/limits](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/agc-shift-results.md).

### Separate predicate width, eager booleans and argument normalization (F11970–F11972)

A narrower cached carrier and a narrower use need different controls. AGC's
short gain cache restores two word tests; use-only level narrowing under &&
branchifies the original two-SETcc/combined-TEST guard. Cross eager boolean &
with that narrow predicate when the original explicitly combines both values.
Here the operands have no evaluation side effects. Retain the wide arithmetic
carrier and reject a smaller wrong-CFG result. A complementary structured
comparison with swapped arms can restore witnessed hot/cold partition placement;
do not add branch hints or reason from frame size alone.

When full-width local data adds an argument extension absent from original,
check the helper interface as well as the caller. RMS can receive a full unsigned
length while its local countdown remains16-bit, preserving its entire exact
body. Audit all callers: SRE needs explicit word normalization at that boundary
to retain its original zero-extending argument load. Do not change the signed
field globally or mistake identical callee bytes for proof of a unique formal
type. Known length-only, width-only and normalized controls distinguish these
interactions. [Complete controls/replay](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/agc-predicate-results.md).

Object-data audits must represent COMMON storage by size, binding and alignment;
it has no section contents. The class1 table is a76-byte/32-aligned positive
control across six cells. Passing it as a section index is an inspector failure,
not a reconstruction result or a reason to skip the TU.

### Trace paired argument lifetimes across opaque calls (F11974–F11975)

An extra extension near a call need not mean a wrong prototype. FSE narrows
both scaled LMS errors before the original first update, keeping the second
across that call. Reconstruction had formed the second afterward. Capturing
both first restores this boundary already in initial RTL; two natural local
forms emit the same complete object. Preserve helper types and validate all
bystanders. The SIZE gap improves without making the function exact.

Fixed pre-call windows nominate, never certify: MTD's original conversion falls
just outside eight rows; TONE and settxlevel convert unrelated values. Report
matched calls, excluded bodies and per-object duplicate weak bodies separately
from the strict function census. A known length-only RMS positive and identity
negative validate this detector; an incorrect positive fixture must abort, not
be excused. Inspect full def/use chains before testing interface changes.
[Controls, partial-link limits and next discriminators](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/lms-paired-error-results.md).

### Cross producer return width with original scalar write-back order (F11977–F11979)

A C++ member name may omit its return type. Inspect producers and consumers
together: Descrambler<T,I> returned T although the original HI returns and
consumes fullEAX. Cross declaration width with final narrowing; fixing the
caller alone need not fix the producer. Then cross an independently observed
scalar tap write-back order. Return alone leaves HI ten bytes different; order
alone leaves its SIZE mismatch; both recover the complete118B body. II110B
needs order only. Confirm the changed order in initial RTL before attributing
it to allocation.

Transfer needs independent controls. Scrambler HI's original tap2-first writes
and HH's tap1-first native writes can still share the same tap2-first source:
HI becomes EXACT while the entire exact-HH TU stays raw-identical. Native
write order alone is not a universal source-order oracle. Preserve bulk loops,
XOR spelling and declarations, and compare every header consumer.

An imported function's vendored declaration/definition is stronger than a
local prototype guess. B103/V23 modem bit counts are int; V23 needs both the
correct declaration and removal of invented word casts to recover original
full arguments. B103 still needs its original normalization at use. A shared
contract does not imply every caller has the same conversion boundary.
[Complete domains, negative controls and validation exclusions](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/scalar-boundary-results.md).

### Separate capture age from narrowing placement (F11982–F11983)

An extension near a debug call can belong to the cached source value. INFO1a
narrows the reversed-bit result before its debug branch; narrowing only the
printf argument delays that boundary. Cross narrow-at-assignment with explicit
destination capture when the original loads both pointers before the first
byte store. Inspect native def/use and initial RTL; call placeholders may hide
a baseline result assignment. Both supported cache spellings can emit the same
object, so retain the clearer declaration without claiming uniqueness.

The location of narrowing can also change behaviour. The fax constructor
compares a promoted max_frame+3 requirement before narrowing the selected ring
value. A word requirement instead compares the wrapped value. Cross requirement
and selected-carrier widths, and use fixed safe constructor cases at wrap
boundaries. State the component boundary and allocation extents; a constructor
result does not prove an end-to-end reachable configuration. Keep a failing
pre-fix control, and distinguish static arithmetic enumeration from executed
blob comparisons. [Controls and results](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v34-vmi-boundary-results.md).

### Check result provenance and evaluation boundaries separately (F11985–F11986)

A pre-call extension screen can miss the useful return boundary. Compare fresh
EAX extensions after matched calls, stopping at intervening writes and control
transfers. Demonstrate the detector on a historical wrong-return caller, and
include identity and overwritten-EAX negatives. Print zero nominations and all
excluded/matched denominators explicitly. A bounded straight-line null does
not certify inlined calls, masks, branches or unmatched call sequences.

Native full-width character arguments do not uniquely imply an int-return
helper. Spectral display's direct-argument controls remove byte extensions
with the char helper unchanged; its late sign comparison is a separate value
lifetime. Cross those explanations before editing a declaration. Likewise,
removing an XF load by switching fractional intermediates to DF may produce a
memory multiply instead of the original double load. Audit operand categories,
constant-pool payloads and full bodies; a SIZE1 result cannot settle that
storage lifetime. Preserve the negative domain and require a new witness to
reopen it. [Complete queue ledger](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/call-boundary-lead-ledger.md),
[spectral controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/spectral-sign-carrier-results.md).

### Review partial source recovery separately from exact closure (F11987–F11988)

A finite experiment's failure to close an entire body does not refute every
original operand boundary it restored. Keep that negative measurement, then
review a supported partial change on its own evidence when partial fidelity
is the authorised objective. Reproduce the current full-TU baseline, especially
if diagnostics or helper return ownership changed since the old control.
Do not reopen neighbouring spellings or profile options merely for a closer size.

V23's original MRF outgoing word is unsigned even though the callee narrows
signed at use. V21 reloads its owner after each actual known-state diagnostic;
its inlined callers must be audited as changed bodies. HDLC unframe keeps entry
scalar snapshots but reads the current parent across CRC/debug calls; recover
that lifetime without claiming arbitrary alias histories are reachable.
All three pass the permitted period gate without becoming exact. Report size
and positioned-byte effects separately from recovered operands and exact counts.
[Three controls, collateral and validation](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/partial-boundary-recoveries-results.md).

### Sequence a call result before reading a mutable destination owner (F11989)

In `root->child->field = helper(...)`, the assignment store follows the call,
but evaluating the destination address can precede it. Compare the owner's
load age, not just the return register or final store. DemodDataV21's original
reloads the child after AGC; a local int result followed by the member assignment
recovers that boundary. Cross it with the independent unsigned MRF slot: each
incomplete control misses, all observed boundaries together recover EXACT217.
This is statement sequencing, not a request for a particular register.

Do not propagate the local blindly. Other nested assignments already capture
their owners before calls in the original; indexed embedded arrays do not read
a second owner pointer. The seven nested and18 indexed nominations were reviewed
without another justified rewrite. A fresh-return detector must stop when EAX
is overwritten, or it can mistake a field reload for a returned value.
[Native controls and bounded screen](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/boundary-transfer-results.md).

### Review source publications and API slots without hiding collateral (F11990–F11992)

An unread argument slot does not prove a missing formal; an epilogue that
publishes and returns the same int does not establish a void-return ABI bug.
Review caller and producer together, and retire historical inference once a
consistent source contract is supported. Keep signed use-site narrowing when
recovering unsigned outgoing slots. Require raw equality of a callee TU whose
API spelling changes without changing its actual word computation.

Accumulator stores and separate ADD/SUB update boundaries can be supported
partial recoveries even when the whole function misses. Validate values under
the period compiler and audit all unmatched helper bodies too. Preserve
original high-count defects; do not execute an unsafe wrap/nontermination case
merely to add coverage. Report larger size gaps and exact-loss controls openly;
V32's collateral was initially withheld. The later period-stage audit below
identifies its scratch-register cause and retains the native unsigned count
without fitting local permutations.
[Complete partial-recovery table](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/boundary-transfer-results.md).

### Fail an unknown test selection before claiming a denominator (F11993)

A runner that skips an unknown requested name can report a green subset. Check
requested names against actual fixture sources before Docker or partial runs.
Test unknown-only and mixed valid/unknown lists with Docker replaced by a
failure sentinel. Keep the valid full selection as the positive control and
compare requested and reported counts. A warning is not a failing gate.

### Follow the promoted comparison operands through the selected value (F11995–F11998)

A short-pointer API does not mean every read is signed. Transmitter countdown
handlers compare a signed positive remaining value against an unsigned budget
word. Preserve those promotions at the comparison use; changing the selected
carrier or public prototype is a different hypothesis. Test0x7fff/0x8000/0xffff
at an explicitly constructed component boundary, with output buffers large enough
for the incorrect control too. Separate public-wrapper reachability from a valid
component invocation. A faithful correction can increase a size gap.

Revisit a declined exactness control for partial adoption when the original
operations are independently established and partial fidelity is authorised.
V29 request bytes retain their age across child stores; V34 FSK combines products
and history updates, reading coefficients first. Keep current full-TU raw controls,
all bystanders and period validation. Do not promote a near-size follow-up into
source truth: V27 Quiet's one-byte gap still contains different read/extension
boundaries, and the four selected-count/member controls fail complete identity.
[Domain and evidence](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/transmitter-budget-fidelity-results.md).

### Validate documented semantic leads at actual constructor boundaries (F11999–F12001)

A closed exactness experiment can leave a real reconstruction mismatch. Review
its original operands, then demonstrate fixed failures before partial adoption.
Wrapper failure checks must run even when no supported converter was selected;
a cadence subindex must select the actual original bank. Compare constructed
objects and selected payloads, not only a source table or its byte count.

For state-machine predicates, construct the component and feed its real input.
Equal first/last tones with a longer middle distinguish the missing interior
comparison without planting period history. Retain an all-equal positive control
and record the first differing sample. Keep existing synthetic probes labelled.

A diagnostic value can differ intentionally from returned metadata. Cadence's
working default type is DIAL while its returned name is INVALID. Trace both
loads and their publication ages across calls; do not reuse one merely because
the strings usually agree. Remove an old transcript exclusion only after the
original operation is understood and a failing control is preserved.
[Finite controls and limits](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/service-constructor-fidelity-results.md).

### Inspect unchanged bystanders before accepting a read-boundary transfer (F12002)

A preceding function change can alter another function's peephole output. Preserve the failed audit, compare complete source
and instruction streams, then identify the actual changed values. In the HDLC
pair, two independently zeroed registers initialise exchanged private spill
slots before the first jump; all subsequent instructions agree. Check the rows
and the join state explicitly, while continuing to report five raw differing
bytes and a nonexact body. Trace stages with a checked alias-set bijection:
agreement through flow2 and a28.peephole2 difference locate the retained transformation,
without recovering original compiler state or authorising source fitting.
[Reproduction and limits](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/service-constructor-fidelity-results.md).

### Separate explicit GCC tree annotations from RTL operands (F12003)

An unchanged symbol can carry a different `<string_cst 0x...>` annotation on
another compiler invocation. Compare its symbol, flags and operands, while
normalizing only the explicitly tagged tree address. Test the actual parsed
form, including a closing bracket attached to the address token. Retain negative
controls for symbol names, flags, real constants, register colours and stack
offsets. Preserve and retract an invalid early-stage report before attributing
a source-unchanged bystander to allocation. [Controls](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/gcc3-stage-divergence-diagnostic.md).

### Separate field storage from narrowing at a use

A narrow argument load does not establish a narrow field. Tone IIR's interval
was represented as a short plus two padding bytes because its constructor
passes it to `GetFP_Value(short, short)`. The independent cadence producer
stores 32 bits and the progress consumer compares 32 bits. Modelling one int
preserves the configuration's offsets and bytes while correcting an early
verdict: a constructor-created interval of 66,036 must not become 500.
Retain the short conversion at the helper boundary. Use a full-width
component case with a valid nonzero low-word divisor, and show the old object
failing before relying on a new fixture's green result.
[Recovery and negative control](https://github.com/philpem/slmodem_dsp_re/blob/5bd9d8572359c47f111cbeb0889bbd051444c237/docs/recovery-toneiir-interval.md).

For each suspected member, record the original writer's store width, the
reader's width and extension, and the base object's identity. Trace a
cross-class pointer before attributing its displacement to a field. Offset
and size assertions establish a representation's layout, not its correctness
or signedness. A zero-extension feeding a truncated operation need not imply
unsigned storage; a sign-extended argument need not imply a signed member.

Matching independent accesses should close a nomination. V.92 mapper fields
and the V.90 phase-3 `+0x400` member have matching narrow writers and readers.
The phase-3 `+0x414` store establishes a narrow write, but its argument alone
does not settle the stored member's signedness. Preserve that uncertainty
instead of searching adjacent types or absorbing padding by preference.
[Bounded five-field audit](https://github.com/philpem/slmodem_dsp_re/blob/5bd9d8572359c47f111cbeb0889bbd051444c237/docs/recovery-fields-v90.md).

Full-width unsigned arithmetic can justify a clearer member representation
without changing machine code. V.22's RMS accumulator has a dword writer,
unsigned division and unsigned threshold comparison, with no signed consumer
or escaping field address in the audited source. An unsigned member removes
compensating casts while all fourteen affected objects remain byte-identical.
Signed storage plus those casts remains a possible original declaration;
this is a supported representation, not a new exact-function gain. Audit every
header consumer and preserve the complete objects as controls.
[Writer/use census and rejected V.34 leads](https://github.com/philpem/slmodem_dsp_re/blob/5bd9d8572359c47f111cbeb0889bbd051444c237/docs/recovery-data-fields.md).

A full-width declaration can also hide two independent narrow regions.
V.32's SNR-drop counter was represented as an int accessed through low-word
casts. The original compares and updates only the signed word at `+0x20`;
whole-parameter copies also preserve `+0x22`. Splitting out the counter must
therefore preserve that adjacent word, without claiming it is padding or
assigning it a meaning. Check template bytes, field offsets and every header
consumer. The signed comparison establishes the counter's interpretation;
a preceding zero-extension alone would not.

For owned buffers, allocation size establishes capacity, not element type.
V.90's equaliser buffers at `+0xb4` and `+0xb8` acquire their `short *` types
from independent signed-word writers and readers. Retyping them can remove
casts while preserving the entire object. Keep allocation/free behaviour
unchanged unless the original supplies separate evidence for changing it.

### Adopt a validated partial without reopening its search domain

When partial reconstruction is authorised, an earlier non-exact control can
supply a retained refactor. Reproduce the current complete TU first, then
verify the independently witnessed operands, all collateral and the deciding
period result. Keep the old search domain closed: changed acceptance criteria
do not justify additional declaration, pointer or compiler-option palettes.

CID's correlation and low-pass wraps derive from the first loop's terminal
cursor. A valid one-past/predecrement representation restores that dependency
without forming a before-array C pointer. Tone IIR restores four physical
stages, sixteen-bit tap induction and a captured shift-array owner. All three
bodies remain non-exact. Their remaining length gaps do not count differing
bytes, and the expanded instructions do not prove that the author explicitly
expanded the source rather than relying on compiler unrolling. Record the
graph recovered, current validation and remaining uncertainty together.
[Current controls and partial adoption](https://github.com/philpem/slmodem_dsp_re/blob/5bd9d8572359c47f111cbeb0889bbd051444c237/docs/recovery-call-partials.md).

### Distinguish completed identity proofs from newly recovered code

An UNRESOLVED function can already have matching instruction bytes while its
anonymous target identity remains unproved. RxNextStateV17 required a decoded
register-only SETcc rule; ConstellationPower required one flag/selector-
preserving argument load in the guard and a balanced push-only leaf frame.
Prove the newly admitted encoding and state effects, retain guard-entry and
ordered-target checks, and test unsafe destinations and changed tables through
the shared comparator. Memory forms or wider control-flow classes remain
unsupported until separately proved. Count these gains as identity proofs for
existing code, with unchanged object hashes, separately from source gains.
[SETcc](https://github.com/philpem/slmodem_dsp_re/blob/5bd9d8572359c47f111cbeb0889bbd051444c237/docs/recovery-setcc-proof.md),
[guard and frame](https://github.com/philpem/slmodem_dsp_re/blob/5bd9d8572359c47f111cbeb0889bbd051444c237/docs/recovery-corridor-proof.md).



### Keep refused call pairs as evidence

A return-use screen must retain the comparisons it refuses, not only count
refusal reasons. Record the defining object, symbol, call ordinal, both call
sites and both first-use traces. Retain unequal target sequences separately,
without inventing a correspondence. The return-mode screen writes these as
`call_pairs` and `unequal_sequences`, with their counts checked against the
summary denominators.

This ledger makes a next experiment specific: an unproved branch edge needs
an ownership proof; an aliasing store needs a storage argument. Aggregate
counts cannot identify either site. Equal target sequences still align only
physical call ordinals, not control-flow paths or arguments. Repeated calls
need independent correspondence before a difference can justify a source
change. Saving a refusal does not resolve it or increase analysis coverage.


### Prove fixed-frame calls and taken dispatch guards separately

A fixed stack frame may contain both callee-save slots and outgoing argument
words. Prove their disjoint extents, reject stack-pointer escape, validate
each direct call relocation, and require restoration along every reachable
return path. State the ABI assumption: callees return normally with balanced
stack depth and preserved callee-save registers. This extends identity
coverage for `V32LocLoopNextState`; it does not reconstruct new source.

A taken unsigned `JBE` can guard a table dispatch beyond the default return
arm. Require a unique taken edge into the dispatch and no fallthrough from
the preceding block. Independently reject named, direct, relocated and table
entries into the comparison corridor or dispatch. `V90Resampler::setBllState`
needs one flag-preserving register-to-object store between comparison and
branch; decode that exact operand form rather than admitting arbitrary MOVs.

Exercise both proofs with rebased tables that must match, altered table
targets that must differ, and unsafe identical pairs that must remain
UNRESOLVED. The fixed-ABI and taken-guard control tools retain these cases.
A proof extension changes resolved relocation identity, not the source
compiler's emitted bytes or literal whole-object completion.


### Recover an expression's evaluation and narrowing together

Named floating temporaries can change both evaluation order and x87 spill
placement. In `V90ConstellationDesigner::calcK`, assigning the denominator's
logarithm first reverses the original call order. Merely moving a numerator
temporary first restores that order but narrows it before the original loads
the constant two. Keeping both logarithms in the return expression restores
the entire 94-byte body under the retained period compiler.

Review inline copies as well as the recovered function. The same change
restores the numerator-first graph in `determineDminForRrn`, which remains
nonexact. Retain the period narrowing and rounding checks, complete-TU data
and export audit, and caller diagnostics. An exact standalone result does
not establish that every inline copy is exact or that the expression is the
unique original spelling. The six-cell `recovery_v90_calck_expression.py`
replay preserves the separate-order and separate-capture controls.

### Test the conversion boundary, not only the constant type

A double constant does not guarantee double arithmetic after simplification.
In `V92Jd::getJdPhase`, retaining the early `(float)phase` conversion let GCC
fold the expression back to SF even with a double denominator. The useful
cross tests both the integer-to-floating conversion and the scale type.
Only the direct integer producer with the default-double denominator
reproduces the original final float store/reload. Initial RTL exposes the
winning DF conversion, DF multiply and SF truncation; the earlier controls
have SF conversion and multiplication without that final truncation.

The original `FILDLL`, `FMULS`, `FSTPS`, `FLDS` sequence provides the independent
witness. The memory operand's single-precision constant does not establish
an SF source expression: a double expression can use an exactly representable
SF constant in the instruction. Audit the complete TU and test the returned
values. A scale-only negative result excludes that control, not the combined
conversion boundary. Preserve the other three cells as explanatory controls.

### Read the spill mode before blaming allocation

Trace an unwanted spill backwards from reload to initial expansion. The
constellation designer's original reconstruction declared the product
`double`, yet its logarithm input expanded into an XF lifetime and `25.greg`
spilled it as XF. Later peephole, register-renaming, scheduling and x87
passes preserved that mode. Changing the logarithm overload introduced
per-iteration DF rounding that the original does not perform. A narrower
spill is not sufficient if its lifetime is wrong.

Separate the accumulator lifetime from the final conversion. A scoped
extended-precision product followed by assignment to a double recovers the
original unrounded recurrence and post-loop DF boundary. That intermediate
candidate still has an extra XF spill carrying the already-rounded value.
This establishes the required value boundary, not the author's declaration:
an original DF store does not prove that every preceding operation used a
double variable. A double-producer split has the same size gap as the
extended-producer split while omitting the boundary and changing another
nonexact body. Equal scores do not establish equivalent recoveries.

Trace the cause of a spill beyond the pass that allocates its slot. Initial
RTL has separate DF-to-XF conversions for the two logarithms; CSE preserves
them, but GCSE merges them into one XF value live across the intervening
calls. Allocation then spills that value. With the unrounded product now
separate, ordinary double logarithm inputs preserve the final DF boundary
without narrowing each loop iteration. The two logarithms reload the rounded
double and the extra XF spill disappears. The earlier double-log control is
still negative for its original accumulator: the producer change is the new
discriminator, not permission to repeat the same spelling search.

Keep the flag control diagnostic. Disabling GCSE changes other bodies and
loses exact functions; it does not justify changing the retained profile.
Record definition/use UIDs and the first pass that shares the lifetime, then
check the resulting source control under the normal flags. A smaller frame
or size gap alone cannot establish that the original values survived.

A separate original lifetime can still justify progress. Capturing the RRN
target before the logarithms restores its witnessed conversion and arithmetic
order independently of the product's final narrowing. If that change reorders a
constant pool, compare each relocation's resolved value in its owning
function and operation, not just the pool's multiset. A same-sized reordered
pool can still send a consumer to the wrong constant.

### Check source grouping before investigating late x87 passes

In `notch`, the original adds the feedforward product to the second state
value before adding the feedback product. Parenthesising that addition
restores the complete function. The changed dependency is already visible
in initial RTL and survives x87 stack conversion; no register or scheduling
control is needed. Use the original operand graph to choose the grouping,
then verify period rounding and the whole TU.

Conversely, independently recovering `FPM_MTD_detect`'s short countdown,
advancing sample pointer and final word comparisons does not settle its clamp
or return allocation. The original clamp explicitly uses NOT, arithmetic
shift by 15 and AND on a sign-extended short; a conditional clamp generated
the shift by 31 instead. The explicit mask recovers that operand graph, and
all 65,536 short inputs establish equivalence to the conditional clamp.
It still leaves different narrowing and return carriers. The length gap
increases from one to six bytes, so describe the restored operations and
remaining mismatches separately. Neither the gap nor the stage that exposes
it proves a unique source declaration.

Trace a residual extension through optimisation before changing its carrier.
MTD's short mask assignment creates an early sign extension in initial RTL;
its original defers that extension until the non-silent threshold arm. An
integer post-mask carrier with an explicit short cast initially puts the
conversion in that arm, but `combine` proves it redundant and deletes it.
The resulting one-byte size gap does not recover the original conversion.
This is an optimisation of known sign bits, not a late register-allocation
failure. Reopen the carrier question only with evidence for an original use
or width that makes the delayed conversion necessary.

### Trace accumulator publication separately from predicate evaluation

`GenericToneDetector::process` publishes the output accumulator before the
input accumulator in both original fast paths. Swapping those stores
restores the scalar prefix and the block overload's publication order,
while preserving each method's object accesses and call targets. Audit both
overloads: the block still schedules its countdown differently, and matching
the scalar prefix does not close either complete method.

A captured predicate is a separate source control. Evaluating a boolean
earlier can introduce a byte result and a later test already in initial RTL,
where the original branches directly on floating comparison flags. Trace
that definition and use before blaming x87 stack allocation. Keep the
publication recovery even when the independent predicate control fails.

Comparison orientation can expose a value failure as well as a codegen
residual. Under the period flags, the scalar tone detector's earlier
threshold-first comparison accepted unordered input, whereas the original
mean-output-first comparison rejects it. A constructor-created detector with
finite positive controls and one external quiet-NaN sample demonstrates the
difference without planting internal state. This establishes a component
boundary failure, not a reachable nonfinite modem input history.

The orientation already differs in initial RTL. Trace the jump inversion
and final x87 split to confirm that later passes preserve it; do not infer a
late operand swap from final disassembly alone. Source synonyms, captured
predicates and split smoother publications can remain negative despite a
proved failure. Preserve the hard-failing probe separately from the existing
suite, label that suite's coverage limit, and require both the value failure
and the original operand/publication graph to close a follow-up.

### Inspect expression folding before initial RTL

An operand order already wrong in initial RTL needs an earlier explanation.
GCC 3.4.2's `tree_swap_operands_p` in `fold-const.c` prefers a non-declaration
on the left when the other operand is a declaration. A named `meanOut` local
compared with the `threshold` member therefore becomes a threshold-first
comparison before RTL expansion. Reversing or complementing the comparison
spelling does not remove this source-tree distinction.

Use the original producer and consumer graph to choose a new control. The
tone detector's original shares the output-mean product between smoothing
and a direct comparison with the threshold in memory. Writing `out * inv`
at the comparison preserves that orientation during tree folding. CSE then
shares the product with the existing mean, and combine folds the threshold
load into the comparison. The final scalar method becomes byte-exact;
no NaN-specific branch or compiler-profile change is needed.

Retain the stage evidence: the original-tree dump distinguishes declaration,
member and arithmetic expressions; initial RTL shows the comparison order;
CSE confirms shared computation. Do not infer duplicated runtime arithmetic
from repeated source expressions, or assume a named temporary is codegen-neutral.
This mechanism is a candidate discriminator elsewhere, not a blanket rule to
inline all expressions into their consumers.

The constructor-based boundary regression from
[issue #292](https://github.com/philpem/slmodem_dsp_re/issues/292) now runs in
the normal tone-detector fixture. Its 64 scalar checks were followed by
192 block-overload checks; that measurement reported 82,628 checks. The
[standalone driver](https://github.com/philpem/slmodem_dsp_re/blob/e82145fe7/tools/probe_generic_tone_unordered.py)
and its [replay wrapper](https://github.com/philpem/slmodem_dsp_re/blob/e82145fe7/test/explore/t_gtonedet_unordered.cpp)
were retired after the default fixture acquired the same scalar regression.
The historical failing run remains evidence of the coverage gap; the passing
regression closes the demonstrated component failure, without claiming a
nonfinite input is reachable through the modem lifecycle.

### Recover original recurrences and entry snapshots before tuning registers

A mathematically equivalent shortcut can hide the original source graph.
`GetFP_Value` used direct ceiling division in the reconstruction, while the
object accumulates subtraction steps in a wrapping short. Restoring the
negative-step recurrence, integer absolute values and original guard removes
the division and recovers the original function length. It remains nonexact.
Bounded short-extreme and counter-wrap tests support the terminating input
domain; zero-divisor nontermination is documented, not executed as a test.
No finite-input value defect was established for the old arithmetic shortcut.

Inspect operand constraints when a mnemonic still differs. The recovered
absolute-value shift exists in initial RTL, but allocation puts its source
in EAX and destination in EDX, allowing the machine pattern to emit CDQ.
The original uses SAR. That difference does not establish a missing source
operation or justify spelling the absolute value as fabricated bit operations.

FSM modulation captures its scale, sample count and genuine frequency-array
base at entry, before calling the tone helpers. Recover those lifetimes with
the observed unsigned-short countdowns and wrapped total. A candidate without
the array-base capture had a smaller size gap, but omitted the original
address lifetime. Preserve the complete owner and reload the child pointer
where the original does; do not invent a field alias to improve allocation.

### Recheck old fixture exclusions against the deciding compiler

An exclusion written for a combined modern/period gate can outlive that gate.
It is not evidence that the period reconstruction fails or that the input
cannot be supplied at a component boundary. The connection evaluator's
constructor/public-update probes pass finite and quiet-NaN controls for
phase-4 ratio and enabled rate-up decisions: eight trials, 24 checks, including
whole-object comparisons. `tools/probe_v90_connection_boundaries.py` retains
those explicit period probes outside default discovery. It makes no modern
portability or modem-path reachability claim and changes no tolerance.

Audit original call slots and owner reloads alongside tests. The checked
V92 RT receive, phase-3 reset and V90 AT report boundaries matched the
reconstruction; their 19 witnesses establish those reviewed boundaries, not
complete coverage of all three functions. Preserve negative audits so the
same comments and branches are not repeatedly treated as new defects.


### Cross decision outcomes, not just exceptional input values

The block tone-detector overload had the same threshold operand reversal as
its scalar sibling, but a separate scoring graph. With a quiet NaN and the
flag set, the incorrectly selected strong arm and the original weak arm both
report a hit. Clearing the flag distinguishes them: the original misses,
while the old reconstruction hits. A constructor-created probe crosses both
flag values with below-half, half-threshold, intermediate, full-threshold,
above-threshold and quiet-NaN samples. It compares the state and counters as
well as the return value. The old body fails two of 192 checks; the restored
producer expression passes all of them, with no special NaN branch.

The size gap stays at three bytes despite this behavioural correction.
Follow that residual independently: the controlled compiler dumps have one
countdown decrement through `27.flow2`; `31.bbro` duplicates it, and both
copies survive scheduling. This identifies a late block-layout question.
It does not establish which source loop produced the original shared decrement.

### Retain witnessed operations even when an aggregate score stays flat

`DialerAbort` tests its progress state with an unsigned range comparison in
the original. The retained source used a signed comparison. Restoring the
unsigned conversion preserves the original decision boundary without changing
the two-byte size gap. Initial RTL distinguishes `LE` from `LEU`; five other
functions in the TU and its data and bindings stay unchanged. Existing valid
state and diagnostic-transcript tests pass. Negative internal states have no
established modem history here, so this is a recovered guard, not evidence
of a reachable modem failure. A previous failure to reach whole-function
identity does not invalidate an independently witnessed partial recovery.

The PCM-upstream arm of `V34SetINFO1aBits` similarly collapsed seven output
publications into one. Restoring the initial zero and six flag updates
recovers the original low-byte reads interleaved with word stores, reducing
the size gap from 77 to 48 bytes. Its three known callers use disjoint output
storage; no aliasing modem failure is claimed. Keep the old collapsed body
as a negative control for the seven-store detector. Runtime unobservability
at known callers does not establish byte identity or justify suppressing
original operations.

### Trace unchanged neighbours through peephole selection

The IIR scalar helper exposes a short countdown, coefficient and history
cursor updates, and a feedforward narrowing before the saturation decision.
Recovering those boundaries improves both the helper and its inlined block
caller. Their remaining size gaps are two and eighteen bytes respectively.
Valid component tests include coefficient/history aliases within one declared
short array; these do not establish an overlapping modem call.

The untouched `FPM_iir_filt_II` also changes. Its compared instruction streams
agree from initial RTL through `27.flow2`. The first differences are two
scratch-register choices at `28.peephole2`; register renaming changes two more
instructions later. Record that collateral and audit the complete TU rather
than attributing it to an invented field error or spill. The retained controls
preserve data and bindings and lose no exact functions. The audit compares
fourteen paired stages for this neighbour and six stages for the scalar
feedforward conversion.


### A more faithful return can increase a size gap

The IIR section accumulator was an `int` even though each new value was
narrowed to `short`. Returning it as `short` introduced another sign extension;
the original returns a copy of its already extended result. A bounded control
using a short accumulator changes the return to a copy in initial RTL.
Combine removes the intermediate return conversion, and the copy survives
allocation and scheduling. The scalar size gap moves from one to two bytes,
while the inlined block caller also changes and the other IIR function stays
byte-identical. The original return operation supports this partial recovery;
no claim of whole-function identity follows from that control.

The positive-iteration path supplies a further witness: at `0xa8af5` the
original sign-extends the section output into EDX, copies it to feedback ECX
at `0xa8af8`, then returns EDX at `0xa8b09`. Keeping a separate short output,
assigning it to the next section's input and returning it recovers that
output/feedback boundary. The complete scalar function reaches REGALLOC with
59 matched instructions. The inlined block caller and the untouched second
IIR engine must still be audited across the complete translation unit.

An uninitialised output must not be inferred from the undefined zero-section
path alone. Here the positive path independently establishes the two
carriers. Under `DSPLIB_REPRODUCE_BUGS`, the zero-section result remains
unconstrained and its D393 differential-fixture exclusion stays in force.
The normal build separately returns the input for a nonpositive section count;
`make check-iir-sections` verifies that repair without treating the blob's
undefined result as an oracle. The reconstruction-mode object must remain
unchanged by this guarded repair. Register-renaming identity does not establish
a unique original local-variable spelling.

### Recover conversion boundaries at the use that establishes them

`V8GetMessage` loads a received word with zero-extension before shifting and
passing its low byte to `charFlip`. The reconstruction sign-extended that word.
An unsigned-short conversion at this use restores the original opcode; the
full-TU control changes exactly one byte and leaves six bystanders, data and
bindings unchanged. The final low-byte selection masks the difference in
result, so this is instruction recovery without a demonstrated behavioural
bug. It does not establish that every use of the shared word field is unsigned.

### Inspect the actual compiler's duplication rule before changing loops

The tone block's countdown remains single through `27.flow2` and is copied
while `31.bbro` connects traces. The retained dump records the seven-byte
shared block being refused during one copy decision, then duplicated when
connecting the slower trace. The installed Gentoo compiler retains DWARF
and function symbols: read-only inspection of `copy_bb_p` and
`connect_traces` confirms the hot-code size multiplier and entry-frequency
threshold used by those decisions. Record the compiler hash with this
inspection; a matching upstream source line alone is weaker evidence.

A full-TU `-fno-reorder-blocks` control removes the copy, but increases the
block's size gap from three to 27 bytes and loses five exact bystanders.
The streams agree before reordering apart from declaration addresses and
label numbers. This isolates the pass without recovering original source.
Keep the retained flags. Reopen loop-source work only with an independent
original branch or loop-boundary witness; the compiler's willingness to copy
a block does not by itself supply one.

Source syntax alone is also a poor nomination test. Two constellation-mask
helpers are written with an `if`, but their retained compiled bodies already
contain the original signed SETL/NEG/AND graph. An explicit mask expression
folds back into a branch and worsens both bodies. Check the current emitted
operations before calling a source construct missing. The losing full-TU
control closes that nomination; it is not evidence for a signedness defect.

The separate `tools/probe_v90_sd_boundaries.py` component probe crosses three
finite/quiet-NaN constructor thresholds, four limits and two public input
sequences, including reset. All 7,680 return/state/history checks pass across
64 cases. It remains outside default test discovery and does not establish
nonfinite modem-path reachability or complete SD-detector coverage.

### Separate missing storage from alignment and declaration order

A smaller BSS section does not establish a missing field or variable. Inventory
named objects, including duplicate local names, before interpreting its extent.
The whole-object audit finds equal sizes for all 38 named BSS objects, covering
2,506 bytes on each side. The 24-byte section deficit lies entirely outside
those named extents. COMMON symbol values encode alignment requirements;
positioned symbols expose their section alignment, not their original source
alignment. Do not assign global ownership from the nearest FILE record.

Callprog provides a bounded source recovery within that inventory. Reordering
its declarations restores all eleven local BSS offsets relative to the first
object. Six functions retain their instructions and canonical relocation
targets; three raw bodies change only relocation addends. The three exact
bystanders remain exact. This improves storage fidelity without adding an
exact function or closing the whole-object BSS deficit.

Use `tools/storage_layout_audit.py` to distinguish named-size differences,
alignment gaps, COMMON alignment and nonallocated records. Its five controls
exercise these distinctions before reporting the comparison. Table extents
need a separate behavioural review: the extra FPM entries and PreFilter loop
row deliberately reproduce observed reads beyond the original symbols.
Recover their physical neighbours before shortening those declarations.

The inventory also identifies storage qualifiers that instruction comparison
cannot see. Four local AGC coefficient arrays in the V.22 and B.103 table TUs
were declared `const` despite occupying writable `.data` in the original.
Removing those qualifiers restores their storage class. Four full-TU controls
preserve eighteen named objects' contents and sizes and 34 pointer targets;
the old read-only placement fails the original-permission witness. These
TUs contain no functions, so the recovery cannot increase the exact-function
count. No runtime write to these arrays is established by this evidence.

### Establish a carrier discrepancy before changing its type

The V8 lifetime trace follows the output argument from initial RTL through
global allocation into EBP. The original reloads that by-value argument from
its stack home; the byte-only `charFlip` call cannot modify the argument.
That reload alone supplies no mutable-owner or stale-pointer defect. Reopen
this source domain only with independent evidence about a carrier or callee
visibility that affects allocation.

Likewise, INFO1a's original Uinfo producer sign-extends AX and stores a full
dword. Its later low-word load does not establish a short local. The retained
source already produces that signed dword and later zero-extends its low word.
The original also keeps an independent pointer at owner +4 live; the current
flat layout only establishes a progress integer there. Investigate independent
callee and layout evidence for an aggregate boundary before introducing a
nested owner to influence spills. The unchanged full-TU replay and a rejected
wrong-producer control preserve this negative result.

### Recheck physical adjacency before retaining an emulation row

The PreFilter type-2 loop table has 33 records in the original. An added zero
record reproduced the counting loop's read beyond that symbol, but the period
compiler's actual layout makes it unnecessary: the following type-1 table is
naturally aligned to 32 bytes. Restoring the original extent leaves exactly
28 zero bytes between the tables, matching the original terminator location.
Two full-TU controls compare all seven objects and sixteen pointer targets;
the original-size control passes the table and PreFilter period fixtures.
No explicit padding or synthetic terminator is needed. Check the emitted
adjacency as well as the counted length, because a correct traversal alone
cannot distinguish the original extent from an extra emulation row.

This evidence does not justify truncating every extended table. Each observed
overread needs its own neighbour and alignment proof under the actual link
order. Preserve the original boundary fixtures while recovering that storage.

### A repeated shifted address does not establish a nested type

An independent `V34GiveProbeResults` recovery already reproduces its shifted
owner address with either an integer-pointer increment or a byte offset.
INFO1a's repeated owner +4 address therefore adds addressing and liveness
evidence, without establishing an embedded aggregate. The current member at
that offset is an integer. Require a typed callee, constructor, or independently
bounded member layout before changing ownership to influence allocation.
The retained original-operand audit rejects both a wrong spill width and a
wrong Precoder source field; its selected boundary checks are not complete
behavioural proofs of the audited methods.

### Use references to local data to refine a global function's TU

A local function's first address need not begin its translation unit. Original
LOCAL `tx_in_internal` and `rx_out_internal` symbols belong to `V32mod.c`, and
`V32FP_modem` has four direct references to those buffers. Together these
witnesses place the global function with those locals, before the deferred
static handlers. The earlier split at `v32_data` omitted this evidence.
Moving the function and buffers together preserves all 34 functions' instruction
streams and canonical relocations across the four-TU control domain.

`Control_Flag` has a local referrer in that same TU; placing it there restores
its original offset before the buffers. The partial BSS extent moves from
24 bytes below the original to four bytes above it. A further cell placing
`SMCv32_CFG` with its sole consumer matches the extent, but that global's
consumer alone does not establish original TU ownership. Retain that cell
as diagnostic evidence. A perfect section-size score does not replace the
missing ownership witness.

### Preserve writable identity when modelling out-of-symbol reads

A copied extension of a sign table can reproduce its initial values while
losing the original storage dependency. The phasor's exported sine and cosine
tables are writable; original instructions read those symbols directly.
Reading private extended copies hides later writes. A component fixture that
writes the public tables distinguishes this defect without claiming that a
modem path performs such writes.

Prefer the witnessed direct indexing when the original physical neighbourhood
can be recovered and measured. The direct phasor control removes the two
invented arrays and restores the original COEF_DC, sine and cosine positions
at relative offsets 0, 12 and 20 in the linked period image. Existing exhaustive
sweeps cover the negative-index window, and the added writable-table checks
cover the lost dependency. A branch-based owner-routing control explains the
bug but is unnecessary once direct reads and adjacency pass. Trace the symbol
references in initial RTL: a wrong owner there cannot be repaired by later
register allocation or instruction scheduling.

### Distinguish table extents from the final test link's section grouping

The remaining extended FPM tables have identifiable original neighbours:
`sqrt` reaches the first reciprocal-table word, `div` reaches the XOR table,
and `log10` reaches the first word of `FPM_PPS_CFG`. An original-size aggregate
control restores those relative positions without explicit padding. Validate
the final test executable as well: linking individual TUs can interleave their
string sections with ordinary read-only data. The period log10 fixture then
reads a diagnostic string instead of the configuration word, even though the
relocatable aggregate has the correct adjacency.

Keep that source cell diagnostic until the deciding fixture measures the
intended aggregate boundary. A future fixture-link change must preserve
cross-TU wrapping, test-visible local symbols, link order and cache invalidation.
Do not move a string or add a source section attribute merely to accommodate
the harness's different grouping.

### Follow loop motion before expanding a coefficient-type experiment

`V90SpectralShapingFilter::getMetric` reloads coefficients per block in the
original and uses two single-precision stack slots. Moving the reconstruction's
coefficient definitions into that loop does not recover those operations:
initial RTL retains the inner placement, GCSE hoists the loads, and loop
optimisation hoists their extended-precision conversions. A crossed float-local
control still promotes the coefficients for the extended-precision recurrence
and retains four extended-precision spills.

The four cells produce two complete TU emissions, preserve the same six
function verdicts and leave all non-text sections unchanged. The diagnostic
float cell passes the period fixture, but does not restore the witnessed
operations. Close this declaration/type domain. Reopen it only with independent
evidence for a source-use or ownership boundary that explains the original
single-precision consumers.

### Test the declared scalar boundary when a reconstruction adds a clamp

The original `FPM_sqrt` reads beyond its 192-word table into the adjacent
reciprocal and XOR tables. The reconstruction added both an endpoint and a
clamp, and its differential fixture stopped at Q15. Restoring the original
sqrt/div extents exposes the original contiguous read window; removing the
unwitnessed clamp then changes 32,704 failures over all 65,536 unsigned-word
inputs to zero. This is component-boundary fidelity, without evidence that a
modem caller supplies values above Q15.

Keep the controls separate: restoring table extents alone leaves all 32,704
failures. The bug-reproduction build restores the reciprocal table's original
128 words, while the fixed build retains its generated endpoint. The scalar
sqrt size gap increases despite the corrected behaviour. Record both facts;
matching behaviour and witnessed reads outrank minimising a length difference.

### Keep local-data ownership evidence separate from naming heuristics

A GLOBAL function's relocation into a unique LOCAL object's actual interval
can identify an original FILE occurrence. Preserve symbol-table indices,
section, extent and FILE occurrence, including duplicate names. Reject global
targets, gaps, one-past addresses, overlapping objects and conflicting owners;
the nearest FILE record is not evidence for a GLOBAL object's definition.
Mergeable and linkonce storage also require separate treatment.

The original-object screen finds 104 uniquely witnessed functions among
1,805 GLOBAL/WEAK functions. All 104 current source basenames agree; this
method yields no further source-move candidates in that set. It supplements
existing attribution without replacing its 1,764 name-derived records.
`SMCv32_CFG` remains unresolved because it is a GLOBAL target. The controls
must reject that tempting nearest-FILE inference as well as detect a deliberately
wrong source assignment.

### Re-run diagnostic-anchor triage and then prove each missing boundary

A fresh census over 300 objects found ten count mismatches among 1,842 eligible
object-symbol entries. A deliberately renamed debug relocation makes the known
`call_delete` detector fire, validating the census before interpreting its
output. Counts alone remain nominations: `RingDetector_Create` differs because
the original inlines the reset helper that contains the print.

The call supervisor has a different explanation. Its create-entry and
rate-conversion announcements, and its old/new progress-message report, were
absent from source. Recover the guard, arguments and publication timing together:
the progress report observes the old stored message and the new result after
output resampling, before publishing the changed message. A public-registration
fixture crosses sample rates and debug levels and repeats blocks to check that
unchanged messages stay silent. State/output agreement alone would miss these
transcript defects.

The B.103 constructor also lacked its original version banner. Recover the
binary's date and time literals, not the reconstruction build timestamp. A
public-constructor fixture crossing debug levels and constructor parameters
exposes the missing output without planting internal state. Keep other
count differences pending until their call and inlining boundaries are checked.

### Preserve an owner's reload after an intervening call

In `V90Phase3Demodulator::twoLevelDemod`, the original reloads the detector
owner after `isAltRbs` before reading two tables. Reusing the pre-call local
loses those loads. Expressing the two post-call accesses through the owning
member restores them and reaches the original body length, while the other
25 TU bodies remain unchanged. This is instruction-graph evidence, not proof
of a functional defect: the observed callee does not change the owner.

### Cross normalization homes with the operation that consumes them

Both FPM sqrt bodies use caller-owned 16-bit normalization outputs in the
original. The count recurrence is promoted inside the loop and published after
its backedge. The subsequent parity test compares the count with twice its
halved value; replacing that graph with a bit test changes the emitted code.

A bounded cross separates pointed outputs from local values and quotient-based
parity from a bit test. Pointed outputs with quotient-based parity recover the
operations but leave both bodies two bytes short because the stack homes are
reversed. Crossing the caller declarations against the observed homes—count at
stack offset 0, mantissa at offset 2—makes both bodies byte-exact. The three
other emitted functions, data and bindings remain unchanged. The scalar's
explicit narrowed index also replaces the old subtraction folded into the
table relocation; audit that destination with the instruction graph.

Use the existing full unsigned-word scalar sweep and wide-input double-word
fixture to validate this recovery. The prior clamp correction made those
component boundaries trustworthy before pursuing the code-generation match.

### Check signedness before treating a residual as allocation noise

In the call supervisor, the original S-register comparisons use unsigned
conditions. Its queue offsets and active-byte count also use unsigned minimum,
modulo and halving operations, while the available-byte count remains signed.
The reconstruction's signed operations already appear in initial RTL. Trace
that divergence before changing declarations merely to influence allocation.

Keep independently witnessed boundaries separate. Recover unsigned consumers
without claiming an unsigned host API return type when no original declaration
establishes it. Read the stored modem owner where the original reloads it.
Cross the S-register and queue changes to identify their separate effects, and
check every TU body and switch-table destination. A larger size gap can expose
remaining address and lifetime differences after the wrong arithmetic is gone.

Ordinary public-constructor and block histories validate the retained source.
High-bit host-return probes are separately labelled synthetic: they expose the
S56 decision difference but do not prove the modem host supplies those values.
Do not infer a second value defect from S16's unsigned instructions when the
measured probe does not fail there.

A smaller owner-reload candidate can still move the wrong reads. Rewriting
`call_block` to obtain its state through `dp->self` recovers the inner owner
reload, but also moves the input/output buffer-offset reads to that owner.
The original computes those buffers from the outer state before reloading the
inner processing owner. Reject that whole-helper candidate even if its size gap
shrinks and the lifecycle tests pass. A later experiment must preserve the
outer buffer producers separately from the inner processing owner.

### Separate missing diagnostics from shared blocks and defensive arms

Resolve formats and call boundaries before interpreting a debug-site count.
The B.103 answer-state mismatch comes from split sibling tails; CID creation
inlines its reset diagnostic; V.8 DATA and V.80 indications share a print block
in the original; fax unframing shares CRC-success prints. Those count
mismatches justify no source correction. Retain format-negative controls so
an audit can distinguish an omitted format from these explained differences.

Two other fax sites contained actual omissions. Receive-buffer recovery lacked
its timestamped report and original index reloads around output handling. A
valid-record queue probe detects the transcript difference, but its supplied
state and countdown remain synthetic component coverage. Finding a modem
lifecycle producer is separate work, not a property conferred by a passing test.

The constructor's published configuration-pointer checks also lacked their
shared diagnostic and NULL-return arm. The original performs unchecked writes
before those checks, so ordinary allocation failure is not a safe way to reach
them. Restore the independently witnessed defensive graph and its lack of
cleanup without claiming allocation-failure safety. Review return and cleanup
behavior whenever a missing print reveals an omitted branch.

### Give an aggregate-boundary fixture an aggregate input

The original log10 table has 128 words and its endpoint read reaches
`FPM_PPS_CFG`. Linking the reconstruction TUs individually into a test executable
interleaves string storage at that boundary. An opt-in fixture mode first links
the ordered, test-visible source objects into a relocatable aggregate, then links
the fixture, harness and reference. This restores the measured neighbour without
source padding, a fabricated endpoint, or section attributes.

Preserve interposition at the partial-link boundary: apply fixture `--wrap`
options there as well as at the final link. A final-only wrapper is too late
once an aggregate has bound the source call. Likewise, aggregate the test-visible
copies so fixtures can still name selected original-local symbols. Negative
controls must demonstrate both failures before trusting the positive run.

Cache aggregate inputs by ordered content, reference and apparatus identities,
wrapper options and the actual linker identity. Cache final links by mode and
link options as well as their inputs. Test stable reuse, mode switches and
content changes with restored timestamps. A correct endpoint in the relocatable
object alone does not establish the endpoint in the executed fixture.

### Preserve buffer producers separately from a reloaded processing owner

The call supervisor computes input/output buffer addresses from its outer
state, then reloads `dp->self` for block processing. Passing the already-built
buffers into a helper that reloads the processing owner preserves both ages.
Reloading the owner before building the buffers changes the graph even when
ordinary lifecycle tests cannot distinguish the two owners.

Address units matter too. The original adds active byte offsets directly;
forming a short pointer with `active / 2` introduces an extra `AND -2` during
combine. Recover byte-address addition before casting the result to a short
pointer. The supported owner and address changes preserve the other TU bodies,
data, bindings and switch destinations; a smaller size gap alone would not
justify them.

### Expand sibling tails before declaring a missing format

A root function's debug-call count can differ because one compiler inlined a
helper and another retained its call. Expand both direct calls and sibling
jumps within the declared TU boundary. A call-only walk of V.34 finds 107 of
110 original format strings; including sibling tails finds the remaining three
in the MOH hold helper. Preserve the call-only walk as a negative control.

Likewise, V.8 can share two formats at one call, and V.90 progress can inherit
four formats from inlined helpers. Compare resolved format contents as well as
counts, and prove the detector rejects a changed format whose count stays the
same. A complete unique-format inventory is not a proof of guards, argument
ages or dynamic multiplicity. Once the count discrepancy is explained, require
independent evidence before changing source or trying more CFG spellings.

### Replace planted control state with production history incrementally

The buffered fax receive fixture now starts with constructor-created sessions,
supplies valid buffered records, and drives the public command and progress
methods. Those methods select the emulation state, arm and expire the countdown,
advance the read index, and reset an exhausted queue. The fixture exercises
three records at four debug levels and detects six transcript failures when the
recovered diagnostic is deliberately omitted.

This strengthens the boundary without establishing the upstream producer:
the queue records are still supplied by the fixture. Keep that limitation in
its label, retain the older synthetic handler probes, and seek a separate
VMI/HDLC producer history before claiming modem-path reachability.

### Stop at the compiler stage that explains a near-exact tail

Log10's original normalizer uses pointed 16-bit outputs and writes the adjusted
exponent back into the count home. Crossing those boundaries recovers the
54-byte normalization segment exactly and reduces the complete body's size gap
from 83 bytes to two. A scalar-only writeback control emits the baseline; an
unsigned-short index control emits the same improved TU. Neither justifies
another nearby type or return-form search.

The remaining trace is specific: regmove coalesces the subtraction with the
table-value pseudo, global allocation assigns that value to EDX and the
coefficient to EAX, and the final sign extension therefore needs a three-byte
move instead of the original one-byte EAX extension. Later peephole and register
renaming passes preserve that binding. Do not force a register to hide it.
A new experiment needs independent evidence for a value owner or definition
boundary that would explain the original tail allocation.

The call supervisor supplies a related arithmetic discriminator. Its original
sample-count update uses a direct signed 32-bit right shift. Reconstructing it
as division by two introduces the sign correction needed for truncation towards
zero; initial RTL already contains that bias, which combine retains. Recover
the witnessed arithmetic shift and check the full TU even if alignment makes
the size gap grow. Valid host-loop lengths are nonnegative, so this recovers an
instruction operation without demonstrating a new reachable behavior defect.

For `call_create`, the next bounded investigation is local stack allocation:
the original places the configuration at stack +0x10 and the dial buffer at
+0x20; the reconstruction places them at +0x50 and +0x10. The difference is
already present in initial RTL. Trace expansion and inlined-helper allocation
timing before trying a source control. A declaration-order palette would not
explain the cause. Likewise, `call_run`'s cached queue bases differ before
allocation, but the available trace does not establish an extra-spill cause.

### Compare literal contents even when call counts agree

A call-site census cannot detect a substituted message or missing whitespace.
`tools/recovery_literal_census.py` checks literal consumers across the canonical
objects, including functions whose call counts agree. On the ninth PR291 pass,
300 objects supplied 1,886 shared body entries (including duplicate emissions),
492 with original string literals and 2,444 unique body/literal pairs. Same-TU
call and sibling-tail expansion explained two direct-body mismatches. The two
remaining bodies contained three actual text discrepancies:

- `getV92Decision` printed `JdNot detected` at the completed-reference-study
  transition. The original prints `enter Wait For V92Jd state`, with the sample
  count before publishing state 7 and clearing the counter.
- `runPcmModem`'s echo-silence ON and Off messages each lacked a leading newline.

The V.92 fixture's earlier 213,241 checks passed without reaching that terminal
study diagnostic. A targeted synthetic boundary supplies ADID terminal state 6
and exercises caller counts just before and at the modulo-six transition, at
four debug levels. Two of its 216 checks fail before the text correction; all
pass afterwards. This proves the component discrepancy, not a complete study
history or modem-path reachability.

The census's real ELF negative control changes a literal without changing file
length or relocation sites. It inventories only `.rodata.str*` address
relocations; it does not prove guards, arguments, multiplicity, indirect-call
coverage or the contents of other data sections. A clean inventory therefore
closes these nominations, not the behavioral audit. Canonical relocation
consumers must still be checked: adding a string can move a bystander's raw
relocation addends without changing its instructions or consumed values.

Treat fixture comments as hypotheses too. The PCM fixture blamed an extra blank
line on `V90Modem::progress`'s `Illegal modemSide` message. Both objects consume
exactly the same literal there. Once the caller's echo messages were corrected,
all existing diagnostic rows passed complete transcript comparisons; the
misattributed exception was removed.

### Trace stack allocation before changing declaration order

For `call_create`, tracing the actual Gentoo compiler's `expand_decl` and
`assign_stack_local_1` established that the configuration's 16-byte allocation
preceded the dial buffer's 64-byte allocation. Inlined-helper expansion came
later and allocated no additional stack object. The original stack positions
require the reverse allocation order. One buffer-first declaration control
restores configuration +0x10 and dial buffer +0x20, preserving all ten bystander
bodies, seven exact names, data and bindings. The function remains SIZE2.

This is evidence for one allocation-order control, not a reason to enumerate
declaration permutations. Use the executing compiler's entry ABI and measured
frame offsets when optimized debug information aliases parameters incorrectly;
upstream compiler source is supporting context, not proof of the installed
Gentoo binary's behavior.

For leading or trailing whitespace, compare individual debug calls as well as
concatenated transcripts. An initial echo-message substring probe falsely
passed because the preceding message supplied the missing leading newline.
The retained fixture wraps both debug imports, checks each formatted message
at its call boundary, and forwards it into the ordinary capture. Its known-old
source fails two checks; the corrected source passes. The selected component
events are explicit fixture inputs, not evidence of an end-to-end modem path.

The remaining `call_create` pointer copies illustrate why pass names need
verification. On this compiler, `flag_if_conversion` controls post-combine
`21.ce2`; `flag_if_conversion2` controls the later `29.ce3`. Disabling the latter
emits the unchanged object. Disabling the former keeps the pointer copies
branch-local and removes the measured overlap, but the copies survive and
`call_create` grows to SIZE17. That control explains interference without
recovering the original source. It also changes `call_run` and
`IsPulseDialerReady`, so it supports no production flag change.

### Extend the producer boundary without overstating reachability

The fax replay fixture now constructs TX/RX framing objects, writes payloads
through the HDLC frame writer, and passes six-bit chunks through the actual
packer/unpacker. Three decoded records, including stuffing-heavy payloads,
feed the existing buffered command/progress replay. The new 2,833 checks bring
the fixture to 3,762; corrupting a source-side payload makes 302 new checks
fail while the older 929 still pass.

This establishes digital framing provenance. The fixture still publishes the
queue itself, so V.21 PCM acquisition and the state-6 queue-append path remain
outside its demonstrated history. The next source discriminator is that append
path's value timing: the original prints the pre-append length/index, then
reloads both before checking capacity and publishing the record. The current
source calculates length and total before the print. Trace that difference
without assuming a mutating diagnostic callback is a valid modem operation.

### Run inexpensive fidelity inventories before compiler tuning

Start with the whole compiled data and literal inventories, then review their
consumers. Align named objects and resolve relocation targets before comparing
contents. For anonymous strings and constants, retain section, offset and
consumer evidence; a nearby printable word is not automatically a diagnostic.
Sliding matches can locate inserted or missing bytes, but padding and string
pooling are not source defects by themselves.

The lexical `debugaudit.py --absent` check also needs header and macro literals.
Its old default falsely reported 40 missing consumers because it scanned only
C/C++ files and discarded `#define` lines. Including headers and concatenated
macro literals reduces that report to zero absent messages, with 15 unresolved
call sites still explicit. Six controls cover the old omission, macro joining,
ordinary literals, include-path exclusion, changed text and escaped-newline
splicing with preserved source line numbers. This presence test
still cannot detect a valid literal attached to the wrong function; the compiled
consumer audit found the V.92 substitution precisely because it keeps ownership.

Instruction-loop differences are nominations until reviewed. Record actual
bounds, stride, signedness, calls and stores, and separate equivalent sentinel
or promoted-width encodings from missing behavior. A detector must fire on a
known changed bound, stride, store and call before a clean result is useful.
Do not return to register-allocation work while an independently supported data
or behavior discrepancy is waiting for a differential regression.

`transcript_exact` now compares exact per-call start/end positions as well as
complete text, byte length and call count. Its negative control produces equal
concatenated text with two calls on each side, but moves a newline between the
calls. The former comparison accepts that arrangement; the boundary comparison
rejects it. Both text and event capacities fail closed on overflow. Use this
shared helper when extending transcript fixtures rather than relying on a
substring match or equal line counts.

The initialized-storage content census found two defects that the size/layout
inventory could not: two `p2400` coefficients were -7786 instead of -7790, and
`MTDb103Org_CFG` represented its first pointer as zero raw words. The original
relocation targets `MTDb103_COEF`. Comparing 724 matched initialized objects
found 722 equal and these two different; seven assembled ELF controls exercise
content and relocation changes, shifted layouts and ambiguity handling. The
new component fixture fails three of 58 checks before correction and passes
afterwards, using `V34SetupModulator` and the actual `FPM_MTD_create` constructor.
All function bodies and exact-name sets remain unchanged by these data fixes.

Anonymous numeric consumers remain a separate queue: 26 of 153 shared-function
inventories differ. Width changes, constant folding and inline boundaries can
explain such differences, so they are not 26 demonstrated bugs. `dtmf_test`'s
one-ULP constant difference is a concrete next operand to trace and test at its
decision boundary before changing a literal.

The broader literal audit retains ordinary `.rodata` and `.data` address
consumers, but labels printable numeric/indexed loads as ambiguous. It compares
112 named-storage windows per side without finding a difference. Its real ELF
controls change a format and slide a relocation by one byte; the old V.92 and
echo defects also fire. Remaining static-count nominations need distinct-path
transcript coverage, particularly `DialerProgress` and the V.92 TRN1u reset
lower-bound report. Do not force duplicated calls to equalise static counts.

### Decode constant operands before comparing their storage formats

`numeric_consumer_audit.py` decodes each x87 memory operand and compares exact
numeric values across float, double and extended storage. On the next bulk
pass, the 26 raw numeric-pool nominations split into 16 same-value inventories
with different consumer counts and ten different value sets. This removes
storage-format noise without proving equal operations or paths. Constants
materialised as immediates, implicit x87 constants, inlined callees and
reciprocal multiplication still need separate evidence.

Twelve controls establish float/double/extended equality, integer-load
conversion, changed values, signed-zero distinction, extreme extended values,
correct accounting for interior and out-of-bounds pool loads, and rejection
of unsupported ELF inputs.
NaN payloads and noncanonical extended encodings retain their representation.
Duplicate function names remain unclassified instead of silently merging
consumers. Use this queue to find value discrepancies; do not force identical
constant pools or static reference counts.

The DTMF US level cutoff was a real value discrepancy: the binary consumes
float bits `0x3b03126e`, while `0.002f` emits the next float, `0x3b03126f`.
A direct decision-component fixture crosses adjacent representable totals with
finite, nonnegative notch energies. It checks US, EUR and other-mode dispatch,
and includes ratio-rejection controls. Three of 100 new checks fail before the
correction; all pass with the original value. This establishes a component
threshold difference, without claiming those exact energies came from a modem
waveform. Only one constant byte changes; all four function instruction bodies
and the other positioned relocation consumers stay equal.

### Keep object registers in the side-effect census

A store through EBP is not necessarily a stack store under
`-fomit-frame-pointer`. The structural screen had discarded all such writes,
creating an apparent missing-store deficit in `V22_SRE_init`. Recognising an
actual `mov %esp,%ebp` frame setup before excluding EBP stores reduces the
screen from 239 to 224 nominations. Two added assembled controls distinguish
an EBP object owner from an established frame pointer.

Expand direct same-TU calls and sibling tails before comparing side-effect
presence. Six ELF controls prove expansion, preserve external boundaries and
detect omitted calls, tails and callee stores. Four reviewed nominations retain
their behavior: V.22 initialization stores, inlined convolution mapping, shared
V.92 reset logging, and an unrolled CRC store sequence. Physical instruction
counts alone do not establish dynamic store counts, argument equivalence or
missing behavior.

Decode the load width before extracting pool bytes. A four-byte load at +12 in
one 16-byte entry is valid; treating its entry width as its load width drops
that consumer. The numeric audit now extracts relocations independently and
requires every pool relocation to become either a classified consumer or an
explicit unclassified record. A cross-boundary load remains unclassified.
Record input hashes and fail an empty shared-consumer comparison.

### Exercise distinct diagnostic paths with public component histories

Static print counts nominated `DialerProgress` and V.92 phase-3 reset, but their
reviewed branches share or duplicate the same runtime message. The dialer
fixture now covers 18 public create/progress histories: initial and later
tones, commas before and after digits, capped and uncapped pauses, and the
70 ms DTMF duration's 560-sample argument. Semicolon histories stop at the
public command event. Complete transcript checks include individual call
boundaries and preserve callback markers.

V.92 reset coverage uses constructor-created objects through reset, symbol
generation and reset again. Twenty-four observations cross the 8160 lower
bound and six-sample quantisation boundary, at three debug levels. The
reference encoder supplies the expected wire representation of the independently
expected length. Captured-argument byte mutations fail in both fixtures and
restoration passes. These tests strengthen component coverage; they do not
establish every modem history or justify forcing duplicate static print sites.

The inventory nominated the original multiplication scales versus reconstructed
reciprocal scales in `V90Equalizer::convertEqualizerToMmx`; the quantisation
audit below closes that pool-only lead. Inspect the paired operations and
destination fields before changing a value. Other inventory gaps already have immediate producers:
`MTK_phasor`'s float 2*pi and `voice_rx`'s reciprocal input scale. Their absence
from a constant pool is not evidence of an absent value.

### Retain original operands even when the exact-function count is unchanged

Two earlier NaN controls were left out after producing no exact-function gain.
The bulk fidelity pass revisits their source choice against the current TUs:
`porcessSecondStudy` and `setConstellationToNoise` load constant quiet NaNs in
the binary, whereas the reconstruction called `nanf("")`. Replacing those
initializers with `NAN` removes one unmatched library call and its empty-string
argument from each TU. This is a call/operand recovery; the normal runtime
values were already equal and no new functional failure is claimed.

Four full-TU cells compare 116 emitted bodies. All 56 canonical bystanders,
bindings, named data and nontext relocation destinations remain equal; 31 exact
names are preserved. The intended differences are two quiet-NaN consumers,
two removed imports and empty arguments, and the target bodies. Five focused
period fixtures pass. ADID's size gap stays 64; the designer's gap grows from
18 to 43. Keep that result explicit instead of describing fewer bytes as a
recovery criterion. Earlier no-gain measurements remain valid.

### Observe state at host callbacks, as well as the delivered bytes

A correct final state and transcript can hide publication at the wrong time.
The CID fixture now observes its owner during 18 paired TTY callbacks from real
Bell-202 valid/corrupt receive histories. It records the rendered buffer,
payload and owner-relative offset, modem identity, mode, option and sample-fill
state, while forwarding the callback unchanged. A state-only corruption is
rejected even though the delivered TTY bytes stay identical.

Three reviewed paths retain the original order: CID recomputes its string
length and owner after host calls; V.8 publishes handoff/expiry fields around
its host calls; voice reloads state after receive and publishes its host count
after send. Only CID has new real-signal callback observations in this pass.
Planted V.8 negotiation states and voice handler scripts do not establish an
ordinary signal history. The negotiated-link fixtures below supply that
evidence for V.8 handoff and expiry.

Resolved call/tail annotation also explains four new structural nominations:
DialerAbort's diagnostic tail, V23ModemMain's demodulator tail, shared voice
resampler creation, and merged FSE history-publication suffixes. FSE's normal
`FPM_FSE_receive` producer uses disjoint local angle/magnitude buffers, so a
captured-history load-age difference alone does not establish an ordinary
producer defect.

### Test quantisation boundaries before replacing reciprocal constants

The equalizer source already spells the original `1 / (beta * 2**k)` grouping;
the retained compiler reassociates it into `2**-k / beta`. These pool values do
not establish a source error. The new constructor-based fixture uses actual
parameter, phase-4 and equalizer constructors, coefficient/beta setters and
`enterDataPhase` to derive maxima and conversion fields. It plants neither
maxima nor MMX history.

Fifty-four trials cover 18 adjacent exponent-boundary groups at two debug
levels. All 812 checks pass across the object, aligned arrays and complete
transcripts. A compiled-object wrong-scale canary fails 81 checks and exits 1.
The unchanged full-TU control reproduces all 35 bodies raw and preserves 20
exact names. No source expression or compiler option changes follow from this
result. The subsequent field-positioned map of V90 default stores includes
immediate producers, which another pool-value substitution would miss.

### Keep parallel mutation subsets non-recordable

`mutate.py --only ... --jobs 2` used to discard the filter when spawning
workers. A requested four-mutation check therefore started all 482 ADID
mutations and printed a full-suite summary. The corrected runner forwards the
filter and verbosity, budgets workers from the selected domain, and prints a
non-recordable subset summary even when an empty filter selects every label.
The unsupported `--all --only` combination fails before dispatch.

The snapshot parser also rejects subset output and summaryless partial
verdicts, preserving the old entry. Fourteen mocked controls reproduce the old
failures and test filtering, worker failure, subset reporting and snapshot
refusal. They need no copied build trees or mutation cycles. Actual four-label
ADID sentinel checks pass serially and in parallel. Neither is a full
482-mutation measurement or a snapshot refresh.

### Reopen a numeric boundary when a production history distinguishes it

`TONE_generate`'s original elapsed sum passes through `fstps`/`flds` before
the duration comparison. The reconstruction's float-only expression keeps
excess x87 precision there. A constructor-created tone with a finite positive
duration of 2097152.5 ms exposes the difference: 512 blocks of 32767 samples,
then 514 samples, produce an elapsed value of 2097152.25 ms. One more sample
rounds the reference clock to the duration and expires it; the reconstruction
compares the unrounded sum and expires one sample later. The fixture advances
all state through positive, representable generation counts. It establishes
the tone component history, not a negotiated modem call.

Three complete-TU controls compare the current expression, an explicit float
cast alone, and the unsuffixed double literal `n * 0.125 + t->elapsed` assigned
to the existing float local. The cast-only object is raw-identical to the
baseline. The double expression introduces `float_truncate:SF` at initial
RTL expansion and recovers the reference's narrowing before the comparison.
This is an expression-type boundary, not a reload-allocation workaround.

The baseline fails three of 1032 checks; the correction passes all 1032,
including the following sample's output and clock. Three focused period
fixtures pass. Across 18 emitted bodies, five canonical bystanders, bindings,
data and relocation consumers stay equal. The one exact name is preserved;
the target's size gap remains 13 bytes. Earlier return-width, guard and phase
controls remain closed: this new valid-history failure justifies the numeric
correction, without claiming the original return type or complete body.

### Negotiate the host boundary before interpreting a planted status

The V.8 fixture now runs independent original and reconstructed caller/answerer
calls through registered wrapper operations at 9600 Hz. The peer's preceding
audio block drives each receiver; no status or protocol state is planted.
Both wrappers negotiate V.34 at frame 125. Public `V8Create` peers whose
menus offer V.32 without V.34 then exercise actual V.32 selection in both
wrapper roles. Each history continues through the natural idle expiry.

The three histories pass 53332 checks, including complete normalised handshake
and wrapper state, audio, menus, host publication and 12 callback observations.
Read-only observers forward get/set calls unchanged. The host result block
starts with an earlier result, so replacing it before the handoff callback is
an observed write. A fixed observer canary corrupts only the recorded snapshot
and fails four checks while leaving protocol state unchanged. Three focused
period fixtures pass. No source divergence is demonstrated by these histories.
They establish component negotiation and expiry; public modem replacement
scheduling and quick-connect remain separate. A normally configured peer
offering neither V.34 nor V.32 is the next discriminator for the unsupported
handoff path.

### Classify extra imports by their callers and definitions

An undefined-symbol difference alone did not identify six added modem calls.
The 300-object census finds seven selected-math relocations against two in the
reference. `pow` has the same two `GetGain` callers on both sides; the reference
also defines its linked library implementation, which the reconstructed empty
`pow.S` deliberately leaves out. `sin`/`cos` in `beepgen_sample` and `fmodf` in
`MTK_phasor` replace reference x87 operations at existing profile/header
boundaries. Neither observation justifies replacing a call by handwritten
machine operations or changing a global profile.

`log10` belongs to an added offline table generator. `sqrt` is a compiler
fallback after `fsqrt` in another added generator. Four reconstruction-only
generators, including division-table and power-coefficient generation, are
used only by their respective differential fixtures. Their calculations now
live as static helpers in those fixtures; they no longer add exports or
library imports to the production object. The coefficient derivations and
their existing sweeps remain available.

Twelve complete-TU cells reproduce unchanged, removal and final-source
controls. All 15 surviving bodies and their relocation consumers remain equal
to baseline; five original-exact names are preserved. Four focused period
fixtures pass 1373296 checks. The change removes four function exports,
336 bytes of text including alignment, 20 bytes of generator-only constants,
seven relocations and two imports. Remaining extra imports are `cos`, `fmodf`,
`pow` and `sin`. This removes apparatus from the replacement; it does not
recover a new original function or the omitted linked library implementation.

### Map defaults to stores, then compare their publication boundaries

The V90 default-store audit resolves immediate/register bit patterns,
numeric-pool loads and x87 zero/one producers to destination fields. All 340
static object-store sites have named fields and matching widths; all 338
constant values agree. The mask recurrence and answer-PCM length are two
explicitly unresolved input-dependent values in this static analysis. Ten
checks across nine assembled fixtures demonstrate offset, width, value and
store-order failures, equal/conflicting CFG joins and unsupported operations.

Original pool values that appeared absent in the reconstruction already have
immediate-store witnesses. No wrong offset, type or constant default is
demonstrated. The map does expose a separate source-order discrepancy:
the reference computes the upstream mask and issues its diagnostics before
the later defaults, whereas the reconstruction placed that block after them.
Moving that one contiguous 43-line block recovers all 319 call-relative store
boundaries without changing literals, types or scalar-store spellings.

Two full-TU controls preserve eight canonical bystanders, bindings, BSS and
nontext relocation targets. Six of nine original names remain exact. The
target changes from 3594 to 3584 bytes against 3589 in the reference; both
length gaps are five bytes. The numeric pool grows from 24 to 28 bytes through
compiler rematerialisation, while the neighbouring consumer retains its exact
values and canonical body. The focused period fixture passes 4005 checks.
This is publication-boundary recovery, without a demonstrated ordinary runtime
failure. Static CFG pair-order differences fall from 1835 to 1433 of 57630;
they remain conservative ordering evidence, not dynamic execution counts or
authority for further store permutations.

The preceding continuation passes `make phase partial-link J=4`: 392 period
fixtures, zero failures, with all structural checks passing. The exact-name
set remains 1101 of 1852, covering 125983 of 720125 original function bytes;
all 724 matched initialised objects agree. Six of 300 production objects
change and each raw-matches its independently audited control. Strict partial
comparison remains DIFFERENT. The next bounded leads are omitted narrowing
before floating decisions, a shared lower-mode V.8 negotiation, and remaining
reconstruction-only production helpers. A lower-mode fixture must observe
`V8_OK` and an actual negotiated lower-mode intersection before claiming the
wrapper's unsupported-handoff arm is reachable; a timeout is a different
result.

### Resolve section identity before interpreting disassembly

An ELF function is identified by section and offset together. A COMDAT
function can start at zero while unrelated sections also contain code at
zero. `tools/dis.py` formerly selected a numeric range without selecting its
section, mixing instructions, relocation annotations and Intel-form operands
from unrelated bodies. The invalid AGC read is archived; it supplies no source
evidence. The byte comparator uses separate section-aware code, and the
independent exact-name census is unchanged.

The reader now resolves the symbol's section, selects it in both disassembler
passes and attaches relocations only within the decoded instruction extent.
Numeric ranges default to `.text`; `--section` selects another section.
Ambiguous local names require disambiguation. Output reports decoded,
displayed and relocation counts, including empty ranges. Nine assembled ELF
controls exercise COMDAT overlap, duplicate locals, padding lookalikes,
multiple relocations and empty ranges. The old reader fails six of the first
seven controls; `make phase` now runs the corrected reader's controls.

### Screen local rounding decisions with explicit unsupported paths

`recovery_float_decision_screen.py` examines all 300 frozen objects: 1930
emitted copies include 758 non-exact original-name copies, of which 82 have
original floating comparisons. The remaining 676 have none. The supported
local grammar follows arithmetic through stack stores/reloads to comparisons
and branches, including flags-preserving scheduling between `fnstsw`, `sahf`
and the branch. Unknown operations, aliases, calls and block-entry provenance
remain explicit limits.

Thirty-nine assembled controls pass: 19 value-flow cases and 20 status-word
scheduling cases. Historical bad TONE and explicit-float-cast controls fire;
the repaired double-literal control does not. Of 215 original and 220 retained
recognised decisions, four narrowed decisions on each side correspond to the
TONE elapsed/phase pair and V92 DIL ceiling pair. Eight apparent Equalizer
deficits are directly loaded float coefficients followed by `fabs`: exact
finite-value copies rather than arithmetic narrowing opportunities. No new
source discrepancy is established in this domain.

Seven bodies remain unaligned: GenericIIR block processing, Resampler,
ConnectionEvaluator phase 4, DIL descriptor packing, DTMF, EchoCanceler and
silence progress. Counts alone do not establish missing branches. For example,
the extra retained phase-4 PDSNR comparison is cloned into mutually exclusive
fallback-counter arms. Trace operands and path ownership before changing
source. Extend cross-block provenance only with controls that distinguish
equal/conflicting joins and reused status flags.

### Finish the bounded lower-mode V.8 history

Public V.8 peers now offer shared V.23/V.21 bits without V.34/V.32, with quick
connect disabled. Both wrapper roles naturally reach `V8_OK` at frame 125 of
700, selecting menu bytes `01/70/00`. Each then reports 343 unsupported
handoff error frames without callbacks, host publication or timer changes.
No status or protocol state is planted.

The five histories pass 92436 checks, with 12 callback comparisons and 686
read-only lower-mode observations. Observer-only canaries fail two lower-mode
checks and four host-snapshot checks while leaving protocol state untouched.
Three focused period fixtures pass. This closes the proposed component
unsupported-handoff history without finding a source defect. Public modem
replacement scheduling, quick connect and actual lower-data sessions remain
separate boundaries.

### Remove measurement exports only after preserving their owners

The production census first found 23 extra external definitions across 300
objects. Ten table accessors and three FixedRC accessors are fixture-only;
their observations now live in the fixtures and read the actual original data
owners through existing test-only symbol exposure. The FixedRC state observer
copies the live pointer without defining a second `struct rc`. Its offline
bank catalog also leaves production. `gen_rc_coeffs.py` now emits the
self-contained `build/rc_coeffs.c`; the generated Gentoo object remains raw
identical, including 16 banks and 7820 coefficient bytes.

Twenty-one full-TU cells preserve all 17 surviving emitted bodies and seven
original-exact names, with no original-name gains or losses. Named data,
positioned relocation consumers, bindings, imports and NOBITS remain checked;
only the added catalog is removed from data. The two focused groups pass
seven fixtures/10200720 checks and three fixtures/260200 checks. Together the
removals save 384 text bytes including alignment, 160 catalog bytes and 30
relocations. These changes reduce whole-object differences without recovering
another original function.

The resulting census has 2684 emitted definitions and nine extra external
definitions: six weak GenericIIR helpers, two runtime selectors and the
`v34handshak_txblock` fixture probe. The 26 extra local names remain explicitly
unclassified. Absence from the reference does not authorise removal of weak
compiler emissions or runtime selectors. The remaining probe needs a
complete-owner control before separation.

### Compare the actual source dialect in both deciding builds

The [dialect audit](../period-dialect-audit-results.md) reproduces three
runtime-test objects by adding only `-std=gnu99` to the frozen canonical
profile. Six full-TU cells cover 82 emitted bodies; 12 change, while all 57
shared original names retain the same 24 exact names. Six preprocessing
controls are identical. Static V34 diagnostic-site reductions therefore need
compiler/path analysis before being called missing messages. The original
dialect and precise lowering mechanism remain unproved.

Runtime source C now uses the retained comparison profile's default dialect;
fixtures and harness retain GNU99. The compiler script prints both flag lists
and invalidates compiled objects after script edits. Ten shell-dispatch/cache
controls pass; the old script fails four. Six affected period fixtures pass
14527339 checks, and all 300 runtime source objects raw-match canonical or
independently accepted helper-removal controls. No reconstruction source was
changed for this alignment.

The integrated `make phase partial-link J=4` passes 392 period fixtures with
zero failures and all structural checks. All 300 runtime source objects
raw-match their canonical comparison objects. Seven canonical objects change
from `13f477a9`; every one raw-matches its independently audited helper-removal
control, and the other 293 remain identical. The exact-name set stays 1101 of
1852, covering 125983 of 720125 original function bytes. All 724 matched
initialised objects agree. Storage matching finds all 763 reference records
and 767 candidate records; BSS remains 2840 versus 2836 bytes. Strict partial
comparison remains DIFFERENT. The strict documentation build and audit pass;
no modern-portability or newly exact original-function result is claimed.

The next bounded reviews, in order, are:

1. `silence_progress`: resolve the original indexed threshold operands at
   function-relative `+179` and `+340` against the retained stack operand at
   `+157`. Establish their predecessor states, exact values and read timing
   before treating the two-to-one site change as a missing decision.
2. `Resampler::resample`: follow both x87 operands at original `+942` and
   `+734` against retained `+730`, including the double published at
   `this+0x0c`. Test whether the extra original site is a cloned cold path.
3. EchoCanceler's combined predicates/shared status tails, then GenericIIR's
   original inlined block normalisation versus its retained scalar callee.
   Compare values and side effects across those boundaries before expanding
   the detector or fitting compiler output.
4. Separate `v34handshak_txblock` only after a whole-owner control preserves
   V34's surviving bodies, diagnostics and data. The weak helpers, runtime
   selectors, DIL finite-domain negatives and explained phase-4 comparison
   remain closed unless new contradictory evidence appears.

### Recover predicate ownership before fitting residual instructions

The fourteenth continuation resolves the preceding silence, resampler,
EchoCanceler and V34 probe priorities on the same PR branch.

`silence_progress` now accumulates through its energy member and reads that
member after the diagnostic call, matching the original's reload. The path
without diagnostics retains the published float in x87. Both original indexed
threshold comparisons use the same writable table row; no missing threshold
or mistyped field was found. The sample-count comparison uses the native
integer against a double literal, restoring the original early constant load
and `fcompp`. Ten complete-TU controls retain all four neighbouring bodies,
data, bindings and imports; all 16 threshold bytes match the reference. The
target grows from 556 to 588 bytes toward the original 623, reducing its size
gap from 67 to 35. It remains non-exact; the TU retains three exact names of
five.

Forty-eight constructor-created silence histories cover adjacent finite
amplitudes around all three active thresholds, four debug levels, and whole
versus 799+1 sample calls. They compare state, output and transcripts. One
upper amplitude is loud in a whole call but quiet across the split: publishing
the running sum narrows it, and the completed mean equals the threshold. An
initial fixture expectation ignored this boundary and was corrected from the
publication arithmetic; reconstruction source was not changed to satisfy it.
The three focused period fixtures pass 982173 checks. No ordinary logging
callback mutation is claimed.

The remaining silence differences have distinct compiler causes. Its combined
level-range guard is already present in `.01.rtl`, before RTL combine. The
four push/pop saves first appear in `.27.flow2`, whereas the original uses
stack-offset moves. A read of the locally archived upstream GCC 3.4.2 source
finds the fast-prologue choice uses an estimated instruction-frequency cost,
not emitted byte size: four saved registers give a threshold of 60. Frame
layout can cache this choice during reload. This source inspection has not
replayed Gentoo patches or measured the actual cached cost; the discriminating
next test is an instrumented Gentoo `cc1` trace of frame layout and
`expensive_function_p`, rather than padding source to cross a guessed size
threshold.

The first integrated phase run passes all 392 differential fixtures but rejects
six stale silence mutation anchors. Updating the anchors preserves their
mutations; the `<=` sample-count mutation is correctly labelled one sample
late. A period-compiler replay catches all 36 registered mutations after
adding the newly covered first-threshold downward ULP. The new split history
catches that mutation through detector state, not a diagnostic message. The
other two previously omitted opposite-direction ULP mutations still survive
this fixture. Earlier claims that the downward threshold had no reachable
preimage are superseded by the measured 799+1 history.

EchoCanceler now owns the history-scan pointer once, computes the quiet-sample
predicate once and reuses it for adaptation, and counts quiet samples with an
unsigned counter. These operations are visible in the original. Four complete
TU cells, including the final working source, preserve the five other bodies,
allocated data, bindings and resolved relocation consumers. The first candidate reaches
the original 359-byte size from 363, with 131 bytes still differing after
relocation handling. Two of six original functions remain exact. The focused
period fixture passes 11 groups and 45905 checks; its runtime object is raw-identical to the accepted
control. This is supported partial graph recovery, without a demonstrated
runtime defect.

The original's explicit `TEST`/`SETNE` truth conversion was still absent from
that first EchoCanceler candidate. The source emits it in initial RTL. `06.cse` first reuses the count-normalised
value in the adaptation condition; `19.life` marks the original predicate
dead at its truth conversion, and `20.combine` folds that conversion back to
the comparison result. Other remaining
differences include the eight-byte versus twelve-byte local stack allocation,
operand scheduling and adaptation predicate lowering. A same-size result does
not establish that only register allocation remains. Earlier controls declined
this source boundary because it did not yield another exact function; the
current acceptance rests on independently witnessed operations and unchanged
neighbours, not size alone.

A subsequent single control expresses the original sequential `mu` and `low`
guards as nested conditionals. Initial RTL now has separate branches instead
of an integer AND, the predicate remains live after counting, and the original
truth conversion survives combine. Only EchoCanceler changes in the full-TU
control; the focused period fixture again passes 45905 checks and its runtime
object raw-matches the accepted candidate. Differing bytes fall from 131 to
55 at the original 359-byte size, with no exact-name gain or loss. The next
specific discrepancy is the invariant `e * mu` product: the original computes
it after the empty-tap guard, while the retained code computes it before.
Trace that boundary before proposing another source change.

### Identify comparison cloning in the compiler dumps

Resampler's original comparison at `+942` belongs to the cold path with no
new input credit; `+734` consumes the extended subtract-loop residual after
its double publication. The retained shared comparison also consumes the
extended value. Four complete-TU controls cross countdown and phase-loop
boundaries, preserving 16 of 25 exact names, all 24 neighbouring bodies,
data, bindings and positioned relocation consumers.

Each control has three comparisons through `33.sched2`. Three variants gain
a fourth at `34.stack`, where GCC converts the x87 stack representation; the
baseline keeps three. Final disassembly agrees. This demonstrates how an
extra static comparison can arise without another source predicate or a
narrowing difference. No source variant is adopted from this domain. The
focused fixture passes 16 groups and 69521 checks. GCC 3.4.2 accepts `-da`
here but rejects `-fdump-tree-all`; rejected runs are archived as invalid,
not interpreted as compiler evidence.

### Remove the final identified production fixture probe

`t_v34hstbl2` now calls complete `v34handshak` on both sides. Its existing
controlled dispatch state already selected the desired TX block; a production
wrapper was unnecessary once the reconstruction was complete.

Three whole-owner controls remove `v34handshak_txblock` and preserve all 53
surviving emitted bodies, positioned calls, diagnostics, data and jump tables.
Ten raw bodies change only because 54 local call-displacement fields rebase.
The removal saves 64 text bytes, including one alignment byte, and 100 raw ELF
bytes. Five audit controls exercise changed data, jump targets, local calls,
diagnostics and the unchanged case. Fifteen focused period fixtures pass
158111 checks. The production census drops to eight extra external definitions:
six weak GenericIIR helpers and two runtime selectors. Neither category is
authorised for deletion merely because its names are absent from the blob.

### Compare persistent double state when float outputs agree

GenericIIR's old block fixtures used a leading denominator of one and checked
only float outputs. The retained block delegated every sample to the scalar
method, whereas the original block normalises with reciprocal then multiply.
An ordinary constructor-created history with denominator
`1.4142135623730951`, numerator `1.0000000000014524` (exactly
`1 + 6541 * 2^-52`), one numerator and denominator coefficient, block size
four and unit input exposes a one-ULP difference in the stored double
accumulator and output history. Float output agrees. The fixed regression
fails two of four checks before recovery; no internal state is planted.
The earlier suspicion about the final member loop index was disproved by the
original's final overwrite and is not a reason for a source change.

Writing reciprocal then multiply alone is insufficient: the period compiler's
`20.combine` contracts it back to division. A normalisation factor initialised
to one, conditionally set to the reciprocal, and then multiplied preserves
the original operation. The block now carries its own witnessed processing
loop. Inline compaction also recovers the absence of helper calls in both
original scalar and block bodies. Thirteen complete-TU controls cover 234
emitted-body verdicts, preserving all 16 other bodies and nine exact original
names of twelve, together with data, bindings and positioned relocations.
The scalar size moves from 483 to 450 toward the original 398; the block moves
from 96 to 548 against the original 584. These are partial graph recoveries
plus a demonstrated numerical-state fix, not exact functions.

The regression's original stored value is `0x3fe6a09e667f5fef`, compared with
`0x3fe6a09e667f5fee` before recovery. The accepted source reproduces the former.
The three focused fixtures pass 19 groups and 337374 checks, including
normalised object state and both histories. This is why output-only tests
cannot establish recurrence fidelity when double state feeds later samples.


The final canonical comparison retains exactly the same 1101 exact names of
1852, covering 125983 of 720125 original function bytes. Four of 300 objects
change from `239b234c0`; each raw-matches its independently audited control,
and the other 296 remain identical. All 300 runtime source objects raw-match
the canonical comparison objects. All 724 matched initialised objects agree;
all 763 reference storage records match, with four additional candidate
records. BSS remains 2840 versus 2836 bytes. Strict partial-object comparison
remains DIFFERENT.

The final `make phase J=4` passes 392 period fixtures with zero failures and
all structural checks, including 10042 mutation anchors across 285 suites.
The strict documentation build and audit pass. No modern-portability result
is claimed.

The next bounded priorities are:

1. Trace EchoCanceler's invariant product across the empty-tap guard. Preserve
   the recovered separate predicates and inspect the first divergent compiler
   stage before changing source.
2. Extend GenericIIR state/history checks across mixed scalar/block calls and
   compaction boundaries with non-unit denominators. Then compare the remaining
   loop/publication boundaries; matching float output alone is insufficient.
3. Measure the actual Gentoo silence prologue cost/cache decision and frontend
   range folding. Keep the two surviving threshold mutations visible as
   coverage limits rather than claiming they have no reachable histories.
4. Keep the Resampler comparison-count domain closed without a contradictory
   operand/path. Apply its stack-conversion lesson to other unaligned floating
   comparisons before labelling them missing predicates.

The next campaign follows the
[Ghidra-assisted fidelity investigation](ghidra-assisted.md), including input
provenance, calibration controls and a complete discrepancy ledger.

## Ghidra-assisted boundary recovery: 17 October 2026

PR #293 starts from the merged #291 tree. The first connected investigation
compares explicitly selected reference and canonical programs in Ghidra 12.2
DEV. The [campaign record](ghidra-assisted.md#first-connected-investigation-17-october-2026)
records inputs, calibration and limits. Ghidra nominated boundaries;
independent instructions and complete-TU controls decide which source to retain.

### Calibrate signatures and the claimed P-code level

Four synthetic x87 controls demonstrate both a useful correction and a hard
limit. Inferred signatures omit a final add/subtract/divide and the third
argument after a float spill/reload. Explicit three-float `__cdecl` signatures
restore the arithmetic. However, narrowed and unrounded subtraction still
have identical high-level expressions and float-width P-code despite native
results `c0920eaa` and `c0920eab`. Do not infer precision identity from either
representation, even after typing the arguments correctly.

The MCP endpoint's `basic` mode is labelled raw but returns HighFunction
operations (`MULTIEQUAL`, `PTRADD`, `CAST`). Record the actual representation;
calling it raw would confer a precision guarantee the observation does not
have. Empty disassembly responses also failed the four known nonempty
controls. Fall back to the independent disassembler and keep the invalid
outputs visible. The committed [calibration fixture](../../test/experiment/ghidra/README.md)
passes 5/5 native checks and can be rerun on a different version.

### Let source loop ownership produce a guarded invariant

EchoCanceler's adaptation product belongs after the nonempty-tap guard in
the reference. The previous reconstruction computed it before the loop.
Initial RTL already contains that order: blaming a later compiler pass would
have sent the investigation in the wrong direction. Declaring the product
inside the adaptation loop lets `08.gcse` hoist it into a guarded preheader,
matching the original boundary without a barrier or extra source guard.

A second discriminator is the original x87 producer copy: `fld` duplicates
still-live `mu`, then `fmulp` consumes `e`. `mu * e` recovers this ownership;
`e * mu` chooses a different destructive operand despite equal arithmetic.
The six-cell domain includes the explicit-empty-guard cross and a final
consumer operand reversal. The latter produces raw-identical objects, so
that consumer spelling domain is closed.

The retained body remains 359 bytes, matching the original size; differing
bytes fall from 55 to 44. All five neighbouring bodies, data, bindings and
resolved relocation consumers are unchanged. The final focused period run
passes one fixture, eleven groups and 45,905 checks. This is a partial graph
recovery, with no new functional defect claimed. Next inspect the first
dot-product load/operand schedule through combine/reload; account separately
for the original twelve-byte versus retained eight-byte stack allocation.

### Recover compaction and publication before register allocation

The GenericIIR reference has signed predecrement history guards, descending
compaction pointers with a `n-2` to `-1` counter, input-history publication
before zeroing the accumulator, pointer/countdown block iteration, and block
output publication before input compaction. These are instruction-backed
source boundaries. In particular, moving the input-history store before
zeroing the accumulator eliminates the old alias-forced accumulator reload;
a matching filter equation had hidden the publication difference.

| Original entry | Previous bytes | Retained bytes | Reference bytes | Remaining grade |
|---|---:|---:|---:|---|
| Scalar `process(float)` | 450 | 398 | 398 | BYTES, 63 differing bytes |
| Block `process(const float *, float *, unsigned)` | 548 | 572 | 584 | SIZE, 12-byte length gap |

The twelve complete-TU controls cover 216 emitted-body verdicts. Only the
two processing bodies and two compaction helpers change; the other fourteen
bodies, all nine exact original names of twelve, data, bindings and noncode
relocations remain unchanged. The compaction helpers retain their weak
exports. Matching scalar length and instruction count does not make the
scalar function byte-exact.

Six added constructor-created history groups cover non-unit denominators,
unequal taps, mixed scalar/block calls, in-place blocks, reset, zero-length
calls and tight compaction headroom. They compare non-pointer state and both
complete double histories. All three focused fixtures pass: 32 groups and
422,747 checks. Restoring block division instead of reciprocal multiplication
fails 30 new checks across five groups; the scalar-only group is a negative
control. No internal state is planted and no public-call reachability or new
original-code bug is claimed.

Stop these source domains here. The next scalar discriminator is the first
feedforward register/base/index choice and input-compaction LEA schedule in
initial RTL, allocation and scheduling dumps. The block needs a live-value
and spill account of the original `0x34` versus retained `0x2c` frame. A frame
size difference alone does not identify its cause.

## Compiler-stage follow-up: exact silence guards and echo operands

This PR #293 continuation uses Gentoo GCC 3.4.2-r2 for every reconstruction
compile, instruction comparison, tree dump and RTL dump. The debugger runs
`cc1` extracted from that image; the selected period assembler must reproduce
the complete original experiment object byte for byte. Period differential
execution is an additional check after those code-generation measurements.

### Preserve guard boundaries before explaining the prologue

`silence_progress` is now byte-exact, 623 original bytes. Its two early-return
checks were previously written as one `lvl > 3 || lvl == 0` expression.
The original compares the low word with three and separately tests the widened
value against zero, with two branches to the epilogue. Two `if` statements
recover that graph; no field/local type or compiler flags change.

A width cross initially suggested a wider local might preserve the two
comparison widths. That combined-expression control instead materialised
boolean results and remained nonexact. Crossing statement boundaries then
showed both the wide-local and original-short-local split forms are EXACT
and produce identical complete objects. Retain the simpler short local;
this is evidence for the guard boundary, not a uniquely recovered declaration.
The six distinct variants cover 30 complete-body verdicts. Only the target
changes; four neighbouring bodies, all data, bindings and relocation consumers
are unchanged. A corrupted threshold byte is rejected by the noncode audit.

The installed compiler observation closes the old unmeasured cost claim:

| Observation | Combined guard | Separate returns |
|---|---:|---:|
| Saved registers | 4 | 4 |
| Cost parameter | 60 | 60 |
| Estimated entry frequency | 3438 | 6875 |
| Weighted decision threshold | 206280 | 412500 |
| Observed weighted count | 206629 at early exit | 367492, full traversal |
| `expensive_function_p` | true | false |
| Save registers with moves | false | true |

Each case has fifteen frame-layout observations but only one cost call. The
first call happens during reload's elimination-offset calculation; its choice
is cached for all subsequent layouts, including prologue and epilogue emission.
That cache is keyed by saved-register count; Queue below demonstrates a second
decision when reload changes that count.
The combined case exits as soon as its count exceeds the threshold, so 206629
is not its whole-function cost. Local space and outgoing space remain 16 and
12 bytes. No padding or invented local is needed to recover the move saves.

[Guard controls](../../tools/recovery_silence_guards.py) preserve the source
cross. The reusable [observer](../../tools/gcc3_prologue_cost_observe.py) records
actual compiler hashes and commands, checks the cost-hook instruction bytes,
and requires raw complete-object equality. Baseline and exact controls retain
that equality; a successfully compiled absent target is rejected as empty.
Early invalid permission/symbol observations remain excluded in the artifacts.
Allocator-address differences in `-dP` comments are not emitted-code changes;
the period-assembled object is the identity control.

This transfers as a triage method: inspect original/source guard boundaries
before trying to steer a prologue by function size. A guard split changes
predicted frequencies and can affect register saves far from the expression.
It does not authorise splitting unrelated guards solely to obtain a score.

### Follow combine's chosen memory operand

EchoCanceler's dot product now uses `hist[pos+j] * coef[j]`. The original
loads the coefficient, calculates the history address and multiplies from
history memory. `20.combine` had folded coefficient memory for the previous
operand order. The reversed source operands restore the original instruction
ownership, reducing differing bytes from 44 to 31 at the same 359-byte size.
Explicit coefficient-capture controls are raw-identical to the old source;
that source family is closed. All five neighbours, data and bindings survive.

The remaining adaptation history load first appears in `25.greg`, which
materialises history in a fresh x87 register rather than copying the live
product. The last stack-conversion pass only maps that choice onto the x87
stack. Both original and candidate have nine local accesses, all at offsets
zero and four; the original's extra four frame bytes are unreferenced by final
local operands. That is not evidence for adding another source local.

### Stop IIR source retries at the measured allocation boundary

The unchanged complete IIR TU raw-reproduces the retained object. Scalar
numerator-count pseudo170 spills during `25.greg`; UID392 chooses BX as the
reload scratch. UIDs41/45 change DX to DI and AX to BP in `30.rnreg`, a
different mechanism. The input-compaction source LEA clobbers the history-base
BX, so `33.sched2` records its anti-dependency on destination LEA UID285.
Reordering source declarations is not justified by the final LEA order.

The original block's two extra stack words are also distinct: a live
destination-pointer spill, and an input-length store at `+0x1f6` that is never
read. Do not describe both as live values or infer original local declarations
from them. Thirteen stage invariants and a deleted-dependency negative control
validate the diagnosis. All four target COMDAT symbols have one definition
across 300 canonical objects. No additional IIR source change is retained.

### Count compiler nodes before inferring a Queue source discrepancy

The next installed-compiler observation explains the retained
`Queue<float>::write(float)` prologue without identifying a source correction.
The original is 85 bytes, the retained body 71; both decode to 31 instructions.
The processing instructions agree, while register saves and restores differ.

Nineteen frame layouts include two cost decisions. Reload increases the saved
register count from one to two, invalidating the first cached choice:

| Decision | Saved registers | Cost parameter | Entry frequency | Weighted threshold | Count at exit |
|---|---:|---:|---:|---:|---:|
| First | 1 | 0 | 10000 | 0 | 10000 |
| Recomputed | 2 | 20 | 10000 | 200000 | 201683 |

Both decisions select push/pop. The second counts 22 active RTL nodes.
The result move, UID85, leaves the count at 191683. UID88, a non-emitted
`(use (reg/i:SI 0 ax [<result>]))`, adds 10000 and crosses the threshold.
The actual `active_insn_p` filters USE/CLOBBER only after reload completes;
both observed decisions occur with `reload_completed == 0`. Counting final
machine instructions therefore cannot reproduce this cost decision.

The [observer](../../tools/gcc3_prologue_cost_observe.py) now records the
current node's UID and checks every frequency sum. Additional instruction-byte
guards establish that the sampled node is passed to `active_insn_p` before
counting and advanced only after the sample. This is not an off-by-one
association with the next RTL node. The observed complete TU raw-matches the
canonical object, with 25 of 30 compared bodies exact.

All three original out-of-line callers discard the return value: V92Modulator
reset at `.text:0x150ed`, C1 at `0x153bf` and C2 at `0x1569f`. None supplies a
return-width witness. Ordinary C++ mangling omits this return type; the callee's
zero/minus-one result does not distinguish candidate signed widths. Do not
change the return type to remove the counted node. The `isfull`, `ifelse`,
`opencoded`, `spacelocal` and `wlocal` source domains remain closed.

Reopen Queue only with a new original guard/operand boundary or independent
compiler-profile evidence. A historical exact helper-query control, if its
complete preprocessed input is available, could discriminate node/frequency
provenance; it must retain the known constructor collateral review. A target
cost crossing alone does not justify adopting that control.

### Distinguish an x87 reload tie from a missing compiler operation

Echo's remaining adaptation multiply, UID187, uses constraints `=f`, `%0`,
`fm`: its output is tied to the first input and the multiply is commutative.
Installed `find_reloads` examines both orientations. Each needs one reload
with rejection cost two, giving `6 * 1 + 2 = 8`. The selection accepts only a
strict improvement, so the 8:8 tie retains the first orientation and loads
history rather than copying the existing product.

All three observations of UID187 agree: two counting passes and one replacement
pass. Across nine recorded choices and fourteen orientation observations,
UID56 and UID189 provide controls that actually switch from cost fourteen to
zero. This is a measured tie, not evidence that GCC cannot copy an x87 value.
The previously raw-identical consumer-operand spelling domain stays closed.

Frame observations identify two four-byte SI spill slots, for quiet pseudo69
and index pseudo76, plus two zero-size BLK alignment requests. All nineteen
layouts use a 32-bit preferred boundary, no outgoing space and zero padding.
The final stack offset is 4 return bytes + 16 saved-register bytes + 8 local
bytes. One cached prologue decision selects push saves, agreeing with the
original's save form. It does not explain the original's twelve-byte local
allocation. Four unreferenced original bytes establish neither a third source
local nor a particular original alignment setting.

The [reload observer](../../tools/gcc3_echo_reload_observe.py) and
[stack observer](../../tools/gcc3_uref_stack_observe.py) preserve raw complete
objects. Changed-cost and changed-padding controls are rejected; absent targets
compile successfully but fail observation. The baseline and every compiler
dump use the recovered Gentoo compiler with `DSPLIB_REPRODUCE_BUGS`.
No reconstruction source changes or additional exact gains follow from this
batch. See the [campaign commands](ghidra-assisted.md#queue-and-echo-observation-validation).

Move next to an independently witnessed source-fidelity discrepancy rather
than forcing Queue's prologue or Echo/IIR spills. Reopen allocation experiments
only when a new lifetime, operand or profile witness distinguishes them.

## Recover initializer visibility before changing inline policy

`preinitdigital` originally embeds two `preinitV34` bodies followed by three
memset calls. The reconstruction instead made two external helper calls.
Historical deduplication in `7fe8c8b24` (F188) introduced that boundary while
reconciling duplicate implementations. It preserved behavior but was not
evidence for the original source's inline visibility.

Move `preinitdigital` beside the existing helper in `v34shell.c`; leave one
canonical helper and preserve both strong exports. Ordinary retained O3 then
embeds both initializers without an attribute or compiler-option change.
This restores the original call graph and reduces the length gap from 233
bytes (300 versus 533 original) to twenty bytes (553). Acquiring the TX role
view only after the clears removes a saved ESI and reduces it to eight bytes
(541). This is a SIZE gap, not eight differing bytes: the candidate has 137
decoded instructions versus 131 original.

Across the complete shell/digital pair, only `preinitdigital` changes. Twelve
neighbouring bodies, all three existing exact names, data, 29 global
definitions and ten external imports survive. `v34digital.c` remains a normal
build input containing layout assertions. Original TU extent remains an
inference because the span has no anchoring local symbol; recovered visibility
does not uniquely establish the author's file layout.

The [three-cell replay](../../tools/recovery_preinit_owner.py) checks baseline,
same-owner and late-role-owner sources, plus a raw-identical final formatting
control. Its frozen revision is reachable on the PR branch, rather than an
unpublished delegated commit. Focused period coverage passes three fixtures,
27 groups and 6186579 checks; full `make phase` passes 392 fixtures. Both moved
callback-pair mutation anchors are exercised with Gentoo-compiled objects and
fail as intended. The mutation suite follows the definition into its new file.

Next inspect the original group-owner acquisitions: LEAs for scrambler
`obj+0x2a54` and descrambler `obj+0xe74`, followed by clears through those
views. Current direct field stores produce extra zero materializations and
different store order. The TX role view also still materializes `obj+0x1be0`
where the original publishes directly to `obj+0x2a28`. Those are independently
witnessed ownership/publication questions, not a reason to tune registers.

## Review exact losses against source fidelity

`V34EqualizerCenterAdapt` exposes a different tradeoff. The original has two
circular-history spans, with backedges `.text:0x72be5 -> 0x72b84` and
`0x72c51 -> 0x72bf0`. The retained reconstruction has one eight-tap loop with
a wrap test at every tap. Ghidra's reference graph corroborates both backedges,
but its basic-mode output remains HighFunction P-code; native instructions
are the operand authority.

The [nine-cell replay](../../tools/recovery_v34_center_spans.py) crosses split
spans, continuous coefficient pointers and per-lane history rereads, then tests
one original imaginary-history-base acquisition. Its 234 complete-body verdicts
include no exact target. A 317-byte candidate, against 347 original and 183
retained bytes, restores the two loops and coefficient ownership while changing
only one neighbour. A 336-byte control narrows the length gap further but has
broader nonexact collateral. Size alone does not select the source candidate.

The 317-byte candidate passes all 392 period fixtures. Existing lifecycle
coverage visits all eighty cursors and checks untouched fractional halves;
a wrong-wrap control fails 2372 checks. Twenty-four neighbouring bodies,
data, bindings and relocation consumers remain unchanged. However,
`V34EchoPreFilter` loses its 139-byte exact match through four register bytes.
Its patterns agree through `27.flow2` and first differ at `28.peephole2`;
the bodies remain equivalent under the register-renaming comparison.

Installed-compiler observation then establishes the cause. Total scratch
searches increase from 78 to 81; CenterAdapt consumes two versus five.
PreFilter's sole search, UID13, has identical range, mode, constraints,
allocation order and register eligibility. Its incoming persistent search
cursor changes from two to one, selecting CX versus DX. Both complete observed
objects raw-match their controls. Peephole2 changes zeroing UIDs187/188;
`30.rnreg` subsequently changes shift-carrier UIDs138/161. This accounts for
all four bytes without a source or algorithm change in the neighbour.

The exact-membership check correctly reports 1102 -> 1101 and identifies the
lost symbol. That result is a regression alarm, not proof that the recovered
loop structure is worse. Treating it as an automatic veto can freeze
compensating source choices and reproduce the local-minimum problem described
in [experiment design](experiment-design.md). Keep differential failures hard;
review explained byte regressions alongside source fidelity, full-TU changes
and independent compiler evidence. Do not silently reset the historical floor
or hide the lost name behind a net count.

After that review, retain the 317-byte source recovery and deliberately rebase
the current exact-membership floor to 1101/1852, 126467 original bytes. The
previous 1102-name floor remains in Git history; the one lost name is
`V34EchoPreFilter`, with no gained exact names. This is an explained byte
regression accepted for stronger original loop/operand fidelity, not an exact
gain or a claim that the original source has been uniquely recovered.

The nine-cell source domain is closed to further spelling retries. Remaining
questions concern the original imaginary-history base and arithmetic operand
ownership. Future losses still fail the ratchet and need their own review;
this case is not an automatic exception for register differences.

### Queue recovery changes a later constructor's scratch registers

The GCC model follow-up supplies another measured example in
`V92Modulator.cpp`. Scalar `Queue<float>::write(float)` calling the existing
`isFull()` predicate reproduces the complete native 85-byte body, including
its frame stores; the expanded arithmetic test produces 71 bytes. The
predicate's arithmetic already has independent native evidence in
`V92Modulator::progress`. The helper-use candidate is retained as recovery of
that source boundary, without claiming it is the only possible spelling.

The mechanism crosses two compiler decisions. The helper changes branch
prediction, reducing the weighted instruction count from 201683 to 199984
across the 200000 prologue threshold. The recovered frame then removes one
dead-pop scratch search: the TU has 52 searches instead of 53. The persistent
cursor entering the later C2 constructor changes from 0 to 2. That constructor
loses its 734-byte EXACT match through seven register bytes in three constant materialisations
and their stores for `Scrambler(5, 23, 99)`. Values, argument slots and the call
destination remain the same. Its instruction patterns agree through
`27.flow2` and first differ in `28.peephole2`.

`progress` also first diverges in peephole2. Its final differences are EBP/EDI
temporary choices and a block-size load moved across a division, without
crossing a call or store. The other 27 of 30 shared TU bodies, exports and
allocated data remain unchanged. Fresh dependency preprocessing of all 299
C/C++ production TUs finds one Queue-header consumer; the remaining source is
assembly. Four focused period binaries pass 804321 checks in 27 groups on
both baseline and candidate. Each linked candidate scalar write is itself
EXACT, ruling out a baseline weak-template copy masking the edit. These are
component fixtures, including synthetic V.92 transition rows, not modem
end-to-end coverage.

Retain the queue recovery and record the constructor's named exact loss;
do not restore the expanded predicate merely to recover its successor's
registers. Further constructor work should inspect earlier source structure
and the full TU search history before changing the constructor's own code.
An explained cursor effect does not prove every earlier function is correct,
nor license dummy scratch consumers or compiler-state knobs. The
[model study](gcc3-model-nonexact-study.md) records the experiment domain and
the accompanying FloatFIR layout tradeoff. Detailed traces and linked-fixture
checks are in `build/gcc3-v92-recovery/`.

The fresh 300-object comparison covers 1852 reference functions. Rebase the
exact-membership floor explicitly from 1109 to 1110 names: gain
`FloatFIR::reset()` (61 bytes) and `Queue<float>::write(float)` (85 bytes),
and lose the 734-byte `V92Modulator` C2 constructor. Exact-covered reference
bytes therefore decrease from 127904 to 127316 despite the net function gain.
REGALLOC changes from 32 to 31 as reset graduates; SIZE falls from 633 to 632
and BYTES rises from 78 to 79. No other exact membership changes. The previous
floor remains in Git history, and future unexpected losses remain alarms.
The retained source passes `make phase`: 393 period fixtures, zero failures,
with structural checks green (`build/gcc3-queue-adopt/phase.log`).

## V92 encoder case ownership and the inline boundary

An initial six-cell visibility cross moved `inverseMap` below `process` under
retained O3, O3 without automatic inlining and O2. Definition order was
raw-inert at every profile. Disabling automatic inlining restored the helper
call but left a 215-byte process against 240 original bytes. O2 also changed
the nonexact table builder without recovering it. Those controls motivate the
case-ownership cross below; they do not justify a per-file compiler exception.

`V92ConvolutionEncoder::process` has a separate source-ownership discrepancy
from its retained automatic-inline boundary. Original instructions at
`.text:0x54f75` store EDI to `state` and return ESI for an unnamed mode,
without reading either trellis table. Each valid case loads `nextState`,
publishes `state`, then loads `output`. The retained reconstruction instead
leaves only `i` uninitialised and performs the table accesses after its switch.
Gentoo GCC 3.4.2-r2 retains those accesses. The source comment claiming that
GCC dropped them was incorrect and is now corrected; the unnamed-mode
reconstruction gap remains open.

The [eight-cell replay](../../tools/recovery_v92_case_owners.py) freezes source
and headers at `c118236196b1e6a6bd4f7aeafd6458cea1ad544c`. It crosses the retained
source and three case-owned source forms with retained O3 and the diagnostic
`-fno-inline-functions` control. The first six cells test a common-result/tail
form and direct case returns. Their differing publication orders motivate the
last two cells: case-owned results with a default-only state store and a common
return. All alternatives preserve state-before-output order in their C++
statements. No attributes, helper changes or per-file flags are retained.

| Source form | Retained O3 process bytes | No automatic inlining |
| --- | ---: | ---: |
| Retained common table lookup | 503 | 215 |
| Case results, common state/result tail | 458 | **240, EXACT** |
| Direct return from each valid case | 433 | 172 |
| Default-only state store, common result return | 442 | 172 |

The original process is 240 bytes. The common-tail control reproduces its
complete bytes and relocation identity under the diagnostic profile, including
the unnamed-mode path and original publication order. It raises that cell from
six to seven exact bodies out of eight. This supports the case-owned
calculation and shared-result boundary; it does not establish the original
compiler profile or justify changing one TU's flags.

Under retained O3, `inverseMap` still inlines. Its estimated cost remains 98
instructions; the common-tail caller grows from 77 to 165 after inlining.
O3 also removes its early duplicate state stores and publishes state after
loading output. The default-only-store control preserves the original valid
publication order under O3, but its no-inline unnamed-mode path coalesces the
two uninitialised values into one register, unlike the original. A closer SIZE
or one diagnostic exact body does not select either alternative for production.

The unchanged source reproduces the archived complete Gentoo object byte for
byte. Across 64 body verdicts, each non-baseline cell changes only `process`;
all seven neighbouring bodies, static data, noncode relocations, strong exports
and bindings remain unchanged. All six baseline exact names survive in every
cell. The no-inline controls restore the original `inverseMap` call; retained
O3 cells keep its three static-table relocation consumers instead. The 440-byte
standalone helper remains exact throughout. A changed static-data byte is
detected by the audit's negative control.

Reproduce with an archived canonical object tree and its `.build-config`:

```sh
mkdir -p build/conv-owner-next293
python3 tools/recovery_v92_case_owners.py --domain case-owned-tables \
    --baseline-dir build/archived-conv-baseline
python3 tools/recovery_v92_case_owners.py --audit
python3 tools/recovery_v92_case_owners.py --comment-control
```

The run records full compiler commands, header/source hashes, identities,
compiler dumps and body/relocation verdicts under
`build/conv-owner-next293/reproduction`. The comment-only retained edit produces
a raw-identical complete object. Existing `t_v92convmapper` coverage drives
reset-built objects through modes 0, 1 and 2 and compares state after each
call. No unnamed-mode runtime fixture is added: comparing indeterminate values
would not establish reachable input history. No functional source alternative
is retained, and no new period runtime pass is claimed. Reopen the source/profile
interaction with common-profile or additional original ownership evidence,
rather than another spelling of the standalone exact helper.

### Separate frontend cost from native helper and table ownership

The [six-cell provenance replay](../../tools/recovery_v92_inline_provenance.py)
freezes the complete encoder TU at `4a6519cba`. It crosses four scalar residue
locals with one four-element array under retained O3 and diagnostic
no-auto-inline. Both representations make four explicit assignments; no loop
is introduced. Two additional scalar controls use no-unit-at-a-time and
issue #22's O2/automatic-inline/web profile. The array is a hypothesis, not
an independently recovered declaration.

The installed Gentoo `cc1plus` contains a readonly `lang_independent_params`
symbol at `0x0838ef00`: thirty-five twelve-byte name/value/help records and
a null sentinel. The replay decodes the pointers and proves defaults of 100
for `max-inline-insns-auto` and 500 for `max-inline-insns-single`. Its compiler
hash is `b778f44bd1a5e8184ca34b11c9701d0a8d67d15406282c1a204237d03c82082d`.
These are measured installed defaults, not thresholds inferred from scores.
The actual commands contain no override of these parameters.

| Control | Helper estimated instructions | Helper bytes | Process bytes | Exact losses |
| --- | ---: | ---: | ---: | --- |
| Scalar, retained O3 | 98 | 440 EXACT | 503 | none |
| Scalar, no automatic inline | 98 | 440 EXACT | 215 | none |
| Array, retained O3 | 106 | 456 | 215 | `inverseMap` |
| Array, no automatic inline | 106 | 456 | 215 | `inverseMap` |
| Scalar, no unit-at-a-time | no unit callgraph dump | 440 EXACT | 503 | none |
| Scalar, O2 + automatic inline + web | 98 | 440 EXACT | 647 | none |

The array crosses the inline threshold and restores the call, but it also
introduces a 28-byte local stack allocation where the exact scalar helper has
none. Three residues acquire stack stores at ESP+0/+4/+8; the fourth remains
in a register. The helper's code grows sixteen bytes, a separate quantity from
its frame growth. This is a source/cost interaction with native consequences,
not an invisible frontend-only spelling change. The O2/inline/web control
keeps the helper inlined and changes the nonexact builder's instructions even
though its length remains 1456 bytes against 1558 original bytes. No cell
gains an exact name; both array cells lose the helper.

The no-unit control preserves all eight function bodies and positions but
changes table layout:

| Global `.data` object | Bytes | Original offset | Retained TU offset | No-unit TU offset |
| --- | ---: | ---: | ---: | ---: |
| `cosetMapping4D` | 256 | `0x6ac0` | `0x40` | `0x0` |
| `subsetLabelTable` | 64 | `0x6bc0` | `0x0` | `0x100` |

All named contents, sizes and strong global bindings match the reference.
No-unit restores the original relative order, with every raw code relocation
offset, type, target and inline addend unchanged. Table consumers use named
symbols with zero addends. This is independent data-layout evidence, but does
not uniquely identify an option: source definition order can compensate for
compiler emission order.

The original `V92ConvEncoder.cpp` FILE record has no local text anchors.
Strong global helper binding does not associate its body with that FILE or
establish an inline/noinline declaration. Neither a TU split nor an attribute
is licensed by the present evidence. Keep the native layout witness separate
from that ownership uncertainty.

The audit covers six cells, forty-eight body verdicts, five unit callgraph
dumps, named data, bindings and relocation consumers. It rejects missing cost
evidence and a changed default parameter. No production source/profile change
or new runtime coverage is claimed.

```sh
python3 tools/recovery_v92_inline_provenance.py --domain scalar-array-cost \
  --baseline-dir build/continuation293-baseline
python3 tools/recovery_v92_inline_provenance.py --parameter-proof
python3 tools/recovery_v92_inline_provenance.py --audit
```

Close nearby residue/cost spellings. The next discriminator needs independent
table-definition or TU ownership evidence crossed with common-profile
controls, including the builder and process call boundary. Relative table
order alone cannot select a source or flag change.

### Retain native table layout and a witnessed case boundary

The [four-cell table cross](../../tools/recovery_v92_table_ownership.py)
freezes `8e216110a` and crosses the two definition orders with retained
unit-at-a-time and diagnostic no-unit. Both compensation pairs are complete
raw-identical objects. Coset-first/no-unit equals subset-first/unit; the
opposite pair reproduces the baseline. All eight bodies and positions,
six exact names, bindings and raw relocation records survive. This closes
the order/profile domain without uniquely recovering either original choice.

Retain subset-first definitions under unchanged flags because the resulting
320-byte data block exactly matches reference `.data:0x6ac0..0x6c00`: the
256-byte coset table immediately followed by the 64-byte subset table. The
old declaration-order comment was false under the deciding compiler. Named
contents and widths stay unchanged. The LOCAL-data ownership detector finds
no owner for any of the eight functions; the three helper table references
target GLOBAL records, which cannot establish their defining FILE. This is
not evidence of separate TUs or an inline attribute.

The [adoption cross](../../tools/recovery_v92_boundary_adoption.py) then
combines that data refinement with the already-tested case-result/common-tail
source. It adds no new case spelling or compiler option. The reference's
unnamed-mode arm stores an indeterminate state and returns an indeterminate
result without reading either trellis table. The prior common lookup after
the switch incorrectly read both tables on that arm. Keep calculations within
named cases and leave their result/state values uninitialised on the unnamed
path. The duplicate common state assignment writes the same value on valid
reset-built histories; state and both table arrays are distinct nonvolatile
regions, and all input reads finish inside `inverseMap` first.

This boundary is worth retaining before the inline profile is solved. Under
retained O3, process improves from 503 to 458 bytes, against 240 original
bytes (SIZE263 to SIZE218). Only its body changes; seven neighbours, all six
exact names, data values, imports and exports survive. The known no-auto
control reproduces EXACT240, but no such option is adopted. Retained O3 still
inlines the helper and removes/moves valid-case state stores after output
reads. Those native publication bytes remain a measured gap; the source
spelling and original option pair are not uniquely established.

Four combined cells cover thirty-two body verdicts. A bounded native selector
walk examines six representative unnamed modes in the reference and each
cell: six original and twenty-four candidate paths. Original and selected
paths have zero trellis reads and one state store; baseline paths have two
trellis reads. Changing the selected native JE to JNE makes the detector fire.
The walker refuses ambiguous selectors, unknown branch flags and external
edges. It checks these emitted MOV/address shapes, not arbitrary x86 programs,
indeterminate values, all 32-bit modes or public-modem reachability.

The three existing process mutation anchors now span the whole function:
removing state carry removes all four state assignments, and redirecting
output reads changes all three cases. Both negative controls are caught by
the period fixture. Replacing the `ns` index with the equal published-state
index remains full-object raw-identical under the deciding compiler. No new
mutants, default-junk runtime comparison or tolerance is introduced.

```sh
python3 tools/recovery_v92_table_ownership.py --domain definition-order \
  --baseline-dir build/allocation293-baseline
python3 tools/recovery_v92_table_ownership.py --audit
python3 tools/recovery_v92_boundary_adoption.py \
  --domain known-case-owner-native-data-2x2 \
  --baseline-dir build/allocation293-baseline
python3 tools/recovery_v92_boundary_adoption.py --audit
python3 tools/recovery_v92_boundary_adoption.py --production-control
python3 tools/recovery_v92_boundary_adoption.py --sensitivity
```

The combined production control raw-matches the selected full TU. The
table-only production-format control applies to its historical source;
current production includes case ownership too. The integrated source passes
392 period fixtures with zero failures and all structural checks, including
10042 unique mutation anchors. Keep the common-profile/TU ownership question
open; do not compensate for it with another residue, case or table-order
palette.

The separate [builder store observer](../../tools/v92_builder_store_observe.py)
runs the existing period fixture without compiling source variants. It pins
both native builder bodies, derives linked addresses from symbols, checks
static 32-bit Gentoo provenance and preserves fixture/object hashes. Fourteen
component calls produce 5376 ordered stores: both implementations agree on
every index, value and visit order over modes 0/1/2, and write nothing for four
unnamed selectors already present in the fixture. Changed-value, missing-store
and swapped-order controls all fail. This reconciles the reconstructed loops
and arithmetic, not the remaining 1456-versus-1558-byte code-generation gap.
The fixture comments now distinguish 64/256/1024 written entries per array
from their 244/504/1024 index extents. No new fixture or reachability claim is
introduced. Close builder source retries without a new operand/lifetime clue.

```sh
python3 tools/v92_builder_store_observe.py build/period/t_v92convmapper \
  --out build/v92-builder-parent
```

## Preserve initializer record owners and publication order

The next `preinitdigital` refinement recovers the original scrambler and
descrambler record views at `obj+0x2a54` and `obj+0xe74`. Within those views,
the original clears scrambler w0, sets nbits, then clears w3/w2/w1; it clears
descrambler count/w0 before w2/w1. A callback-field owner also restores direct
TX publication at `obj+0x2a28`, removing the intermediate shell-base LEA.
No field type, offset or compiler option changes.

The [eight-cell cross](../../tools/recovery_preinit_publication.py) separates
record ownership, group-store order and callback-field ownership. Every cell
produces a distinct object. The selected native-order/owner control is 533
bytes with 131 decoded instructions, both matching the original, but remains
BYTES147. The ascending-order owner control also has 533 bytes and 131
instructions while preserving the wrong store sequence. Size and instruction
count alone therefore cannot select the recovered source.

Across thirteen pair functions, all twelve neighbours, sixteen named data
records, twenty-nine definitions, ten imports and relocation consumers remain
unchanged. The three previously exact names survive; no exact name is gained.
The graph detector fires on both original and selected code, rejects the
ascending-order and missing-owner controls, and rejects a corrupted owner
address. Parent replay independently checks every candidate's bystanders and
exact set, rather than trusting the recorded score.

The three focused period fixtures pass 27 groups and 6186579 checks. Both
callback-pair mutants are caught with Gentoo objects, each failing two of 424
clause-7 checks. The complete source passes `make phase J=4`, 392 fixtures,
zero failures, with structural checks. Final production formatting raw-matches
the selected complete object; the mutation anchors follow the callback-field
expression. No standard mutation snapshot refresh is claimed.

Reproduce against the archived `c11823619` canonical objects/config:

```sh
python3 tools/recovery_preinit_publication.py --domain state-owner-publication \
  --baseline-dir build/next293-baseline
python3 tools/recovery_preinit_publication.py --production-control
```

Close this eight-cell owner/order domain. Next discriminate the original
interleaving of receive state-index and descrambler-count stores with the
remaining group clears. Keep that publication question separate from memset
argument setup and register scheduling, which have not been attributed to a
source discrepancy.

### Separate witnessed state publication from scheduled count publication

The next initializer refinement moves `rx->state_idx = 0` between scrambler
w0 and nbits, matching original instructions at `0x598da`, `0x598e6` and
`0x598ef`. The [four-cell replay](../../tools/recovery_preinit_interleave.py)
crosses that state position with the descrambler count position. All five
complete-TU compilations use the recovered Gentoo compiler, original flags
and bug-reproduction define; both baseline objects reproduce raw.

Four distinct shell source cells produce two distinct complete objects.
Moving state alone changes BYTES147 to BYTES106, with the same 533-byte
length and 131 instructions. It also restores the original count interleaving.
Moving count alone is raw-inert; moving both reproduces the state-only object.
Retain the minimal state move. The count's source position is not uniquely
recovered from its scheduled instruction position.

Only `preinitdigital` changes across thirteen pair bodies. All twelve
neighbours, sixteen named data records, twenty-nine definitions, ten imports,
nontext sections and resolved relocation consumers survive. All three exact
names survive, with no gains. The audit checks the expected graphs, verdicts
and raw equivalence classes, and rejects corrupted state and count addresses.

The remaining role-compare ordering has a measured allocation dependency.
In initial RTL, descrambler clears 239/241 precede role load 253 and compare
254. Combine folds 253 into 254. `25.greg` first displays owner pseudo 83
as EDX; the subsequent observer below establishes that local allocation made
that choice earlier. Peephole2 introduces XOR 413 to materialise zero in EDX;
its anti-dependencies on both owner uses and flags clobber constrain compare
254. Only sched2 moves the compare ahead of data/train publications, while
keeping it after the descrambler clears. The reference's ECX owner avoids
this EDX chain. This does not independently justify moving the source role
test earlier. Count 235 also acquires its native interleaving only at sched2.

The focused period fixtures pass three binaries, twenty-seven groups and
6186579 checks. Both existing callback sensitivity mutants are caught, each
failing two of 424 clause-7 checks. The complete delegate gate passes 392
fixtures, zero failures and all structural checks; no fixtures, mutation
anchors, flags or field layouts change. Production recompilation and the
runtime shell object raw-match the selected complete object.

```sh
python3 tools/recovery_preinit_interleave.py \
  --domain native-state-count-position-2x2 \
  --baseline-dir build/continuation293-baseline
python3 tools/recovery_preinit_interleave.py --audit
python3 tools/recovery_preinit_interleave.py --production-control
```

The frozen baseline is `4a6519cba`; earlier initializer controls remain
historical. Close further count/store permutations. Next trace owner 83's
allocation and zero-materialisation costs, separately from the first three
memset calls' argument owners. Their remaining register differences do not
establish another source statement-order defect.

### Observe the allocator decision before interpreting the first changed dump

The [initializer observer](../../tools/gcc3_preinit_alloc_observe.py) corrects
the stage inference above. `local_alloc` chooses the owners' hard homes;
`25.greg` first shows their substituted operands. Seeing a change in a dump
does not establish that the named pass decided it.

| Pseudo / role | Quantity | Lifetime | Selected home |
| --- | ---: | --- | --- |
| 79 / first memset destination | 0 | 20..22 | EAX |
| 80 / second destination | 1 | 30..32 | EAX |
| 81 / third destination | 2 | 40..42 | EAX |
| 82 / scrambler owner | 3 | 46..60 | EAX |
| 83 / descrambler owner | 4 | 48..68 | EDX |

These quantities use GENERAL_REGS, cross no calls, have no register suggestions
and equal EAX/EDX/ECX costs. Owner 83 overlaps 82, excluding EAX; both EDX and
ECX are free. Allocation order tries EAX, EDX, ECX, selecting EDX as the first
eligible home. Global allocation preserves the choices. This is not a measured
cost preference for EDX. The later rename chain cannot move owner 83 because
the other homes are unavailable over its four uses.

Memset size/zero constants are a separate mechanism. They remain direct
memory constants through `27.flow2`; no local pseudo allocation chose their
final registers. Peephole2 materialises all six in EAX. Actual scratch
observation covers 100 TU searches, twenty-three in this function. At
`30.rnreg`, reverse renaming produces destination EDX, zero EAX and size ECX
for all three calls. On the third call, destination 392 chooses EDX with
EAX/EDX/ECX ticks 11/9/13; zero 423 chooses EAX at 11/14/13; size 425 chooses
ECX at 15/14/13. These least-recent eligible choices explain the current final
arguments, not a source argument-evaluation order.

The reference uses owner ECX and destination/zero/size EAX/ECX/EDX. An original
competing lifetime or different compiler state remains possible; neither is
established by the present trace. Do not add a synthetic local to occupy EDX.
Close local store/argument spellings until an independent source constraint
changes the allocator's eligibility set or the rename chain.

The frozen `8e216110a` replay validates nine local choices, twenty-six rename
decisions and the scratch census. Positive, actual absent-target and scratch
observations all preserve the complete object. The absent target compiles
successfully but observation fails; a wrong-owner-choice trace also fails.
This is apparatus-only work, with no source or exact-set gain.

```sh
python3 tools/gcc3_preinit_alloc_observe.py \
  --baseline-dir build/allocation293-baseline
python3 tools/gcc3_preinit_alloc_observe.py --audit
```

## Recover the CenterAdapt history owner before fitting its spill

The original acquires the imaginary-history base `q+0xa0` once and indexes
through it in both spans. Capturing that view restores the coefficient-before-
history operand order and original sixteen-byte frame. CenterAdapt moves from
317 to 336 bytes against 347 original bytes, reducing its SIZE gap from thirty
to eleven. A four-cell cross shows that explicit coefficient-accumulator
statements are raw-inert with either history-owner choice; those statements
are not retained. This reviews an already witnessed owner acquisition rather
than reopening the earlier nine-cell spelling domain.

The [replay audit](../../tools/recovery_v34_center_owners.py) compares two
93-byte original arithmetic loop regions with their 90-byte candidate regions.
Each pair is literally byte-identical after deleting the original's single
three-byte `mov %eax,(%esp)` history spill. The two write-only stores and five
original alignment bytes account for the eleven-byte length gap. Corrupting
either candidate loop is detected. This proof does not establish an original
local declaration or justify adding a dead store, `volatile` or padding.

The twenty-six-function TU retains all fifteen exact names, with twenty-two
neighbouring bodies unchanged. Data, bindings, exports, imports and nontext
relocations agree. Changed neighbours are `V34EchoPreFilter`,
`V34ModulatorProcess` and `V34SetupModulator`. Their initial RTL differs only
in printed alias-set numbering; first executable-pattern differences appear
at peephole2. Do not mistake those numeric metadata changes for an earlier
algorithm discrepancy.

Installed Gentoo observation independently preserves the complete objects.
The extra CenterAdapt scratch search moves the incoming PreFilter cursor from
one to three, selecting BX rather than DX. Whole-TU searches increase from
81 to 82, CenterAdapt searches from five to six, and downstream choices shift
too. PreFilter's differing bytes increase from four to forty-three; it no
longer meets the grade-1 register-renaming comparison. This collateral is
explicitly reviewed for the stronger original ownership/operand recovery.
No exact name is gained or lost, and the exact floor remains unchanged.

The four-cell audit covers 104 body verdicts, both literal loop proofs and
two corrupted-loop controls. Source passes `make phase J=4`: 392 period
fixtures, zero failures and all structural checks, including 10042 unique
mutation anchors. The eq/ec/handshake lifecycle fixtures contribute 6/21/43
groups and 1606267/13789992/14368945 checks respectively. Parent independently
replays the source controls and scratch observations.

```sh
python3 tools/recovery_v34_center_owners.py --domain center-owners-four-cells \
  --baseline-dir build/next293-baseline
python3 tools/recovery_v34_center_owners.py --audit
python3 tools/gcc3_mechanism_scratch_observe.py \
  --reproduction-dir build/center-owners-293-cross \
  --target V34EchoPreFilter --out build/center-owners-scratch-parent
```

Next trace the actual period compiler's real-history pseudo lifetime, spill
allocation and the unused candidate frame word. Establish where an original-
compatible spill could arise before another scalar-owner source experiment.
Keep the separate history rereads after the real coefficient store. Recover
the unchanged neighbour against the final structural state, rather than
steering the compiler cursor around an intermediate reconstruction.

### Follow an allocated spill through regmove and reload cleanup

The unused candidate word is an allocated history spill home. The installed
Gentoo compiler allocates four SI words for pseudos 59 (`err_re`), 65
(`limit`), 66 (`wrapped`) and 75 (first real-history value), at offsets -4,
-8, -12 and -16. The fourth allocation occurs in `finish_spills`. Pseudos
75/89/104/118 share that last home, which becomes `(%esp)`. Two zero-size
BLK requests add no storage. The final frame has sixteen local bytes and
zero padding/outgoing space. Frame size alone would not reveal this owner.

The [installed-compiler observer](../../tools/gcc3_center_spill_observe.py)
follows each history value through initial RTL, regmove and reload. Before
`22.regmove`, the multiplications have separate destinations 77/91/106/120
and `REG_DEAD` notes for their history inputs. Regmove retargets each result
to that dying input. Reload then emits and deletes two output copies per
history pseudo before writing `25.greg`:

| Span / coefficient lane | History definition | History pseudo | Multiply / subtract | Output copies |
| --- | ---: | ---: | --- | --- |
| First / real | 64 | 75 | 65 / 66 | 262, 263 |
| First / imaginary | 83 | 89 | 84 / 85 | 265, 266 |
| Second / real | 124 | 104 | 125 / 126 | 268, 269 |
| Second / imaginary | 143 | 118 | 144 / 145 | 271, 272 |

These are observed candidate instruction UIDs. For each pair, the first copy
is emitted while processing the history definition and deleted at its
multiply; the second is emitted at the multiply and deleted at the accumulator
subtraction. All copies target pseudos with the same stack equivalence;
no access to that home survives in the first post-reload dump.

The reference has two surviving write-only stores at `.text:0x72bbc` and
`0x72c28`. They correspond in role to history-output copies 265/271, rather
than all eight candidate copies. The trace explains the candidate's unused
word; it does not reconstruct original compiler UIDs, establish why those
two original stores survived or identify an original local declaration.

The observer freezes `4a6519cba`, verifies the installed compiler hash and
preserves the complete candidate object in both target and absent-target
runs. The absent target compiles successfully but fails observation with no
target events. Missing-output, changed-padding and wrong-UID-origin controls
also fail. No reconstruction source, fixture, tolerance or exact floor changes.

```sh
python3 tools/gcc3_center_spill_observe.py \
  --baseline-dir build/continuation293-baseline \
  --out build/center-spill-parent
```

Close source spelling retries at this boundary. Reopen only with independent
lifetime/death-note or common-profile evidence explaining the selective
original spill. An allocated slot is insufficient evidence for a source
local, and adding artificial stores would bypass the measured question.

## Preserve equivalent switch arms when their call boundaries differ

`designWindow<float>` explicitly distinguishes BOXCAR (zero) from `default`
in the reference. Zero tail-jumps to `boxcar`; default calls it and returns.
The reconstruction collapsed both into default and its comment dismissed the
extra original path as a layout artefact. That preserves window values but
does not recover the original selector structure.

Restoring the separate BOXCAR arm makes both emitted copies EXACT: 99 to
116 ELF bytes, including two symbol-owned padding bytes. This is one unique
original function and 116 newly exact original bytes. The bounded two-cell
header cross covers all five quoted-include consumers and 103 function copies;
only the two selector copies change. The other three complete objects are
raw-identical. Data, bindings and all other relocation consumers are preserved;
the selector gains its second boxcar reference. Only its symbol/COMDAT sizes,
relocation-section size and later physical file offsets change.

The focused Gentoo gate passes three binaries, 25 groups and 612113 checks.
Existing selector sweeps cover zero and unnamed values, so no new fixture or
semantic failure is claimed. Canonical recompilation independently reproduces
the selected objects; the other 298 source objects remain raw-identical.

```sh
python3 tools/recovery_window_boxcar.py --domain explicit-boxcar-two-cells \
  --baseline-dir build/native293-baseline
python3 tools/recovery_window_boxcar.py --audit
python3 tools/recovery_window_boxcar.py --production-control
```

The [tail-transfer audit](../../tools/recovery_tail_transfer_audit.py) joins
direct calls and unconditional external jumps, including uniquely resolved
same-section function starts. Historical call/tail inventories already cover
named PC32 edges; the residual call ledger by itself omits tails. Reconcile
this audit to the canonical manifest and census rather than treating a bare
call-count difference as missing behavior.

At frozen `6fe0b1e42`, all 300 objects contain 1886 shared defining copies of
1852 names. Two hundred copies have a named tail; original/candidate tail-site
counts are 234/232. Six target-multiplicity nominations include both selector
copies; after recovery there are four and the tail counts are 234/234.
Transfer-kind nominations fall from sixteen to fourteen. The remaining target
differences are B103's previously reviewed shared diagnostic suffix and three
helper-inlining witnesses, not newly established missing behavior.

Four controls check the original separate boxcar call/tail, a changed transfer
opcode, refusal of an interior-target addend, and exclusion of the actual
boxcar loop. Indirect transfers, ambiguous/interior destinations and conditional
exits remain unproved. Equal target bags do not establish arguments, order,
path coverage or behavior. This read-only audit performs no compilation.

```sh
python3 tools/recovery_tail_transfer_audit.py --objects build/tc_repro \
  --census build/native293-census.json --out build/native293-tail-after.json
```

## Check native delivery boundaries before reopening CID source variants

The [CID observer](../../tools/cid_process_trace.py) follows real public
create/feed/delete histories through two independent receiver chains. It
records only calls made directly by `CID_process`, their results, input
offsets/counts, owner identities, string guards and TTY/debug arguments in
execution order. Receiver internals remain behind `cid_progress`. All sixteen
relocations per wrapper and all other linked body bytes are checked against
the input objects; normalized body hashes pin the reviewed instruction sites.

The fixture includes debug-enabled bad-checksum Bell 202 audio and four
real MDMF histories: date/number/NAME, with and without an extra tag, at debug
levels zero and two. It now passes 476 checks, up from 193. These are valid
CID component histories, not modem end-to-end reachability evidence.

The MDMF investigation first failed in both independent receivers before
`get_strings`. Decoded received bytes exposed matching packet corruption.
The fixture's old sine lookup was malformed: its quadrature samples were
0, 7688, -929 and -7331, rather than 0, 8000, 0 and -8000. Replacing it with
rounded `8000*sin(2*pi*i/64)` restores the intended carrier without changing
frequency, amplitude or production source. The observer independently checks
the actual period-linked table against the continuous formula and half-cycle
symmetry. The old table fails that calibration. The original failed executable
was not frozen; retained packet observations do not substitute for that missing
artifact. Do not describe this fixture correction as a receiver bug fix.

The expanded trace has 214 paired wrapper calls and 830 matching ordered
events. Four successful histories per side visit four nonempty fields; two
also leave a rendered fifth field undelivered. NAME reaches TTY, while the
extra MESG does not. Debug failure still prints after failed progress and
performs no string or TTY operation. Whole-buffer success stops before its
remaining input is offered.

At wrapper entry the observer decodes the actual fixture `bits`/`nbits`,
checking the seizure/mark prefix, async start/stop framing and trailing mark.
Every settled received packet must equal those transmitted bytes; successful
packets must also have a valid checksum. Eleven detector controls fail,
including changed carrier samples, fourth-guard and fifth-delivery mutations,
zero observations and equal received-byte corruption with a recomputed valid
checksum. Equality between implementations and a valid checksum alone cannot
certify a transmitted message.

The complete CID source object remains raw-identical and both exact companions
survive. `CID_process` remains 368 versus 386 bytes. No new source discrepancy
or codegen gain is established; keep the 1433 recorded source/option cells
closed. Raw output and 9600-Hz processing remain separate fixture limits.
Do not plant a null string return: the actual callee returns its owned buffer.

```sh
python3 tools/cid_process_trace.py build/period/t_cid \
  --require-four-field-cap --out build/order293-cid-trace
```

## Separate helper-reference evidence from original TU ownership

The [V92 reference audit](../../tools/v92_reference_ownership.py) verifies a
frozen 300-object build, all relocations and exact per-function decoding.
Its reference/current denominators are 1852/1916 functions, 18317/18329
relocations and 185305/183556 decoded instructions. No function decode is
incomplete and no function extents overlap ambiguously. Five copied-ELF
controls detect an extra caller, equivalent section-relative target, non-call
address use, incomplete opcode stream and overlapping symbol extent.

Only encoder `process` references `inverseMap`; only transmitter `process`
references encoder `process`. The current transmitter boundary remains
EXACT355, while the helper's standalone body remains EXACT440. Function/table
binding, visibility and section kinds agree, as does the 320-byte table block.
The mangling confirms non-const `int*` inputs and non-const methods, without
encoding return type, access control or original inline annotations.

GLOBAL DEFAULT definitions and a FILE record without LOCAL anchors do not
settle source-object membership. No independent caller, address reference,
field/type discrepancy or defining-owner witness licenses splitting this TU.
Indirect destinations without relocated addresses, anonymous gaps and callers
outside the object remain outside the observation. Require independent source
visibility evidence before another ownership or inline-cost spelling palette.

```sh
python3 tools/v92_reference_ownership.py --baseline-dir build/native293-baseline \
  --output build/native293-v92-ownership/audit.json
```

### Cross definition visibility across independent callers

The [visibility cross](../../tools/recovery_visibility_cross.py) compiles
unchanged encoder, V92 echo canceller and V90 phase-4 modulator TUs at retained
O3 with automatic inlining and unit-at-a-time independently on/off. All three
baseline objects reproduce raw. Twelve complete-TU cells account for 304
original-function verdicts and 312 emitted copies, including eight additional
local-helper copies. Complete commands, source/header/object hashes, actual
compiler/assembler banners, data layouts, bindings, imports and relocation
consumers accompany the body scores.

| Profile | Encoder helper / V90 wrapper | Echo constructor reset / delay setup |
| --- | --- | --- |
| Retained | Both inline | Both inline |
| Automatic inline off | Original CALL / tail JMP restored | Reset tail restored; delay becomes explicit calls |
| Unit-at-a-time off | Both still inline | Reset tail restored; delay becomes explicit calls |
| Both off | Original CALL / tail JMP restored | Reset tail restored; delay becomes explicit calls |

Automatic inline off makes encoder process EXACT240 and the short V90 wrapper
EXACT5, plus five other V90 wrappers. It also emits four extra LOCAL echo
helpers and loses the original constructor's inlined delay setup: each
constructor gains two static delay-call sites. Unit-off changes encoder table
order and diagnostic-string layout, loses exact standalone `setEchoDelay` and
V90 `resetRRNSecondSection`, and gains a Scrambler constructor copy. Preserve
all these observations; no cell recovers the original constructor's mixture
of inlined delay and called reset, and no production option is adopted.

The original strong GLOBAL `setEchoDelay` body precedes the constructors.
Its constructor expansions load parameter `+0x70`, read old object `+0x38`,
publish the new delay there and adjust `+0x2c` before printing. Current source
defines this ordinary method after the constructors, as it also does reset.
Physical emission order alone does not prove declaration order under
unit-at-a-time, and ordinary header-inline factoring can change the strong
export to weak or omit it. Do not add an inline annotation to force a score.

The subsequent four-cell cross moves only the unchanged delay definition
before the constructors, versus current order, under retained and unit-off
profiles. Under retained options both complete objects are raw-identical:
zero of fifteen bodies, positions or records change. Before-plus-unit-off
restores the original constructor edge bags and keeps all fifteen strong
exports, but the constructors remain BYTES112. Current-plus-unit-off instead
has two explicit delay sites per constructor and loses exact `setEchoDelay`.
Emission-order inversions also remain. This establishes the predicted
visibility interaction without recovering an original global profile or
justifying a retained source-order change. No move or option is adopted.

The replay checks sixty original-function verdicts, a raw baseline and three
negative controls. Its focused Gentoo gate passes three binaries, twenty-five
groups and 161066 checks. Matching standalone/expanded operations support
inlining or manual expansion; they do not uniquely prove the original author
called the helper.

```sh
python3 tools/recovery_echo_delay_order.py --domain ordinary-delay-definition-order-by-unit-visibility-four \
  --baseline-dir build/order293-baseline
python3 tools/recovery_echo_delay_order.py --audit
```

```sh
python3 tools/recovery_visibility_cross.py --domain visibility-profile-twelve \
  --baseline-dir build/native293-baseline
python3 tools/recovery_visibility_cross.py --audit
```

## Trace helper factoring before changing the inline profile

V92EchoCanceller reset originally duplicated four existing helpers' operations
in its source. Native operands and order agree with `zeroEchoCoeff`,
`resetEchoHistory`, `setEchoBeta(0)` and `setDecayFactor(0)`, around the same
state publication and ARMA reset. Other methods in this TU already call these
ordinary helpers. Native constructor references to LOCAL coefficient arrays
anchor the constructors to this TU; they do not establish the setter/reset
source owner or original call spelling.

A two-cell retained-profile experiment replaces only the duplicated reset
regions with ordinary helper calls. Definitions, headers, source order and
compiler flags stay fixed. The period compiler still emits EXACT195 for reset
and preserves the other twelve non-constructor bodies after relocation
normalization. Both constructors fall from 447 to 313 bytes, matching the
reference size and original ordered call/reset-tail edges. They remain
BYTES112; no exact name or exact original byte is gained or lost.

The causal evidence is in the actual Gentoo callgraph dump. Baseline reset's
initial estimate is 69 instructions and it is inlined into the constructors.
Factored reset starts at 58, then grows through 65, 83 and 101 as helper
expansions are considered. GCC explicitly declines the constructor expansion
after that growth, reporting `--param max-inline-insns-single limit reached
after inlining into the callee`. The installed defaults are auto 100 and single
500; that generic diagnostic is not proof that a 500-instruction bound was
exceeded. The constructor estimate changes from 159 to 157 rather than growing
to 216. This recovers a native boundary under retained options
without tuning thresholds, disabling automatic inlining or adding attributes.
An already-exact out-of-line body can conceal a different source factoring
that changes its callers.

All fifteen strong exports and named data survive; emitted function bytes fall
from 4513 to 4245. Other raw body differences come from pooled-string relocation
addends, whose canonical consumers are unchanged. The replay audits thirty
body verdicts, complete commands and actual compiler/assembler versions.
Missing-edge, missing-cost and WEAK-export controls fail, including a changed
export whose function-body verdict remains EXACT. Four focused Gentoo binaries
pass 28 groups and 161199 checks. Sixteen moved reset mutation anchors now name
the shared helper/reset owners; all sixteen faults are caught (fourteen
differential failures, two runtime signals), and the restored baseline passes.
This is a targeted mutation run, not a full snapshot refresh.

```sh
python3 tools/recovery_echo_reset_factoring.py --domain ordinary-reset-factoring-two \
  --baseline-dir build/order293-baseline
python3 tools/recovery_echo_reset_factoring.py --audit
```

The next discriminator is the constructor history-allocation arithmetic.
Reference code keeps `quotient*blockLen` separate from `span+2*blockLen`;
current code forms `(quotient+2)*blockLen`. Its extra instruction and spill
therefore do not establish a missing statement. In both constructors this
factoring already appears in the earliest saved `.original` tree, remains in
`.inlined`, and reaches initial RTL as DIV, ADD2, MULT, ADD span, ADD extra and
store (UIDs 147 through 152 in the retained C1 dump). Later CSE, reload and
scheduling do not introduce it. The saved preprocessed source still has the
separate source terms. The `.original` tree already contains helper expansions,
so its name does not establish parse/pre-inline timing.

Trace expression construction/folding in the installed period compiler before
proposing another arithmetic spelling. Require independent expression/lifetime
evidence, an absent-target control and full-object preservation from the
observer, then review the full TU for any source candidate.


## Trace parser folding before explaining a spill

The V92 constructor history-allocation expression is a worked counterexample
to attributing a spill to register allocation. A read-only GDB observer of
the installed Gentoo `cc1plus` binds the actual `historyAlloc` assignment to
parser `build_binary_op` and nested `fold` calls. The parser factors
`blockLen*2 + (span/blockLen)*blockLen` into
`(span/blockLen+2)*blockLen` before adding `extra`. The captured arithmetic
is unsigned 32-bit. This establishes the stage and transformation; it does
not uniquely recover the original declaration spelling.

The observer requires the frozen complete compiler command, installed compiler
hash, actual selected assembler, source/header provenance and raw equality of
the complete reassembled object. An absent assignment target must compile
successfully but fail observation; missing binding, missing fold and changed
arithmetic type controls must also fail. A dump's filename alone is not a
timestamp for the transformation.

A bounded three-cell experiment retains the baseline, computes
`roundedSpan = span / blockLen * blockLen` in a separate ordinary statement,
then commutes only the two operands forming `span`. The product statement
restores the reference DIV/IMUL product lifetime and stack spill: both
constructors move from BYTES112 to BYTES2 at 313 bytes. Those two remaining
displacement bytes identify the native loads of `echoDelay` before
`filterLengthMinusOne`. Restoring that operand order makes both constructors
EXACT313. No flags, barriers, volatile stores or forced registers are needed.
Review the complete TU, including all fifteen strong exports, data, positions,
relocation consumers and the thirteen other function bodies.

Replay against the frozen `c7cf4f8cd` baseline:

```sh
python3 tools/gcc3_echo_history_fold_observe.py --baseline-dir build/fold293-baseline
python3 tools/recovery_echo_history_product.py --domain history-product-three \
  --baseline-dir build/fold293-baseline
python3 tools/recovery_echo_history_product.py --audit
```

Four focused period binaries pass 28 groups and 161199 checks. Four moved
arithmetic mutation anchors are caught: three differential failures and one
runtime signal; the restored baseline passes. These are targeted mutation
checks, not a full snapshot refresh. Independent observer replay preserves
both complete objects, including the absent-target control.

## Audit anonymous-copy identity without hiding extraction failures

`getSegmentPointer(PcmType, int)` has one unresolved anonymous relocation:
the original uses `.rodata+0xb80`, the candidate `.rodata+0`. All 64 copied
bytes agree, and the other bytes of the 115-byte function agree. A diagnostic
can establish forward REP MOVSL, a bounded private stack destination, sixteen
words, no source-pointer escape, and a full source-register overwrite after
the copy. That is evidence for a future bounded identity proof, not a source
change or permission to canonicalize arbitrary equal anonymous data.

Thirteen actual ELF mutations exercise extent, contents, writability,
relocations, copy state, pointer escape and interior entry hazards. The
named-interior-entry mutation exposed an independent extraction defect:
`objdump --disassemble=name` can return bytes from the wrong address when
symbols overlap. Comparing two identical malformed inputs then falsely says
EXACT because the intended relocation was never extracted. Validate symbol
ownership and decoded address coverage before interpreting such a verdict.
The shared extractor now reads the ELF owner independently, accepts exact-extent
aliases, refuses ambiguous overlaps, and disassembles the selected section's
explicit address interval. It verifies every extracted byte and the complete
in-function relocation-offset inventory before canonicalization. Unsupported
ownership returns NODATA, never EXACT. The original false-EXACT control now
returns NODATA; legitimate free-function and constructor aliases still work.

The immutable before/after audit covers 300 objects, 1852 shared names and
1886 candidate body copies: no ordinary body or verdict changes. Four real
ELF cases, two constructor alias entries and five corrupted decoder-output
controls exercise the guard. The subsequent [readonly-copy proof](#prove-readonly-initializer-copies-without-changing-emitted-code)
credits existing emitted code, not newly recovered source.

```sh
python3 tools/recovery_body_extraction.py --objects build/tc_repro
```


## Cross CID output mode and sample rate at the component boundary

The raw-output contract is `cid_val == 2`: lowercase hexadecimal type, length
and payload, excluding the checksum, delivered as one field plus CRLF.
Formatted output retains its four-field delivery cap. Native constructor
arguments establish that 9600 Hz retunes the FSK receiver; native wrapper
calls establish that 8000 Hz uses 160-sample resampling and 9600 Hz uses the
192-sample direct path. These are component contracts, not modem end-to-end
reachability claims.

Six added positive lifecycles cross raw 8000, formatted 9600 and raw 9600
with debug 0/2. All pass without waveform tuning. The fixture has 836 checks;
the observer compares 430 paired calls and 1420 events. Per side it sees five
constructor retunes and 218 resampler calls at 8000 Hz, none at 9600 Hz.
It checks the complete waveform on 426 entries per side against the actual
decoded wire bits, line rate and calibrated sine table, rather than trusting
the fixture's case label. Nineteen rejection controls cover equal corruption,
wrong oscillator rate at unchanged packet length, a changed sample, raw case,
byte order and checksum inclusion, missing retuning, resampling and coverage.

Production CID source and its complete object stay unchanged; no discrepancy
was found in this domain. Raw/9600 failure and recovery histories remain open.
Do not interpret positive success coverage as a reason to repeat closed source
spelling experiments on the remaining 368-versus-386-byte process body.

```sh
python3 tools/cid_process_trace.py build/period/t_cid \
  --require-four-field-cap --require-raw9600
```


## Trace store forwarding across related native consumers

For V92EchoCanceller's two diagnostic setters, the source tree and initial
period RTL retain the sign's member read. The first RTL CSE pass removes that
read and substitutes the incoming argument. In the pinned complete TU, the
store is UID 14 from SF pseudo 59, the member load is UID 75 into pseudo 74,
and branch UID 83 changes to use pseudo 59. The member load survives through
`05.null` and first disappears in `06.cse`. Reload and x87 stack conversion
therefore inherit this value lifetime; they do not originate the missing read.

The native setters compare with `fcomps` against member `+0x30` or `+0x34`;
the candidate keeps a value live and uses `fcompp`. The remaining difference
also includes a register-stack multiply versus a memory multiply, pop behaviour
and scheduling. A two-byte length difference does not isolate a two-byte defect.

Use related native consumers to test the explanation. All six `setState`
member comparisons survive in the reference, including two zero-valued cases.
Its candidate loses those reads in first CSE. But native `reset` folds both
zero-valued signs to literal `'-'`, and the current reset is already EXACT195.
Its initial RTL also has member reads, removed by the same first CSE pass.
A blanket volatile-member change on the current shared-helper source does not
explain both native behaviours. This does not exclude a crossed original
source/qualification explanation in which reset used different diagnostic
expressions. No such source reconstruction is established by the present trace.

Keep the source and profile unchanged until a discriminator explains the
site-dependent reloads. Compare original helper/macro argument ownership and
reset's sign expression before trying another arithmetic spelling. A field
read written in source is not evidence that the compiler emitted it; comments
must distinguish the native operand from the current generated operand.

The read-only named-setter follow-up does not provide that missing witness.
Across 72 emitted setter copies, 51 are exact and six call `edprintf`; the
only exact floating diagnostic in that subset is `V90PreFilter::setParamEia6`.
It compares a computed local, not a published member. The related equalizer
beta setters are SIZE9 and compare the incoming argument before publication.
Their shared sign predicate does not license transferring their source spelling
to V92's store-then-readback case.

Replay the two raw-identical complete-TU controls and the trace audit:

```sh
python3 tools/recovery_echo_decay_lifetime.py --baseline-dir build/copy293-baseline
python3 tools/recovery_echo_decay_lifetime.py --audit --baseline-dir build/copy293-baseline
```

The audit checks thirty body verdicts, 52 setter RTL streams, twelve consumer
streams and eight trees. Six changed-trace controls reject missing loads,
wrong fields/replacements, retained loads, wrong scale mode and missing death
notes. Four focused period binaries pass 161199 checks. No executable source,
type, profile or exact-membership change is licensed by this result.

## Model recovery at the receiver's actual lifecycle boundary

The CID host treats any nonzero `CID_process` result as terminal, calls the
stop path, and creates a new receiver for the next start. The public wrapper
has no reset method. A checksum-error recovery fixture must therefore delete
and recreate the receiver before delivering another packet. Replaying into the
failed object or clearing private parser state would test a different lifecycle.

Cross raw 8000 Hz, formatted 9600 Hz and raw 9600 Hz with debug 0/2. Preserve
the existing calibrated waveform and require a checksum error with no callback
or TTY delivery, destruction, a fresh construction at the same configuration,
and then successful delivery. Identify lifetimes by observed construction
and destruction events; pointer inequality is not a valid freshness test,
because the allocator may reuse the same address.


The six failure/recreate/recovery pairs raise CID's fixture to 1106 checks.
Independent native replay compares 646 paired calls and 1858 ordered events;
all 24 controls reject. All 44 accepted constructions across both sides have
zero packet length, packet bytes and fill count. Receiver addresses are reused
in all twelve paired histories, confirming why construction generations matter.
Production source and its complete object remain unchanged.

```sh
python3 tools/cid_process_trace.py build/period/t_cid \
  --require-four-field-cap --require-raw9600 --require-failure-recovery
```

## Prove readonly initializer copies without changing emitted code

The shared comparator can now identify an anonymous readonly literal consumed
by a bounded stack initializer. The proof decodes instructions and dataflow;
it contains no function-name, function-size or fixed-byte-template exception.
Its supported domain is deliberately narrow: an ELF32 i386 leaf with entry
CLD, one fixed private frame, a straight-line setup, one default-width REP
MOVSL with a known positive count, and an immediate full source-register kill.
It tracks register aliases and source/stack pointers, rejects pointer escape,
checks saved-register disjointness and both arms of the remaining CFG, and
requires balanced restoration. Unknown forms fail closed.

The complete source extent must be ordinary allocated readonly data, without
ambiguous ownership or relocations. Named, encoded-direct and relocated
interior entries must not bypass the setup. Only the consumed relocation gets
a tagged identity containing width, count and every copied byte. All other
instruction bytes and relocation operands retain their ordinary comparison.
Function extraction also rejects relocation fields crossing either function
boundary: a relocation starting outside the symbol can still overwrite its
first instruction at link time.

For `getSegmentPointer(PcmType, int)`, the two 64-byte literals are identical.
The decoded copy uses sixteen words in a 92-byte frame; saved registers occupy
slots 76, 80, 84 and 88. REP MOVSL is at function offset 50 and the full ESI
kill at 52; seventeen tail CFG states are checked. This completes an identity
proof for existing emitted code. It is not a newly reconstructed source body,
and it does not make the partial object raw-identical.

Controls must distinguish unsafe copies from different safe copies. Twenty-five
hazardous or unsupported actual ELF variants remain UNRESOLVED or NODATA even
when compared with themselves. Two safe variants—a changed final literal byte
and a shorter bounded copy—are exact against themselves but fail against the
original. Requiring every mutation to refuse its own identity would merely
replace a semantic proof with a hidden encoding whitelist.

```sh
python3 tools/recovery_readonly_copy.py --objects build/tc_repro
python3 tools/recovery_readonly_copy.py --objects build/tc_repro --focused-only
```

The focused controls run in Gentoo CI after canonical compilation. Its metric
cache keys include the proof module. The older body-extraction audit explicitly
disables the new literal canonicalization during its extraction-only comparison,
so its historical no-change assertion still tests extraction independently.

## Audit recorded defensive defaults before tuning the compiler

A deviation labelled harmless can still encode source absent from the reference.
The D385, D386 and D1201 entries explicitly described invented initialisers,
but their sources unconditionally supplied those values even in the reproduction
build. Check the native definition/use paths, rather than trusting the old claim
that undefined source must never appear in this tree.

Three bounded corrections retain the existing fallback only outside
`DSPLIB_REPRODUCE_BUGS`:

| Function | Native evidence | Period result under retained flags |
| --- | --- | --- |
| `V90Demapper::hardDecision` | Capacity refusal skips both sign-slot stores, then multiplies the loaded value by zero | 559 → 560 bytes, reference 660; thirteen other TU bodies unchanged |
| `V90SignBitsExtractor::process` | States other than 0/1 reach the action dispatch without assigning its register | 418 bytes before and after, reference 417; eleven other TU bodies unchanged |
| `FAX_FSE_decision_128pt` | Prologue initialises `base` and `amb`, but not `best` | 904 → 888 bytes, reference 910; six other TU bodies unchanged |

These are source-fidelity corrections, not exact-function gains. The V.17 size
gap grows from six to twenty-two bytes while removing an invented store. The
full-TU comparison preserves data, exports and resolved relocation consumers;
a smaller scalar score would not justify restoring that store.

The focused period fixtures pass: `t_v90demap` and `t_v90sbereset` cover the
sign-bit process with 1027 checks and reset with 488; `hardDecision` has 88328
checks. `t_v17slicer` passes 19 groups and 2798739 checks, including component
handshake walks and the existing region/threshold tests. No test asserts that
two indeterminate register or stack values agree. Sign-bit reset permits an
invalid state at the component boundary, but this is not modem-lifecycle
reachability. The all-0x7fff V.17 search outcome remains unproved, not impossible
because a finite scan did not reach it.

Replay the V.17 full-TU control against the frozen pre-change objects:

```sh
python3 tools/recovery_v17_best_initialization.py \
  --domain documented-native-best --baseline-dir build/copy293-baseline
```

The replay first reproduces the complete baseline object, then checks all seven
bodies, data, binding and relocation consumers. Initial RTL supplies a positive
and negative control: exactly one zero assignment to target local `best` in the
baseline and none in the candidate. Scope this count to the target function;
the same local name occurs in four other slicers in this TU.

## Distinguish forwarding passes across diagnostic consumers

A broader native inventory finds 24 lexical SF store/member-compare pairs in
11 symbols, across 1852 functions and 185305 decoded instructions. Its domain
contains 195 explicit SF member stores and 57 direct SF member comparisons.
It excludes indexed addresses, EBP owners, integer stores and register-based
comparisons; lexical address equality is not an alias or control-flow proof.
Known setter positives, removed-store negatives and reset controls calibrate
this screen. All nominated baseline consumers are nonexact.

Two independent witnesses widen the Echo investigation:
`V90Parameters::loadModemParamsData` stores member `+0x380` then rereads it for
the sign with no intervening call or branch; `ResamplerTiming::adjustHalfBaudBpfGain`
does so at `+0x80` with no intervening call. The Parameters source uses a direct
sign expression, so a sign-helper spelling alone cannot explain this family.

The period traces falsify a shared first-CSE explanation. Echo's sign loads
vanish in first local CSE. Parameters' load UID85 and Resampler's member load
UID126 survive that pass, then disappear across GCSE. Precisely skipping the
first `cse_main` invocation in the installed period compiler leaves both full
objects raw-identical; plain, unchanged-GDB and absent-target controls reproduce
the baseline too. This intrusive diagnostic is not a compiler profile or source
change.

A separate `-fno-gcse` control preserves those two loads but does not recover
an exact function. Parameters emits the native member comparison and remains
BYTES173. Resampler still compares a loaded register and changes constant
sections. Echo still forwards its setter reads in first CSE. Across 38 function
bodies and two profiles, the control gains zero exact names and loses five.
All export bindings survive. Retain these observations, not the option.

```sh
python3 tools/recovery_diagnostic_inventory.py
python3 tools/recovery_diagnostic_cse.py --domain independent-sign-readbacks \
  --baseline-dir build/copy293-baseline
python3 tools/recovery_diagnostic_cse.py --observe
python3 tools/recovery_diagnostic_cse.py --audit
```

The audit checks complete compiler commands, the selected period assembler,
three raw-reproduced baselines, 76 body verdicts, data and relocation collateral,
and three corrupted-stage controls. A restored memory operand is an observed
mechanism effect; it does not uniquely recover the original source or flags.

## Recover the quotient and countdown before explaining scratch registers

`SGD_correlate` supplies a small, independently witnessed width and loop recovery.
The reference narrows the integer division result with `cwtl` before saving its
scale. It carries a decremented short loop counter while testing the previous
value, and loads the `a` operand before `b` for the XOR table index. The source
now declares `short scale` and uses `for (i = n; i-- != 0;)` with that operand
order. The 107-byte body becomes 114 bytes, matching the reference length and
its first forty instructions; the verdict moves from SIZE7 to BYTES3.

The eight-cell width/countdown/operand cross and twelve ordinary declaration/
product-order controls preserve all nine TU symbols, three exact names, binding,
visibility and allocated nontext contents. Only `SGD_correlate` changes. Two
corrupted-nontext controls demonstrate the collateral audit. The isolated
candidate passes `make phase`: 392 period fixtures, zero failures, including
192 correlate checks.

The last three differing bytes belong to the scale reload register, multiply
operand and dead pop. Do not infer their origin from final assembly: dumps
through `27.flow2` still contain a memory multiply and a stack adjustment.
`28.peephole2` introduces both the BX reload and CX pop; the reference uses SI
and BX. The bounded declaration/product domain has no exact hit. Reopen source
only with a new lifetime or surrounding-source witness, not a forced register.

```sh
python3 tools/sgd_structure293_reproduce.py --domain native-short-countdown \
  --baseline-dir build/copy293-baseline
python3 tools/sgd_tail293_reproduce.py --domain ordinary-tail-order \
  --baseline-dir build/copy293-baseline
python3 tools/sgd_structure293_audit.py
```

Pin whole-object baselines independently of the source experiment. An agent's
300-object partial-link pair included the parent's concurrent V.17 correction;
its 71975 → 72089 positioned-byte result is an isolated pair, not the delta
against the frozen PR partial object. Object hashes identified the changed
V.17 body. Preserve that provenance and use the integrated parent comparison
for the PR checkpoint.

## Preserve floating absolute value before diagnostic conversion

The wider member-readback investigation exposed a separate operation mismatch
in `V90Parameters::loadModemParamsData`. Native `fabs` at `0x2a731` feeds the
whole-part integer conversion at `0x2a76b`. The reconstructed integer absolute
value after conversion produced the same ordinary values but a different
instruction graph. Use `(int)fabsf(pr)` and retain the signed fractional path.
The earlier conversion at `0x2a752` serves the fraction; pairing it with the
whole-part `fabs` would misidentify the expression.

Under retained flags this source correction makes the target 344 bytes, the
reference length, from 332. BYTES159 remains. All eight other TU bodies, data,
binding and relocation-consumer inventories are unchanged, with no exact-name
gain or loss. The focused period fixture passes 4005 checks, including 642
checks of this method across debug levels 0..3 and unsigned input endpoints.
The input-derived value is finite, nonnegative and below 2^29; this result does
not excuse a NaN or integer-conversion overflow mismatch elsewhere.

The first candidate lacked a declaration for `fabsf` and was rejected by the
period compiler. Preserve that invalid run separately, then rerun with the
ordinary `<math.h>` declaration before interpreting emitted code. The source
operation is supported by the binary; the declaration makes that source valid
for the compiler which must decide it.

```sh
python3 tools/recovery_diagnostic_cse.py --whole-expression \
  --domain native-float-absolute --baseline-dir build/copy293-baseline
python3 tools/recovery_diagnostic_cse.py --audit-whole
```

Publishing `DIGITAL_POWER_REDUCTION` immediately after computing `pr` recovers
another native ordering constraint. The store moves from initial RTL UID70 to
UID42, before the fractional and whole-part work, and restores the native
`fsts` / `fmul`, `fld`, `fabs`, `fxch` sequence. Under retained flags the
344-byte body improves from BYTES159 to BYTES106. Five source cells and two
GCSE controls produce four distinct complete objects; the three early-store
source spellings are raw-identical. Their original spelling remains unknown.
All 63 body verdicts preserve the other eight bodies and nontext collateral;
the 4005-check period fixture passes. Replay with `--owner`, `--audit-owner`
and `--owner-period` on `recovery_diagnostic_cse.py`.

Reanchor a displaced mutation explicitly. Removing `fabsf` changes the object
but still passes all 4005 checks: `pr` is nonnegative throughout this input
domain. The existing mutation remains labelled equivalent. The 37 mutation
anchors are valid; this is not a newly detected functional difference.

## Trace a predecessor before tuning its successor's scratch register

The SGD peephole trace opens a new source lead in `SGD_sequence_gen`. Native
code advances the output pointer before fetching the symbol, saves the old
short sequence index before incrementing it, and tests the old loop count.
Combining the output assignment with `seq[i++]` and using short postdecrement
recovers that graph. Four crossed expression/countdown cells move its 214-byte
body, SIZE32, to the native 246-byte length, BYTES47. After offset `+0x49`, only
four MOV displacement bytes differ, selecting different local stack slots.
Keep all 47 raw differences in the result; that suffix observation is not a
normalisation or an exactness claim.

Read-only GDB tracing of the actual period compiler models all 52 scratch
searches across these four cells. The expression adds four predecessor
searches. Its last search selects DI, changing the cursor inherited by
`SGD_correlate`; correlate moves from BYTES3 to BYTES9. Only these two of nine
bodies change, and the three exact names and nontext contents survive. The
focused period fixture passes 26256 generator and 192 correlate checks.

The isolated whole-object pair makes the cost visible: replacing only SGD in
300 frozen objects moves positioned matches from 72230 to 72102 out of
943398, while exact symbols stay 457. Both are raw-different from the reference.
The frozen PR-start object has 72088 positioned matches and is a separate
baseline. Retain the independently supported source graph with its measured
layout tradeoff. Do not infer the original source from the aggregate score.

`sgd_scratch293_reproduce.py` / `sgd_scratch293_audit.py` retain the read-only
trace controls; `sgd_sequence293_reproduce.py` / `sgd_sequence293_audit.py`
retain the crossed source experiment. Their baseline pins the durable
intermediate correlate commit. Plain and observed compilations must remain
raw-identical. The initial `-dP` attempt emitted nondeterministic pointer
comments and remains invalid evidence; the valid replay uses `-v -save-temps
-da` without deleting differences.

## Audit raw-to-typed replacements at the use site

Five V32 errors came from commit `0e473a213` replacing correct raw addresses
with incorrect typed members. Correct structure sizes and offset assertions
did not catch the wrong *choice* of field. Existing comments still quoted the
correct addresses. Review the complete owner chain, effective offset, width
and signedness at each replacement, using the binary as the authority.

| Consumer | Native location | Incorrect member | Correct member |
| --- | --- | --- | --- |
| Control ring-response clear | HDX `+0x44` | `short_46` | `regs[4]` |
| Status `r12` | FP `+0x182` | `ecc.pwr_out` | `sre.pll_acc` |
| Status `r18` | FP `+0x188` | `ecc.adapt_near` | `sre.active` |
| Status `r0a` guard/denominator | FP `+0x102` | `mrf.phase` | `ecc.pwr_out` |
| Status `r0a` numerator | FP `+0x100` | `mrf.need` | `ecc.pwr_in` |

A complete status census covers 17 declared fields: sixteen written, one
untouched. No other mapping error was found. Constructor and component calls
for echo cancellation, timing recovery and FSE reception distinguish the
relevant fields without planting their values. At the proper aggregate link
boundary, the old source fails 80 of 560 new lifecycle checks; all 560 pass
after correction. The ring-response probe is separate: it models an internal
handoff without proving the preceding detector history, and remains labelled
synthetic. Its original source fails two of 77 checks; the correction passes.

The V34 audit is a useful bounded negative: nineteen diagnostic destinations
agree, including calibrated x87 stores at `+0x70` and `+0x74`, and fourteen
public getter bodies were reviewed, eleven already exact. Dropping the known
float store makes the audit fail. This does not prove arbitrary aliases,
delegated outputs or lifecycle reachability; no V34 field correction follows.
Replay `tools/recovery_v34_status_owners.py`.

## Keep the reference's link boundary when testing adjacent-data reads

The V32 lifecycle test initially remained red after all four status operands
were corrected. Equal component inputs reached `FPM_log10(64716, 0)`, whose
index 378 reads beyond its declared 128-word table into neighboring data.
The existing exhaustive logarithm fixture already requires aggregate linking
for this reason. Linking individual source objects changes those neighbors.

The new audit holds all 305 linked objects and the fixture constant. Individual
linking fails 79 of 560 lifecycle checks; aggregate linking passes all 560 at
the unchanged signal amplitude. At index 378 the individual source sees 26702
instead of 32381, explaining scaled results 417 versus 505. Aggregate source
and reference match the original across all 385 words reachable by unsigned
mantissas, plus one extra layout-guard word. The declared table stays 128 words.

`t_v32fpdisp.ldmode` therefore selects the established aggregate mode. No
arithmetic source, tolerance or input-amplitude workaround is warranted. The
existing log/div/sqrt period fixtures pass three binaries and 1217541 checks,
including all 65535 nonzero unsigned mantissas at exponent zero. Replay
`tools/recovery_status_log_link_audit.py` against the preserved aggregate and
individual binaries; retain the individual failure as a negative control.

## Recover a shared narrow intermediate without guessing its spelling

The native phasor routines narrow the interpolation fraction after both
`phase - (idx << 5)` and the odd-quadrant reflection `32 - frac`. A `short`
fraction models both boundaries in the shared helper. The period compiler
still folds the initial subtraction into `phase & 31` at `20.combine`, but
now retains the reference's sign extension after reflection in all three
functions. Their size gaps improve by one byte each: 11 for `FPM_phasor`,
12 for `FPM_phasor_dp`, and 18 for `FPM_phasor_demod`. None becomes exact.

A four-cell phase-carrier/fraction-width cross and one HI-mode mechanism
control delimit the result. The unsigned-phase control changes behavior for
negative phases and remains diagnostic only. The retained change preserves
all nontext section contents and exported bindings. The focused period
fixtures pass 3342336 phasor checks, 4456448 demodulator checks and 1005941
double-precision trials; all 36 mutation anchors remain valid. The ordinary
fraction range makes narrowing redundant at runtime, so these tests do not
uniquely identify a `short` local rather than casts on both assignments.

[The phasor audit](../phasor-fraction-width-audit.md) records the complete
cross and replay command. The initial-subtraction difference now needs new
compiler-context evidence; further nearby spellings are not a fresh lead.

## Cross helper visibility against translation-unit context

`VPcmV34GetDiagnostics` contains the standalone SNR algorithm in the reference,
including both reciprocal loops, but the reconstruction called a definition
in another translation unit. Cross two factors before moving an entire module:
whether diagnostics shares the complete Main TU, and whether the SNR definition
is visible to its caller. Keep every public definition exactly once.

| Context | Definition unavailable | Definition visible |
| --- | ---: | ---: |
| Diagnostics separate from Main | SIZE40, helper call | SIZE2, inlined |
| Diagnostics merged into Main | SIZE47, helper call | SIZE5, inlined |

Definition visibility controls the inline boundary in all four cells. Only
the diagnostic caller changes across the 59 exported function bodies;
the standalone SNR body remains exact with strong public binding. Allocated
nonstring data and exported bindings agree. The retained change moves the
single SNR definition beside diagnostics, preserving the separate Main TU.
This supports shared definition visibility; it does not identify the original
source filename within the `dp_wrapper.c|V34.c` attribution ambiguity.

Two additional cells remove the persistent rate-configuration pointer, since
the native body addresses those fields directly from the root owner. This
restores that addressing feature but also changes early captures and moves
round-trip-delay publication ahead of the SNR loops. The native publication
is later. No independent source history settles those tradeoffs, so the
owner rewrite remains diagnostic. SIZE2 describes a length gap, not two
isolated wrong bytes: operand and scheduling differences remain throughout.

Both production objects raw-match the selected complete-TU candidate. The
focused period fixtures pass 108775 checks across two binaries; their planted
component states do not establish modem-lifecycle reachability. Ten Main-bound
mutation suites contain 450 anchors, none overlapping the moved definition.
No registry reanchor is needed. Both existing SNR caller mutants are executed
and caught; the restored production candidate passes. The nineteen-output
field audit still passes, including its missing-float-store negative control.

Replay `tools/recovery_v34_diag_visibility.py` against a frozen baseline with
`--baseline-dir`, then `--audit`, `--period` and `--mutations`. The source pin
and immutable baseline manifest prevent a later rebuild from silently replacing
the control. An initial helper-only cell omitted the public `extern "C"`
prototype; the missing-export assertion rejected it. That run is retained as
invalid, and the valid cells include the public declaration.

## Bound historical field audits explicitly

The [typed-owner worksheet](../typed-owner-conversion-audit.md) follows up the
V32 regression with forty selected sequence/control substitutions and five
known-error controls. Pairing uses statement role before offset comparison.
The period probe checks offset, width and signedness for 35 unique member
paths; forty ordinary statement occurrences share thirty of those paths.
No additional address mismatch was found in this sample. Two tap-position
reads changed from unsigned accessors to signed members, but subtraction
followed by conversion to short preserves the same target low sixteen bits.
The signedness difference is recorded without claiming a functional defect.
This is a bounded historical audit, not coverage of all substitutions in the
commit. Replays use this checkout's recorded compiler configuration, rather
than assuming a particular worktree nesting or borrowing another build.

## Use native calls to test a suspected compilation boundary

Bell 103's merged source made four signal helpers available for automatic
inlining, while the reference's half-duplex states call them. Splitting carrier
detection alone recovers `RxHdxStartB103` byte-for-byte; splitting all four
helpers also recovers `TxHdxDataB103`. These are 139 and 54 original bytes,
respectively, with no exact losses across the seventeen-function pair.
The first call-graph dump records the unwanted inlining; the separated source
retains the native calls in initial RTL under unchanged compiler options.

The four controls distinguish merged source, carrier-only separation and
four-helper separation in two emission orders. Helper order does not change
the per-function results; the retained source follows the native order.
Static dispatcher buffers remain with their proven owner. The existing
`B103int.c` slot is a plausible destination, but no local symbol assigns the
four globals to that filename: retain this attribution limit alongside the
stronger call-boundary evidence.

An additional receive-data count correction follows actual use timing:
capture the demodulator's low word as unsigned, then sign-extend only at the
short return. Its size gap grows relative to the original merged function,
while the native calls and narrowing boundaries return. Keep that partial
result explicit rather than restoring unwanted inlining to improve a length
score. Transmit marks and silence narrow their gaps to three and two bytes.

[The Bell 103 audit](../b103-helper-boundary-audit.md) records all seventeen
bodies, nontext data and relocations, public bindings, replay controls and
four passing period fixtures. Searching both source and test names finds no
registered mutation suites for this family; there is no mutation coverage
claim or reanchoring to report.

## Recover helper structure before blaming an inline threshold

`TimingV34` called `setTimingStateParameters` in the reference, but the
reconstruction inlined it. The helper's comment explained why: two native
role-specific switches had been collapsed into one switch with conditional
values and a shared reporting flag. That simplification changed the compiler's
cost estimate. The current helper has 66 front-end instructions and is cloned
three times; the native two-switch/direct-report form has 118 and retains the
reference's calls under the same flags.

Earlier role-control artifacts had already shown that direct-report switches
restore the calls. The new experiment credits that finding and crosses it
with unchanged-helper visibility: moving the unchanged definition below its
caller changes nothing, while placing it in a separate TU restores calls but
leaves the helper graph unchanged. Recovering both native switches improves
both bodies, leaving SIZE4 in `TimingV34` and SIZE7 in the helper. The source
structure, native tables and report sites support this choice; the speculative
separate-TU cell remains diagnostic. Neither size gap is an isolated number
of wrong bytes.

Across 53 bodies, only the helper and its caller change; public exports are
preserved. The two actual relocated jump tables cover eighteen entries.
Executing original and candidate helper instructions for 150 synthetic
role/state/offset cases agrees on four output fields and external callback
arguments. Redirecting case 2 to case 0 is rejected in five cases. Unrelated
rodata and relocation consumers remain identical after accounting for the
target switch tables. This apparatus checks table semantics, not reachability.

Three focused period cells each pass 164106837 checks, including captured
Timing transcripts. One existing mutation anchor becomes ambiguous because
the new switches contain the same short default-arm text. Extend its anchor
with the preceding rate case while preserving the exact fault. The production
fixture passes 45805 checks; the mutant fails two of 45803. All 1644 anchors
in fourteen same-source suites remain unique. The mutation driver's actual
matching behavior is the authority: its `fn` annotation does not scope the
replacement, even though it looks as if it might.

`tools/recovery_v34_timing_visibility.py` retains the four complete-TU controls,
table negative control, mutation replay and immutable baseline manifest.
A fresh replay after source adoption reproduces all four earlier objects.

These three families distinguish superficially similar inline mismatches:
V34 diagnostics needed its helper definition visible, Bell 103 needed helper
separation, and V34 timing needed the original helper graph. A call inventory
must include tail jumps and distinguish an absent callee from fewer shared
call sites before nominating a visibility experiment.

## Finish validation of supported partial recoveries

A closed source-search domain does not mean its best supported candidate was
validated and rejected. The earlier Jd CRC investigation retained a native-like
expanded/direct-member endpoint but did not run its period or mutation gates:
zero whole-function exact gains had stopped adoption. Reopen the missing
validation, not the exhausted source palette.

The two-endpoint replay restores both native wrapper calls, the 0x48 packer
frames, and the sixteen CRC word promotions. Both packers reach their native
665/681-byte lengths and instruction counts, with BYTES255/BYTES259 remaining;
the getters shrink to their native 22 bytes, with BYTES1/BYTES4 remaining.
These are partial graph recoveries, not four new exact functions. Of twenty-one
TU bodies, four change and seventeen remain fixed; twelve exact names survive.
Public bindings, nontext data and unrelated relocations remain unchanged.

Both endpoints pass 99446 period checks. Eight affected mutation rows are
reanchored, splitting two shared tap faults by packer; all ten affected mutants
are caught. The sibling suite's 51 anchors remain unchanged. The durable
`tools/recovery_v92jd_crc_retention.py` replay reproduces the adopted object.
Compiler cost changes explain the wrapper boundary, but do not reveal the
unknown original source's cost. Keep that distinction explicit.

The subsequent Bell 103 controls also recover useful partial structure after
whole-function gains. Unsigned count capture and native branch/result sharing
make marks, silence and receive-data exact, bringing the family to thirteen
of seventeen exact bodies. Direct receive-owner reloads reduce `RxDetMarkB103`
to its native 238 bytes with six differing bytes. Short outer-loop indices,
unsigned limits and direct circular-index publication reduce the modem's size
gap from 61 to eight bytes. A later owner-capture variant is measured and
rejected; it still fails to recover the reference's per-iteration loads.
These controls preserve the other family bodies and pass all four focused
period fixtures. The remaining register and alias-context differences need
new evidence, not another arbitrary spelling palette.

## Historical mutation evidence and its limits

The following recorded guidance was written while mutation tooling existed.
Its examples remain historical evidence; the current tree uses differential,
code-generation, transcript and conformance checks.

## Validate configuration capture at existing reset boundaries

`V22_FSE_init` reads both coefficient-owner pointers before publishing either
in the reference. Capturing the two-field configuration restores that ordering;
the old reconstruction interleaved a publication and the second read. Replaying
only the previously measured baseline/copy endpoints narrows SIZE7 to BYTES5 at
the native 372-byte length. The five residual bytes encode register choices;
the other three TU bodies, data, bindings and relocation consumers stay fixed.

The ordering detector accepts native/copy and rejects the old body. Existing
period fixtures pass 21570 checks per endpoint, including reference-warmed
constructor graphs whose reset reuses the equaliser as its own configuration.
That boundary does not justify arbitrary partially overlapping configurations.
The existing I/Q-swap mutant is preserved and caught after reanchoring; no new
mutation campaign was needed. Production raw-matches the audited candidate.
[The V22 capture audit](../v22-fse-capture-audit.md) records replay provenance,
actual commands, all four body verdicts and the remaining register differences.


## Preserve an unsupported guard as an explicit nonbug fallback

`FPM_TONE_create`'s post-allocation NULL return was absent from the reference.
The earlier removal control had been withheld because its length gap grew
from 84 to 90 bytes without an exact gain. The native allocation branch is
stronger evidence: restore its unchecked path under `DSPLIB_REPRODUCE_BUGS`
while retaining the defensive return for normal builds. This changes only the
constructor among eleven audited functions; seven exact bodies remain exact,
and nontext data, bindings and unrelated relocations are unchanged.

Three focused period fixtures pass through successful constructor and owner
histories. There is no reason to manufacture an allocation-failure crash just
to obtain another test result. All 41 existing source anchors remain valid;
no mutation execution or registry change is needed for this refinement.
[The tone allocation audit](../fpm-tone-allocation-boundary.md) records the
witness, bounded two-endpoint replay and the intentionally larger size gap.


Issue #296 recorded the historical mutation-policy follow-up. The mutation
tooling has since been removed; the period differential and native evidence
remain the reconstruction checks.


## Explain exact-set losses at the first differing compiler stage

`DemodDataV32` passes an unsigned short count to the MRF's integer argument in
the reference. The reconstruction's explicit short cast changed the outgoing
word's extension. Removing that cast restores the observed MOVZWL and full-word
argument store. Valid existing period fixtures compare the operation without
inventing unsafe high-count histories.

The unchanged `SetAdaptEqV32` loses byte identity in the same TU. Its normalized
period RTL agrees through `27.flow2`; `28.peephole2` first chooses opposite
CX/DX scratch registers for constant-one/constant-zero stores. Record those
seven differing bytes as compiler-state collateral. The source contract has
independent native evidence; an exact-membership ratchet should expose this
tradeoff, not require retaining the incorrect narrowing. No allocator flags,
register annotations or local spelling search is justified by that loss alone.


The V32 replay audits all twenty-four bodies, including the two other changed
non-exact methods; twenty stay fixed. All three existing focused period fixtures
pass 27045 checks per endpoint. [The count-boundary audit](../v32-mrf-count-audit.md)
records the exact-name loss, full data/relocation collateral and thirty period
RTL streams per unchanged method. The existing wrong-owner fault anchor is
updated mechanically; no mutation execution is needed to establish this result.


## Separate API bounds from storage extent

A checked accessor establishes which indices it exposes, not necessarily the
size of its backing array. The V32 constructor clears seven consecutive words
at offsets +0x3c through +0x48, while native `LoadReg` and `StoreReg` accept
only indices 0–4. This permits a five-element array followed by two fields,
a seven-element array with two private entries, or an incorrect API limit.
Our `regs[5]` declaration makes the reconstructed seven-iteration loop exceed
its declared bounds; it does not establish which original model is correct.
Review all producers and consumers before changing the layout or API, and
record uncertainty when the binary cannot distinguish the source models.

## Recover arithmetic signedness and reload timing together

`dp_wrapper_run` uses three unsigned minima, two unsigned remainders and an
unsigned fragment comparison in the reference. It also reloads the fragment
size after each external copy. Restoring these operations changes its length
gap from two to five bytes, but removes independently observed instruction
and value-timing discrepancies. Keep the public positive-count guard signed.

A shared-field type change requires measuring every header consumer. The
wrapper replay checks nine translation units and 55 emitted bodies per
endpoint; eight non-wrapper objects remain raw-identical, all thirty exact
entries survive, and data, bindings and relocation consumers stay fixed.
The existing component lifecycle fixture passes 33686 checks per endpoint
and in production. [The wrapper audit](../dp-wrapper-unsigned-audit.md) records
the complete commands, read-age detector and signed-baseline negative control.


## Check a shared helper in both production and its direct fixture

The V34 snapshot index refinement changes one declaration in a header included
by production and a direct component fixture. Both must receive the selected
header. The two six-entry loops now use signed-word operations seen in the
reference; the arithmetic temporary remains an integer. Audit every emitted
body, including helpers with no same-named native counterpart. Here all nine
exact functions survive among 53 production bodies, and the fixture's selected
object also reproduces production bytes.

Changing helper size can move call and jump-table destinations without changing
their meaning. Resolve each affected relocation to its owning function and
interior offset before normalising it. The snapshot recovery changes its parent
by 24 bytes and one dispatcher call-displacement byte; all other dispatcher
bytes and canonical nontext destinations agree. Raw nontext equality would be
an inaccurate claim. [The snapshot audit](../v34-snapshot-index-recovery.md)
records the complete comparison and the corrected fixture `__FILE__` control.


## Cross layout hypotheses with the observed operation

V32's five-element array plus two fields and seven-element backing array are
indistinguishable across fourteen period translation units. Keeping the API
limit at five, both loop forms produce the same objects; both explicit-store
forms also produce the same objects. This experiment cannot recover the
original declaration, but it separates that uncertainty from initialization:
the native constructor emits seven ascending word stores, whereas the loop
forms emit a loop.

Use the explicit stores with the current layout to recover the observed
operation without choosing an unsupported declaration or changing the API.
That also removes indexing beyond the currently declared array. The two
constructor-backed fixtures pass 689769 checks in total, and production
raw-matches the previously validated explicit-store object. The constructor
grows sixteen bytes; the other five emitted function bodies, nontext contents
and canonical relocation consumers remain unchanged. The other thirteen
header-consuming translation units remain raw-identical. Do not describe this as
proof that the original bank had five or seven entries.


## Prove that an overlay header reached the compiler

A raw-identical baseline cannot detect an ignored candidate header. The first
promoted V32 storage replay used `-I.../overlay` instead of
`-I.../overlay/include`; the compiler silently fell back to the live header.
Its apparent four-cell pass was invalid. The seven-element explicit cell also
retained names of members removed by its intended header, a contradiction that
should have caused compilation to fail. Preserve such a run as invalid and
exclude its verdicts rather than adjusting the interpretation afterward.

Check the actual preprocessed declaration or a period-compiled layout witness
for every cell, alongside dependency paths. Here the array must occupy ten
bytes in the five-element cells and fourteen in the seven-element cells; the
latter must not expose the removed named fields. Deliberately using the wrong
include path must fail that witness. Copying pinned headers into a directory
is not evidence that the compiler read them.


## Cross a sample cursor with the sample's read boundary

The V8 DFT reference publishes the bin phase before loading the current input
sample, and advances the sample pointer by two bytes in the outer loop. The
reconstruction used an indexed sample load before phase publication. A
four-cell cursor/read-order cross separates these two discrepancies: each
single-axis control restores only its own native operation, while the
combined candidate restores both. The ordering difference is already present
in initial RTL; register allocation did not introduce it.

Keep the sample read inside the bin loop. Hoisting it would introduce another
unsupported value-capture boundary. The existing component fixture passes
1081 DFT checks, including zero and nonzero counts and bin counts; this is
not evidence for arbitrary aliasing or unsafe large counts. All three emitted
bodies are audited per cell; only the update changes, with both exact
siblings, table data and bindings preserved. Its size gap improves from
fifteen to seven bytes, but the remaining inner-loop and extension differences
are recorded rather than dismissed as register allocation.

[The V8 read-age audit](../v8-dft-read-age-recovery.md) records source/header
pins, actual compiler dependency output, both negative controls and the
complete selected command. Production raw-matches the selected replay object.


## Preserve replay ancestry when rebasing

Source-recovery tools may pin commits for both source extraction and accepted
collateral evidence. Before rewriting a published investigation branch, retain
its old tip under an explicit archive tag and push that tag with the rebased
branch. Verify the committed source, headers and period compiler inputs are
unchanged by the rebase, then rerun the applicable gates on the integrated
base. A successful cherry-pick does not establish those properties by itself.

Base-pinned loss reviews also need revalidation. PR293's new master base
changes mutation policy without changing reconstruction or period inputs;
the reviewed PreFilter fingerprint stays identical. Updating that base pin
is justified by those checks, not by disabling or widening the exactness gate.


## Cross independent native operations before judging a near-size body

The tone filter has two independent native witnesses: a word-sized old-count
test and a conditional ring increment that GCC converts to SETL/NEG/AND.
A four-cell source cross separates their effects. The baseline is 238 bytes,
the ring-only cell 233, the countdown-only cell 258, and the combined cell
236 against the native 235. Both operation witnesses, rather than size alone,
justify the combined source. A one-byte length gap still hides register,
stack-home and scheduling differences throughout the body.

Keep zero and wrapped negative counts in the component validation when the
native word counter admits them. Existing constructor-backed tone fixtures
already supply enough storage for the minus-one case; no impossible history
or unsafe buffer is needed. The full period gate passes after adoption,
and the other ten TU bodies and their relocations stay unchanged. See the
[tone ring/count audit](../fpm-tone-ring-count-recovery.md) for the crossed
controls, header-resolution proof and remaining differences.


## Recover shared continuations before expanding helper closures

A small reconstructed root can hide substantial native code in artificial
helpers. Moving one helper back into the root can evict others through the
period compiler's inline budget, so inspect the whole call graph and first
RTL instead of treating the root's size as an isolated score.

V34's diagnostic closure expansion made this visible: ordinary body expansion
created 104 copies of the same transmit dispatch, for 106 tables overall.
Sharing the terminal continuation restored the native three-table shape and
reduced the root from 149526 to 66889 bytes against native 61541. This is
structural evidence, not permission to adopt the generated macro expansion.
A production implementation still needs clear ordinary cases, one physical
body home, direct fixture access and a reviewed root-lifetime frame.

Debug text is a separate witness. Resolve the outgoing format argument through
the control flow, including register clobbers and unknown indirect
predecessors. A nearby literal or matching unique-string set does not prove
that the right message is called on the right path. V34's shared candidate
retains all 110 unique texts but has 286 debug calls against native 262;
the remaining multiplicities expose cloned blocks and inline boundaries.
Known wrong formats and removed argument assignments must make the detector
fire. See the [V34 placement audit](../v34-handshake-body-placement-audit.md)
for the bounded controls and the limits of argument resolution.


## Keep local and CI loss reviews consistent

When an independently supported source correction has explained exactness
collateral, updating the local exact baseline is only one part of the review.
CI separately checks losses against the PR base using a base revision, current
body/relocation digest and immutable evidence commit. Record the accepted loss
there too, and demonstrate rejection without the review and acceptance with
it. PR293's V32 unsigned-count correction initially missed this second record;
its existing period and peephole evidence justified the record without changing
source or weakening the comparator. Revalidate these pins when rebasing.


## Recover the predecessor graph before interpreting scratch colour

The November 5 GCC campaign recovered both `V92Jd` vector getters by combining
two independently evidenced properties: native function emission order and
constant-index CRC shifts in both unpackers. The blob promotes sixteen CRC
words across the bit loop; the reconstructed indexed shift loop prevented that
promotion. Expanding the fifteen shift assignments restores the native
80-byte frame and reduces the unpackers' size gaps from 455 and 413 bytes to
43 bytes each. The packers already used this form.

Order and source structure must be reviewed together. Native order alone did
not recover the getters with the old predecessor bodies. The combined change
gains both getters, retains all twelve previous exact bodies, and changes six
of the TU's twenty-one bodies. Two packers change first at peephole2; their
earlier instruction graphs are unchanged. The final capture contains forty
supported SI scratch searches. A model calibrated on the old instruction
graph cannot simply predict a new graph's searches.

Moving functions also moves anonymous jump-table targets. Raw allocated-data
inequality is therefore a prompt to audit relocation meaning: all nineteen
entries remain within the same two unpackers, and canonical data, bindings and
unrelated consumers are unchanged. Native-machine probes agree on 564
synthetic component cases; deliberately wrong table entries fail in 48 cases.
These probes supplement the 99,446-check period fixture, whose reset and framed
stream cases provide component lifecycle coverage. They do not establish
public modem reachability for planted decoder states.

Two smaller recoveries illustrate the boundary of the scratch model.
`_iir_filter_create` becomes exact after publishing `n_a_minus_1` before `n_b`
in source. Both count values already remain live through the coefficient
copies. The complete two-order experiment changes only this 195-byte body;
the sixteen-event TU scratch trace is identical and the function has no
scratch searches. Its register difference belongs to ordinary allocation.
The final machine store order still differs from the source statement order,
so exact output establishes a valid preimage, not unique original spelling.

`RxHdxTone` instead needed an owner capture: retain the detector's short
result, reload `modem->hdx`, then test the result and dispatch through the
captured owner's mode. The native instructions place that reload before the
branch. Keep the subsequent reload after the optional callback too: the
callback can replace the owner. This recovers the complete 246-byte function
while retaining the other thirteen bodies. Focused period controls pass
806,302 checks for the IIR consumers and 3,686 for the HDX fixture; the latter
includes owner replacement during the callback.

Reproduction evidence is under `build/gcc3-wave-jd`, `build/gcc3-wave-tu`,
`build/gcc3-wave-fse` and `build/gcc3-overnight/checkpoint1`. Compare each
candidate against the frozen 300-object campaign baseline before interpreting
the aggregate exact count.

The combined checkpoint passes `make phase`: 393 period tests, zero failures.
Fresh production comparison covers all 1,852 functions and moves EXACT from
1,110 to 1,114, with no exact losses; REGALLOC falls from 31 to 29.


## Keep storage width separate from report access width

`V17RX_status` and `V29RX_status` read the low sixteen bits of their
configuration's protocol word. Their constructors copy all thirty-two bits.
Representing both operations needs a four-byte storage member and an overlaid
two-byte report view at offset zero; shrinking the storage member would lose
caller data. Period layout assertions cover both widths, the following
bit-rate field, and the unchanged configuration sizes.

The same functions provide positive native evidence for byte bitfields. Their
flag publications include SETE followed by shifts of three and five, with
intermediate stores observable through aliases. A one-byte bitfield view at
report offset `0x14` reproduces those publications, including the adjacent
byte clear's interleaving. Both status functions become exact at 190 bytes.
This evidence is specific to these writers; mask-based flag operations
elsewhere do not acquire a reason to change.

Check the complete header consumer closure. Here, 56 production TUs include
one of the three changed headers: 54 remain literal object matches, and only
the two status bodies change in the other two. The closure contains 284 sized
functions and four data-only TUs. Record the latter's raw object equality
explicitly rather than dropping them from the denominator. Exported bindings,
allocated data and function order are unchanged across all 56 consumers.

The new `t_faxstatusprotocol` fixture constructs both receiver types with two
distinct upper-word patterns and the default low-word protocol. It checks
complete configuration storage and the low-word status report before deleting
the receivers. This is a component storage-width regression, not a claim that
arbitrary upper bits are public modem configurations. Together with the two
existing receiver fixtures, baseline and candidate each pass 219,379 focused
checks. Existing alias and planted-state cases remain labelled component
fidelity probes.

The neighbouring `_delete_data_rx_modem` recovery restores a different native
boundary: capture the unsigned sixteen-bit slot, and keep the final config
free in each dispatch arm. The V.17 arm reloads its owner after every opaque
free; both arms converge at the actual free call. This recovers all 149 bytes
without changing the other four TU bodies. Baseline and candidate each pass
563 focused checks, including real receive construction followed by a
modulation-switch teardown. Two full 300-object partial links retain all
1,881 other selected bodies. Executable byte/target checks confirm the tested
functions are the intended implementations; eight deliberate corruptions are
rejected.

Historical commits `ce8753952c` and `a8ea56c16e` supplied discovery leads.
The current native instructions, frozen baseline, consumer closure and fresh
period tests establish these results independently. Reproduction evidence is
under `build/gcc3-wave-fax`, `build/gcc3-wave-class1rx` and
`build/gcc3-overnight/checkpoint2`.

The combined second checkpoint passes `make phase`: 394 period tests, zero
failures, including the new storage-width fixture. Fresh production comparison
records 1,117 EXACT and 29 REGALLOC of 1,852, with three gains and no losses
against the first checkpoint.


## Follow native values into callees when a green fixture stops too early

The `_init_receiver` instruction review found a missing functional effect.
The blob captures a short-training flag for V.17 rate codes `0x4a`, `0x62`,
`0x7a` and `0x92`, then writes that four-byte value into the reinitialisation
request at offset `0x10` (`.text+0x94771`). The reconstruction recognised the
same codes only to print a debug message and left the request field zero.
`V17RX_control` copies it to receiver configuration offset `0x14`, and
`V17RX_create` propagates it into the private control's offset `0x10`.
Period layout checks confirm all three owners, offsets and four-byte widths.

The existing 451 checks passed because the reinitialisation fixture compared
the outer context and a few propagated fields, excluding both short-training
destinations. Extend the fixture along the existing component lifecycle:
construct at a long-training rate, request short, long, then short again,
and tear down. Cross all four legal rate pairs with debug levels zero and two.
The 32 calls provide 192 new checks without planting internal state. They
exercise the Class 1 configuration boundary; they do not establish a completed
physical long-training signal history.

The unchanged source fails 64 of the 192 new checks. Capturing the flag and
assigning `req.short_train` passes all 643 checks, while preserving the other
four exact TU bodies, data, exports and function order. Executable provenance
checks cover both selected source and native implementations and reject four
deliberate byte/call-target corruptions.

The correct body grows from 1,769 to 1,784 bytes against native 1,583; its SIZE
gap grows from 186 to 201. Retain the required behaviour and its regression.
A smaller incorrect implementation is not a reconstruction improvement.
Keep the separate initializer placement and shared-call-tail investigation
out of this functional correction. Evidence is under
`build/gcc3-wave-class1rx/short-train` and
`build/gcc3-overnight/checkpoint3`.

The functional correction passes the full `make phase` gate: 394 period tests,
zero failures. The focused test now contains 643 checks. Fresh production
compilation reproduces the reviewed candidate literally; exact membership
remains 1,117 of 1,852.


## Cross value lifetime with the native operation and function order

The next GCC campaign batch recovers seven exact bodies and one REGALLOC body
through small, explicitly crossed source domains:

| Body | Native evidence and retained source | Result |
| --- | --- | --- |
| `V21RX_control` | Capture the request flags before the HDX word store; use the equivalent positive-mask predicate for TEST/SETNE. | EXACT, 75 bytes |
| `V21TX_control` | Capture flags, then load the HDX owner at its native late position; use the positive-mask predicate. | EXACT, 94 bytes |
| `V29RX_control` | Use the positive-mask predicate on its existing flag snapshot. | EXACT, 110 bytes |
| `V29TX_control` | Capture flags and multiply the already-published PPS scale, preserving the native single request-scale load. | REGALLOC, 126 bytes |
| `check_for_valid`, `check_for_valid_easy` | Read `w[0]` directly and share the zero-result return path. | EXACT, 52 and 34 bytes |
| `dp_v8_init` | Restore its predecessor's native switch structure. | EXACT, 30 bytes |
| `V92CP::evaluateCRC` | Restore integer `abs`, initialise the byte accumulator after CRC clocking, and restore native strong-definition order. | EXACT, 668 bytes |

The masked predicates have nonnegative results over their complete declared
input domain, so `!= 0` and `> 0` agree. The compiler nevertheless emits
different operations. Flag capture is a separate property: the native RX
control retains the loaded byte across a store that can overwrite an aliased
request. `t_v21ctlalias` supplies aligned, bounded union storage for that
synthetic component probe and restores the original heap owner before
destruction. The old source fails one of ten checks; the candidate passes all
ten. This is supplementary alias fidelity evidence, not public modem
reachability. Four ordinary constructor/control fixtures pass 499,498 checks
per source variant. Their executed implementations are checked against the
candidate bytes and resolved relocation targets.

The Beepgen two-by-two experiment requires both direct reads and a shared
return to reproduce the native CMP orientation and terminal MOV. Each change
alone remains non-exact. Only the two helpers change among 23 bodies; the
focused fixture passes 178,693 checks, including 256 checks spanning all
equality combinations of the two window shapes.

V.8 needs explicit empty cases for statuses 2, 7 and 9 plus the default
zero-result assignment. This restores all eighteen native jump-table target
classes. An explicit `V8_INIT` label is unnecessary: removing it produces the
same object, so do not claim it as recovered spelling. The process body stays
SIZE33, but its full-TU scratch stream changes from 24 to 19 searches. The
unchanged registration function consequently enters at cursor 52 instead of
1 and selects the native register. Three period fixtures pass 252,552 checks,
including real handshake histories; status 7 appears in those histories,
while 2 and 9 were not observed at that boundary.

V92CP's absolute-difference and late-initialisation cross first reaches
REGALLOC. Native strong-definition order then changes its QI scratch entry
cursor from 2 to 3, selecting DX instead of CX and recovering all 668 bytes.
The first calibrated model supported only HI/SI searches; the QI explanation
here comes from fresh read-only compiler traces. Do not silently generalise
the old model to this mode. Four period fixtures pass 38,834 checks, with the
exact body verified in each executable. The inline consumer `bitsToInfo`
remains non-exact and its size gap grows from 27 to 33; `float2Bits` retains
SIZE22 with changed scratch operands. The C2/C1 clone order still differs
from the reference, so this is function recovery rather than final-layout
identity.

Evidence is under `build/gcc3-wave-status`, `build/gcc3-wave-beep`,
`build/gcc3-wave-tu/v8-review`, `build/gcc3-wave-tu/v92crc-review` and
`build/gcc3-overnight/checkpoint4`.

V92CP's non-text audit has an important limit. Its 68-byte `.rodata` section
contains seventeen relocated table entries: six `evaluateInfo` targets retain
their function-relative offsets, while eleven `bitsToInfo` targets move within
that changed inline consumer. All entries retain their ordered function owner,
extent and instruction-boundary checks; the other allocated data sections and
named data records are literal matches. Strict function-plus-offset equality
is false for the moved eleven entries. The audit rejects a different valid
case, an interior-byte target and a wrong owner, but does not claim semantic
CFG equivalence. The generic table prover refuses both builds at an
unsupported ABI-data relocation. Record this limit alongside the source
equivalence and period evidence instead of declaring all data unchanged.

The fourth checkpoint passes `make phase`: 395 period tests, zero failures.
Fresh production comparison records 1,124 EXACT and 29 REGALLOC of 1,852,
with seven exact gains and no losses. The V.29 TX promotion adds one REGALLOC
while the V.8 registration promotion removes one. Eight affected mutation
anchors were updated; all 10,044 anchors across 285 suites remain structurally
valid. No mutation execution or snapshot refresh was needed.


## Retain supported partial graphs without claiming an exact gain

Three further campaign candidates restore native source properties while
remaining non-exact. Their adoption is separate from the exact-count batches.

* `V90Jd::unPackData` needs the same fifteen constant-index CRC shifts as the
  V.92 unpackers. This restores promotion of the sixteen words and the native
  80-byte frame. Its size gap falls from 354 to 18 bytes; all fourteen other
  TU bodies remain unchanged. Baseline and candidate each pass 50,583 period
  checks. The additional machine probes agree on 270 synthetic component
  cases, and deliberately corrupted table entries fail in 24 cases.
* `V32FP_create` updates bit ten of its options word through the native byte
  at parameter offset `0x11`, preserving every other bit, and publishes the
  independent configuration fields in native order. The source still reads
  the declared integer `cfg->r10` at offset `0x10`; GCC narrows its low-bit
  use to the native byte load. The raw byte gap falls from 101 to 13, with
  the other five bodies unchanged. Both captures contain 73 scratch searches
  and none in this function, so its residual is outside the cursor model.
  The constructor-backed fixture passes 106,013 checks, including 24 real
  configuration combinations. A lower-scoring operand commutation lacked
  independent evidence and was not adopted.
* The corrected `_init_receiver` has three request/template initialisation
  regions feeding one common rate/width/control-call continuation. Declare
  the three request objects in that continuation's enclosing scope so their
  lifetimes cover the call. Copy `FAXVMI_CTL` within each branch, after its
  request template. This restores fifteen native template-word loads and one
  `FAXVMI_control` call, replacing six loads and three calls. Its 50
  relocations match the native count; the size gap falls from 201 to nine
  bytes. Only this body changes among five, and all four exact neighbours
  remain exact. All four crossed source controls pass the 112-check teardown
  fixture and the corrected 643-check initialisation fixture.

Keep `_init_receiver`'s existing zero defaults for `mod` and `rate`. A native
prologue without explicit zeroing does not by itself prove uninitialised
source locals: caller-domain and optimisation evidence are separate
obligations. The diagnostic default-removal controls established neither a
valid input contract nor an exact match and were excluded. The forced
short-training store and its failing-baseline regression remain intact.

Evidence is under `build/gcc3-wave-jd/v90-constant-shifts`,
`build/gcc3-wave-fse`, `build/gcc3-wave-class1rx/common-tail` and
`build/gcc3-overnight/checkpoint5`.

The fifth checkpoint passes `make phase`: 395 period tests, zero failures.
Fresh production objects reproduce all three reviewed candidates literally.
Full-TU checks confirm only those three non-exact bodies change, preserving
exact and REGALLOC membership.


## Header alias effects and reset predecessors can recover neighbouring bodies

The sixth overnight checkpoint recovers three exact bodies without changing
any exact neighbour. `V22_status` becomes exact through the joint recovery of
native rate-branch orientation and typed flag views. The report has one-byte
views at offsets `0x14` and `0x15`; parameters retain their four-byte storage
at `0x10`. Period layout assertions and the measured byte SETcc/shift writers
support these bitfields specifically, not a general conversion of masks.

The header closure covers seven consumers: 44 functions in six code objects
and one data-only object. Five objects remain literal matches. Besides status,
only non-exact `v22_data` changes: its transmit-count store moves before the
parameter-bit test, matching native order. RTL first changes the memory alias
set, then scheduling; this is not a scratch-cursor effect. Seven focused
fixtures pass 2,058,993 checks per form. An additional 20-check synthetic
aligned-report-overlap probe passes both forms. A final-only publication
negative control passes the disjoint fixture but fails two overlap checks;
these count the same wrong byte through two compared objects. The overlap
probe claims component fidelity, not public modem reachability.

`V90Phase3Demodulator::reset` restores branch-owned modulator calls and the
native A-law fallthrough. Four argument regions merge into one emitted
modulator call; three paths retain `resetLinearMapping`, and WaitForQTS skips
it. The reset size gap falls from 85 to one byte. Its two subsequent constructor
variants become exact without source changes: their RTL first differs at
peephole2, where the zero scratch changes to AX. The C1 and C2 symbols are
separate native bodies at `0x212c0` and `0x21430`, not address aliases.
All other bodies among 26 retain their grades and bindings. Nonstring data
remains unchanged; the string pool reorders while preserving all 51 payloads
and 86 per-owner references across six functions. Focused reset/constructor
fixtures pass 72,021 checks per form.
Ten mutation rows were reanchored with their fault intent retained; no mutants
were executed and no snapshot was refreshed.

Evidence is in `build/gcc3-wave-v22status`, `build/gcc3-wave-p3d-reset` and
`build/gcc3-overnight/checkpoint6`. The full phase gate passes 396 period tests
with zero failures. Production comparison measures 1,127 EXACT and 29 REGALLOC
of 1,852: three exact gains, no losses. Only the three reviewed production
objects differ from checkpoint five.


## A prospective TU-state prediction closes V90MP evaluateInfo

The new QI/q calibration has a source-recovery use beyond replaying known
outcomes. V90MP's native strong-definition order puts `evaluateCRC` before
`getBitVector` and `evaluateInfo`. Freeze that complete order before compiling:
the six byte-register and eight word-register searches predict that CRC leaves
the cursor at 13, and `evaluateInfo` selects ESI for its dead stack-release POP.
The old order enters at three and selects EBX. Independent observation verifies
all fourteen predictions, including unchanged eligibility, and the 482-byte
function becomes exact. The residual instruction is at function offset `0x1de`;
the frozen prediction's prose said `0x1da` in error, without affecting its
register prediction or event inputs.

All fourteen member definitions retain their bodies. Fifteen of sixteen emitted
bodies retain their bytes after masking only validated relocation fields and
retaining canonical targets. Five table entries preserve their function-relative
offsets. The string pool grows from 373 to 374 bytes through layout padding;
its payload multiset and all consumers remain equal. Wrong-table-entry and
changed-string controls both fail. Raw object layout is therefore different,
and C2/C1 emission order still differs from native. Exact function recovery is
not a claim of a fully exact object.

The parent independently repeats the body, prediction and allocated-data audits.
Focused fixtures pass 18,051 checks in 32 groups per form, with the selected
482-byte body proved present in each executable. Production reproduces the
reviewed object literally, and the other 299 objects remain unchanged. The
seventh checkpoint passes all 396 period tests and the structural phase gate.
The census reaches 1,128 EXACT and 28 REGALLOC of 1,852, one exact gain and
no loss. Evidence: `build/gcc3-wave-tu/v90mp-review`,
`build/gcc3-wave-tu/v90mp-order` and `build/gcc3-overnight/checkpoint7`.


## Recover arm ownership, partial writes and reference publications

The eighth checkpoint combines two exact gains with a supported decision-method
recovery. `init_vmi_v27tx` writes only the low sixteen bits of the copied FIFO
factor at configuration offset `0x14`. A union retains its four-byte storage
and exposes the native two-byte override. The 248-byte constructor becomes
exact. The closure covers seventeen consumers and 103 functions; sixteen
objects are literal matches and only this constructor changes. Existing period
fixtures pass 110,373 checks per form. A ten-check synthetic writable-template
probe preserves a nonzero upper word, failing two checks on the old source and
passing after correction. It tests the configuration constructor, without
instantiating a pump from the artificial factor.

`v8_process` restores two arm-owned outgoing argument stores and the shared
emitted call tail, removing the post-switch sentinel test. Its status store is
unconditional, as the native unchanged-status branch targets that store. The
size gap falls from 33 to three bytes; all eighteen dispatch-table entries now
have the native function-relative targets. Subsequent `dp_v8_exit` becomes
exact through a changed incoming scratch cursor, while `dp_v8_init` stays
exact. Six source controls bound the result without establishing a unique
spelling. Baseline and candidate each pass 252,552 focused checks in fifteen
groups, with linked body and relocation provenance verified.

Both phase-three decision methods now call the existing `twoLevelDemod`
helper instead of duplicating its body. Automatic inlining preserves eighteen
native decoder-result publications through its integer reference argument.
The remaining native `isAltRbs` tests also narrow to sixteen bits, giving all
nineteen AX tests. Only these two of 26 bodies change; both constructor
variants remain exact. Size gaps fall from 241 to 117 bytes and 427 to 238
bytes. Focused period fixtures pass 3,221,079 checks per form, including
constructor lifecycle coverage and explicitly synthetic decision-state sweeps.
The V.92 local bit initializer remains unchanged.

For these decision methods, table ownership is not exact target identity:
41 of 68 function-relative table offsets change, although all owners remain
the same. Linked-provenance checks verify each selected object's own table
entries, not equivalence between different case graphs. The shared CFG/frame
prover still refuses unsupported instructions; no proof requirement is weakened.
The string pool also changes ordering, so neither literal nontext equality nor
strict table-target equality is claimed. Period differential evidence and the
native publication/width witnesses support this partial recovery.

Four V.8 mutation rows were reanchored: two stale matches and two formerly
shared-call faults that must now affect both source arms. All 10,044 anchors
remain valid across 285 suites, without mutation execution or snapshot refresh.
The first phase run passed all 397 period tests but failed the two stale
anchors; the corrected complete rerun is recorded with this checkpoint.
Evidence: `build/gcc3-wave-vmi27`, `build/gcc3-wave-tu/v8-cfg-review`,
`build/gcc3-wave-p3d-decision` and `build/gcc3-overnight/checkpoint8`.

The corrected phase rerun passes all 397 tests and structural checks. The
census is 1,130 EXACT and 27 REGALLOC of 1,852: two exact gains, no losses.
Parent review verifies unchanged string-payload and per-function string-consumer
multisets for the phase-three candidate; raw string ordering remains different.


## Follow a measured width correction through its sibling constructors

The V.27 transmitter override has direct V.17 and V.29 counterparts: native
MOVW stores at `0x948fc` and `0x949fc` overwrite only configuration offset
`0x14` after complete template copies. Both configurations retain their
four-byte FIFO factor and gain a two-byte union view. V.29's 241-byte VMI
constructor becomes exact. V.17 reaches its native 247-byte size but retains
thirteen differing bytes; recovering its access width is the reason to keep
it, not the smaller size gap.

The two-header closure covers 38 consumers, 218 shared functions and three
data-only objects. Thirty-seven objects are literal matches; only the VMI
constructor object changes. The already exact V.27 constructor remains exact.
All three relocated code pointers retain their exact function-relative targets;
masked data payload and layout are equal. Five existing fixtures pass 281,144
period checks in 61 groups per form. A separate twenty-check synthetic template
probe fails four checks on the original reconstruction and passes with the
native partial writes. It restores the writable templates before proceeding
and never instantiates a pump using the artificial upper word.

`FDSP_DP_Run` now calls its two existing conversion helpers. Their automatic
inlining restores the indexed input loop, advancing output loop and native
gain lifetimes. A four-cell input/output cross changes the raw byte gaps from
108 through 111 and 17 to one. The combined endpoint is REGALLOC: only the
final dead stack-release POP differs (DX versus native CX). The other 22 TU
bodies, allocated data and symbol bindings remain unchanged, including the two
recently exact validity predicates. Focused fixtures pass 181,092 checks in
21 groups per form. Primary evidence uses eight valid sample counts from zero
to 200 and transmit values within `[-1,1]`; the supplementary voice fixture's
planted context and out-of-short-range products remain exploratory evidence.
Linked-body checks and deliberate status corruptions confirm the intended
candidate executes. Two scale mutation anchors now target the helper arguments.

A fresh capture verifies all 53 scratch searches. The inherited entry cursor
49 selects DX; this does not justify rearranging source solely to obtain CX.
Likewise, `FDSP_DP_Delete` can select native EBX only for incoming cursors
3..12 under its captured eligibility, but no native predecessor stream is
established. Native FILE records place Run and its helpers in a thirty-global,
five-TU attribution bracket. Adjacency to Delete and duplicate constant-pool
slots do not prove they shared a TU. No speculative regrouping was applied.

Evidence: `build/gcc3-wave-vmi1739`, `build/gcc3-wave-fdsp-state`,
`build/gcc3-wave-tu/fdsp-helper-review` and
`build/gcc3-overnight/checkpoint9`. Production matches both reviewed candidate
objects literally; the other 298 objects remain unchanged. The ninth checkpoint
census is 1,131 EXACT and 28 REGALLOC of 1,852: one exact and one REGALLOC gain,
no exact losses. Its first phase run passes all 398 period tests but identifies
two stale scale anchors; the corrected complete rerun is recorded separately.

The corrected ninth-checkpoint rerun passes all 398 period tests and the
structural phase gate. All 285 mutation suites retain valid anchors; no
mutation execution or snapshot refresh was performed. The campaign retains
21 additional EXACT symbols and 4,116 additional exact reference bytes from
its 1,110-symbol baseline, with no exact losses.


## Capture the complete wave as reusable method, not only winning counts

The November 5 campaign starts at `212b5df19` with 1,110 EXACT and 31
REGALLOC symbols. The ninth checkpoint reaches 1,131 EXACT and 28 REGALLOC
of the same 1,852 symbols, gaining 4,116 exact reference bytes with no exact
losses. These are relocation-aware function-body grades, not complete-object
identity. The detailed sections above retain the individual witnesses,
controls, failed endpoints, fixture domains and data-layout qualifications.
The following synthesis covers the whole wave, including improvements that
remain non-exact.

| Reusable pattern | Wave evidence | Evidence needed before transferring it |
| --- | --- | --- |
| Recover the predecessor's source graph before chasing a later register | V.8 switch-arm ownership recovers registration wrappers; phase-three reset structure recovers both constructor variants. | A native branch/call/store witness for the predecessor, then full-TU comparison and the first differing compiler stage in each affected neighbour. |
| Preserve value publication and reload timing | Tone-IIR allocation counts, RxHdxTone's owner reload, V.21/V.29 request snapshots, and V92Jd's lookahead byte. | Track owner, offset, width and intervening calls/stores. An equivalent expression or desired register does not establish a capture point. |
| Recover a partial store without shrinking its backing storage | V.17/V.27/V.29 VMI FIFO-factor overrides write a word inside a copied dword. | An actual narrow native store, complete nested-offset/layout checks, all header consumers and a discriminating destination value. A narrowed load used only for low bits is insufficient. |
| Use typed flag views only where the machine gives a witness | Fax and V.22 status use byte SETcc/shift publications and native byte reads. | Measure period packing and access widths per site. Keep whole-word/byte views where callers copy storage; audit changes in alias sets and unrelated header consumers. |
| Restore constant-index CRC structure | Jd shifts expose sixteen independent words for promotion; related CRC accumulator lifetime/order changes recover V92CP. | Native constant-index operations or promotion/frame evidence, bounded crossed controls, and all inline consumers. A lower size gap alone is not enough. |
| Recover existing helper boundaries despite automatic inlining | Phase-three `twoLevelDemod` restores eighteen reference publications; FDSP conversion helpers restore loop and gain lifetimes. | Existing helper semantics plus native ownership/publication/loop witnesses. Check the selected linked copy, preserve return narrowing, and separate source recovery from an unresolved dead POP. |
| Follow an apparently handled option into the callee's request | V.17 short-training codes were recognized and logged but their request flag was omitted. | Observe the destination request/configuration, alternate long and short constructor/control histories, and demonstrate failure before correction. The added lifecycle is not proof of physical modem training. |
| Model only captured compiler mechanisms | HI/SI general-register and QI/q searches replay; V90MP's native order predicts ESI before compilation. | Pin compiler/image/input, freeze predictions first, verify eligibility/event identity on a fresh trace, and require ordinary/repeated/observed output equality. Unsupported modes/classes fail closed. |
| Separate data claims that often look alike | P3 tables keep owners but change 41 relative targets; V90MP keeps targets but changes string padding; reset reorders strings. | Report raw bytes, masked payload/layout, code owners, exact relative targets, and string payload/consumer audits separately. Owner equality is not target equality or CFG equivalence. |
| Preserve a fault's scope when source calls split or merge | V.8's formerly shared diagnostic faults must cover both source arms; reset call anchors move with their branches. | Historical detector lesson: verify the intended operation and owner. Mutation anchors and snapshots are retired from routine work. |

Three operational lessons are part of the method. Pure Make variable queries
must not recursively walk all experiment dependency files; the query guard's
poison-dependency controls still reject default and mixed build goals. Every
header change needs a complete consumer closure, including data-only objects,
and full production hashes expose changes outside the proposed TU. Finally,
new analysis must fire on known negatives: the data auditor's independent
review found duplicate local function names, absolute symbols and truncated
sections that could otherwise produce false equality. These now have explicit
refusal/unresolved controls, run normally and under `python3 -O`.

The model remains a conditional scratch-search replay, not a model of GCC's
entire register allocator. Source changes can alter eligibility, aliases,
scheduling, inlining and event counts before the cursor is consulted. Conversely,
a target with no scratch event cannot be explained by this cursor model merely
because its registers differ. Source fidelity outranks a bystander's exactness;
explain and measure collateral instead of retaining incorrect source or adding
register carriers to protect a count.

Negative results are bounded. In that historical wave, the V.22 control
captures recovered some request value ages but still needed native DSP-owner
reload structure. The later V22/HDX deep dive above supersedes that control
status and records the unresolved alias-cause alternatives. The sixteen-cell
V92Jd phase/rate family gives no exact or REGALLOC gain; its smaller-size
branches are not retained. FDSP's thirty-global/five-TU attribution bracket
does not license regrouping Run and Delete by adjacency. Existing SDM, FSE,
correlation and fixed-point families remain closed to unchanged repetitions.
None of these observations establishes a global allocator-only wall.

### Complete the deferred V92Jd byte-capture differential

The single byte-capture endpoint is separate from those phase/rate controls.
Native constructor code reads parameter offset `0x40` once with MOVZBL, copies
that byte, and splits its two bits. Capturing an `unsigned char` preserves this
read-before-publication lifetime; the original parameter field retains its
integer storage. Only the two constructor variants change among 21 TU bodies.
Both remain SIZE2; all fourteen exact members, exports and allocated data stay
unchanged. No branch spelling or register-only adjustment is retained.

Baseline and candidate each pass 99,446 checks in `t_v92jd` and 2,441 checks
in `t_v90modemctor`. The first contains synthetic component state/parameter
sweeps; the latter also builds parameters and Jd through the enclosing modem
constructor, with default lookahead three. It is constructor lifecycle coverage,
not completed modem negotiation. Parent proof verifies eight selected
constructor bodies across four executables: 1,640 literal bytes and eight
resolved constant relocations. Private executable copies that clear the high
lookahead bit fail both suites, including the enclosing lifecycle checks.
The one transposed-lookahead mutation anchor is updated to the captured value.
Artifacts: `build/gcc3-wave-jd/constructor-width` and
`build/gcc3-overnight/checkpoint10`.

The tenth checkpoint passes all 398 period tests and structural checks. Fresh
production objects reproduce the byte-capture candidate literally; the other
299 objects are unchanged. The complete byte-identity census, exact-symbol set
and REGALLOC count are unchanged. Both declared binary corruption controls
were executed (2/2, no exclusions); their wall/CPU timings and compiler scope
are retained in `parent-linked-proof.json`.


### Look for the wave's mechanisms outside its changed symbols

A separate low-cost agent screened the checkpoint-seven discovery inventory of
724 non-exact symbols/730 defining copies against current production objects.
After excluding changed TUs, mangled names and native bodies larger than 300
bytes, it examined all 190 selected unmangled defining copies in 91 objects.
All 91 object hashes agree with checkpoint nine. This is the screen's actual
denominator, not the whole 1,852-symbol census; unmangled functions can still
come from C++ translation units.

The narrow-store detector finds both original VMI17/VMI29 defects against the
frozen baseline and clears both corrected objects. An unchanged exact
`LastPulseDigitDialed` is also clean. It finds no additional immediate narrow
store/wider-store pairs in the 190-copy domain. This negative is limited to the
same constant, complete destination spelling and occurrence count, with an
explicit displacement. Register-produced stores, changed base-register names,
implicit zero displacements and byte-to-word cases are outside the query.
No general absence of partial-write defects is claimed.

A separate eight-copy direct-call screen yields two structural follow-ups.
`bValidateEnergyValue` already calls `FDSP_Kernel_InitObj` in source, but current
code inlines the helper where native code retains a call. `create_cid` already
calls `reset_cid`; native code has two call edges while current emission exposes
its body and `FPM_MRF_init`. These are source-present inline-boundary leads,
not evidence of omitted behavior. The subsequent tooling hour below tests branch ownership and records callee
cost and native TU attribution; neither lead yields an accepted source change. K56 wrapper
stubs are an explicit implementation/profile boundary, not missing helper calls
to fix by this analogy. Other call-count nominations remain unpromoted.

One explicit extension covers `CrossDataLinks`, outside the 190-copy screen
because its TU changed elsewhere in this wave. Native and current bodies are
both 123 bytes but contain 37 versus 42 non-padding instructions. Native code
has an indexed input loop, advancing output pointers and gains loaded outside
their loops. Source still spells two indexed loops. The existing conversion
helpers used successfully by `FDSP_DP_Run` therefore give a concrete next
hypothesis. The initial screen did not compile this helper transfer. The tooling-hour
follow-up below applies it after full-TU and differential validation, reaching
REGALLOC without changing the recovered neighbours.

The parent reviews the detector's controls and scope and repeats its scanner.
The reusable scratch script, JSON denominators, native witnesses and triage
report are under `build/gcc3-pattern-followup/`. This follow-up addresses the
whole wave's transferability while keeping new hypotheses distinct from the
validated reconstructions above.

## Follow source mechanisms through the complete translation unit

The 5 November tooling-first recovery hour starts from 1,131 EXACT and 28
REGALLOC symbols among 1,852. Its first accepted change follows the earlier
`CrossDataLinks` nomination: reuse both existing conversion helpers, preserving
the native indexed input loop, advancing output pointers and fixed gains.
The four-cell cross distinguishes the mechanisms: input-only gives SIZE16,
output-only BYTES18, and both give REGALLOC with one raw differing byte.
The original had BYTES96 at the same 123-byte length. All 22 other shared TU
functions, exports, allocated data and 72 exact code-table destinations stay
unchanged. There is no new exact function from this change.

Both endpoint fixtures pass 178,693 checks in 15 groups; replacing the input
gain with `1/31999` fails the `cross f` assertion. Linked-body review establishes
the selected 123 bytes and both constant relocations. This is direct component
API coverage, including empty work and conversion samples; it does not claim a
modem lifecycle for an exported leaf with no internal caller. The existing
scale-fault anchor is reattached to the helper call with its scope unchanged.
The full period/structural gate passes 398 tests, zero failed. Production
matches the tested object literally; the other 299 production objects stay
unchanged. The census becomes 1,131 EXACT, 29 REGALLOC, 73 BYTES and 619 SIZE,
with zero unresolved symbols.

The helper rewrite does not advance a different scratch stream: both captures
verify 53 events in 12 blocks. `CrossDataLinks` still consumes cursor 52 and
selects hard register 0 (EAX); native code discards the stack word into EDX
(hard register 1). Later `FDSP_DP_Run` still selects EDX at cursor 49, while
native code uses ECX (hard register 2). Preserve that register-number mapping:
GCC's i386 order is EAX, EDX, ECX, not assembler alphabetical order. These dead
pop residuals are not permission to add a carrier or permute source blindly.

### Identify the earliest compiler decision before testing source cells

The branch-owned `create_cid` control duplicates its source reset call across
the debug paths, following the native two-call-edge observation. GCC still
inlines `reset_cid`, now twice; the native length gap grows from 429 to 929,
and the local `pack_next_bit` gap grows from two to eleven. The cgraph reports
98 tree instructions in the helper and estimated growth 176 for the two sites,
versus 88 for the single-site baseline. Native reset-before-create order already
matches source. Local-data evidence attributes the reset to Rxcid, but does not
establish the caller's original TU. No reorder or TU regrouping follows.

For `bValidateEnergyValue`, cgraph marks `FDSP_Kernel_InitObj` as 58 tree
instructions and chooses inlining with estimated growth 48; the caller grows
from 115 to 163 tree instructions. The diagnostic object equals ordinary
production literally. The native caller retains a call, but current evidence
does not establish their original shared TU. Preserve this as a visibility/cost
question, not a missing helper or a reason to force `noinline`.

### Screen predecessors, including those that consume no scratch

A frontier over the 28 initial REGALLOC symbols finds both genuine order
inversions and misleading ones. B103's Rx family already has native order;
interleaving its Tx family is not an original-TU witness. ADID's 34 strong
functions already have native order. In both V90 and V92 phase-four modulator
TUs, the remaining strong-order differences occur after the REGALLOC targets,
so moving those later definitions cannot repair the earlier incoming cursor.

Two complete-TU controls establish useful negatives. Moving Psd's overlap
setter and frequency getter to native order changes no function body; neither
moved definition consumes scratch. All eight predicted events verify, and all
13 shared bodies keep their grades. A V90CP native-method-order control verifies
95 predicted events but gains no exact function and loses C2's exact grade;
C1/C2 emitted order remains a separate mismatch. A VPCM native-order control
also gains no exact function and changes `dp_vpcm_exit` from EXACT to REGALLOC.
These controls remain isolated. An exact loss is not itself a veto on an
independently supported source correction; these experiments establish no
additional body recovery that warrants adopting their collateral changes.

Artifacts for this hour are under `build/gcc3-hour/`. Reusable apparatus and
cache/capture controls are described in [the compiler-model guide](gcc3-model-search.md).

### Retain a recovered conversion boundary without claiming register closure

The earlier CPt wide-magnitude control was declined because it remained
BYTES24 rather than becoming exact. Replaying it on this hour's baseline
confirms the independent native witness: zero-extend the amplitude word,
negate the wide unsigned value when required, then sign-extend its low word
once at return. The field type and exported return ABI stay unchanged.
`generateCPt` improves BYTES29 to BYTES24; its inlined `generateSymbol` copy
also changes, and that enclosing function's SIZE distance grows from 52 to 53.
Recovering the measured conversion boundary takes precedence over that aggregate
length. Register ownership remains unresolved; no declaration or XOR-order
palette is reopened.

Only those two bodies change among 44 shared TU functions; no EXACT or REGALLOC
grade changes. Allocated masked payload/layout and exports stay fixed. Of 30
code-table destinations, 25 move within the same function owner; all resolve
to instruction boundaries. This is not strict relocation-target identity or a
CFG equivalence claim.

Each frozen endpoint passes 4,859,916 checks in 32 groups across `t_v92p4sym`,
`t_v92p4mod` and `t_vpcmrunpcm`. The direct-member/state grid in `t_v92p4sym`
is component evidence, including synthetic signed-short amplitude stress;
the constructor test and enclosing PCM path do not establish that every CPt
state is protocol-reachable. Normal V92 reset callers pass amplitude 4000.
Removing the terminal short conversion fails 427 checks in each CPt group,
including a synthetic amplitude -1000 case, while the other 22 groups pass.
That negative control establishes sensitivity to the conversion, not reachability
of the planted state. All eight existing CPt mutation anchors retain their
original fault scope after the type/conversion update. Production reproduces
the frozen candidate object literally.

The V90CP constructor also gets two bounded helper controls: its five detector
stores through `resetDetector()`, or its reset fields through `reset()` followed
by the constructor-only byte clear. Both complete captures contain 95 events.
The detector form leaves C1 unchanged and changes the exact C2 and `reset`
bodies to REGALLOC. The full-reset form changes both constructor copies to
BYTES9 and does not retain their register-only equivalence. Neither recovers a
new native body, so neither is adopted. These controls also correct an old
source comment: absence of an emitted call cannot prove that the author wrote
manual stores rather than an inlined helper. It rules out an emitted call only.

### Cancel matching store occurrences, then establish their owners

The width follow-up expands the earlier 190-copy domain to all 730 previously
inexact emitted copies: 490 unmangled and 240 mangled, across 212 objects, with
no size or changed-TU exclusions. These are historical discovery candidates
measured against hashed current objects, not a claim that all 730 remain
inexact. The old detector nominates FAXVMI, V17RX and V21TX constructors.

V17RX exposes a detector defect: both sides already contain matching narrow
and wide zero stores at the same printed address. Pairing a native narrow store
with an already-matched wide store manufactures a mismatch. The reusable
`gcc3_search_width.py` cancels equal width counts first; a real-ELF negative
control covers exactly that case. The frozen VMI17/VMI29 defects still fire
once each and both corrected objects are clean.

The corrected 730-copy scan has one nomination, `FAXVMI_create`. Owner tracing
rejects it: native ESI names the framer at its word-sized `pack_bit` clear,
whereas current ESI names the root VMI at its long `overflow` clear. Both real
owner-specific writes exist on both sides, with different register names.
The resulting count is zero supported narrow-to-wide source defects in this
query, not absence of other width defects. The V21TX nomination separately
exposes an extra native byte clear; it is not a replacement for its earlier
long clear and must be evaluated as its own source-operation question.

The V21TX follow-up retains the redundant typed status initialization. Native
`V21TX_create+0x70` clears `tx->result.byte.status` at owner-relative +0x1c,
width one, after the whole result word clear and adjacent flags update. The
candidate restores that instruction at the same construction boundary. It does
not preserve previously live upper bytes: the earlier whole-word clear remains.
Only the constructor changes among two TU functions; its SIZE81 grade remains,
and deletion stays EXACT. Exports and allocated data stay unchanged.

Both period endpoints pass 789 constructor checks. The primary self-allocating
path covers default configuration and explicit protocols 0/1; protocol 2/-1
and poisoned in-place reinitialization remain separately labelled boundary or
synthetic probes. Changing only the restored status byte to 1 fails 12 of 789
checks, including protocol 0/1 status results. This supports the fixture's
sensitivity, not a claim that the formerly missing redundant store caused a
behavioral defect. The selected object and linked constructor are verified.

Parent linked-byte review also compares all non-relocated bytes in both CPt
functions across all three focused binaries, plus both V21TX endpoints: 14
selected bodies in total. The CPt audit separately resolves all 132 typed
fields in each `generateSymbol` object (95 PC-relative calls and 37 absolute
fields), including debug wrappers, strings and the linked 30-entry table.
These linked checks complement, rather than replace, the full-TU data audit.

The final production census remains 1,131 EXACT / 29 REGALLOC / 73 BYTES /
619 SIZE across 1,852 symbols, with zero unresolved symbols. Three of 300
production objects change (Beepgen, V92Phase4Modulator and V21tx); all three
match their frozen, differentially tested candidates. V90CP's comment correction
leaves its object byte-identical. The final `make phase` passes 398 period tests
and the structural checks. Optional mutation execution/snapshot refresh is not
part of this result. Comparison, capture, screen and width controls pass under
normal Python and `python3 -O`; the width suite has six portable tests plus one
optional frozen-artifact test. Parent review verifies 25,931 selected linked
body bytes, and separately resolves 222 absolute fields plus 180 dispatch-table
entries at their actual linked addresses. The five pre-existing owner-edited
files retain their recorded contents.

## Lever 27. A bit-test predicate's COMPARISON OPERATOR is a forced fact: `!= 0` folds to shift-and, `> 0` if-converts to setcc

An unsigned single-bit test assigned to an integer looks like it has one
spelling. GCC 3.4.2 does not agree. `(x & C) != 0` is canonicalised at the
tree level to `(x >> log2 C) & 1`, so the object gets a shift and a mask;
the ORDERING comparison `(x & C) > 0` is left as a compare against zero and
if-converts to `xor`/`test`/`setne`. Both are 16-bit-bounded values here
(`x` an `unsigned char`, `C` a single positive bit), so the two predicates
are semantically identical and the one the object emits is the one the
author wrote.

Measured on `V21RX_control` (F12015): of nine equivalent spellings --
`!= 0`, `? 1 : 0`, `!!`, an `int` or `unsigned char` local holding the
comparison, the shift-and written out, `== mask`, `== 0x10`, and `> 0` --
only `> 0` reproduces the object's `xor %eax,%eax ; test $0x10,%dl ;
setne %al ; mov %eax,(%ecx)`. The others emit `mov %edx,%eax ; shr $0x4 ;
and $0x1` (or stay at the wrong length when the form hoists a local) and
were all rejected with measurements.

**When to reach for it.** A `BYTES` row whose first rejection is `xor vs
mov` (or a missing zeroing `xor`) on an assignment from a masked value is
the fingerprint. Enumerate the comparison operators and the local-variable
forms; do not accept the tidier `!= 0` because it reads better, and do not
infer the operator from final assembly alone -- the fold, not the source
spelling you started from, is what produced ours. Keep both predicates
semantically identical at the value's real width before calling a mapping
a recovery.


## Recover pre-merge edge multiplicity before calling a block order final

A block or switch-arm order that survives every case-label permutation can
still be a source fact, because GCC seeds switch arms from their CFG edge
frequencies before it reorders blocks. RD_create is the worked case (finding
F12044): four equal targets put every arm at 25%, below the round-2 threshold,
so the libiberty fibheap tie-break produced 650, 850, 1000 rather than the
object's 1000, 650, 850. Splitting the two distinct source cases that return
1000 gave five targets at 20%; the later edge merge summed the parallel edges
to 40%, the 1000 arm was seeded first, and the object's order followed.
Splitting a single straight-line body is not this lever: the split must be
between distinct source labels or arms that the RTL later merges, and the
domain must show that no other grouping maps.

Two measured boundaries. First, an arm-order difference with equal body
content is not automatically this lever: wave 17 found 59 non-exact symbols
with a table in both builds and an equal entry count, and none had the same
arm multiset in a different physical order; block-bag scans of 85 same-size
and 654 non-exact symbols found no reorder candidate, while the RD_create
control was flagged. Second, for load/store or argument-setup order the test
is an alias barrier or operand capture before allocation (see the predictor
capture recovery); when every spelling emits the same canonical order and the
object differs only at `sched2` -- or at the reload base register -- record
that first differing stage and stop rather than fitting.

Wave 17 also measured three switch-lowering leads that prove the lever but did
not then recover the original labels: splitting the V8 timeout cases produces
a 14-entry table (base 4) against the object's 18 (base 0), and splitting the
V22 error-valued cases reproduces the object's exact 17-entry shape and target
order but the label set was not uniquely determined by the object. Both were
declined as blanket duplication with an underdetermined domain.

**The V22 case was resolved from outside the object, and that is the
generalisable step (F12047).** The label set did not have to come from the
table: it is the set of values the status producers can actually leave in the
switch operand. Enumerating every writer of `struct v22fp::status` over the
object's table domain (0..16) gives exactly 0..11 and 16; no writer produces
12..15, so those are the default holes. Each value is a separate source label
with its own (identical) arm -- collapsing them into one fallthrough group
makes GCC 3.4 drop the table entirely -- and the resulting 17-entry table has
the blob's five unique targets in its order, LCS 124 against 104 for the
unjustified 17-label spelling. Search the domain's producers before declaring
a table's label set underdetermined.


## Compare byte-immediate tests by form and mask, not by a summed count

A byte-immediate test has three encodings: `a8 ib` (AL), register `f6 /0`
with a byte operand, and memory `f6 /0`. Counting masks alone hides a site:
`initTxSequence`'s memory `0x04` row disappears once its register `0x04` test
is summed, so a mask-total screen under-reports. Count `(form, mask)` and
compare each form separately.

The helper that recovered the native register form takes two byte parameters
and returns `int`:

    static int flag_test(unsigned char value, unsigned char mask)
    { return value & mask; }

Passing a field directly can still leave `testb $imm,disp(%reg)`; a live byte
local forces the value into a register and reproduces `test $imm,%al/%dl`. The
helper inlines in every TU (`nm` shows no standalone symbol). For V92Jd the
test is on the SHIFTED byte and needs `!= 0` (`sar %cl; test $1,%al; setne`),
while its `phase` must be unsigned (`shl %cl; test %eax,%edx; setne`). Both
C1/C2 closed EXACT; V29RX_control is the other closure, and five further TUs
gained LCS or size.

Two structural facts came from the same screen. `V34descrambler` and
`decoderv34` have TWO constant-shift loop copies selected by `testb $4`; our
hoisted `tap` ternary if-converted to `sbb` and blocked loop unswitching.
Moving the conditional tap into the loop body restored both copies, and
`decoderv34`'s slice counter is `short`. A memory-form site is not a helper
site: `V8UpdateModemParameters`'s memory `0x08` recovered the native test but
lost LCS (174 -> 171), and `V22FP_control` either stays memory or moves 128
bytes. See [byte/bit tests](byte-bit-tests.md).


## Let the caller decide a width; check both extensions before retyping

`widthaudit` keys an access by `(displacement, base register)`. Two different
owners can share a displacement under different bases, and the tool will pair
the blob's signed load with our unsigned one as if they were one field.
`cid_modem +0x15c` was exactly this: `struct cid::pack_len` is loaded both
`movzwl` and `movswl` in BOTH builds (blob 0x091f42 / 0x092163, ours
0x4bb / 0x701), so the reported s16/z16 pair was a base-role artefact. Confirm
ownership and check both extensions before retyping anything.

A real width fact is decided by the CALLER, not by the callee's prologue.
`TxHdxDataV27` wanted `movzwl %ax,%ebx` plus an unsigned 16-bit `cmp/jbe`;
every blob caller loads the count zero-extended and passes it unconverted, and
every sibling scrambler's count parameter is `unsigned short`. The callee's
own `movswl 0xc(%esp)` is the tail-call widening to `SDMv27_scrambler`, not
evidence of a signed parameter. The fix was `unsigned short taken` plus an
`unsigned short` count parameter; the rate chain was a switch
(`test/je; dec/je`), not an if-chain. Result: 89 -> 83 instructions, LCS
56 -> 100%, SIZE -> BYTES.

The same trace found a magnitude shape: the blob's gain loop in `cid_modem`
inlines `static int iabs(int)` (`movswl/lea/movswl/cltd/xor/sub/add`), and
`sum += iabs((int)buf[i])` matched it (368 -> 365 instructions, LCS
236 -> 241). Search for `cltd/xor/sub` magnitude loops before writing a
branch.


## Recover a shift loop's two-pointer walk and its single return value

`V90SdDetector::process` moved 66 -> 82 instructions against the blob's 81
(LCS 70 -> 80, run 29 -> 52, SIZE 2). Three facts: the shift walks two
decrementing pointers (window end, source and destination, body at the third),
with `src` declared first and the index assigned after both; the `fldz`/`fld`
sit AFTER the shift, not before; and the return value is ONE variable live
across the shift (`xor %ebp,%ebp` before, `mov %ebp,%eax` at each exit).
Spelling the return per-arm or moving the index assignment deleted the
prologue zero or reordered the blocks, and every such cell measured worse.
The TU's emission order also matched the blob's (reset, C2, C1, D2, D1,
process): check definition order before blaming allocation when a small body
is close.


## Recover ownership before compensating for inlining

Issue [#25](https://github.com/philpem/slmodem_dsp_re/issues/25) supplies a
same-TU control for a misleading exact match. PR #311 had moved `reset_cid`
and its static `V23_MRF_FILT` from `Rxcid.c` to `Dtmf_Rx.c` to retain exact
`create_cid`. Its attribution used GLOBAL call relocations and adjacent text.
Neither establishes that split. In the reference, FILE symbol 495 is
`Rxcid.c`, LOCAL OBJECT 496 is `V23_MRF_FILT` (.rodata +0x9240, 180 bytes),
and `reset_cid+0x43` has an absolute `R_386_32` into that local object.
`tools/local_data_ownership.py` reports the unique owner from that witness.
This also agrees with the earlier F1410 attribution. A symbol-name/size/
section audit alone missed the incorrect owner.

The retained recovery puts the table and reset back before `create_cid` in
`Rxcid.c`, matching their relative text order. It spells the reset's call as
`if (history != NULL) FPM_MRF_init(..., 0); else FPM_MRF_init(..., 1);`.
The reference's `test/je` at reset +0x6d sends NULL to the cold literal-1
arm; non-NULL falls through to literal 0. Passing `history == NULL` as one
argument instead lets the period compiler expand reset inside the constructor.
Two explicit call arms retain the constructor's existing exact 183 bytes.
This establishes the measured call/control shape within the tested family;
it does not recover unique original C syntax.

The bounded domain was seven cells: unchanged split baseline, then reset
before/after the constructor group crossed with boolean argument, NULL-first
explicit calls and non-NULL-first explicit calls. All 14 complete objects and
56 function verdicts used Gentoo GCC 3.4.2-r2, the retained flags and
`DSPLIB_REPRODUCE_BUGS`. The unchanged split control reproduced all eight
production bodies. Reproduce it with:

    python3 tools/cid_ownership_reproduce.py --out /path/to/scratch/cid --dumps

The tool pins source inputs to `286d7126d791addb4b1d503a723a006679297e5b`,
prints the header checkout, complete commands and executed compiler/assembler
identities, and retains sources, objects, hashes and the result ledger.
`--dumps` also retains RTL and assembly; its 56 bodies were checked against
the non-`-save-temps` run and were unchanged.

Full-TU collateral is part of this decision. All four DTMF bodies remain
unchanged. `create_cid` remains exact; `cid_modem` has identical instructions
and relocation targets, with only the raw addend locating its moved mergeable
diagnostic string changing. `reset_cid` moves from 487-byte SIZE to 517-byte
BYTES against the reference's 517, still with 63 differing bytes and a
grade-1 rejection. Its first rejected aligned rows are the two argument-address
setup instructions in opposite order; the remaining gap is open.

Unedited `pack_next_bit` grows from 303 to 319 bytes against the reference's
301. In paired dumps, bijective alias-set renumbering (preserving universal
set zero and class equality) leaves its instruction patterns and UID order
identical through `27.flow2`. The first difference is `28.peephole2`: UID 233
clears AX in the split control and SI after co-location. Later if-conversion
and block layout diverge. This localises the collateral to a late compiler
choice; it does not independently prove the persistent scratch-cursor cause.
No padding or scratch consumer was added to compensate for it. `cid_modem`'s
29 stage pairs agree after also resolving `.LC` labels to their actual strings.

The measured acceptance is structural recovery with no grade-0-or-1 set
gain or loss: 1,196/1,852, with 1,163 link-selected exact functions. Period
differential passed 394/394 and `make phase` passed. The partial object remains
different: 77,225/943,398 positioned reference bytes against baseline 77,179;
exact-position relocation matches are 1,157/18,317 against 1,161, with the
candidate relocation denominator unchanged at 18,301. These are position
counts, not added/deleted relocation counts or an identity claim. The
initialised-data screen resolves the one wrong LOCAL-owner key: 662 matching
non-relocated payloads, 100 relocated payloads excluded, over 762 reference
and 765 candidate data keys. The three apparatus extras remain.

Use this lever when an exact constructor seems to require an unsupported TU
split: check local-data ownership and the callee's native call/control shape
together. Retain an independently witnessed boundary correction with its
explicit collateral ledger; keep the non-exact neighbour open for subsequent
structural recovery.

## Close inherited experiments with their native operation widths

The unfinished wave-25 work contained two useful source shapes. In
`interpretMohTimeout`, the native jump table has fourteen entries: zero
targets the shared out-of-range return, and codes 1–13 target their literal
returns. Removing the explicit zero case and placing `return 0` after the
switch recovers all 102 bytes, including the resolved fourteen-entry table.
The previous source emitted 136 bytes. This is one exact preimage in the
inherited two-spelling domain, not a uniqueness claim across arbitrary C.

Six magnitude sites provide a separate operation-width witness. The reference
uses `cltd` or a sign shift followed by XOR/subtraction, with sign extension
before the operation where the input is a short. A local `int iabs(int)`
helper retains this promoted magnitude operation under the period compiler.
The helper name and original spelling remain inferred. In `V27RX_eq_train`,
the phase corrections still narrow to short, but magnitude and the subsequent
`cmp $0x2000` use 32 bits; keep `err` as int at that boundary.

| Function | Before bytes | Retained bytes | Reference bytes |
| --- | ---: | ---: | ---: |
| `FP_Pow` | 152 | 145 | 143 |
| `FPM_FSE_receive` | 2157 | 2132 | 2131 |
| `V27RX_eq_train` | 264 | 255 | 255 |
| `FSEv22_decision24` | 431 | 414 | 467 |
| `V22_FSE_receive` | 1722 | 1704 | 1885 |
| `band_pass` | 1258 | 1232 | 1187 |

Only the timeout function becomes exact. The V27 function enters BYTES;
the other five remain SIZE. The larger V22 size gaps are retained for the
independently witnessed magnitude graph, not selected by proximity. Do not
use these results to infer the rest of those functions or compensate for
their remaining allocation and control-flow differences.

The inherited screen compiled sixteen complete objects and recorded 180
function verdicts. Its unrelated V29 countdown signedness edits were not
retained: the proposed field retyping still needs the all-loads/both-extension
audit. Final controls against master `75c3447b1` compiled fourteen complete
objects and recorded 158 function verdicts. Seven of 79 bodies change, exactly
the seven edited functions; no symbol, binding or section membership changes.
Non-code payloads remain unchanged in six TUs. The timeout TU changes its
relocated `.rodata` table, whose fourteen resolved targets match the reference.

`make phase` passes 394 period binaries with zero failures. Across 1852 names,
grade 0-or-1 rises from 1196 to 1197 with no losses: 1161 EXACT, one UNRESOLVED,
35 REGALLOC, 93 BYTES and 562 SIZE. The partial object remains different:
76787/943398 positioned reference bytes versus 77225 before the batch, and
1160/18317 exact-position relocation matches versus 1157. Position counts are
layout-sensitive; neither count establishes whole-object identity. Preserve
these costs alongside the exact-function gain and the operation-width evidence.

The [proposed follow-on waves](../source-recovery-waves-after-wave25.md) start
with the parked signedness audit and relocated-data ownership inventory.

## Resolve data targets and observation lifetimes before refining syntax

The [waves 26–29 first pass](../recovery-waves-26-29.md) adds a relocation-aware
data inventory and closes bounded source families without adopting near fits.

**Compare pointer initialisers by their relocation targets.** A changed named
target can leave every raw byte of a pointer table unchanged: the addend is
still zero and the target identity lives in the relocation record. Conversely,
repacking a LOCAL object or mergeable string changes raw addends without
changing the initialiser's resolved descriptor. `tools/toolchain/datareloc.py`
compares ordinary bytes plus field positions and target identities/offsets,
retaining unique LOCAL FILE ownership. It reports unsupported and ambiguous
records explicitly. Mergeable string contents are a narrower comparison than
pointer identity; GLOBAL/WEAK names establish no TU owner. Keep strict
partial-object comparison separate.

The calibrated inventory resolves 96 of 100 relocated keys (659 of 677 relocations
per side). Four COMDAT/linkonce vtables containing 18 relocations stay unsupported.
Together with 662 non-relocated matches this is 758 matched / 4 unsupported over 762
reference keys, with 765 candidate keys and three apparatus extras. The controls
pass 22 assertions over 21 real ELF32 objects, including the identical-raw-bytes
wrong-target negative. A clean supported subset is not a whole-object result.

**A zero-extending load alone does not recover a field declaration.** The four
V29 countdown handlers sign-test the low half and sign-extend a register for a
later full-width comparison. Six paired field-only signedness controls leave
all 84 paired function bodies unchanged. Two complete families (42 objects,
196 function verdicts, including both controls) yield no exact target and do
not explain the native reload before subtraction. Leave the field neutral
rather than selecting a same-sized but different instruction graph. Preserve
the owner when inventorying raw offsets: the constructor's later `+0x16` clear
belongs to a different object reached through parent `+0x24`.

**Track when a value is observed, not only which field supplies it.** Native
`V34GiveINFO0dBits` reads the session variant at distinct diagnostic/selection
points and delays a decoded-word read until earlier conjunction terms pass.
The reconstruction captures scalars early. Two declared families compile 14
complete objects / 98 function verdicts and produce no exact target; a 755/758-byte
near-fit still has the wrong load/boolean graph, while a 758-byte body differs
in 118 bytes and changes two neighbours. None is retained.

Writing a live member expression repeatedly does not force repeated machine
loads: GCC can reuse it. A future recovery must explain the original observation
points through owner/type/alias evidence or period RTL, rather than add volatile
accesses or repeat nearby syntax. Executed component histories and planted
boundary probes likewise need separate claims; the first fixture audit records
the outstanding V27 constructor-to-training callback history.

## Test an alias-analysis discriminator before repeating live-read syntax

The follow-up to the two closed INFO0 families crosses captured versus live
source with retained aliasing versus `-fno-strict-aliasing`: four full objects,
28 function verdicts, unchanged control identical across all seven production
bodies. The live source's final session read exists in `07.addressof` but global
CSE replaces it with register reuse in `08.gcse`. With aliasing withdrawn it
survives to assembly, after the short `is_short` store, matching the reference's
observation point. This localises a compiler mechanism in our candidate; it does
not identify the original source alias representation or original flag profile.
The candidate becomes 774 bytes against 758 and four TU bodies change. No flag
or source is adopted. Use this discriminator to investigate independently
witnessed alias/owner structure or broader profile controls, not to select a
near-fit or manufacture a reload with volatile.

**Constructor-derived callback histories can close a specific fixture gap.** The
V27 follow-up starts from the real constructors and invokes the installed
epoch/training/running decision chain without planting history or replacing live
callbacks. It observes phase-error neighbours 8190/8192/8194 in all four supported
rate/training configurations, but no low-fold branch in its non-negative angle
domain. State the decision-component boundary: this is not an FPM/AGC/waveform
history. Fixed-answer detector controls and deliberate bin/fold perturbations
demonstrate sensitivity separately from differential agreement. Preserve the
wider seeded probes and their labels. See [waves 30–31](../recovery-waves-30-31.md)
for native addresses, denominators and limits.

## Cross alias representation before selecting a global profile

A load restored by `-fno-strict-aliasing` does not uniquely identify an original
compiler option. Cross independent complete TUs and a justified diagnostic source
representation before interpreting it. Keep raw data, bindings, relocation records
and every emitted local body in the ledger, not just reference-shared functions.

The INFO0 follow-up compiles eight objects across three TUs: 156 emitted-body
verdicts, 110 reference-shared and 46 local-helper verdicts. All 71 distinct unchanged
retained bodies reproduce production. Withdrawing alias analysis leaves the RX
energy-union bodies unchanged and loses `ApplyBulkDelay`'s grade-1 match. Native
debug-level register reuse after a short store contrasts with the withdrawn
candidate's memory comparison. This is evidence against selecting the option on
the current source, not a global exclusion over other original declarations.

A separate four-cell union-view/int-pointer cross supplies a source-representation
explanation: alias-set-0 union reads retain INFO0's fourth session read through
global CSE under retained flags. Only INFO0's body changes; the other six bodies,
bindings and non-text payloads remain unchanged. Both read representations emit
the same body when alias analysis is withdrawn. The union is a diagnostic model,
not an independently recovered original declaration, and the non-exact candidate
is not adopted for its nearer byte count. Original ownership/interface/type
evidence is the next discriminator. See [waves 32–33](../recovery-waves-32-33.md).

## Retain witnessed partial boundaries without selecting a near-match

When an independent owner/interface inventory supplies no new declaration
witness, record that negative and move to another measured residue. Separate
allocation history and mangled neighbouring interfaces do not recover an
unmangled caller's optimiser-visible alias declarations. The bounded INFO0
owner pass found no such witness and did not reopen its syntax families.

Visual diagnostics supplies a positive partial example. A declared 2×3×2
family plus unchanged control compiles 13 objects/39 function verdicts. Original
instructions independently require a selector-8 range exclusion, three echo
load/advance sequences before output stores, and two direct receiver-relative
equaliser loads. The retained source restores those boundary classes; 31 period
layout assertions and changed-use owner/offset/width checks prove address
preservation. The control object is byte-identical to production, both neighbours
remain unchanged, and exact/grade-0-or-1 gain/loss sets are empty.

The partial is 1006 bytes against native 1023 (previously 987), with no exact candidate
in the declared domain. Two source forms emit identically, so the retained
capture spelling is not uniquely recovered. Retention rests on the independent
boundary witnesses, not the size gap. The partial-link positioned-match count
decreases by 12 bytes; its DIFFERENT/raw-different status remains explicit. Block/tail,
subowner and resampler-base residuals remain in the ledger, without scratch
compensation or unsupported compiler-stage attribution. Focused period and the
394/0 full phase gate pass. See [waves 34–35](../recovery-waves-34-35.md).

## Cross equivalent owner bases once, then strengthen the input history

The visual-diagnostics follow-up crosses explicit PCM subowner formation with
owner-rooted resampler reads: four objects/12 function verdicts, 23 layout
assertions per object, and a full-object byte-identical unchanged control.
Native session `+0x6bd0` then `+0x14/+0x20` supports naming the existing echo
subowner. Native owner `+0x43c/+0x434` supports the two fallback load encodings;
complete addresses remain equal to receiver `+0x1d8/+0x1d0`. Preserve owner,
offset and width at each changed use before interpreting the base choice.

The retained partial changes only three instruction regions; neighbours, all
40 relocation records and non-text payloads remain identical. Target size stays
1006/native 1023 and positioned matches decrease one byte. This is bounded native
base recovery, not unique source syntax or a reason to expand cast spellings.

A separate fixture constructs the actual owner and drives exported echo and
equaliser component APIs: 24 paired trials, 278 new assertions and independent
integer known answers. A wrong-owner near-to-far control fails 2/278 assertions.
It supplements the unchanged synthetic grid. Constructor provenance plus caller-
supplied component inputs does not establish modem-generated adaptation histories;
PCM/status-2 and selector-8 states remain unproved by this fixture. Keep those
boundaries explicit. Final period/structural gate passes 394/0. See
[waves 36–37](../recovery-waves-36-37.md).

## Follow both layout invocations and distinguish status from progress

Candidate `.31.bbro` first reorders visual selector bodies, but cleanup and a
second STC reorder inside `.34.stack` choose the final DFE-first layout. Native
final topology is near-echo-first; original RTL is unavailable. A predeclared
`-fno-reorder-blocks` control preserves source-arm order, merges the return and
loses exact SNR without recovering native topology. The two-object/six-verdict
domain closes as a mechanism diagnostic, not a flag/source recovery. Diagnostic
normalisation must retain UIDs, operands and edges; identical preprocessed input
alone does not validate a dump normaliser.

A constructor session argument is not live status, and owner+4 progress is not
owner+0 status. Real V.92 construction here enters quick-connect status 4/role `0x66`;
exported retrain clears status 0. Required negotiated phase-2 history for visual
status 2/3 was not established. An observer-only wrong-owner control fails 6/61,
distinguishing owners without planting state. Valid PCM echo allocations still
do not license the status guard. Selector 8's native zero-return stub makes
output-only coverage blind: a valid future fixture needs call-target observation.
Preserve these negatives rather than adding a seeded stand-in. See
[waves 38–39](../recovery-waves-38-39.md) for denominators and next controls.

## Trace result-value joins before compensating for block order

Native visual DFE guard failures join an explicit zero definition; the previous
candidate carries its entry-zero result directly to an epilogue. This difference
exists before block reordering. A baseline-plus-explicit-fallback experiment
restores the zero definition, native five-class join and near-echo-first selector
order: two full-TU objects/six verdicts, without case permutations or scratch.

A diagnostic build from the recovered Gentoo recipe and one unchanged replay
reproduce production raw bytes while exposing predecessor traversal. Probability,
trace length and first-encountered ties explain the unchanged candidate's two STC
decisions. These are candidate observations, not original RTL/source uniqueness;
the alternative's ordinary dumps do not inherit baseline instrumentation claims.

Retain the independently witnessed value boundary, not a presumed layout cause.
The two-line fallback is 1007 versus native 1023 bytes, still non-exact; neighbours
and exact/grade-0-or-1 membership remain unchanged. Review changed jump-table
addends, raw relocations and symbol order explicitly. Focused period and full
phase 394/0 pass. See [waves 40–41](../recovery-waves-40-41.md) for controls,
compiler provenance, partial-link result and unestablished status-event histories.

## Require compiler causality before a new source control

A four-target residue investigation traces expansion, if-conversion, reload and
tail duplication rather than trying alternate structures. Nine unchanged raw
full-TU replays establish the diagnostic path. Live-set checks explain blocked
pointer motion and its later LEA-enabled legality; `stmt.c` explains unsigned
partitioning and bounded-zero omission. These are candidate compiler causes,
not evidence of original helper factoring or source spelling.

Only one native-backed source prediction follows: an unsigned switch-expression
cast in V8SetMessage's selector helper. Two cells reproduce the predicted first
RTL and final branch prefix, while six neighbours and data remain unchanged.
The public/helper argument stays int; helper-null join remains unresolved. The
partial remains 403/native 392, with no exact gain. Do not expand this into helper,
case-order or code-structure palettes; trace the remaining compiler unknown first.

Generated INFO0 peer history illustrates the corresponding fixture discipline:
observe native bits and state progression, independently compute CRC, and make
external CRC corruption a passing rejection case. A copied-observer negative
control demonstrates sensitivity separately. The maintained 7808 assertions extend
only the INFO0 prefix, not later negotiation or status reachability. Full phase
394/0 passes. See [waves 42–43](../recovery-waves-42-43.md) for denominators,
collateral, invalid-run exclusions and bounded next steps.


### Recover narrow induction and promoted dispatch independently

`VTBv32_init` normalises its induction value with `CWTL` after each increment
in both loops. The reconstruction's `int` with casts only at the comparisons
omitted those operations. A short counter restores them; the two loops reach
128 and eight respectively, within its range.

The same function's native mode dispatch sign-extends the short argument and
uses a signed 32-bit decision tree. A `switch` applies the required integer
promotion, whereas the previous `if` chain compiled to halfword comparisons.
A four-cell full-translation-unit comparison isolates the two boundaries:
short induction alone and promoted dispatch alone remain SIZE; together they
recover all 355 bytes and relocation targets, reaching EXACT at 75 instructions.
Keep the existing case order and all branch bodies fixed. The inlined copy in
`V32FP_recreate` also changes and remains SIZE, so it needs its own component
validation and collateral audit.

## Cross source lifetimes with a compiler-pass control

When a native reload disappears, trace the actual transformation before trying
more type spellings. In [F12100](../findings.md#f12100-native-transmit-lifetimes-and-a-shared-gcse-control),
a WORD reload survived CSE and disappeared in GCSE PRE. A pass control restored
a complete training function while losing an already-exact wrapper. Inspecting
the wrapper's native pointer capture and refresh ages recovered it under both
profiles. The old exact body had concealed a source/profile compensation.

Treat that loss as a discriminating question. Compare the complete TU, preserve
useful controls, and cross the native-supported source explanation with the
option. A flag change alone is not recovered source; an old exact neighbour is
not proof that its current source is uniquely original. Require the complete
native constraints, named collateral sets, actual-build differential and object
parity before adopting the combined explanation. Record the resulting profile's
scope and ambiguity rather than manufacturing function attributes.

Keep the unit of work large enough to close the dependency graph. Shared
request lifetimes, callback owner epochs and outgoing arguments can couple
several functions in one TU. Analyse their constraints together without
extracting the original state machine into helpers. Retain a native-supported
capture even when its frame is temporarily less close, provided the complete
review explains the tradeoff; size and local runs do not overrule evidence.
Batch final validation at the closure checkpoint. Report whole-function gains
separately from partial recoveries and distinguish measured model usage from
free local computation.

## Transfer a mechanism without transferring its assumptions

[F12101](../findings.md#f12101-transfer-native-epochs-without-transplanting-field-types-or-proof-claims)
extends the countdown/owner inquiry across related fax code and a data-mode
caller. Preserve each site's actual interpretation: an unsigned shared field
can feed a signed guard through an explicit narrow local or conversion.
Do not retype the field to copy another module's spelling. Also distinguish a
reload lost by GCSE PRE from a reload absent because our source caches the value.
The same final instruction deficit can have different first causes.

Transfer the narrowest explanation supported by the native graph. Moving a
post-callee owner capture before its condition closes the data-mode example;
that does not support transferring the fax optimisation profile to it. Audit
callbacks, outgoing arguments and protected neighbours in the whole TU.

Keep evidence limitations separate from code differences. First refresh the
selected partial-link context for an alternate object tree. If the official
recogniser still refuses a native instruction or guard form, record its exact
reason. Ordered function-relative table targets and instruction boundaries are
stronger evidence than matching debug strings alone, but do not silently replace
the grader's proof. Retain an independently supported source recovery with its
honest UNRESOLVED status when the complete review and period gate justify it.
A separately scoped recogniser extension needs its own positive and negative
controls; changing source to fit an unsupported proof pattern is not recovery.


### Isolate a pass control at its actual input

A diagnostic flag is a single-variable control only when the source, headers
and other options are identical. Similar final arithmetic does not establish
that condition. If a historical diagnostic used different expressions, either
recompile the retained source with only the flag changed or demonstrate that
the executable RTL and CFG have converged before that pass. Normalise incidental
UIDs and source locations without discarding operands, edges or probabilities.
Follow downstream register renaming before interpreting final register choices
as the input to an earlier tail-merging decision.

Keep this compiler diagnosis distinct from fixture reachability. A constructor
followed by a real control request may reach states that an older test comment
calls unreachable. Check setters and installed handlers before accepting that
claim; extend the component history instead of planting an unproven internal
state. Report the boundary and retain synthetic probes under their own label.


### Preserve the operand view at its use

An unsigned-short local does not ensure that its uses remain narrow. In
[F12104](../findings.md#f12104-direct-component-counts-preserve-the-receive-loops-two-value-views),
initial expansion promoted a carry local and shared its widened value between
pointer arithmetic and the loop test. Direct reads of the component count
preserved a WORD control view and the separately extended subtraction value.
One native-supported explanation predicted a second complete wrapper before
its candidate was compiled. Keep callback observations, signed old values and
unsigned new values explicit; do not introduce another carrier to force a
register. Review all TU peers and preserve the scripted component boundary.

### Recover the complete formatting argument graph

[F12105](../findings.md#f12105-formatter-producer-lifetimes-improve-without-closing-the-whole-body)
shows why isolated comparison or constant spellings do not establish original
helper types. Map every argument producer, x87 value lifetime, conversion,
control-word change, scale materialisation and post-call field age together.
A popping instruction can reflect a dead value rather than a different scalar
type. Current dumps and backend rules explain current decisions; original RTL
remains unavailable.

Use a withheld contrast without tailoring it. An unchanged target body is an
inert source control, not a recovered graph, even if it already has the desired
instruction. A partial may still recover independently supported native
constraints. Select it by the complete observations, preserve its period
oracle and name its unresolved obligations; neither a smaller instruction count
nor a closer byte extent determines which source is better supported. Stop
candidate extension when the whole-body or withheld prediction fails.

## Keep experimental compiles outside integration gates

Run exploratory variants with unchanged full-TU controls and local native/TU
comparisons. Full phase gates, global fidelity censuses, whole-tree parity,
full partial links and CI are not prerequisites for experimental compilation.
Refresh only the proof inputs required by the local comparison.

Validate retained source with focused period checks. Settle independent review
and supported partials before freezing the integration candidate, then batch
the full gate, global comparisons and PR/CI validation at that checkpoint.
See [experiment design](experiment-design.md#1-validate-the-experiment-before-interpreting-it)
for the distinction between diagnostic controls and integration evidence.

### Check both sides of an inline budget against native callers

A profile can gain EXACT bodies and lose no EXACT names while damaging functions
already classed NONMATCH. In F12108, disabling automatic inlining restores six
complete native bodies but introduces twenty helper calls across two large
pumps. The relevant native contract requires both the wrapper call and the
pumps' inline expansions. Current callee costs separate those outcomes; an
interior automatic-inline budget restores the six bodies and leaves both pumps
raw unchanged, with an adjacent budget as a failing boundary control.

Review the complete TU, exports, data and constructor collateral before
retaining such a profile. Treat it as a qualified source/profile hypothesis;
exact original flags need not be unique. Conversely, a score gain cannot waive
a native call boundary. The measured V90 cost interval is local evidence, not a
budget to copy into unrelated TUs. Check parameter identity before reusing old
`-finline-limit` results as exclusions for `max-inline-insns-auto`.

F12109 bounds this technique's reach. The demodulator has a consistent interval
and recovers two native ABI cutpoints with all peers unchanged, but closes no
function and reduces its longest matching run. Retain such a result only for
its independently supported structural facts, with its data/layout changes and
remaining mismatches explicit. Do not extrapolate the phase-4 promotions into a
profile sweep. A remaining native SF spill, when initial RTL already uses SF,
requires producer-lifetime/stack-lowering evidence before a type or cast change.

F12110 separates a tap-producer boundary from a superficially similar XOR
association experiment. The former changes which value is spilled and transfers
to the duplicate function; the latter was already tested. Publishing the WORD
before consuming it for feedback supplies a second, independently observed
boundary. The recovered loops still differ in capture/allocation details, so
remain NONMATCH and leave that frontier low priority. Search replay tools by
source family as well as literal symbol: dynamically constructed target names
can hide relevant old domains. A history index supplies leads, not exclusions.


### Treat destructive comparisons as predicates before arithmetic (F12111)

An INC instruction can implement a comparison with -1 when its narrow input
is dead. It need not represent a source increment. Inspect the period machine
description and the actual death notes: `RxClampV32` keeps a signed narrow
counter for the next decrement, while its separately tested producer dies at
the shared sentinel. That lifetime permits GCC's HI comparison peephole and
`*addhi_4` emitter, closing all 49 bytes without a flag or register constraint.
A SI addition followed by a HI zero test cannot reuse the SI zero flag and
therefore does not reproduce the same instruction.

Keep frontend loop formation separate from the later comparison decision.
A common loop test may be duplicated before the first optimized jump dump;
removing that transformation diagnostically did not recover this whole body.
Trace initial RTL, header copying, value reuse, combine and peephole2, then
cross the supported source graph and value lifetimes. Preserve wrapping count
behaviour and final aliased reads. Useful intermediate cells can remain in a
cumulative scratch investigation; the run's complete promotions and protected
losses must still be reported separately from partial instruction runs.

### Separate access types from pointer identities at each pass (F12112)

Start with ordinary source habits suggested by native access groups. An author
may write `substruct *p = foo->inner` simply to avoid repeated qualification,
then use `p` for several stores. Consider real argument/subobject aliases and
captured fields before a prolonged compiler-flag investigation. They are normal
source hypotheses, not artificial register uses. An opaque entry followed by
a concrete cast is a different idiom; infer callback interchangeability or C
polymorphism only from actual uses across functions. Preserve an existing API
when the object does not distinguish the alternatives.

A missing native reload need not imply an aliasing write or wrong member type.
Test a supported opaque argument with its concrete typed local, alongside a
typed-argument/local control. V22 control produces identical complete objects
from both forms: the local leaves different early and late base pseudos at PRE,
and later copy propagation recognises their equality after that opportunity.
Concrete alias sets and typed members remain unchanged.

Inspect the expressions at the deciding pass. Equal final base registers do
not prove that PRE saw equal expressions. Distinguish access type, public
argument type and local value identity; a successful opaque form does not
uniquely identify the original API when the typed form has the same object.

Keep the explanation cumulative with real value captures. The pointer local
alone did not close V22: the early scrambler producer, conditional HDX refresh
and late parameter-bit capture also had to agree. Trace all remaining regions,
preserve reads across potentially aliasing stores and review the protected peer.
This is not a reason for a local-copy or pointer-spelling palette elsewhere.

### Recover volatile access at the supported scope

A reference reread can justify a volatile-access hypothesis, alongside aliasing
and value-lifetime explanations. Test its scope: a volatile field affects all
consumers, whereas `volatile T *p` makes this reader's pointee accesses volatile
without making the pointer variable itself volatile. The latter is different
from `T * volatile p`.

The V34 delay inquiry first recovered parameter-owner epochs, but the compiler
then reused a DMA-field value the reference rereads. A reader-local volatile
view closes the full body. The native writer independently reuses its just-written
DMA value across a separate compound update, supporting narrower qualification
than a shared volatile field. Compare readers and writers and require the whole
TU result; a reread alone does not prove original qualifiers or asynchronous
hardware access. This is qualifier recovery, not permission to insert a barrier
until an instruction matches.

### Transfer demonstrated observation boundaries (F12115)

Choose a transfer candidate from an actual native/source read boundary, then
check previous experiments before compiling. A short body or a narrow-load
screening tag alone is insufficient. Keep the target limit as a ceiling rather
than filling it with weaker or exhausted leads.

`VPcmV34InitiateHangUp` closes only when two consumed captures agree: retain the
session observed before the diagnostic call, and capture status between the
first rate clear and the following two. Either isolated owner correction leaves
the function nonmatching. This is the same kind of observation-boundary inquiry
as the earlier owner transfers, without copying their compiler flags or types.

Keep negative controls specific. Reusing V21's default-diagnostic owner removes
an extra root load but allows CSE to remove a native field reread; local volatile
controls still do not close that body. Preserve the result rather than applying
the delay reader's qualifier elsewhere by analogy. Review every affected TU:
the hang-up source change also changes scheduling in the unchanged MOH function.

### Isolate scalar captures from added pointer carriers (F12116)

Measure value locals and added owner pointers separately. The V34 rate setter's
engine/K56 owners recover 85 of 89 literal instruction tuples, leaving four
rate-load/argument-store instructions. Scalar captures with another parameter
pointer remain NONMATCH; those value captures through the existing owner close
the function. Preserve the supported owner graph while testing its remaining
value boundary.

Retain reviewed structural gains separately from classification claims.
`SetAdaptEcV32` recovers native FP ownership and the complete operation sequence,
but different register carriers fail the strict consistent-renaming proof.
Its 81/81 normalised run is diagnostic; the grade stays NONMATCH. Independent
value-flow review and period checks support retention without changing the
classifier. Stop allocation-only spelling variants while structural gaps remain.

### Match a floating producer's lifetime, not just its numeric value (F12117)

The reset diagnostic computes an absolute floating value early and converts it
to an integer argument later. Capturing the integer immediately is numerically
plausible but gives the wrong x87 lifetime. Retain the ordinary floating local
and its late conversion; inspect the popping subtraction and constant load as
well as the final formatted value. Add period inputs around fractional truncation
boundaries before retaining a changed arithmetic producer.

Distinguish an observed float member from its pointer owner. The V90 partial
restores the late float-field comparison while still caching an owner the
reference reloads. A disappearing memory operand between pass dumps narrows the
search interval; it does not prove a particular pass caused the loss. F12118
identifies the actual PRE deletion and corrects the earlier feature verdict:
the no-GCSE control restores the owner reload, although the function remains
NONMATCH. Whole-TU obligations still block profile retention.

### Separate feature verdicts from complete-function grades (F12118)

For each hypothesis, record whether its named operation appeared, separately
from EXACT/REGALLOC/NONMATCH. The V90 control restored the requested reload;
using its unchanged NONMATCH grade as a negative verdict sent the next inquiry
in the wrong direction. Track instruction identities, reaching values and the
emitted instruction pair, with known positive and negative controls. Textual
memory counts can mix different uses and equivalence annotations.

A feature success admits the whole-TU review; it does not approve a flag. The
no-GCSE profile improves two late owner observations but exposes missing native
owner captures elsewhere and merges a call tail. Native-backed source locals
repair two of those features in crossed controls, while the unresolved argument
snapshot/call-tail obligations remain explicit. Test those obligations before
retention rather than widening the source or flag search.

### Trace type bridges and loop legality before retaining a flag (F12120)

A float-returning operation and a float cast of a double-returning operation
can have different rounding boundaries. Follow the actual RTL modes and stores;
the AGC negative control demonstrates the distinction rather than relying on
source type names. Keep a known failing precision control beside the green run.

A flag can restore arithmetic indirectly by changing loop recognition and
constant placement. AGC's default pipeline rejected a multiple-entry loop;
combine then folded reciprocal multiplication into division. A shared loop exit
preserved the original operation order while restoring hoisting under both
profiles. Inspect actual operation nodes and their instruction identities:
REG_EQUAL annotations can otherwise look like additional executed arithmetic.

### An exit store can be compiler promotion, not a source cache (F12121)

Do not infer a manually cached local from register-held state and one final
member store. AGC's direct member updates produce the native graph through actual
GCSE PRE and store motion. Follow the transformation before adding more locals.
For a spill, inspect the interference point: changing the observation epoch of
an independently witnessed replacement count removed overlap with a two-register
conversion, without forcing allocation.

Stop source variants when the complete instruction/branch correspondence differs
only by an independently checked GP permutation and encoding layout. Report the
maintained grade and its proof limits separately. A pair-specific checker is not
a generic recognizer; frame-free EBP use, saved state, definitions dominating
uses and branch destinations require guarded eligibility and negative controls.


### Distinguish a block's consumed cursors from an inlined helper (F12122)

Identical arithmetic does not prove that a block routine called a scalar
helper in the original source. Inspect which pointers the block consumes,
which initial values survive each iteration, and where the sample cursor
advances. These are separate constraints from the inner arithmetic.

`FPM_iir_filt_block` initially called the already-REGALLOC scalar filter through
indexed samples. Advancing the sample cursor before the inner filter and
writing through `samples[-1]` recovered the native sample epoch, but left the
function NONMATCH. The reference also saves the initial coefficient/history
pointers before the outer guard and restores those snapshots each iteration.
An explicit block body consuming its parameter cursors, with real saved-pointer
reset consumers, recovers all 86 instructions under the maintained REGALLOC
proof. The scalar arithmetic, short count and guarded normal-build repair stay
unchanged. Do not replace the saved roots with cached coefficient values:
bounded overlapping sample/coefficient/history arrays distinguish them.

Transfer only the witnessed ownership. `FPM_iir_filt_II` independently has the
same saved-root pattern but indexes samples. Coupling those roots with its
native short postdecrement recovers the prologue and 73-instruction count,
but still fails the operand/order proof. Matching size or instruction count
does not close the function. The later coefficient-observation repair below
supplies an independent reason to retain this graph as a partial; its remaining
instruction ordering still needs a first-pass explanation, not a cursor, width
or declaration palette.

### Trace tied operands before interpreting an x87 spill

A stack store can originate in register matching rather than a source precision
boundary. In the Gentoo `GetGain` dumps, CSE shares a coefficient with a later
product. At `.22.regmove`, the dead integer-conversion temporary is merged into
the cross-call `base` pseudo. `.25.greg` then spills that conversion before the
multiplication, whereas the reference stores the multiplied base. The existing
private-float-result control recovers a separate SF lifetime and does not solve
this earlier DF spill. A no-regmove diagnostic restores that named feature but
closes no function and changes nine TU bodies; no profile is retained.

Read the matching constraint and death notes before proposing source variants.
The i386 commutative `%0` constraint allows both multiplication inputs to match
the destination. Swapping the written operands does not remove the dead-input
match. A useful next source hypothesis must explain both that matching input
and the independently observed coefficient folding; a cast or nearby expression
spelling alone does neither.


### Check memory epochs before dismissing different scheduling (F12123)

Two streams with the same operations can read different values. In
`FPM_iir_filt_II`, the reference publishes the first history word, reads the
third coefficient, then publishes the second history word. Reading the
coefficient after both stores changes the result when that coefficient aliases
the second history word. A bounded one-array component fixture gives 5096 in
the reference and 1500 in the earlier reconstruction; all intermediate integer
arithmetic stays in range.

Capture the consumed coefficient at the witnessed read point. Keep the saved
cursor roots and short countdown established independently. This repairs the
component oracle without making the full function REGALLOC: remaining loads
and multiplies are ordered differently within equivalent memory intervals.
Retain the native-backed observation correction and label its full grade
honestly. Do not discard a failing valid-input oracle as mere allocation, or
claim that equal operation counts prove equivalence.

### Validate nonzero word countdowns at a bounded public slot (F12124)

A signed-positive and a nonzero countdown can agree on every small positive
fixture while disagreeing on a complete bounded input buffer. MRF's public
int count narrows to a word: positive 65535 becomes -1, and the native
partial-input loop consumes 65535 samples through its wrapping countdown.
Constructor-created state and an eight-word ring keep every access valid;
zero outputs avoid the separate signed-output-index hazard. Compare the
complete history as well as return/count fields. The earlier source fails
9/24 checks even though both versions return zero and leave the same need.

Retain the minimal BUG-only predicate correction when the oracle and native
branch prove it, even if the whole function remains NONMATCH. Preserve normal
behavior separately; the old and new nonreproduction full objects are raw
identical here. Do not generalize this fixture into normal modem reachability,
a uniquely recovered formal type, or permission to test negative history sizes
and wrapped output stores. Do not replay the old loop/wrap spelling cube.

### Separate extraction width from the carrier across a branch (F12125)

When a native byte extraction supplies full-width array indices across a
branch, trace the local's mode in initial RTL. An eager `unsigned char k`
can define QI before the branch and add an SI extension in the successor.
`int k = (unsigned char)input[index]` preserves byte extraction while carrying
the full-width result. In PPS, that one predicted change removes the redundant
successor extension. A byte-typed local elsewhere is insufficient evidence:
V.22 PPS's corresponding map uses are already in the capture's block.

Require the complete function comparison after recovering the local feature.
PPS still fails at 230/229 instructions; the remaining extra state-argument
reload first appears in global allocation despite a shared initial owner
pseudo. Stop source variants without an independent source-level distinction.
Do not fit declaration order to physical stack slots. A native discarded read
also needs a read-footprint detector before runtime output/state tests may be
credited with observing it; ordinary green differential output cannot do so.

### Keep initial shifts distinct from final reverse copies (F12126)

A historical cursor experiment excludes only the loops it actually changed.
FDSP's old final-copy cube did not test the two initial delay shifts. Their
native consumed destination pointers justify one new control, which recovers
those regions. Composing independently witnessed final-copy constraints still
requires a whole-function proof: a final control reaches native's 126-instruction
count while retaining different source-update epochs, branch forms and call
scheduling. None of these controls closes FDSP.

Trace the first differing graph. Here initial RTL already contains the
predecrement source cursor and the counter comparison against 159; the loop
pass preserves them. Do not attribute either to allocation or enumerate
postdecrement/predicate spellings merely to select an opcode. The next
admission needs an independent call/alias witness or the relevant expansion
rule. Keep synthetic fixtures labelled and establish a constructor-to-loop
component history before retaining a later FDSP change.

For a missing call, inspect actual cgraph decisions and original local-symbol
ownership before splitting a TU. Native FDSP local anchors place InitObj inside
the same TU envelope as its validator. The current inliner first reduces the
callee with four helper inlines, then expands it into that caller. The original
reason for retaining the call remains open; a source/profile budget diagnosis
is the next checkpoint. A missing call alone does not justify a TU split or
an attribute to suppress inlining.

### Audit return observability before clearing a stale register map (F12127)

A CFG-aware comparison may correctly identify independent returning arms while
still needing a return-value contract. Do not clear maps at RET or ignore EAX
merely because the reconstructed header says void. V92 reset's native returns
leave path-dependent residues and its immediate callers propagate them; only
known transitive callers discard them. No original declaration is recovered.
Keep that bounded conclusion distinct from a whole-export proof. Also compare
actual successor rows: V34's 37/37 normalized run conceals different shared tails.

### Separate original erroneous reads from their linked memory (F12128)

For a layout-dependent original bug, reproduce the operation in BUG mode and
supply an explicit test memory environment. D302's zero substitution hid its
native SAR1 and out-of-table read. A scoped test-only relocation binding lets
source and reference read the same backing while preserving production tables
and independent data checks. Keep the genuine original-backed comparison and
label synthetic backing separately; neither proves final-object placement.

Validate the binder structurally and the fixture dynamically. Nonzero distinct
targets and poisoned neighbours expose flat-zero, repaired-shift and wrong-index
controls. Report coverage for every selected region. Preserve input boundaries,
check exact instruction bytes rather than assumed signedness, and invalidate
cached executables when binding bytes change. These are apparatus changes;
production source carries the original expression without hooks or padding.
Equal instruction totals still require the whole-function operand/CFG proof.

### Preserve a shared narrowing boundary through CSE (F12129)

When native code shares a shifted signed energy between later shifts, inspect
CSE before changing arithmetic or scheduling. In V.32, an `int` intermediate
lets CSE associate SAR16 then SAR4/SAR3 into SAR20/SAR19. A native-supported
short intermediate blocks that early association; combine later removes the
redundant extension without losing the shared producer. Check the complete
consumer graph and emitted width: a short introduced merely to fit registers
has no such justification. This mechanism is demonstrated across all six
quality callers, with full-TU controls and period differential coverage.

Inspect initial RTL for eager predicate folding before blaming allocation.
Nested retrain predicates preserve the native branch sequence here. Likewise,
keep a full-width increment when native narrows only its comparisons and store;
a named short can create an extra shared extension. These are per-site source
constraints, not global rewriting rules.

Recover initializer and owner lifetimes from consumed native spills and branches.
The V.32 slicers initialize the nearest index after quality, but initialize the
consumed differential fallback before it. Their default quality arm refreshes
the owner after storing count, while early exits retain it. Returning this real
selected owner from the existing helper expresses the shared graph; it does not
prove the historical helper declaration. Preserve subsequent sample rereads and
alias-sensitive output/read order.

Stop a spelling route when later passes erase its distinguishing graph. An
integer bit-pack temporary survives initial expansion here but combine produces
the same nine final bodies. Incoming argument reloads first inserted by global
allocation do not independently justify new source captures. The six-cell wave
retains native-backed partial recovery but closes no function: keep that outcome
separate from instruction totals and local mnemonic-run changes.

### Explain a failed global allocation with its complete blocker set (F12130)

A reload first appearing in `.greg` identifies a stage, not a cause. Read the
allocation order, the pseudo's initial hard conflicts, its class, local homes
and the earlier conflicting global assignments. In V.32 `16Tpt`, the root is
last and all three remaining registers are occupied by identified global
competitors. The allocator's local-eviction path cannot displace those globals.
The incoming argument's memory equivalence explains its subsequent reloads;
a separately logged sample spill must not be mistaken for the root's rejection.
See the [full audit](../v32-16tpt-allocation-audit.md) for the observed masks,
source rules, native lifetimes and limits.

Compare native spill and address-producing epochs before proposing a component
alias. A natural consumed alias can be a valid hypothesis, but changing its
lifetime merely to perturb an explained priority order supplies no new source
discriminator. Conversely, allocation-stage divergence does not establish that
source is correct. Seek a supported live-range, use/dependency or class change;
the original machine code constrains those inputs without exposing original RTL.

### Use initialized input aliases to expose a stale publication (F12131)

A native input reread after state publication can differ from a captured local
even without an intervening call. When the component declaration permits the
alias and both pointers name valid storage, use an initialized member that the
method updates as the input. In V.8, input `&pr->run` exposes the stale stored
sample: the blob rereads the incremented run, while the old reconstruction
publishes its previous value. A 64-call history covers the index wrap and checks
the complete owner; the old-source negative fails 128/193 checks and the recovered
source passes. Label the component boundary and alias arrangement; do not claim
normal waveform or modem lifecycle coverage from it. This does not license
arbitrary overlapping objects or impossible planted histories.
An ordinary short input may alias a short state member even when the input
pointer is const-qualified. The native reread needs no volatile qualifier or
barrier; test the actual alias/read-age explanation before proposing one.

Treat a shared native action as a source-graph constraint. Two failed predicates
join one V.8 increment, so a short-circuit conjunction with a common else action
is an admitted source form. Verify initial RTL and final edges: eager predicate
folding or a changed memory-read epoch would falsify the prediction. A consumed
member-pointer local can likewise represent an independently observed address
producer; it is not an artificial register use.

### Recover predicate signedness without reopening unrelated floating code (F12132)

A bounded nonnegative loop still has a measurable source signedness. V.90
prefilter selection uses unsigned branches in both native six-element loops;
one unsigned-int index restores both directly from initial RTL. Preserve the
function's floating narrowing, unordered comparisons and diagnostic read ages.
A successful predicate prediction with an unchanged NONMATCH grade is partial
source recovery, not a whole-function promotion. Check the complete TU and
protected siblings before retention.

### Follow captured pointer values through both conversion directions (F12133)

A local pointer copy is a direct source hypothesis when native loads a member
at entry and consumes the saved value after intervening stores. In equalizer
edge fading, the same high/low coefficient pointers feed conversion out and
conversion back; spelling the members anew loses that captured epoch. Verify
both consumers in initial RTL and final code, rather than assuming that a new
local survived optimization. Do not invent an owner-alias history merely to
turn this source constraint into a behavioural-bug claim.

Name packed fields from the actual join/split operations and verify their
allocation topology. Two independent allocations, two-byte strides and explicit
high/low concatenation establish separate 16-bit planes; a single logical
32-bit value does not establish one 32-bit array. Keep raw/aligned/skew names
consistent, preserve offsets and prove a naming-only compile unchanged.

### Preserve member observations across calls in a template (F12134)

Publishing a member from an argument does not make the two interchangeable
across later calls. LowPassFIR validates incoming `nTaps`, then observes published
`taps` across free/allocation/hamming. Each sinc iteration independently captures
the coefficient pointer before its call. Keep these epochs separate and preserve
the existing floating recurrence/narrowing and normalization contracts.

Audit all defining copies and actual ELF section flags. A census can select
fewer owners than the full TU emits, and executable linkonce sections are code
even when a local analysis helper puts every non-`.text` section into a data
bucket. Explain the named section and verify true data/bindings/bystanders;
do not weaken a production comparator to excuse drift.

### Trace inlined helper parameters before blaming register allocation (F12136)

An inlined reconstruction helper can still alter pseudo identities and live
ranges. FP_Pow's coefficient helper introduced an early int copy of a short
index. CSE redirected a late post-increment comparison to that older copy,
extending its lifetime across the loop. Direct native-supported table access
removed the copy and produced an EXACT scratch candidate. Compare initial RTL
and the first CSE output before trying declaration-order or cast palettes.
A helper introduced for a guarded bug substitution is especially worth checking.

Preserve the normal repair while recovering the reproduction expression. If
the original access leaves a measured table extent, provide a separately
validated test memory context; instruction identity alone cannot establish
linked runtime fidelity. Keep historical backing and synthetic context probes
distinct. See the [FP_Pow deep dive](../fp-pow-deep-dive.md).

### Keep diagnostic axes separate from acceptance (F12138)

A first rejection or similarity score does not inventory all residual causes.
Report extents/placement, relocations, CFG, operations, register/stack evidence
and strict identity separately, with units, unmatched sides and unknowns.
Resolve branch destinations under an explicit instruction correspondence; byte
displacements alone do not show a changed successor. Preserve ambiguous
correspondence and every defining copy. The existing strict grade remains the
acceptance authority. See the [vector design](../metric-vector-assessment.md).

Third-party search scores need negative controls. The examined permuter already
has i386 support but erases real operand/relocation distinctions in zero-score
cases. Prefer its bounded manual enumeration behind an unchanged full-TU
compiler wrapper and the existing strict oracle. Do not turn a lower heuristic
score into source evidence. See the [assessment](../decomp-permuter-assessment.md).

### Use a diagnostic vector to select an inquiry, then inspect the proof (F12139)

Run `diffvector.py` on explicit defining copies to separate placement, raw
encoding, canonical relocation identity, mapped edges and operand observations.
Keep units, unknown records and the chosen mapping visible. A literal stack
text difference can be a relocated immediate, not a changed stack slot; a raw
instruction encoding difference can be only relocation placement. No diagnostic
axis replaces the existing grade or period differential. The
[guide](metric-vector.md) includes measured examples and controls.

Test the acceptance checker independently when axes disagree. F12141 finds a
high/low-byte false acceptance even though current selected REGALLOC rows do
not show that specific mismatch. Fix that proof boundary before widening search.
The first actual finite-permuter trial (F12140) was raw-identical: enumeration
works, but the admitted short-local form is not an allocation lever. Stop there
instead of filling a palette budget.

### Preserve register bit selection in a renaming proof (F12142)

Register family and spelling length are insufficient: AL and AH select
different bytes, while AL and AX have different widths despite equal name
length. Compare an explicit width/bit-position descriptor before binding a
family. Keep the full-width-write rule separate so partial writes do not kill
the unknown upper bits of an old live value.

Test both the main comparison and fallback certificates. The saved-frame
implementation needed no expansion; its literal caller operands and full-width
callee permutation already refuse the tested changes. Keep known-negative
before/after controls and real-ELF positives. A checker tightening is not a
source gain: refresh named membership and report any reclassification explicitly
rather than changing a floor or source to preserve a former false acceptance.

### Keep a measured type recovery separate from a whole-function gain (F12144)

The 4-point slicer's int counter omitted the native signed-word increment
conversion and comparison. A short local restores that complete latch from
initial RTL onward, with an independently bounded 0..3 value domain, unchanged
siblings/data and passing period component checks. Its longest global run and
whole grade do not improve. Retain the demonstrated type boundary as a partial;
do not call a closer byte count or an extra matching row a function promotion.

Use a complete diagnostic census for navigation, then verify the actual site
and experiment history. F12143's 607-function population yields three inquiry
sites in two DATA functions, while eleven attractive screens are closed or
mispaired. All rows retain their mapping/unknown caveats. The
[next closure](../nonmatch-vector-triage.md#next-bounded-wave) crosses only the
MOH carrier width and its witnessed ordered reason chain.

### Lever 55. A `?:` that the reference compiled BRANCHLESS is a real source difference; tell it by the shape, not the value

The first difference in `FPM_atan`'s `if (x == 0)` arm was not allocation: the
reference computes `0x2000 + ((y >> 31) & 0x4000)` (`sar $0x1f` / `and
$0x4000` / `lea`), where our `(short)(0x2000 + (y < 0 ? 0x4000 : 0))` compiled
to a branch (`test` / `mov $0x6000` / `js` / `mov $0x2000`).  Same compiler, so
the original spelling was arithmetic, not `?:` (F12242).

Read the two shapes, not the computed constant:

- BRANCHLESS: `sar $0x1f` + `and <mask>`, or `setcc` + `neg`/`shl`, or a
  `cmov` -- the compiler was given an arithmetic expression or a plain select.
- BRANCH: `test` + `js`/`jns` + two materialised constants -- the compiler was
  given a `?:` in a context it chose not to if-convert.

They compute the same value, so `fidelity.canonical` (and every value-level
screen) cannot separate them; only the instruction shape can.  Do not normalise
a whole function to one spelling: in `FPM_atan` line 106 is arithmetic and line
110 is genuinely ternary -- the two arms differ, so the form must be derived per
site.  A spelling change that lands the block but leaves the follow-up `add`
vs `lea` + register difference is a partial, not a promotion; retain it and say
so.

### Lever 56. A length FIELD may hold an element COUNT, not a byte length -- check the units against the field's USES (HYPOTHESIS: unproven)

A length field can be spelled `len & ~3u` (a byte length) or `len >> 2` (an
element count), and the two differ whenever the low bits are non-zero.  The
discriminator is the pair (field, uses), not the instruction alone, and the
field's consumer settles it -- `for (i = 0; i < n; i++)` over an array means `n`
is a count.  Where the conversion is a bytes-to-elements one, spell it by the
type (`len / sizeof(float)`) rather than a literal or a bare shift; all three
emit the same shift for an unsigned operand.

**The one candidate was withdrawn.**  F12255 read the reference's `shr $0x2`
against our `and $0xfffffffc` and concluded `V90Equalizer`'s length fields are
counts.  That was a MIS-PAIRED alignment -- the reference's shift writes
`0x4c(%ebx)`, ours the mask `0x48(%ebx)`.  The change was retained on a single
passing fixture and broke TEN (`make phase`); F12268 records the correction and
the revert.  So the lever stands as a hypothesis to TEST, with no confirmed
worked case: check the units against the uses, and judge any change against the
whole affected fixture set -- one fixture is not the check.

### Lever 57. Reference-only `add` counts are POINTER-WALK signatures -- but the walk is per-LOOP, never per-function

After excluding zero idioms (`xor %r,%r`, `sub %r,%r`) and `inc`/`dec`
counters, the corpus's reference-only real arithmetic is dominated by `add`, and
an `add` in a loop body is what `*p++` emits where `p[i]` emits nothing.  That
makes the filtered `add` count a nominator for source SHAPE (F12258).

`V34EchoFilter`'s convolution is the worked case: indexing the dot product
(`e->coeff[k] * hist[k]`) against the reference's `acc += *cp++ * *hp++` moved
the byte delta -2 -> 0, the aligned run 10 -> 15 of 55 and the multiset distance
35 -> 22, and passes the differential.  The convolution above it in the SAME
function stays indexed, and `V34EqualizerAdapt`'s attempted walk OVERSHOT
(delta 19 -> 57), while `V34EqualizerFilter`'s worsened it (-22 -> -27).  A
`for (k...) sum += a[k]*b[k]` loop is the strong case; a loop that also indexes
a wrapping delay line usually is not.

### Lever 58. An early capture of a pointer/owner installs scheduler ANTI-dependencies -- evaluate it inside the call instead

`sched-deps.c:569-570` adds a memory anti-dependency (read-before-write) for every
pending read that may alias a later store:

	if (anti_dependence (XEXP (pending_mem, 0), t))
	  add_dependence (insn, XEXP (pending, 0), REG_DEP_ANTI);

So a statement that CAPTURES a pointer early -- `dsp = obj->dsp;` before
`f(&dsp->x, ...)` -- puts the owner LOAD ahead of the outgoing argument stores,
and the four resulting edges forbid the scheduler from moving those stores
earlier.  No priority, tie-break, scratch cursor or renaming that preserves the
edges can produce the object's order.

Worked case F12264: `ModDataV22` (src/pump/v22/V22int.c).  The object stores
`data` and `count` before loading `v22->dsp`; ours loaded `dsp` first.  Writing
the call as `FPM_SMC_encoder(&v22->dsp->smc, &v22->dsp->smc_ring, data, count)`
-- owner evaluated inside the call, so it mixes into argument setup and the
cheap stores precede it -- is **EXACT** (28/28, delta 0).  A single captured
local is WORSE (delta +11), so the two independent owner reads are load-bearing.

Where to look next: any function whose source captures `ptr = obj->field;` (or a
similar owner) in its own statement and then passes `&ptr->...` to a call.  The
tell in the bytes is our load appearing before the outgoing argument stores
while the object stores them first.  Companion mechanism in the same function:
`local-alloc.c:1672-1693` breaks a quantity-score tie by quantity NUMBER
(`return q1 - q2;`), which decides which of two equal-priority live values gets
the scarce call-crossing register.

### Lever 59. Peephole2 scratch-register state is threaded across the whole TU in emission order -- a correct earlier-function fix can rotate a later EXACT function's lanes; repair by matching the earlier function's scratch-search COUNT

`recog.c:2932` `peep2_find_free_register` holds `static int search_ofs;`
(line 2935), advanced on every successful scratch search (lines 3016-3018) and
reset to 0 on failure (line 3024).  It persists across ALL functions in the TU,
so how many scratch searches an earlier function performs shifts the
scratch-register choice of every later function.  A source correction in
function A that changes A's scratch-search count -- adding or removing an
`lea`/`add`/`test` that a peephole2 pattern matches -- can therefore silently
rotate the register lanes of an already-EXACT function B later in the same TU,
with no change to B's source.  Confirm the coupling by recompiling the TU with
`-fno-peephole2` (via `TC_EXTRA`): if A's change and the baseline then emit
IDENTICAL B lanes (only addresses differ), the scratch cursor is the carrier.

Worked case (this session): `V34SetINFO0aBits` (src/pump/v34/v34info.c).  The
object reaches members through the "engine view" base at `obj+4`
(`struct v34_engine *e = (struct v34_engine *)(void *)&obj->progress;`,
documented include/dsplib/v34fsk.h:399-432), ours used the flat
`obj->v90_receiver`.  Correcting that ONE access moved 0aBits toward the object
(align ratio 0.635 -> 0.653) but rotated the neighbour `V34SetINFO0dBits`
EXACT -> REGALLOC -- a ratchet loss, not an acceptable trade.  The repair was to
match the EARLIER function's whole representation, in four parts: (1) the `e`
base for both `v90_receiver` reads in 0aBits; (2) the shared `pcm_name` helper
also through the `e` base (`e->k56flex_receiver`, not `obj->k56flex_receiver`);
(3) and (4) both branches restructured so the V.PCM / info0d path is the
fall-through, matching the reference block order.  Only then did 0aBits's
scratch-search count match the original and 0dBits fall out EXACT again:
0aBits NONMATCH -> EXACT (717/717, delta 0) with 0dBits EXACT preserved.

Where to look next: a NONMATCH/REGALLOC function whose PREDECESSORS in emission
order carry a representation difference (a base pointer, block order, or helper
inlining) that changes how many peephole2 scratch searches run.  The signature
is a single-access fix that improves the target yet regresses a later EXACT; do
not accept the trade -- correct the earlier function's full representation.

### Lever 60. A `?:`'s ARM ORDER picks the fall-through -- the arm the original wrote as the THEN is the one that falls through

For `x = (cond) ? A : B;` GCC 3.4.2 expands the THEN as the fall-through and
branches around to the ELSE (the test is `je`/`jne` to the ELSE label).  So when
the object's fall-through arm is the one our source wrote as the ELSE, the
condition is inverted in the original: rewriting `(c) ? B : A` as `(!c) ? A : B`
-- or as `if (c) A; else B;` with the object's arm first -- restores the layout
without changing behaviour.

Worked case (F12278): `_hdlc_receive_look_carrier_state`'s `limit` initialiser.
Our `(*rx_count == 0) ? s7*50 : s7*8000/rx_count` put the `* 50` arm as the
fall-through; the object falls through to the division.  Writing
`(*rx_count != 0) ? s7*8000/rx_count : s7*50` moves the longest run 18/205 ->
21/205, symdiff 26 -> 24, no size change, and the two equivalent spellings
(ternary and `if`/`else`) measured byte-identical.  This is the C-ternary
analogue of the branch-order restructures used for `if` bodies (Lever 46/47):
read which arm the object falls through to, and make that arm the THEN.

Where to look next: a `short` (lever-2) row whose largest block contains a
ternary or a two-arm `if` whose fall-through is the arm our source wrote second,
once the frame-offset and pointer-walk confounds are set aside.

### Lever 61. The AUTO-INLINE budget is a per-TU profile: read the cgraph dump's `N insns` against `max-inline-insns-auto` (default 100), never against the source

`cgraph_default_inline_p` (`cgraphunit.c:1068-1077`) inlines a callee that is
not declared `inline` iff `estimate_num_insns(callee) <
PARAM_MAX_INLINE_INSNS_AUTO`.  The parameter's `DEFPARAM` default is **100**
(`params.def:63-66`; the neighbouring doc comment's "150" is a stale bug).  The
`-da` cgraph dump prints the estimate directly (`NAME: N insns needed tree
inlinable`) and the decision (`Considering NAME with N insns / Estimated growth
is +G insns / Inlined into CALLER`), so the number is measured, not inferred.

Two native witnesses settle whether the ORIGINAL budget differed from 100: a
callee the reference DID inline sets a lower bound, and a callee it did NOT
inline sets an upper bound.  Worked case (F12280/F12281): the FDSP kernel TU
inlines `zFLTUTL_FloatMemSet` (8) into `FDSP_Kernel_InitObj` but does NOT inline
`FDSP_Kernel_InitObj` (78) into `bValidateEnergyValue`, bounding the budget to
[8, 78); a `--param max-inline-insns-auto=50` profile for that TU moves
`bValidateEnergyValue` +300 -> +20 with zero collateral.  This is the same lever
as `TC_V90P4_FLAGS` (=80) and `TC_V90DEMOD_FLAGS` (=50) at
`period.mk:176-185` (F12108/F12109).

Do NOT read "our helper is larger than the reference inlined" as proof of a
missing source statement: read the estimate in the dump and the reference's
inline/call decision BEFORE touching the callee's body.  Where a budget profile
leaves a residual (F12280's +20), that residual is the next lever, not evidence
against the profile.

#### Addendum to Lever 61. Prove "global parameter" false with a counter-witness PAIR, not a sweep

A whole-tree sweep only tells you the best global value; it cannot show that a
per-callee boundary is unreachable by ANY global value.  That needs two inlined
callees with OPPOSITE outcomes and inverted sizes:

	V92Modulator::reset                  est 88   INLINED by the blob
	V90Phase4Modulator::recivedSUVtag    est 82   NOT inlined by the blob

88 > 82, so no single `max-inline-insns-auto` produces both (a limit above 88
inlines the 82-callee too; at or below 88 it drops the 88-callee).  One pair of
EXACT counter-witnesses therefore kills the global-parameter explanation for the
whole inlining family, where four sweeps (100/80/50/`-O2`) could not (F12286).

Read the estimates straight out of the cgraph dump (`Considering X with N insns`)
at two budgets; the flipped callee is the diff.  Then decide the per-callee
cause: `cgraph_default_inline_p` (`cgraphunit.c:1069-1077`) withholds only on
`!DECL_INLINE || !DECL_SAVED_TREE` (a TU boundary) or the size gate; a
code-invisible statement (a call the compiler folds, e.g. an accessor written
where the source could use a direct store) raises the estimate without changing
the bytes -- the estimate weights are 10 per CALL, 10 per div/mod, ~4 per `int`
assignment, 1 otherwise (`c-common.c:5663-5788`).

### Lever 62. A reference `setcc` count of 0 where ours has N: the original wrote a DEFAULT assignment plus one conditional OVERRIDE, not a ternary

`-fif-conversion` (`ifcvt.c`, `noce_try_store_flag_constants`) rewrites
`v = cond ? A : B;` into a materialised flag (`setcc`) plus an arithmetic or
`lea` combination.  When the object shows a plain `mov $C,%reg ; jcc` pair and
its `setcc` count is 0 where ours is N, the original did not write a ternary: it
wrote a default assignment followed by ONE conditional override, so the two arms
are separate stores that if-conversion will not merge into a flag:

	v = C;                 /* C = the object's emitted default */
	if (!cond) v = D;

(the mirror `v = D; if (cond) v = C;` is the same shape with the other default --
pick the one whose default matches the object's `mov $C,%reg`).

Worked case (F12289): `FAX_FSE_decision_64pt` / `FSE_decision_64pt` region trees.
Per leaf `base = (short)(q > 0x1fff ? 0x28 : 0x2c);` ours emitted
`cmp $0x2000,%si ; setl %cl ; lea 0x20(,%ecx,4)` while the object emitted
`cmp $0x1fff,%si ; mov $0x28,%ecx ; jle`.  Rewriting every leaf as
`{ base = 0x28; if (q <= 0x1fff) base = 0x2c; }` removed the conversion:
delta -30 -> +5 and -34 -> +5, symdiff 62 -> 15, run 15 -> 18.  A plain `if`/`else`
(V1) is BYTE-IDENTICAL to the ternary -- the difference is the single-assignment
path, not the branch shape.

READ THE SIGN: `scratch/cd/setcc.py` prints `ref/ours`.  `ours > ref` (the object
has FEWER setcc) means our source over-materialised -- a ternary or `&&` where
the object branches: apply THIS lever.  `ours < ref` (the object has MORE) is the
opposite direction -- the source needs a conditional VALUE (`?:`, `&&`) or a
mask, per Lever 32/40.

Where to look next: `rebuildJMSequence` (src/v8/V8.c) ref 48 / ours 26 -- the
object materialises 22 flags that ours branches; the reverse lever.

### Lever 63. A `?:` whose two arms are the SAME CALL with different constant arguments collapses to ONE call with a computed argument -- the object's TWO calls mean `if`/`else`

`f(c ? A : B, x)` with constant A/B becomes ONE call whose argument is computed
(`cmp ; sbb ; add`), because the argument is a conditional value, not a branch.
When the object instead shows TWO literal calls -- one per arm -- the original
wrote the control flow out:

	if (c) f(A, x);
	else   f(B, x);

Worked case (F12293): `V90Equalizer::process`.  The ternary folded the condition
into the state argument (ours: `cmp $0x1 ; sbb %edi,%edi ; add $0x9,%edi`),
while the blob has literal-8 and literal-9 calls sitting on the two arms of a
`jne`.  Rewriting it as `if`/`else` moved delta -250 -> -207, insns 2039 -> 2046,
symdiff 987 -> 980, and the TU's `setBllState` relocation count 9 -> 10 (the
blob's).

The tell is the ARGUMENT: a ternary folds the condition into the VALUE; two calls
cannot.  Screening signal: a relocation-count shortfall of one per
`f(cond ? A : B)` site.  This is the mirror of Lever 62 (there a ternary had to
become default+override to stop an if-conversion; here it had to become `if`/`else`
to stop a call merge).
