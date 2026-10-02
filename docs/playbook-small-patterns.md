# Small-function playbook pass (2026-10-02)

This is a bounded source-pattern pass, separate from the V34 deep dive and
active V8 work. Baseline 555036b4: 853/1852 exact functions, 82,921 exact bytes.
No fuzzing, mutation execution or excluded-directory inspection.

## Screening and scope

300 comparison objects were screened. Path exclusions for active V34/V8,
V90/V92 work and deferred fax leave 493 shared symbols. Of these, 167
non-exact bodies have reference sizes 35..450 bytes and absolute length
gaps at most 32. This is an eligibility filter, not a closeness ranking or
source-recoverability claim. The curated shortlist below has 20 bodies;
all have structural differences under the canonical live-range comparison.
Known register-only cases such as FloatFIR::reset were left outside the
five investigation families. The classifier fires on both that known
register-only case and the structural FIFO8_write control.

| Symbol | Blob/ours bytes | Initial verdict | Disposition |
| --- | --- | --- | --- |
| `RenegotiateDetectV32` | 44/43 | SIZE(1) | Common-result family; EXACT candidate |
| `RetrainDetectV32` | 53/52 | SIZE(1) | Common-result family; non-exact neighbor not adopted |
| `silence_is_more_then` | 66/65 | SIZE(1) | Screened reserve; no compile domain opened |
| `FIFO8_write` | 149/153 | SIZE(4) | Wrap/return family; API ambiguity, no adoption |
| `SetPulseBreakTime` | 88/90 | SIZE(2) | Cached owner family; local recovery, still non-exact |
| `SetPulseMakeTime` | 88/90 | SIZE(2) | Cached owner family; EXACT candidate |
| `check_for_valid` | 52/50 | SIZE(2) | Predicate family; no gain |
| `_ZN8FloatFIR15setCoefficientsEPfj` | 58/56 | SIZE(2) | Geometry/minimum family; no exact candidate |
| `_ZN8FloatIIR15setCoefficientsEPfj` | 58/56 | SIZE(2) | Geometry/minimum family; EXACT candidate |
| `RxHdxStartB103` | 139/138 | SIZE(1) | Screened reserve; no compile domain opened |
| `cid_get_strings` | 145/146 | SIZE(1) | Screened reserve; no compile domain opened |
| `GenerateCallingTone` | 215/214 | SIZE(1) | Screened reserve; no compile domain opened |
| `ModDataV22` | 95/95 | BYTES(59) | Screened reserve; no compile domain opened |
| `TxHdxTRN` | 237/237 | BYTES(45) | Screened reserve; no compile domain opened |
| `v23FP_tx_create` | 255/255 | BYTES(44) | Screened reserve; no compile domain opened |
| `DetSequence` | 275/275 | BYTES(214) | Screened reserve; no compile domain opened |
| `RxHdxSequenceE` | 339/339 | BYTES(49) | Screened reserve; no compile domain opened |
| `V22FP_modem` | 346/346 | BYTES(6) | Screened reserve; no compile domain opened |
| `RxClampV22` | 44/40 | SIZE(4) | Screened reserve; no compile domain opened |
| `TxNOP` | 44/40 | SIZE(4) | Screened reserve; no compile domain opened |

The five families cover eight related functions. Reserve candidates remain
uninvestigated by this pass; this ledger prevents a shortlist from being
mistaken for 20 completed experiments. Prior register/emission-order closures
were checked before opening each domain. The FloatFIR/FloatIIR historical
ordering controls did not test the conditional clamp or successful-return CFG.

Replay screening with tools/playbook_shortlist.py --baseline-report <canonical
byteident JSON> --json-out <path>. It deliberately reports current objects;
after retention three former candidates no longer appear as non-exact.
Original census/shortlist: /tmp/playbook-candidates.json and
/tmp/playbook-shortlist20.json; their explicit results are preserved above.

## Parent domains: owners, common returns and predicates

Predeclared domain:
https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5942376827.
Three cells per full TU (nine valid cells): unchanged control and two finite
source alternatives. Gentoo GCC3.4.2-r2 and executed assembler2.15.92.0.2,
complete retained flags and mandatory bug define via shared helpers. Every
raw unchanged full TU reproduces its retained object. Headers include V32's
unchanged source-local v32fpdisp-common.h; all are hashed.

Pulse cached owner before debug, whether declared in the main scope or a
NULL-guarded inner block, gains SetPulseMakeTime (88 bytes) with no losses.
Both cells keep the same 11 function/7 global inventories and all shared comparison symbols. Only
SetPulseBreakTime/SetPulseMakeTime bodies change. Break's size becomes 88
against blob88 but its prologue register/store scheduling remains non-exact;
the same owner correction is independently observed in both blob bodies.
The public opaque handle remains opaque.

V32 initialized-result/common-exit with result declared before owner loads
gains RenegotiateDetectV32 (44 bytes); declaration after the loads does not.
The paired controls change both pollers, with no exact losses. Their request
masks, stores and unsigned counter behavior remain unchanged. Retention
isolates only the exact renegotiation source and leaves RetrainDetectV32
untouched; the final full-TU check must confirm that isolation retains the hit.

Beepgen check_for_valid's conjunction and initialized-result branch both
preserve 7/23 exact with no gains/losses. They do not recover the complete
blob body. Neither is adopted. The input remains a seven-word window.

Initial V32 baseline compilation omitted the source-local header. The mixed
run was preserved at build/playbook-parent-patterns-invalid-local-header/
and /tmp/playbook-parent-patterns-invalid-local-header.log and excluded from
accepted results. Copying the pinned, hashed local header restores valid raw
baseline reproduction. The incidental exception-reporting dis.py shadow was
fixed outside reconstruction source. Valid nine-cell replay:
tools/playbook_small_patterns.py --domain <URL>; complete commands/hashes,
RTL, body/relocation verdicts and bindings in build/playbook-parent-patterns/.

## Filter geometry and minimum recovery

Filter coefficient source-family result (baseline555036b4)

Prior closure screen: findings’ FloatFIR full emission-order negative and FloatIIR120-ordering study concern upstream emission carriers; neither tests conditional clamp or successful-return graph. Object-first comparison establishes identical58-byte FIR/IIR reference setters: after buffer rejection, compare old tap count, replace coefficient pointer, conditionally change geometry, unconditionally store signed minimum index, and share final return0.

Three bounded domains declared in issue22 before compilation: initial8cells crossing conditional/ternary clamp and cached equality bool;4 unsigned-count-cache alternatives plus2 repeated raw controls;4 nested-if geometry alternatives plus2 repeated raw controls. URLs5942366465/5942389625/5942400842. All20 valid compile cells preserve raw unchanged TU baseline separately, actual full retained C/C++ flags/mandatory bug define, executed GCC3.4.2-r2 and assembler2.15.92.0.2. Each FIR cell preserves8functions/8globals and2/8exact except target outcome; IIR preserves18functions/18globals (12shared blob symbols),5/12exact baseline. Every alternative changes only its setter; no exact losses or missing/new symbols.

Initial domain: FIR baseline SIZE2, min BYTES26, cachedbool SIZE33, both SIZE42; IIR baseline SIZE2, min SIZE2, cachedbool SIZE33,both SIZE42. Ternary creates the common unconditional index store, but early successful return introduces zero-result definition before geometry comparison. Cachedbool falsifies flags-preservation prediction: SETE+TEST adds boolean live range and spills; knowninput detector sees SETE1 versus baseline0.

Unsigned count-cache extension: both TUs conditional SIZE27/minimum SIZE26, noexactgain/loss. It preserves old tap load before pointer store but requires an extra live register and leaves comparison after store. No adoption, finite family closed.

Final return-graph domain: both TUs nested conditional SIZE4. FIR nestedminimum SIZE2, nohit. IIR nestedminimum EXACT58bytes,6/12exact (+1, no losses), only setter changed. It retains coefficient pointer assignment outside a nested if(m_ncoeff!=n), changes geometry in that arm, uses ternary signed minimum, then returns0 once. Final instruction/equality/pointer/min graph matches reference byte for byte, confirming recovered source family under retained flags. Exact candidate build/playbook-filter-nested/FloatIIR/nested-minimum/FloatIIR.cpp. No source/header/test edits by investigating agent; production adoption requires parent’s period differential gate. FIR’s same nested minimum still has room-local operand/lifetime differences; do not adopt nearestsize or expand domains this pass.

Earliest artifacts: complete .01.rtl and per-setter setCoefficients-initial.rtl show ternary selection and common store graph before allocation; successful early return and shared-return candidate have different return-value definitions from expansion. The exact output, not SIZE change, decides adoption. Existing valid-input behavior unchanged (same failed capacity rejection, pointer replacement on equal taps, signed clamp result); unconditional same-value store is the blob’s observed graph, with no claims about concurrency/volatile fields.

First final-domain invocation accidentally named wrong issue-commentURL in invocationmetadata though correct domain was declared; preserved /tmp/playbook-filter-nested-invalid-domain.json and initial log, excluded as invalidmetadata. Correct-URL replay confirms same exact candidate. Mainvalid results under build/playbook-filter-coefficients, build/playbook-filter-count-carrier, build/playbook-filter-nested; aggregate analysis.json in firstdirectory. Tool tools/playbook_filter_coefficients.py supports original, --count-carrier, --nested; all source generators assert distinct source count and inventory controls, known bool SETE control fires. Domain closed, no more permutations.


## FIFO return ambiguity limits adoption

- baseline: SIZE(4), no exact gains/losses.
- short-prefix-if: SIZE(1), no exact gains/losses.
- unsigned-short-separate-if: SIZE(3), no exact gains/losses.
- unsigned-short-prefix-if: SIZE(2), no exact gains/losses.
- int-separate-if: SIZE(3), no exact gains/losses.
- int-prefix-if: SIZE(2), no exact gains/losses.

Six predeclared short/unsigned-short/int return x separate/prefix increment
cells preserve all four functions/globals and 2/4 exact. Prefix increment
inside the wrap condition restores the blob's branch instead of setb/neg/and.
Unsigned short and int each remove the sign-extending return and produce
raw-identical full objects at each wrap choice. The sole internal blob caller,
voice_tx, ignores the result; independently reconstructed reference fixture
prototypes use short and consume low16 bits. These facts do not identify the
original public API. No source/header adoption, no consumer ABI migration.
Temporary header overlays and their hashes are diagnostic only.
Domain https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5942399974;
replay tools/playbook_fifo_write.py --domain <URL>. Artifacts
build/playbook-fifo-write/{results,analysis}.json. Stop at six cells; this
negative does not establish a source-recovery ceiling.

## Retention controls

Final combination declared before rebuild:
https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5942431811.
Cached owners for the pulse pair, exact FloatIIR nested geometry/minimum,
and only the exact V32 renegotiation source are combined under retained flags.
No FloatFIR, Retrain, detector or FIFO near-match is retained. The combined
full-TU census is build/playbook-adoption/combined.json. Period, whole-tree
and partial-link results follow after completion.


## Independent review and validation

The parent corrected the filter driver's PATH argument: the original helper
was passed an executable-shaped string although it exports a directory then
invokes gcc. All original domain artifacts/logs were preserved under
/tmp/playbook-filter-invalidcompilerpath and excluded. The corrected driver
uses the recorded Gentoo compiler directory. All 20 cells were replayed,
including six raw controls, and all 20 object hashes and verdicts reproduce
exactly. The original-domain replay also exposed a stale generator-variable
assertion; it was repaired to check each cell's label and the rejected run
preserved. Corrected known SETE/no-SETE controls now fire. This is apparatus
repair, not a new source/flag domain. The parent independently rescored the
three retained TUs with canonical body/relocation comparison.

Final combined inventories: call.c 11 functions/7 globals, 11 shared symbols;
FloatIIR.cpp 18/18, 12 shared; V32int.c 25/31, 25 shared. All preserved.
Only four bodies change: SetPulseBreakTime, SetPulseMakeTime,
RenegotiateDetectV32 and FloatIIR::setCoefficients. Isolating the V32 winner
preserves its exact hit and leaves RetrainDetectV32 raw/canonical unchanged.

Canonical whole-tree result: **853/1852 -> 856/1852**, exact bytes
**82,921 -> 83,111**, exactly the three named gains, no losses. Final report
build/playbook-adoption/tree-after.json. A post-retention screen reports
164 remaining small non-exact bodies and 17 remaining shortlist entries;
its register-only/structural positive controls both fire. This is not a
whole-tree ceiling or a classification of every remaining non-exact symbol.

make tc: 300/300 objects, zero failures. Initial fixed make phase passes
385 period fixtures but fails structural checking on one detached V32
counter metadata anchor. The anchor is retargeted to the same renegotiation
arm and same counter mutation, with new indentation/common result, not
executed and no stored mutation verdict refreshed. Final fixed period/
structural gate passes 385/0 with all non-fuzz fixtures explicitly selected;
10038 anchors/285 suites are clean. The final preparation wrappers overlapped
while checking the same unchanged candidate; the standalone final make phase
wrapper exits0. No merged-compiler attribution or 770-fixture claim is made.

Before/after partial links include all 300 objects in the same recovered
order, overriding only the three unchanged baseline objects for the before
control. Positioned equal bytes **68,209 -> 68,215 /943,398**; candidate
allocated bytes 914,158 unchanged. Exact section records70/92, relocation
records1,018/18,317 and symbol records394/2,907 unchanged. Both strict
partial comparisons are **DIFFERENT (exit1)**; these function gains do not
establish final-object identity. Artifacts build/playbook-adoption/partial/.
Reproduce with tools/tuattrib.py, recoverorder.py, partiallink.sh and
partialcmp.py using the saved baseline/current manifests and three raw
baseline objects named in combined.json.

All five finite families are closed for this pass. The remaining twelve
reserve symbols have been screened, not attempted. Resume only from a
new object/pass observation; do not reopen FloatFIR operand ordering,
Retrain scheduling or FIFO return types as nearest-score spelling searches.


Final structural cross-reference census: 14,218 references/2,670 finding
headings, zero dangling/pending/stale entries. Source/header inventories
unchanged; one static anchor retargeted. Tool syntax and whitespace checks
pass. No modern portability claim is made by this period reconstruction pass.
The historical byte-ident ratchet's pre-existing V90 constructor loss from
PR238 is not re-blessed; the direct comparison to the saved branch baseline
preserves every baseline exact name and adds exactly three.
