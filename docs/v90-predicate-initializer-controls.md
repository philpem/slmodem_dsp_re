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
