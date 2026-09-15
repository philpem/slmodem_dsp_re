# Scrambler allocation shared-header scope

This valid shared-header scope rebuilds every freshly discovered `Scrambler.h` consumer using the published Gentoo GCC 3.4.2-r2 image. It compares an unchanged overlay and the count-local candidate; neither overlay edits versioned source. The candidate remains a source hypothesis, not a uniquely established original spelling.

## Validity and scope

- Actual compile manifest: 275/275 objects.
- Actual link manifest: 275/275 objects.
- Fresh consumers: 30.
- Raw-identical unchanged controls: 30/30.
- Allocated non-text-equal candidate consumers: 30/30.
- All `Scrambler` and `Descrambler` definitions, including every defining copy and specialization, are inventoried in `results.json`.

## Body and surface result

- Changed canonical bodies: 2 copies across `_ZN9ScramblerIihEC1Ejjj`.
- Symbol-record delta rows: 2.
- Added plus removed relocation records across consumers: 8.
- Full exact-name census: 830 before, 831 after.
- Exact gains: `_ZN9ScramblerIihEC1Ejjj`.
- Exact losses: `none`.

Exactly two defining copies change, in `V90Modulator.cpp` and
`V92Modulator.cpp`; both become exact at 107 bytes. Every other Scrambler
specialization and every Descrambler definition remains body-identical. The
two symbol records retain name, FUNC type, WEAK binding, DEFAULT visibility,
section and value; only size changes. Each copy retains the same two relocation
types and targets at offsets three bytes earlier.

Names, bindings, visibility, sizes, relocation targets, non-text payloads, per-copy verdicts, commands, object hashes, overlay hashes, and complete changed records are retained in `build/scrambler-allocation-scope/results.json`. The canonical CLI output is in `byteident.log`.

## Partial link

The recovered 275-entry `tc_link_manifest.txt` was linked with explicit `TC_IMAGE`; candidate partial SHA-256 is `c35b961d4b51ed671a535c4a77e875778fd6dfc7439006f31a93e3e8879177f6`.

A direct comparison with retained partial
`7756fdd9ec5d34054092043e555d514cd07058e5ad906a10d5bbdbda6fe3e3b8`
finds equal raw size, section count, relocation count, symbol count, and
ordering. Only the constructor COMDAT section, its two internal relocation
offsets, and its symbol size differ; no global layout moves. Full direct
evidence is in `build/scrambler-allocation-scope/retained-partial-compare.json`.

```json
{
  "reference": {
    "path": "/tmp/slmodem-byte-fidelity/ref/slmodemd/dsplibs.o",
    "sha256": "1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379",
    "raw_size": 1233728
  },
  "candidate": {
    "path": "/tmp/slmodem-byte-fidelity/build/scrambler-allocation-scope/candidate-dsplibs.o",
    "sha256": "c35b961d4b51ed671a535c4a77e875778fd6dfc7439006f31a93e3e8879177f6",
    "raw_size": 1181716
  },
  "sections": {
    "reference": 92,
    "candidate": 99,
    "common_names": 92,
    "exact_records": 69,
    "ordered_matches": 61,
    "exact_contents": 61,
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
    "candidate_bytes": 897621,
    "equal_positioned_bytes": 68594,
    "different_or_missing_reference_bytes": 874804,
    "candidate_size_delta": -45777,
    "reference_nobits_bytes": 2836,
    "candidate_nobits_bytes": 2840
  },
  "relocations": {
    "reference": 18317,
    "candidate": 17144,
    "exact_records": 976,
    "ordered_matches": 968,
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
    "exact_records": 301,
    "ordered_matches": 300,
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

No source change, differential gate, or git operation was performed.
