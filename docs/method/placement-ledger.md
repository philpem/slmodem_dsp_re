# Causal placement ledger (phase 3)

`partialcmp.py` measures how different the recovered-order partial link is. It
does not say why a function moved. `tools/toolchain/placementledger.py` walks
the executable symbol sequence of the blob and the linked candidate and
attributes each positional delta to a predecessor term.

    make placement-ledger                 # needs `make partial-link`
    make placement-ledger-selftest
    python3 tools/toolchain/placementledger.py --limit 40 \
        --json build/partial/placement-ledger.json

## What it reports, and the denominators

Per symbol: blob start and size, candidate start and size, cumulative delta
(candidate start - blob start), the step since the previous matched symbol,
the immediate predecessor on each side, and the terms that explain the step
(body-size change and padding change).

It prints the denominator every time:

- blob and candidate `.text` FUNC counts, section extent and FUNC-covered
  bytes (they are not the same number);
- symbols compared by name, of which sequence-aligned and reordered;
- names present on only one side, split into "defined elsewhere in the other
  artifact" and absent;
- manifest definition count and multiply-defined names.

`.text` is the primary sequence because blob and candidate offsets are
directly comparable there. A function that is in `.text` on one side and a
COMDAT section on the other is reported under category (e), not silently
paired.

A missing, empty or non-ELF artifact, or one with no `.text` FUNC symbols, is
an error (exit 2). The tool never prints a clean zero for an absent input.

## Categories

Each non-zero step is classified as one of:

| category | meaning |
|---|---|
| (a) wrong selected body | the predecessor is multiply-defined/COMDAT and the chosen body differs in size or bytes |
| (b) wrong TU/emission order | the candidate's immediate predecessor is not the blob's |
| (c) upstream size drift | a single-definition body differs in size; the padding note says whether alignment absorbed it |
| (d) padding/alignment | only the gap between same-size bodies differs |
| (e) data/binding/layout | a name or section exists on one side only, or a step has no body-size and no padding term |

`--self-test` carries a known positive for each category that can be
synthesised, a negative for identical sequences and for a same-size body
change, refusal controls for missing and empty artifacts, and a real-artifact
load that reports the measured first shift without hard-coding its value. It
is 13 checks today, 0 failed.

## First run (master 9768a45b2)

```
blob:      1852 FUNC, 1773 .text, section 728304 bytes (713824 covered by FUNC symbols)
candidate: 1881 FUNC, 1796 .text, section 711015 bytes (697040 covered by FUNC symbols)
symbols compared (common .text names) 1773 (sequence-aligned 1538, reordered 235)
blob-only .text 0; candidate-only .text 23
manifest definitions 1881 names, 32 multiply-defined
matched with a moved start 1764, unmoved 9
cause histogram: order 366, size-drift 430, body-content 2
```

The first positional divergence is `vce_hook_on`:

| symbol | blob | candidate | delta |
|---|---:|---:|---:|
| `dcr_process` | 0x100 / 568 | 0x100 / 567 | 0 |
| `CID_create` | 0x340 / 195 | 0x340 / 195 | 0 |
| `CID_delete` | 0x410 / 84 | 0x410 / 84 | 0 |
| `CID_process` | 0x470 / 386 | 0x470 / 368 | 0 |
| `vce_hook_on` | 0x600 / 40 | 0x5e0 / 40 | -32 |
| `vce_hook_off` | 0x630 / 40 | 0x610 / 40 | -32 |
| `vce_get_sreg` | 0x660 / 188 | 0x640 / 188 | -32 |
| `VOICE_create` | 0x720 / 493 | 0x700 / 505 | -32 |
| `VOICE_delete` | 0x910 / 133 | 0x900 / 133 | -16 |

Two things this makes explicit:

- **Not every size drift moves the link.** `dcr_process` is one byte short
  (568 -> 567), but the blob's next symbol starts at 0x340 either way: the
  drift is absorbed by alignment padding (gap 8 -> 9). The ledger reports it
  as (c) with an "absorbed by alignment" note and a zero step.
- **The first real shift is two terms.** `CID_process` is 18 bytes short
  (386 -> 368) and the blob needs 14 bytes of padding before `vce_hook_on`
  while the candidate needs none (the candidate body ends exactly on the
  16-byte boundary at 0x5e0). Step -18 - 14 = -32.

After that the next drift is inside an already-shifted region:
`VOICE_create` +12, `VOICE_command` -18, `VOICE_process` -84,
`FAX_process` -343. Category (b) first appears at `vpcm_create`, where the
candidate order is `vpcm_run, vpcm_create, ...` and the blob order is
`vpcm_create, vpcm_delete, vpcm_run, ...`; the reorder contributes 235
sequence-misaligned names and 366 category-(b) steps. `blob-only .text` is 0;
the 23 candidate-only names are reconstructed helper bodies with no blob
symbol.

## Witness: `CID_process`

The bounded witness chosen from the ledger is `CID_process`, the body that
creates the first positional shift.

Measurements (canonical `build/tc_out`, Gentoo GCC 3.4.2-r2):

| metric | value |
|---|---|
| per-TU grade | **SIZE**, 18 bytes differ, 99 blob / 102 candidate instructions |
| link-selected grade | **SIZE** -- the name has one definition record (`src_service_cid.c.o`), so selection cannot change it |
| manifest copies | 1 |
| predecessor/padding | 386 vs 368 bytes; 14 vs 0 bytes padding |
| `contentgap` | 22 blob-only instructions in 13 hunks (largest 4); 25 ours-only in 10 hunks (largest 7) |

It is therefore **not** (a) a wrong selected body and **not** (d) padding:
padding is a consequence of the 18-byte body gap. It is category (c), a
body-size drift with a distributed instruction difference.

Source forms enumerated against the period compiler (full `CID_process`
extent, same TU and flags):

| form | size |
|---|---:|
| current `if (res == 1) ... else if (res != 0) ...` | 368 |
| nested `if (res != 0) { if (res == 1) ... else ... }` | 329 |
| `if (res == 0) {} else if (res == 1) ...` | 329 |
| `switch (res)` cases 0 then 1 | 328 |
| `switch (res)` cases 1 then 0 | 327 |
| `int res` instead of `short res` | 368 |
| `volatile int ret` | 423 |
| `ret` initialised once outside the loop | 352 |
| loop condition `count > 0 && !ret` | 455 |
| `p`/`i` hoisted to function scope | 368 |
| all loop locals hoisted | 368 |

No tested form maps onto the blob's 386 bytes / 99 instructions, and the
spread (327-455) is wider than the 18-byte gap. The blob's remaining
difference is the shared exit/ret test and basic-block placement rather than a
single missing statement. Fitting further spellings on byte count is the
hill-climb that `docs/method/refinement.md` says to decline. **No source
change was made.**

Expected effect if the body gap were recovered: every symbol from
`vce_hook_on` onward moves +32 bytes, the first divergence disappears, and the
next ledger step is the `VOICE_create` +12 drift already listed above.

## What remains open

- A source fact that reproduces `CID_process`'s 386-byte block layout. The
  measured domain above does not contain it.
- The emission-order cluster behind `vpcm_create`: `recoverorder.py`'s
  ordering candidates do not establish original TU provenance, and changing
  them is not a source fact.
- The 23 candidate-only helper symbols and the 32 multiply-defined names are
  reported by the tool but not acted on here.
