# Dependency and scalar-expansion batch

PR275 merged at6b4509bd with both CI checks green. New baseline1069/1852,
116229 exact original bytes. Isolated branch/worktrees avoid PR263's V34/
shared structure scope and issue22 writes. Gentoo3.4.2-r2 complete recorded
commands, actual selected assembler2.15.92.0.2, retained build config and
DSPLIB_REPRODUCE_BUGS-last are used for every compiler cell.

## One adopted complete function

calcModulusParameters becomes EXACT622B. The original has five modulo and
six signed64 division call sites plus six literal place-value stores. Retained
source loops expose only one modulo and two division sites. Expanding both
fixed six-entry sequences, including the special sixth truncation, then
restoring the original two sum-operand roles reproduces the entire body.
No chosen registers, declaration permutations, flags or fake dependencies.

This source-complexity change also restores getPower's original out-of-line
call boundary and574-byte body shape. It remains BYTES10 with two exchanged
double stack homes; no attempt is made to fit their declaration/slot order.
Seven other bodies are unchanged. Six jump-table destination relocations
move, decoded to exact original ordered case offsets275/304/336/368/400/42;
metadata/bindings/imports/exports, allocated payload shape and BSS remain fixed.
The source family is recovered under the retained profile, not uniquely proven
against every possible original compiler unrolling option.

Strict final census1070/1852,116851 exact original bytes: +1function,+622bytes,
zero losses. All300production objects reviewed:299raw unchanged; one raw-equal
to the independent winning object. Only target and its getPower caller change.
Configuration unchanged. Four static apparatus anchors preserve the same
exponent and five-index product faults; no mutation execution or snapshots.

## Research and negative domains

| Area | Valid full-TU cells | Live emitted/shared comparisons | Gains |
| --- | ---: | ---: | ---: |
| FSE verbose dependency replay |2|8|0|
| FSE diagnostic-result/cap/exit domains |14|56|0|
| Constellation mixed-radix expansion |5|45|1|
| V92 CRC promotion transfer |4|84|0|
| V32 phase-reversal graph |12|168|0|
| V29 epoch factoring/index graph |7|35|0|
| Total |44|396|1|

FSE typed copy has no edges to two integer reset stores (priority6 versus
copy5), allowing them to hoist; builtin memcpy's set0 memory introduces both
edges and copy priority8. Two verbose raw repeats and actual scheduler issue
order corroborate this mechanism. Five official scheduler/dependency/alias
source files are hash-pinned, not claimed to reproduce all Gentoo patches.
The original's alias tags are not available; neither copy candidate is exact.
No unused field retyping, alias/volatile casts or production flag change follows.

FSE_getdiag's entry result, selected count, overflow arm and conditional/literal
exit controls preserve original negative count returns but remain nonexact.
One229-byte body has70instructions against72original; another has71. Size
does not settle allocation or missing operations. Fourteen cells/56body grades,
three raw baselines/two cross-domain raw repeats/eleven positive controls.

V92's direct expanded CRC restores promotion and packer lengths665/681B,
and its two get-vector wrappers recover22-byte call shapes, but all four remain
nonexact. Losing direct/rolled control's two reset losses are explicitly recorded;
expanded/direct restores those existing exacts but adds none. No V92 change.

V32 persistent counter publication survives allocation then collapses between
flow2 and sched2. RTT publication and extension controls miss. Twelve cells,
168bodies,96validated field-stage observations; twelve duplicate GCSE streams
explicitly excluded. Six same-string pool permutations are audited as such.
No source adoption or blanket original-profile conclusion.

V29 standalone epoch has no observed call-boundary discrepancy. Streaming
arithmetic moves an eq spill to completed distance; late index read also misses.
Seven cells/35bodies/49RTL observations preserve all eight multiplies/seven
shifts, with a known spill detector. The final GCSE stream is selected only
at its explicit labelled boundary and duplicate-UID checking remains active.
No source adoption; possible fully inlined original helpers/TU effects remain
uncertain, rather than ruled out by absence of calls.

## Transfer screen and next steps

Read-only repeated-helper screen:99data/service C TUs,389emitted bodies,
146small nonexact reference bodies; zero original>=2/retained1 same-callee
nominations. It fires on the known Constellation5/1mod and6/2div example.
Five paired fixed-loop reviews provide no independent missing expansion graph.
The screen excludes larger bodies, indirect calls and fully inlined helpers;
it is not a proof that all remaining source is correct or unrecoverable.

The requested five-to-twenty gains were not reached. We retain one complete
gain and measured stopping points instead of partial/score-selected edits.
Next useful work is to widen the fixed-graph screen to medium C++/DSP bodies,
look for independent original literal accesses/call repetitions, and audit
caller inline costs with every recovered helper. For FSE, new original field/
copy/API evidence must precede any candidate changing its alias graph. After
PR263's structure work merges, remeasure that shared-header context before
assuming previous allocation results persist. Keep profile research with22's
owner; no register/slot/declaration fitting or fuzz/mutation programme.

Replay and audits are the matching tools listed in the individual domain/
result documents. tools/byteexact_dependencies_batch_audit.py recomputes every
saved source/object hash and live canonical grade, then checks the whole
production delta and ordered table destinations. Generated artifacts stay in
build, not the commit. Final combined period/structural gate is recorded below.


Final combined make tc and make phase exit0:300/300period objects;388period
passes/0failures and all structural gates green. Static anchors285suites/
10038unique,0detached/nonunique, no mutation execution. All19new Python tools
parse; modified source fetcher reports/verifies5/5files. Final whole-tree live
production audit passes. Newly staged references are checked before commit.

Staged reference audit:14382references,0unresolved/held/stale;2938finding
headings checked. Static285suites/10038anchors clean. Working diff check clean.
