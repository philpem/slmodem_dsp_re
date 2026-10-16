# GCC3 candidate screening and two V34 recoveries

Issue [248](https://github.com/philpem/slmodem_dsp_re/issues/248), baseline
4387a6fb (PR247), isolated branch investigate/gcc3-candidate-screen.
PR245 paths at 17d182af were excluded; its worktree was read only.
No optimization flags were changed and no fuzzing or mutation harness was run.

## Screen and qualification

`tools/gcc3_candidate_screen.py` reads disassembly and reports constrained
division, variable shifts and scalar-width differences. It reports its
scope, missing disassembly and three known controls: exact updateAlpha,
historical stack-reference difference and one divide on each side.
Stack references are not inferred spills; feature hits are not source preimages.
The baseline screen covers 300 objects, 131 in-scope TUs, 583 shared symbols,
340 nonexact functions, zero missing-disassembly refusals and 180 feature
candidates. JSON records revision, complete build config and object hashes.

The best qualified leads were:

| Function | Baseline / blob bytes | Result / next discriminator |
|---|---:|---|
| V34TimingHPFilter | 70 / 76 | EXACT with short index plus in-place multiply |
| V34TimingFiltersInit | 101 / 102 | EXACT with explicit state clears and sizeof history bound |
| V34GiveProbeResults | 105 / 71 | Direct double load plus short index reaches 71 bytes, still BYTES44. Recover typed owner/root guard before another source experiment. |
| VPcmV34GetSNR | 90 / 99 | Divide prefix already matches; inspect old-value loop lifetime, not updateAlpha's spill hypothesis. |

Closed GetFP_Value and setInitialPhase source families were not reopened.

## Declared finite reproductions

All commands use `tools/gcc3_candidate_reproduce.py`, which sources the complete
TU at 4387a6fb and shares the experiment-toolchain driver. Gentoo GCC3.4.2-r2,
its selected assembler, mandatory DSPLIB_REPRODUCE_BUGS and complete command
are recorded. `-da` supplies RTL; this compiler rejects tree-dump requests
(see the parent reload tracing record).

| Family/options | Cells | Result |
|---|---:|---|
| `--family hp` | 2 | Narrow index alone: 76 bytes, BYTES29 |
| `--family init` | 8 | Nested-loop order, index width and split/word clear: no exact cell |
| `--family probe` | 4 | Copy form crossed with counter width: no exact cell |
| `--family hp --carry-update` | 4 | Both changes recover all 76 bytes |
| `--family init --open-init` | 4 | Explicit six stores improves first loop, no exact cell |
| `--family init --open-init --hist-sizeof` | 3 | Production and ordered controls; sizeof member loop recovers 102 bytes |
| `--family combined` | 2 | Both recoveries coexist, 11 to 13 exact of 26 functions |

Each requires `--domain URL`, using its predeclared issue248 comment.
The sizeof family initially ran against the preceding domain URL before the
third control was explicitly declared. Those three attempts remain under
`build/gcc3-screen-init-open-sizeof-invalid-domain` and are excluded; the
correct rerun uses issuecomment-5973998963. Total: 27 valid cells and three
excluded metadata-invalid attempts. The final combined domain is
issuecomment-5974018975; carry-update uses issuecomment-5973937150.

`python3 tools/gcc3_candidate_audit.py` reviews all 27 valid cells: v34filters
has 26 functions/48 named data objects, v34info has 7 functions/0 named data.
Types, binding, visibility, allocated nontext and canonical relocations agree
with each family's control. A raw nontext comparison first flagged probe's
six jump-table addends. Canonical function-plus-offset targets prove unchanged
entries despite a shifted preceding body; do not simply mask these differences.
The corrected audit passes 27/27 cells.

## What the RTL establishes

HP's short index changes sign extension and comparison width before allocation.
The in-place multiply changes combine UID35 from an anonymous product to a
named carry update subsequently consumed by the accumulator, before restoring
the prior history sample. Local/global mappings follow this lifetime change;
neither baseline nor exact cell has stack substitutions. This is a source
arithmetic boundary, not the previous updateAlpha eviction mechanism.

Init's blob has six fixed-offset short stores per outer iteration, rather than
a nested loop. A short counter plus `i < sizeof(t->hist)` produces the unsigned
bound and member-relative word clear directly. hist has 40 shorts but its byte
size is 80: this reproduces the already documented D29 overwrite into adjacent
state. The shipping branch retains complete bounded initialization. Explicit
six stores need not imply the original declared one two-dimensional array;
six named state arrays are also possible. No unique source spelling is claimed.

## Production verification and replay

A fresh 300-object baseline raw-matches PR247's build. After adoption only
v34filters.c.o changes; the final production object raw-matches the combined
candidate (including removal of an unused diagnostic variable). Whole-tree
exact set rises 927 to 929 of 1852; exact bytes rise 95604 to 95782. Sole gains
are the two named functions; zero losses. Full fixed `make phase J=4` passes
388/0 and structural checks; the final-source component rerun passes 1/0.

Replay screening with `--baseline-report REPORT --object-dir OBJECTS
--excluded-paths PATH_LIST --old-alpha-object HISTORICAL_ALPHA_OBJECT
--json-out OUTPUT`; all inputs are explicit. Capture the competing PR paths
with `gh pr view 245 --json files` and write its source paths as one per line.
Use the parent reload reproduction to obtain the historical alpha control.
Archive the baseline objects before adoption. Reproduce each family with its
recorded domain, then run the full-TU audit and the production gates.

The partial probe candidate remains unadopted. Its matching length does not
justify inventing a header or changing a guard. SNR's division prefix likewise
does not justify transplanting an unrelated spill repair.
