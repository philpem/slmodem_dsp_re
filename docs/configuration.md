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
