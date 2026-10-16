# Abbreviations in `dsplibs.o`

Smart Link's naming is terse and mostly undocumented. This records what each
prefix means, **with the evidence**, because a wrong expansion is not harmless:
reading `MRF` as "matched root filter" is what hid the 8000↔7200 rate converter
for several passes (finding F24).

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
exponential). Their interfaces expose fixed-point values; `FP_Pow` takes
one argument and approximates `exp()`.

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
| ✅ `MRF` | **Multi-Rate Filter** | `FPM_MRF_init` takes up/down factors and computes taps-per-phase; Bell 103 runs 10:9 and 3:10 (finding F24). *Not* "matched root filter". |
| ✅ `FSM` | **Frequency Shift Modulator** | `FPM_FSM_modulate` switches a tone generator between two frequencies per bit. |
| ✅ `FSD` | **Frequency Shift Demodulator** | the counterpart; delay-line discriminator plus lowpass (finding F27). |
| ✅ `MTD` | **Multi-Tone Detector** | a bank of resonators, one per tone, with an energy-ratio verdict (finding F28). |
| ✅ `ECC` | **Echo CanCeller** | the entry point is `FPM_ECC_cancel`. Not "error correcting code" — this is the DSP layer. |
| ✅ `SDM` | **Scrambler/Descrambler** | `FPM_SDM_scrambler` and `FPM_SDM_descrambler` process the stream; `FPM_SDM_init` initialises their state. |
| ✅ `VTB` | **Viterbi** | `VTB_decoder`, `VTB_DIFF_TBL`, `TrellisTransitionTable`, `VTB_BOUND_7200/9600/12000/14400` — the trellis decoder for V.32bis/V.17 rates. |
| ✅ `AGC` | Automatic Gain Control | standard; `FPM_AGC_Freeze`/`_Release` behave as expected. |
| 🟢 `FSE` | **Fractionally Spaced Equalizer** | standard term, and its position in the V.22 chain fits (below). |
| `PPS` | **Context-dependent** | V.22 receive PPS is a phase splitter; `FPM_PPS_filter` and `V22_PPS_filter` are transmit polyphase pulse shapers. Trace the direction and caller before expanding the name. |
| 🟢 `SRE` | **Symbol Recovery** (timing) | `FPM_SRE_recover` is the processing entry point and it uses `FPM_atan` and `FPM_rms` — a phase-error estimate, i.e. timing recovery. |
| ✅ `ADEQ` | Adaptive equalisation | `fpm_adeq.c` defines `FPM_lmsupd`, `FPM_lmsupd2` and `FPM_block_update`. |
| 🟡 `SMC` | Symbol Mapping / Coder | `FPM_SMC_encoder` and its constellation tables map bits to complex points; the original expansion of the abbreviation remains uncertain. |
| 🟡 `DFTC` | Discrete Fourier Transform; suffix uncertain | `src/pump/v34/DFTC.c` measures V.34 probe-bin energy. V.8 has a separate `V8Dftc.c`; neither filename establishes the meaning of the suffix. |

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
  10/9 fast with no conversion at all (finding F23, since retracted).
- **`FP_Pow`** reads as a general power function. It is `exp()`. Anything built
  against the name rather than the coefficients would be wrong.

The rule that follows: treat an abbreviation as a hypothesis until the code
confirms it, and record which it is.

**Pre-emphasis field names.** The object uses the stem `preemp`, reflected
in names such as `preemp_index`, `preemp0`, `preempindex` and `tx_preemp`.
Use that stem when searching field accesses.

Search implementation fields with `preemp`; diagnostic strings can use the
longer spelling. Finding F1959 records how searches for the long form missed
the transmit-side implementation. Findings 1956–1959 preserve the earlier
field spelling as historical evidence.

Two related names that are NOT this: `ec_prem_coef_*` is the echo canceller's
pre-filter, and `V34PREEMPHASIS` is the object's debug string prefix, which
keeps the long spelling because the differential tier compares transcripts
character for character.
