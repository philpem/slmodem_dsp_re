# Spec conformance: what to test, what not to, and what it costs

`docs/method/tiers.md` §5 is the tier this plans for. It exists because every
other tier measures agreement with the blob and **none of them can tell you
whether the blob is right**: hand the differential, codegen and mutation tiers
a function that faithfully reproduces an object which mis-implements the
standard and all three go green.

The tier has exactly ONE instance today — `V90MP`'s CRC pair against
10.1.2.3.2/V.34 and Table 16/V.90, in `test/unit/t_v90mp.cpp`'s
`run_mp_crc_spec`, 1,084 checks (finding 7413). This document says what else
is worth having, what is not, and what each would cost.

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
is the LSB. A symmetric round trip through one class cannot see a reversal —
which is exactly how D920 survived until finding 6800 read V.90 Table 14 and
V.92 Table 23. Every field of `V92CP` round-trips correctly except the mask
words, and *nothing in this tree could have said so* without the Recommendation.

**SHAPE 3 — AN INDEPENDENTLY DERIVABLE CONSTANT.** A polynomial, a table
entry, a rate, a limit, a timing bound. The discriminator is
`docs/deviation-triage.md`'s, taken over from D250: **the correct value must be
derivable without reading the object.** A constant that can only be checked by
reading it back off the blob is not a candidate — it is a differential check
wearing a spec's clothes.

Anything that is none of the three is already pinned by the differential tier
and is named in §7 rather than planned for.

## 2. Method, and what it could not measure

- The Recommendations were read as text: `pdftotext -layout` over all nine
  PDFs in `../itu-specs/`, 13,494 lines. Every quotation below is from that
  text.
- **What is "reconstructed enough to test" was taken from source presence, not
  from the object tree.** `tools/coverage.py` and `tools/service.py` learn what
  exists by globbing `build/repro`, and that directory is empty in a fresh
  worktree; filling it means a build, and this survey ran alongside another
  agent's continuous one. So "exists" here means *defined in `src/`*, checked
  by grep. `docs/coverage.md`'s standing figure is 71.1% translated, 1,204 of
  1,861 `.text` symbols, and that is the denominator every count below is
  quoted against.
- Two small `gcc` invocations were made — `src/service/pcm.c` plus a fourteen-
  line driver, twice — to check the companding functions against a published
  table. That is the only compilation this survey did.
- `V90Phase4Modulator` was being reconstructed in another worktree while this
  ran. Nothing in that family is planned here.

---

## 3. Do any of the Recommendations carry usable test vectors?

**YES — two of them, and one is directly usable today.** Finding 7413 recorded
that V.34 and V.90 carry no vector for the CRC, and that is still true of the
CRC. It is not true of the corpus.

### 3.1 Table 1/V.90 — "The universal set of PCM codewords". USABLE NOW.

128 rows, four published columns each: Ucode, µ-law PCM codeword, µ-law linear
value, A-law PCM codeword, A-law linear value. *"All modifications defined in
Recommendation G.711 have already been made... A linear representation of each
PCM codeword is also given."* That is 512 published numbers, and the functions
they judge are landed and heavily used:

- the **Ucode → PCM codeword** mapping, which this tree spells
  `(code & 0x7f) ^ 0xff` for µ-law and `(code & 0x7f) ^ 0xd5` for A-law, at
  `src/pump/v90/V90ConstellationPower.cpp` (`getPower`),
  `src/pump/v90/V90Phase3Modulator.cpp` (`resetDILGenerator`) and
  `src/pump/v90/V90Mapper.cpp`;
- `alaw2linear` and `ulaw2linear` in `src/service/pcm.c`.

**Checked during this survey: 0 mismatches in 512.** All 128 Ucode→µ-law
codewords, all 128 Ucode→A-law codewords, all 128 µ-law linear values and all
128 A-law linear values agree with Table 1. So the test to be written records a
PASS and guards it; it does not open a deviation.

### 3.2 Tables 7 to 10/V.92 — the ANSpcm codeword sequences. NOT USABLE YET.

The strongest vector in the corpus and there is nothing to run it against.
8.3.1/V.92 gives the generating equation —

> The 301 symbol Ucode sequence may be generated using the following equation:
> x = scl × √2 × cos(2πk × 79 / 301 + ϑ) for k = 0, 1, 2, …, 300 ... **The
> resulting output shall equal the output defined in Tables 7 to Table 10**

— and then prints all four sequences, µ-law and A-law, at −9.5, −12, −15 and
−18 dBm0: **2,408 published codewords**, with a "shall equal" beside them.

Nothing in `src/` generates ANSpcm. The library is the ANALOGUE side, and
ANSpcm is the digital modem's signal; what this tree has is the detector's
parameters (`ANSPCM_DEMODULATION_LENGTH`,
`ANSPCM_CORRELATION_THRESH_FOR_VALIDATION` in
`include/dsplib/V90Parameters.h`) and no reference sequence to correlate
against. **A note for later, not a plan item** — but the highest-value one in
the file, because if a reference sequence is ever reconstructed, its oracle
already exists and is ITU's.

### 3.3 Table 8 and Table 9/V.34 — self-checking, but nothing implements them.

8.2/V.34 gives the SWP derivation as an algorithm (*"the counter is set to
zero. The counter is incremented by r at the beginning of each mapping frame.
If the counter is less than P, send a low frame; otherwise, send a high frame
and decrement the counter by P"*) **and** the resulting 12-to-16-bit patterns
for every rate/symbol-rate pair in Table 8, with a worked example
(*"at 19 200 bit/s and symbol rate 3000, SWP is 0421 (hex)"*). Table 9 does the
same for AMP. A table that the spec both derives and tabulates checks itself.

No SWP or AMP table is in `src/` — the V.34 data-mode mapper is not
reconstructed, only the handshake. Note for later.

### 3.4 What was searched, so nobody redoes it

All nine documents, for: `test vector`, `check value`, `worked example`,
`for example`, `test pattern`, `test sequence`, `illustrat`, `shall equal`,
`resulting output`, `verify that`, `may be derived`, `calculated as`,
`polynomial`, `x^`. Plus a full listing of every numbered Table in the corpus
(`^ *Table [0-9]+`) and an eye over each for a column of *results* rather than
a column of *definitions*. V.8, V.25 and V.25 Cor. 1 were additionally read
whole, being 780, 672 and 179 lines.

---

## 4. Inventory

*(Written up per Recommendation in §5's ranking; this section is the raw list,
grouped by the mechanism rather than by document, because the clauses cross
documents — V.90 and V.92 both defer their CRC to V.34, and V.92's Tables 23
and 14 restate V.90's.)*

### 4.1 The CRC family — nine implementations, one tested

10.1.2.3.2/V.34 is the only CRC definition in the corpus, and V.90 and V.92
defer to it everywhere: *"The CRC generator used is described in
10.1.2.3.2/V.34"* appears at 8.5.2/V.90 (CP), 8.6.3/V.90 (MP) and **eight
times in V.92** alone. The clause fixes four things, of which the fourth is the
one no differential test can reach:

    polynomial x^16 + x^12 + x^5 + 1
    the shift register is loaded with ALL ONES before anything is shifted in
    the contents are output starting with bit 0, and bit 0 of the CRC is the LSB
    the CRC covers every information bit in the sequence EXCEPT the frame sync
        bits, the start bits and the fill bits

`src/` contains **nine** distinct implementations of that register:

| # | site | file | spec-tested? |
|---|---|---|---|
| 1 | `V90MP::calcCRC` / `evaluateCRC` | `src/pump/v90/V90MP.cpp` | **yes** (7413) |
| 2 | `V90CP::calcCRC` / `resetCRC` / `evaluateCRC` | `src/pump/v90/V90CP.cpp` | no |
| 3 | `V92CP::calcCRC` / `resetCRC` / `evaluateCRC` | `src/pump/v90/V92CP.cpp` | no |
| 4 | `v90jd_crc_bits` + its extent | `src/pump/v90/V90Jd.cpp` | no |
| 5 | `v92jd_crc_bits` + its extent | `src/pump/v90/V92Jd.cpp` | no |
| 6 | `dilCrcBit` + its extent | `src/pump/v90/DILdescriptorPacker.cpp` | no |
| 7 | `dilCrcBit` + its extent | `src/pump/v90/V92DILdescriptorPacker.cpp` | no |
| 8 | `v8_crc` | `src/v8/v8util.c` | no |
| 9 | `getbit`'s fold | `src/pump/v34/v34hshak.c`, `v34hstx1.cpp` | no |

Sites 1–7 are the **reflected** spelling: feedback `crc[0] ^ bit` into stage
15, XORed into 3 and 10 — 0x8408, which is 0x1021 reversed. Sites 8 and 9 are
the **unreflected** spelling: `bit = crc >> 15`, `crc <<= 1`, `crc ^= 0x1021`.

**That is not two answers to one question, and it was worth an hour to be
sure.** With `u_i = r_(15-i)` the two registers are the same machine: `u15` is
`r0` so the feedback bit is the same, a left shift of `u` is a right shift of
`r`, 0x1021 reversed is 0x8408, and 0xffff reverses to itself — so
`U = bitreverse16(R)` at every step over the same input. The two emit the same
wire bits provided one writes `u15` first where the other writes `r0` first,
and both do: `getbit` loads the CRC into its accumulator and emits bit 15
downwards, and `V90MP::infoToBits` writes `bits[end + 1 + k] = crc[k]` from
`crc[0]` up. **So the polynomial and the direction are settled by algebra and
need no test.** What is NOT settled by algebra, and is different at each of the
nine sites, is the EXTENT: which bit index the register starts at, which it
stops at, and which indices are skipped as framing.

### 4.2 The message layout tables — extent and direction

Every wire message in the corpus has a table that fixes its field boundaries
and says *"Bit 0 is transmitted first in time"*:

| document | tables | what they define |
|---|---|---|
| V.34 | 14, 15, 16, 18, 19, 20, 21, 22, 23 | INFO0, INFO1c, INFO1a, J, J′, MP Type 0/1, INFOh, MPh |
| V.90 | 7, 8, 9, 10, 11, 12, 13, 14, 16 | INFO0d, INFO0a, INFO1d, INFO1a, DIL descriptor, Jd, CP, MP |
| V.92 | 11–20, 23, 25, 27, 28, 30, 31, 32, 33, 34 | QC1d/QC2d/QCA1d/QCA2d, INFO0d/0a, INFO1d/1a, DIL, CPu/CPt/CPd, RM, SUVu/SUVd, TRN2u, MH, T1 |
| V.92 Amd. 1 | 18, 19, 31, 32 (revised) | INFO1a for PCM- and V.34-upstream, SUVd, MH |
| V.8 | 1–8 | preamble, information categories, call function, modulation modes, PCM availability, protocol, PSTN access, NSF |

Table 14/V.90 is the worked case for what makes these testable. Its start bits
sit at 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255 and
272+δ; the CRC occupies 273+δ:288+δ and the fill bits 289+δ:291+δ. The object
walks `i` from 0x12 to `word_3bb0 - 0x11`, stepping over every multiple of
seventeen. **That is correct only because γ and δ are always multiples of
136 = 8 × 17** — γ is 136 times the maximum constellation index and δ is γ or
2γ+136 — so "every multiple of seventeen is a start bit" survives the variable
length. Nothing in the tree records that argument and no differential test can
make it: both sides skip multiples of seventeen either way.

### 4.3 Independently derivable constants

| clause | what it fixes | site in `src/` | exists |
|---|---|---|---|
| Table 1/V.90 | 128 Ucodes × µ-law/A-law codeword and linear value | `src/service/pcm.c`; the `^0xff`/`^0xd5` masks in `V90ConstellationPower.cpp`, `V90Phase3Modulator.cpp`, `V90Mapper.cpp` | yes |
| Table 15/V.90 | 32 average-power limits, printed as squared amplitudes | `V90ConstellationPower::averagePowerLimits`, `src/pump/v90/V90ConstellationPower.cpp` | yes |
| 8.5.2/V.90, the formula under Table 14 | the average-power sum, `/(6 · 2^K)` | `V90ConstellationPower::getPower` | yes |
| Table 14/V.90 bits 20:24 | `(drn+20)*8000/6` in CP, `(drn+8)*8000/6` in CPt | rate decode in `V90CP.cpp` | yes |
| eq. 7-1, 7-2/V.34 | GPC = 1 + x^-18 + x^-23, GPA = 1 + x^-5 + x^-23 | `src/pump/v34/v34scram.c` | yes |
| G.711 segment endpoints (via Table 1) | `codeSegmentsBoundriesLookupTable[2][8]` | `src/pump/v90/V90Phase3Modulator.cpp` | yes |
| Table 8, Table 9/V.34 | SWP and AMP per rate and symbol rate | — | **no** |
| Tables 7–10/V.92 | the ANSpcm 301-symbol sequences | — | **no** |

---

## 5. The ranking

Ranked by *what a conformance test could say that no existing tier can*, with
the specific wrong-together failure named. A candidate whose failure cannot be
named concretely is ranked below one whose can.

*(populated below)*

---

## 6. Cost

The benchmark is the one that exists. `run_mp_crc_spec` and its six helpers are
**about 300 lines of `test/unit/t_v90mp.cpp`** — 117 lines of spec-derived
helpers (`spec_crc16`, `spec_start_bits`, `spec_crc_at`, `spec_info_bits`,
`spec_layout`, `spec_seed`) and 182 lines of driver — producing 1,084 checks in
a fixture that already existed. **Extending a fixture is most of a day; a new
fixture is most of the cost**, because it brings a Makefile target, a `ref_`
declaration block, a seeded slot with a guard, and a mutation suite with it.

---

## 7. What would NOT be tested, and why

---

## 8. Candidate non-conformances noticed while surveying

**None of these is a licence to change `src/`.** The reconstruction must match
the object; a conformance failure is a DEVIATION, recorded in
`docs/deviations.md`'s shape with the clause quoted and the side named, and any
fix goes behind `DSPLIB_REPRODUCE_BUGS` with `src/dsp/fpm_div.c` as the
pattern. D920, D923 and D250 are the worked dispositions. Nothing below has
been written to `docs/deviations.md`; they are candidates for the test to
adjudicate, not findings.

---

## 9. Order of work
