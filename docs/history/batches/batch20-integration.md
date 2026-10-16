# Twenty-function byte-exact batch

Base: master `93d7eee1`, Gentoo GCC 3.4.2-r2 with the saved complete
production configuration and `DSPLIB_REPRODUCE_BUGS`. Work is isolated from
PR245; source/header ownership exclusions are in batch20-ownership.md.
Our unpublished findings moved to F11730 onwards after the other draft
renumbered its findings into F11710–F11716. Shared docs are append-only.

The combined census improves **940/1852 → 960/1852**, **97,508 → 101,930 exact
bytes**, **20 gains / zero losses**. COMDAT copies use the normal worst-copy
census. These are function identities, not a claim of whole-object identity.

| Recovered bodies | Exact bytes |
|---|---:|
| call_delete | 158 |
| FPM_lmsupd / FPM_lmsupd2 | 150 / 182 |
| realfft | 505 |
| v23FP_tx_create / v23FP_rx_create / CreateV23Modem | 255 / 581 / 524 |
| B103OriginateNextState | 295 |
| ModDataB103 / TxNoCarrierB103 | 99 / 124 |
| ModDataV21 / TxNoCarrierV21 | 87 / 103 |
| b103_create / dp_b103_exit | 406 / 49 |
| float2Bits | 283 |
| V90Resampler::getTimingHistoryStd / resample | 66 / 170 |
| V90ConnectionEvaluator::updateAvePdsnr | 113 |
| V90ConnectionEvaluator::indicateLocalRetrain / indicateRemoteRetrain | 136 / 136 |

The new source boundaries include arithmetic temporary widths, point-of-use
narrowing, explicit sign branches, default result lifetimes spanning calls,
late history-index updates and original omitted diagnostics. Independent finite
controls and combined winners have complete TU metadata, named-data, nontext
relocation and bystander audits. Size-only candidates are not adopted.
B103AnswerNextState's diagnostics-only candidate is not counted: the combined
MRF recovery forgoes it at the known global peephole2 scratch-selection boundary.

All **300/300 production objects** are checked: **287 raw unchanged**, **13
match retained compiler replay objects**. The compiler configuration and MRF
callee remain raw unchanged. `tools/gcc3_batch20_production_audit.py` reports
both denominators and asserts exact-set membership, not only count.

The same ordered 300-input partial link remains DIFFERENT on both sides.
Positioned exact reference bytes change **68,664 → 68,345 / 943,398**;
exact relocations **1,029 → 1,031 / 18,317**; exact symbols **394 / 2,907** on
both. Whole-object positional matching can fall as recovered body lengths move
later functions; this measurement is preserved rather than reported as a gain.

## Validation checkpoint

The first `make phase J=4` reports **387 passed / 1 failed**. The expanded
Create→Dial callback/transcript fixture exposes additional original getter
boundaries in non-exact CALLPROG_Dial; they are being recovered under unchanged
strict checks. The failed run is preserved, not accepted. Static anchor metadata
is retargeted with unchanged labels; no mutation or fuzzing harness is executed.
Final `make phase J=4` exits zero: **388 period tests passed / 0 failed**,
structural checks all OK. The unchanged seventy-check fixture fails **12/70**
on original source and passes **70/70** on the recovered callback boundaries
(F11742). Static anchors: **285 suites / 10,038 entries**, zero detached,
nonunique or mislabelled. No mutation or fuzzing harness was executed.

Archived inputs and evidence live under build/production-before and the named
build/*batch20* experiment folders. Replay old source against its headers with
explicit `--historical-headers`; default drift rejection stays enabled.
