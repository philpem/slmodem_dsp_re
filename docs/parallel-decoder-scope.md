# Parallel decoder shared-header scope

This is the valid 18-consumer scope for the all-cursors decoder candidate. It uses fresh Gentoo compiler dependency discovery, 18 raw-identical controls, the retained compiler flags, and a 275-object manifest-order partial link with explicit `TC_IMAGE`.

## Correction to the earlier valid-form report

The earlier report table correctly showed symbol-record differences, while its prose incorrectly claimed equality. Function size changes necessarily change ELF symbol records. In this scope, every record difference is checked field by field: there are 1 changed record rows, all limited to the expected `size` field of `_ZN27ParallelDifferentialDecoderIhE7processEPhS1_`; names, types, bindings, visibility, sections, and values are unchanged.

## Measured scope

- Fresh affected consumers: 18
- Control raw matches: 18/18
- Manifest/link inputs: 275/275
- Changed canonical bodies: 1 copies, symbol names `_ZN27ParallelDifferentialDecoderIhE7processEPhS1_`
- Actual allocated non-text contents equal: 18/18
- Enumerated function definitions: recorded per object in `results.json`

## Full byte-identity result

The actual `byteident.py` CLI compared the full candidate object directory. Exact symbols: 830 before, 830 after. Exact-name gains: `none`. Losses: `none`.

## Full partial-link metrics

```json
{
  "reference": {
    "path": "/tmp/slmodem-byte-fidelity/ref/slmodemd/dsplibs.o",
    "sha256": "1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379",
    "raw_size": 1233728
  },
  "candidate": {
    "path": "/tmp/slmodem-byte-fidelity/build/parallel-decoder-scope/candidate-dsplibs.o",
    "sha256": "7756fdd9ec5d34054092043e555d514cd07058e5ad906a10d5bbdbda6fe3e3b8",
    "raw_size": 1181716
  },
  "sections": {
    "reference": 92,
    "candidate": 99,
    "common_names": 92,
    "exact_records": 68,
    "ordered_matches": 60,
    "exact_contents": 60,
    "first_difference": [
      0,
      [
        ".text",
        "PROGBITS",
        "AX",
        0,
        728304,
        16
      ],
      [
        ".text",
        "PROGBITS",
        "AX",
        0,
        684472,
        16
      ]
    ]
  },
  "contents": {
    "reference_bytes": 943398,
    "candidate_bytes": 897624,
    "equal_positioned_bytes": 68520,
    "different_or_missing_reference_bytes": 874878,
    "candidate_size_delta": -45774,
    "reference_nobits_bytes": 2836,
    "candidate_nobits_bytes": 2840
  },
  "relocations": {
    "reference": 18317,
    "candidate": 17144,
    "exact_records": 974,
    "ordered_matches": 966,
    "first_difference": [
      16,
      [
        ".rel.text",
        696,
        "R_386_32",
        "dsplibs_debug_level"
      ],
      [
        ".rel.text",
        708,
        "R_386_32",
        "dsplibs_debug_level"
      ]
    ]
  },
  "symbols": {
    "reference": 2907,
    "candidate": 2959,
    "exact_records": 300,
    "ordered_matches": 299,
    "first_difference": [
      4,
      [
        "FUNC",
        "LOCAL",
        "DEFAULT",
        ".text",
        1536,
        40,
        "vce_hook_on"
      ],
      [
        "FUNC",
        "LOCAL",
        "DEFAULT",
        ".text",
        1504,
        40,
        "vce_hook_on"
      ]
    ]
  },
  "raw_exact": false,
  "exact": false
}
```

Artifacts: `build/parallel-decoder-scope/results.json`, `byteident.json`, `byteident.log`, `link.log`, and `toolchain.txt`. No source or full differential gate was run.
