# Signal processing and modem families

A datapump has two simultaneous jobs: synthesise the waveform that carries
outgoing bits and estimate incoming bits from a distorted, delayed waveform.
The transmit and receive paths share negotiated parameters but keep their own
histories and progress. A sample, a symbol and a payload bit are different
units; never convert between them without the mode's rate and mapping.

The [module catalogue](modules.md) identifies the implementing files for every
production module. [V.34 training algorithms](training-algorithms.md) develops
the probing, adaptive equalisation and recovery loops described here.

## The basic signal chain

Conceptually, a quadrature amplitude modulation (QAM) transmitter performs:

```text
payload bits -> scrambling -> coding / constellation mapping
             -> pulse shaping -> carrier modulation -> line samples
```

Scrambling prevents repetitive payload patterns from dominating the signal.
Coding and mapping select constellation points, which represent amplitude
and phase. Pulse shaping limits the occupied spectrum and controls how adjacent
symbols interfere. Carrier modulation places the shaped signal in the
telephone channel. Some implementations combine these steps within a loop.

A corresponding receiver estimates gain, timing and carrier phase, filters
and equalises the signal, makes symbol decisions, and reverses the coding and
scrambling. An equaliser compensates channel distortion; an echo canceller
estimates the contribution of the local transmitter to the received signal.
They solve different problems. Adaptation updates their coefficients from
training sequences or decisions, and may be frozen during specific protocol
states.

The DSP names in this tree make these responsibilities easier to locate:

| Name | Role and source entry |
|---|---|
| AGC | Automatic gain control; `src/dsp/fpm_agc.c`, `include/dsplib/Agc.h` |
| FSE | Fractionally spaced equalisation; `src/dsp/fpm_fse.c` and mode-specific receive code |
| ECC | Echo cancellation; `src/dsp/fpm_ecc.c`, `include/dsplib/V92EchoCanceller.h` |
| MRF | Multi-rate filtering; `src/dsp/fpm_mrf.c` |
| PPS | Polyphase pulse shaping; `src/dsp/fpm_pps.c` and mode-specific filters |
| FSM / FSD | Frequency-shift modulation / demodulation; `src/dsp/fpm_fsm.c`, `fpm_fsd.c` |
| FIR / IIR | Finite / infinite impulse response filters; `FloatFIR.cpp`, `FloatIIR.cpp` and fixed-point equivalents |
| PSD | Spectral power estimation; `src/dsp/psd.cpp` |

These are navigation aids, not interchangeable APIs. Use the
[glossary](glossary.md)
for project-specific expansions and misleading legacy names.

## Low-rate FSK: Bell 103, V.21 and V.23

Frequency-shift keying (FSK) represents bit values with different tones.
Bell 103's originate and answer stations transmit in different bands. Its
receive oscillators translate the opposite station's tones into a common
baseband so both directions can share a discriminator design. The adapter's
station role chooses the channel filters and oscillator, not merely a status
label.

Start at
[`b103.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/b103/b103.c)
and the `b103fp` layout, then follow the `FPM_*` components. The family can
select a V.21 tone plan; the fax V.21 path has its own integration. V.23 has
its own adapter and asymmetric channel roles. Avoid treating every FSK path
as Bell 103 with different constants.

## V.22/V.22bis and V.32/V.32bis

V.22bis uses frequency division for the two directions. The reconstructed
V.22 transmitter exposes the separation between constellation encoding and
pulse shaping through `FPM_SMC_encoder` and `V22_PPS_filter`. See
[`v22.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v22/v22.c)
for its host boundary.

V.32 and V.32bis use a duplex modem architecture with echo cancellation.
The half-duplex training states prepare that full-duplex operation; “Hdx”
in a training filename does not mean the final data service is half duplex.
Trace `V32FP_modem`, the transmit/receive training states, then the data
modulator and demodulator. The
[V.32 structure record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/v32-structure-reconstruction.md)
explains recovered object ownership. Keep V.32 work together across original
span labels: those labels can split a single protocol subsystem.

The [standards table](references.md#protocol-standards) distinguishes V.32's
9600 bit/s ceiling from V.32bis's 14,400 bit/s. A mode's bit rate is not its
sample rate or symbol rate.

## V.34

V.34 adds more elaborate channel probing, coding and rate selection. Read
`V34hshak.c` for sequencing, `V34RX.c` for receive processing and
`v34shell.c`/`v34digital.c` for mapping and digital state. The standard's
training phases provide protocol context; the implementation's three state
machines determine when work actually happens.

The receive map in
[`v34recv.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/v34recv.h)
defines the `0x79c`-byte `receiver` member embedded in `v34_object` at
`+0x264`. The receive queue, burst window, timing filters and equalizer all
belong to that member. Root and receiver-relative offsets therefore identify
the same fields with a `0x264` displacement between their bases; for example,
`receiver.short_382` is at root `+0x382` and receiver `+0x11e`. Both headers
assert the layout. See [MG-02](reconstruction-debt.md#mg-02-overlapping-v34-root-and-receiver-views)
for the ownership map and the correction to the guide's earlier overlap claim.

### Per-block supervision

`datapumpv34` in `V34hshak.c` schedules the V.34 engines and decides when to
retrain or renegotiate. It holds the existing receiver and transmit queue
through typed pointers; the supervisor's state belongs to `v34_object` and
its embedded `v34_receiver`, rather than a separate supervisor allocation.

First it checks the unsigned span `sample_count - int_0248`. A value greater
than 288,000 sets `progress` to 5. The dispatch then depends on `hs_mode`:

| Path | Work performed |
|---|---|
| Unsigned `hs_mode > 1` | Call `v34handshak` while transmit space remains **or** the receive queue contains more than five samples, then return |
| Data path | Fill the transmit block with `modulatevector`, drain receive bursts with `receiver`, then evaluate the four supervisor decisions |

Preserve the transmit-before-receive order. `demapFrame`, reached through
`receiver` and `decoderv34`, can set the receive shell's `latched` member at
a frame boundary. On a fresh vector, `modulatevector` reads that latch to
advance `train_symcount` and eventually set `data_enable`. Reversing the loops
can make transmit processing observe a latch established in the same block.

Queue guards are rechecked after the engine calls; a handshake fixture must
establish that its selected state can advance those guards. The data-path receiver loop increments `rx_blocks`
through its signed 16-bit cap test before calling `receiver`. It then reads
the error that the receiver has just published and updates consecutive-run
counters:

| Receiver member | Qualifying error | When updated |
|---|---|---|
| `bad_run` | `equerr > bad_thresh` | Every receive burst |
| `bad_long_run` | `equerr > bad_long_thresh` | Unsigned elapsed span greater than 144,000 |
| `good_run` | `equerr < good_thresh` | Unsigned elapsed span greater than 1,152,000 |

A qualifying burst increments the relevant 16-bit counter; a nonqualifying
burst clears it. A delayed counter stays untouched until its timer opens.
These constants count the implementation's sample clock; convert them to
seconds only after establishing the clock at the boundary being investigated.
Error/threshold comparisons and the block-cap comparison are signed 16-bit.
A zero-extending load in the binary does not by itself establish unsigned
source semantics.

The four decisions are independent `if` statements, evaluated in order:

1. A retrain flag or `bad_run` above half `ratecfg.rx_baud` starts a retrain.
2. A remote-renegotiation flag starts remote rate renegotiation.
3. `bad_long_run > 2 * ratecfg.rx_baud` requests a local step down.
4. `good_run > 8 * ratecfg.rx_baud` requests a local step up.

The down/up comparisons sign-extend the counter and retain a 32-bit product.
Each selected action clears run counters. Renegotiation records direction
in `rrn_local_dir` (1 remote, 2 down, 3 up), copies `ratecfg.rxbits` into
`baud_copy` after initialisation, and reports the corresponding indication.
Retrain rechecks its mode decision after `v34handshakinit`. Later decisions
read state after earlier initialisation and indication calls. Hoisting those
reads or changing the four decisions into an `else if` chain changes the
observed call boundaries.

For the original-operand audit, signedness correction and remaining code
generation differences, see the
[supervisor recovery record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/maintainer-offset-access.md).

## V.90 and V.92

[V.90](https://www.itu.int/rec/T-REC-V.90/en) specifies a digital/analogue
modem pair, with asymmetric maximum downstream and upstream rates. Two
analogue V.90 endpoints do not substitute for the required digital peer.
This matters when designing an end-to-end test: component-level PCM tests
and a successful V.34 fallback do not establish a V.90 connection.

The PCM session bridges the V.34 negotiation and the C++ V.90/V.92 components.
`V90Modem` contains shared parameters and selects its modulator/demodulator
work by modem side. Phase-specific components handle training and parameter
exchange before the data mapper or demapper can use the selected constellation.
Digital impairment learning (DIL), spectral verification and constellation
design each have separate objects under `src/pump/v90/`.

V.92 enhances V.90; its transmitter, precoder, echo canceller and phase
modulators are additional owners, not evidence that every V.90 method is
replaced. Follow the session dispatch before choosing which object's state
to inspect. For PCM levels, keep G.711 A-law/µ-law code conversion separate
from the V.90 mapping rules and from host `short` sample scaling. See
[G.711 and V.90](references.md#protocol-standards).

## Fax and ancillary services

Fax Class 1 exposes control and data boundaries through `class1*.c` and
the `Vmi_v*.c` adapters declared in `faxadapt.h`; HDLC framing is in
`faxvmi_hdlc.c`. V.21 carries low-speed fax
control signalling, while V.27ter, V.29 and V.17 supply fax modem paths.
Class 1 DCE control and the higher-level fax procedure are distinct; use
T.31 and T.30 for their respective context. Fax source reconstruction is
closed; remaining validation and structural work should be described at the
specific boundary involved.

Caller ID, ring detection, DTMF and voice have service entry points outside
the ordinary data-pump registration sequence. They reuse DSP primitives but
have different framing and timing requirements. In a detector investigation,
record both the sample rate and observation-window size before interpreting
a threshold or cadence duration.

## Rates and coefficients

Host rate, native datapump rate and internal filter rate can all differ.
For example, Bell 103's wrapper interface runs at 8000 samples/s while its
FSK core uses a 7200 samples/s design. Changing the host rate alone cannot
retarget every loop or coefficient bank.

Use the existing
[sample-rate register](sample-rates.md),
[coefficient derivations](coefficients.md)
and [resampler bank record](resampler-banks.md).
Preserve original coefficients for reconstruction. The separate 8 kHz
retarget needs design-based regeneration and its own functional evidence.
