# Batch20 V.90 small boundaries

Baseline93d7eee1, retained complete Gentoo period profile, fullTU replay -da.
Closed SignBits definition order family F7800/F7815 excluded. DiffCoder template
bodies belong to the other session and are not candidates here.

Declared domains before compilation:

* V90DilDescriptorSettings: eight cells crossing removal of redundant explicit
  zero-count returns (retaining the loop's zero-trip test), code local loaded
  before the 16-int automatic array initialization, and result initializer moved
  after the invalid pointer/type guard. Both overloads changed consistently;
  invalid inputs still return0. Blob's code load precedes REP MOVSL, and its
  outer zero-trip check is singular, unlike our explicit+loop checks.
* ModulusDecoder::progress: four cells crossing acc initialization to in[5]
  then separate multiply/add versus the initial expression; all subsequent
  multiply/add pairs separated versus expression. Blob emits repeated general
  DImode products with independent zero extensions and product/add boundaries,
  not the cheaper reduced products in our current expression lowering. Same
  signed long long accumulator, unsigned digits/factors, bound and arithmetic
  shift. No operand permutations/types/flags/signatures change.

Baseline must reproduce raw. Measure fullTU function sets, metadata/data/relocs
and exact gains/losses before adoption. Reject all cells if no exact, record first
pass boundary rather than loop over register/declaration synonyms.

First DIL8/modulus4 domains: no exact gains. Both baseline raw reproduced.
Splitting initial DImode product gives equal-size534B but remains BYTES452;
subsequent split pairs do not change bytes. DIL zero-trip+early-code gives equal
size196B, BYTES69. These source families are not adopted.

CPpck float2Bits four cells: if-chain versus switch(mode) with two cases/no
explicit default; original versus negated table-comparison with arms exchanged.
Negate the whole `table>x` predicate rather than use x>=table so NaNs retain
routing. Blob initial dispatch places mode0 after no-match return, and its table
true arm branches away from fall-through subtraction. This is explicit CFG
source structure, not reordering unrelated bodies. No helper calls/definition
order/types/flags/table changes. Full enclosing large CPPacker remains audited.

CPpck result: switch plus subtraction-first is byte-exact float2Bits283B.
Neither component alone is exact (switch BYTES30; arm inversion alone SIZE6).
CPPacker's entire relocation-normalized body remains unchanged SIZE133. Whole
TU is 0→1/2 exact, no losses. Both floating table-branch and initial dispatch
origins already differ in initial RTL: `ne` initial if dispatch becomes switch
`eq`; the two table branches become complement `le` in the inverted-arm source
rather than `gt`. These are source CFG edges, not a guessed postallocation
register-color change. All four complete TU cells preserve metadata, tables,
allocated nontext bytes/absence of nontext relocs. Together with the failed
DIL/modulus controls, 16/16 complete TU audits pass; 24/24 float2Bits stage
records retained and 8/8 dispatch/table-edge controls pass.

Replay with saved baseline archive/profile:

    python3 tools/gcc3_batch20_v90_small_reproduce.py --domain docs/batch20-v90-small-boundaries.md
    python3 tools/gcc3_batch20_v90_small_reproduce.py --family cppck --domain docs/batch20-v90-small-boundaries.md
    python3 tools/gcc3_batch20_v90_small_audit.py

Parameters loadModemParamsData four cells: move DIGITAL_POWER_REDUCTION store
before integer-display computations (blob stores at+0x380 before FIST), and use
(int)fabsf(pr) for whole instead of integer cast then integer abs (blob FABS
before its separate FIST). No float types/expressions/scales change. This tests
two observed source factoring distinctions; F7900's parameter cast guard domain
is not repeated. FullTU clones/by-standers must be measured, not just thisbody.

The first float-abs compile rejected undeclared fabsf (this TU lacked math.h).
Preserved under build/gcc3-batch20-v90-parameters-invalid-missing-math; exclude
it from results. Corrected candidate float-abs cells include standard math.h
for the period libc declaration; baseline still contains precisely the retained
source and must reproduce raw. No source ABI/header definition changes.

Parameters corrected4cell domain also closes withoutexact (floatfabs yields344B
BYTES159; earlystorecombinedBYTES106). WholeTU6/9exactunchanged, no losses.
All20validcells nowmetadata/data/nontextrelocationaudited, unchangedby-stander
exports. Replay parametercontrol: same tool --family parameters. The original
invalid missing-declaration compile remains excluded and preserved.

ADID resetStudyUrefHandler (966B versus blob956): four structured control-flow
spellings, preserving both complete branch bodies: plain-case early return
baseline; hot-case early return; plain-first if/else with shared final state
clear; hot-first if/else with shared final state clear. Blob's QC case falls
through and plain code is out of line, unlike current source. The two final
clears occur on both arms. No statement permutation, types, arithmetic, tables,
flags, helper calls or definition ordering change. Include entire ADID TU audit;
separate exact-set effects from unused helper/store changes.

ADID full -da baseline ICEs at TU end (3039), segfault after dumps but before
object. Preserved/excluded under build/gcc3-batch20-v90-adid-invalid-da-ice.
Try initial-RTL-only -dr instrumentation as a bounded diagnostic fallback;
raw full production baseline must reproduce before interpreting any candidate.
The failure is not interpreted as evidence about reconstruction source.

ADID -dr baseline raw reproduces, four valid cells 12/34 exact unchanged.
Plain-first shared cleanup emits identical target bytes; hot-first/hot-return
change only resetStudyUrefHandler but remain SIZE10. Family closed with no gain.
The -da baseline instrumentation crash is not part of this denominator.
24/24 complete TU metadata/named-data/canonical allocated-nontext relocation
controls pass including ADID's switch table function/offset targets.

ADID hot-first changes only the anonymous .rodata.cst4 constant pool's order
(160B both, identical multiset of40float bit patterns), not values/owners or
function-relocation targets. The audit records canonical pool values; canonical
text-body checks additionally prove every reference's value, so sorting pools
alone cannot excuse incorrect use. Other allocated sections retain rawbytes
except their verified function-relative table relocations. CPpck winner's
allocated nontext bytes are raw unchanged, not merely pool-equivalent.

Fresh allocated b103.c object-first domain, sixteen cells: original/restored
four debug anchors (create entry/config, process changed-status/link); explicit
B103-id branch before V21 flag; corrected backward widening loop predecrement;
if-chain/switch on low status byte. Blob names every print string and argument.
The loop control is semantic recovery, not an equivalent spelling: blob begins
at count, decrements BEFORE its first write, stops at zero; current for loop
writes extra element[count]. Preserve/identify this discrepancy, do not excuse
it with byte-count improvements. CFG status switch follows blob's balanced
5/6/7 comparisons. No header/prototype/template changes; parent gate at end.

B103 first16 cells: restoring prints gets create to SIZE14, explicit id branch
plus prints SIZE4; corrected loop and switch plus prints gets process SIZE11.
No target exact yet, but dp_b103_exit becomes exact as a bystander (fromBYTES4).
Next independent boundaries, eight cells around that combined recovery plus raw
baseline: id flag local/common store (blob branches rejoin before v21 store),
reversed n_rx/n_tx declaration slots (blob has tx+0x28/rx+0x2a; candidate reverses
those two escaped stack locals), eager conjunction of two side-effect-free edge
predicates (blob SETNE/SETE/TEST beforebranch, candidate shortcircuits). No other
permutation. Record initial RTL stack identities and predicate forms; body/code
alignment alone is not proof of the caller's original integer types.

B103 next8 boundary cells plus rawbaseline: no exact create/process; explicit
id common local store worsens SIZE4 to13, count declaration order/eager edge
expression don't change emitted process. Declaration slot hypothesis refuted:
stack address-creation/use order, rather than declaration sequence, dominates.

Independent emission-order control: object emits create/delete/process/init/exit,
source/period baseline emits init/create/exit/delete/process. No closed b103.c
emission-order family found in Playbook/findings (C++ permutations are not this
TU). Compile raw baseline; restored combined original-order; raw-source blob-order;
restored combined blob-order. Completefunction order/bindings/data/reloc audit.
Only object's measured order; no arbitrary domain of permutations.

B103 actual emission order plus recovered statements yields create BYTES1 at
406B (all instructions/relocations/registers align except config-name ternary
jump sense). Plain order control retains missing-code residuals; no exact target
yet. Last discriminator: complement config.v21's zero/string branches as source
predicate (cfg.v21==0?Bell103:V.21). Cross with counts unsigned short vs signed
short and result unsigned vs int: blob zero-extends returned counts/uses %x full
status. Signature/header unchanged; C compiler's pointer signedness warning is
recorded rather than a header overlay. Compare all eight source cells plus raw
baseline to avoid treating a single branch fit as unique original spelling.

B103 process next independent provenance observation: blob's two line-rate calls
load modem from self+4, while candidate uses input dp+4. These pointers coincide
on valid wrapper lifecycle but original owner is still observable. Change only
those two calls to self->dp.modem. Cross with print status low byte spelled as
(unsigned char)result rather than result&0xff: blob MOVZBL in changed-status
print, candidate AND imm32. Four around exact-create combined + rawbaseline.
This is a field-owner/narrowing preimage, not a declaration permutation. The
pointer owner difference must be documented separately from runtime reachability.

B103 remaining pointer-lifetime boundary: retained candidate keeps the input dp
as EBP throughout process, blob reloads the original parameter near status/rate
edge while retaining B103FP return in EBP. Test direct struct dp* internal
parameter instead of void* plus typed local; compare inline casts of void*
parameter without a persistent typed local. Include explicit count-address
locals formed tx then rx to discriminate escaped stack object identity. No
volatile, register hints, flags or public headers. Internal static callback
signature variant is ABI-identical but emits a C pointer-type diagnostic when
passed to generic wrapper; no adoption of warnings just for layout.

Parameter/count-address six controls produce no exact process and no reduction;
close that family. Next independent source ownership: keep B103FP's full return
in fp_status and DP wrapper result in its own variable, versus reuse result for
both as current source does. Blob keeps full status in EBP, wrapper result ESI.
Cross placing default wrapper result=OK before B103FP call versus before status
switch. Four around combined + rawbaseline, no numeric/CFG/call changes. This
is a two-domain value lifetime/definition boundary, not hardware reg naming.

The initial shared-result/early-default generator wrongly removed the later
wrapper default despite the intervening FP assignment. It is not equivalent,
so preserve initial run as invalid-shared-default and exclude from conclusions.
Correct shared-result early cell keeps its necessary later default too; first
initialization is a dead-definition control. Separate-status early cell's
single default remains live. Rerun all corrected controls/rawbaseline.

B103 final valid controls: 54 complete TUs across seven declared source families;
all baseline raw reproduce. Two exact gains adopted: create406B and exit49B,
2→4/5 TU exact. All four config-ternary/type-cross zero-name cells match create;
unsigned count/result hypotheses provide no gain, so keep original local types.
Final source also corrects count-1..0 widening, restores four authentic debug
calls, uses self->dp.modem for line-rate calls and unsigned-char log argument.
Process remains SIZE11; the six parameter/address and four corrected split-status
controls are closed no-hit families. No header/prototype change adopted. Two
invalid runs are explicitly preserved/excluded (full-da ADID crash and shared-
result early-default generator error). Missing math.h was a third invalidrun.

B103 audit:54/54 full TUs with5functionssamebindings/types; canonical24-byteops
table unchanged including static function pointer targets. The only newimports
are proven debug level/printf symbols; string pools contain only seven strings
read directly from blob (including original ops name).90selected RTL records
and10initial missing-debug controls pass. No previous exact loss. Parent batch
must run deciding period/structural gate before committing the semantic fixes.

Replay B103:

    python3 tools/gcc3_batch20_v90_small_reproduce.py --family b103 --domain docs/batch20-v90-small-boundaries.md
    python3 tools/gcc3_batch20_v90_small_reproduce.py --family b103 --b103-next --domain docs/batch20-v90-small-boundaries.md
    python3 tools/gcc3_batch20_v90_small_reproduce.py --family b103 --b103-order --domain docs/batch20-v90-small-boundaries.md
    python3 tools/gcc3_batch20_v90_small_reproduce.py --family b103 --b103-types --domain docs/batch20-v90-small-boundaries.md
    python3 tools/gcc3_batch20_v90_small_reproduce.py --family b103 --b103-owner --domain docs/batch20-v90-small-boundaries.md
    python3 tools/gcc3_batch20_v90_small_reproduce.py --family b103 --b103-parameter --domain docs/batch20-v90-small-boundaries.md
    python3 tools/gcc3_batch20_v90_small_reproduce.py --family b103 --b103-result --domain docs/batch20-v90-small-boundaries.md
    python3 tools/gcc3_batch20_b103_audit.py

Coordinated B103FP header cross (declared before compilation): data agent's
candidate changes only ModDataB103 and TxNoCarrierB103 public return declarations
from short to unsigned short. Neither is directly called by the b103 wrapper;
the wrapper calls B103FP_modem, whose int declaration is unchanged. Therefore
predict raw unchanged full TU, rather than assuming a process return-extension
gain. Three controls: historical raw baseline/original header, stable winner
source/original header, identical winner source/candidate-only unsigned-return
header. No production header edits or new local type permutations.

    python3 tools/gcc3_batch20_b103_header_reproduce.py --candidate-header /path/to/candidate/b103fp.h --domain docs/batch20-v90-small-boundaries.md

Cross result: all three compile/control records pass; historical raw baseline
reproduces. Candidate-only header leaves the entire stable object raw identical,
including metadata, data, relocations and all five functions. No process gain:
the wrapper remains SIZE11 and both create406B/exit49B gains stay exact. This is
an unaffected-consumer proof for the shared declaration, not a new local lever.

Next B103 process domain, declared before compilation: blob uses preserved
byte-buffer bases for both in-place loops (tx ESI, rx EDI), whereas stable source
generates displaced self-member addressing inside loops. Cross explicit scoped
unsigned-char pointers for tx fetch/widen and rx pack/put (each independently),
unsigned-char conversion of switch status versus equivalent low-byte AND, and
CONNECT result definition before its debug guard versus after print. Blob has
MOVZBL at status formation and defines CONNECT before the debug predicate.
Sixteen finite combinations plus the historical raw baseline; no count/local
declaration permutations, header changes, or register naming. Preserve negative
cells and review complete TU, including the two existing exact gains.

    python3 tools/gcc3_batch20_b103_buffers_reproduce.py --domain docs/batch20-v90-small-boundaries.md

All seventeen controls complete. No gain: scoped rx pointer and byte-status
conversion are raw body no-ops; scoped tx pointer widens SIZE11 to SIZE12;
CONNECT-before-print widens SIZE11 to SIZE17. Both exact gains are unchanged.
Close this scoped-pointer/status-boundary family; do not infer pointer-local
source solely from the blob's register base choice. Full-TU audit now covers
71 valid source cells, preserving all prior controls and two exact gains.

Fresh allocated resampler pool: ResamplerTimingOffset is already entirely exact.
V90Resampler::getTimingHistoryStd has blob66B/ours39B: blob compares variance
to zero, loads ±1, multiplies, then FSQRT; ours FABS/FSQRT. Predeclared sign
domain: negative predicate selecting -1 first, nonnegative selecting +1 first,
and whole negative-predicate complement selecting +1 first; ternary versus
explicit float sign assignments. Six cells including raw historical baseline.
Nonnegative/complement predicates are equivalent for finite values; record
period unordered behavior from emitted compares before adoption, and preserve
negative zero behavior rather than silently accepting FABS as equivalent.
No header/template/definition-order edits. Trace initial RTL to identify when
the compiler's fabs fold occurs and audit all TU bodies/data/relocations.

    python3 tools/gcc3_batch20_resampler_reproduce.py --domain docs/batch20-v90-small-boundaries.md

Six-cell result: explicit nonnegative and whole-complement statements each make
getTimingHistoryStd exact66B; negative-first statements are BYTES9 at66B, and all
ternaries remain SIZE27 at39B. Whole-complement statements adopted, preserving
the original complete comparison predicate rather than changing unordered
source semantics. Exact machine comparison and ±1 paths agree with the blob.
Full TU13→14/16 exact, no losses; all fifteen bystanders raw canonical unchanged.
Metadata/named data/vtable/switch-table relocations unchanged; only original
zero/-1 constants added to the formerly absent cst4 pool. Six-TU audit passes.
Thirty-six RTL stage records separate two distinct simplification boundaries:
positive ternaries already ABS in initial RTL; original negative ternary first
becomes ABS in21.ce2; explicit statements preserve branch and multiply. GCC3
ifcvt.c:noce_try_abs recognizes opposite NEG/source assignments and forms ABS;
the observed ce2 boundary is consistent with that implementation. This is a
source form/pass lever, not a register carrier. Parent batch gate still required.

    python3 tools/gcc3_batch20_resampler_audit.py

ResamplerTiming next finite domains, before compilation: addPhase's body is
instruction-for-instruction equal until final wrap/no-wrap store/return layout.
Cross whole-predicate early return versus guarded body, and local double phase
ownership (single final member write after wrap) versus direct member updates.
Three alternatives plus historical baseline; full predicate retains unordered
routing. No arithmetic reassociation or width changes.

timingCorrection's blob explicitly writes step0 then step1 in the even arm;
baseline constant0/increment collapses to one store1. A distinct source mechanism
is normalizing the original even counter with %=2 or &=1, rather than assigning
literal zero. Cross that three-spelling finite semantic domain with odd-first
versus whole-predicate even-first arms (blob even fallthrough). Six including
shared historical baseline. Only even values reach this normalization, so all
three yield zero; preserve the unbounded odd arm increment. Do not add volatile
or arbitrary store permutations merely to force a dead store.

    python3 tools/gcc3_batch20_timing_reproduce.py --domain docs/batch20-v90-small-boundaries.md

Initial local-phase generator accidentally changed phases to nextPhases and was
rejected by the compiler. Preserve invalid-generator artifact, exclude it, fix
replacement to whole identifier only, and rerun raw baseline/all finite cells.

Resampler history-allocation constructors: borrowed-bank blob174B has a single
common history store after optional malloc, keeping the returned pointer live
for inlined reset. Baseline172B stores zero before the guard and allocation result
inside the arm, then reloads history for reset. This is an observable source
ownership/use boundary. Cross shared local allocation result separately in
borrowed/owned-bank constructors with historyLen guard form (ternary versus
default=minHistory; if(taps>=minHistory) =10*taps). Eight complete TUs, original
raw baseline included. Guard form is motivated by blob's unsigned branch to
skip10*taps arm; do not permute independent constructor stores. No template/header
changes; both clones and all exported template copies/bystanders must be audited.

    python3 tools/gcc3_batch20_resampler_ctor_reproduce.py --domain docs/batch20-v90-small-boundaries.md

V92Precoder::reset(params) has blob232B straight-line transfers versus baseline
123B with two loops. Historical F3602's unroll observation under an earlier
profile cannot establish scalar source or current retained-profile codegen.
Finite expansion domain: retain six-transfer loop, expand preserving head/LC
pairs, expand with LC/head pairs, or expand separate LC then head / head then LC
groups, crossed with retained12-transfer coefficient loop versus scalar copies.
Ten cells including historical baseline. LC-first group boundary is object-first:
blob copies LC0..4 before head0 and interleaves LC5 among first heads; source can
carry group boundaries and scheduling can interleave. This is an exhaustive
small set of transfer-group organizations, not25!store fitting. Review direct
member owners, all loads/stores and any alias-sensitive semantic implications
before adopting a cell. No declaration/header/profile/FloatFIR changes.

    python3 tools/gcc3_batch20_precoder_reproduce.py --domain docs/batch20-v90-small-boundaries.md

Resampler borrowed constructor's local-history cross reaches equal174B but
retains an opening cohort/vptr difference. New independent C++ lifetime boundary:
initialize all five borrowed-bank parameter-derived members in the initializer
list versus body assignments, crossed with existing common-history pointer.
Four controls. Blob places coeffs bank copy before its vptr store; member
initialization precedes the constructor body and can expose this scheduling
boundary without arbitrary member permutations. Initializer list follows field
order; no header changes, reset edits or owned-constructor statement permutations.

    python3 tools/gcc3_batch20_resampler_init_reproduce.py --domain docs/batch20-v90-small-boundaries.md

V90 history wrapper source boundary: blob gets timing offset before loading
history index for increment; baseline defines next before the getter and carries
it across the call. Late next definition is object-supported, not register fit.
Three controls: historical baseline, next local after history assignment/getter,
and direct member increment with conditional wrap after assignment/getter. Getter
is reconstructed read-only, so moving the local past it preserves reachable
behavior; final original call/load ordering remains primary evidence. Direct
member form may expose extra transient stores and must be audited separately.

    python3 tools/gcc3_batch20_v90_history_reproduce.py --domain docs/batch20-v90-small-boundaries.md

Three history cells: direct late member increment is exact170B; late local
remains nonexact. Before adoption, cross all three history cells with the proved
explicit sign-statement Std winner. Six total cells; final combined TU must
retain both gains and all other bodies/data/relocs before production source edit.

Combined six-cell cross passes: late direct-member increment plus explicit
Std sign statements retain both exact170B/66B gains. Full TU13→15/16 exact,
no losses; fourteen bystanders unchanged, all metadata/data/vtable/switch-table
relocations unchanged; original zero/-1 literals only. Seventy-two RTL records
and six full-TU audit controls pass. Adopted late member increment source; root
still owns final production rebuild and period/structural gate.

    python3 tools/gcc3_batch20_v90_history_audit.py

Closed additional pool domains (unadopted): timing phase-return/counter nine
cells no gains; modulo/mask odd-first reaches191B/BYTES146. Constructor common
allocation-owner/guard eight cells no gains; common pointer reaches borrowed
174B/BYTES44 and owned373B/BYTES247. Member-init/common-owner four cells no gain.
Precoder transfer-group/scalar ten cells no gains; LC/head groups plus scalar
coefficients reach232B/BYTES70. Thirty-one negative complete-TU audits preserve
all prior exact bodies, named data, metadata and anonymous constants/relocs.
No speculative winner is adopted merely for equal size.

    python3 tools/gcc3_batch20_resampler_negative_audit.py

Final distinct precoder process CFG domain: blob normal three-symbol bounds
fall through and final coset-symbol bounds are out of line; source tests final
symbol first. Cross normal-first versus final-first arms independently for
bounds and candidate index expressions (two i>2 conditionals). Four cells;
swap full predicate and arms, preserving arithmetic, narrowing and all extra
precision. This is finite CFG organization at observed parity boundaries,
not changing best/point/sum float widths that F1372 already settled.

    python3 tools/gcc3_batch20_precoder_cfg_reproduce.py --domain docs/batch20-v90-small-boundaries.md

All four precoder CFG cells complete with no gain/loss, no adoption. Whole pool
negative audit now35/35 complete TUs. Member-init and CFG generators both had
precompile selection guards fail before any candidate compiled; fixed constructor
scope and complemented-predicate matching, then reran historical raw baselines.

Fresh ConnectionEvaluator: retrain twin blobs136B retain default verdict4 before
threshold and fallback5 before debug call, whereas baseline111B uses immediate
early-return constants. Cross shared versus duplicated counter cleanup with an
owned verdict defined at entry or after retrain increment; fallback verdict is
defined before the original print. Four candidates plus shared historical raw
baseline; twins use the same idiom, both must match independently. No other store
or parameter/header modifications.

updateAvePdsnr blob113B has weighted hot fallthrough and reloads member count
for the first product; baseline106B uses cold zero-count early return and caches
the count. Cross hot-first if/else versus baseline early return, and explicit
member product read versus cached count product. Four including same raw
baseline. Preserve entire extended-precision arithmetic and exactly one float
result store; no local float intermediates or algebraic regrouping. Eight total
independent cells across the two families, not arbitrary cross-function fitting.

    python3 tools/gcc3_batch20_connection_reproduce.py --domain docs/batch20-v90-small-boundaries.md

Initial eight cells yield three independent gains: retrain shared cleanup plus
entry verdict matches both136B twins; post-counter verdict matches local only.
Average hot-first/member product matches113B; either component alone misses.
Before adoption add ninth combined cell, completing baseline/twin/average/both
factor cross. Must retain all three and thirteen bystanders in same TU.

Combined proof passes nine full TUs, sixteen symbols each; adopted winner8→11/16
exact with all three gains113/136/136B. Thirteen winner bystanders unchanged;
metadata/named data/all allocated nontext constants and relocations unchanged.
One negative post-counter/shared-cleanup control changes nonexact later
evaluateMeanErrorStdPhase3 by scratch-register selection plus independent final
zero-store scheduling; unchanged instruction count69 and SIZE17 verdict retained.
Entry-verdict winner does not alter that bystander. All negative artifacts stay
available; audit reports this carrier instead of claiming all negatives unchanged.
162RTL records and nine initial count-member-reference controls pass: member-
product source adds a distinct initial load reference (four including stores vs
baseline three). Adopted three bodies exactly as combined replay source. No
arithmetic precision change, extra diagnostics, flags, headers or templates.

    python3 tools/gcc3_batch20_connection_audit.py
