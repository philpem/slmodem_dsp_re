# From dialling to data

A modem call must establish a usable line, agree capabilities, train its
signal-processing loops and then carry payload. Follow status and parameter
changes across those boundaries; a recognisable tone alone does not establish
that a call has reached data mode.

## 1. Establish the call

The call datapump brings together the dialler and call-progress detectors.
The dialler produces digits and intentional silence for gaps and waits. Tone
detection identifies frequency content; cadence detection checks how long
that content is present and absent. Busy and ringback therefore depend on
both frequency and timing.

Start in
[`call.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/call/call.c),
then follow
[`Dialer.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/dialer/Dialer.c)
and [`Cadence.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/callprog/Cadence.c).
The country table's cadence durations are in 10 ms units. The
[configuration reference](configuration.md)
explains their conversion into detector intervals; confusing these units with
milliseconds changes every timeout. Call progress operates on an 8000
samples/s path, with conversion where required by the host rate.

## 2. Negotiate a compatible datapump

[ITU-T V.8](references.md#protocol-standards) defines session-start procedures.
In this implementation, `v8_process` calls `V8Process`, maps timeout events
to host errors and publishes successful negotiation through
`V8UpdateModemParameters`. Its normal selection logic prefers the agreed
V.90 capability, then V.34, then V.32; quick connect has a separate path.
An idle timer governs handover, so repeated success events must not restart it.

Inspect [`v8.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/v8/v8.c)
for host-facing decisions, `V8Interface.c` for `V8Process` and `V8.c` for
the negotiation state machine. Distinguish
negotiation success from carrier training and from `DPSTAT_CONNECT`: they occur
at different boundaries. V.8 returns control information to the host rather
than directly substituting a new C++ modem object in its processing loop.

## 3. Construct and train the selected modem

The classic adapters copy or build family configuration, select originate or
answer behaviour and create their wrapper and modem engine. A built-in
configuration is not necessarily a working call configuration. Bell 103's
default `B103_CFG` selects loopback; its adapter supplies the station role.
The tone plan and station direction are separate concepts.

For V.PCM, `vpcm_create` fetches the host's runtime and DSP-information blocks,
rate limits, codec type and I/O delay. These values feed the V.34 object and
its PCM session. The host-owned runtime record is
[`_tagModemParameters`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/modem_params.h).
Check which rate-limit pair a consumer reads: host limits and the V.34 rate
indices derived elsewhere are distinct fields.

V.34 training lives chiefly in
[`V34hshak.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/V34hshak.c).
It coordinates three state words in `v34_object`: `microstate` at `+0x3592`,
`rxstate` at `+0x3594` and `txstate` at `+0x3596`. They describe cooperating
machines. The same numeric state value cannot be interpreted without its
machine. Transmit work occurs both per sample and per block; receive
transitions also use conditional chains. Consult
[`v34hshak.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/v34hshak.h)
and the `StateName` table when decoding traces.

Training establishes timing and carrier phase, learns the channel and echo
paths, exchanges capabilities and selects operating parameters. These
responsibilities have different owners and clocks. A state that has transmitted
its own sequence may still wait for the receive machine to recognise its peer.

### Latency is an input to the handshake

The I/O delay parameter is a host measurement in 9600 samples/s units.
The implementation adds four samples to obtain its hardware delay and derives
the internal filter-delay count from that adjusted value. V.34 turnaround
timers account for this pipeline latency. Above the supported delay range,
construction requests a host delay adjustment; it does not merely reject
the call.

Use the formulas, clamp behaviour and fixture limits in the
[configuration reference](configuration.md).
A value that permits a clean simulated connection is not a measurement of a
real echo path. Keep transport buffering, sample-rate conversion and the
channel model consistent when diagnosing repeated handshake states.

## 4. Exchange data and handle changes

The processing loop keeps running after connection. It consumes payload,
generates line samples, recovers receive bits and watches for loss of carrier,
retrain, rate renegotiation or hangup. `vpcm_run` translates the progress and
mode returned by `VPcmV34Progress` into host events. A `DPSTAT_CHANGEDP`
request requires the host to perform the corresponding transition.

For V.90/V.92, follow `VPcmV34Progress` in
[`VPcmV34Main.cpp`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v34/VPcmV34Main.cpp)
into `VPcmFloModem`, then the appropriate phase modulator or demodulator.
Training-to-data handover also transfers scrambler history and negotiated
parameters. A correct standalone bit recurrence does not prove that the
handover occurs at the right symbol.

For V.34, [`datapumpv34` supervises each block](signal-processing.md#per-block-supervision)
after the data engines run. Its error-run counters decide retrain and rate
changes. Preserve engine order, signed counter widths and reads after
initialisation/indication calls when changing that boundary.

## 5. Tear down and preserve session information

Use the owning adapter's destructor. It releases family resources and the
wrapper or session it created. V.PCM also copies learned information back into
the host's DSP-information block. A reset, retrain and full destruction have
different effects on histories and saved negotiation state; do not substitute
one for another to make a test pass.

[`t_vpcmrun.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/test/unit/t_vpcmrun.c)
is a useful lifecycle reading example: it compares mixed reference/reconstructed
V.34 endpoints against a reference/reference call, records per-block output,
and keeps payload observers separate for each endpoint. Its evidence applies
to the exercised delayed-wire fixture, not to every modem family or channel.
