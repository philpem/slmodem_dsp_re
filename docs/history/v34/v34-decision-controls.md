# V34 decision width, traversal, result and target-read controls

Baseline361ef919 retains decision at167B against the reference170B.
Three finite domains investigate independently observable source boundaries.
No source or API change is adopted from any of these controls.

The reference advances the point cursor rather than indexing pts[i], compares
the metric as a word, conditionally reads target fields after the positive
count guard, and normalizes the selected signed-short index in EAX after its
object store. There are zero relocation or resolved instruction call/jump
consumers of this export, so that epilogue does not uniquely identify a short
versus int result declaration. No source caller or debug type resolves it.

The first [four-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954685314)
crosses int/short best_dist with indexed/advancing point traversal, keeping
the void API and all other source boundaries. Replay:

    python3 tools/playbook_v34_decision.py --domain DOMAIN_URL

| Best distance | Points | Size | Result |
| --- | --- | ---: | --- |
| int | indexed |167|SIZE3|
| int | advancing |164|SIZE6|
| short | indexed |168|SIZE2|
| short | advancing |165|SIZE5|

The [twelve-cell result cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954731576)
retains those four void controls and adds short/int results using identical
return(short)(best-pts) after the existing stores, with isolated matching
public headers. All four void objects reproduce the first domain. Short/int
results raw-merge per source form:172,169,173,170B respectively. The last
matches reference size but remains BYTES129. Replay:

    python3 tools/playbook_v34_decision_return.py --domain DOMAIN_URL

The [final nine-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5954763890)
includes production plus eight combinations of best_dist int/short, dist
int/short and target reads unconditional/guarded. All eight retain advancing
points and the already tested normalized int result. Two parent objects
reproduce the previous cross. Replay:

    python3 tools/playbook_v34_decision_loads.py --domain DOMAIN_URL

| Best distance | Current distance | Target reads | Size | Result |
| --- | --- | --- | ---: | --- |
| int | int | entry |169|SIZE1|
| int | int | guarded |170|BYTES135|
| int | short | entry |169|SIZE1|
| int | short | guarded |170|BYTES135|
| short | int | entry |170|BYTES129|
| short | int | guarded |171|SIZE1|
| short | short | entry |170|BYTES127|
| short | short | guarded |171|SIZE1|

The period initial RTL explains why narrowing only best_dist did not recover
the word comparison: it still compares SI values (32bits). Narrowing both
distance locals gives HI-subreg comparison at .01.rtl insn56 and final
CMPW/JGE. Guarded target reads are present in initial RTL and survive into
the final conditional branch. The fully supported combined candidate recovers
those operations, pointer ADD4 and signed result normalization, but retains
different spills, owner loads and first-entry layout. Both target values spill
where the reference retains them; best/count carriers differ in the opposite
direction. No further independent source discriminator was found. This is
not evidence to choose register names, declaration positions or synonyms.

Across all domains:25 valid compilations and12 distinct raw objects.
Each uses the actual saved production command, DSPLIB_REPRODUCE_BUGS,
Gentoo GCC3.4.2-r2 and the selected/executed assembler2.15.92.0.2. Unchanged
production raw bytes reproduce. Complete TU denominator is12 emitted
functions/five named data objects; only decision changes. The other11 bodies,
relocations, symbol types/bindings/visibility/imports/exports, data offsets/
values/targets and allocated nontext bytes remain unchanged. Exact count
stays1/12 with no gains or losses.

Existing fixed component coverage is507 paired calls:22 positive point counts
by23 targets plus one zero-count case. The zero case still selects pts[0].
It has no full-EAX return assertion or public lifecycle reachability claim.
No fixture or new deciding differential gate is run for these unadopted
source/API alternatives; production remains at919/1852 exact functions.
No fuzzing or mutation execution is used.

Close the finite width/cursor/result/guarded-target domains. A new experiment
needs a different independently established source or compiler-stage boundary;
equal size, desired spill positions and the absence of consumers are not one.
Artifacts remain under build/playbook-v34-decision{,-return,-loads}, including
complete-object audits and repeated raw controls.
