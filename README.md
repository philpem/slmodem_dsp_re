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

Abbreviations are decoded in **[docs/glossary.md](docs/glossary.md)** — worth
reading first, since several are misleading (`MRF` is *multi-rate filter*, not
"matched root filter"; `FP_Pow` computes `exp()`).

Datapump configuration — what each config field means and how to ask for an
originating or answering station — is in
**[docs/configuration.md](docs/configuration.md)**.

Start with **[docs/findings.md](docs/findings.md)**: what the blob is, how it
is structured, and the commands that prove each claim.

## Status

**V.90 / V.92 is the end goal.** V.34 is being done first because it is the
base V.90 builds on, not for its own sake.

| phase | content | state |
|--:|---|---|
| 0 | tooling, TU map, differential harness | **done** |
| 1 | core plumbing (`dp_wrapper`, `FixedRC`, `dp_param`, `FP_math`) | **done** |
| 2 | Bell 103 / V.21 — *first real connection* | **done** — connects and carries data at BER 0 |
| 3 | call progress / dialler — *originate as well as answer* | **done** — `src/callprog/`, `src/dialer/`, `src/call/`; one caveat below |
| 4 | V.23 | **done** — all six modules at 1200/75 bps, interop against SpanDSP's `fsk.c` |
| 5 | V.8 negotiation | **done** — negotiates against SpanDSP over a socket; `MEMORYC.c` identified here (finding 37) |
| 6 | V.22 / V.22bis / Bell 212 | **not started.** The `fpm_*` fixed-point framework this row also covered was pulled forward into phase 2 and *is* done (finding 16) |
| 7 | V.32 / V.32bis | **not started** |
| 8 | remaining services (CID, DTMF, ring detect, voice, beep) | **not started** |
| 9 | fax Class 1 (V.17 / V.27ter / V.29) | **not started** |
| 10 | V.34 | **in progress** — the fast pass; see [docs/fastpass.md](docs/fastpass.md) |
| 11 | V.90 / V.92 — *the end goal* | **not started**, beyond the 16,003 bytes of `VPcmV34Main.cpp` that `v34handshak` reaches (finding 215) |
| 12 | 8 kHz retarget | **not started** |

Run `make coverage` for the live figure rather than trusting a number written
here; it reports translated bytes, what fraction of them a test drives against
the blob, and what is left by translation-unit span.

**Phase 3's caveat.** The phase's own milestone is met, but the `Dialer.c`
span still holds unwritten bytes. That span brackets nineteen translation
units, so what those bytes belong to is not attributed — some of it is
probably phase 8 services sharing the range rather than dialler code.

**Two numbering traps.** Task subjects `#33`–`#47` used to be labelled "Phase
6a" through "Phase 6z" for what is V.34 work — phase **10** here, phase 6
being V.22. They now read "Phase 10a" and so on, but older findings and
hand-over notes still say "phase 6d", "phase 6g–6m" and the like, and those
mean V.34. Separately, a second task store exists whose `#11`–`#22` are
different tasks from this one's; `docs/fastpass.md` holds the mapping. Say
which store you mean when quoting a task number.

### Phase 2 detail

| module | state |
|---|---|
| `fpm_iir` | `FPM_iir_filt` (form II) and `FPM_iir_filt_II` (form I) complete; `FPM_iir_filt_block` pending |
| `fpm_mrf` | complete — the 7200↔8000 and 8000↔2400 converters |
| `fpm_fsm` | complete — FSK modulator |
| `fpm_mtd` | complete — multi-tone detector |
| `fpm_tone` | complete except `FPM_TONE_find_rev` / `_kill` |
| `fpm_fsd` | complete — Schmitt slicer and edge-resynchronised bit clock |
| `fpm_agc` | complete — block AGC and noise gate |
| `b103fp` | complete — signal path, seven Hdx states, three NextState tables, `B103FP_modem`, `create` and `delete` |
| primitives | `FPM_phasor`, `FPM_phasor_demod`, `FPM_sqrt`, `FPM_sqrt_dp`, `FPM_rms`, `FPM_div`, `FP_math` — complete |
| `b103` | complete — registration, `b103_create`, `b103_delete`, `b103_process` |

Full plan, including rationale for the ordering:
`~/.claude/plans/the-directory-slmodemd-contains-compiled-lark.md`

## Bug compatibility

The reconstruction fixes a defect found in the original **only** where leaving
it in would break a working modem, and every such fix sits behind
`DSPLIB_REPRODUCE_BUGS` so bit-exactness stays provable:

```sh
make test      # -DDSPLIB_REPRODUCE_BUGS: proves equivalence with the blob
make interop   # without it: proves the fixed build is a working modem
```

There is currently one, **D4** — `FPM_div` reading one past its table and
handing the AGC a zero gain, which silences a block and drops a Bell 103 call
on 6.6% of blocks. Everything else the original gets wrong is reproduced
faithfully and recorded in [docs/deviations.md](docs/deviations.md).

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
| `tools/relocscan.py` | resolve relocations to the objects they point at |
| `tools/eddecode.py` | decode the `$!$ `/`????` diagnostic channel in a log |

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
