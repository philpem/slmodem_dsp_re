# Issue #20: allocated BSS rather than COMMON

Baseline: merged master `89caf465`, tree-identical to the previously gated
`cf3eb770`. This follow-up changes only the initialization of three existing
definitions. It does not add symbols, rename translation units, change compiler
flags, or insert padding.

## Evidence and retained change

The reference defines these GLOBAL objects in `.bss`; the baseline emits them
as COMMON. Explicit zero initialization makes the period compiler allocate
them in `.bss`, matching the reference's storage class.

| Symbol | Size | Reference BSS offset | Candidate BSS offset |
| --- | ---: | ---: | ---: |
| `SGD_CTL` | 8 | `0x8c8` | `0x8a0` |
| `SMCv32_CFG` | 4 | `0x188` | `0xaf8` |
| `Control_Flag` | 4 | `0x1a0` | `0xaf4` |

The offsets still differ. In particular, the two V.32 definitions are not yet
in reference order or position. This change does not establish their original
translation-unit ownership or solve their alignment.

## Measurements

Built with the reviewed `/tmp/issue20-gentoo partial candidate` runner and
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, image
`sha256:fe868cc44a48c36d1130862965729d0b384bc9e0a1908d37891aa5ae07352f16`.
The compiler is Gentoo GCC 3.4.2-r2; the reconstruction build enables
`DSPLIB_REPRODUCE_BUGS` and retains the established flags.

| Dimension | Before | After | Reference denominator |
| --- | ---: | ---: | ---: |
| Positioned matching content bytes | 55,398 | 55,398 | 943,398 |
| Exact relocation records | 952 | 952 | 18,317 |
| Exact symbol records | 247 | 247 | 2,907 |
| Exact section descriptors | 67 | 67 | 92 |
| Exact section contents | 60 | 60 | 92 |
| Candidate NOBITS bytes | 2,804 | 2,812 | 2,836 |
| Exact functions | 828 | 828 | 1,852 compared |
| Exact function bytes | 79,916 | 79,916 | 720,125 compared |

The exact-function sets have zero gains and zero losses. The affected objects'
`.text` sections are unchanged byte for byte: `Sgd.c` is 1,780 bytes, and
`v32fptab.c` has no text. Shared-name binding still agrees for 2,441/2,441
names, with zero disagreements; 77 reference-only and 132 candidate-only names
remain. The strict partial comparator still exits 1 with `DIFFERENT`.

`/tmp/issue20-gentoo phase candidate` passes: **375 period differential tests
passed, 0 failed**, and all structural checks pass. The mutation snapshot
reports 0 current, 264 stale and 0 never recorded out of 264 registered suites;
this change does not claim a fresh mutation run or change mutation fixtures.

Although the three objects total 16 bytes, the NOBITS section grows by only
8 bytes. Its remaining 24-byte size difference cannot be decomposed by adding
symbol sizes alone: internal alignment and input ordering matter. Equal
section alignment does not exclude internal padding differences.

## Remaining work

Issue #6's reproducible ordering and independent measurement infrastructure is
merged and the issue is closed. Full original-order recovery remains work for
#20, not a claim established by closing #6.

- The current manifest has 263 inputs, 180 ordering candidates and 83
  unresolved inputs. Candidate FILE records number 263 against 281 reference
  records. These are measurements, not a one-FILE-per-original-TU proof.
- The first FILE-stream mismatch is reference `V92MappingParamsInt.cpp`
  versus candidate `V92Modem.cpp`. Establish ownership before moving or
  renaming definitions merely to match the label.
- The earliest function-position mismatch is not itself an ordering defect:
  `CID_process` starts at `0x470` in both objects but is 386 versus 368 bytes.
  The following `vce_hook_on` starts at `0x600` versus `0x5e0`. Recover the
  code generation; do not conceal the discrepancy with artificial padding.
- Recover data ownership, definition order, alignment and remaining storage
  differences. The reference's 16-byte `default_voice_configuration` is still
  absent by name; its source ownership has not been established.
- Preserve the period differential and exact-function set while closing the
  remaining content, relocation, section and symbol-record differences.

Local evidence is under `/tmp/issue20-bss-evidence/`; the reproducible comparison
commands remain `make partial-compare`, `byteident.py --ratchet`, and
`partialcmp.py --require-exact`. The latter must pass before #20 is complete.
