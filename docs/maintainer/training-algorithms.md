# V.34 training algorithms

Training estimates the line and establishes the receiver's gain, symbol timing,
carrier phase and equaliser state before payload decisions are reliable.
The [lifecycle](lifecycle.md) describes sequencing; this chapter describes the
algorithms in `V34hshak.c`, `V34RX.c` and `v34filters.c` at `4169cb3b`.
Use [the module catalogue](modules.md#v34) to locate their source files.

## Probe measurement and pre-emphasis

The line probe is measured in 25 frequency bins. At the 9600 samples/s
boundary, the bin increment gives 150 Hz spacing. Array index 4 represents
750 Hz; indices 5, 7, 11 and 15 are the omitted-tone noise slots.
A bin retains a short `energy` and a normalisation `shift`. Reading energy
alone loses information below its quantisation floor. Energy is a power
quantity: use `10 * log10(power ratio)` to express a ratio in dB.

`probe_preemp` requests a filter for the remote transmitter. The compatible
path compares the reference bin with a baud-dependent edge bin. It repeatedly
multiplies the edge energy by a per-rate Q14 constant, narrows the result to
`short`, and increments a counter initially set to five before testing it.
The returned indices are consequently 6–10; the index-zero arm is unreachable
in this path. Deviation D53 records that original behaviour.

The tree also contains `probe_preemp_shape`, a deliberate alternative that
scores eleven filter templates against usable bins and selects the smallest
residual variance. It can fall back to the original counter when there is
insufficient evidence. `dsplib_v34_blob_preemp` defaults to one when
`DSPLIB_REPRODUCE_BUGS` is defined and zero otherwise. Record both the build
mode and this runtime flag when comparing results. A compile-time
`DSPLIB_V34_TEST_PREEMP_INDEX` override is a separate experimental actuator.

The [historical training study](https://github.com/philpem/slmodem_dsp_re/blob/master/docs/history/v34/training.md)
describes an earlier Theil–Sen fitter named `probe_preemp_fit`. That is not
the current function name or selection algorithm. Its bench measurements and
proposals retain their original scope; they do not establish the performance
of the present selector on a different channel.

## Complex equalisation

The receiver embeds an 80-tap complex equaliser. Each coefficient has high
and fractional halves; a circular complex delay line holds samples. Filtering
walks from the cursor to the end, then from the beginning back to the cursor,
so coefficient zero multiplies the oldest sample. Cleanup clears the object
and installs a Q12 unity coefficient at tap 40.

The full adaptation is a complex least mean squares (LMS) gradient update.
For complex input `d` and scaled error `e`, it applies `-e * conj(d)`:

```text
real coefficient -= dre * ere + dim * eim
imag coefficient -= dre * eim - dim * ere
```

`eq_adapt_tap` reassembles the full coefficient from both halves, performs
the update with the reconstructed wrap behaviour, then stores both halves.
The receiver scales its decision error by `equ_step` before the call.
The step is scheduled by training/frame progress, rather than estimated from
an independent convergence criterion.

Acquisition has a second update, `V34EqualizerCenterAdapt`, over taps 36–43.
It uses the high coefficient half and rounds with `0x8000`; fractional halves
remain untouched. In the receiver's adaptation block, `V34_RX_FLAG_DATA`
selects full adaptation, otherwise centre adaptation runs at twice the error.
An earlier `V34_RX_FLAG_TRAINED` test bypasses this block. These control tests
matter: the existence of two update routines does not show that both execute
on the same receiver iteration.

`V34EqualizerClearCenterTaps` clears the centre high halves, whereas
`V34EqualizerCleanUp` clears the whole equaliser. Preserve this difference
when comparing reacquisition, reset and full retrain histories.

## Carrier and timing recovery

The receiver rotates its target with sine/cosine carrier state. A proportional
phase-error term and an integrator update the numerical oscillator's phase;
the phase selects `costbl`, with interpolation between steps. Changing shifts,
rounding or the point at which state is reread can alter decisions even when
the mathematical loop appears unchanged.

The timing filters include complex positive/negative half-baud sections.
Their mirrored imaginary coefficients select opposite-frequency components;
timing state drives sample selection. The [coefficient reference](coefficients.md)
records the pole relationships and fixed-point scales. The floating PCM
resampler has separate timing/offset owners; see [PCM modes](modules.md#pcm-modes).

## Error reporting and rate control

The receiver accumulates `dr * dr + di * di` into `equerr_accum`. Its
`err_symcount` wraps at 1024. On wrap it publishes a shifted short `equerr`,
using `0x7fff` when the signed accumulator is negative, and clears the
accumulator. Precoder error and signal energy are published at that boundary
too. This is a scaled block statistic; it is not an instantaneous error or
a signal-power-normalised SNR.

The datapump supervisor observes the published error over receive bursts.
Its counters, timer gates and four independent retrain/rate-change decisions
are described in [per-block supervision](signal-processing.md#per-block-supervision).
An error counter measures a run of qualifying observations; it does not count
payload bits. Distinguish these units when diagnosing a repeated retrain.

## Evidence and experiments

The formulas and control tests above are readings of current reconstruction
source. They describe the implemented computation; they do not constitute a
new differential run or a modem conformance claim. Historical channel studies
remain in the investigation archive and Hindsight. A proposed improvement
needs a stated channel, constructor/lifecycle boundary, compatibility mode
and measured observables before it can become maintenance advice.
