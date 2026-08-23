# V.34 anonymous field-name review

This records the anonymous-field mapping and the source refactor.  The renamed
members retain their original offsets, types, ABI layout, and differential
fixtures.

Receiver offsets are relative to `struct v34_receiver` (which begins at
`struct v34_object+0x264`); object offsets are labelled explicitly.  This
distinction matters: the V.34 object also has
fields called `f25e` and `f260`, but they are its transmit/output and
receive/input samples, not the receiver's rate-change fields.

## Completed

| object offset | former member | current name | type | evidence |
|---:|---|---|---|---|
| `+0xa23c` | `fa23c` | `far_echo_enabled` | `short` | `modem_serrint` filters and adapts `echo1` only while non-zero (`src/pump/v34/v34rx.c:1339-1346`, `1481-1519`); bulk-delay setup clears it for V.90/K56Flex or insufficient delay (`src/pump/v34/v34hshak.c:3094-3120`). Renamed in the source tree. |

## Refactored receiver measurements

`receiver()` writes the following seven fields as one 1024-symbol measurement
transaction (`src/pump/v34/v34rx.c:2634-2663`): it accumulates the squared
decision error and predictor error, increments a modulo-1024 counter, then
publishes their scaled values and the accumulated target-point energy.

| offset | former member | current name | type | evidence |
|---:|---|---|---|---|
| `+0x21a` | `f21a` | `equalizer_error_1024` | `short` | `f220 >> 16`, saturated, on the counter wrap; emitted as `equerr` and used by the data-rate ladder. |
| `+0x21c` | `f21c` | `error_window_symbols` | `short` | Incremented modulo 1024; its zero value publishes the measurement window. |
| `+0x220` | `f220` | `equalizer_error_accum` | `int` | Accumulates `dr*dr + di*di` until publication. |
| `+0x224` | `f224` | `predictor_error_1024` | `short` | `f228 >> 16`, saturated, on the same wrap; emitted as `preerr`. |
| `+0x228` | `f228` | `predictor_error_accum` | `int` | Accumulates the predictor-error term until publication. |
| `+0x248` | `f248` | `target_signal_power_1024` | `int` | Published from `f24c >> 8`; diagnostics compute `f248 / f21a`, and the value is the same window's target-symbol energy. |
| `+0x24c` | `f24c` | `target_signal_power_accum` | `int` | Accumulates `target_re² + target_im²` until the counter wrap. |

The recovery supervisor in `datapumpv34()` accesses the next eight receiver
words by offset, and their behaviours are fully determined there, so they can
be named without inference.  The site is the `DP_RX_THR_A`/`_B`/`_C` defines
(`src/pump/v34/v34hshak.c:9930-9932`) and the three `dp_run` calls that consume
them (`10005-10015`) -- **an earlier revision of this table cited
`v34hshak.c:10252-10260`, which is past the end of a 10,136-line file.**  Cite a
grep-able token, not a line number: eight names rested on a range that could not
be opened.

| offset | former member | current name | type | evidence |
|---:|---|---|---|---|
| `+0x252` | `f252` | `retrain_error_threshold` | `short` | `DP_RX_THR_A`; threshold for the fast local bad-block retrain run. |
| `+0x254` | `f254` | `reneg_down_error_threshold` | `short` | `DP_RX_THR_B`; threshold for the delayed rate-down run. |
| `+0x256` | `f256` | `reneg_up_error_threshold` | `short` | `DP_RX_THR_C`; threshold for the delayed good-block run. |
| `+0x258` | `f258` | `retrain_bad_block_run` | `short` | Consecutive `equerr > threshold` count used by the full-retrain gate. |
| `+0x25a` | `f25a` | `reneg_down_bad_block_run` | `short` | Consecutive high-error count, enabled after the 18-second guard. |
| `+0x25c` | `f25c` | `reneg_up_good_block_run` | `short` | Consecutive low-error count, enabled after the 144-second guard. |
| `+0x25e` | `f25e` | `rate_change_reason` | `short` | The reader is the object's own words: `== 2` takes the arm printing " TRNSEG4A : returning from local rrn down => forcing rate down" and any other value above 1 takes the "...rrn up => forcing rate up" arm (`src/pump/v34/v34hstx1.cpp:2638-2654`).  So 2 = down and 3 = up are established; **1 = remote is NOT** -- no reconstructed writer stores 1, 2 or 3, and the only in-tree writer clears it (`v34hshak.c:490`). |
| `+0x260` | `f260` | `rate_change_rate_index` | `short` | Read as `d` and compared against the freshly computed rate, which is then forced to `d - 1` or `d + 1` (`src/pump/v34/v34hstx1.cpp:2639-2650`) -- so it is a rate INDEX and one step either side of it is the request. |

## Refactored receiver fields

These are not guesses inferred from a nearby constant: each has a direct
producer/consumer relationship or an in-tree comment tied to the reconstructed
operation. They have now been renamed in the source; the former `f...` labels
remain in this table solely as offset keys.

| offset | former member | current name | type | evidence |
|---:|---|---|---|---|
| `+0x120` | `f120` | `vectpp_cursor` | `short` | The receiver map identifies it as `vectpp`'s cursor (`include/dsplib/v34recv.h:29-43`). |
| `+0x124` | `f124` | `rx_symbol_count` | `short` | The receiver's diagnostic calls it `rxsymcnt` and the state thresholds consume it as a symbol clock (`include/dsplib/v34recv.h:34-40`). |
| `+0x128` | `f128` | `timing_output_count` | `short` | `rxtiming`'s output count (`include/dsplib/v34recv.h:41-43`). |
| `+0x12a` | `f12a` | `agc_decimation_phase` | `short` | Counts demodulated sample pairs to the fourth-pair AGC update, then resets (`src/pump/v34/v34rx.c:750-772`).  It is not a held-sample count. |
| `+0x19c` | `f19c` | `rms_window_index` | `short` | Index for the 36-sample RMS buffer (`include/dsplib/v34recv.h:68-76`). |
| `+0x1aa` | `f1aa` | `previous_constellation_index` | `short` | Used by `decoderv34` for differential decoding (`include/dsplib/v34recv.h:80-82`). |
| `+0x1ac` | `f1ac` | `timing_phase` | `short` | Fractional phase in the timing loop (`include/dsplib/v34recv.h:83-85`). |
| `+0x1ae` | `f1ae` | `timing_phase_increment` | `short` | The timing loop's phase increment (`include/dsplib/v34recv.h:83-94`). |
| `+0x1b0` | `f1b0` | `timing_phase_wrap` | `short` | The timing loop's wrap threshold (`include/dsplib/v34recv.h:83-85`). |
| `+0x1b8` | `f1b8` | `carrier_phase_increment` | `short` | NCO/carrier-table phase increment (`include/dsplib/v34recv.h:87-90`). |
| `+0x1ba` | `f1ba` | `carrier_quadrature_offset` | `short` | Offset from the sine table to the cosine half of the carrier table (`include/dsplib/v34recv.h:87-90`). |
| `+0x1bc` | `f1bc` | `carrier_phase` | `short` | Current carrier-table phase (`include/dsplib/v34recv.h:87-90`). |
| `+0x1c0` | `f1c0` | `timing_state` | `short` | Despite a diagnostic calling it `pllcnt`, it is the nine-state timing-recovery machine; `-1` is done and `f230`/`f232` advance it (`src/pump/v34/v34rx.c:1768-1783`, `1847-1916`). |
| `+0x1d2` | `f1d2` | `timing_report_interval_symbols` | `short` | Installed from frame length/8 and used as the timing-offset reporting interval (`include/dsplib/v34recv.h:106-113`). |
| `+0x1f8` | `f1f8` | `carrier_loop_integrator` | `int` | Carrier-loop integrator (`include/dsplib/v34recv.h:132-133`). |
| `+0x1fc` | `f1fc` | `carrier_phase_error` | `int` | Phase error produced by `decision` and consumed by the NCO (`include/dsplib/v34recv.h:135-140`). |
| `+0x214` | `f214` | `predictor_work_re` | `short` | Initially receives the real `target - decision` error, then is modified in place by `rx_predict` and accumulated as `preerr` (`src/pump/v34/v34rx.c:2078-2127`, `2539-2552`). |
| `+0x216` | `f216` | `predictor_work_im` | `short` | Imaginary counterpart of the in/out predictor work value (`src/pump/v34/v34rx.c:2078-2127`, `2539-2552`). |
| `+0x218` | `f218` | `equalizer_error_gain` | `short` | Scales the equaliser error before its adaptation call (`src/pump/v34/v34rx.c:2630-2637`).  Do not encode a Q-format in the name: the implementation multiplies then shifts by 16, contradicting the older Q15 comment. |
| `+0x230` | `f230` | `timing_state_dwell` | `short` | Counter compared with `f232` before advancing the timing state (`src/pump/v34/v34rx.c:1855-1867`, `1905-1909`). |
| `+0x232` | `f232` | `timing_state_dwell_limit` | `short` | Per-state dwell limit; `-1` disables it (`include/dsplib/v34recv.h:192-199`, `src/pump/v34/v34rx.c:1905-1909`). |
| `+0x234` | `f234` | `timing_proportional_gain_q11` | `short` | Per-symbol timing-loop gain scaled in Q11 (`include/dsplib/v34recv.h:192-199`, `src/pump/v34/v34rx.c:1967-1969`). |
| `+0x236` | `f236` | `timing_integral_gain_q15` | `short` | Timing-loop integrator gain scaled in Q15 (`include/dsplib/v34recv.h:192-199`, `src/pump/v34/v34rx.c:1967-1969`). |
| `+0x240` | `f240` | `demod_i` | `short` | Demodulated I; subsequently timing-filtered/interpolated (`include/dsplib/v34recv.h:201-204`, `src/pump/v34/v34rx.c:788-828`). |
| `+0x242` | `f242` | `demod_q` | `short` | Demodulated Q; subsequently timing-filtered/interpolated (`include/dsplib/v34recv.h:201-204`, `src/pump/v34/v34rx.c:788-828`). |
| `+0x244` | `f244` | `demod_prev_i` | `short` | Previous I endpoint used by the interpolator (`include/dsplib/v34recv.h:201-204`, `src/pump/v34/v34rx.c:840-851`). |
| `+0x246` | `f246` | `demod_prev_q` | `short` | Previous Q endpoint used by the interpolator (`include/dsplib/v34recv.h:201-204`, `src/pump/v34/v34rx.c:840-851`). |
| `+0x262` | `f262` | `agc_reset_gain` | `short` | Copied into `agc_gain` by receiver-start paths (`src/pump/v34/v34hshak.c:381-392`) but also rewritten from the current gain during phase 3 (`src/pump/v34/v34hshak.c:8448-8451`); it is a reset value, not immutable initial configuration. |
| `+0x266` | `f266` | `demap_subframe_index` | `short` | Passed as the current sub-frame index to `demapFrame`, then incremented (`include/dsplib/v34recv.h:225-226`, `src/pump/v34/v34rx.c:1571-1576`). |
| `+0x268` | `f268` | `retrain_prev_i` | `short` | Prior equaliser-output I; shifted to `f26c` as the next sample arrives (`include/dsplib/v34recv.h:228-236`, `src/pump/v34/v34rx.c:2319-2322`). |
| `+0x26a` | `f26a` | `retrain_prev_q` | `short` | Prior equaliser-output Q; shifted to `f26e` as the next sample arrives (`include/dsplib/v34recv.h:228-236`, `src/pump/v34/v34rx.c:2319-2322`). |
| `+0x26c` | `f26c` | `retrain_prev2_i` | `short` | Two-symbol-old equaliser-output I (`include/dsplib/v34recv.h:228-236`, `src/pump/v34/v34rx.c:2319-2322`). |
| `+0x26e` | `f26e` | `retrain_prev2_q` | `short` | Two-symbol-old equaliser-output Q (`include/dsplib/v34recv.h:228-236`, `src/pump/v34/v34rx.c:2319-2322`). |
| `+0x2a4` | `f2a4` | `fir60_coeffs` | pointer | Coefficients for the 60-tap receive FIR (`include/dsplib/v34recv.h:269-270`, `src/pump/v34/v34rx.c:1380-1399`). |
| `+0x798` | `f798` | `retrain_reneg_motion_count` | `short` | Saturating movement counter whose thresholds request retrain or renegotiation (`include/dsplib/v34recv.h:273-280`, `src/pump/v34/v34rx.c:2325-2356`). |

## Refactored V.34-object fields

| object offset | current member | proposed name | type | evidence |
|---:|---|---|---|---|
| `+0x25e` | `f25e` | `tx_sample` | `short` | Refactored. The PCM V.34 loop writes it to output after `modem_serrint` (`src/pump/v34/v34pcmmain.cpp:2120-2122`). |
| `+0x260` | `f260` | `rx_work_sample` | `short` | Refactored. It begins as the input sample and is overwritten by echo processing (`src/pump/v34/v34pcmmain.cpp:2464-2466`, `src/pump/v34/v34rx.c:1331-1349`). |
| `+0x2aa4` | `f2aa4` | `history_2aa8_index` | `short` | Refactored. Generic because the ring carries both residual and target I/Q (`src/pump/v34/v34rx.c:1428-1437`, `2378-2385`). |
| `+0x2aa6` | `f2aa6` | `history_2f58_index` | `short` | Refactored. Generic because PCM paths fill it from input/output blocks (`src/pump/v34/v34pcmmain.cpp:2493-2500`, `2645-2655`). |
| `+0x354c` | `f354c` | `echo_adapt_count` | `int` | Refactored. It gates near/far adaptation schedules (`src/pump/v34/v34rx.c:1442-1489`, `1508-1519`). |
| `+0x3550` | `f3550` | `near_echo_alpha` | `short` | Refactored. `updateAlpha` maintains the near alpha (`src/pump/v34/v34rx.c:1485-1493`). |
| `+0x3552` | `f3552` | `far_echo_alpha` | `short` | Refactored. `updateAlpha` maintains the far alpha (`src/pump/v34/v34rx.c:1487-1498`). |
| `+0x3554` | `f3554` | `echo_alpha_decay_start` | `int` | Refactored. The PCM diagnostic calls it “decay start” (`src/pump/v34/v34pcmif.c:210-229`). |
| `+0x3558` | `f3558` | `echo_alpha_decay_factor` | `int` | Refactored. The PCM diagnostic calls it “decay fact” (`src/pump/v34/v34pcmif.c:210-229`). |
| `+0x355c` | `f355c` | `echo_beta` | `int` | Refactored. The PCM diagnostic calls it “beta” (`src/pump/v34/v34pcmif.c:220-229`). |
| `+0x3560` | `f3560` | `near_echo_startup_energy` | `int` | Refactored. Accumulated for the first 0x8f calls and used to select beta (`src/pump/v34/v34rx.c:1211-1238`). |
| `+0xa23e` | `fa23e` | `residual_correction` | `short` | Refactored. One-shot correction shared between `modem_serrint` and `adaptecho` (`src/pump/v34/v34rx.c:1177-1184`, `1331-1337`). |
| `+0xa240` | `fa240` | `residual_energy_estimate` | `short` | Refactored. Leaky per-symbol residual-energy estimate (`src/pump/v34/v34rx.c:1428-1440`). |

## Adversarial review, 2026-08-20

The proposed names were checked against every reconstructed writer and reader,
looking specifically for mode-dependent reuse, contradictory units, and names
that implied a more specific signal path than the stores prove.  The following
attempts *did* disprove the original spelling and have been incorporated above:

| field | rejected name | reason it failed | retained name |
|---|---|---|---|
| receiver `+0x12a` | `demod_samples_held` | It increments per demodulated pair and resets after the fourth-pair AGC opportunity; no held-sample operation reads it (`src/pump/v34/v34rx.c:750-772`). | `agc_decimation_phase` |
| receiver `+0x1c0` | `pll_symbol_count` | The debug label `pllcnt` was misleading: it indexes the timing-state jump table, with `-1` as terminal state (`src/pump/v34/v34rx.c:1768-1783`, `1847-1916`). | `timing_state` |
| receiver `+0x218` | `equalizer_step_q15` | The actual multiply is followed by `>> 16`, so the proposed Q15 unit was not supported (`src/pump/v34/v34rx.c:2630-2637`). | `equalizer_error_gain` |
| receiver `+0x262` | `agc_initial_gain` | It is re-derived from `agc_gain` during phase 3, so it is not initialization-only (`src/pump/v34/v34hshak.c:8448-8451`). | `agc_reset_gain` |
| object `+0x2aa4` | `filtered_residual_history_index` | Another path writes target I/Q to the same ring (`src/pump/v34/v34rx.c:2378-2385`). | `history_2aa8_index` |
| object `+0x2aa6` | `raw_residual_history_index` | PCM paths populate its ring directly from input/output blocks (`src/pump/v34/v34pcmmain.cpp:2493-2500`, `2645-2655`). | `history_2f58_index` |
| object `+0x3550/+0x3552` | `near_echo_step` / `far_echo_step` | `updateAlpha` calls the values alpha, while beta is a separate scaling control (`src/pump/v34/v34rx.c:1213-1238`, `1485-1498`). | `near_echo_alpha` / `far_echo_alpha` |
| object `+0xa23e` | `echo_residual_correction` | It occurs before the echo filter and its recurrence does not establish an echo-specific function (`src/pump/v34/v34rx.c:1177-1188`, `1331-1337`). | `residual_correction` |

The remaining names survived this pass only at the specificity shown.  In
particular, `f124` remains `rx_symbol_count`: although handshake paths seed
and reset it, all observed uses are symbol-count thresholds or increments
(`src/pump/v34/v34rx.c:1638-1641`, `src/pump/v34/v34hshak.c:8469-8473`).
Do not promote a surviving proposed name to a source rename without retaining
this test: a later reconstructed writer can still invalidate it.

For the receiver sub-object specifically, no additional contradiction was
found for the timing phase/wrap pair, carrier table phase/offset pair, AGC RMS
index, 1024-symbol error accumulators, rate-supervisor counters, retrain
history, or FIR coefficient pointer.  `f214/f216` and `f266` are the only
names narrowed in this pass; they remain proposals until the receiver struct
is renamed as one ABI-preserving transaction.

## Refactor constraints

1. Keep offsets and types exactly as above; the public layout is pinned by
   `V34RX_ASSERT` checks in `src/pump/v34/v34rx.c`.
2. Rename accumulator/publication pairs together.  Calling `f248` merely
   “signal power” without its window would hide that it is coherent with
   `equerr`, but not an instantaneous power reading.
3. Preserve compatibility aliases temporarily if tests or out-of-tree work
   reference the old member names.  A mechanical rename should be followed by
   the receiver, handshake, diagnostics, and rate-ladder differential tests.
