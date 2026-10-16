# AGC return ownership and word operands

Pinned source ad83fa7c, Gentoo GCC3.4.2-r2, retained profile and mandatory
DSPLIB_REPRODUCE_BUGS. [Domain](v23-agc-return-domain.md) separates this new V23
diagnostic context from the closed F11614 return-only family.

## Observations and adoption

Original FPM_AGC_agc has one epilogue: form a boolean, store EAX to signal, return
with that EAX. Both V23 consumers capture signed AX. The old void implementation
published the same field but left a pointer in EAX; its callers substituted field
loads. That substitute passed sample/state tests, but did not recover the original
return boundary. Restore the explicit int return and two signed-AX consumers,
without claiming a uniquely recovered formal type. All other callers' source and
all fourth arguments remain unchanged. No calling a void C implementation through
an int prototype to manufacture runtime evidence.

Sixty complete-TU cells cover the callee and all11 caller TUs, including ignored
results, with580 shared-body verdicts. No exact gains/losses or target SIZE changes.
Four bodies change: callee, BwChDem_Progress, v23FP_rx_progress, RxDetMarkB103.
The last only swaps two adjacent outgoing argument stores. Its original post-call
field read remains; there is no new B103 result capture or source edit.

The callee's outer counter is independently witnessed: original repeatedly
MOVSWL AX then TEST SI, whereas the int-index source narrows only initially and
then decrements SI. Both original RMS gates compare signed words. An eight-cell
cross separates short outer index, short measured/cached gate operands, and
unsigned-word pointer countdowns. Adopt the first two because they reproduce
those specific narrowing and comparison categories. Initial RTL already adds
the decrement sign extension and uses HI subregs for both gate comparisons;
these changes precede allocation and scheduling. SIZE10 becomes SIZE3, but
prologue, quiet/gain loops, arithmetic scheduling, gate placement and epilogue
registers remain different. This is not a near-exact or register-only claim.

Pointer countdowns reproduce the original quiet-loop instructions, but not the
complete gain loop. Additional original word-zero tests motivate a separate
cross with short mult and level_est. Eight more cells restore some operand
categories without the original complete gain graph; none is adopted. These
losing controls are informative, not evidence for arbitrary nearby spellings.
Keep int level_est/mult and the existing arithmetic and explicit shift mask.

| Callee cell | SIZE gap (bytes) |
| --- | ---: |
| explicit-return baseline | 10 |
| short outer index | 5 |
| short gates | 10 |
| index + gates (adopted) | 3 |
| pointer countdown alone | 40 |
| index + pointer | 43 |
| gates + pointer | 40 |
| index + gates + pointer | 44 |

The separate eight-cell gain-local cross, relative to adopted index+gates, gives
SIZE3/44/6/45/6/56/6/56. Size is reported, not the acceptance criterion. The
word-counter and gate changes preserve the finite unsigned-count partition and
published signal semantics; cached thresholds already come from short fields,
and FPM_rms returns short. The retained initial signed counter interpretation
is unchanged.

## Replay and audit

```
python3 tools/gcc3_v23_agc_return.py --domain docs/history/data-modes/v23-agc-return-domain.md --historical-headers --baseline-dir build/v23-agc-before-objects
python3 tools/gcc3_agc_word_loops.py --domain docs/history/data-modes/v23-agc-return-domain.md --historical-headers --baseline-dir build/agc-word-baseline
python3 tools/gcc3_agc_word_loops.py --gain --domain docs/history/data-modes/v23-agc-return-domain.md --historical-headers --baseline-dir build/agc-gain-baseline
python3 tools/gcc3_v23_agc_return_audit.py
```

Archive all300 canonical objects and .build-config before source edits. The
word baseline is the first experiment's return-only producer; the gain baseline
is word-3. Both are copied under src_dsp_fpm_agc.c.o with the retained
.build-config. Driver headers are revision-pinned and overlaid only for the
explicit return. Save complete commands, selected assembler versions, source,
headers, RTL, assembly, object hashes and results.json; reproduce raw baselines.

Audit76 complete-TU cells/644 shared-body verdicts, including losing controls.
Metadata, named/allocated data and every other body are unchanged. Known
negative/positive word tests, gate compares and pointer clear detections fire.
Four selected production objects raw-match independent candidate controls.
Four/300 objects change,296 and build configuration remain raw-identical;
all300 faithful inputs raw-match canonical.

Existing initialized AGC component trials now compare returned values with the
blob and with each published signal:476 calls,34 zero/442 one. No synthetic
caller histories or tolerance. Samples and AGC state comparisons remain hard.
The component reports51643 checks over10 groups, including1431 new checks
(three per476 calls plus three coverage checks). V23 valid debug lifecycle
coverage remains22 groups/4656926 checks. No fuzzing or mutation execution.

make phase J=8 exits0:388 passed/0 failed, with structural285-suite/10038-anchor
checks green. Whole-tree census remains1082/1852 EXACT,119759 original exact bytes,
669 SIZE/65 BYTES/31 REGALLOC/5 UNRESOLVED; ratchet green. Partial .text grows
702380→702396, candidate allocated918388→918404; positioned equal bytes
69537→69557/943398. Both strict comparisons are DIFFERENT/exit1. An initial
comparison used a missing relative blob path; its log is preserved as invalid,
excluded, then rerun against the canonical ref object. No whole-object identity,
modern portability or new exact-function claim.

Stop these finite families here. The next useful evidence is the original gain
loop and gate CFG ownership, or changed caller/TU context; do not repeat type,
cast, source-order or return spelling permutations without a new witness.
