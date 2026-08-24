# Spec conformance: what to test, what not to, and what it costs

`docs/method/tiers.md` §5 is the tier this plans for. It exists because every
other tier measures agreement with the blob and **none of them can tell you
whether the blob is right**: hand the differential, codegen and mutation tiers
a function that faithfully reproduces an object which mis-implements the
standard and all three go green.

The tier has exactly ONE instance today — `V90MP`'s CRC pair against
10.1.2.3.2/V.34 and Table 16/V.90, in `test/unit/t_v90mp.cpp`'s
`run_mp_crc_spec`, 1,084 checks (finding 7413). Grepping all 242 files of
`test/unit/` for a Recommendation reference returns that file and nothing else.

**Headline numbers.**

| | |
|---|--:|
| clause-to-site pairings examined | **58** |
| worth doing | **21** — ten first, eleven queued |
| named as NOT worth doing | **21** |
| clauses about code nobody has written | **14** |
| already covered | 2 |
| candidate non-conformances found while surveying | **8** |
| retrofit backlog, in landed symbols | **113** of 1,204 |

Two Recommendations carry published known-answers this tree can use today, and
a third carries one it cannot.

---

## 1. The discriminator, and it is not "is it in the spec"

Almost everything in these nine documents is *in* the spec. That is not the
question. The question is whether a conformance test could tell us something
**no existing tier can**, and that needs our source and the object to be able
to be wrong TOGETHER. There are exactly three shapes where that happens, and
every candidate below is classified by which one it is:

**SHAPE 1 — EXTENT.** Which fields an operation includes or excludes. V.34
10.1.2.3.2's *"all of the information bits in a sequence, except the frame sync
bits, the start bits, and the fill bits"* is the type case: a differential test
can never see it, because both sides skip the same fields whether or not those
are the right fields to skip.

**SHAPE 2 — DIRECTION.** Bit order, which end is transmitted first, which bit
is the LSB, which arm of a branch goes with which type. A symmetric round trip
through one class cannot see a reversal — which is exactly how D920 survived
until finding 6800 read V.90 Table 14 and V.92 Table 23. Every field of
`V92CP` round-trips correctly except the mask words, and *nothing in this tree
could have said so* without the Recommendation.

**SHAPE 3 — AN INDEPENDENTLY DERIVABLE CONSTANT.** A polynomial, a table entry,
a rate, a limit, a timing bound. The discriminator is
`docs/deviation-triage.md`'s, taken over from D250: **the correct value must be
derivable without reading the object.** A constant that can only be checked by
reading it back off the blob is not a candidate — it is a differential check
wearing a spec's clothes.

Anything that is none of the three is already pinned by the differential tier
and is named in §7 rather than planned for.

**One corollary that decides several rows below, and that a first pass over
this survey got backwards.** "The differential tier already pins it" is *not* a
reason to reject a shape-3 candidate. The differential tier pins our table
against the blob's; that agreement is what this tier exists to look past. D250
is the proof: `MTD7_COEF_9600` is reproduced byte for byte, passes every tier,
and is wrong — and what convicted it was a derivation the object did not supply
(findings 1413, 1416). A published table is a derivation the object did not
supply. Reject a shape-3 candidate only when the correct value cannot be
obtained except from the object.

## 2. Method, and what it could not measure

- The Recommendations were read as text: `pdftotext -layout` over all nine PDFs
  in `../itu-specs/`, 13,494 lines. Every quotation below is from that text.
  V.8, V.25 and V.25 Cor. 1 were read whole, being 780, 672 and 179 lines.
- **What is "reconstructed enough to test" was taken from source presence, not
  from the object tree.** `tools/coverage.py` and `tools/service.py` learn what
  exists by globbing `build/repro`, and that directory is empty in a fresh
  worktree; filling it means a build, and this survey ran alongside another
  agent's continuous one. So "exists" here means *defined in `src/`*, checked by
  grep. `docs/coverage.md`'s standing figure is 71.1% translated, **1,204 of
  1,861 `.text` symbols**, and that is the denominator §9 quotes against.
- Four derivations were run rather than asserted: a 64-point DFT of `probe[]`
  against Table 17/V.34; the 512 numbers of Table 1/V.90 against
  `src/service/pcm.c`; the 32 squared amplitudes of Table 15/V.90; and the
  algebra relating the tree's two CRC spellings (§4.1). Two `gcc` invocations
  on one file plus a fourteen-line driver were the only compilation this survey
  did.
- `V90Phase4Modulator` was being reconstructed in another worktree while this
  ran. Nothing in that family is planned here.

---

## 3. Do any of the Recommendations carry usable known-answers?

**YES — three, of which two judge code that exists today.** Finding 7413
recorded that V.34 and V.90 carry no test vector *for the CRC*, and that is
still true of the CRC. It is not true of the corpus.

### 3.1 Table 17/V.34 — the probing tones. USABLE NOW.

> Table 17/V.34 – Probing tones. `cos(2πft + ϕ)`
> 150/0, 300/180, 450/0, 600/0, 750/0, 1050/0, 1350/0, 1500/0, 1650/180,
> 1950/0, 2100/0, 2250/180, 2550/0, 2700/180, 2850/0, 3000/180, 3150/180,
> 3300/180, 3450/180, 3600/0, 3750/0

Twenty-one frequencies with twenty-one phases and — just as load-bearing —
**four in-band absences**: 900, 1200, 1800 and 2400 Hz are not in the table.
`const short probe[V34_PROBE_SAMPLES]` at `src/pump/v34/v34hshak.c:1698` is one
150 Hz period of that waveform, 64 samples, played round and round by
`v34hstx1.cpp`'s `TX_L1`.

**Derived and checked during this survey.** A 64-point DFT at 9600 Hz (bin
spacing exactly 150 Hz):

| bins | f (Hz) | phase | Table 17 |
|---|---|---|---|
| 1,3,4,5,7,9,10,13,14,17,19,24,25 | 150…3750 | 0° | 0° ✓ |
| 2,11,15,18,20,21,22,23 | 300,1650,2250,2700,3000,3150,3300,3450 | 180° | 180° ✓ |
| 6, 8, 12, 16 | 900, 1200, 1800, 2400 | no energy | omitted from Table 17 ✓ |

Twenty-one tones present, twenty-one rows in the table, **zero disagreements**,
all amplitudes equal to within 0.01% (2605.30 to 2605.57).

Why this is the best candidate in the file is written in the tree's own words,
ten lines above the table:

> *"The values are a property of the object rather than a derivation. They came
> out of `.rodata` BY TOOL, not read off a listing, and what proves the
> transcription is `t_v34hstx1.c`'s `memcmp` against `ref_probe`."*

Table 17 makes it a derivation. It also retires two facts the same comment
records as unexplained regularities — `probe` is even about index 32 and
`probe[32] == -probe[0]` — both of which fall out of Table 17's sign pattern.

### 3.2 Table 1/V.90 — "The universal set of PCM codewords". USABLE NOW.

128 rows, four published columns each: Ucode, µ-law PCM codeword, µ-law linear
value, A-law PCM codeword, A-law linear value. *"All modifications defined in
Recommendation G.711 have already been made… A linear representation of each
PCM codeword is also given."* That is **512 published numbers**, and the code
they judge is landed and used at three separate sites:

- the **Ucode → PCM codeword** mapping, spelled `(code & 0x7f) ^ 0xff` for
  µ-law and `(code & 0x7f) ^ 0xd5` for A-law, in
  `src/pump/v90/V90ConstellationPower.cpp` (`getPower`),
  `src/pump/v90/V90Phase3Modulator.cpp` (`resetDILGenerator`) and
  `src/pump/v90/V90Mapper.cpp`;
- `alaw2linear` and `ulaw2linear` in `src/service/pcm.c`.

**Checked during this survey: 0 mismatches in 512.**

### 3.3 Tables 7–10/V.92 — the ANSpcm codeword sequences. NOT USABLE YET.

The strongest known-answer in the corpus and there is nothing to run it
against. 8.3.1/V.92 gives the generating equation —

> The 301 symbol Ucode sequence may be generated using the following equation:
> x = scl × √2 × cos(2πk × 79 / 301 + ϑ) + 0.5 for k = 0, 1, 2, …, 300 … **The
> resulting output shall equal the output defined in Tables 7 to Table 10**

— and then prints all four sequences, µ-law and A-law, at −9.5, −12, −15 and
−18 dBm0: **2,408 published codewords with a "shall equal" beside them.**

Nothing in `src/` generates ANSpcm. This library is the ANALOGUE side and
ANSpcm is the digital modem's signal; what the tree has is the detector's
parameters (`ANSPCM_DEMODULATION_LENGTH`,
`ANSPCM_CORRELATION_THRESH_FOR_VALIDATION`, `include/dsplib/V90Parameters.h`)
and no reference sequence to correlate against. **A note for later, not a plan
item** — but the highest-value one in the file, because the day a reference
sequence is reconstructed its oracle already exists and is ITU's.

### 3.4 Table 8 and Table 9/V.34 — self-checking, and nothing implements them.

8.2/V.34 gives the SWP derivation as an algorithm (*"the counter is set to
zero. The counter is incremented by r at the beginning of each mapping frame.
If the counter is less than P, send a low frame; otherwise, send a high frame
and decrement the counter by P"*) **and** the resulting patterns for every
rate/symbol-rate pair, with a worked example (*"at 19 200 bit/s and symbol rate
3000, SWP is 0421 (hex) or 000 0100 0010 0001 (binary)"*). Table 9 does the
same for AMP. A table the spec both derives and tabulates checks itself.
Neither is in `src/`: the V.34 primary-channel mapping chain is not
reconstructed, only the handshake. Note for later.

### 3.5 V.8, V.25 and both corrigenda carry no known-answer at all

Searched case-insensitively across all nine documents for `test vector`,
`check value`, `worked example`, `for example`, `test pattern`, `test
sequence`, `illustrat`, `shall equal`, `resulting output`, `verify that`,
`may be derived`, `calculated as`, `polynomial`, `x^`, `hexadecimal`,
`Annex`, `Appendix`; plus a full listing of every numbered Table in the corpus
(`^ *Table [0-9]+`) and an eye over each for a column of *results* rather than
a column of *definitions*.

In V.8, V.25 and V.25 Cor. 1 the combined hit count for the vector-shaped terms
is **two, both the same cross-reference** (*"for example, see 9.3.1/V.92"*), and
`Annex` appears twice, both pointing at V.32 bis. **A corrigendum is where a
numeric correction with a worked value would most likely land and neither
V.25 Cor. 1 nor V.92 Cor. 1 has one.**

What V.8 has instead is the next best thing: **Table 1/V.8 prints three literal
10-bit patterns**, which are known answers for the encoder —
`1111111111`, `0000000001` and `0000001111` — directly matchable against
`V8_SEQ_PREAMBLE_0 = 0x3ff`, `V8_SEQ_PREAMBLE_1 = 0x00f` and the QCA1
patterns. Tables 2 to 8 are the same thing at octet granularity. And the two
Recommendations are dense in **tolerance figures**, which is what makes §5's
V.8 and V.25 items possible at all: 2100 ± 1 Hz, 450 ± 25 ms, 15 ± 0.1 Hz,
envelope 0.8 ± 0.01 to 1.2 ± 0.01, ≥ 24 dB outside 2100 ± 200 Hz, 5 ± 1 s,
three CJ octets; and from V.25, 1300 ± 15 Hz with 0.5–0.7 s on and 1.5–2.0 s
off, 2100 ± 15 Hz, 3.3 ± 0.7 s, 425–475 ms, 180 ± 10° within 1 ms, and
75 ± 20 ms.

---

## 4. Inventory

Grouped by mechanism rather than by document, because the clauses cross
documents: V.90 and V.92 both defer their CRC to V.34, and V.92's Tables 23 and
24 restate V.90's Table 14.

### 4.1 The CRC family — nine implementations, one tested

*"The CRC generator used is described in 10.1.2.3.2/V.34"* appears **five times
in V.90** (INFO, DIL descriptor, Jd, CP, MP) and **ten times in V.92**. The
clause fixes four things:

    polynomial x^16 + x^12 + x^5 + 1
    the shift register is loaded with ALL ONES before anything is shifted in
    the contents are output starting with bit 0, and bit 0 of the CRC is the LSB
    the CRC covers every information bit in the sequence EXCEPT the frame sync
        bits, the start bits, and the fill bits

`src/` contains **nine** distinct implementations of that register:

| # | site | file | table | fixture | spec block |
|---|---|---|---|---|---|
| 1 | `V90MP::calcCRC` / `evaluateCRC` | `src/pump/v90/V90MP.cpp` | 16/V.90 | `t_v90mp.cpp` | **yes** (7413) |
| 2 | `V90CP::calcCRC` / `resetCRC` / `evaluateCRC` | `src/pump/v90/V90CP.cpp` | 14/V.90 | `t_v90cpleaf.cpp`, `t_v90cpinfo.cpp` | no |
| 3 | `V92CP::calcCRC` / `resetCRC` / `evaluateCRC` | `src/pump/v90/V92CP.cpp` | 23,24,30/V.92 | `t_v92cpcrc.cpp` | no |
| 4 | `v90jd_crc_bits` + its extent | `src/pump/v90/V90Jd.cpp` | 13/V.90 | `t_v90jd.cpp` | no |
| 5 | `v92jd_crc_bits` + its extent | `src/pump/v90/V92Jd.cpp` | 27/V.92 | `t_v92jd.cpp` | no |
| 6 | `dilCrcBit` + its extent | `src/pump/v90/DILdescriptorPacker.cpp` | 12/V.90 | `t_dilpack.cpp` | no |
| 7 | `dilCrcBit` + its extent | `src/pump/v90/V92DILdescriptorPacker.cpp` | 20/V.92 | `t_v92dilpack.cpp` | no |
| 8 | `v8_crc` | `src/v8/v8util.c` | — | `t_v8util.c` | no, and see §7 |
| 9 | `getbit`'s fold | `src/pump/v34/v34hshak.c`, `v34hstx1.cpp` | 14–16/V.34 | `t_v34hstx1.c` | no |

Sites 1–7 are the **reflected** spelling: feedback `crc[0] ^ bit` into stage 15,
XORed into 3 and 10 — 0x8408, which is 0x1021 reversed. Sites 8 and 9 are the
**unreflected** spelling: `bit = crc >> 15`, `crc <<= 1`, `crc ^= 0x1021`.

**That is not two answers to one question, and settling it saved a test.** With
`u_i = r_(15-i)` the two registers are the same machine: `u15` is `r0` so the
feedback bit is the same; a left shift of `u` is a right shift of `r`; 0x1021
reversed is 0x8408; and 0xffff reverses to itself — so `U = bitreverse16(R)` at
every step over the same input. The two emit the same wire bits provided one
writes `u15` first where the other writes `r0` first, and both do: `getbit`
loads the CRC into its accumulator and emits bit 15 downwards, and
`V90MP::infoToBits` writes `bits[end + 1 + k] = crc[k]` from `crc[0]` up. **So
the polynomial and the emission direction are settled by algebra and need no
test at any of the nine sites.** What is not settled by algebra, and differs at
every one of them, is the EXTENT: which bit index the register starts at, which
it stops at, and which indices are skipped as framing.

### 4.2 The message layout tables — extent and direction

Every wire message in the corpus has a table fixing its field boundaries, and
each says *"Bit 0 is transmitted first in time"*:

| document | tables | what they define |
|---|---|---|
| V.34 | 14, 15, 16, 18, 19, 20, 21, 22, 23 | INFO0, INFO1c, INFO1a, J, J′, MP Type 0/1, INFOh, MPh |
| V.90 | 7, 8, 9, 10, 11, 12, 13, 14, 16 | INFO0d, INFO0a, INFO1d, INFO1a, DIL descriptor, Jd, CP, MP |
| V.92 | 11–20, 23, 24, 25, 27, 28, 30, 31, 32, 33, 34 | QC/QCA, INFO0d/0a, INFO1d/1a, DIL, CPu/CPt/CPus/CPd, RM, SUVu/SUVd, TRN2u, MH, T1 |
| V.92 Amd. 1 | 18, 19, 31, 32 (revised) | INFO1a for PCM- and V.34-upstream, SUVd, MH |
| V.8 | 1–8 | preamble, information categories, call function, modulation modes, PCM availability, protocol, PSTN access, NSF |

Table 14/V.90 is the worked case for what makes these testable. Its start bits
sit at 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255 and
`272 + δ`; the CRC occupies `273+δ:288+δ` and the fill bits `289+δ:291+δ`. The
object walks `i` from `0x12` to `word_3bb0 - 0x11`, stepping over every multiple
of seventeen. **That is correct only because γ and δ are always multiples of
136 = 8 × 17** — γ is 136 times the maximum constellation index and δ is γ or
2γ+136 — so "every multiple of seventeen is a start bit" survives the variable
length. Nothing in the tree records that argument, and no differential test can
make it: both sides skip multiples of seventeen either way.

V.8 is the same problem at octet granularity. Clause 5.1 fixes the frame as
*"start-bit (0) b0 b1 b2 b3 0 b5 b6 b7 stop-bit (1)"* with *"b0 the least
significant bit"*; 5.2 fixes b4 to ONE in an extension octet with b3 and b5
ZERO; and clause 5 states the invariant the framing exists for — *"a coding
constraint is maintained which ensures that HDLC flags (01111110) cannot appear
in the bit stream"*. The tree's recogniser is `(w & 0x39) == 0x11` at
`src/v8/v8jm.c:534`, which is 5.2 exactly. `charFlip ∘ charFlip` round trips
through `V8GetMessage`/`V8SetMessage`, which is precisely the symmetry that
hides a reversal.

**V.92 Amd. 1 replaces Tables 18, 19, 31 and 32.** Replacing a table moves field
positions and therefore moves the CRC extent. Any spec block for INFO1a, SUVd
or MH must be written against the amendment, not against V.92 (11/2000).

### 4.3 Independently derivable constants

| clause | what it fixes | site in `src/` | exists |
|---|---|---|---|
| Table 17/V.34 | 21 probing tones, 21 phases, 4 absences; 24 repetitions at 6 dB | `probe[64]` `v34hshak.c:1698`; `TX_L1`, `v34hstx1.cpp` | yes |
| Table 1/V.90 | 128 Ucodes × codeword and linear value, both laws | `src/service/pcm.c`; the `^0xff`/`^0xd5` masks at three V.90 sites | yes |
| Table 15/V.90 | 32 average-power limits, printed as squared amplitudes | `V90ConstellationPower::averagePowerLimits` | yes |
| the formula under Table 14/V.90 | the average-power sum, `/(6 · 2^K)` | `V90ConstellationPower::getPower` | yes |
| eq. 7-1, 7-2/V.34 | GPC = 1 + x⁻¹⁸ + x⁻²³, GPA = 1 + x⁻⁵ + x⁻²³ | `src/pump/v34/v34scram.c` | yes |
| 5.3, 6.5/V.90; 6.3/V.92 | which end scrambles with which polynomial | eight `Scrambler`/`Descrambler` sites under `src/pump/v90/` | yes |
| 5.4.2, 5.4.3/V.90 | `R0 = b0 + b1·2¹ + … + b_{K−1}·2^{K−1}`; `2^K ≤ ∏Mi` | `src/pump/v90/ModulusCoder.cpp` | yes |
| Tables 2, 14, 17/V.90; 23, 30/V.92 | `(drn+20)·8000/6` in CP, `(drn+8)` in CPt, `(drn+17)` in CPd | `V90Demodulator::getBitRate`, `V90MappingParamsInt.cpp:301`, `V92ParamsInfo.c:206` | yes |
| Tables 3, 5/V.90 | shaping-frame geometry, `blockLength = 6 / Sr` | `src/pump/v90/V90SpectralShaper.cpp` | yes |
| 5.4.5.1, Table 4/V.90 | `$0 = s0 ⊕ $5(prev)`, `$i = si ⊕ $i−1` | `V90SpectralShaper.cpp`, `src/dsp/DiffCoder.cpp` | yes |
| Table 13/V.90 | `rate(bit n) = 28000 + (n−18)·8000/6` over the Jd mask | `src/pump/v90/V90Jd.cpp` | yes |
| G.711 endpoints, via Table 1/V.90 | `codeSegmentsBoundriesLookupTable[2][8]` | `src/pump/v90/V90Phase3Modulator.cpp` | yes |
| 7.2/V.8 | ANSam: 2100 ± 1 Hz, 15 ± 0.1 Hz envelope, depth 0.8–1.2, reversals 450 ± 25 ms, ≥ 24 dB out of band | `v8_ansaminit`, `v8_ansamgenerate`, `v8_phase_rev_detect`, `src/v8/v8sig.c` | yes |
| 3.5, 8.2.2, 8.2.3/V.8 | CJ is three all-zero octets; ANSam runs 5 ± 1 s; JM stops after all three CJ octets | `src/v8/v8hs.c`, `v8hsrx.c`, `v8handshak.c` | yes |
| 3.4, 3.6/V.8 | CM on V.21(L), JM on V.21(H) | `v8_V21_Init` call sites | yes |
| Tables 1–7/V.8 | the preamble patterns and every category and option-bit code | `include/dsplib/v8.h`, `src/v8/v8seq.c`, `v8jm.c` | yes |
| 2.1, 2.2, 2.3, 4.4/V.25 | 1300 ± 15 Hz on 0.5–0.7 s / off 1.5–2.0 s; 2100 ± 15 Hz for 3.3 ± 0.7 s; reversals 425–475 ms; silence 75 ± 20 ms | `src/dsp/fpm_tone_cfg.c`, `src/pump/v23/v23modem.c`, `src/callprog/callingtone.c` | yes |
| Table 8, Table 9/V.34 | SWP and AMP | — | **no** |
| Tables 7–10/V.92 | the ANSpcm sequences | — | **no** |
| Table 33/V.92, Tables 25/28/29 | T1 timeout encoding, RM symbol pattern | — | **no** |
| Annex A, Table A.10/V.34 | the precode-CRC superframe | — | **no** |

---

## 5. The ranking

Ranked by *what a conformance test could say that no existing tier can*, with
the specific wrong-together failure named. **The four items that would convict
something already identified are ranked above the ones that would only record a
pass**, and each is marked with the candidate non-conformance it settles.

### 5.1 Do first — ten

| # | clause | site | shape | fixture | checks | settles |
|--:|---|---|:-:|---|--:|---|
| 1 | 7.2/V.8 | `v8_ansaminit`, `v8_ansamgenerate`, `v8_phase_rev_detect` | 3 | `t_v8sig.c` (extend) | ~30 | **N3, N4** |
| 2 | Table 17/V.34 | `probe[64]`, `TX_L1`'s repetition count | 3 | `t_v34hstx1.c` (extend) | ~50 | — |
| 3 | Table 14/V.90 + 10.1.2.3.2/V.34 | `V90CP::calcCRC` / `evaluateCRC` | 1 | **new** `t_v90cpcrc.cpp` | ~1,000 | — |
| 4 | 5.1, 5.2, Tables 2–7/V.8 | `V8UpdateModemParameters`, `ext_word`, `v8_getbit` | 1,2,3 | `t_v8jm.c`, `t_v8util.c` (extend) | ~80 | **N5, N1** |
| 5 | Table 1/V.90 | `ulaw2linear`, `alaw2linear`, the Ucode masks | 3 | `t_pcm.c` (extend) | 512 | — |
| 6 | 3.5, 8.2.2, 8.2.3/V.8; 2.3, 4.4/V.25 | CJ count, ANSam duration, the V.23 answering sequence, `FPM_TONE_CFG` | 3 | `t_v8hs.c`, `t_v23modem.c`, `t_fpm_tone.c` (extend) | ~20 | **N2, N6, N7** |
| 7 | Tables 23, 24, 30/V.92 + 10.1.2.3.2/V.34 | `V92CP`'s CRC extent and the CPt/CPu/CPus arms | 1,2 | `t_v92cpcrc.cpp` (extend) | ~800 | — |
| 8 | Table 13/V.90 | `V90Jd`'s CRC extent **and** its rate-capability mask | 1,2 | `t_v90jd.cpp` (extend) | ~300 | — |
| 9 | eq. 7-1, 7-2 and clause 7/V.34 | `scrambleGPC`/`GPA`, `descrambleGPC`/`GPA`, the pairing | 3,2 | `t_v34scram.c` (extend) | ~200 | — |
| 10 | Table 15/V.90 and the formula under Table 14 | `averagePowerLimits`, `getPower` | 3 | `t_v90cpower.cpp` (extend) | ~40 | — |

### 5.2 Do next — eleven

| # | clause | site | shape | fixture |
|--:|---|---|:-:|---|
| 11 | 5.3, 6.5/V.90; 6.3/V.92 | the eight `Scrambler`/`Descrambler` sites, taps measured as delays | 3 | `t_scrambler.cpp` (extend) |
| 12 | Table 27/V.92 | `V92Jd`'s CRC extent | 1 | `t_v92jd.cpp` (extend) |
| 13 | Table 12/V.90 | `DILdescriptorPacker`'s CRC extent | 1 | `t_dilpack.cpp` (extend) |
| 14 | Table 20/V.92 | `V92DILdescriptorPacker`'s CRC extent | 1 | `t_v92dilpack.cpp` (extend) |
| 15 | 5.4.2, 5.4.3/V.90 | `ModulusEncoder`/`Decoder` bit weighting, and `out[5] < M5` | 2,1 | `t_moduluscoder.cpp` (extend) |
| 16 | Tables 2, 14, 17/V.90; 23, 30/V.92 | the whole `drn` → rate chain | 3 | `t_v90demod.cpp`, `t_v92unpck.c` (extend) |
| 17 | Tables 3, 5/V.90 | `V90SpectralShaper`'s frame geometry | 3 | `t_v90shapeact.cpp` (extend) |
| 18 | 5.4.5.1, Table 4/V.90 | the differential-coding recurrences | 3,2 | `t_diffcoder.cpp` (extend) |
| 19 | Table 1/V.90 via G.711 | `codeSegmentsBoundriesLookupTable` | 3 | `t_v90p3mod.cpp` (extend) |
| 20 | clause 5/V.8 | the HDLC-flag invariant over the whole CM/JM stream | 1 | `t_v8util.c` (extend) |
| 21 | 7.2/V.8, 2.3/V.25 | ANSam out-of-band power ≥ 24 dB; the ≤ 3 dB / 400 µs reversal transient | 3 | **new**, borrowing `t_psd.cpp`/`t_fft.cpp` |

### 5.3 The top three, and why each earns its place

**#1 — 7.2/V.8's ANSam parameters.** It earns the top slot on yield: it is the
only block in the plan that has already convicted the object twice, and one of
the two is structural rather than a typo. *"amplitude-modulated by a sinewave at
15 ± 0.1 Hz"* against `v->tone.f04 = 0x1a` — 26 steps of a 14-bit accumulator at
9600 Hz is **15.234 Hz**, and step 25 is 14.648, so with a granularity of
9600/16384 = 0.586 Hz **no integer step can land inside the tolerance**. Both
sides step by 26; the differential tier is blind by construction; the codegen
tier compares two identical immediates; and mutation would report the change
CAUGHT at a claim nobody made. Only the Recommendation can say the number is
wrong. The same block covers the carrier (`0xe00` → exactly 2100.000 Hz, which
also *proves* the 9600 Hz sample rate by arithmetic rather than by comment), the
envelope depth (`0xccd`/`0x4000` → exactly [0.800, 1.200], dead on
*"0.8 ± 0.01 and 1.2 ± 0.01"* — and incidentally shows two source comments
calling it "five percent" to be wrong), the transmit reversal interval (450.0 ms
exactly) and the receive acceptance window, which is N4.

**#2 — Table 17/V.34 against `probe[64]`.** The sharpest *methodological* case,
because the tree says out loud what the test would fix: *"The values are a
property of the object rather than a derivation."* Sixty-four numbers pulled out
of `.rodata` by tool and checked by a `memcmp` against the blob — the definition
of a table both sides get right or wrong together. The Recommendation prints
twenty-one frequencies, twenty-one phases and, harder to get right by accident,
four in-band absences; a DFT of the array reproduces all forty-six. **What no
other tier can say:** that this is the V.34 line probe and not merely the
object's line probe. One wrong sample leaves the probing signal wrong on the
wire, the far end's channel estimate wrong with it, and every gate here green.

**#3 — `V90CP`'s CRC extent against Table 14/V.90.** The direct retrofit of the
worked example and the first case the tier's own text names. `calcCRC` walks `i`
from `0x12` to `word_3bb0 - 0x11` and skips every `i % 17 == 0`. All three are
extent decisions, both sides make the same one, and **a differential test cannot
see which fields are skipped — only that both skip the same ones.**
`word_3bb0 - 0x11` is Table 14's start bit at `272 + δ` exactly when `word_3bb0`
is `289 + δ`; and "every multiple of seventeen is framing" survives the
variable-length body only because γ is 136 times the constellation index, δ is γ
or 2γ+136, and 136 = 8 × 17. That argument is nowhere in the tree. Everything
about the method transfers from `run_mp_crc_spec`.

---

## 6. Cost

**The benchmark is the one that exists.** `run_mp_crc_spec` and its six helpers
are **about 300 lines of `test/unit/t_v90mp.cpp`** — 117 lines of spec-derived
helpers (`spec_crc16`, `spec_start_bits`, `spec_crc_at`, `spec_info_bits`,
`spec_layout`, `spec_seed`) and 182 lines of driver — producing 1,084 checks in
a fixture that already existed.

**Extending a fixture is most of a day. A new fixture is most of the cost**,
because it brings a Makefile target, a `ref_` declaration block, a seeded slot
with a guard past the object, and a mutation suite with it.

**Nineteen of the twenty-one items extend an existing fixture.** The two
exceptions:

- **Item 3.** The MP pair lives in one file and the CP pair does not.
  `V90CP::calcCRC` is driven from `test/unit/t_v90cpleaf.cpp` and
  `V90CP::evaluateCRC` from `test/unit/t_v90cpinfo.cpp`, and
  `run_mp_crc_spec`'s shape needs both in one binary — it calls `calcCRC`, reads
  the register out, then hands `evaluateCRC` a crafted peer CRC and asserts
  which way it answers. Either a declaration moves between two fixtures that
  were deliberately split, or a third fixture is created in `t_v92cpcrc.cpp`'s
  image. **The second is right**, because the V.92 twin already has exactly that
  file and the two should read alike.
- **Item 21.** A spectral measurement needs an FFT harness `t_v8sig.c` does not
  have. It is in §5.2 rather than §5.1 for that reason alone.

Item 6 has a smaller version of the same problem: the CJ acceptance counter
lives behind `static` functions in `src/v8/v8hsrx.c` that no fixture reaches
directly. Driving it through `V8Process` inside `t_v8hs.c` is cheaper than
exporting anything, and is the route to take.

Two costs the line count hides:

- **A spec block must be shown to fire** (finding 134). `run_mp_crc_spec` was
  validated by changing `spec_crc16`'s preload from 0xffff to 0 and watching 337
  of its 1,084 checks fail while every differential block stayed green. Budget
  that ritual per block, not per plan.
- **A spec helper must not be written the way the code is written.**
  `spec_start_bits` is Table 16 copied out as ten literals *because* a helper
  that derived them as multiples of seventeen could not catch the code deriving
  them wrongly. That discipline is most of why the helpers are 117 lines rather
  than 20.

Rough totals for §5.1: items 5, 1, 6 and 10 are under a day each; items 2, 4, 8
and 9 about a day and a half; item 7 about two; item 3 about three, because of
the new fixture. **Call it two working weeks for the first ten.**

---

## 7. What would NOT be tested, and why

Twenty-one pairings were reached and rejected. A plan that proposes testing
everything is not a plan.

**Already settled, so a test would restate rather than decide**

- *The CRC polynomial and its emission direction, at all nine sites.* §4.1: the
  two spellings are one register under `u_i = r_(15-i)`, and both emit matching
  wire bits. Algebra, not a test.
- *10.1.2.3.2's "output starting with bit 0"* at each `infoToBits`. Finding 7413
  settled it for MP against Table 16 and the same algebra transfers; only the
  POSITION the CRC is written to varies, and that is covered by each site's
  extent.
- *V.25 2.1's calling tone.* 1083.7 Hz against *"1300 Hz ± 15 Hz"*, 0.72 s on
  and 2.10 s off against *"not less than 0.5 s and not more than 0.7 s"* and
  *"not less than 1.5 s and not more than 2.0 s"*, and not a sine at all. All of
  it is already carried as D11, D12, D13 and D82 and asserted by
  `t_callingtone`. Nothing to add.

**The differential tier already pins it, and no independent derivation exists**

- *V.34 Table 1 (symbol rates), Table 2 (carrier frequencies), Tables 3 and 4
  (α, β, γ).* Held against the blob's own tables; no extent, no direction.
- *V.34 Tables 14/15/16 (INFO0, INFO1c, INFO1a).* Worth a later look, but the
  handshake-layer bit maps are pinned by `t_v34info.c`, `t_v34info1d.c` and
  `t_v34info1a.cpp`, and no field was found whose position the spec states and
  the code derives differently.
- *V.21's mark and space frequencies* (980/1180/1650/1850). V.21's constants,
  not V.8's. The *channel assignment* is in scope and is item 6's neighbour; the
  frequencies are not.
- *`V8_HS_DRAIN_BLOCKS`, `V8_RX_SETTLE_BLOCKS`, `V8_AGC_BLOCKS`, the detector
  coefficient table.* No spec-derivable correct value.
- *`v8_crc`.* **Neither V.8 nor V.25 defines a CRC**, so the polynomial can only
  be read off the object; and `crc_enable` is 0 on both the CM and the JM path,
  so it is dead for V.8 signals. It appears in §4.1's table because it is the
  same register as `getbit`'s, not because V.8 asks for one.
- *DTMF row and column frequencies, and the dial-string parser.* 1.1/V.25 puts
  automatic calling in V.25 bis and V.25 ter, and the frequencies are Q.23.

**The spec leaves it to the implementation**

- *`Scrambler`/`Descrambler` store order, temporaries, `shr` against `sar`.* No
  clause; already recorded as unobservable.
- *Filter designs, the equalizer, AGC, timing recovery, the pre-emphasis
  ladder.* V.34 and V.90 specify what goes on the line, not how a receiver gets
  there. A test against "the standard" here is a test against somebody's taste.
- *11/V.25, inhibiting the 2100 Hz detector* — *"the detector **may** be
  inhibited"*, *"It is **suggested** that…"*.
- *The NOTE under Table 15/V.90* — *"The actions that a digital modem takes when
  a constellation set is found to have an average power above the appropriate
  limit are a national matter and are beyond the scope of this
  Recommendation."*
- *V.25 Cor. 1's whole substantive content* — *"there is a **potential** for
  failure to connect if the phase reversal option of answer tone is not used"*.
  Advisory. `v23modem.c` sets `rev_period = 0` and `v8_ansaminit` sets its
  enable to 1; a test can pin the *choice* but cannot call either wrong.
- *3.9 and 5/V.25's DTE/DCE sequencing* (100–600 ms recognition, the 1.8–2.5 s
  silent interval, the response delays). This library does not own the line
  sequence.

**A failure would tell us nothing actionable**

- *`ModulusCoder`'s signed 64-bit accumulator.* Divergence needs K ≥ 64; V.90's
  K tops out at 39.
- *`evaluateCRC` reading outside `bits[]`* for +0x119 values the class never
  writes. Finding 7411 disposed of this: bounded read, cannot leave the object.
- *`initTxSequence`'s one-past-the-end write* at `src/v8/v8seq.c:143`, reachable
  only with both extension fields plus PCM, which `v8dp.c` never asks for. Not
  one of the three shapes; a bounds question, not a conformance one.

**Looks checkable and is not — the two traps worth writing down**

- *Table 1/V.90's µ-law/A-law pairing against `alaw2ulaw` / `ulaw2alaw`.* The
  two halves of Table 1 are two independent ladders indexed by Ucode, not an
  amplitude correspondence: Ucode 0 is µ-law linear 0 and A-law linear 8, Ucode
  1 is 8 and 24. G.711's code-to-code tables map by nearest amplitude, a
  different question. **Measured: the two disagree at 79 of 128 Ucodes**, and
  that is correct behaviour on both sides. A test built on the obvious reading
  would have failed 79 times and indicted nothing.
- *10.1.3.8/V.34's rotation direction* (*"rotating that point clockwise by
  In·90 degrees"*) against `vect16`/`vect4`. Derivable in principle and **not
  judgeable from the tables alone**: the sign convention of the stored imaginary
  part and the index packing are both unpinned, and the decoded points are not
  origin-centred. It needs `v34hstx1.cpp`'s transmit path read first. Named
  rather than attempted.

**Clauses about code nobody has written — fourteen, listed so they are not
re-derived**

V.34 8.2/Table 8 (SWP) and 8.3/Table 9 (AMP); V.34 clause 9 with Table 10
(K, M, L), Table 11, Table 12 (bit inversion) and Table 13 ([Y4..Y1]) — the
precoder, trellis encoder and shell mapper; V.34 Tables 20/21/23/24 (MP and MPh
layouts — there is no `V34MP` class, and `t_v34mp.cpp` tests handshake framing,
not the clause-10.1.3.9 bit layout); V.34 Annex A Table A.10, the precode-CRC
superframe (*"The 16-bit CRC is computed by applying the three complex precode
coefficients to the 16-bit CRC generator (see 10.1.2.3.2)"* — a clean deferral
to the same generator, shapes 1 and 2, and no code); V.92 Table 33 (T1 timeout),
Table 32/MH and Tables 25/28/29 (RM) — the modem-on-hold path is absent; Tables
7–10/V.92 (ANSpcm), §3.3; V.8 Table 8's non-standard-information field layout
(`ext1`/`ext2` are raw octet arrays supplied by the caller); V.8 7.1's signal CI
(neither transmitted nor detected, and 7.1 makes detection optional); and V.8
8.1.1–8.1.2's Te and 75 ± 5 ms silence, for which no implementing constant could
be found.

---

## 8. Candidate non-conformances noticed while surveying

**None of these is a licence to change `src/`.** The reconstruction must match
the object; a conformance failure is a DEVIATION, recorded in
`docs/deviations.md`'s shape with the clause quoted and the side named, and any
fix goes behind `DSPLIB_REPRODUCE_BUGS` with `src/dsp/fpm_div.c` as the pattern.
D920, D923 and D250 are the worked dispositions. **Nothing below has been
written to `docs/deviations.md`**; they are candidates for a test to adjudicate,
not findings. Every one names the side as **both** — the reconstruction
reproduces the object faithfully at each site, which is exactly why no existing
tier can see them.

**N1 — JM's PSTN-access b5 is a constant where the clause is an "if and only
if".** 7.4/V.8: *"Bit b5 is set to ONE if and only if the corresponding bit (b5)
is set to ONE in the received CM."* `rebuildJMSequence` writes the fixed
`V8_SEQ_TAIL_B = 0x161` at `src/v8/v8jm.c:727` and never inspects the received
access0. A CM from a DCE on a cellular connection (b5 = 1) gets a JM with b5 = 0.
Both sides emit the same constant.

**N2 — CJ is accepted after two octets, not three.** 8.2.3/V.8: *"JM
transmission shall continue until signal CJ is detected and **all 3 octets** of
CJ have been received."* `V8_HS_CJ_COUNT = 2` at `src/v8/v8hsrx.c:164`, with
`fdb6` zero on entry from `v8_hs_message_done`, so the counter fires on the
second sync. JM stops one octet early. The *transmitter* is correct — three
octets of `0x001`, `nbits = 30`.

**N3 — the ANSam envelope is 15.234 Hz against a ± 0.1 Hz tolerance, and no
integer step can fix it.** 7.2/V.8: *"amplitude-modulated by a sinewave at
15 ± 0.1 Hz."* `tone.f04 = 0x1a` = 26 over a 14-bit accumulator at 9600 Hz gives
`26/16384 × 9600 = 15.234 Hz`; 25 gives 14.648. Granularity is 0.586 Hz, so the
deviation is **structural, not a transcription slip**. `src/v8/v8sig.c:27` and
the inline copy at `src/v8/v8hs.c:166`. Verified during this survey.

**N4 — the phase-reversal detector's window is narrower than the transmit
tolerance the spec permits.** 7.2/V.8: *"phase reversals at an interval of
450 ± 25 ms"*; 2.3/V.25: *"at intervals of 425 to 475 ms."* The test at
`src/v8/v8sig.c:441` accepts [431, 469] ms. Stated precisely: the clauses
constrain the *transmitter*, so the finding is that the detector does not cover
the tolerance a conformant remote is allowed to use. Our own transmitter is
exactly 450.0 ms and conformant.

**N5 — the V.21-availability bit can never be cleared, because the mask includes
the stop bit.** 5.1/V.8 fixes every octet as *"preceded by a start-bit (ZERO),
and followed by a stop-bit (ONE)"*, and Table 4 item 12 puts V.21 availability
at modn2 b7. `V8UpdateModemParameters` tests `if ((f & 3) == 0) out->b1 &= 0xdf;`
on the **raw** third modulation word at `src/v8/v8jm.c:279-280`. Word bit 1 is
b7; **word bit 0 is the stop bit, which 5.1 fixes at ONE**, so `f & 3` is never
zero and V.21 availability is never withdrawn. Verified during this survey, and
the internal evidence is strong: the block immediately above takes
`f = word[i] >> 1`, shifting the stop bit out before testing, and the transmit
side encodes V.21 at word bit 1. It should read `f & 2`. A textbook shape-1
defect — the operation includes a field the spec excludes — and not recorded in
`docs/deviations.md`.

**N6 — ANSam is transmitted for 12 s against a 5 ± 1 s clause.** 8.2.2/V.8:
*"If not terminated by the receipt of CM or a suitable sigC, ANSam shall be
transmitted for a period of 5 ± 1 s."* `cfg.timeout_a = 0x0c`
(`src/v8/v8dp.c:84`) through `deadline()` gives 12 × 2400 four-sample blocks =
12 s. Weaker than the others because the value is *configuration* — a caller
could pass 5 — but the library's own datapump passes 12.

**N7 — the post-answer-tone silence is 50 ms nominal against 75 ± 20 ms.**
4.4/V.25: *"At the end of the transmission of the answering tone, the DCE shall
provide a silent period for 75 ± 20 ms."* `V23_SILENCE_DIVISOR = 20` gives
8000/20 = 400 samples = 50 ms, below the window's 55 ms floor. It is carried
into range only by frame quantisation — `elapsed > limit` with 160-sample frames
runs 480 samples, 60 ms — a compensation `src/pump/v23/v23modem.c:96-106`
already records in a comment and nothing enforces. **Softer than N3 or N5**,
because the delivered timing does conform; what does not is the nominal figure,
and whether that matters depends on a frame size the library does not fix.

**N8 — two source comments describe the ANSam envelope depth as five percent
and it is twenty.** `include/dsplib/v8.h:844` says `/* Q14: 0.05 */` and
`src/v8/v8sig.c:264` says *"modulates its amplitude by five percent either
way"*. `0xccd`/`0x4000` is 0.2000, and 7.2/V.8 wants *"between (0.8 ± 0.01) and
(1.2 ± 0.01) times its average amplitude"* — which the code hits exactly. **The
code is right and the documentation is wrong**, so this is a comment defect
rather than a deviation; it is listed here because item 1's test is what would
have caught it and it is the cheapest thing in this section to fix.

### What came out conforming, stated as a search result rather than a blank

- `probe[64]` matches Table 17/V.34 tone for tone, phase for phase, absence for
  absence — 46 of 46. Verified here.
- `ulaw2linear`, `alaw2linear` and the `^0xff`/`^0xd5` Ucode masks match
  Table 1/V.90 — 512 of 512. Verified here.
- `averagePowerLimits[0..31]` are Table 15/V.90's amplitudes squared exactly —
  32 of 32. Verified here. Entries 32, 33 and 34 (2396², 2261², 2133²) continue
  the ladder three half-decibel steps past the Recommendation's last row; **that
  is not a deviation**, the table simply stops at −16 dBm0 and the object goes
  further.
- The ANSam carrier `0xe00` is exactly 2100.000 Hz at 9600 Hz — and that
  arithmetic is also the proof of the sample rate, since 2100 has no integer
  representation at 8000 Hz. The envelope depth is exactly [0.800, 1.200] and
  the transmit reversal interval exactly 450.0 ms.
- `scrambleGPC`/`scrambleGPA` derive to taps (18, 23) and (5, 23) — equations
  7-1 and 7-2 — and `descrambleGPC`/`descrambleGPA` are feedforward from the
  input register, the correct dual of *"shall effectively divide"*.
- All eight `Scrambler`/`Descrambler` construction sites carry the polynomial
  5.3 and 6.5/V.90 and 6.3/V.92 require of that end.
- `ModulusEncoder::progress` puts b0 at weight 2⁰, per 5.4.2 and 5.4.3/V.90.
- `V92CP`'s `char_01` at bits 19:20 and `char_02` at 21:25, and the −20/−8
  branch, match Table 23/V.92 **including which arm takes which constant**;
  `V92ParamsInfo.c` uses `(drn + 17)` per Table 30/V.92, correctly different
  from CPu/CPt's 20.
- `V90Jd::getBitVector` matches Table 13/V.90 end to end, CRC extent included:
  sync 0:16 all ones, start bits at 17/34/51, CRC over 18:33 and 35:50 only,
  written to 52:67 low bit first, fill 68:71 zero, 72 bits total.
- `V90CP::evaluateInfo` was checked for D920's defect and does not have it: it
  uses no `binaryTable`, and its accumulate loops walk downward from the high
  index, which correctly realises Table 14's LSB:MSB column convention.
- V.8's extension-octet recogniser `(w & 0x39) == 0x11` is 5.2/V.8 exactly; the
  preamble constants `0x3ff` and `0x00f` are Table 1/V.8 rows 1 and 3; the five
  category tags are Table 2's; the four call-function codes are Table 3's; CM
  goes on V.21(L) and JM on V.21(H) per 3.4 and 3.6; CJ is three all-zero octets
  per 3.5; and 6.3/7.3's PCM co-presence rules hold.

The only *established* non-conformance in scope remains D920,
`V92CP::evaluateInfo` reading the constellation mask words bit-reversed, settled
by finding 6800 against Table 14/V.90 and Table 23/V.92.

---

## 9. The retrofit question, answered with a number

By this tier's standard, everything landed before it existed is untested against
the spec. The count below is of **landed symbols that a numbered clause or table
constrains in one of §1's three shapes** — the definition matters, because "a
clause exists about it" would catch almost every V.90 function and mean nothing.

| family | shape | symbols |
|---|:-:|--:|
| CRC extent and seeding — the nine sites of §4.1 | 1 | 20 |
| V.90/V.92 message field layout — CP, MP, Jd, DIL | 1, 2 | 30 |
| V.34 and V.8 message field layout — INFO, CM, JM, CJ | 1, 2 | 22 |
| Derivable constants — V.34/V.90/V.92 | 3 | 28 |
| Derivable constants — V.8 ANSam and the V.25 tones | 3 | 13 |
| **total** | | **113** |

**113 landed symbols of the 1,204 this tree has translated, or 9.4%**, on top of
the 2 already covered (`V90MP::calcCRC` and `V90MP::evaluateCRC`). That is the
size of the backlog.

Three things to read with it:

- **It is a count of symbols, not of tests.** The 113 collapse into the 21 spec
  blocks of §5, in 19 fixtures, because one block judges a whole class: 1,084
  checks over `V90MP` covered two symbols and would have covered five had the
  class had five. §5 is the actionable list; 113 is the size of what it covers.
- **It is a floor, not a ceiling**, and the reason is §2: source presence was
  the test for "exists", so anything reconstructed but not on this branch is
  uncounted.
- **The ratio worth watching is not the backlog but the hit rate.** Six blocks
  of the twenty-one already have a candidate non-conformance attached before
  anyone has written a line of test — N1 through N7 across items 1, 4 and 6.
  That is the number that should decide whether §5.2 gets written at all.

---

## 10. Order of work

1. **Item 5 first** (Table 1/V.90 into `t_pcm.c`). Smallest, already verified,
   and it establishes the second worked example in the tree — a spec block for a
   *table* rather than for an *algorithm*, which is the shape nine of the
   twenty-one items need and which `run_mp_crc_spec` does not demonstrate.
2. **Item 1** (ANSam). Highest yield, smallest fixture change, and it settles
   N3, N4 and N8 in one sitting.
3. **Item 2** (Table 17/V.34), which retires a comment that admits the values
   are underived.
4. **Item 3** (V90CP CRC), which is also where `t_v90cpcrc.cpp` gets created;
   item 7 (V92CP) then reuses everything item 3 builds.
5. **Items 4 and 6** — the rest of the V.8 and V.25 work, and the other four
   candidate non-conformances.
6. Then 8, 9, 10, and reassess. **Ten blocks is enough to know whether this
   tier's yield is closer to "one D920 per ten blocks" or to "everything
   conforms", and the answer should decide whether §5.2 is written at all.**

**Before any of it, read `docs/method/tiers.md` §5 and finding 7413.** The two
rules a spec block gets wrong if it is written from the code rather than from
the Recommendation are both there: write the helper from the clause and not the
way the code is written, and say in the test that no official vector exists
rather than letting a reader assume the number is ITU's.

**And when one of N1–N7 is confirmed, it goes to `docs/deviations.md`, not to
`src/`.** The clause quoted, the side named, the reachability measured or
declared unmeasured. D250 is the only one of 232 bug-marked entries that earned
a fix, and it earned it by having the correct value derivable four independent
ways.
