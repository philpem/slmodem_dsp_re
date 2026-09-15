# Issue #20: Callprog, V.8, modulus and prefilter ownership groups

Baseline: `fd21a97e`. This batch changes source ownership and definition
ordering, not statements or compiler flags to chase byte matches.

## Retained moves

- Merge `callprog_status.c` into `Callprog.c`: the reference's surviving
  LOCAL `message_names` belongs to `Callprog.c`. Preserve both tables and
  the status accessor. Public emission order is now the reference's:
  `Dialer_IsDialStringInvalid`, `CALLPROG_Delete`, `CALLPROG_Status_string`,
  `CALLPROG_Create`, `CALLPROG_Progress`, `CALLPROG_Dial`.
- Restore `V8Dftc.c` with `v8_costbl`, `v8_dftupdate` and `v8_cosread`.
  The reference's three relocations to `.rodata+0x5740` are two from
  `v8_dftupdate` and one from `v8_cosread`; the LOCAL table belongs to
  `V8Dftc.c`. Other helpers remain in their existing units rather than
  being moved on address adjacency alone.
- Split the `ModulusDecoder` and `ModulusEncoder` families into
  `V90ModulusDecoder.cpp` and `V90ModulusEncoder.cpp`.
- Separate the three prefilter coefficient families into
  `V90PreFilterCoeffType1.cpp`, `V90PreFilterCoeffType2.cpp` and
  `V90PreFilterCoeffType3.cpp`; restore `V90PreFilterRefLoops.cpp` for
  the six loop families and dependent codec table.

The C++ filename correspondences are supported by the respective class/data
families. They are not promoted to the stronger direct LOCAL-owner evidence
used for the Callprog and V.8 moves. Empty compatibility translation units
are not retained in the reconstruction manifest.

## Complete affected-inventory check

The seven original inputs and ten resulting inputs contain the same 45
functions, with no missing or duplicate function names. Against the old
objects, `byteident.py` reports 42 unchanged functions and these three
non-exact emission changes:

| Function | Before bytes | After bytes | Reference bytes |
| --- | ---: | ---: | ---: |
| `CALLPROG_Progress` | 2,934 | 2,934 | 2,929 |
| `ModulusDecoder::progress` | 451 | 452 | 534 |
| `v8_dftupdate` | 190 | 150 | 164 |

The Callprog body has 74 non-relocated byte differences under the comparator;
the other two differ in size. Their source statements are retained and
none was exact before the move. These remaining body differences are not
refined in this ownership batch. All ten prefilter data objects are identical
to their pre-split values and sizes, including the complete codec table.
This is a comparison with the prior reconstruction, not a claim that every
prefilter object is already identical to the blob.

## Gates and independent measurements

The reviewed Gentoo GCC 3.4.2-r2 partial build and phase runner preserve the
established flags and `DSPLIB_REPRODUCE_BUGS`. The fresh results are:

- Period differential: **375 passed, zero failed**; structural checks pass.
- Exact-function set: **828/1,852**, with zero gains or losses and
  **79,916/720,125** exact function bytes.
- Positioned matching bytes: **56,129 -> 56,970 / 943,398**.
- Exact relocation records: **962 -> 991 / 18,317**.
- Exact symbol records: **253 -> 261 / 2,907**.
- Exact section descriptors: **67/92**, unchanged.
- Shared-name binding: **2,443/2,443**, unchanged, with zero disagreements.
- Inputs: **264 -> 267**; ordering candidates **186 -> 192**;
  unresolved inputs **78 -> 75**.

Candidate text falls by 32 bytes to 684,840, increasing the overall content
deficit from 45,332 to 45,364 bytes. Candidate NOBITS remains 2,812 versus
2,836 reference bytes. The partial comparator still reports `DIFFERENT`;
improved placement is not whole-object identity.

## Mutation coverage survives the source split

All original entries are preserved. The two modulus suites are divided by
the unique source containing each unchanged anchor. The fresh run is:

| Suite | Cases caught by tests |
| --- | ---: |
| `moduluscoder` | 10/10 |
| `moduluscoder_encoder` | 13/13 |
| `moduluscoderstd` | 4/4 |
| `moduluscoderstd_encoder` | 5/5 |
| `v8sig` | 8/8 |
| `callprog` | 17/17 |

Total **57/57 caught**, zero uncaught, unusable, equivalent or miscounted.
The phase snapshot reports **6 current, 260 stale, zero never recorded** out
of **266 registered** suites. No new differential-test binary is introduced.

## Invalid extraction run and corrected control

The first prefilter extraction had an extra array terminator and truncated
the codec initializer. That partial build failed. Its comparison output
used a stale linked object and an incomplete object directory, so those
results are invalid, not a baseline or a retained measurement.

All three coefficient declarations were re-extracted intact from committed
source, and the complete reference-loop/codec source was restored under the
canonical filename. The fresh partial build, ten-object data comparison,
57-case mutation run and phase gate above all followed that correction.
Artifacts are under `/tmp/issue20-ownership-final-batch/`; the initial run
is separately labelled `invalid-extraction/` and excluded.

Issue #20 remains open: the remaining directly evidenced owner conflicts,
unresolved inputs and definition-order classification listed in
`issue20-ownership-order-pass.md` are not complete. This batch does not
claim that the full ownership/ordering phase is ready to close or that
byte-fidelity refinement should begin.
