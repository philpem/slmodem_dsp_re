# Helper-boundary transfer and destination evaluation timing

Baseline ec575202; retained Gentoo profile, selected assembler and mandatory
bug define. The [predeclared review](boundary-transfer-domain.md) contains
46 complete-TU cells across13 TUs,406 shared-reference verdicts and418 emitted
body comparisons. All unchanged raw baselines reproduce. Source/header/object
hashes, complete commands and dumps are retained under the three build roots
named by the reproduction tools.

## Exact recovery: DemodDataV21,217 bytes

Three independent boundaries explain the residual:

1. The original consumes the AGC's explicit int result; the source substituted
   a load of the published field.
2. The original MRF outgoing count is unsigned-low-word; the source explicitly
   sign-extended it, although the callee narrows signed at use.
3. The original reloads the destination DSP owner after AGC (a5772), before
   storing EAX (a5778). A direct member assignment captures the destination
   owner before the call in this compiler.

The first four count/result controls leave SIZE9. Two additional controls use
a block-local int return before evaluating the destination. The local alone
restores that read age but leaves SIZE9; local plus unsigned MRF handoff is
**EXACT217**, including canonical relocation targets. The two TU bystanders
remain unchanged. Source statement sequencing, not a forced register or layout,
accounts for the missing boundary. It does not imply a unique local name or
source spelling, and native register names were not used as the acceptance rule.

The fresh-EAX audit initially mistook a later reload into EAX for the original
return. That failed assertion is retained in boundary-transfer-audit-invalid-eax.log;
the corrected detector stops at intervening EAX writes and checks destination
read ordering. It distinguishes baseline, direct assignment, cached return and
the original. No conclusion is drawn from the invalid audit.

## Other adopted boundaries

These controls restore original operands or publications without exact closure.
A size gap is not an instruction-distance metric; larger gaps are kept visible.

| Function | Adopted boundary | Before → after |
| --- | --- | --- |
| DemodDataB103 | Unsigned MRF count | SIZE70→86 |
| DemodDataV17 | AGC result and unsigned MRF count | SIZE32→19 |
| DemodDataV27 | AGC result and unsigned MRF count | SIZE26→8 |
| DemodDataV29 | AGC result and unsigned MRF count | SIZE18→2 |
| cid_modem | Original unsigned MRF count | SIZE54→54 |
| DemodDataV22 | Full incoming MRF slot, signed-word use inside callee | SIZE3→BYTES231 |
| TxNextStateV21 | Three post-diagnostic owner reloads | SIZE3→27 |
| GenEQTrnSequenceV27 | Signed fill index, postdecrement cursor, rate reread | SIZE15→11 |
| FPM_TONE_find_rev | Add sample energy before subtracting history energy | BYTES463→SIZE36 |
| GenericIIR<float,double>::process(float) | Publish member accumulator during the computation | SIZE99→85 |

V22's caller/formal2x2 controls reproduce the entire three-function callee TU
raw-identically in every cell. Only the combined caller/API control changes
DemodDataV22. The int incoming formal with explicit signed-word narrowing at
use recovers outgoing slot bits without widening internal processing; the
original formal width is not uniquely established.

The AGC result review independently confirms that V32 and B103 originally read
the signal field, while V17/V21/V27/V29 consume EAX. Only those four result
consumers change. The current int/four-argument API makes this coherent; older
claims that unread slots proved a three-argument void definition were unsupported.
D490/D1030/D1031/D1036/D1091/D1094 now mark that inference as historical and retain
its earlier reasoning below the correction.

TX's original root+20 owner loads after known-state debug calls match the three
restored loads. Its unchanged-source inlined callers also change: Data SIZE69→78,
Idle SIZE13→4, Start SIZE15→23. Their source and direct-call inventories agree;
no exact caller is lost. Default behavior and the current short state carrier
remain fixed. Existing modem-cycle and underrun cases repeat at debug2, without
new inputs, state histories, assertions or transcript claims.

The V27 training control preserves the original high-count defect: bit15 counts
can drive signed fill-index wrap and nontermination/invalid accesses. Those
counts are not executed as tests. Existing fixed component cases retain the
original next-word access and output bounds. Original INC/CWTL, unsigned
postdecrement output cursor and per-output rate reads determine the source
changes; the SIZE1 postdecrement-only control is not selected for its size.

Tone's native separate ADD/SUB update pair is absent in baseline and present
once in both original and candidate. Two static mutation locators follow the
new spelling with unchanged labels and mutation meaning; no mutants execute.
IIR's original has four nonpopping accumulator FSTL sites, baseline two, candidate
four; initial RTL member publications also increase. All other17 emitted IIR
bodies remain unchanged (11 shared-reference and six without original counterparts). Only a numeric-zero comparison pool changes. The
standalone IIR period test passes, followed by the full permitted gate.

## Withheld and stopped leads

The V32 unsigned MRF control changes three unrelated-source siblings and loses
SetAdaptEqV32's exact identity to BYTES7. Its metadata/data/call audit passes,
but that collateral is unresolved. Do not adopt it or search declarations and
register layouts to conceal the loss. It remains a measured supported boundary
with an unaccepted current-TU control.

The new destination screen covers299 C/C++ source files and nominates seven
nested assignments in five functions across three files. Its direct-V21 control
fires once; the explicit-cache control clears. Native review of V21RX_create,
connect_2400/connect_1200, v22_originate and FDSP_DP_Create finds their destination
owners already captured before calls in the original, agreeing with the source.
The V21 result fix therefore does not transfer blindly to those sites.

A separate read-only indexed-assignment sweep found18 sites in13 named source
functions/helpers. V8 word/extension arrays, V32 registers, V34 timing_out,
Detector tone pointers, ECC coefficient pointers and call-progress timeouts are
embedded arrays. The relevant local owner/index values are stable, or the small
extension helper is inlined. V32 GetSequence stores and both V34 timing-filter
stores agree with original destination capture. ECC init and RxHdxSequenceE
are already exact. Two V8 helpers are not separately emitted. No additional
source experiment is justified by these nominations.

The older FSE configuration-copy trace places its remaining ordering difference
in sched2 after allocation; it supplies no new source boundary. Previous inline,
profile, local-type, spectral and AGC smoother domains stay closed. This exhausts
the supported transfer queue and the new assignment-timing nominations, not all
possible source hypotheses or the global compiler-profile question.

## Period selection guard

An initial ten-name targeted run contained three nonexistent names. The old
runner printed warnings but returned7 passed/0 failed, so that run is explicitly
incomplete and excluded as a ten-test gate. The log and requested/actual
metadata are preserved. Source conclusions use the final valid387-binary gate.

period.sh now validates the complete explicit T selection before Docker. Any
unknown name fails without a partial run. Two host self-tests cover unknown-only
and mixed valid/unknown lists, with a fake Docker executable proving Docker is
never invoked. Both pass. The final387-binary selection is the positive control
and reports387 requested/0 unknown plus387 passed/0 failed. The new
period-selection-selftest target is a structural phase dependency and executes
no modem or fuzz workload.

```
python3 tools/gcc3_boundary_transfer.py --domain docs/boundary-transfer-domain.md
python3 tools/gcc3_v22_boundary_transfer.py --domain docs/boundary-transfer-domain.md
python3 tools/gcc3_publication_partial_review.py --domain docs/boundary-transfer-domain.md
python3 tools/gcc3_boundary_transfer_audit.py
python3 tools/nested_call_destination_screen.py
make period-selection-selftest
```

Historical replay after header drift needs --historical-headers and an archived
raw baseline through --baseline-dir. Generated artifacts remain under build;
no compiler-profile, raw-byte padding, forced-register or unsupported fixture
controls are used.

A C++ single-arrow follow-up covers72 source files and12 raw sites: four are
V90PW/V90PF macro/cast matches, five invoke the inlined/read-only maxK helper,
and the remaining three use stable formal destination pointers (SNR results,
getAT_UD results and CP packing). The actual helper bodies and parameter shapes
supply no further mutable destination-owner witness. No compile matrix follows.

## Aggregate validation

Final permitted period is387 passed/0 failed with all387 requested names valid,
explicitly excluding t_fuzz. TX's same-input debug repetitions add155+8 passing
checks. IIR's targeted period test also passes. No inputs, tolerances or existing
assertions are weakened. Static anchors retain285 suites/10038 locations; only
two Tone locators retarget, preserving labels and mutation meanings. No mutation
or fuzzing execution, snapshot update, full-phase or portability claim.

Strict census is1086/1852 EXACT,120322 original exact bytes: +1 function/+217B,
zero adopted exact losses;667 SIZE/65 BYTES/29 REGALLOC/5 UNRESOLVED. Exactly
11/300 canonical objects change,289 remain raw-identical. Twelve selected TU
controls raw-match production, including the unchanged V22 callee; all300
faithful inputs raw-match canonical. The compiler profile is unchanged.

Partial text702524→702476 bytes; allocated PROGBITS918526→918484. Positioned
equality falls69544→69472/943398; both strict comparisons are DIFFERENT/exit1.
The new exact function and recovered source boundaries therefore do not imply
a whole-object positional-byte gain. The full-TU audit preserves the V32 exact
loss as rejected collateral rather than hiding it in the aggregate count.
