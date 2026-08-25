# Datapump configuration

How each datapump is told what to be. One section per pump, added as they are
reconstructed.

Every field meaning here was established by **sweeping the configuration and
observing the resulting object**, not by reading the create function. That
distinction earned its place: the one field meaning derived by reading
`B103FP_create` rather than measuring it was wrong, and stayed wrong through
two findings before a sweep caught it (finding F35).

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
samples — 20 ms at the fixed 8000 Hz call-progress rate (finding F41) — and
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
bandpass, the tones or the oscillator, which is the error finding F32 made.

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
against where each value lands in the constructed object. Findings F820–825.

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
> `+0x34`, which is what `vpcm: VPCM rate limits: %d-%d` prints. The pair
> `V90Parameters::setToDefault` divides by 2400 to get a rate index is `+0x38`
> / `+0x3c`, and `vpcm_create` writes those as the **literals 4800 and 33600**
> whatever the host asked for.
>
> **This used to say `+0x30`/`+0x34` are printed and "nothing else reads"
> them, and that was wrong** — finding F1020. `VPcmV34InitiateRetrain` reads
> that pair at three sites (0x66b5/0x66c5, 0x6870/0x6877, 0x6acc/0x6acf) and
> divides *it* by 2400 for the V.34 rate indices, so 300 and 56000 become 0
> and 14 after the clamp. Index 14 is the 33,600 a V.34 call converges to.

So an AT+MS that narrows the modem's rate window does not narrow V.PCM's.

### What the configuration actually changes, measured

Every parameter above was swept and a whole 1,600-block V.34 call re-run
against it (finding F824). **Five of the six are inert**: the rate window, the
codec type and all four `dsp_info` words leave the call's trajectory identical
to the last count, while the assertions that read them off the constructed
object do fire — so the sweep is live and the call genuinely does not care.

The exception is `MDMPRM_IODELAY`. HW delay is `IODELAY + 4` and DMA delay is
`HW − 48` plus a correction at root `+0xd254`; the 48 is slmodemd's own
`ST7554_HW_IODELAY`. **The formula is recovered; the input is a host
measurement and differs per driver**, and the driver that answers 0 is the one
that is a stub:

| driver | answers | HW | note |
|---|--:|--:|---|
| socket | 0 | 4 | the real expression is commented out beside it (`modem_main.c:682`) |
| ALSA | 424 | 244 | 384 startup samples + `INTERNAL_DELAY` 40; over the cap, see below |
| modemap | ~192 + kernel | 196+ | `modemap_start` writes 192 samples |

It is the **HW** delay that matters and not the DMA correction: two settings
that pin HW to the same 244 with DMA differing by 760 give identical results
to the last count — on a wire with no echo, which is the caveat the next
section keeps.

The tests use 0, deliberately and not by default. The I/O delay and the
simulated wire are the same physical quantity modelled twice, so they move
together or not at all — see the note on `CFG_IODELAY` in
`test/unit/t_v34conn.c`. Neither setting connects.


## Choosing `MDMPRM_IODELAY` for a transport the original never had

`MDMPRM_IODELAY` is the one configuration input that cannot be recovered from
the object, because it is a **host measurement**: `slmodemd` answers it from
`m->driver.ioctl(m, MDMCTL_IODELAY, 0)` and each driver measures its own path.
It is also the difference between a V.34 call connecting and not. A SIP/RTP
backhaul inherits `slmodemd`'s **socket driver, which is a stub returning 0**
(`modem_main.c:682-686`), and 0 does not connect.

Everything in this section is marked **DERIVED** — read off the object or off
`slmodemd` — or **JUDGEMENT**, which is engineering opinion about a transport
neither ever saw. Findings F1020-1026.

### The unit is SAMPLES at 9,600 Hz — DERIVED, twice

Not milliseconds and not blocks. Two independent uses agree:

- The object divides it by four. `filtdelay = ((hwDelay + 2) >> 2) + 0x22`
  produces a count of `datapumpv34` invocations, and an invocation is four
  samples, so the input is samples. `srate` is guarded as exactly 9600
  (`vpcm_create` 0x3a1c), so the rate is not a variable.
- `slmodemd` writes it as 16-bit frames. `MDMPRM_UPDATE_DELAY` — the parameter
  the pump uses to hand delay back — does `memset(outbuf, 0, n * 2)` and
  `device_write(dev, outbuf, n)` into the same `dev->delay` that
  `MDMCTL_IODELAY` returns (`modem_main.c:987-998`, `:541`).

So 216 and 232 are **22.5 ms and 24.2 ms**, a sound card plus kernel; and the
object's ceiling of 240 is **25 ms**.

### The formula, exactly — DERIVED

```
    hwDelay   = MDMPRM_IODELAY + 4                        runtime +0x64
    dmaDelay  = hwDelay - 48 + extradelay                 runtime +0x68
    filtdelay = ((hwDelay + 2) >> 2) + 0x22               V.34 obj +0xaa7c
              = ((MDMPRM_IODELAY + 6) >> 2) + 34
```

`>> 2` is arithmetic (`sar`). Three sites compute it — `VPcmV34Create` 0xaf19,
`VPcmV34InitiateRetrain` 0x674a and `VPcmV34SetDelays` 0x6405, a function the
object names itself — and all three read the delays out of the *same*
`_tagModemParameters` block the host supplied, which reaches the V.34 object
as its `pac3c` (finding F1020).

> **Do not use `35 + iodelay/4`.** It appears in findings F960 and F962 and in
> two test assertions, it was fitted to a sweep, and it is **one too small
> whenever `IODELAY mod 4` is 2 or 3** — finding F1021. The tests that assert it
> run at 216, where the two agree.

The same two fields also set the echo canceller, which is why `dmaDelay` is
not merely decorative:

```
    V.34 obj +0x25c  =  0x610 - dmaDelay
    V92EchoCanceller::setEchoDelay(dmaDelay + 0x68)
```

### What `filtdelay` IS — DERIVED

It is a total **pipeline latency**, in units of one microstate step, and a
microstate step is four samples at 9600 Hz (finding F1040):

```
    filtdelay  =  the HOST's I/O latency in steps  +  34 steps
                                                      \_ the pump's OWN
                                                         internal latency,
                                                         136 samples
```

V.34 §11.2.1.1.3 and §11.2.1.2.5 require the tone phase-reversal turnaround to
be 40 ± 1 ms **measured at the line terminals**. The state machine sees an
incoming reversal one pipeline-latency late and its own reversal appears one
pipeline-latency after it emits it, so it preloads its counter with
`filtdelay` and counts to a fixed **96 steps = 384 samples = 40.000 ms**.
Findings F1041 and F1042; `0x5f` is that 96 minus one, because the compare is on
`counter + 1`.

### The working range — DERIVED, measured to the sample

A V.34 answerer entering microstate 47 `TX_PHASE2_ANS` must count from
`filtdelay` up past `0x5f` before its receiver declares all-ones on the line
the caller has correctly gone silent on. **The wait is `96 - filtdelay` steps,
so a LARGER I/O delay is a SHORTER wait** — a larger I/O delay is already part
of the 40 ms. That is the whole mechanism, and it is why the knob works in the
direction it does (findings F960, F1022, F1041).

> Findings F960 and F1022 say the wait is `0x5f - filtdelay`, which is the same
> thing counted the other way: the compare is `n = counter + 1;
> if (n <= 0x5f) stay`, so the state is left on the step at which `n` would be
> 96 and `counter` is never seen holding 96. Whether that exiting step is
> "sat out" is a convention. What is *not* a convention is the total from the
> reference event, which is 96 steps whatever `filtdelay` is — finding F1041.

| `MDMPRM_IODELAY` | `filtdelay` | V.34 |
|---|--:|---|
| 0 .. 85 | 35 .. 56 | **does not connect** — "Repeated info0 is detected", for ever |
| 86 .. 240 | 57 .. 95 | connects, 33,600 each way, BER 0 |
| 241 and up | 95 | connects; see the negotiation below |

**85 fails and 86 connects**, measured one value at a time; the old "somewhere
between 88 and 80" was a sweep in steps of eight. The connect column is
measured throughout. The `filtdelay` column is the formula above — below 86 it
cannot be read off a run, because the harness samples the field only at the
moment it connects.

### Above 240 the pump negotiates rather than failing — DERIVED

`vpcm_create` guards `IODELAY + 4 <= 0xf4`, but the failing branch is not an
error path (finding F1024 corrects finding F962 on this):

```
    modem_set_param(modem, MDMPRM_UPDATE_DELAY, 244 - (IODELAY + 4))  ; negative
    extradelay = max(-(that), 384)                    ; root +0xd254
    hwDelay    = 244                                  ; pinned, filtdelay 95
    dmaDelay   = 244 - 48 + extradelay
```

The host is asked to **shed** the excess — `slmodemd` does it by discarding
that many input samples at `modem_main.c:957-968`. This is the only path on
which `dmaDelay != hwDelay - 48`.

Measured where the two readings disagree: at `IODELAY` 400 the object's
`filtdelay` reads **95** (the pinned `hwDelay` of 244), not the 135 the
unpinned formula gives, and the call still connects. So ALSA's long-buffer 424
is not refused either — it runs at the pinned maximum.

### So what should a SIP/RTP host answer? — JUDGEMENT

**Set 240.** Not a computed number.

20 ms of RTP is **192 samples** in this field's units, and the resemblance to
`modemap_start`'s 192 is real rather than numerology — both are 20 ms at the
datapump's rate. But packetisation is one term of three: a real RTP path adds
a jitter buffer (conventionally two to three packets) and 8 kHz↔9.6 kHz
resampling. Sixty milliseconds one way is 576 samples and a round trip is over
1,100 — **more than four times the largest value the field accepts**. The field
cannot express a SIP round trip, so choosing it is not a measurement problem
and 192 would be a floor mistaken for an estimate.

Given that, 240 is the choice that maximises the only margin `MDMPRM_IODELAY`
demonstrably buys, and it stays inside the acceptance window so the
negotiation above never fires at construction — a new host is not required to
implement `MDMPRM_UPDATE_DELAY` correctly before its first call can connect.
Reporting the transport's true latency is *worse*: it trips the over-cap path,
which asks the host to throw away several hundred buffered samples, and ends
at the same pinned `hwDelay` of 244 that 240 gives anyway.

**216 is the conservative alternative** — a real driver's measurement, and the
only value with an end-to-end proof in this tree (`t_v34link`, finding F963).
Prefer it if a tested constant matters more than margin.

**The cost of a high value is not quite zero.** Every value in 86..240
connects identically here, so this is not a fine-tuning knob — but the
measurement that established `dmaDelay` is inert ran on a **noiseless wire
with no echo path**, and `dmaDelay`'s only consumers are the echo canceller's
delay and `+0x25c`. A harness with no echo cannot observe an echo canceller
pointed at the wrong lag. "Inert for the handshake trajectory on a clean wire"
is derived; "free on a real line" is not. On an RTP path there is no analogue
hybrid to cancel, which is why the risk is judged acceptable rather than
measured away.


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

`b103fp.flags` is settled — finding F38. Only `0x01` is ever tested, and `0x02`
is consumed as a one-shot without being tested. **The other six are written
and never read**, by anything: `b103_process` masks the return with `0xff`, so
the flags byte does not leave the library. They are reproduced but left as
literals, because a name asserts a meaning and there is none to recover.

`struct dp`'s `status` turned out not to be a bit set at all — it holds a
`DPSTAT_*` scalar. Re-check for others when V.22 and V.32 are reached.
