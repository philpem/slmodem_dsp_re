# Issue #20: restore the counter-connected cDATAtx.c boundary

Baseline: `f8cdcbef`. This extracts `_tx_scrambled_ones_init`,
`_tx_scrambled_ones_state`, `_tx_data_state` and their shared static
`cDATAtx_counter` from `class1tx.c` into `cDATAtx.c`. Function bodies and
their detailed comments are retained; no compiler flags, wrappers, padding
or visibility attributes are introduced.

## Ownership evidence and scope

Reference FILE symbol 567 (`cDATAtx.c`) owns LOCAL OBJECT symbol 568
(`cDATAtx_counter`, four bytes at `.bss+0x8c0`). The complete relocation
scan has eight references: one from init, five from scrambled-ones state,
and two from data state. Reproduce with:

```sh
python3 tools/relocscan.py ref/slmodemd/dsplibs.o --at .bss:0x8c0
```

The extracted functions follow reference address order: `0x9cf70`,
`0x9d200`, `0x9d4b0`. This recovers the counter-connected subset; it does
not assert that every original function belonging to this FILE is recovered.
All helper dependencies use existing shared declarations and definitions.

## The split restores calls, not just a filename

Both states previously inlined `_handle_data_input` because it was visible
in `class1tx.c`. Each reference state calls it once. The extracted states
now also call it once; their PC-relative call target multisets match the
reference, including the diagnostic calls formerly expanded by inlining.

| Function | Reference bytes | Before | After |
| --- | ---: | ---: | ---: |
| `_tx_scrambled_ones_init` | 193 | 209 | 209 |
| `_tx_scrambled_ones_state` | 681 | 910 | 661 |
| `_tx_data_state` | 618 | 890 | 637 |

Neither changed state becomes exact. The full affected inventory remains
44 functions, with no missing, new or duplicated functions. The other
42 functions, including the moved init and retained helper, are unchanged
under `byteident.py`'s byte/relocation-target comparison.

## Independent measurements

| Dimension | Before | After |
| --- | ---: | ---: |
| Exact functions / 1,852 | 828 | 828 |
| Exact function bytes / 720,125 | 79,916 | 79,916 |
| Positioned matching bytes / 943,398 | 55,398 | 55,313 |
| Exact relocation records / 18,317 | 952 | 955 |
| Exact symbol records / 2,907 | 247 | 248 |
| Exact section descriptors / 92 | 67 | 67 |
| Candidate text bytes | 685,368 | 684,872 |
| Candidate NOBITS bytes / 2,836 | 2,812 | 2,812 |
| Manifest inputs | 263 | 264 |
| Ordering candidates | 180 | 181 |
| Unresolved inputs | 83 | 83 |

There are zero exact-function gains or losses. Shared binding agrees for
2,442/2,442 names, with 76 reference-only and 131 candidate-only names.
The strict partial comparator still exits 1 (`DIFFERENT`).

The 85-byte positioned-match loss and 496-byte increase in the total text
deficit are disclosed rather than hidden by the other gains. Retention rests
on direct local ownership and restored reference call boundaries, not on
optimizing a scalar placement or size score.

## Gate and reproducibility

The reviewed `/tmp/issue20-gentoo partial candidate` and `phase candidate`
runner uses the established Gentoo GCC 3.4.2-r2 profile with
`DSPLIB_REPRODUCE_BUGS`. `make phase` reports **375 period tests passed,
0 failed**, with all structural checks passing. `byteident.py --ratchet`
passes and the exact-symbol sets were compared explicitly.

No mutation-suite entries referred to the moved source/functions, so no
fixture routes changed. The snapshot reports 0 current, 264 stale and
0 never recorded out of 264 registered suites; no fresh mutation result
is claimed. Local artifacts are in `/tmp/issue20-datatx-split-evidence/`.

Issue #6 remains closed for its ordering/measurement infrastructure. Issue
#20 still needs the remaining ownership, definition order, data/alignment
and code-generation work; strict object identity has not been reached.
