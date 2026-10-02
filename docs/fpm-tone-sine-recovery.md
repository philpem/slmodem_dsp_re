# Sine generator traversal and reversal recovery

Revision54bf1179, complete fpm_tone.c, Gentoo GCC3.4.2-r2 and selected
assembler2.15.92.0.2. All saved .build-config flags and mandatory
DSPLIB_REPRODUCE_BUGS retained. Blob0xaad50 is213B versus243 production.
It uses a word countdown/cursor, reloads scale after FPM_phasor, and has no
artificial output clears. Callee defines both outputs before use. Period is
read after generation; elapsed/period compare is signed word-sized. Counter
clears before private phase capture (0xaadd6 before0xaaddc).

## Fixed behavioral controls

Both generator states clone a fully allocated reference-created object from
supplied configuration, like existing generator fixtures. They share only
unused backing-buffer pointers; generation updates scalar state and separate
adequately sized outputs. Negative-1 call with period0 covers65,535 samples,
return, state and both guards. Old source skips its loop:65,536/65,667 failures.
Counter group uses period32767 and eight ordinary32760-sample calls to reach
32760, then128 samples cross the signed-word boundary. Original retains
32776 while old source prematurely resets and flips phase; the following
ordinary8-sample call observes that wrong phase.10/262,237 old-source failures.
No planted history or modem lifecycle claim. Whole-state compare uses
 diff_eq_obj, not a byte loop.

The first fixture incorrectly passed zeroed caller-owned storage/len0 to a
constructor that unconditionally writes resonator buffers. It crashed before
generator verdict; preserved under build/playbook-fpm-tone-sine/invalid-fixture,
INVALID/excluded. Fixture was corrected without changing src and rerun;
negative-corrected.run.log records actual mismatches. Ref generator declaration
now has the established short return contract; earlier calls simply ignored it.

## Bounded source evidence

Eight generation cells cross direct scale, removed output clears and short
post-decrement cursor. None exact; combined236B/SIZE23 fixes generation graph
but leaves reversal. Eight reversal cells cross late period, short elapsed and
predicate order; none exact. Six promotion cells recover word compare/test but
introduce eager Boolean setcc/and:SI; initial .01.rtl already contains it,
before allocation. Local GCC expr.c distinguishes eager TRUTH_AND_EXPR from
short-circuit TRUTH_ANDIF_EXPR; no unproven branch-cost attribution is made.
Six direct-period counter/guard cells recover two branches,212/211B versus213;
sequential guards and conjunctions canonicalize within each carrier family.

Final twelve cells cross three counter boundaries (short local, int narrowed at
uses, owner word field), predicate order and phase capture before/after counter
clear. Three controls raw-replay. Only owner/reference-order/capture-after is
213B/EXACT; capture-before is213B/BYTES11. Other families remain212/211B. The
retained source advances the counter directly, compares its signed short value,
and initializes private phase after clear. GCC coalesces the early counter
write into the reference conditional stores. This is source/behavior evidence,
not an adopted nearest score or a regalloc-only claim.

Across domains40 matrix compilations plus one staged seed =41 valid
compilations,34 distinct sources/32 complete emissions. Eleven functions/twelve
global bindings and every data section preserved; only generator canonical
body changes. Final six/eleven ->seven/eleven exact, no losses. Raw unchanged
production replay and staged controls reproduce. Seed compile has its own
command/source/object hashes and is labeled staged, not unchanged production.

Domains:
[generation/reversal](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946738852),
[promotion](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946778488),
[direct-period guards](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946818535),
[predicate/phase lifetime](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946856379).
Replay tools/playbook_fpm_tone_sine.py, _reversal.py, _narrow.py, _guards.py and
_order.py with matching --domain URL. Corresponding build/playbook-fpm-tone-sine*
artifacts preserve commands, hashes, inventories, complete bodies/relocations
and RTL. No flags, field types, public signatures, phase arithmetic or output
ordering changed. Two existing static tick anchors retargeted to owner update
while preserving four-/sixteen-sample fault meaning; no mutation execution.
F11577. Retained validation artifacts: build/playbook-fpm-tone-sine-adoption.


## Retained object measurements

Production full TU raw-reproduces the213-byte winner, including canonical
relocation. Only src_dsp_fpm_tone.c.o changes among300 saved objects. Complete
comparison build300/300, zero failures. Whole-tree871/1852 ->872/1852,
84,914 ->85,127 exact bytes; sole gain FPM_TONE_generate, no losses.
Before census finished before src edit. Complete same-order300-object partial
links remain DIFFERENT (strict exit1 each). Positioned equality68,343
->68,419/943,398, allocated914,126 ->914,094. Exact section70/92 and symbol
394/2,907 unchanged; matching relocation records1,019 ->1,018/18,317.
Per-function canonical target identity differs from absolute-position whole-
object records; both movements are retained without treating one as the other.


Fixed Gentoo make phase385 passed/0 failed (`/tmp/fpm-sine-phase.log`). Repaired
negative group65,667/65,667 and reachable counter group262,237/262,237 pass;
existing V.25/ordinary/quadrature/demod groups remain green. Static285 suites/
10,038 anchors and14,231 references/2,706 headings clean. No tolerance or modern
portability claim. Exactly two existing tick anchors retargeted; no fuzzing,
mutation execution or mutation snapshot refresh.

Actual symbol sizes are taken from ELF, not the absolute SIZE delta. The final
cross is212B for all four short-local cells and211B for all four int-use cells.
Owner positive-first before/after capture gives211B; reference-first before
capture213B/BYTES11, after capture213B/EXACT. An earlier domain-comment summary
incorrectly inferred214/215 from absolute deltas; this ledger corrects it to
212/211. Compiler/source/control commands and canonical body evidence were
unaffected; no result is used as a closest-size adoption.
