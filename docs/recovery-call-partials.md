# CID and Tone IIR partial graph recovery

Three non-exact bodies now retain independently witnessed instruction graphs:
`CID_FSD_demodulate`, `toneiir_progress` and `_iir_filter_progress`. The project
owner explicitly authorised supported partial reconstruction on 2026-10-16.
This changes the adoption condition used by the historical
[CID wrap domain](recovery-cid-terminal.md),
[Tone interval domain](recovery-toneiir-interval.md) and
[shift-owner control](batch20-toneiir-shift-capture.md).
Their finite search domains remain closed. No additional source palette or
compiler option was searched.

## Original witnesses

CID correlation wrap uses the first loop's terminal EBX in `lea 0xa(%ebx),%ecx`
at 0x9234b and 0x92544, in separate 7200/9600 arms. Low-pass wrap uses
`lea 0x22(%ebx),%ecx` at 0x923d8. The adopted source preserves those terminal
cursor dependencies and separate coefficient arms. Starting at `base+index+1`,
the first loop's predecrements finish at `base`; adding the extent yields the
legal one-past pointer. The second loop finishes at `base+index+1`.
This proof assumes the component's ring indices (0..4 and 0..16).

Both Tone IIR engines contain four expanded sections in the reference.
Their tap counters narrow to sixteen bits and compare against 2: examples
include 0x7c52b/0x7c55c and 0x7cb24/0x7cb27. The helper computes its shift
array owner with `lea 0xc8(%esi),%edx` at 0x7ca83, then spills it at 0x7ca90.
The adopted source restores these expanded graphs, short tap counters and
captured shift owner. This does not establish whether the author's original
source was explicitly expanded or the compiler unrolled it. Current int
interval storage, its full-width cadence producer, envelope SAR, modular
products, state updates and configuration objects are preserved.

## Current-profile controls and collateral

The frozen baseline is `eccf0f2a`, from
`source-recovery-next/build/recovery-fields/baseline`. Both unchanged full TUs
reproduce their retained objects byte for byte using current headers and the
complete saved Gentoo flags. The shared experiment helper appends
`DSPLIB_REPRODUCE_BUGS`. Gentoo GCC 3.4.2-r2 selects and executes GNU assembler
2.15.92.0.2 (20040927).

| Body | Current baseline bytes | Adopted bytes | Reference bytes |
| --- | ---: | ---: | ---: |
| CID_FSD_demodulate | 795 | 1022 | 1049 |
| toneiir_progress | 673 | 1101 | 1141 |
| _iir_filter_progress | 306 | 799 | 802 |

The fixed expanded-short control without shift capture emits 782 helper bytes
and 1101 tone-progress bytes. Its exact set agrees with the adopted control.
Size gaps are absolute length differences, not differing-byte counts; the
adoption evidence is the original operand graph and differential validation.
All three bodies remain non-exact.

Five complete TUs (two CID controls, three Tone controls) retain identical
non-text data, relocations, symbol inventories, binding and visibility.
The CID TU has one function and three named data objects. The Tone TU has
eight functions; only its two progress bodies change. Constructor, reset,
delete and default-configuration bystanders retain their canonical bodies.
Text relocation positions change in CID while coefficient targets remain the
same. Exact gains and losses are both zero. A deliberately corrupted data
byte fires the collateral detector (1/1). No header changed, so the source
consumer scope is these two TUs. Independently compiled period-test objects
reproduce all nine final canonical function bodies and relocations.

Artifacts under `build/recovery-fields-call` contain actual compiler commands,
current header hashes, source/object hashes, raw controls, RTL, complete
changed-body disassemblies and `results.json`. The committed
[machine record](recovery-call-partials-results.json) preserves commands, hashes
and all five per-function verdict sets. The initial denied Docker
connection is an invalid operational attempt; only the successful authorised
compiler run is interpreted.

## Validation and replay

The focused deciding period run passed **11 binaries, zero failures**:

```sh
make period J=4 T='t_cid_fsd t_cid t_cidprog t_cidsvc t_cidleaves t_cid_mtd t_toneiir t_cadence t_detector t_voicesvc t_callprog'
```

These are component differential fixtures. Seeded internal verdict paths do
not establish public modem-call reachability. No fixture, tolerance, mutation
semantics or compiler flag changed. Neither changed source file has a
registered mutation descriptor. The static anchor gate passes with zero broken
anchors; its complete denominator is in `build/fields-call-anchors.log`.
The parent integration owns full `make phase`, whole-object exact census and
any broader portability claim.

Replay the five fixed controls from the repository root:

```sh
python3 tools/recovery_call_partials.py \
  --domain docs/recovery-call-partials.md \
  --baseline-dir /absolute/path/to/frozen/baseline
```

The field audit also closed narrow-storage negatives without source changes:
`cadence.threshold` +0x294 has a word store at 0x7d4c4 and word reads at
0x7d721/0x7d988; calling-tone phase +0 and amplitude +0xc have word stores and
signed word reads; call-progress `dialtone_seen` +0x80 has byte writes at
0x795fe/0x7a129 and a byte test at 0x79bd9. FDSP channel +0x1688/+0x1690
likewise have narrow stores, with no independent wide-reader witness found.
These observations do not justify promoting adjacent padding into the fields.
