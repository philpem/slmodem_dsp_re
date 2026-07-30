# dsplibs — a source reconstruction of `dsplibs.o`

A documented, modular, maintainable source reconstruction of
`slmodemd/dsplibs.o`: the 1.2 MB x86-32 binary blob holding the entire Smart
Link soft-modem datapump (Bell 103 through V.92, fax Class 1, Caller ID, DTMF,
call progress, voice). The slmodem tree is BSD-licensed with rights waived, but
the source for this one object was lost.

The reconstruction must be **functionally equivalent** — same input, same
output — and filter coefficients are traced back to their original design
intent so they can be *regenerated* at a different sample rate rather than
merely resampled.

Start with **[docs/findings.md](docs/findings.md)**: what the blob is, how it
is structured, and the commands that prove each claim.

## Status

| phase | content | state |
|--:|---|---|
| 0 | tooling, TU map, differential harness | **done** (SpanDSP deferred) |
| 1 | core plumbing (`dp_wrapper`, `FixedRC`, `dp_param`, `FP_math`) | **done** except `FP_math`, `MEMORYC` |
| 2 | Bell 103 / V.21 — *first real connection* | registration, `FPM_phasor`, `fpm_tone` generator done |
| 3 | call progress / dialler — *originate as well as answer* | — |
| 4 | V.23 | — |
| 5 | V.8 negotiation | — |
| 6 | V.22 / V.22bis / Bell 212 (rest of `fpm_*`) | — |
| 7 | V.32 / V.32bis | — |
| 8 | remaining services (CID, DTMF, ring detect, voice, beep) | — |
| 9 | fax Class 1 (V.17 / V.27ter / V.29) | — |
| 10 | V.34 | — |
| 11 | V.90 / V.92 | — |
| 12 | 8 kHz retarget | — |

Full plan, including rationale for the ordering:
`~/.claude/plans/the-directory-slmodemd-contains-compiled-lark.md`

## Ground rules

- **This tree builds standalone.** It never modifies `slmodemd/` or its
  Makefile, and never references `../re/` — that is separate work belonging to
  another effort and is out of scope entirely.
- **32-bit for now.** Differential testing needs `-m32` for as long as the blob
  is the reference. The reconstruction itself is written 64-bit-clean, so the
  blob is the only thing forcing `-m32`.
- **SpanDSP is a test peer only.** It is LGPL; this reconstruction is BSD. It
  is linked into `test/` as an independent interop partner and stimulus
  source. No SpanDSP code, tables or algorithms are read into or copied into
  `src/`. See `third_party/README.md`.
- **Mirror the original.** 209 `.c`, 70 `.cpp`, 1 `pow.S` — the language mix
  and the file layout follow the blob's own translation units.

## Layout

```
docs/     findings, TU map, function attribution, coefficient derivations
include/  public headers (the slmodemd contract)
src/      core/ dsp/ pump/ fax/ service/ tables/
tools/    ELF analysis: TU mapping, symbol maps, table extraction
test/     harness/ (differential + interop) unit/ integration/ interop/
third_party/spandsp/   test peer, LGPL, never linked into src/
```

## Tools

| tool | purpose |
|---|---|
| `tools/tumap.py` | recover the 281-TU map from `STT_FILE` symbols |
| `tools/rcfilter.py` | characterise the 18 resampler filter banks |
| `tools/gen_rc_coeffs.py` | emit `src/core/rc_coeffs.c` from the blob |
| `tools/tuattrib.py` | attribute `.text` functions to their translation unit |
| `tools/symmap.py` | emit the `objcopy --redefine-syms` list for the harness |
| `tools/tabdump.py` | extract a named coefficient table from the blob as C |

```sh
python3 tools/tumap.py    ../slmodemd/dsplibs.o --md docs/modules.md
python3 tools/tuattrib.py ../slmodemd/dsplibs.o --verify --md docs/attribution.md
```

`--verify` holds out the TUs with surviving local symbols as ground truth. Name
-derived attribution scores 22/22; contiguity inference scores 0/8 and is
labelled `provisional` throughout. Treat the two differently.

## Testing

Three tiers, because no single one is sufficient:

1. **Differential** (primary) — reconstruction and blob linked into one binary,
   driven from one deterministic input, outputs compared sample-by-sample.
   Proves equivalence.
2. **Blob ↔ reconstruction interop** — the two talk to each other through a
   channel simulator. Proves round-trip, not accuracy; a supplement, never a
   substitute.
3. **SpanDSP 3 interop** — independent ground truth, and the only tier that
   distinguishes *correct* from *bug-compatible*. Where the blob and SpanDSP
   disagree the blob wins by default, but the deviation gets recorded in
   `docs/interop.md`.
