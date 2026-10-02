# Multi-tone detector staged source controls

F11600–F11602. No source adoption. Retained production stays884/1852 exact,
88,310 exact bytes, last fixed Gentoo phase385/0. These are compiler diagnostics,
not candidate differential or modem-lifecycle results.

## Sentinel countdown and energy width (F11600)

[Four-cell declared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5949735901)
at43ef6841 crosses short countdown from count-1 through sentinel -1 with
advancing sample pointer, against short wideband/tone/out_of_band locals
(threshold remains int). Blob297B; baseline288/SIZE9, words287/SIZE10,
sentinel306/SIZE9, both305/SIZE8. Four sources/four complete emissions,
no exact gain/loss,2/3 exact unchanged. Only detect changes; exact create184B
and delete preserved. All3 functions/3 data objects/type/binding/visibility,
named owner values/relocations and allocated nontext agree. Baseline raw
reproduces production.

Initial generator used a broad declaration-name substring and rejected its
match count before compiling any candidate. Original invocation log preserved
at /tmp/playbook-mtd-detect.log; corrected exact declaration matching reran
all4 cells under the same domain, valid log /tmp/playbook-mtd-detect-valid.log.
No invalid candidate result was interpreted.

Word carriers restore some comparisons but clamp still emits SAR31 rather
than blob SAR15. This alone does not support a manual mask or header change.
Initial RTL of sentinel compares narrowed counter with -1; final CMPWffff
persists, unlike the object's DEC/narrow/INCW flags. Replay
 tools/playbook_mtd_detect.py; artifacts build/playbook-mtd-detect.

## Postdecrement stage (F11601)

[Declared staged discriminator](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5949865105):
two new cells for(i=count;i-- !=0;) with advancing pointer, int or short
energies, plus recompiled unchanged baseline. Three sources/three emissions:
288/SIZE9, postdecrement299/SIZE2, postdecrement-words298/SIZE1. No exact
gain/loss,2/3 exact unchanged. Complete same data/symbol/other-body controls.

This restores DEC/narrow/INCW old-value flags in the blob's loop. The complete
body still differs, including early energy load and terminal decision; not a
register-only or one-byte recovery. Replay tools/playbook_mtd_postdec.py;
artifacts build/playbook-mtd-postdec. No equivalent while spelling counted.

## Terminal result stage and scope review (F11602)

[Predeclared result-CFG discriminator](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5949899276):
production baseline, postdecrement-word control and one shared short verdict
initialized NOSIGNAL, set under unchanged conditions and returned once.
Three sources/three emissions:288/SIZE9,298/SIZE1,316/SIZE19. No exact
hit/gain/loss,2/3 exact unchanged. Complete symbol/data controls agree; only
detect changes. Original ternary already lowers to a boolean definition in
initial RTL, so this was a separate stage discriminator, not a late pass flag.
Replay tools/playbook_mtd_result.py; artifacts build/playbook-mtd-result.

Stop this staged detector source family. Seven distinct sources across ten
compiles (four + three + three), each stage with raw unchanged production
control; repeated baseline and postdecrement-word compiles are not new sources.
No result-width/declaration/condition permutations, clamp masks or profile
exceptions are justified by these misses. Reopen only with independently
supported early-stage evidence. A source countdown behavior mismatch for
negative API counts is statically predicted; it has not been measured by a
runtime probe here. No negative-count modem reachability inferred.

Every stage retains actual full saved flags plus mandatory bug define, Gentoo
GCC3.4.2-r2 and executed selected assembler2.15.92.0.2, source/header hashes,
complete objects/initial RTL and complete-object-audit.json. No candidate
runtime/census/partial gates; existing t_fpm_mtd fixtures are not unrun
candidate results. No fuzzing or mutation execution.
