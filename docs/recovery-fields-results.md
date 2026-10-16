# Field audit and partial reconstruction results

Branch `investigate/recovery-fields`, based on `eccf0f2a`.
Three Sol agents audited field representations and implemented supported
refactors. A second agent independently reviewed the CID/Tone IIR source
changes and their original operand witnesses. Integrated source checkpoint:
`7799604c`.

**Three function bodies now recover more of the original instruction graph;
the exact set remains 1,093/1,852, with zero gains or losses.** The full
Gentoo period/structural gate passes 388 binaries with zero failures.
No new misplaced-field error was established by this bounded audit.

## Retained changes

| Function | Previous length | Retained length | Original length | Supported recovery |
| --- | ---: | ---: | ---: | --- |
| `CID_FSD_demodulate` | 795 | 1022 | 1049 | Separate coefficient arms and wrap pointers derived from terminal history cursors |
| `toneiir_progress` | 673 | 1101 | 1141 | Four physical stages and sixteen-bit tap induction |
| `_iir_filter_progress` | 306 | 799 | 802 | Four stages, sixteen-bit induction and captured shift-array owner |

These are lengths, not differing-byte counts or a similarity percentage.
Each retained operation has independent original instruction evidence;
matching the whole body remains unfinished. Explicit source stages are one
supported representation, not proof that the original author wrote them
instead of using a loop that the compiler unrolled.

The old finite domains remain closed. Explicit partial-reconstruction
authorisation changed their adoption criterion, so the fixed candidates were
revalidated against current headers and complete TUs. It did not justify
another spelling or compiler-option search. Five complete-TU controls pass
their data, relocation, binding and exact-set audits; a deliberately corrupted
data byte demonstrates the audit firing. The focused period selection passes
11/11. See [the source evidence and replay](recovery-call-partials.md).

`v22fp_hdx::rms_accum` now uses an unsigned representation consistent with
all audited full-width arithmetic, removing compensating casts. All 14 V.22
objects remain raw-identical across 28 compiler cells, covering 61 shared
symbols and 27 exact names. The focused two-test period selection passes.
Signed storage with explicit casts remains a possible original declaration;
this is clearer modelling, not a newly demonstrated behavioural fix or exact
gain. See [the complete writer/use census](recovery-data-fields.md).

## Field hypotheses and Playbook

The historical V.34/FSK offset leads do not justify changing their fields:
word stores and low-word arithmetic explain the apparent extension mismatch.
The five-field V.90/V.92 screen found four widths supported by independent
writers/readers, while the fifth retains an explicit signedness uncertainty.
The call/service checks also declined narrow-field widening where original
stores and reads agree. None establishes that every remaining field is right.
[V.90/V.92 evidence](recovery-fields-v90.md) and the family reports preserve
the ownership paths and original addresses.

The [Playbook](method/refinement.md) now records how to separate backing
storage from use-site narrowing, audit cross-class ownership, retain a
byte-neutral representation, and adopt validated partial graphs without
reopening closed searches. It also records the preceding identity-proof
lessons. Historical CID and Tone IIR reports link to the later partial
adoption while preserving their old results.

## Integrated validation

`make phase partial-link J=4` with the retained Gentoo image exits zero:
388 period binaries pass, all default structural checks pass, and all 300
sources build. Static locator validation covers 285 suites and 10,040
locators, with zero skipped, nonunique, vacuous or wrong-arm results.
No mutation execution, fixture change, tolerance weakening or modern
portability result is claimed.

The final census is 1,093 EXACT, 662 SIZE, 65 BYTES, 29 REGALLOC and three
UNRESOLVED, covering 1,852 names. Exact original bytes remain
123,672/720,125. Every prior exact name survives; the ratchet is unchanged.
All 300 input hashes remain stable across the census. Exactly two objects
change, `toneiir.c` and `Cidfsd.c`; both raw-match their independently tested
candidates. The other 298 objects and complete compiler configuration remain
raw-identical to the frozen baseline, including all V.22 consumers.

Strict partial comparison still exits one: semantic and raw identity remain
DIFFERENT. It reports 69,660/943,398 equal positioned bytes, 421/2,931 exact
symbol records and 993/18,317 exact relocation records. Positioned counts
move with layout and must not be substituted for function or source fidelity.

Evidence is under `build/recovery-fields/`: `baseline/`,
`baseline-objects.json`, `integrated-gates.log`, `object-audit.json`,
`final-byteident.json`, `final-partial.json` and `final-checkpoint.json`.
Family reports retain the individual experiment commands and limits.

Next, expand the owner-aware writer/reader inventory and call-boundary
coverage beyond these bounded fields. Require a contradictory original
access before changing another layout. The V32LocLoopNextState and
V90Resampler fixed-frame proof targets remain separate opportunities; no
progress on those proofs is claimed by this source-refactor pass.
