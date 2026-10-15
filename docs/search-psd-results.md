# Guarded search and scalar PSD recovery

Baseline d13949ad /1081 exact. Eighteen compiled cells /15 distinct source texts /218 live shared-body
verdicts, across four declared domains. Every cell preserves bindings, named
data, allocated nontext contents and relocation metadata. Both unchanged TUs
reproduce production raw. Follow-up baseline objects reproduce their previous
independently compiled selected controls raw. Fixed recovered Gentoo profile,
mandatory bug define, no fuzzing/mutation execution or tolerance change.

## Mapping

| Delayed length | Count postincrement | getConstellationsIndex | Inlined caller |
| --- | --- | --- | --- |
| no | no | SIZE4 | SIZE4 |
| yes | no | SIZE8 | SIZE24 |
| no | yes | SIZE4 | SIZE6 |
| yes | yes | SIZE6 | SIZE22 |

Original guard j<count precedes length load; moving that initialization into the
search body recovers the entry's six instruction/operand rows exactly. Original
unmatched tail stores distinctIndex, increments/publishes count, then stores
count-1 into group. Postincrement source recovers this boundary. No callbacks
or writes intervene in the delayed load; valid grouping semantics stay intact.
Original zero-length match remains a match.

Four follow-up comparison controls preserve both boundaries. Swapping the
size comparison's source operands is raw-inert; spelling length>n instead of
n<length restores cmp0/JBE entry and CMP/JA backedge. Two raw emissions across
four texts; no exact gains. Retain axis-0-1, avoiding the inert size synonym.
getConstellationsIndex280B/SIZE6 versus original286B; caller retains SIZE22.
Nine other bodies unchanged. Remaining allocation, operand ownership and
unmatched-tail copies are separate; do not expand the outer loop or retry
comparison synonyms without a new witness.

## PSD

| Loop-expression normalization | Float peak | process verdict |
| --- | --- | --- |
| no | no | SIZE17 (618B) |
| yes | no | SIZE2 (637B) |
| no | yes | SIZE19 (616B) |
| yes | yes | BYTES56 (635B) |

Original reciprocal/conversion lives after the bin guard in every scaling arm.
Writing division in the loop expression lets GCC hoist it to that guarded
preheader. Float peak values come only from float bins/exact zero; combined
control also restores direct fcom, preserving ordered comparison behavior.
No division/logarithm intermediate is narrowed except the original float log
temporary and output stores. This is checked in emitted code, not assumed
from a source cast or optimization flag.

Four scalar/window controls on BYTES56 give baseline56, scalar-only41,
window-only40, both25. Ordinary out[i]/peak or out[i]/frames removes artificial
long-double casts; initial RTL divisions are SF. Retained x87 excess precision
keeps the operations wide until original narrowing. Window*input recovers flds
input/fmuls window. Both together match all60 original x87 instruction/operand
rows and27 branch rows, including all memory operands and rounding sites.
No universal NaN payload claim follows from source commutativity alone.

Two initial-read controls then place bins initialization after the frame-count
calculation. Original reads overlap before length and derives bins after division
operand preparation; this recovers the original integer entry/initialization
rows without changing the recovered homes. process635B/BYTES6 remains. All six
remaining rows are register substitutions in two unsigned-to-float stack
conversion sequences (rows175,177,197,198,199,202). Existing alpha_why accepts
the complete graph under live-range renaming; no comparator change. No claim
about original RTL or the pass choosing those registers. Twelve PSD bystanders
and data are unchanged. Close scalar/source-order domains; next trace the two
conversion sequences and incoming compiler state before more source controls.

## Invalid run and artifact correction

The first scalar replay selected canonical d13949ad objects instead of the
documented combined boundary baseline. Its raw-baseline assertion refused the
run before candidate conclusions. Saved as build/gcc3-psd-scalar-operands-invalid-baseline,
excluded from all denominators. Valid replay uses build/psd-scalar-baseline with
the independently compiled boundary object and unchanged .build-config.

The old build/partial object preceded the previous batch. Preserve it as
partial-before-stale.o, exclude it as a d13949ad baseline, and reconstruct the
true before-link from archived pre-batch objects in recovered order. All300
faithful objects equal canonical objects raw; only the two selected objects
change,298/configuration unchanged. Function-grade recovery is not whole-object
closure; strict before/after partial comparisons must remain visible.

## Replay

```
python3 tools/gcc3_search_psd_boundaries.py --domain docs/search-psd-boundaries-domain.md
python3 tools/gcc3_psd_scalar_operands.py --domain docs/psd-scalar-operands-domain.md --baseline-dir build/psd-scalar-baseline
python3 tools/gcc3_psd_initial_read.py --domain docs/psd-initial-read-domain.md --baseline-dir build/psd-initial-read-baseline
python3 tools/gcc3_mapping_comparison.py --domain docs/mapping-comparison-domain.md --baseline-dir build/mapping-comparison-baseline
python3 tools/gcc3_search_psd_audit.py --integrated
```

Prepare each follow-up baseline directory with its .build-config and preceding
selected candidate named as its canonical src_*.o. Scalar baseline is boundary
psd/axis-1-1; initial-read baseline scalar psd/axis-1-1; comparison baseline
boundary V90MappingParamsInt/axis-1-1. Audit verifies these raw links and all
grades, original graph rows, metadata/data and chosen canonical source/objects.
Artifacts retain complete commands, identity, config/header/source/object hashes,
RTL and build/search-psd-audit.json.

## Final integration

Both complete canonical objects equal independently selected controls raw;
exactly2/300 objects change,298/configuration unchanged. Census1081/1852 EXACT,
119682 original exact bytes unchanged: SIZE673→672, REGALLOC30→31, BYTES63 and
UNRESOLVED5 unchanged. PSD moves from SIZE to grade1, not grade0. Ratchet green.
Final make phase J=8 exits0:388 period differential passed/0 failed, structural
checks green;285 suites/10038 static anchors attached/unique/correct-arm. PSD
locators retargeted preserving defect intent; none executed. Existing14429
references still point correctly. No modern portability claim.

Faithful300-input partial .text702028→702012. Before is rebuilt from actual
pre-batch objects, not the excluded stale file. Both strict comparisons remain
DIFFERENT/exit1. Whole-object closure is still open. Artifacts
build/search-psd-integration, byteident-search-psd-final.json, search-psd-phase.log,
search-psd-refcheck.log and final static reference recheck log.
