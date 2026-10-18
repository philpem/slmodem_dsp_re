# Bell 103 helper boundaries

Separating four signal-path helpers from the half-duplex callers restores
the reference's calls under the retained Gentoo compiler profile. Two
callers become byte exact: `RxHdxStartB103` (139 bytes) and `TxHdxDataB103`
(54 bytes). There are no exact losses across the 17 shared functions.

## Evidence and ownership limits

The reference's FILE order includes `B103.c`, `B103prc.c`, `B103tab.c` and
`B103int.c`. The local `tx_in_internal` and `rx_out_internal` buffers belong
to `B103prc.c` and anchor the host dispatcher there. They remain in that file.

`ModDataB103`, `TxNoCarrierB103`, `DemodDataB103` and `CarrierDetectB103`
form a consecutive native run at 0x8f9a0 through 0x8fcd0, after the state
functions. The callers contain actual calls to the smaller helpers, whereas
the merged reconstruction inlines them. The existing `B103int.c` FILE slot
is a plausible home for this run, but it has no local symbol that pins these
globals to that filename. The separation is supported by the call graph;
the filename attribution remains an inference.

The baseline's `00.cgraph` explicitly records `CarrierDetectB103` inlined
into both receive callers, `ModDataB103` into the data/marks transmit
callers, and `TxNoCarrierB103` into the silence caller. In the separated
source those definitions are unavailable to this compilation unit. The
first RTL dump retains the calls that the native object contains. No
attributes or compiler-option changes are needed.

## Bounded controls

The replay pins the unchanged source at `a0c079f61` and requires literal
reproduction of both frozen baseline objects. It uses the complete frozen
`.build-config`, Gentoo GCC 3.4.2-r2 and the shared experiment helper's final
`DSPLIB_REPRODUCE_BUGS` define. Commands, source/object hashes, compiler dumps,
all 17 body verdicts and canonical relocations are retained.

Four controls test the merged baseline, separation of carrier detection
alone, separation of all four helpers in existing source order, and
separation in native helper emission order. The carrier-only cell recovers
`RxHdxStartB103`; both four-helper cells also recover `TxHdxDataB103`.
The two helper orders produce the same per-function results. The retained
source follows the witnessed native helper order.

One additional, independently witnessed control changes the receive-data
count local to `unsigned short`. The native caller zero-extends the low word
of `DemodDataB103`'s result at capture and sign-extends it only at the final
short return. A signed local had moved the sign extension to capture time.
The public prototypes and return types remain unchanged.

| Function | Reference | Merged baseline | Separated helpers | Retained unsigned count |
| --- | ---: | ---: | ---: | ---: |
| `RxHdxStartB103` | 139 | 138 | 139, EXACT | 139, EXACT |
| `TxHdxDataB103` | 54 | 106 | 54, EXACT | 54, EXACT |
| `TxHdxMarksB103` | 156 | 203 | 153, SIZE(3) | 153, SIZE(3) |
| `TxHdxSilenceB103` | 130 | 207 | 128, SIZE(2) | 128, SIZE(2) |
| `RxHdxDataB103` | 180 | 177 | 170, SIZE(10) | 171, SIZE(9) |

Numbers are function lengths in bytes; SIZE differences are length gaps.
The larger receive-data gap accompanies recovery of the actual carrier
call and the count's narrowing boundary. Its three call targets
(`DemodDataB103`, `CarrierDetectB103`, `B103NextState`), object fields,
flag updates and consecutive-loss threshold agree with the reference.
Branch layout remains different; the row is still nonexact.

The complete pair retains all 17 function symbols and their strong bindings.
The existing eight exact bodies remain exact. Nontext contents and section
metadata, canonical nontext relocations, exported bindings and resolved
external imports remain unchanged. The four moved helper bodies retain
their reference verdicts. The replay records every changed body, including
changes hidden by an unchanged SIZE grade.

## Validation

The final cleaned source pair literally reproduces the selected experimental
pair. Focused period validation passes four binaries with zero failures:
`t_b103fp`, `t_b103hdx`, `t_b103create` and `t_b103link`. Existing receive
tests report 41,079 half-duplex checks, 120 direct `RxHdxStartB103` checks
and 62,820 demodulator checks; the bit-error-rate fixture remains green in
both directions. No test input or tolerance changed.

Searching the mutation registry by both source paths and case-insensitive
Bell 103 test names finds zero registered suites. There are no mutation
anchors to relocate with these four bodies.

```sh
python3 tools/recovery_b103_helper_boundary.py \
  --objects build/copy293-baseline --out /tmp/b103-helper-boundary
make period T='t_b103fp t_b103hdx t_b103create t_b103link' J=8
```

The replay does not run differential tests. Full `make phase` and strict
partial-link checks remain integration gates; the function gains do not
establish whole-object identity.
