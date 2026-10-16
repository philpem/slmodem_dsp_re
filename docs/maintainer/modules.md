# Module catalogue

Use this catalogue to locate each production module and the algorithm it owns.
Rows group cooperating translation units into a runtime module; every `.c`, `.cpp` and `.S` input under `src/` is linked once. The linked source is the
implementation reference, including comments that distinguish recovered evidence
from hypotheses. Names inherited from original input records can divide a module
across files. Empty record placeholders have no signal-processing algorithm.

The catalogue covers the source at `4169cb3b`. It records responsibilities and
algorithm families, rather than claiming that all paths are reachable or that
every filter design has been recovered. See [signal processing](signal-processing.md),
[training algorithms](training-algorithms.md) and [coefficient derivations](coefficients.md)
for the processing details and evidence limits.

## Core

### Registration and parameters

Register and unregister modem families; obtain host runtime state.

Ordered registration calls and a host parameter accessor. These functions do not process samples.

Sources: [dp_init.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/dp_init.c), [dp_param.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/dp_param.c).

### Fragment adapter

Bridge host fragments to a classic datapump.

Two-fragment rings accumulate complete native blocks; fixed rational conversion runs in each direction. An initial silent output fragment sets pipeline latency.

Sources: [dp_wrapper.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/dp_wrapper.c).

### Fixed rate conversion

Convert between supported sample rates.

Modes 2–19 select a rational polyphase FIR bank: a phase accumulator chooses a branch, the dot product uses Q14 coefficients, and the history is compacted between blocks. Modes 0/1 have a separate lifecycle, but resampling currently declines their kind-1 state; see the [coefficient reference](coefficients.md#current-fixedrc-boundary) for this existing component limitation.

Sources: [FixedRC.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/FixedRC.c).

### Diagnostics and parser

Encode diagnostic messages and expose the shipped parameter-parser entry points.

Character substitution encodes the diagnostic channel. Vparser retains the small shipped entry-point behaviour; do not infer a general file parser from its name.

Sources: [encode.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/encode.c), [Vparser.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/Vparser.c).

### Object record placeholders

Preserve original input-file records.

These inputs carry no reconstructed executable algorithm. Their presence supports final-object layout/provenance work.

Sources: [MEMORYC.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/MEMORYC.c), [pow.S](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/pow.S).

## Call setup

### Call datapump

Adapt host call control to dial generation and progress detection.

Convert the host stream to the call-progress clock, run dialler and detector state machines, and translate their messages into host events.

Sources: [call.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/call/call.c).

### Dialler

Interpret the dial string and generate digits, pauses and waits.

A state machine advances through dial tokens; generated tones and explicit zero samples implement audible digits and timed gaps. DialerConfig supplies country/control-dependent configuration.

Sources: [Dialer.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dialer/Dialer.c), [DialerConfig.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dialer/DialerConfig.c).

### Call-progress supervisor

Recognise dial tone, busy, ringback and timeout outcomes.

Tone verdicts drive table-based transitions. Cadence checks on/off durations and repetition counts; detector enable tables depend on configuration. See the call-progress reference.

Sources: [Callprog.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/callprog/Callprog.c), [Cadence.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/callprog/Cadence.c).

### Call-progress filters

Select and run frequency-selective call-progress filters.

Coefficient banks and recursive IIR sections isolate bands before cadence decisions. Filter energy and timing are separate observations.

Sources: [CPfiltrs.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/callprog/CPfiltrs.c), [Elliptic1.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/callprog/Elliptic1.c), [Elliptic2.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/callprog/Elliptic2.c), [Elliptic3.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/callprog/Elliptic3.c), [toneiir.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/callprog/toneiir.c).

### Calling and answer tones

Generate calling tones and classify answer-tone combinations.

Oscillator/filter stages and energy decisions implement the tone-specific paths. DualTone detection does not own busy/ringback cadence.

Sources: [CallingTone.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/callprog/CallingTone.c), [DualTone_Detector.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/callprog/DualTone_Detector.c).

## Shared DSP

### Adaptive equalisation

Compensate channel distortion and decide received constellation points.

Fractionally spaced filtering maintains complex sample histories; LMS gradient updates change coefficients. Mode-specific decision callbacks select training or data slicers.

Sources: [fpm_adeq.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_adeq.c), [fpm_fse.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_fse.c).

### Gain and level

Estimate signal level and control gain.

Scaled sum-of-squares and square-root lookup produce the level estimate; reciprocal arithmetic forms gain. Freeze/release state controls adaptation. The RMS scaling is fixed, so it is not an arbitrary-length statistical RMS API.

Sources: [fpm_agc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_agc.c), [fpm_rms.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_rms.c).

### Fixed-point arithmetic

Supply scaled nonlinear arithmetic and circular dot products.

Normalisation, lookup and fixed-point products implement reciprocal, square root, logarithm and angle. FP_Pow is an exponential. Preserve narrowing and shift boundaries; mathematical equivalence alone does not preserve integer results.

Sources: [FP_math.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/FP_math.c), [fpm_atan.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_atan.c), [fpm_div.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_div.c), [fpm_div32.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_div32.c), [fpm_log10.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_log10.c), [fpm_sqrt.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_sqrt.c).

### Echo cancellation

Subtract the locally transmitted signal from receive samples.

A transmit delay line feeds near/far complex histories; adaptive convolutions estimate echo, which is subtracted before receive decisions.

Sources: [fpm_ecc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_ecc.c).

### FSK

Convert bits to frequency-shift keyed samples and recover bits.

The transmitter switches oscillator increments between mark and space. The receiver uses a delayed-signal discriminator and low-pass filtering; role-specific filters and frequency plans belong to the modem configuration.

Sources: [fpm_fsm.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_fsm.c), [fpm_fsd.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_fsd.c).

### Recursive and multi-rate filters

Filter fixed-point streams and convert their sampling clock.

IIR routines use biquad cascades with entry-specific direct forms. MRF uses phase-selected FIR branches rather than explicitly inserting zero samples.

Sources: [fpm_iir.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_iir.c), [fpm_mrf.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_mrf.c).

### Multi-tone detection

Test a stream for configured tones.

A resonator bank compares tone energy with input energy. The observation interval and coefficient bank determine the meaning of the threshold.

Sources: [fpm_mtd.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_mtd.c).

### Oscillators and tones

Generate quadrature sinusoids and configured answer tones.

A phase accumulator selects a quarter-wave table, reflects quadrants and interpolates. Tone generation adds configured phase reversals; fixed clock assumptions are listed in the sample-rate reference.

Sources: [fpm_phasor.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_phasor.c), [fpm_tone.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_tone.c).

### Transmit pulse shaping

Turn mapped I/Q symbols into band-limited samples.

Polyphase FIR branches convolve separate symbol histories. This transmit PPS is distinct from the V.22 receiver phase splitter.

Sources: [fpm_pps.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_pps.c).

### Scrambling and coding

Whiten bits, map symbols and recover trellis-coded data.

Feedback shift registers implement scrambling; differential/quadrant and constellation tables implement mapping. Trellis encoding and Viterbi path metrics/survivors implement coded transmission and reception.

Sources: [fpm_sdm.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_sdm.c), [fpm_smc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_smc.c), [fpm_tren.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_tren.c), [fpm_vtb.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_vtb.c).

### Timing recovery

Estimate and correct symbol sampling time.

Filtered phase/error measurements feed the timing state and fractional sample selection. Modem-specific SRE configurations supply coefficients and clocks.

Sources: [fpm_sre.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_sre.c).

### Fixed DSP tables

Own shared constant/configuration records.

Original tables supply coefficients, pointers and dimensions to the algorithms above; table ownership is distinct from runtime state.

Sources: [fpm_tables.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fpm_tables.c).

### Floating filters

Perform floating FIR, recursive and notch filtering.

FloatFIR convolves a compacted history. GenericIIR uses separate input/output histories and double accumulation; den[0] = 0 skips normalisation. FloatARMA pre-normalises coefficients and rounds counts up to four. Notch is a second-order recursive section.

Sources: [FloatFIR.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/FloatFIR.cpp), [FloatIIR.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/FloatIIR.cpp), [FloatARMA.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/FloatARMA.cpp), [Notch.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/Notch.c).

### Floating tone detectors

Detect tones and configure the ANSam frequency-selective path.

IIR filtering and accumulated input/output energies drive persistence decisions. GenericToneDetector block processing has an additional weaker detection arm; repeating the scalar API is not equivalent.

Sources: [GenericToneDetector.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/GenericToneDetector.cpp), [AnsamToneDetector.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/AnsamToneDetector.cpp).

### Spectrum analysis

Transform sample blocks and estimate spectral power.

Complex FFT butterflies and a real-input transform support windowed power spectra. Psd selects linear, dB or peak-referenced output; preserve its window and overlap semantics.

Sources: [fft.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/fft.cpp), [psd.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dsp/psd.cpp).

## Classic data

### Bell 103 / V.21

Provide host adaptation, modem control and FSK data paths.

b103.c owns registration/wrapping; B103.c/B103int.c/B103prc.c own engine lifecycle, interfaces and progression. Configuration/table units set station-specific tone plans. Shared FSK, gain and rational filters connect the 8000-sample/s interface to the 7200-sample/s core.

Sources: [B103.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/b103/B103.c), [B103int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/b103/B103int.c), [B103prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/b103/B103prc.c), [B103tab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/b103/B103tab.c), [b103.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/b103/b103.c), [b103_cfg.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/b103/b103_cfg.c), [b103_tables.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/b103/b103_tables.c).

### V.22 / V.22bis

Run a frequency-division duplex QAM modem.

v22.c adapts the host; V22/V22int/v22prc/v22stc sequence training and control. v22mod and V22Dec encode/decode symbols. MRF converts rate, receive PPS splits phase, FSE equalises, SRE recovers timing, and IIR filters select bands. Transmit PPS uses a 40-phase, three-tap bank; rx/tx table units supply constants.

Sources: [V22.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/V22.c), [V22Dec.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/V22Dec.c), [V22int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/V22int.c), [v22.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22.c), [v22_fse.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22_fse.c), [v22_iir.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22_iir.c), [v22_mrf.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22_mrf.c), [v22_pps.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22_pps.c), [v22_sre.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22_sre.c), [v22mod.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22mod.c), [v22prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22prc.c), [v22rxtab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22rxtab.c), [v22stc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22stc.c), [v22txtab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22txtab.c).

### V.23

Run the asymmetric FSK modem and channel detector.

v23.c wraps the host; v23modem sequences control, v23rx/v23tx perform directional processing, V23filt supplies filters and bwchdem measures channel content. Role selects the transmit/receive channel configuration.

Sources: [V23filt.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v23/V23filt.c), [bwchdem.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v23/bwchdem.c), [v23.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v23/v23.c), [v23modem.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v23/v23modem.c), [v23rx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v23/v23rx.c), [v23tx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v23/v23tx.c).

## V.32

### Host and engine interfaces

Create and supervise the V.32/V.32bis duplex datapump.

The host adapter converts fragments and I/O-delay units. Engine/interface routines run the fixed-point pipeline; protocol/control routines select training, data, retrain and diagnostics.

Sources: [v32.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/v32.c), [V32.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32.c), [V32int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32int.c), [V32mod.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32mod.c), [V32stc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32stc.c).

### Handshake and training

Coordinate originate/answer/loopback training and turnaround.

Separate transmit and receive half-duplex machines generate and recognise training sequences before full-duplex data operation. Dispatcher tables select next-state handlers; V32prc generates/detects sequences and v32anstone times answer-tone cadence.

Sources: [V32RNG.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32RNG.c), [V32TXHDX.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32TXHDX.c), [V32ans.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32ans.c), [V32org.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32org.c), [V32loop.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32loop.c), [V32rxhdx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32rxhdx.c), [V32prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32prc.c), [V32states.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32states.c), [v32anstone.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/v32anstone.c).

### Coding and decisions

Scramble, map and slice V.32 symbols.

Two-tap self-synchronising registers scramble/descramble the stream; absolute, differential and trellis encoders use constellation tables. Equaliser callbacks select training or data decisions.

Sources: [V32SMC_TX.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32SMC_TX.c), [V32Sdm_rx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32Sdm_rx.c), [V32Sdm_tx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32Sdm_tx.c), [V32dec.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32dec.c).

### Configuration and tables

Own modem, equaliser, timing, echo, constellation and dispatch constants.

Tables configure the shared fixed-point algorithms. V32TAB144 holds trellis constellations. V32RXTAB and V32TXTAB are empty record placeholders; they add no sample-processing algorithm.

Sources: [v32dec_tables.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/v32dec_tables.c), [v32ecc_tables.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/v32ecc_tables.c), [v32fptab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/v32fptab.c), [v32fse_tables.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/v32fse_tables.c), [v32hdx_tables.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/v32hdx_tables.c), [v32sre_tables.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/v32sre_tables.c), [V32TAB144.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32TAB144.c), [V32RXTAB.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32RXTAB.c), [V32TXTAB.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/V32TXTAB.c), [v32cfg.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/v32cfg.c).

## V.34

### Handshake and PCM integration

Coordinate V.34 training/data supervision and the PCM session boundary.

Three concurrent handshake state words schedule per-sample and per-block work. datapumpv34 preserves transmit-before-receive order and four independent retrain/rate decisions. VPcmV34Main owns lifecycle, progress and V.90/V.92 handover.

Sources: [V34hshak.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/V34hshak.c), [VPcmV34Main.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/VPcmV34Main.cpp).

### Receive and transmit engines

Recover symbols/bits and synthesise the outgoing waveform.

Receive filtering, timing/carrier recovery, complex adaptive equalisation and decoding feed shell state. Transmit mapping, shaping and carrier generation produce samples. v34filters supplies the timing, echo and equaliser primitives; see training algorithms for the LMS update.

Sources: [V34RX.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/V34RX.c), [V34TX.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/V34TX.c), [v34filters.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/v34filters.c).

### Mapping and scrambling

Convert bit groups into coded frames and back.

Shell mapping distributes energy/ring choices; digital frame logic handles coding and bit allocation. GPA/GPC feedback recurrences scramble in complementary station roles.

Sources: [v34digital.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/v34digital.c), [v34shell.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/v34shell.c), [v34scram.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/v34scram.c).

### Probing, information and diagnostics

Measure training signals, exchange INFO parameters and expose diagnostics.

Discrete-frequency accumulators estimate probe energy; differential phase signalling carries training information. Bit packing/unpacking exchanges parameters, and diagnostic readers expose bounded state arrays.

Sources: [DFTC.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/DFTC.c), [DPSK.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/DPSK.c), [detector.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/detector.c), [v34info.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/v34info.c), [v34info1a.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/v34info1a.cpp), [v34diag.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/v34diag.cpp).

### Constants

Own shaping, mapping and training constants.

Lookup/configuration records feed the V.34 engines; retain their original scales and layouts. Coefficient derivations identify which designs can be regenerated.

Sources: [V34ARRAY.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/V34ARRAY.c), [V34CONST.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/V34CONST.c).

## PCM modes

### Host and session adapters

Connect host samples/bits to V.34 and PCM modem owners.

vpcm groups samples in fours and converts integer/float samples and byte/int bits. Session/modem dispatch selects direction and phase. NoK56Flex preserves the shipped unsupported K56Flex behaviour; its class name is not evidence of a working modem.

Sources: [vpcm.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/vpcm.c), [VpcmFloModem.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/VpcmFloModem.cpp), [V90Modem.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Modem.cpp), [V92Modem.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92Modem.cpp), [NoK56Flex.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/NoK56Flex.cpp).

### Resampling and clock recovery

Interpolate the PCM stream and track clock phase/offset.

A polyphase FIR advances fractional phase and input credit. The designing constructor transposes a Blackman-windowed sinc bank; timing and offset subclasses add correction state. The adopting constructor has different coefficient ownership.

Sources: [Resampler.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/Resampler.cpp), [ResamplerTiming.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/ResamplerTiming.cpp), [ResamplerTimingOffset.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/ResamplerTimingOffset.cpp), [V90Resampler.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Resampler.cpp).

### Phase progression

Run phase-specific training and transition into payload operation.

State-driven symbol generators and receiver decisions exchange training patterns and messages. Transition codes can change the active phase within a block; preserve per-symbol phase reads and the resampler split position.

Sources: [V90Modulator.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Modulator.cpp), [V90Demodulator.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Demodulator.cpp), [V90Phase3Demodulator.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Phase3Demodulator.cpp), [V90Phase3Modulator.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Phase3Modulator.cpp), [V90Phase4Demodulator.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Phase4Demodulator.cpp), [V90Phase4Modulator.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Phase4Modulator.cpp), [V92Modulator.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92Modulator.cpp), [V92Phase3Modulator.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92Phase3Modulator.cpp), [V92Phase4Modulator.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92Phase4Modulator.cpp).

### Protocol records

Represent configuration, negotiated messages and phase measurements.

Pack/unpack bit fields, validate message CRCs where provided, and retain negotiated parameters. CPpck builds the analogue CP stream; Phase2Info reports measurements rather than performing channel estimation.

Sources: [V90CP.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90CP.cpp), [V90CPpck.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90CPpck.cpp), [V90MP.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90MP.cpp), [V90Jd.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Jd.cpp), [V92CP.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92CP.cpp), [V92Jd.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92Jd.cpp), [V90Parameters.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Parameters.cpp), [V92Parameters.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92Parameters.cpp), [V90Phase2Info.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Phase2Info.cpp), [V92Phase2Info.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92Phase2Info.cpp).

### Impairment learning and evaluation

Design training sequences, learn digital impairments and assess connection state.

DIL descriptor packing defines the learning sequence; measured level/pattern decisions estimate impairments and phase completion. TRN2d constructs training symbols; R/SD detectors recognise their training signals.

Sources: [V90AutoDigitalImpDetector.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90AutoDigitalImpDetector.cpp), [V90ConnectionEvaluator.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90ConnectionEvaluator.cpp), [V90DILdesPCK.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90DILdesPCK.cpp), [V92DILdesPCK.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92DILdesPCK.cpp), [V90DilDescriptorSettings.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90DilDescriptorSettings.cpp), [V90TRN2dDesigner.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90TRN2dDesigner.cpp), [V90RDetector.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90RDetector.cpp), [V90SdDetector.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90SdDetector.cpp).

### Constellation design

Choose usable PCM levels subject to rate and power constraints.

Candidate constellations use learned levels and power calculations. Search and rate limits constrain the selected mapping; distinguish measured PCM levels from G.711 code conversion.

Sources: [V90ConstellationDesigner.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90ConstellationDesigner.cpp), [V90ConstellationPower.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90ConstellationPower.cpp).

### Mapping and modulus coding

Convert payload groups into selected PCM symbols and recover them.

Mixed-radix/modulus operations index negotiated constellations; sign handling and hard decisions invert the mapping. V.92 adds its own mapping/encoding path rather than replacing every V.90 helper.

Sources: [V90Mapper.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Mapper.cpp), [V92Mapper.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92Mapper.cpp), [V90Demapper.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Demapper.cpp), [V90MappingParamsInt.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90MappingParamsInt.cpp), [V92MappingParamsInt.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92MappingParamsInt.cpp), [V90ModulusEncoder.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90ModulusEncoder.cpp), [V92ModulusEncoder.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92ModulusEncoder.cpp), [V90ModulusDecoder.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90ModulusDecoder.cpp), [V90bitsToSymbol.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90bitsToSymbol.cpp), [V92bitsToSymbol.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92bitsToSymbol.cpp), [V90SignBitsExtractor.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90SignBitsExtractor.cpp).

### Receive conditioning

Condition received samples before PCM decisions.

Selected FIR banks and reference-loop tables feed prefiltering. The equaliser uses linear and decision-feedback paths with configurable adaptation steps; preserve error and narrowing boundaries.

Sources: [V90Equalizer.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90Equalizer.cpp), [V90PreFilter.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90PreFilter.cpp), [V90PreFilterCoeffType1.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90PreFilterCoeffType1.cpp), [V90PreFilterCoeffType2.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90PreFilterCoeffType2.cpp), [V90PreFilterCoeffType3.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90PreFilterCoeffType3.cpp), [V90PreFilterRefLoops.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90PreFilterRefLoops.cpp), [V92PreFilter.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92PreFilter.cpp).

### Spectral shaping and checks

Shape transmitted spectra and check received spectral constraints.

Filter/state decisions constrain spectral content. SpectralVerifier delegates spectrum estimation to Psd; FFT rounding differences can therefore originate below the verifier.

Sources: [V90SpectralShaper.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90SpectralShaper.cpp), [V90SpectralShapingFilter.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90SpectralShapingFilter.cpp), [V90SpectralVerifier.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V90SpectralVerifier.cpp).

### V.92 upstream

Encode, precode and transmit upstream data while cancelling local echo.

Convolutional coding and precoder feedback precede waveform generation. Adaptive echo estimation uses transmit history to remove local contribution from the receiver.

Sources: [V92Transmitter.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92Transmitter.cpp), [V92Precoder.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92Precoder.cpp), [V92EchoCanceller.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92EchoCanceller.cpp), [V92ConvEncoder.cpp](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/V92ConvEncoder.cpp).

## V.8

### Negotiation interface

Negotiate capabilities and request host datapump handover.

The wrapper translates host parameters/events; the interface builds/reads messages; concurrent handshake state selects tone, FSK and timeout actions.

Sources: [v8.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/v8/v8.c), [V8Interface.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/v8/V8Interface.c), [V8.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/v8/V8.c).

### Signalling DSP

Generate and recognise V.8 tones and V.21 messages.

Frequency accumulators and phase-reversal tests classify tones. V.21 FSK modulation/filtering carries CM/JM messages; shared routines maintain sample/bit queues and fixed-point products.

Sources: [V8Detector.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/v8/V8Detector.c), [V8Dftc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/v8/V8Dftc.c), [V8Dpsk.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/v8/V8Dpsk.c), [V8Fsk.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/v8/V8Fsk.c), [V8global.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/v8/V8global.c).

## Services

### Caller ID

Receive and interpret Caller ID messages.

The host service wraps the core state machine. Cidfsd/Cidmtd demodulate FSK and detect tones; Rxcid assembles asynchronous bits into bytes, and Data renders decoded records. National variants require separate evidence.

Sources: [cid.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/cid.c), [cid.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/cidcore/cid.c), [Cidfsd.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Cidfsd.c), [Cidmtd.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Cidmtd.c), [Rxcid.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Rxcid.c), [Data.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Data.c).

### DTMF

Identify paired dial tones, including Caller ID DTMF.

Band-pass/resonator energy decisions choose low/high groups, with rejection, persistence and release logic. The fixed detector uses group pre-notches; the floating receiver owns separate coefficient banks.

Sources: [Dtmf.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Dtmf.c), [Dtmf_Detector.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Dtmf_Detector.c), [Dtmf_Rx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Dtmf_Rx.c).

### Ring and DC removal

Detect ringing and remove slowly varying offsets.

rd translates host events; RingDetector tracks threshold/cadence measurements. dcr estimates running means across configured phase intervals and subtracts the DC estimate.

Sources: [rd.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/rd.c), [ringDetector.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/ringDetector.c), [dcr.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/dcr.c).

### Voice control and streaming

Select voice mode and process receive, transmit, duplex and silence states.

Service/session dispatch installs directional handlers. Streams convert/route samples; the silence detector combines level observations with duration. The two voice.c files own distinct original records.

Sources: [voice.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/voice.c), [voice.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/voice/voice.c), [Rx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Rx.c), [Tx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Tx.c), [duplex.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/duplex.c), [silence.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/silence.c).

### Speakerphone

Allocate and run the full-duplex speakerphone kernel.

Fdsp owns allocation state; Fdspkrnl sequences the kernel and shared DSP calls. Fifo8 is a byte ring with its original narrow-index arithmetic.

Sources: [Fdsp.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Fdsp.c), [Fdspkrnl.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Fdspkrnl.c), [Fifo8.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Fifo8.c).

### Voice tones and numerical utilities

Generate beeps/tones, recognise voice tones and supply utility calculations.

Phase/table oscillators generate samples; filtering and energy tests classify tones. Beepgen also owns float/linear conversion and maximum-magnitude helpers; TABLES owns MTK trigonometric/bit-count constants.

Sources: [Beepgen.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Beepgen.c), [Detector.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/Detector.c), [PHASOR.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/PHASOR.c), [TABLES.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/TABLES.c), [TONE.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/TONE.c).

### PCM companding

Convert linear samples and G.711 A-law/µ-law codes.

Piecewise segment/exponent and sign encoding implement companding and expansion. Host short scaling and V.90 constellation selection are separate operations.

Sources: [pcm.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/service/pcm.c).

## Fax

### Service and Class 1

Dispatch Class 1 commands and construct directional modem paths.

Command/state logic selects modulation, silence and framing. class1rx/class1tx own directional construction/reporting; configuration records supply receive modes.

Sources: [fax.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/fax.c), [class1.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/class1.c), [class1rx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/class1rx.c), [class1tx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/class1tx.c), [faxcfg.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/faxcfg.c).

### Virtual modem and framing

Adapt modem bits to host frames and buffers.

VMI dispatch chooses asynchronous, transparent or HDLC codecs. HDLC uses flags, bit stuffing and frame-check sequences; utility paths reverse bits and move ring data. cDATA/cHDLC own directional state; FIFO holds 16-bit elements. faxvmi_tbls is an empty input record.

Sources: [faxvmi.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/faxvmi.c), [faxvmi_asyc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/faxvmi_asyc.c), [faxvmi_hdlc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/faxvmi_hdlc.c), [faxvmi_null.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/faxvmi_null.c), [faxvmi_pack.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/faxvmi_pack.c), [faxvmi_tbls.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/faxvmi_tbls.c), [faxvmi_utls.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/faxvmi_utls.c), [faxvmififo.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/faxvmififo.c), [T30frames.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/T30frames.c), [cDATArx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/cDATArx.c), [cDATAtx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/cDATAtx.c), [cHDLCrx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/cHDLCrx.c), [cHDLCtx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/cHDLCtx.c), [cTOOLS.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/cTOOLS.c), [fifo.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/fifo.c), [reversedchars.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/reversedchars.c).

### Modem adaptation

Wrap V.17, V.21, V.27ter and V.29 engines behind the fax interface.

Each adapter forwards lifecycle/control/sample processing to its modem and translates status/message codes. It does not implement the whole T.30 procedure.

Sources: [Vmi_v17.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/Vmi_v17.c), [Vmi_v21.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/Vmi_v21.c), [Vmi_v27.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/Vmi_v27.c), [Vmi_v29.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/Vmi_v29.c).

### V.21 signalling

Carry the low-speed fax signalling stream.

Receive/transmit engine, interface (_int), progress (_prc) and state-control (_stc) units implement the directional FSK pipelines. V21rxtab is an empty record placeholder.

Sources: [V21r_int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V21r_int.c), [V21r_prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V21r_prc.c), [V21r_stc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V21r_stc.c), [V21rx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V21rx.c), [V21rxtab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V21rxtab.c), [V21t_int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V21t_int.c), [V21t_prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V21t_prc.c), [V21t_stc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V21t_stc.c), [V21tx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V21tx.c).

### V.17 modulation

Train and run the V.17 fax waveform.

Directional engines use fractionally spaced equalisation, timing recovery, constellation decisions and trellis/Viterbi coding. _int exposes interfaces, _prc progresses states, _stc controls them; table units supply filters/constellations. V17txtab is an empty record placeholder.

Sources: [V17r_int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17r_int.c), [V17r_prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17r_prc.c), [V17r_stc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17r_stc.c), [V17rx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17rx.c), [V17rxdec.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17rxdec.c), [V17rxtab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17rxtab.c), [V17t_int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17t_int.c), [V17t_prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17t_prc.c), [V17t_stc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17t_stc.c), [V17tx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17tx.c), [V17txtab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V17txtab.c), [v17.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/v17.c), [v17dec_tables.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/v17dec_tables.c), [Vtb_tab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/Vtb_tab.c).

### V.27ter modulation

Train and run the V.27ter fax waveform.

Phase decisions, equaliser training and directional pulse shaping implement the signal path; V27_SDM implements the guarded scrambling recurrence. Interface/progress/control and table units have the same role split as V.17.

Sources: [V27_SDM.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27_SDM.c), [V27r_int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27r_int.c), [V27r_prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27r_prc.c), [V27r_stc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27r_stc.c), [V27rx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27rx.c), [V27rxdec.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27rxdec.c), [V27rxtab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27rxtab.c), [V27t_int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27t_int.c), [V27t_prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27t_prc.c), [V27t_stc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27t_stc.c), [V27tx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27tx.c), [V27txtab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V27txtab.c), [v27.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/v27.c).

### V.29 modulation

Train and run the V.29 fax waveform.

Directional training and QAM symbol decisions use mode-specific equaliser/timing and constellation tables. _int, _prc and _stc separate external calls, progression and control.

Sources: [V29r_int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29r_int.c), [V29r_prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29r_prc.c), [V29r_stc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29r_stc.c), [V29rx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29rx.c), [V29rxdec.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29rxdec.c), [V29rxtab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29rxtab.c), [V29t_int.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29t_int.c), [V29t_prc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29t_prc.c), [V29t_stc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29t_stc.c), [V29tx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29tx.c), [V29txtab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/V29txtab.c).

### Shared fax coding

Provide common scrambling, sequence and symbol-coding support.

Feedback recurrences, training-sequence control and table-driven absolute/differential/trellis symbol mappings serve the modem engines; Tab144/Tx_rxtab own shared constants.

Sources: [SDM.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/SDM.c), [Sgd.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/Sgd.c), [Smc.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/Smc.c), [Smc_tx.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/Smc_tx.c), [Tab144.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/Tab144.c), [Tx_rxtab.c](https://github.com/philpem/slmodem_dsp_re/blob/master/src/fax/Tx_rxtab.c).

## Header-defined components

Some algorithms are instantiated from headers and therefore do not own a
separate production `.cpp` file. Include their definitions when tracing callers.

| Component | Responsibility and algorithm |
|---|---|
| [LowPassFIR](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/LowPassFIR.h) | Windowed-sinc coefficient design; owns coefficients, with no sample-processing history. |
| [DspMath](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/DspMath.h) | Window design, numerical helpers and shared DSP templates. Blackman window selection is used by the designing Resampler constructor. |
| [GenericIIR](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/GenericIIR.h) | Feed-forward minus feedback difference equation; float samples with double coefficients/accumulation in the shipped instantiation. |
| [Agc](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/Agc.h) | Floating gain-control state and processing used by PCM receivers. |
| [ModulusCoder](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/ModulusCoder.h) | Shared modulus encoder/decoder layouts and operations behind the PCM coding units. |

The [architecture chapter](architecture.md) maps the host boundary and ownership.
Recovered layouts and template contracts remain in `include/dsplib/`; a name in
this catalogue does not override an explicit field width, allocation rule or
coefficient-ownership condition in those definitions.
