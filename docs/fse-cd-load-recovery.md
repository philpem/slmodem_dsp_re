# V32 CD slicer: recover count reads before magnitude output

Baselineb1632e79. An independent read-only audit and parent disassembly show
blob FSE_decision_CD reads owner count before magnitude output on both arms:
0x8153c before0x8154e, and0x8156c before0x81577. Reconstruction's return rereads
count after `*mag = 0x3299`, and the period object follows that order. The
count field is unsigned short; the corresponding signed-short output alias
is permitted C aliasing. This is a concrete behavioral discrepancy at the
component boundary, not a source rewrite for register names.

## Fixed input evidence

The existing fixture tests ordinary CD progression and handover. A new fixed
component group establishes counts0,1,13,14 by setup plus0,1,13,14 ordinary CD
calls. It then makes one call with magnitude pointing at the count member,
compares returned decisions, magnitude/count, angle and decoder/control state.
It does not plant impossible internal histories or claim that a modem carrier
loop uses this alias. The aliased output overwrites count, and the fixture
does not resume the CD lifecycle afterward.

Unrepaired source fails3/268 checks under Gentoo GCC; the ordinary15-symbol
progression passes321 checks. Negative-control log `/tmp/fse-cd-alias-negative.log`
and repaired phase log `/tmp/fse-cd-phase.log` separate the failure from all
other slicers. No tolerance, fuzzing or mutation execution.

## Bounded source and compiler controls

Four full-TU lifetime cells preserve the original counter: raw source,
captured int count before magnitude, int decision before magnitude,
unsigned-short decision before magnitude. Results118/118/121/118 bytes
against reference122, no exact hits. Only CD changes,9 functions/9 global
bindings and data preserved; raw baseline reproduces.

The blob also zero-extends the counter, increments it and immediately compares
the low word. Eager `short n` introduces a cwtl absent there. Ten cells cross
the above four forms plus a signed-short count carrier with eager narrowing
versus int n narrowed only at comparison/store. All four original cells
raw-replay. The signed-short carrier loads count before magnitude, then maps
its parity back into that carrier after the output store. Owner field type
and all state writes remain unchanged.

| Lifetime/result family | Eager short counter | Narrow at counter uses |
| --- | --- | --- |
| direct return |118B/SIZE4|118B/SIZE4|
| captured int count |118B/SIZE4|118B/SIZE4|
| captured int result |121B/SIZE1|122B/BYTES4|
| captured unsigned-short result |118B/SIZE4|118B/SIZE4|
| signed-short count carrier |121B/SIZE1|122B/BYTES2|

The last cell recovers initial comparison, signed count reloads, magnitude
ordering and final zero-extension. It is retained for those independent object
and fixed behavioral observations, not merely because it has the closest score.

BYTES2 does not mean just two remaining raw edits: the successor-pointer and
lms stores swap, four raw bytes differ, and the canonical relocation moves
offset33 ->40. The comparison masks both relocation fields before counting
differing bytes. All other canonical body bytes agree. Flow2 has reference
store order; sched2 reverses those stores.

A four-cell source/option cross compares raw/recovered source with retained
flags versus diagnostic `-fno-schedule-insns2`. Raw and recovered retained
controls reproduce. Disabling sched2 changes all9 canonical bodies and gives
recovered122B/BYTES27; no exact hit in any cell. No flag exception is adopted.
This verifies a scheduling contribution without recovering the original
profile or proving that all remaining differences have one cause.

Across the first three valid domains:18 compilations,10 distinct sources and10
complete emissions,9 functions/9 globals/data preserved,0/9 exact unchanged.
The first lifetime run used an incorrect domain URL: its artifacts are retained
as `build/playbook-fse-cd-load-invalid-domain-url`, explicitly INVALID and
excluded. All four cells were rerun with the actual returned predeclared URL.

[Lifetime domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946055936),
[conversion cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946068908),
[scheduler attribution](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946111445).
Replay tools/playbook_fse_cd_load.py, playbook_fse_cd_conversion.py and
playbook_fse_cd_schedule.py, each with --domain URL. Corresponding build/
artifacts preserve actual .build-config-derived full commands, mandatory
DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2/selected assembler2.15.92.0.2,
source/header/object hashes, inventories, changed disassembly and RTL dumps.
The measured flow2/sched2 reversal then motivates a two-cell staged source-order
control: reverse only the adjacent successor-pointer and lms assignments,
which address distinct non-overlapping fields of the same object. Both source
orders emit the identical complete object, still BYTES2, so lexical reversal
does not recover the reference scheduler result. No broader store permutations.
[Store-order domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946175490),
replay tools/playbook_fse_cd_store.py after the conversion tool. Across all
four domains20 valid compilations,11 sources/10 emissions. These finite
lifetime/conversion/sched2/store-order domains are closed; reopening requires
new independent source or complete-profile evidence.

## Retained validation

Retained full TU raw-reproduces the recovered candidate. All300 comparison
objects build; only V32dec.c.o changes. Before/after whole-tree census, fixed
Gentoo phase and complete same-order partial-link records live under
build/playbook-fse-cd-adoption. Whole-tree exact set stays868/1852 and84,590
exact bytes, no gains or losses. Fixed Gentoo make phase385 passed/0 failed.
Repaired alias group passes268/268 checks; whole t_v32fse35,312 checks passes,
including unchanged CD progression321. The direct host-container-sandbox
execution attempt failed with Bad system call and is not a functional verdict;
the period-container execution succeeds and reports the stated checks.

Final structural14,229 references/2,701 headings checked separately after
recording the findings; phase checks285 suites/10,038 static anchors, all unique,
no retarget needed. Complete same-order300-object partial links remain
DIFFERENT, strict exit1. Positioned equality68,316/943,398 and candidate
allocated914,142 bytes unchanged. Exact section70/92, symbol394/2,907 and
relocation1,018/18,317 records unchanged. Canonical function recovery and
positional whole-object comparison remain separate measurements. Behavioral
source recovery is retained despite no new exact-function gain.
