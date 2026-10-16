# V.34 receiver field-name review

This is a naming proposal only.  It makes no ABI, source, or behavioural
change.  The fields remain `f...` in `struct v34_receiver` until the mapping is
reviewed and a refactor can retain the layout assertions and differential
fixtures.

## Evidence and proposed names

`receiver()` writes the following six fields as one 1024-symbol measurement
transaction (`src/pump/v34/V34RX.c:2694-2719`): it accumulates the squared
decision error and predictor error, increments a modulo-1024 counter, then
publishes their scaled values and the accumulated target-point energy.

| offset | current member | proposed name | type | evidence |
|---:|---|---|---|---|
| `+0x21a` | `f21a` | `equalizer_error_1024` | `short` | `f220 >> 16`, saturated, on the counter wrap; emitted as `equerr` and used by the data-rate ladder. |
| `+0x21c` | `f21c` | `error_window_symbols` | `short` | Incremented modulo 1024; its zero value publishes the measurement window. |
| `+0x220` | `f220` | `equalizer_error_accum` | `int` | Accumulates `dr*dr + di*di` until publication. |
| `+0x224` | `f224` | `predictor_error_1024` | `short` | `f228 >> 16`, saturated, on the same wrap; emitted as `preerr`. |
| `+0x228` | `f228` | `predictor_error_accum` | `int` | Accumulates the predictor-error term until publication. |
| `+0x248` | `f248` | `target_signal_power_1024` | `int` | Published from `f24c >> 8`; diagnostics compute `f248 / f21a`, and the value is the same window's target-symbol energy. |
| `+0x24c` | `f24c` | `target_signal_power_accum` | `int` | Accumulates `target_re² + target_im²` until the counter wrap. |

The recovery supervisor in `datapumpv34()` accesses the next seven receiver
words by offset (`src/pump/v34/V34hshak.c:10252-10260`), but their behaviours
are fully determined there and can be named without inference:

| offset | current member | proposed name | type | evidence |
|---:|---|---|---|---|
| `+0x252` | `f252` | `retrain_error_threshold` | `short` | `DP_RX_THR_A`; threshold for the fast local bad-block retrain run. |
| `+0x254` | `f254` | `reneg_down_error_threshold` | `short` | `DP_RX_THR_B`; threshold for the delayed rate-down run. |
| `+0x256` | `f256` | `reneg_up_error_threshold` | `short` | `DP_RX_THR_C`; threshold for the delayed good-block run. |
| `+0x258` | `f258` | `retrain_bad_block_run` | `short` | Consecutive `equerr > threshold` count used by the full-retrain gate. |
| `+0x25a` | `f25a` | `reneg_down_bad_block_run` | `short` | Consecutive high-error count, enabled after the 18-second guard. |
| `+0x25c` | `f25c` | `reneg_up_good_block_run` | `short` | Consecutive low-error count, enabled after the 144-second guard. |
| `+0x25e` | `f25e` | `rate_change_reason` | `short` | Written as 1 (remote), 2 (down), or 3 (up). |
| `+0x260` | `f260` | `rate_change_rate_index` | `short` | Captures the current baud/rate index when the supervisor starts a rate change. |

## Refactor constraints

1. Keep offsets and types exactly as above; the public layout is pinned by
   `V34RX_ASSERT` checks in `src/pump/v34/V34RX.c`.
2. Rename accumulator/publication pairs together.  Calling `f248` merely
   “signal power” without its window would hide that it is coherent with
   `equerr`, but not an instantaneous power reading.
3. Preserve compatibility aliases temporarily if tests or out-of-tree work
   reference the old member names.  A mechanical rename should be followed by
   the receiver, handshake, diagnostics, and rate-ladder differential tests.
