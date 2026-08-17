# Abbreviations in `dsplibs.o`

Smart Link's naming is terse and mostly undocumented. This records what each
prefix means, **with the evidence**, because a wrong expansion is not harmless:
reading `MRF` as "matched root filter" is what hid the 8000↔7200 rate converter
for several passes (finding 24).

**Confidence:** ✅ confirmed by behaviour · 🟢 strongly implied by position or
naming · 🟡 plausible, unverified

---

## The two that organise everything

### ✅ `FP` — Fixed Point

Every classic datapump has an `FP`-suffixed implementation:

```
B103FP_create / _delete / _modem
V22FP_create  / _delete / _modem
V32FP_create  / _delete / _modem / _recreate
v23FP_rx_create / v23FP_tx_create
```

and `FP_math.c` holds `GetFP_Value` (a Q14 divide) and `FP_Pow` (a Q14
exponential). All fixed point, no floating point anywhere near them.

### ✅ `FPM` — Fixed Point Modem

The shared library of fixed-point building blocks the `*FP` datapumps are
assembled from. Not "Fixed Point Math": the family includes `FPM_AGC`,
`FPM_FSD`, `FPM_MRF`, `FPM_ECC` and a dozen other DSP stages alongside the
arithmetic helpers. The maths is a subset, not the whole.

Contrast the float side, which uses no prefix at all: `FloatFIR`, `FloatIIR`,
`FloatARMA`, `GenericIIR`, and the `V90*`/`V92*` C++ classes.

---

## DSP stages

| | expansion | evidence |
|---|---|---|
| ✅ `MRF` | **Multi-Rate Filter** | `FPM_MRF_init` takes up/down factors and computes taps-per-phase; Bell 103 runs 10:9 and 3:10 (finding 24). *Not* "matched root filter". |
| ✅ `FSM` | **Frequency Shift Modulator** | `FPM_FSM_modulate` switches a tone generator between two frequencies per bit. |
| ✅ `FSD` | **Frequency Shift Demodulator** | the counterpart; delay-line discriminator plus lowpass (finding 27). |
| ✅ `MTD` | **Multi-Tone Detector** | a bank of resonators, one per tone, with an energy-ratio verdict (finding 28). |
| ✅ `ECC` | **Echo CanCeller** | the entry point is `FPM_ECC_cancel`. Not "error correcting code" — this is the DSP layer. |
| ✅ `SDM` | **Scrambler/Descrambler** | exports exactly `FPM_SDM_scrambler` and `FPM_SDM_descrambler`. |
| ✅ `VTB` | **Viterbi** | `VTB_decoder`, `VTB_DIFF_TBL`, `TrellisTransitionTable`, `VTB_BOUND_7200/9600/12000/14400` — the trellis decoder for V.32bis/V.17 rates. |
| ✅ `AGC` | Automatic Gain Control | standard; `FPM_AGC_Freeze`/`_Release` behave as expected. |
| 🟢 `FSE` | **Fractionally Spaced Equalizer** | standard term, and its position in the V.22 chain fits (below). |
| 🟢 `PPS` | **Phase Splitter** | sits between the input filter and the equalizer, the slot a Hilbert pair occupies in a passband receiver. |
| 🟢 `SRE` | **Symbol Recovery** (timing) | `FPM_SRE_recover` is the only entry point and it uses `FPM_atan` and `FPM_rms` — a phase-error estimate, i.e. timing recovery. |
| 🟢 `ADEQ` | Adaptive Equalizer | `fpm_adeq.c`; no distinct exports found yet. |
| 🟡 `SMC` | Symbol Mapping / Coder | `FPM_SMC_encoder`, and `SMCv22_IMAP_1200BPS` / `_2400BPS` map bits to constellation points. |
| 🟡 `DFTC` | Discrete Fourier Transform, Coarse? | `DFTC.c` sits with the V.8 tone detection. |

**The V.22 receive chain reads as a textbook passband receiver**, which is what
pins `PPS`, `FSE` and `SRE`:

```
V22_MRF_init   resample
V22_PPS_init   phase split to I/Q
V22_FSE_init   equalize
V22_SRE_init   recover symbol timing
```

---

## Smaller ones

| | expansion | evidence |
|---|---|---|
| ✅ `RC` | Rate Converter | `RcFixed_*` — fixed rational rate conversion. |
| ✅ `DCR` | DC Remover | `dcr_create/process`; the state holds running means. |
| ✅ `Hdx` | Half Duplex | `TxHdxStartB103`, `RxHdxDataB103` — the Bell 103 half-duplex state machines. |
| ✅ `CFG` | config | pervasive: `FPM_TONE_CFG`, `AGCb103_CFG`, … |
| 🟢 `CP` | Call Progress | `CP_350_600_a/b` are call-progress bandpasses named by passband. |
| 🟢 `MTK` | ? | `MTK_phasor` exists alongside `FPM_phasor`; possibly a different fixed-point flavour. |
| 🟡 `BwCh` | Bandwidth / Channel | `BwChDem_Create/Progress` — a channel bandwidth detector. |
| 🟡 `VMI` | Voice/Modem Interface | `faxvmi*.c`, `Vmi_v17.c` — the fax modulation interface layer. |

---

## Why this matters

Two concrete costs already paid for guessing an expansion:

- **`MRF`** read as "matched root filter" put the rate converter out of scope
  for several passes, and led to a written conclusion that Bell 103 might run
  10/9 fast with no conversion at all (finding 23, since retracted).
- **`FP_Pow`** reads as a general power function. It is `exp()`. Anything built
  against the name rather than the coefficients would be wrong.

The rule that follows: treat an abbreviation as a hypothesis until the code
confirms it, and record which it is.

**pre-emphasis, and how it is spelled here.** The object spells it **`preemp`**,
one 'h' short of the English word, and so does this tree: `preemp_index`,
`preemp0`, `m->preemp`, `preempindex` (the object's own name for the selector),
`tx_preemp`. Our own additions were briefly spelled `preemph` and have been
renamed to match; nothing in `src/`, `include/` or `tools/` uses the long form
any more.

**Grep for `preemp`, never `preemph`.** Searching the long form finds nothing
and invites the conclusion that the subsystem is absent -- which is exactly what
happened in finding 1959, where four searches missed the entire transmit-side
implementation and a "we never comply with the far end's request" finding was
one step from being written. Findings numbered 1956-1959 still contain the old
spelling because the record is append-only and is not rewritten.

Two related names that are NOT this: `ec_prem_coef_*` is the echo canceller's
pre-filter, and `V34PREEMPHASIS` is the object's debug string prefix, which
keeps the long spelling because the differential tier compares transcripts
character for character.
