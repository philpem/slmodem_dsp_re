# NONMATCH vector triage and the next source inquiry

The frozen vector measured all **607 selected functions / 611 defining copies** in **126.02 seconds**. No function or copy was unavailable, including `v34handshak`. Selection is the prior census NONMATCH set; this is a diagnostic batch, not a new acceptance census. All 300 actual period source objects were byte-identical to their faithful counterparts before being snapshotted. Native object, source files, build flags, source manifest and tool hashes are retained. No source cell, compile, differential test or gate was run by this audit.

The batch contains 370 DATA-reachable, 101 FAX-reachable, 36 OTHER-reachable and 100 unreached/exported function names, using the service roots and actual relocation graph. FAX was measured for denominator completeness and excluded from source admission. Reachability here is a static link closure, not runtime provenance.

Strict copy verdicts are **610 NONMATCH / 1 EXACT**. The EXACT copy is `Scrambler<unsigned char,unsigned char>` C1 in `V92Phase4Modulator.cpp`; the selected name has another nonmatching defining copy. This does not represent a promotion.

## Axis coverage

Observed means this diagnostic axis has no unknown records; it does not certify equal behavior or unique correspondence. All 611 instruction maps are explicitly heuristic and ambiguous. Padding remains in instruction streams.

| Axis | Observed copies | Not applicable | Partial copies |
| --- | ---: | ---: | ---: |
| encoding | 611 | 0 | 0 |
| mnemonics | 611 | 0 | 0 |
| typed_operations | 0 | 0 | 611 |
| immediates | 0 | 0 | 611 |
| addressing | 0 | 0 | 611 |
| gp_registers | 611 | 0 | 0 |
| x87_registers | 116 | 495 | 0 |
| stack_observations | 611 | 0 | 0 |
| branch_displacements | 175 | 4 | 432 |
| relocation_destinations | 420 | 123 | 68 |
| relocation_sites | 488 | 123 | 0 |
| cfg_edges | 118 | 0 | 493 |
| cfg_blocks | 133 | 0 | 478 |
| extent | 611 | 0 | 0 |
| placement | 611 | 0 | 0 |
| padding | 611 | 0 | 0 |

Both CFG axes are observed for **86/611 copies**. Canonical relocation destinations are observed for420, absent for123 and partial for68. Relocation-site positions are complete observations in all488 copies containing relocations, but site equality does not establish destination equality. Typed operations, immediate ownership and addressing have unknown evidence in every copy. The bounded decoder refuses unsupported encodings/calls/x87 effects; some ordinary XOR and accumulator-immediate forms also remain unknown. CFG incompleteness additionally includes indirect control and branch targets without an instruction correspondence. Explicit x87 register syntax is observed in116 copies; that is not x87 stack-effect proof.

The edge axis contains42,467 reference and42,165 candidate records:23,886 mapped matches,3,287 mismatches,3,551 unknown pairs,11,743 reference-only and11,441 candidate-only records. These are conditional on the heuristic map. They are not counts of actual semantic branch differences. `axis-coverage.json` retains every axis denominator and unknown count.

## Admitted inquiries and disposition

1. **FSE_decision_4pt: restore the measured signed-word search counter.** Native0x81080+0x109 increments via LEA, +0x10c sign-extends AX into ECX, and +0x10f compares CX against3. Source `int i,q,k` produces full-width INC/CMP. Only k runs0..3, indexes the two maps and copies to best; it has no escape or call. One ordinary short-k cell predicts both the conversion and word comparison. Keep D301 unequal SAR15/SAR16, owner observations and the differential helper untouched. This is a distinct conversion boundary allowed by F12129/F12130 reopening criteria, outside their closed quality/16-point/allocator domain. The one-cell validation subsequently restored this conversion and comparison and is retained as F12144. The function remains NONMATCH; this inquiry is complete, with no further counter spelling cell proposed.

2. **VPcmV34InterpretMohMessageBits: narrow the message carrier.** Native0x8970+0x16,+0x34,+0x3e,+0x54,+0x5e compare AX, with a separate MOVZWL AX,ECX at+0x20 for masks. Source uses `unsigned w=(unsigned short)bits[0]` and compares EAX. One unsigned-short carrier hypothesis is supported; original spelling remains non-unique. `t_v34info` already compares all65,536 first-word inputs and logged cases. Preserve D50:0x0150 is MHack,0x0133 is not MHreq. Do not narrow to a byte or change global aliasing.

3. **The same MOH decoder: preserve its ordered reason chain.** Native MHcld dispatch compares5, then6, then10, each with JE, before its reserved default. The current switch compares6 then partitions with JA. One ordered if/else chain is a specific source hypothesis, separate from the carrier. Preserve reason10 and reserved as separate diagnostic arms despite both storing2. Verify successors by following actual stores and strings; the vector pairing alone cannot prove this. No switch palette is admitted.

These are **three inquiry sites in two DATA functions**, not three independently recovered functions. No source correctness defect or grade promotion is claimed. `admissions.json` records predictions, falsifiers, existing test domains and whole-TU controls.

A lower-priority reserve is `TONE_create`: native captures signed fir_len once at+0x3f and carries it through allocations and MTK_phasor into the FIR bound, whereas source rereads it. Its eager allocation-guard spelling is already closed by batch50 and must not be repeated. Only the separate count-lifetime question remains potentially fresh; service OTHER keeps it behind DATA work, and no valid alias-based behavioral counterexample is asserted.

## Screens rejected after context/history review

The V.8 ANSam counter/cursor mismatch is real but its exact cross was tested in batch50. FPM_MTD_detect and constelBuild zero/sign extensions feed later narrowing; the upper bits are dead. Detect_Rmloop2_ACK SAR31/SAR15 and V34TimingPrefilter SAR16/SAR14 are different operations paired by the heuristic, not wrong shift constants. V92Phase4 generateE2u zero-load plus CWTL equals the signed-load sequence and its lifetime palettes are closed. DilReceived0x89b0/0x8990 are different branch arms, both present on both sides. Eleven rejection records and eight selected full JSON examples are retained.

The vector unblocks navigation to specific operand widths, branch conditions, known targets and relocation destinations while preserving unknowns. It does not infer a causal source change from a first delta, prove register liveness, or remove the need to trace complete successor/dataflow paths and inspect experiment history. No weighted score or diagnostic-to-grade conversion was used.

## Reproduction and evidence

- `input-provenance.json`, `freeze.json`: baseline, manifest/flags, all source/object hashes and eight frozen implementation hashes.
- `compiler-provenance.json`: immutable image and SHA256 of GCC driver, cc1 and cc1plus.
- `summary.json`, `axis-coverage.json`: selected/measured/available/partial denominators and all axis counts.
- `summary.jsonl`, `reports/*.json.gz`: every function and defining-copy vector.
- `selected-examples/index.json`: eight uncompressed review examples.
- `admissions.json`, `rejected-screens.json`: source inquiry contracts and closed/false-screen exclusions.

Image: `sha256:fe868cc44a48c36d1130862965729d0b384bc9e0a1908d37891aa5ae07352f16`. GCC driver SHA256: `a352c3e5d58c95b142e505e2be4374f9a620f1151f6e5fa2e9cab849f8ef1777`. Frozen diffvector SHA256: `b315e0072c7dcb1de779ecbd30284cc10ea393ff7410b147f6c0d4359e63994f`. Frozen byteident SHA256: `52fe59a2f5d63f06a2338263013d12945e07fb844a3550acacec8cd49c616bcd`.

## Next bounded wave

The 4-point counter cell has been validated and retained separately. The next
source wave should focus on **one MOH-decoder closure**, with its two measured
features crossed in at most four full-TU cells: unchanged control, unsigned-
short carrier only, ordered dispatch only, and both. Reproduce the unchanged
period object first. Preserve every message code, reserved/default arm, log
string and D50 outcome. The carrier is already fed a narrowed 16-bit value;
verify every use before changing its declaration.

Predictions are concrete: the carrier should recover AX comparisons and the
word-to-int mask boundary; ordered dispatch should recover the 5, 6, 10 sequence
and its actual successors. Check each feature separately from the whole grade.
Use `t_v34info`'s existing 65,536-word differential domain and log cases, then
review complete-TU bodies, data, bindings and consumers before retaining a cell.
Stop after that domain if neither the predicted feature nor a supported
whole-function improvement appears. No switch palette, flags change or
return-contract exception is admitted.

This report describes the frozen pre-counter-change population. A separately
refreshed acceptance census is required for any retained source; diagnostic
counts do not update that population or promote its one independently EXACT
COMDAT copy. The checker hardening is F12142, coverage is F12143, and the
retained 4-point partial is F12144.
