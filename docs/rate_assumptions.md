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

## 🔴 R-5 — `dcr_create` initialises a field to 9600

`dcr_create` writes 5760, **9600** and 19200 into its 32-byte state. The DC
remover runs at the host rate.

**Retarget:** these need re-deriving for 8000. Finding 11.

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

## 🟡 R-7 — `dp_wrapper`'s rate table is a fixed list of six pairs

`dp_wrapper_create` dispatches on literal rate pairs among {8000, 9600, 48000}
rather than calling `RcFixed_Check_Combination`. Any other pair silently gets
no converter.

**Retarget:** harmless if R-1 makes the rates equal, but if a new rate is ever
introduced the table needs extending — it will not fail loudly, it will just
produce no conversion. Noted in `src/core/dp_wrapper.c`.
