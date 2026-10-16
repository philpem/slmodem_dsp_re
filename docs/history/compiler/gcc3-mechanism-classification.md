# Remaining function mechanisms at PR261

Read-only comparison of the same immutable300 complete Gentoo objects in
`../byteexact-batch100/build/tc_out` against the original object. Baseline
9f1199b5:1852 common symbols,1041 strict exact,811 nonexact. All copies are
scored with byteident's strict worst-copy ordering; the811 nonexact symbols
have819 emitted copies. Object hashes, complete saved build configuration,
blob hash, every copy's typed relocation verdict and full observed feature
vector are retained in `build/gcc3-mechanism-inventory.json`.

| Primary observed class | Symbols |
| --- | ---: |
| Different instruction mnemonic sequence |673|
| Size difference and different typed direct-call target/count |83|
| Complete existing alpha/register-renaming proof |40|
| Same mnemonic sequence, other operand/relocation difference |7|
| Unresolved selector destination identity |4|
| Register-erased shape agrees, alpha live-range constraints fail |2|
| Unresolved anonymous data destination identity |1|
| Only demonstrably dead ECX/EDX epilogue POP differs |1|
| Total nonexact |811|

The classes are observations, not complete causes. A size/call gap does not
prove inlining or its budget. A first alpha rejection does not identify the
whole mismatch. Every copy also reports complete instruction counts,
branch-mnemonic counts, direct typed callee multisets, indirect/local call
operands, register-erased shape, canonical relocation proof status and alpha
rejection. Categories use all failed copies. No best COMDAT copy is selected.
Only40/811 pass the existing complete alpha proof with established relocation
identity; this does not license an allocator-only explanation for the rest.
Alpha is the project's structural grade1 comparison, not an additional ABI or
behavioral equivalence proof. All40 have actual decoded operand differences,
not merely identical decoded rows with alternate padding/encoding.

Direct call classification uses only canonical typed R_386_PC32 destinations
and multiplicities. Printed relocated call-site offsets, indirect callback
registers and unrelocated local-call offsets are not callee identity. The
initial scan using those offsets was invalid and is preserved as
`gcc3-mechanism-inventory-invalid-callsite-normalization.json`; the intermediate
all-call-operands artifact is excluded from final classification. Neither
changed the strict1041/1852 byte-exact baseline. Final counts above come only
from the corrected immutable full scan.

## Detector controls

`python3 tools/gcc3_mechanism_classify.py --self-test` fires4 positives and7
negatives: dead scratch POP; actual live use, return-EAX, extra behavior and
calls rejected; alpha rename accepted and changed immediate rejected; same
typed callee at different printed sites accepted, distinct typed callees
separated, indirect register difference excluded from helper-target evidence.
`build/gcc3-mechanism-control-results.json` retains the augmented control run;
the completed census embeds its earlier8 successful controls separately.
No generated program is executed.

Dead-POP detection is deliberately narrower than alpha. Both lengths and all
other nonpadding rows must be identical; canonical relocation identity and
function size must agree. Only ECX/EDX destination changes qualify. Neither
family may occur on either suffix, and the suffix permits only MOV/POP,
constant ADD ESP and RET with no calls/branches/unknown operations. This
establishes a dead operand candidate; the compiler pass still needs tracing.
The actual positive is applyPadGainToLinMapp:141B, row39 original POP EDX,
ours POP ECX, followed by POP EBX, POP ESI, RET. Playbook~805 already names
this function in the F78xx cursor mechanism; it is not a fresh source family.

## Ranked original witnesses

`build/gcc3-mechanism-ranked-witnesses.json` records a reviewed diagnostic
shortlist and closure status, independent of the automatic structural ranking
in the complete inventory. No source preimage is claimed unique.

1. **v22_ans_rmloop2 and voice_modem:** every mnemonic and register-erased
   operand agrees. Alpha refuses pinned EBP destination in the former and a
   live-range use conflict in the latter. Trace actual ABI saves, returns and
   GCC CFG/lifetimes before treating these as a comparator limitation or a
   source difference. Do not relax alpha to count them as gains.
2. **ParallelDifferentialDecoder<unsigned char>::process:**57B original,
   sparse mnemonic MOV versus MOVZBL at aligned row15. Trace full-width
   working value versus byte formal/use through expansion and combine;
   public template types alone do not determine the working carrier. Prior
   closed-family audit is required before a source experiment.
3. **updateLinMappMeanAndVarAlt:**118B original, FLDS/FADDP versus direct
   FADDS around rounding0.5. Identify original constant relocation/precision
   and lifetime first; floating operand ownership and scheduler effects remain
   competing explanations. Prior closed-family audit is required.
4. **Four unresolved selectors:**RxNextStateV17(730B), V32LocLoopNextState
   (794B), V90Resampler::setBllState(693B), and
   V90ConstellationPower::getConstellationInfo(227B). Establish concrete
   frame/dispatch/call/table ownership before extending the bounded prover.
   These are unproved measurements, not established source mismatches. Do
   not enter other-session source/issue22 work.
5. **getSegmentPointer:**115B, unproved anonymous .rodata destination,
   separate from selector tables. Prove array extent, object ownership and
   every pointed-to destination before canonicalizing identity.
6. **applyPadGainToLinMapp:**positive detector control for the known dead-POP
   mechanism. Reopening needs new earlier TU scratch history, not dead locals
   or declaration/definition permutation.
7. **Constellation clearing counters:**fresh unsigned-versus-signed loop
   witness, now measured closed in four full-TU controls. See
   [cross and complete audit](gcc3-mechanism-constellation-domain.md).
   Both APIs and their inline consumer recover original JBE, but gain zero
   exact functions. An extra MOV alone supplies no new index owner/lifetime.

## Reproduction

```
python3 tools/gcc3_mechanism_classify.py --self-test
python3 tools/gcc3_mechanism_classify.py \
  --objects ../byteexact-batch100/build/tc_out \
  --expected-nonexact 811
```

The assertion deliberately refuses a moved baseline instead of silently replacing
the denominator. This inventory changes no comparator, source, header, flags
or exact count. No runtime, mutation, fuzzing or phase gate was executed by
this analysis. The only compilation was the explicitly authorized four-cell
constellation source overlay, raw full-TU baseline reproduced and audited.
