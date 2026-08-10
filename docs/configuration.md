# Datapump configuration

How each datapump is told what to be. One section per pump, added as they are
reconstructed.

Every field meaning here was established by **sweeping the configuration and
observing the resulting object**, not by reading the create function. That
distinction earned its place: the one field meaning derived by reading
`B103FP_create` rather than measuring it was wrong, and stayed wrong through
two findings before a sweep caught it (finding 35).

The reconstructed configurations live beside their datapumps —
`src/pump/b103/b103_cfg.c` — and the header declares the struct with the field
meanings on it. This document is the overview; the header is the reference.

---

## Call progress — the country table's units

The call-progress modules are configured almost entirely from the host, via
`modem_get_param`, rather than from a configuration block. `docs/parameters.md`
has the index table and the full derivation; the one thing worth repeating
here, because getting it wrong silently mis-times every tone:

> **The cadence times in the country table are in units of 10 milliseconds.**
> A 500 ms busy tone is `50`. `cadence_create` converts each window to a count
> of `toneiir` intervals with `time * 80 / GetCallProgressSamplesBufferLength`,
> and that expression only means "intervals" if the input is centiseconds.

The interval itself is per tone. Busy, congestion and ringback hard-code 160
samples — 20 ms at the fixed 8000 Hz call-progress rate (finding 41) — and
only dial tone takes it from `GetCallProgressSamplesBufferLength`, defaulting
to 666 when the table says zero, which is 83.25 ms.

So a 500 ms busy tone is 25 intervals and the cadence resolution is 20 ms,
while dial tone is measured four times more coarsely. `GetBusyToneDiffTime`,
the matching tolerance, is clamped to a minimum of 3 intervals — 60 ms for the
cadenced tones.

---

## Bell 103 / V.21 — `struct b103_cfg`

28 bytes, copied wholesale into the first 28 bytes of the `b103fp` object by
`B103FP_create`. Original at `.data:0x77cc`.

| offset | field | built-in | meaning |
|---|---|--:|---|
| `+0x00` | `call_type` | **2** | `B103_CALL_*`. The only field that changes the object's shape. |
| `+0x04` | `v21` | 0 | Selects the **V.21 tone plan** instead of Bell 103's. Not a direction flag; see below. |
| `+0x08` | `loop_high_channel` | 0 | **Loopback only**: non-zero transmits 2025/2225. Ignored otherwise. |
| `+0x0c` | `tone_timeout_ticks` | 14000 | `hdx->tone_timeout = max(this / 20, 700)` blocks. |
| `+0x10` | `f10` | 1 | Gates a branch in `B103FP_create`. No effect on any field observed so far, but **not inert**. |
| `+0x14` | `f14` | 0 | No observed effect. |
| `+0x18` | `tx_scale` | 3200 | Modulator output gain, straight into `fsm.scale`. |

### `call_type` decides everything

| value | | `hdx->mode` | bandpass | tone detector | transmits | local oscillator |
|--:|---|--:|---|---|---|--:|
| 0 | `B103_CALL_ORIGINATE` | 1 | `B103_BPF_CALLER`, 40 taps | yes | 1070 / 1270 | 1350.1 Hz |
| 1 | `B103_CALL_ANSWER` | 2 | `B103_BPF_ANSWER`, 50 taps | yes | 2025 / 2225 | 395.0 Hz |
| else | loopback | 0 | none | no | 1070 / 1270 | 1350.1 Hz |

There is **no range check** — `B103FP_create` falls through to the loopback arm
for any other value, including negative ones.

### The built-in configuration is loopback, and does not link

`B103_CFG` sets `call_type` to 2. An object built straight from it has no
channel bandpass and no tone detector, so it cannot complete a call: the
measured bit error rate for such a station talking to itself is **0.485**.
`b103_create` is expected to build its own copy with `call_type` set from the
caller/answer argument it is handed.

This is worth stating plainly because it is the single most misleading thing
about the Bell 103 configuration, and it cost this project two findings.

### The two oscillators are the frequency plan

Each side mixes the pair it *receives* down to the same baseband:

| side | receives | LO | at baseband |
|---|---|--:|---|
| originate | 2025 / 2225 | 1350 | 675 / 875 |
| answer | 1070 / 1270 | 395 | 675 / 875 |

675 and 875 straddle the demodulator's 775 Hz discriminator null, so **one
demodulator design, one filter set and one slicer serve both directions** —
only the oscillator differs. Set them wrong and nothing else in the receiver
reveals it; the symptom is a bit error rate near 0.5.

### `is_answer` is not `call_type`

`+0x04` is read by exactly one function, `TxHdxMarksB103`, and only to decide
when to stop transmitting mark:

- **answering** station: stop after `tx_blocks` alone.
- **calling** station: stop after `tx_blocks` *and* once its own receiver has
  acquired.

So the caller holds mark until it hears the answerer. Setting `+0x04` without
also setting `call_type` changes nothing else — it does **not** select the
bandpass, the tones or the oscillator, which is the error finding 32 made.

### Enforced, not just written down

`test/unit/t_b103link.c` asserts the reconstructed `B103_CFG_data` matches the
blob byte for byte, and asserts each documented effect above against a freshly
built object. If a future edit changes what a field does, the build fails
rather than this page going quietly stale.


---

## V.PCM (V.34 / V.90 / V.92 / K56Flex) — there is no configuration block

The one datapump that is **not** configured from a struct in `.data`. There is
no `VPCM_CFG`. `vpcm_create` asks the host six questions through
`modem_get_param`, and everything else it needs it writes as a literal.

That matters more than it sounds, because the host is `slmodemd` and
**slmodemd's source survives** — `modem_get_param` is undefined in the blob and
defined in `slmodemd/modem_param.c`. So this configuration is not inferred from
what makes the object behave; it is read off the caller, and then checked
against where each value lands in the constructed object. Findings 820–825.

| index | parameter | slmodemd answers | value | lands at |
|--:|---|---|--:|---|
| 10 | `MDMPRM_DPRUNTIME` | `m->dp_runtime` | a 136-byte block | root `+0x28` |
| 11 | `MDMPRM_DSPINFO` | `&m->dsp_info` | 16 bytes | root `+0x24` |
| 3 | `MDMPRM_MIN_RATE` | `m->min_rate` = `MODEM_MIN_RATE` | 300 | runtime `+0x30` |
| 4 | `MDMPRM_MAX_RATE` | `m->max_rate` = `MODEM_MAX_RATE` | 56000 | runtime `+0x34` |
| 5 | `MDMPRM_IODELAY` | `m->driver.ioctl(m, MDMCTL_IODELAY, 0)` | the driver's | runtime `+0x64`/`+0x68` |
| 6 | `MDMPRM_CODECTYPE` | the same ioctl | the driver's | runtime `+0x54` |

and the two arguments the constructor *guards* rather than fetches:

| argument | guard | slmodemd passes |
|---|---|--:|
| `srate` | `cmp $0x2580` — must be exactly 9600 | `m->srate` = `MODEM_RATE` = 9600 |
| `max_frag` | `cmpl $0x30` / `jg` — must be ≤ 48 | `m->frag` = `MODEM_FRAG` = `MODEM_RATE/200` = 48 |

> **48 is the value, not merely a value the guard admits.** It is exactly the
> shape of a number that looks derived and is not, and it happened to be right.

### The two addresses are typed, and the type is the host's

`MDMPRM_DPRUNTIME` is a **`struct _tagModemParameters`** — the type the
mangling of `VPcmFloModem`'s constructor gives it — allocated and initialised
by `dp_runtime_create` (`src/core/dp_param.c`, blob 0x58e0), which is a
function *in this object that only the host calls*. That is why it is in no
datapump's closure and why nobody had looked at it. It is 136 bytes, and
`include/dsplib/modem_params.h` is the field map.

`MDMPRM_DSPINFO` is a **`struct dsp_info`**, four words, declared in the same
header. Two of them — `connection_type` and `clock_deviation` — go into the
runtime block at construction and come back out in `vpcm_delete`, so what a
call learns about the line survives the datapump being rebuilt.

### The rate window is two different pairs and they are easy to confuse

> The host's `MDMPRM_MIN_RATE` / `MDMPRM_MAX_RATE` land at runtime `+0x30` and
> `+0x34`, which is what `vpcm: VPCM rate limits: %d-%d` prints and **nothing
> else reads**. The pair `V90Parameters::setToDefault` divides by 2400 to get a
> rate index is `+0x38` / `+0x3c`, and `vpcm_create` writes those as the
> **literals 4800 and 33600** whatever the host asked for.

So an AT+MS that narrows the modem's rate window does not narrow V.PCM's.

### What the configuration actually changes, measured

Every parameter above was swept and a whole 1,600-block V.34 call re-run
against it (finding 824). **Five of the six are inert**: the rate window, the
codec type and all four `dsp_info` words leave the call's trajectory identical
to the last count, while the assertions that read them off the constructed
object do fire — so the sweep is live and the call genuinely does not care.

The exception is `MDMPRM_IODELAY`. HW delay is `IODELAY + 4` and DMA delay is
`HW − 48` plus a correction at root `+0xd254`; the 48 is slmodemd's own
`ST7554_HW_IODELAY`. Between HW 44 and HW 244 the handshake's trajectory
changes completely. **The formula is recovered and the input is not
recoverable** — it is `m->driver.ioctl(...)`, a property of the sound card: 0
for slmodemd's socket driver, `dev->delay` for ALSA. 0 is what the tests use,
and it was deliberately not tuned.


---

## Bit-set fields

Several fields are used as bit sets rather than scalars. They are documented
where they live, but collected here because they are the one category in this
reconstruction where a *plausible* name is worse than no name: knowing where a
bit is written says nothing about what the layer above does with it, and a
wrong guess propagates into every caller.

The rule applied throughout: **name a bit only once something is found that
reads it**; otherwise document the set and clear sites and leave the literal.

### `struct b103fp`'s `flags` (+0x1d)

Full table with set/clear sites in `include/dsplib/b103fp.h`. Three of eight
bits are named; the other five have known write sites and no known reader.

`B103FP_modem` returns the whole 32-bit word at `+0x1c` — status in byte 0,
these flags in byte 1 — so the consumer is `b103_process`, which is not
reconstructed yet. Decoding it should settle the remaining five.

One is worth knowing now regardless: **`0x02` is a one-shot**. Every timeout
path sets it, and `B103FP_modem` clears it at the top of every call, so a
caller that does not read it each block loses the event.

### Resolved

`b103fp.flags` is settled — finding 38. Only `0x01` is ever tested, and `0x02`
is consumed as a one-shot without being tested. **The other six are written
and never read**, by anything: `b103_process` masks the return with `0xff`, so
the flags byte does not leave the library. They are reproduced but left as
literals, because a name asserts a meaning and there is none to recover.

`struct dp`'s `status` turned out not to be a bit set at all — it holds a
`DPSTAT_*` scalar. Re-check for others when V.22 and V.32 are reached.
