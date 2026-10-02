# V.90 predicate and nearest-entry controls

F11663 and F11664 record two independent bounded source domains at 0a9b564f. Both use the complete translation unit, saved production .build-config, Gentoo GCC 3.4.2-r2, executed assembler 2.15.92.0.2 and DSPLIB_REPRODUCE_BUGS. Neither is adopted into reconstruction source; production remains 925/1852 strict exact, and the prior fixed deciding gate remains 386/0.

## Capability predicate: logical versus eager OR

[Declared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5957733687): exactly two cells replace only `||` with `|` between the existing comparisons in V90PreFilter::isV90WithEia6. The blob at 0x451fd..0x45214 evaluates the parameter comparison unconditionally and combines two SETE results using byte OR. The baseline short-circuits the second read.

The candidate recovers the unconditional read and the 69-byte extent (baseline75), but uses a 32-bit OR followed by AND1 rather than the blob's byte OR followed by MOVZBL. Strict verdict is BYTES7, not EXACT; this is more than a register rename. Inlined getV90Capability also changes, remaining125B versus122B. Exactness remains5/16. All14 sibling bodies, zero named data objects, symbol metadata, allocated nontext contents and canonical relocations agree. Raw baseline reproduces; both valid cells emit distinct objects.

The compiler-stage audit locates the width difference before allocation: initial RTL already contains IOR:SI, while combine retains it and adds AND:SI1 when merging the zero extensions of the SETE results. Allocated RTL preserves that pair. The baseline has no IOR at these stages. GCC3.4.2 expr.c routes BIT_IOR_EXPR/TRUTH_OR_EXPR to ior_optab; the period dumps, rather than an assumed allocator heuristic, establish the stage here. A narrower return declaration is still unproved by the symbol name, which does not encode it.

The existing run_iseia6 component fixture traverses16 codecs, each selected loop plus two negative selectors, and five registry values. It supplies valid parameter storage and deliberately marks a table capability to exercise both answers; those table modifications are explicit component probes. It does not establish modem lifecycle or equivalence for missing parameter storage. No candidate runtime claim is made. Close the operator-only family; recovering eager evaluation does not establish the Boolean representation or original return declaration.

Reproduce:

```sh
python3 tools/playbook_prefilter_eager.py --domain https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5957733687
python3 tools/playbook_prefilter_eager_audit.py
```

Artifacts: build/playbook-prefilter-eager/results.json and complete-object-audit.json.

## Nearest-entry search: unsupported initial index

[Declared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5957757154): exactly two cells remove only `= 5` from the unsigned-short result index in unSuspectedPhaseNearestLinMapp. The blob has no initial result-slot store. For valid phase rows0..5, abs(short input) lies in0..32768 and each mapping value in−32768..32767. Every distance is therefore at most65536, below the1,000,000 initial best sentinel. The first iteration necessarily assigns the result; removing the initializer does not introduce an unset read within this leaf boundary.

The unsupported store disappears, but the complete function remains133B versus132B. Scheduling and loop alignment change as well; do not interpret removed-instruction count as a size prediction. Its inlined updateAltRbsPhaseInDil body changes but remains1073B versus1124B. Exactness remains12/34; all32 sibling bodies, zero named data objects, symbol metadata, allocated nontext contents and canonical relocations agree. The full raw baseline reproduces production and the two valid cells emit distinct objects. No source adoption or runtime claim.

The first baseline with diagnostic `-da` failed with a period compiler segmentation-fault ICE. Preserve it under build/playbook-adid-nearest-init-invalid-da and exclude it from results. Removing diagnostic dump generation, while retaining every production code-generation flag, gives a raw-identical baseline. The shared driver now exposes DUMP_FLAGS with its original `-da` default; this domain explicitly selects no dumps. Do not change source to accommodate a compiler diagnostic crash, and do not call a failed baseline a source finding.

Existing t_v90adid coverage includes64 direct nearest calls and64 inlined-consumer calls on seeded storage without constructor execution. They remain synthetic component fidelity probes. A constructed-owner leaf witness would be required before adopting a justified candidate; none is added for this rejected source change. The six-function screen in this34-function TU is a selected sample, not exhaustion of the remainder.

Reproduce:

```sh
python3 tools/playbook_adid_nearest_init.py --domain https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5957757154
python3 tools/playbook_adid_nearest_audit.py
```

Artifacts: build/playbook-adid-nearest-init/results.json and complete-object-audit.json. Close this initializer-only family without changing widths, declaration order, sentinel or absolute-value spelling.

## Scope review

These independent source predictions both fire without full-body recovery. Together with the prior minimum-level reload control, they justify returning to compiler-stage and independently typed ownership evidence instead of extending local spelling families. The next discriminator for the capability predicate is the Boolean-width lowering stage and its inlined consumer, with return-declaration evidence checked before any type experiment. For the nearest search, only fresh allocation/owner evidence would reopen the residual; the removed initializer alone is not an adoption argument. No finite sample proves a global byte-exactness ceiling.

## F11665: result conversion and operand capture follow-up

At0c5d71b4, three sequential domains test the newly located width/folding mechanism. Domains were posted before each compilation: [four-cell result cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5957865775), [Boolean predicate capture](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5957919643), and [integer value capture](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5957948389). There are eight valid full-TU compilations, six distinct source variants and six distinct raw emissions; repeated baselines reproduce production.

Keep the int signature. Blob enterRRN tests EAX at0x386e8 and enterPhase3 at0x1b770; enterPhase3 separately narrows a second result with CWTL. Those consumers support the wide return convention. The callee's OR8/MOVZBL alone does not establish a bool API.

| Cell | Bytes, blob69 | Initial RTL OR | Outcome |
| --- | ---: | --- | --- |
| Baseline logical OR | 75 | none | short-circuit branches |
| Direct eager OR | 69 | SI | OR32/AND1 residual |
| Boolean result, logical OR | 74 | none | branches remain |
| Boolean result, eager OR | 72 | QI | OR8 returns, extra widening remains |
| Captured Boolean predicate, logical OR | 76 | none | unconditional read, branches remain |
| Captured integer value, logical OR | 69 | SI | eager fold, OR32/AND1 remains |

The initial, combine and allocated RTL checks establish the respective OR modes. Only explicit Boolean result plus eager OR produces IOR:QI, yet its complete body has an extra widening and scheduling differences. All cells remain5/16 exact. The inline getV90Capability body changes in each candidate; all14 other bodies remain unchanged. Symbol metadata, zero named data, allocated nontext payloads/sizes and reviewed relocation destinations agree.

Some bool-result cells move32 .rodata dispatch targets by16 bytes. An initial absolute-section-addend equality assertion correctly failed. The independent audit then checks that each target belongs to exactly one sized named function, lands at a decoded instruction boundary and retains the same function-relative offset. This explains relocation layout movement without masking a destination change. This review-only identity is not inserted into byteident or its grading.

[GCC3.4.2 fold-const.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/fold-const.c) explains why the capture distinction matters: fold_truthop first requires two comparison trees; its unconditional-evaluation path then requires simple RHS operands and BRANCH_COST≥2. simple_operand_p accepts constants and suitable local declarations, not the original memory expression. A bare bool predicate local fails the comparison-tree gate. The integer local passes it and changes the initial instruction graph, but does not narrow the result. [i386.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.c) gives pentiumpro/i686 branch cost2 already, so adding mbranch-cost2 would not be a discriminating experiment. The real period dumps establish the behavior; the stock release source supplies the mechanism, not a claim that every Gentoo patch is absent.

Reproduce with the three playbook_prefilter_{bool,operand,value}.py tools and their posted domain URLs, followed by:

```sh
python3 tools/playbook_prefilter_capture_audit.py
```

Per-domain results.json and complete-object-stage-audit.json live under build/playbook-prefilter-{bool,operand,value}. All capture/result domains are closed. No source adoption, candidate runtime, return-type change or exact gain. Do not extend the family with declaration/order/register/cast permutations. The next work moves to independent owner/call-boundary evidence; no global optimum is inferred. Production remains925/1852, all300 object hashes unchanged, prior fixed deciding gate386/0.
