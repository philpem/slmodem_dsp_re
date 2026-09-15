# Combined V.32 and V90Parameters full-object scope

## Scope and fixed baseline

This bounded scope copied the retained 275-object manifest into three isolated
directories and replaced no more than the named objects:

| arm | replacement objects |
|---|---|
| `v32-only` | `src_pump_v32_v32seq.c.o` |
| `params-only` | `src_pump_v90_V90Parameters.cpp.o` |
| `combined` | both |

No source was compiled.  Each 275-entry manifest was linked by
`tools/toolchain/partiallink.sh` with explicit
`TC_IMAGE=ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, selecting the
published period binutils 2.15 linker without any shape-check workaround.

Before linking, the current retained artifacts were copied into this scope:

- Baseline partial SHA-256:
  `c35b961d4b51ed671a535c4a77e875778fd6dfc7439006f31a93e3e8879177f6`.
- Baseline byteident JSON SHA-256:
  `ff4990bb08efd18d85405802bb64d45b60a974770a52bec1fb4a987195da2b69`.
- Baseline census: 1,852 compared, 831 EXACT, 48 REGALLOC, 81 BYTES,
  888 SIZE, 4 UNRESOLVED, 0 RELOC.

The copied files are `baseline-dsplibs.o` and `baseline-byteident.json`.

## Owner-object surface

The V.32 replacement changes only `InitGenSequence`, from 71 to 68 bytes.  It
is `EXACT` against the 68-byte blob body.  Its canonical relocation set is
empty before and after.  The owner object's non-text contents and all
relocations are unchanged; the only symbol-record difference is this global
function's size.  Name, binding, visibility, section and value stay fixed.

The V90Parameters replacement changes only
`V90Parameters::loadModemParamsData()`, from 332 to 344 bytes.  Against the
344-byte blob it moves from `SIZE(12)` to `BYTES(159)`: this is a size closure,
not an exact-function closure.  Its fourteen relocation targets and types are
unchanged, although instruction offsets move within the body.  Non-text is
unchanged.  The target symbol retains global/default binding and its start;
three following functions retain their bodies and bindings but their values
move by the linked alignment consequence.  Full before/after canonical
relocations and symbol records are in `results.json`.

Thus the measured change surface is exactly two bodies in the two replacement
objects.  No other owner-object body changes.

## Full byte-identity census

The canonical full CLI was run on `combined` only, with `BLOB` and `TC_OUT`
set explicitly.  It reports:

| metric | retained | combined |
|---|---:|---:|
| compared | 1,852 | 1,852 |
| EXACT | 831 | 832 |
| REGALLOC | 48 | 48 |
| BYTES | 81 | 82 |
| SIZE | 888 | 886 |
| UNRESOLVED | 4 | 4 |
| RELOC | 0 | 0 |

Exact gain: `InitGenSequence`.  Exact losses: none.  The two SIZE removals are
`InitGenSequence -> EXACT` and
`V90Parameters::loadModemParamsData() -> BYTES`; hence the one extra BYTES
row is expected and is not a regression hidden by the exact count.

## Partial-link comparisons

Against the retained partial:

| arm | raw bytes | content bytes | positioned equal | differing/missing | size delta | section records | exact section contents | exact relocations | exact symbols |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `v32-only` | 1,181,716 | 897,621 | 897,590 | 31 | 0 | 99/99 | 97/99 | 17,144/17,144 | 2,958/2,959 |
| `params-only` | 1,181,748 | 897,637 | 396,650 | 500,971 | +16 | 98/99 | 95/99 | 7,015/17,144 | 1,519/2,959 |
| `combined` | 1,181,748 | 897,637 | 396,646 | 500,975 | +16 | 98/99 | 95/99 | 7,015/17,144 | 1,519/2,959 |

The large positioned deltas for the Parameters arm are the expected
downstream address/relocation movement after `.text` grows and aligns by 16
bytes; they are not additional changed bodies.  Counts remain exactly 99
sections, 17,144 relocations and 2,959 symbols in all arms.

The clean marginal V.32 comparison, `params-only -> combined`, isolates the
interaction: equal raw and content sizes, 99/99 section records, 97/99 exact
section contents, 17,144/17,144 exact relocations, 2,958/2,959 exact symbols,
and only 31 positioned content bytes differ.  Therefore the V.32 closure adds
no relocation, binding or layout interaction when combined with Parameters.

Against the blob, the principal aggregate partial-link figures are:

| arm | positioned equal | differing/missing reference | candidate content delta | exact section records | exact relocations | exact symbols |
|---|---:|---:|---:|---:|---:|---:|
| `v32-only` | 68,596 | 874,802 | -45,777 | 69/92 | 976/18,317 | 301/2,907 |
| `params-only` | 68,241 | 875,157 | -45,761 | 69/92 | 967/18,317 | 301/2,907 |
| `combined` | 68,244 | 875,154 | -45,761 | 69/92 | 967/18,317 | 301/2,907 |

At fixed Parameters layout, adding V.32 recovers three positioned blob bytes,
exactly its removed `movzwl`; it introduces no exact-set or surface loss.
Raw linked SHA-256 values are `9b46f171...` (`v32-only`), `091989d3...`
(`params-only`) and `bca7c6e1...` (`combined`), with full hashes retained in
`results.json`.

## Conclusion

The V.32 `int top` candidate is clean at full-object scope: one exact gain,
no loss, and no non-local relocation, binding or layout effect.  The
Parameters replacement composes without interaction, but its status must be
stated narrowly: it closes the target's 12-byte size gap and converts SIZE to
BYTES, while 159 target bytes still differ and it contributes no exact gain.

Machine commands, replacement hashes, owner surfaces, all three blob and
baseline partial comparisons, and the combined exact-symbol set are in
`build/v32-params-scope/results.json`; canonical CLI output is in
`combined-byteident.log`.  No versioned source, gate, or git operation was
performed.
