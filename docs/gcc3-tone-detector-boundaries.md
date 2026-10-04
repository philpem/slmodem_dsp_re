# GenericToneDetector statement boundaries

Baseline 80c5dea3, complete GenericToneDetector.cpp TU, saved period profile.
F1991 closes swapped predicates and inverted arms: no repeats. This domain
instead crosses independently observed value lifetimes. Constructor blob has
branch-separated quotient stores whereas baseline if-converts local increment
to ADC; test keeping quotient in blocks1/blocks2 during its multiply-back test.
Single-sample blob compares meanOut against threshold before smoother stores;
test explicit predicate evaluation before those stores. Blob evaluates output
square/accumulation before input square/accumulation; test that boundary too.
Eight cells: constructor local/member quotient x process baseline/predicate-first
x input/output accumulation order. No flags, signatures, constants, FP expression
operators or source definition order change. Baseline must raw reproduce;
measure all bodies, exported metadata, named data and relocations. No adoption
without exact full body and no bystander exact losses. Close domain if no exact.

First cross: 8/8 valid, raw baseline reproduced; 0 gains/losses. Member quotient
alone changes constructor SIZE35 to BYTES19 and aligns the complete branch CFG.
The remaining first difference is reset's inlined integer count store preceding
its four accumulator zero stores; blob has the reverse, while its out-of-line
reset has the original order. Independent next boundary: cross member quotient
with reset count_2c before/after four accumulator stores and four separate/shared
float-zero assignment. Eight cells; test both constructor clones AND reset, no
claim that matching inline tail can excuse reset loss. Preserve prior controls.

Result: 16 valid complete TU cells, 14 source hashes, 8 object hashes across
both crosses (the baseline/member quotient controls recur). C1 and C2 become
byte-exact at 267 bytes each only for member-quotient + late-count. No exact
bystander losses. Out-of-line reset remains byte-identical to the baseline/blob.
Shared float-zero assignment loses reset exactness, retained as negative control.

The branch change occurs in ce2: baseline's two local quotient-increment
branches survive combine but disappear in ce2; member quotient retains both.
Late count shifts the initial RTL store order from count/four accumulators to
four accumulators/count; the constructor's final order becomes the blob's,
without changing the emitted out-of-line reset. Audit covers 16 complete TUs
with seven functions each, identical named data, metadata and allocated nontext
bytes and no nontext relocations; all text bodies are canonical-relocation
compared. Thirty-six selected constructor stage records cover both clones and
six stages for baseline/member quotient/exact combined. Ten explicit ce2 and
initial-store controls pass. Process source boundary controls do not close its
SIZE16 residual; no predicate/accumulation synonyms adopted.

Replay:

    python3 tools/gcc3_tone_detector_reproduce.py --domain docs/gcc3-tone-detector-boundaries.md
    python3 tools/gcc3_tone_detector_reproduce.py --reset-cross --domain docs/gcc3-tone-detector-boundaries.md
    python3 tools/gcc3_tone_detector_audit.py

Retain the merged-baseline production archive/config with --baseline-dir
build/production-before when replaying both reproduction commands after adoption. Major functional/structural gates run once on the
parent's completed batch, not once for each source-control cell.
