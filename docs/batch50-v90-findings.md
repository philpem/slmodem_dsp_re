# Batch50 V90 findings (numbers allocated by parent)

Final contribution:17 source gains in7 TUs/145 bodies, zero losses. Full audit144 valid cells; parent combined gate/census pending. Old/new apparatus archive973/1852 unchanged.

Base902f47fa, original period profile/.build-config with DSPLIB_REPRODUCE_BUGS, Gentoo3.4.2-r2 and invoked binutils2.15.92.0.2. Every finite matrix reproduces retained full TU bytes in its untouched control; headers hash to pinned revision. Parent runs batch period/structural gate and strict whole-census. No fuzz/mutation execution or committed code here.

## Explicit signed mask clamp: two128B exact setters

V90MappingParamsInt::setConstellationMask/setCodecConstellationMask change ternary `c=which<6?which:0` to a converted copy followed by `if(which>=6)c=0`. Four cells: ternary, boolean product, boolean mask and explicit clamp. Product/mask are raw-identical to baseline; explicit alone yields SETL/NEG/AND and exact whole loop layout. All eleven TU functions reviewed: only these two change. Named data, allocated nontext, canonical relocations and every nine bystanders unchanged. Negative selector values preserved. Getter unsigned zeroing-index witness checked three further cells crossed with clamp: still not exact; not adopted.

## Count initialization ownership: one33B getter

V90PreFilter::getNofRefLoops initializes zero count before obtaining the loop bank. Blob clears EDX early, walks pointer in EAX and returns through MOV EDX,EAX. Baseline31B initializes count after bank load. Early-count and crossed explicit pointer-walk cells both reproduce33B exactly; adopt early-count minimal source. Sixteen TU bodies reviewed, fifteen bystanders unchanged; metadata/data/relocations unchanged3/3 cells.

## Covered unsigned cycle result: six exact leaves

V90 Sd/SdNot and V92 Ru/RuNot/Su/SuNot previously returned through short-valued early-return helpers with defensive default zero. Preserve explicit narrow negative-amplitude casts but let each switch assign a common int result, then return it. Unsigned modulo6 proves all inputs select a labelled case; no reachable uninitialized read. Cross helperwidth-only and initialized-common-result controls against no-default common result. Width-only and initialized controls fail; covered result reproduces all six leaf instruction streams. V92 four immediately strict EXACT (88,88,128,133B), V90 two130B require newly supported bounded fixed-frame table proof (separate apparatus finding). Eight fullTU controls over28+22 functions: metadata/named data/allocated sizes unchanged; only anonymous .rodata owner-relative destinations shift in changed nonexact generator bodies; raw table bytes/order unchanged. Nonexact generators improve size gaps V90 39->35 and114->110; V92 49->16. No exact losses.

## Direct member vector / primitive ownership: one80B Jd

Original generateJd computes unsigned modulo72 before fetching the member vector, then owns full polarity update and narrow return. Current vectorBit(pointer,count) evaluates and retains pointer before division, forcing a second saved register. Merely replacing member vector helper expressions reproduces Jd but loses exact JdNot (negative axis retained). Direct primitive body at Jd alone reproduces80B with no losses; cross with confirmed Sd results retains all three gains. Five vector-only and seven direct-primitive completeTU cells retain all outcomes. Minimal Jd-only body adopted. Scrambler<h,int> C1 COMDAT carrier changes BYTES22->15 while remaining nonexact; same binding/size/reloc destinations, no other copy change inferred. Parent must retain worst-copy census. Other V92 vector siblings remain BYTES14; no source adoption for them.

## Dispatcher narrowing: one45B gain

V90Phase3Modulator::generateSymbol originally calls each int child, then CWTL before returning. An explicit short cast on either branch preserves the short-valued child contract and prevents the current sibling jump from omitting that boundary. Branch casts, a common short result and a conditional cast each independently reproduce45B; cross all three with covered cycles and direct Jd. Seven completeTU cells, no exact losses in the adopted branch-cast winner. Four gains in the combined28-body TU.

## Verdict initialization across a phase scan: one57B gain

V90AutoDigitalImpDetector::isThereAnyAltRbsPhase originally clears an independent int verdict before accumulating six short flags and conditionally sets it afterward. Declare result0 before the sum, assign1 only on a positive sum, then return it. Four crossed cells with isAltRbs: phase-only exactly57B, sample-only leaves BYTES2 and is not adopted. Six additional comparison-complement/reject-first controls do not close that sibling. The adopted34-body TU changes only the phase scan; data and canonical relocations remain identical. This TU uses initial RTL -dr: its existing full -da compiler ICE is retained as a limitation, not interpreted as source evidence.

## Closed diagnostics and encoder controls

EchoCanceller fractional forward/reverse subtraction crossed with float/longdouble operands:4 fullTU cells,0 gains or losses. Float expression cells change anonymous constant-pool representations; preserve as negative precision evidence, not full-object identical. No adoption. V92CP byte float encoder if/switch mode crossed with whole-predicate complement/subtraction-first:4 fullTU cells,0 gains or losses. Getter unsigned index:3 cells,0 extra gains.

## Replay

Unique gcc3_batch50_v90_{masks,prefilter,cycle,vector,vector_direct,mask_index,echo_frac,cp}.py tools each require its declared docs/batch50-v90-*-domain.md. Complete audit: `python3 tools/gcc3_batch50_v90_audit.py`. Fixed-frame apparatus: docs/batch50-v90-fixed-frame-proof.md and new jumptable_batch50_stack_controls.py. Artifacts preserved under build/gcc3-batch50-v90-* and build/jumptable-batch50-stack-controls. See final totals above; the selected source manifest and complete audit are current. Archived300-object census: old/new prover973/1852, identical exact-name sets, zero apparatus-only gains. Positive manifest: build/batch50-v90-positive-manifest.json; full audit: build/batch50-v90-complete-audit.json.

## Additional bounded negative domains

V92CP byte-output alias boundary retains table weight across the char store in the original. Cached-weight/subtract-first crossed with pointer ownership, branch complement and case-index lifetime controls improve shape but do not reproduce the function; none adopted. calcK logarithm product-first and long-double retained-result domains, V92Echo reset history count initialization, V90Modem setSessionFlag switch/return boundaries, and V92PreFilter FIR hot-arm/inclusive-copy/do-copy crossings all yield zero gains. Preserve invalid CP global-counter-removal compile and interrupted ENOSPC k-narrow run separately; corrected valid controls are counted in87. No register fitting or previously closed declaration-order domains expanded.

## SD and spectral follow-up controls

SdDetector's descending independent pointer endpoints, count decrement after endpoint capture, delayed accumulator initialization and common verdict recover original180B but retain two comparison/Jcc bytes. Four initial cells, eight ownership crossings, five count/acceptance crossings and three below-limit live-return controls all fail exactness; no source adoption. This separates recoverable loop/source-use skeleton from the remaining GCC branch canonicalization. Spectral shaping immutable float coefficient captures still promote to XFmode under long-double consumers; six cells fail, as do four native-float recurrence crossings. No precision/source changes adopted. Complete audit expands to117 valid cells; the original12 positive manifest remains unchanged.

## R detector member update plus positive group guard: four gains

F8085 return-variable/cast synonyms were raw-inert and left the original signBits reload open. Cross a direct ushort member shift/OR against a positive complete-group guard whose incomplete else stores the count and reaches a shared return. Baseline and both individual axes miss; direct member updates alone restore the original member reload but retain a literal-zero early exit (BYTES2), while positive guard alone retains the local-forwarding mismatch. Their crossing alone reproduces all four detectors (detectR164B, detectRf164B, detectRNot113B, detectRfNot117B), yielding9/9 exact TU symbols. The narrowing after shift preserves the original ushort history, and both group paths retain identical stores/returns. Original store-before-count-branch and complete-group member reload now emerge naturally. Four fullTU controls retain metadata/data/canonical relocations and all five bystanders, no exact losses. This is a new source/CFG ownership interaction, not a retry of F8085's closed return-synonym domain.

Ja post-negation narrowing:7 valid independent/cycle-crossed cells, no additional gains or losses; no adoption. Full audit128 valid cells, selected6 TUs120 bodies,16 gains and zero losses.

## Literal expression mode at a diagnostic half: one190B gain

V90Modulator::enterDataPhase originally FLDS-loads the half offset before unsigned rate conversion and ends the offset with FADDP. Current0.5f becomes FADDS after conversion. Preserve the float reciprocal and use idiomatic double literal0.5 for the addition: exact190B, same numeric constant payloads and canonical relocations. Independently cross this change at the standalone entry and progress inline copy (four cells); entry-only hits, progress-only does not gain, both retain entry. Adopt the standalone one-literal change only. Full25-body TU audit preserves all24 bystanders, named data, nontext sizes and relocations; zero losses. This source expression mode recovers a load/use boundary that a mnemonic census could misclassify as x87 allocation alone. Final audit144 valid cells; selected7 TUs145 bodies,17 source gains, zero losses, zero apparatus-only gains.

PrintBase2 latch/bit-test4 and V92Jd branch-predicate grammars4+4 controls closed with no gain; source unchanged.

First-CSE localization and the transferable source/CFG rule: [R detector trace](batch50-v90-rdetector-cse-trace.md),8 preserved snapshots; the baseline complete-group member load is removed at firstCSE, while direct member publication retains it.
