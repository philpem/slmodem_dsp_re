# Architecture and ownership

The host owns the modem session and supplies samples, payload bits,
configuration and memory services. The library registers datapumps with the
host. Each datapump implements a processing step and reports events such as
connection, busy tone or a request to change datapump.

## The host boundary

[`dp.h`](https://github.com/philpem/slmodem_dsp_re/blob/master/include/dsplib/dp.h)
defines `struct dp` and `struct dp_operations`. A `dp` carries an identifier,
host pointer, status, operations table and private `dp_data`. The table
provides create, destroy, process and hangup entries. A process call receives
input and output buffers plus a sample count; its integer result is a
`DPSTAT_*` event, not a count of samples produced.

[`dp_init.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/dp_init.c)
implements `prop_dp_init` and `prop_dp_exit`. Both visit the registration
families in this order: call progress, Bell 103, V.22, V.23, V.32, V.8 and
V.PCM. Registration is library setup, separate from constructing a particular
call. Service interfaces such as Caller ID have their own entry points.

The host supplies `modem_get_bits` and `modem_put_bits` for the data pipe and
`modem_get_param`/`modem_set_param` for control. Allocation and other platform
operations pass through the `sysdep_*` interface. The surviving host contract
is available under
[`ref/slmodemd/`](https://github.com/philpem/slmodem_dsp_re/tree/master/ref/slmodemd).
Trace both the call site and host declaration when establishing units or
buffer ownership; a locally plausible prototype is insufficient ABI evidence.

## Two sample-processing paths

```text
Host samples and host payload callbacks
  |
  +-- Bell 103 / V.22 / V.23 / V.32
  |     dp_operations.process = dp_wrapper_run
  |       -> host-fragment rings -> fixed rate conversion
  |       -> family process callback -> fixed-point modem
  |       -> reverse conversion -> host output ring
  |
  +-- V.8
  |     v8_process -> V8Process -> negotiated host parameters / change request
  |
  +-- V.PCM (V.34 / V.90 / V.92)
        vpcm_run -> sample queues and byte/int bit conversion
          -> VPcmV34Progress -> V.34 or PCM session processing
          -> sample conversion and host payload callbacks
```

The diagram shows processing responsibility, not a promise that every mode
runs at once. The host selects and changes datapumps as the call progresses.

### Classic datapumps

[`dp_wrapper.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/core/dp_wrapper.c)
adapts host fragments to the selected modem's native fragments. Each direction
has a ring two host fragments deep. The wrapper accumulates a complete input
fragment before invoking the modem, and starts its output one silent fragment
ahead. This initial silence is part of observable latency.

For the classic families the native datapump interface is 8000 samples/s.
The wrapper explicitly offers conversions between 8000, 9600 and 48000
samples/s through `RcFixed`. Equal rates bypass conversion. Do not treat this
as a general arbitrary-rate API: the conversion list is fixed, and
unsupported unequal rates are refused before reverse-converter dispatch (see
[debt MG-04](reconstruction-debt.md#mg-04-wrapper-preconditions-need-an-explicit-boundary)).

There are two private-state hops: `dp->dp_data` points to the wrapper and
`wrapper->dp_data` points to the family object. The family constructor creates
and wires these objects; `dp_wrapper_delete` releases the converters and
wrapper, while the family destructor is responsible for family resources.
See [`v32.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v32/v32.c)
for a complete example. Its 40-sample native fragment is 5 ms, corresponding
to twelve symbols at 2400 symbols/s.

### V.PCM

[`vpcm.c`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/vpcm.c)
provides a different adapter. `vpcm_create` requires 9600 samples/s and admits
`max_frag` up to 48. `vpcm_run` groups pending input into multiples of four
samples, converts samples to floating point, transfers bits, and calls
`VPcmV34Progress`. Output samples are converted back to `short`.

Transmit bits arrive as bytes and are widened backwards into an integer
array. Receive bits are narrowed forwards into bytes before returning them
to the host. These directions preserve unread data during in-place conversion.
They are observable storage behaviour, so a cleanup must preserve their order.

## Source map

| Area | Responsibility | Start tracing here |
|---|---|---|
| `src/core/` | Registration, runtime parameters, fragment/rate adaptation, shared queues | `dp_init.c`, `dp_wrapper.c`, `dp_param.c` |
| `src/call/`, `src/dialer/`, `src/callprog/` | Call setup, dial generation, tone and cadence detection | `call.c`, `Dialer.c`, `Callprog.c`, `Cadence.c` |
| `src/v8/` | Initial capability negotiation and handover | `v8.c`, `V8Interface.c`, `V8.c` |
| `src/pump/b103/`, `v22/`, `v23/`, `v32/` | Classic data-mode adapters and modem engines | Family `*.c` adapter, then `*FP*` and state-machine files |
| `src/pump/v34/` | V.34 handshake, timing, shell mapping, receive processing and PCM integration | `V34hshak.c`, `V34RX.c`, `VPcmV34Main.cpp` |
| `src/pump/v90/` | V.PCM adapter, V.90/V.92 modem objects and training/data components | `vpcm.c`, `VpcmFloModem.cpp`, `V90Modem.cpp`, `V92Modem.cpp` |
| `src/dsp/` | Shared fixed-point and floating-point filters, oscillators, gain control and coding | `fpm_*.c`, `FloatFIR.cpp`, `FloatIIR.cpp`, `psd.cpp` |
| `src/service/`, `src/voice/` | Caller ID, ring, DTMF, PCM conversion and voice control | `cid.c`, `ringDetector.c`, `Dtmf.c`, `voice.c` |
| `src/fax/` | Class 1 control, HDLC and V.21/V.27ter/V.29/V.17 fax modems | `class1*.c`, `Vmi_v*.c`, `faxvmi_hdlc.c` |

The PCM integration filename is
[`VpcmFloModem.cpp`](https://github.com/philpem/slmodem_dsp_re/blob/master/src/pump/v90/VpcmFloModem.cpp);
its class is `VPcmFloModem`. Case and spelling matter on this filesystem.
Headers under `include/dsplib/` describe recovered layouts and contracts.
File/span labels recovered from the object do not reliably delimit a protocol
module; use call relationships and `tools/service.py` for service ownership.
