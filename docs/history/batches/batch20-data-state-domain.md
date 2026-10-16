# Data-state batch controls

Baseline 93d7eee1. Ownership excludes all paths named in the root batch20
ownership document. No shared headers change. No fuzzing or mutation execution.

## V23 transmitter creation: two source boundary axes

The complete blob initializer writes period_len, space, period, mark,
period_index, resume, remaining, mute; current source writes these in a
materially different order. Its configuration overrides src before scale,
whereas current source overrides scale before src. Cross those two observed
boundaries (4 complete-TU cells including baseline). Hold guards, tone creation,
all values/types, table/data bytes, and function definition order fixed.
Prediction: initializer/store source order alters scheduling before reload;
configuration override order alters only its final two stores. Accept only a
complete raw canonical byte/relocation exact body, with all-TU bystander/data,
binding and relocation controls. Close on a negative cross rather than extend
arbitrary permutations.

## V32 creation: options update and config-store sequence

The blob updates only byte1 of options with and/or byte instructions; current
combined mask-or expression updates the full dword. Cross a split options
clear/set (compiler is free to narrow each operation) with the observed
configuration store sequence protocol, phys_delay, options, timeout,
energy_drop_time, rx_rate, tx_rate, trellis. Four cells; no parameter/header
retyping, byte pointer casts, or bitfields. This is an independent expression
boundary, not a declaration permutation. Hold config copy, values, and callee
arguments unchanged. Compare every complete TU body and close if no exact hit.

## V23 deleted original diagnostics

The blob's relocation records establish diagnostic gates/calls absent from
source, where comments explicitly say they were dropped. Recover exact format
bytes from the object's own string sections, retain gate >1 and their original
statement positions. First controls: baseline and restored creation message in
v23rx.c; baseline and restored entry message in v23.c. Unlike source-order
cells these deliberately add measured debug imports/string data, which must be
validated against the blob rather than asserted unchanged. No invented message
or compilation timestamp substituted into the object.

## V23 receiver creation extension: new stack and initializer evidence

Diagnostic restoration reaches the full581B size but leaves43 differing bytes:
its three address-taken configurations occupy different stack regions. Blob
fsd@10,mrf@30,tone@40, versus current tone@10,fsd@40,mrf@60. Enumerate the
complete3! declaration order domain, crossed with original diagnostic presence,
tone src/ratio/freq versus current freq/ratio/src overrides, and iir_coeff
before rx_state versus current reverse stores. 48 cells including exact
baseline; no unrelated decl/count/type or arbitrary statement permutations.
Hold helpercalls/loops/values fixed. This is new independent allocation evidence,
not retry of the initial two-cell diagnostic domain.

## V23 wrapper independent member-store discriminator

Restored debug entry gives complete274B but ten remaining bytes occur only in
initial id/modem loads and stores. Cross their relative source assignment
order with debug restoration (4 cells); caller and op stores remain in place.
All source values and allocation/error paths remain fixed.

## V23 receiver ratio: independent literal fact

The 48-cell winner family leaves exactly one byte: movw immediate0x7146 vs
blob0x7148 at0x86e5e. The current V23RX_TONE_RATIO28998 is incorrect; blob29000
is also recorded by the original F23 discussion. Cross literal correction
with the measured complete lifetime/diagnostic winner (4cells, baseline
included). This source value change is accepted only if the complete body
becomes exact; final batch period constructor fixture must pass.

## V23 composite and process diagnostics

Three independent relocated original sites in v23modem.c: creation banner with
recovered fixed build strings15:48:09/Sep22 2005; answer-tone announcement
inside answer_tone branch; state-change diagnostic comparing reported/state
and updating reported only on a change. Cross2^3=8 cells. Build-string values
are historical metadata recovered from the blob, not current compiler time.
No loops/types/helperfactoring change.

Two independent sites in v23.c: entry diagnostic and changed-status diagnostic.
Cross2^2=4 cells; status update conditional matches blob's status comparison,
return on unchanged path unchanged. Preserve all error/lifecycle behavior.

## Measured so far

V23 TX cross4: baselineBYTES44, cfgBYTES33, storesBYTES11, combinedEXACT255B.
All3functions/1nameddata/15canonicalrelocations stable; only create changes.
V32cross4: split raw-body merges baseline; store sequence changes create alone
and remainsSIZE1. Full6functions/8nameddata/170canonicalrelocations stable.
No V32 source change adopted.

V23 RX48-cell config/debug cross gives two1-byte misses, both201-debug-tone
(tone,mrf,fsd declaration order), member store axis merges. Every other body
retains its baseline exactness status. The independent ratio4-cell control
requires both corrected ratio29000 and boundaries to recoverEXACT581B. Wrong
ratio only and boundaries only miss. Candidate adoption preserves existing
rx_state/iir_coeff member order because that axis is unidentifiable.

## V23 constructor arm polarity, independent CFG evidence

Both original constructor diagnostics restore524B but remaining differences
are exactly opposite allocation-arm layout and tone-scale ternary polarity:
blob executes host path afterJE onmode0, currentdoesafterJNE. Cross creation
and answer diagnostics as one complete recovered boundary with swapped null
allocation sub-branches (mode==0 terminal first), and tone-scale ternary
(mode==0?0:scale). 8cells baseline included, all inputs/sideeffects fixed.
No functiondefinitionorder change.

## B103 original state-announcement sites

Original four originate and three answer case tails print the entering case
label (not its new state), while default prints "default\n". Each gate is >1.
The blob tail-merges these into one sibling call plus a default out-of-line
call per function. Cross restoring all sites in each function independently,
2^2=4 complete-TU cells. Do not force tail merging, source labels/values,
transition store order, or unrelated helper inline choices.

CreateV23Modem8-cell control: diagnostics aloneBYTES52, branch axes alone
negative; debug+nullarm+tone-ternaryEXACT524B. Constructor branch polarity also
restores deferred static table emission order: fwthenbw (blob.data777c/7782),
previous baselinebwthenfw. Named values/bindings unchanged; physical data
order improvement explicitly measured rather than called unchanged.

B103 state4cells: baselinebothSIZE, org-onlyoriginateEXACT295B, answer-only
SIZE7, combinedoriginateEXACT295B+answerEXACT237B. Onlythese two functionbodies
change, all17functionscored,4→6exact,zero losses. Answer-only close miss may
reflect TU peepholecursor or shared-tail emission; do not claim uniquemechanism
without staged evidence.

The first v23.c mapped-status debug control was invalid: original prints raw
V23ModemMain return while compare/store uses mappedDPSTAT. Two preserved cells
in batch20-v23-composite are explicitly excluded from valid preimagecount and
from source adoption. Correct rawstatus carrier remains a new independent
follow-up, also requiring original int-return public boundary investigation.

Full-TU audit:90validcells+2invalidexcluded; rawbaselinesreproduced; metadata,
bindings/visibility, nameddata values, nonstringdata and canonical nontext
relocations stable apart from constructor two-table order recovery above.
All added literalstrings exist byte-for-byte in blob; added textrelocs only
original debuggates/calls/strings. Five adoptedfunctions1892B,zeroexactlosses.
Major periodgate and global productioncensus deferred to rootintegrated batch.

## V23 receiver progress original diagnostics

Enumerate each of three original gates independently per TU (8cells each,
16total). Forward: acquisition timeout, detected-tone counter (signed rx_state/5
narrowed to short before vararg promotion), energy loss. Backward: detected390Hz
tone, acquisition timeout, silence timeout. Literal bytes/argument boundaries
come from blob relocation/gate instructions. Keep retained void AGC API and
signal-field reload; F11614's return-consumption domain is closed and not
reopened merely to fit diagnostics. No header/count/return-type edits.

## V23 wrapper complete emission order

Root's complete production census independently observes wrapper's five
functions emitted init/create/exit/delete/process versus blob
create/delete/process/init/exit. No closed wrapper whole-order cross found:
F8121 controls static table visibility; F8120 is DeleteV23Modem branch polarity.
Cross the one complete object-supported function definition order with authentic
entry diagnostic (4cells). No arbitrary permutations. All original helper
bodies/values/types/imports/exports and table pointers remain in scope.

Progress diagnostic spelling audit fired on four forward cells: experiment
mistyped the first character of the format asV23 rather than the originalv23.
Preserve build/batch20-v23-progress as invalid-for-the-four-tonecells; do not
interpret their metrics. Correct toolliteral from exact relocation111bc and
rerun complete16-cell controls in batch20-v23-progress-literal-fixed. This is
a source literal transcription repair, not an optimization-domain extension.

V23 complete wrapperorder×entry4cells reproduces exact emittedfive-function
sequence but neither target becomesexact; entrybothordersBYTES10 remainsonly
id/modem load/store scheduling. No sourceadoption; notproofemissionorderirrelevant
to everyfunction, only a measured closed family here.

## MRF formal-slot width versus narrowing at use: new API evidence

Root allocates candidate-only fpm_mrf.h/fpm_mrf.c plus b103fp.h. Callee reads
count signedword, but that does not distinguish shortformal from intformal
narrowed toshort at use. Blob B1032calls zeroextend nsamples and both wrappers
zeroextend their returns while source formalshort MRF forces signextension and
signed wrapperreturn forces CWTL. Blob also reloads fp->dsp acrossFSM call.

Cross3formalpreimages (retainedshort; unsignedshort+explicitshortremaining;
int+explicitshortremaining), unsigned B1032returns, directownerexpressions,
and omitted redundant short nsamplescast. 3x2x2x2=24cells on complete B103prc
and MRF TUs. Sharedheaderoverlays consistent with everydefinition. No untyped
or incompatibledeclarations, no arbitraryABIcast. Hypothesis acceptance must
show callee canonical/rawcontrol and all12blobcallsitefunctions across9caller
TUs (V32int,v23rx,B103prc,Rxcid,V17r_int,V21r_int,V21t_int,V27r_int,V29r_int).
Controls are exploratory: full300productionrebuild requiredbeforeadoption.

MRF24x2TU cross closes both B103 transmit helpers with either intformal or
unsignedshortformal+narrow-at-use; no single original formalwidth established.
Every24 MRFobjectrawmerges retainedobject; body stillSIZE87 toblob. Intformal
selected for broadercontrol because all explicitshortcasts retain promotion,
where unsignedshortformals also change unrelatedDemodDataB103. Selectedint
winner changes five B103bodies, twoexact,zero losses over17.

Extendselectedwinner to all9callerTUs+MRF (10TUs), baseline/winner/combined
controls (30compilations). Combined retains preceding adoptedB103 case logs,
V23RXconstructor. V23 composite creation lives in a separate TU. Everybody/export/data/canonical
relocation and headerfingerprint audited before production. No source change
in fax/CID/V32 consumers; candidate-only header overlay necessarily reaches
these existing consumers without reopening reconstruction.

Initial allconsumer generator reused the24-cell formal generator with a two-cell
axis map and hit its countassertion before anycandidatecompiler execution.
Preserve build/batch20-mrf-consumers as invalid-preflight; correctedwrapper
selectsbaseline/winner from immutable24-cell source family and reruns under
batch20-mrf-consumers-validated. No failedcell included in validdenominators.

### MRF integrated caller extension and V21 width control

The 10-TU × three-cell consistent API extension completed. The helper and all
8 other consumer TUs merge raw; B103 exact pair survives but diagnostic-only
AnswerNextState becomes SIZE13 in the combined TU. Both objects place that
function at0x940: address alignment is not the explanation. After normalizing
only dump addresses, source locations, and alias-set IDs, its first divergence
is peephole2 (scratch dx→cx and ax→dx); preceding flow2 agrees. Preserve this
interaction, with global scratch-selector state the next mechanism to inspect.

The blob's ModDataV21 zero-extends AX immediately before MRF at+0x2f and again
on return; both transmit functions currently force `(short)nsamples` at that
boundary and are SIZE2. New bounded four-cell domain crosses retained-short /
consistent-int MRF formal with retained / omitted nsamples casts in these two
functions only. No other fax source or header changes. Compare the complete
V21t_int TU and retain the old callee raw-identity proof; formal type remains
non-unique from the helper's narrowing alone.

### B103 composite creation banner

The blob's 2151-byte B103FP_create has one gate>1 diagnostic tail call with
`B103FP version: %s %s\n`, date `Sep 22 2005`, time `15:48:11`. Source omits
that call. Two cells retain/restore this literal diagnostic at final return;
no other source or header axis. This authentic independent omitted operation
can change the shared scratch cursor and will be checked with all prior
B103 body-boundary candidates before any integrated exact count.

V21 four-cell result: retained/count-only/int-only all0/2exact; combined
int-count closes ModDataV21(87B) andTxNoCarrierV21(103B), no bystander functions
or data. MRF complete audit82cells passes bindings/visibility/type, named data
values/canonical relocs, allocated nontext, explicit changed-body bounds and
zero baseline exact losses. The combined B103 Answer provisional loss remains
separate; first changed pass observed by tools/batch20_b103_mrf_stages.py over
30passdumps is28.peephole2. This matches existing F7812/Playbook lever3b, rather
than a new register-allocation mechanism. Do not repair by padding or counters.

Parent authorized production int MRF + explicit short use narrowing,
unsigned B103 returns/direct owner reloads, and both V21 unsigned count handoffs.
These recover four99/124/87/103-byte bodies. Combined with diagnostics three V23
constructors and B103 Originate are still exact, total8gains over93d7eee1,
2068bytes; Answer237B provisional diagnostics-only win is not counted.
All major gates and complete300-TU/header-consumer rebuild remain parent work.

B103 composite banner-only result: SIZE229→177, no exact gain,4/17exact
unchanged; authentic missing debug preimage retained but not adopted. Further
constructor work needs an independent object-supported source boundary, not
arbitrary local-order trials.

### Authentic B103 constructor banner × integrated source boundaries

The banner-only2cells were negative on raw93d7baseline. They were never crossed
with independently recovered unsignedreturns/directowner/MRFcount boundaries
and original state diagnostics; those materially change early TU caller bodies.
New3cells: historical rawbaseline; integrated source/API; integrated plus exact
original B103FP_create versionbanner. Fixed consistentint MRF/unsignedB103 API
forbothintegratedcells; noinlineflags, constructorlocal/order fitting, padding,
syntheticcalls or cursor shifts. Parent authorizes this genuine omitted source
operation crossing before additional V22leads. CompleteTU controls decide any
additional exactgain; preserve knownbaseline and integratedgain set.

Integrated authentic constructor-banner crossing negative: three cells retain
4/17historical baseline and7/17integrated functions, no additional gain/loss.
Banner changes only B103FP_create, SIZE229→177; Answer staysSIZE13. Full-TU
metadata/data/nontext/canonicalrelocs preserved except original debug literals
and imports. Earlier8productiongains remain intact; banner notadopted.
