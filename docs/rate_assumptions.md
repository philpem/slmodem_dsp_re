# Sample-rate assumptions

A running note of every place the code bakes in a sample rate, found while
reconstructing.

**Nothing here is acted on.** The current goal is a drop-in replacement that is
bit-identical to the Smart Link blob, so every one of these is reproduced
exactly as found. This file exists so that when the 8 kHz retarget comes, the
work is a list to walk rather than an archaeology exercise repeated from
scratch.

Each entry records what was found, whether it is *confirmed* as a rate
dependency or merely suspected, and what a retarget would have to do.

**Status key:** 🔴 confirmed rate dependency · 🟡 suspected, unconfirmed ·
🟢 checked and rate-independent

---

## 🔴 R-1 — Host rate is 9600, pumps are 8000; the wrapper bridges

`slmodemd/modem.h`: `MODEM_RATE 9600`, with the upstream comment `/* 8000 */`
already hinting at the intent. B103, V.22, V.23 and V.32 all pass `dp_srate =
8000` to `dp_wrapper_create` and are otherwise rate-agnostic; V.8 and
V.34/V.90/V.92 run at the host rate directly.

**Retarget:** set the host to 8000 and `dp_wrapper` short-circuits to a
pass-through — the equal-rate branch is already there and tested. V.34 and
above keep needing 9600. See findings 5 and 6.

## 🔴 R-2 — `d-modem.c` resamples 8000 → 9600 for a pump that wants 8000

`d-modem.c:189` pins the SIP wire to PCMU/PCMA 8000; `d-modem.c:216` declares
the pjmedia port at 9600. PJSIP upsamples, then `RcFixed` downsamples straight
back. Two conversion stages that cancel.

**Retarget:** with R-1 done, declare the pjmedia port at 8000 and both stages
disappear.

## 🔴 R-3 — `FixedRC` coefficient banks are designed per rate pair

18 polyphase banks, each a windowed-sinc designed for its own ratio at
`fs = up * f_in`, with the −6 dB cutoff at ≈0.94 of the binding Nyquist.

**Retarget:** the design rule is recovered (`docs/coefficients.md`), so new
ratios can be generated rather than resampled. A clean regeneration also
measures ~9.6 dB better in the stopband than the original. Not needed at all
if R-1 removes the conversion.

## 🔴 R-4 — Host-rate service layer carries `_8000`/`_9600` coefficient pairs

`MTD1..8_COEF_8000/_9600` (used by `DTMF_MTD_detect`) and
`AUTOCOR_COEF_9600/_7200` (used by `CID_FSD_demodulate`) already ship in both
flavours; `IIR2100_Coef_A/B_8000/_9600` likewise. These run at the host rate,
not the pump rate.

**Retarget:** select the 8000 variants. The pairs also serve as a free
acceptance test for any coefficient generator — derive the 8000 set from the
9600 set and check the bytes match.

## 🔴 R-5 — `dcr_create` initialises three phase intervals at 9600

`dcr_create` writes 5760, **9600** and 19200 into its 32-byte state. The DC
remover runs at the host rate.

**And they are DURATIONS, which is what the whole service being reconstructed
added.** Each is the sample count that ends one phase of the estimator, read by
exactly one phase and written by nobody but `dcr_create`, so at 9600 Hz they
are 0.6 s of settling, 1.0 s of initial evaluation, and a 2.0 s re-estimation
interval thereafter:

| field | value | at 9600 | phase it ends |
|---|--:|--:|---|
| +0x14 | 5760 | 0.6 s | SETTLE — discard the opening transient |
| +0x18 | 9600 | 1.0 s | EVALUATE — the "initial DC Evaluation" |
| +0x1c | 19200 | 2.0 s | TRACK — one pass of the 0.9/0.1 blend |

**Retarget:** scale all three by 8000/9600, i.e. 4800, 8000 and 16000, which
keeps the durations the module was tuned for. Nothing else in `dcr.c` mentions
a rate — the estimator is a mean and a leaky blend, and neither has a
frequency in it — so unlike the filter banks there is no coefficient set to
regenerate. These are fields rather than constants, so a host could also poke
them after `dcr_create` without touching the library. Findings 11 and 4200.

## 🔴 R-8 — Bell 103's FSK core is clocked at 7200 Hz

`FPM_FSM_CFG` carries `24` samples per symbol, which at 300 baud is 7200 Hz
(8000 would give 26.667). Independently, `FPM_FSM_init`'s 10/9 pre-scale
composed with `FPM_TONE_create`'s 32768/8000 yields phase increments for
exactly 7200. See finding 17.

So B103 converts twice: `dp_wrapper` brings the host 9600 down to 8000, and
B103 converts 8000 to 7200 internally. `RcFixed` modes 18 and 19 are 9:10 and
10:9, so the ratio is supported, but `dp_wrapper`'s table has no 7200 entry --
whatever B103 does internally does not go through the wrapper.

**Retarget:** moving the host to 8000 removes the *outer* stage only. The inner
8000 → 7200 has to stay, because 7200 is what makes 300 baud an integer number
of samples. Any attempt to run the FSK core at 8000 would need the symbol
timing redesigned, not just the filters regenerated.

## 🔴 R-9 — `FPM_TONE_create` bakes in 8 kHz

`(freq * 0x8312 + 0x1000) >> 13` is `freq * 32768/8000`: the tone generator
converts Hz to a Q15 phase increment assuming a 8000 Hz sample rate. Every
modulation uses it (14 callers), so this constant is load-bearing across the
whole library.

**Retarget:** callers running at another rate currently compensate by
pre-scaling the frequency (see R-8). A rate-agnostic version would take the
sample rate as a parameter instead, and the pre-scales would disappear.

## 🟢 R-6 — `FPM_FSM_init` scales tone frequencies by 1.11108 — **explained**

Superseded by R-8 and R-9: the 10/9 is the correction that retargets the
8000-assuming tone generator to the FSK core's 7200 Hz. Not a mystery constant
and not an independent dependency; it is a consequence of the other two.

<details><summary>original note</summary>

`FPM_FSM_init` multiplies its mark and space frequencies by `0x471c` in Q14
before passing them to the tone generator:

```
imul $0x471c, freq -> sar $14      freq * 18204 / 16384 = freq * 1.11108
```

1.11108 is 10/9 to five figures, and 8000/7200 = 1.1111. That *may* be a
7200 Hz → 8 kHz conversion baked into the modulator, which would be an
exception to "the pumps are rate-agnostic" (R-1).

**Unconfirmed.** It could equally be a generator-specific normalisation with no
rate meaning. Settle it from how `FPM_TONE_create` consumes the value before
concluding. Reproduced verbatim either way.
</details>

## 🔴 R-10 — `FPM_TONE`'s phase-reversal period is counted in 8-sample units

`FPM_TONE_generate` advances its reversal counter by `count >> 3` and compares
against `state[0x04]`, whose default is 450. That reaches threshold after 3600
samples, which is 450 ms **only at 8000 Hz** — the config carries the ITU-T
V.25 figure in milliseconds precisely because the scaling assumes 8 kHz.

**Retarget:** at another rate the divisor must change with it, or the reversal
period drifts. At 7200 Hz (where the B103 FSK core runs, R-8) the same config
would give 500 ms instead of 450. Finding 19.

## 🔴 R-11 — V.34's handshake counts in FOUR-SAMPLE TICKS, which are 2400-baud symbol periods only at 9600 Hz

The V.34 microstate machine steps once per four received samples, and four
samples at 9600 Hz **is** one symbol period at 2400 baud, V.34's reference
symbol rate. The object never distinguishes the two readings, because at the
only rate it runs at they are the same quantity — `vpcm_create` guards
`srate == 9600` exactly at 0x3a1c. Findings 1040–1045.

Three constants ride on that tick and they do **not** move together:

```
    filtdelay = ((hwDelay + 2) >> 2) + 34         V.34 object +0xaa7c
                 \______ samples -> ticks         \__ the pump's own
                                                      pipeline latency,
                                                      136 samples
    arm 47/55 leave when counter + 1 > 0x5f       i.e. at 96 ticks
                                                  = 384 samples = 40.000 ms
```

- **`>> 2` is the STEP SIZE, not a rate conversion.** It divides by the number
  of samples in a tick. Identical in form to the object's own
  samples-to-symbols idiom, whose 2400 arm is literally `>> 2`
  (`v34tx1_ppseg` 0x680ac).
- **`+ 34` is SAMPLE-relative.** 136 samples of internal filter/pipeline
  delay; unchanged in samples if the filters keep their tap counts, so the
  constant changes only because the tick does.
- **`0x5f` is TIME-relative and is the one that MUST change.** 96 ticks is the
  40 ± 1 ms tone phase-reversal turnaround of V.34 §11.2.1.1.3 and §11.2.1.2.5,
  measured *at the line terminals* — which is why the pipeline latency is
  preloaded into the counter in the first place. At 8000 Hz with a four-sample
  tick, 40 ms is **80** ticks, so `0x5f` becomes `0x4f`. Get this wrong and the
  reversal lands outside the Recommendation's ±1 ms. **There are three
  `0x5f` sites** — 0x6685d (arms 47/56), 0x65ba4 (arm 55) and 0x66027 (arm 58)
  — and only the first two are the turnaround; arm 58's is unidentified and
  must move with them anyway.
- `0x18c` in arm 49's round-trip estimate has the shape of §11.2.1.2.4's
  "minus 40 ms" in samples, but it is 396 where 40 ms is 384. The identification
  is **not** made: understand the twelve samples before rescaling it.

**Retarget:** decide the tick first — keeping four samples keeps `>> 2`
verbatim and forces `0x5f`; keeping the tick a 2400-baud symbol is impossible
at 8000 Hz, since 8000/2400 = 3.333. Either way this is a change to the
block-versus-symbol relationship, not a rescale of the delay constants.

**Latent, and to be reproduced rather than fixed:** `v34tx1_ppseg` at 0x68154
adds `filtdelay` (ticks) to a count of symbols at the *negotiated* baud. The
sum is dimensionally right only at 2400. Finding 1043.

## 🟡 R-7 — `dp_wrapper`'s rate table is a fixed list of six pairs

`dp_wrapper_create` dispatches on literal rate pairs among {8000, 9600, 48000}
rather than calling `RcFixed_Check_Combination`. Any other pair silently gets
no converter.

**Retarget:** harmless if R-1 makes the rates equal, but if a new rate is ever
introduced the table needs extending — it will not fail loudly, it will just
produce no conversion. Noted in `src/core/dp_wrapper.c`.

## 🔴 R-12 — `v32_create` rescales `MDMPRM_IODELAY` from 9600 to 8000 by ×5/6

`v32_create` is the only one of the four 8 kHz datapump constructors that reads
`MDMPRM_IODELAY` at all (`b103_create`, `v22_create` and `v23_create` never
call `modem_get_param`).  It computes `phys = IODELAY + 48`, pins it at 216,
and hands the V.32 core `phys * 5 / 6` — and 5/6 is exactly 8000/9600.  The
host's delay is in host samples; the echo canceller behind `dp_wrapper` wants
its own 8 kHz ones.  Finding 1197.

**Retarget:** if R-1 makes the host 8000, **three** constants move together,
not one.  The ×5/6 has to go to 1; the `+48` is one host fragment
(`dp_wrapper_create`'s `40 * 9600/8000`) and becomes 40; and the 216-sample
ceiling is in host samples (22.5 ms at 9600) and has to be restated as 180, or
the pump silently pins a delay it should have accepted.  Changing the factor
alone leaves the offset a rate dependency in disguise.
