# V8 DFT sample traversal and read age

`v8_dftupdate` now advances its sample pointer once per outer iteration and
captures each sample after publishing the current bin's phase. Both changes
restore independently observed native operations. The sample capture remains
inside the bin loop, so each bin obtains its sample at the same boundary.
The function remains nonexact; this is a partial source recovery.

## Native witnesses

The reference publishes `bin->phase` at 0x78b46, reads the current sample
through `(%edi)` at 0x78b5c, and advances that sample pointer by two bytes at
0x78b8a. The baseline instead reads `samples[j]` before updating the bin's
phase and retains indexed sample addressing.

The older F11650 cursor experiment, replayed by
`tools/playbook_v8_dftupdate_cursor.py` at `379d400b`, restored direct sample
addressing but did not recover the whole body. Its sample read still preceded
phase publication. The present four-cell cross at `94caa17be` independently
separates traversal from this newly established read-age boundary. There is
no declaration/register palette, type change or forced allocation constraint.

The first RTL dump already places the sample load before the phase store in
baseline and cursor-only cells, and after it in both post-phase cells. The
machine detector accepts the native order and refuses the old order. Its
separate addressing check accepts native/cursor cells and refuses indexed
cells. Only the combined cell passes both native checks. These controls show
what each change recovers before register allocation; length alone does not
select the source.

## Four-cell complete-TU result

| Cell | Bytes against native 164 | Read after phase publication | Direct sample load |
| --- | ---: | --- | --- |
| Baseline | 149, SIZE(15) | no | no |
| Cursor only | 171, SIZE(7) | no | yes |
| Post-phase read only | 149, SIZE(15) | yes | no |
| Both, retained | 171, SIZE(7) | yes | yes |

These are current measured lengths, rather than the historical experiment's
150/172-byte pair. All three TU functions are reviewed at each endpoint.
Only `v8_dftupdate` changes. `v8_dftenergy` and `v8_cosread` retain their raw
bodies and canonical relocations; the exact set stays two of three. The
single named cosine table, all nontext bytes and section metadata, nontext
relocations, strong exports and resolved imports remain unchanged.
The remaining seven-byte length difference is not a seven-byte instruction
distance. Entry/layout, register scheduling and cosine-index lowering still
differ; no further local spelling is selected merely to close that gap.

## Reproduction and actual inputs

`tools/recovery_v8_dft_read_age.py` pins source to `94caa17be` and reproduces
the frozen canonical baseline object literally before interpreting cells.
It uses the complete frozen `.build-config`, recovered Gentoo GCC 3.4.2-r2
and its selected assembler, with the shared helper appending the mandatory
`DSPLIB_REPRODUCE_BUGS` define after configurable flags. Complete commands,
compiler dumps, source/object hashes, three body verdicts and relocations,
metadata and detector results are retained for every cell.

The replay checks the actual contents of `include/dsplib/v8.h` and
`tools/toolchain/period_compat.h` against the pinned revision. Neither header
includes another file. For every cell, an additional compiler `-M` invocation
uses the same input and include flags and must resolve exactly that cell's
source, `/src/include/dsplib/v8.h`, and
`/src/tools/toolchain/period_compat.h`. Thus intended header hashes are backed
by actual compiler-resolved dependency paths. The replay uses no shadow
header overlay. `--check-adopted` additionally requires the selected source
to equal the production source.

## Validation and boundaries

The selected object first passed `t_v8sig` in separate period output storage.
After approval, the production source was changed and a normal focused period
run again passed one binary with zero failures. Its production source object
is raw-identical to the final selected replay object.

The existing direct DFT group passes 1081 checks. It uses initialized arrays
of eight bins and sixteen samples, crossing bin counts zero through eight
with sample counts zero through sixteen in steps of four. The zero-bin cells
therefore retain a valid sample array while the outer cursor advances; the
zero-sample cells skip processing. Both loop indices and both formal counts
remain signed shorts, with the same comparison boundaries as before. No
unsafe large count, null sample buffer or overlapping bin/sample history is
claimed reachable or added as a fixture. The native store/read order itself
is the source-fidelity evidence, rather than an invented alias test.

No mutation suite directly targets `src/v8/V8Dftc.c`. The three suites using
the existing signal binary (`v8agc`, `v8detector`, `v8sig`) were checked
mechanically: 13 locators remain unique and valid, with zero displaced
operations, registry changes or mutation executions. No fixture, assertion,
amplitude or tolerance changed. The parent runs the complete phase gate.

```sh
python3 tools/recovery_v8_dft_read_age.py --check-adopted \
  --objects build/v8-dft-boundary-baseline --out /tmp/v8-dft-read-age
make period T=t_v8sig J=8
python3 tools/anchorcheck.py v8agc v8detector v8sig
```
