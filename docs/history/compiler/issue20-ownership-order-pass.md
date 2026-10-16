# Issue #20: ownership and ordering before byte refinement

The current task is to finish ownership and ordering and merge that work before
starting instruction/byte-fidelity tuning. Strict whole-object identity remains
the eventual goal, but is not claimed by this structural pass.

## Retained batch after e8f8716c

- Restore `ThresholdsTable` and its accessor to `FP_math.c`. Its surviving
  LOCAL symbol belongs to that FILE, and the reference has one relocation
  from `Get_Detection_Threshold_Table` at `.text+0x7e2d3`.
- Restore `FPTONE` and `TONE_read` to the same FILE. The reconstructed
  `TONE_quarter_cosine` array matches its 1,026 bytes exactly; the threshold
  table matches all 32 bytes. Restore the original table name rather than
  treating the alias as a separate object.
- Rename the remaining detector source to `DualTone_Detector.c` and retain
  its function bodies and private filter helpers.
- Put `GenerateCallingTone` before `ResetCallingTone`, both symbol encoders
  before their initializers in `fpm_smc.c` and `Smc.c`, and `GetFP_Value`
  before `FP_Pow`. The relocated tone accessors follow them in reference
  address order. Bodies and compiler flags are unchanged.
- Restore four C++ FILE-name correspondences: `V90TRN2dDesigner.cpp`,
  `V92ConvEncoder.cpp`, `V92DILdesPCK.cpp` and `V90DILdesPCK.cpp`.
  These whole-file renames use unique class/function-family correspondence;
  that is an ordering policy supported by naming, not the stronger direct
  LOCAL-symbol ownership evidence above. No extra TU membership is inferred
  merely from a GLOBAL symbol's address.

## Measurements and safeguards

All 33 functions in the nine affected inputs remain, with none missing or
duplicated. `byteident.py` compares 32 unchanged against the prior objects;
`TONE_read` has three relocation-target spelling changes to `FPTONE`, not an
instruction change. All 828/1,852 reference-exact functions remain in the
exact set, with zero gains or losses (79,916/720,125 exact function bytes).

| Dimension | Before | After |
| --- | ---: | ---: |
| Inputs | 264 | 264 |
| Ordering candidates | 181 | 186 |
| Unresolved inputs | 83 | 78 |
| Positioned bytes / 943,398 | 55,313 | 56,129 |
| Exact relocation records / 18,317 | 955 | 962 |
| Exact symbol records / 2,907 | 248 | 253 |
| Exact section descriptors / 92 | 67 | 67 |
| Shared-name binding agreements | 2,442/2,442 | 2,443/2,443 |
| Reference-only / candidate-only names | 76 / 131 | 75 / 130 |

Candidate text remains 684,872 bytes and NOBITS remains 2,812 versus 2,836.
The total content deficit grows by 32 bytes (45,300 to 45,332); positioned
matches are not a complete identity score. The diagnostic verdict remains
`DIFFERENT`.

The reviewed Gentoo GCC 3.4.2-r2 build retains bug reproduction and its
established flags. `make phase`: **375 passed, zero failed**, structural
checks pass. Six mutation suite paths follow the C++ renames; all 136
existing anchors still match exactly once. This is route/anchor validation,
not a fresh mutation run: the snapshot has 264 stale suites and zero never
recorded. Local evidence: `/tmp/issue20-ownership-order-batch/`.

## Ownership work still open

The remaining work must not be relabelled byte refinement simply to close
the issue. A read-only local-symbol census at the preceding 264-input
baseline found 14 unique-name ownership disagreements, all data, among 188
matched current locals. This batch resolves the `ThresholdsTable` placement;
the following directly evidenced ownership questions remain:

- `Fdspkrnl.c`: two local objects belong to reference `Fdsp.c`.
- `VPcmFloModemCtor.cpp`: three local tables belong to `VpcmFloModem.cpp`;
  merging constructor/helpers needs full-TU and mutation-route controls.
- `v8util.c`: `v8_costbl` belongs to `V8Dftc.c`. Other unrelated helpers in
  that source cannot be moved merely because the table has a known owner.
- `callprog_status.c`: `message_names` belongs to `Callprog.c`.
- `v22org.c`: `iSilenceAfter2100` belongs to `v22mod.c`.
- `v22fp.c`: locals have mixed ownership in `V22.c` and `v22mod.c`; a
  whole-file rename would conceal the split.

Additional class/family-supported candidates include splitting `ModulusCoder`,
the three prefilter coefficient families, and the five `V90SessionFlag`
methods. They require separate dependency and mutation-route handling, not
blind moves. Some globals, including `default_voice_configuration`, still
lack original-owner evidence. The V.92 mapping-parameters FILE association
also remains an interval-based hypothesis, not a recovered boundary.

The initial function-order screen found 92 inputs with descending reference
addresses among emitted shared functions. It includes C++ ABI clones and
compiler-controlled ordering, so it is not a count of 92 source defects.
Four directly actionable C orderings are corrected here. The remaining
screening results and 78 unresolved inputs still need classification before
ownership/ordering can be called finished.
