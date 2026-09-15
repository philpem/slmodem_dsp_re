# Translation-unit byte-fidelity plan

This is the post-PR95 planning census. Its canonical measurements and provenance are in [the baseline](byte-fidelity-baseline.md); experiment acceptance follows [experiment design](method/experiment-design.md). The machine-readable complete 275-input census is `build/byte-fidelity-baseline/tu-plan.json`.

## Measured scope

`byteident.py` compares 1,852 names and 720,125 reference bytes. The baseline has **830 exact names (44.8%%) and 80,030 exact bytes (11.1%%)**. The remaining verdicts are **4 UNRESOLVED, 48 REGALLOC, 0 RELOC, 80 BYTES, and 890 SIZE**. Against `/tmp/issue20-closure-batch/fresh-byteident.json`, exact membership is unchanged: **830 retained, zero lost, zero gained**.

The separate binding census compares defined FUNC/OBJECT/NOTYPE names including ABS and finds **2,526/2,526 shared names agree** on `(kind,binding,visibility)`, with **75 reference-only and 136 candidate-only**. This is deliberately not called a 1,852-name result: byte equality canonicalizes relocations and does not prove export/interposition correctness or complete symbol presence. Three COMDAT names also have object-dependent verdicts; `byteident` scores their worst defining copy.

## Ranking policy

Priority is evidence-weighted, not largest-first. Wave 1 contains full TUs whose remaining shared bodies are same-size (BYTES/REGALLOC/UNRESOLVED), so a bounded source/profile cross has a crisp falsifier. Wave 2 requires at least three exact anchors and at most 64 aggregate bytes of SIZE gap. Wave 3 retains meaningful exact anchors but has broader SIZE work. Large low-anchor reconstruction is wave 4. Fax is shown separately at lower priority because coverage is complete and its remaining codegen work should not displace sharper non-fax experiments. V.22 is excluded because its nine-cell experiment is already owned separately and found all cells INIT-first with no adoption.

`rank_score` in the JSON is only a stable ordering device within those waves. It rewards exact anchors, exact-byte coverage and same-size misses, and penalizes SIZE count/gap; it is not evidence that a source spelling is original.

## Wave 1: bounded same-size experiments

| Rank | TU | Exact/shared | Exact/ref bytes | Misses | SIZE gap |
| ---: | --- | ---: | ---: | --- | ---: |
| 1 | `src/pump/v90/V92bitsToSymbol.cpp` | 9/10 | 1553/1661 | BYTES 1 | 0 |
| 2 | `src/service/rd.c` | 3/4 | 230/523 | BYTES 1 | 0 |
| 3 | `src/core/dp_param.c` | 2/3 | 33/331 | BYTES 1 | 0 |
| 4 | `src/dsp/fpm_atan.c` | 0/1 | 0/409 | BYTES 1 | 0 |
| 5 | `src/pump/v32/v32nsloop.c` | 0/1 | 0/794 |  | 0 |

For each TU, first reproduce the unchanged TU through `experiment_toolchain.py`, then run a retained-source/retained-flags control. The first source axis must come from forced encoding in the miss: load signedness/width, arithmetic width, constant type, declaration order, or a demonstrated preceding-emission carrier. Cross that single source family with one isolated option/pass control only when the body shows an inline, scheduling, or block-layout discriminator. Record all shared names, exact gains and losses, body/relocation changes and binding. Stop after two batches without a new explanatory observation.

## Wave 2: anchored small-SIZE experiments

| Rank | TU | Exact/shared | Exact/ref bytes | Misses | SIZE gap |
| ---: | --- | ---: | ---: | --- | ---: |
| 6 | `src/pump/v90/V90SpectralShaper.cpp` | 10/15 | 932/2989 | BYTES 2, SIZE 3 | 23 |
| 7 | `src/pump/v90/V90SignBitsExtractor.cpp` | 8/12 | 514/1092 | BYTES 2, SIZE 2 | 6 |
| 8 | `src/pump/v90/NoK56Flex.cpp` | 20/22 | 45/87 | SIZE 2 | 11 |
| 9 | `src/pump/v90/V90Modulator.cpp` | 18/25 | 2188/4231 | SIZE 7 | 29 |
| 10 | `src/pump/v32/v32seq.c` | 9/13 | 762/1406 | BYTES 1, SIZE 3 | 29 |
| 11 | `src/pump/v90/V90Parameters.cpp` | 7/9 | 8161/12094 | SIZE 2 | 17 |
| 12 | `src/pump/v90/V90Resampler.cpp` | 13/16 | 1622/2551 | SIZE 2 | 34 |
| 13 | `src/pump/v32/V32TXHDX.c` | 3/8 | 381/1432 | BYTES 1, SIZE 4 | 9 |
| 14 | `src/pump/v32/v32.c` | 3/5 | 193/1691 | BYTES 1, SIZE 1 | 15 |
| 15 | `src/pump/v90/ResamplerTiming.cpp` | 11/14 | 1171/2135 | SIZE 3 | 25 |
| 16 | `src/pump/v90/V90bitsToSymbol.cpp` | 10/11 | 1760/1868 | SIZE 1 | 41 |
| 17 | `src/pump/v90/V90TRN2dDesigner.cpp` | 7/8 | 355/4122 | SIZE 1 | 17 |
| 18 | `src/pump/v90/V92PreFilter.cpp` | 6/7 | 600/785 | SIZE 1 | 32 |
| 19 | `src/pump/v90/V90Mapper.cpp` | 6/8 | 945/1866 | SIZE 2 | 50 |
| 20 | `src/pump/v90/V92Transmitter.cpp` | 5/6 | 1061/3222 | SIZE 1 | 21 |
| 21 | `src/pump/v90/V92Mapper.cpp` | 5/6 | 60/167 | SIZE 1 | 2 |
| 22 | `src/dsp/psd.cpp` | 6/13 | 297/1549 | SIZE 7 | 40 |
| 23 | `src/pump/v90/V90RDetector.cpp` | 5/9 | 145/703 | SIZE 4 | 10 |
| 24 | `src/pump/v90/V90SdDetector.cpp` | 5/6 | 294/474 | SIZE 1 | 36 |
| 25 | `src/pump/v90/VPcmXfCreate.cpp` | 4/5 | 690/799 | SIZE 1 | 8 |

Start with the smallest SIZE body in each TU and classify the missing/extra instruction before changing source. Missing calls demand helper placement/visibility and FILE-boundary checks before flag trials. Small length gaps demand instruction/CFG inspection, not “nearest byte count” source edits. Exact anchors are controls and any loss rejects the local candidate unless a crossed source/profile result explains the interaction.

## Later queues

Wave 3 is anchored mixed work and wave 4 is broad reconstruction; both are fully enumerated in the JSON. They should begin only after the first two waves produce a shared mechanism or close their bounded domains. Apply a mechanism found in multiple local controls to representative TUs, then the complete object, before proposing a profile change.

Fax remains a separate lower-priority queue despite complete behavioral coverage. Its same-size near misses may be useful controls for a mechanism already established elsewhere, but fax size alone is not a reason to lead the byte-fidelity work.

Ambiguous reference ownership is not force-assigned. The JSON preserves every `attribution.json` row whose `how` is `ambiguous`, with its candidate owner string and reference size where known. Resolve those FILE/TU boundaries before treating a local score as a full-TU denominator. Inputs with zero shared text symbols are retained as `exact-or-no-shared`; they are layout/data participants, not “100%% exact” TUs.

## Acceptance and next tests

1. Run the wave-1 unchanged-path controls and reject any path that changes baseline output before the intended source/option cell.
2. For the highest wave-1 candidates, define a finite source family from forced operands and cross it with retained flags plus one justified alternative. Do not enumerate synonymous C without distinct emissions.
3. Run wave-2 helper-boundary and small-CFG discriminators, preserving exact-anchor losses and symbol binding as hard failures.
4. If two or more TUs implicate the same pass combination, test that combination on representative families and then the full 275-input object. Do not infer a global option from local gains.
5. Keep the project completion condition `partialcmp.py --require-exact`. The current partial link is structurally different; TU gains are intermediate evidence only.
