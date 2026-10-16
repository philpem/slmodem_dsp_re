# BwChDem_Create ratio recovery and table-visibility controls

[Four-cell predeclared cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5948837469)
at70bf8bc8 compares current ratio28996/0x7144 with blob29000/0x7148
(immediate0x875c8), and current visible const table initializer with early
tentative const declaration/definition immediately after Create. Values,
constness, other initialization/calls/data preserved.

Four sources/two complete emissions. Moving initializer raw-reproduces the
unchanged complete object; corrected-ratio+move raw-agrees ratio-only. Gentoo
still folds first table entry26 despite deferred source definition. This
visibility family does not recover blob's first-entry load at0x87525. No
source-order adoption or further arbitrary declaration placement. All3
functions/3 globals/nontext/type/binding/visibility preserved; only constructor
changes with ratio correction;348B/SIZE16 remains vsblob364B, exact1/3 unchanged.
Production baseline raw-reproduced. F11594.

Ratio correction is independently literal source fidelity: one wrong immediate
is not original configuration. Retain only29000, not initializer move. Existing
t_v23bwch skips child state and assumes generic tone coverage; generic constructor
coverage cannot check this owner's specific child configuration. Added fixed
post-create ratio compare fires in all five constructions on baseline:
1/39009 acquisition,3/131595 alternate blocks,1/114326 silence checks fail;
coverage6 passes. Reference value independently checked29000. No tolerance,
fuzzing or mutation execution. Corrected source must pass the same fixtures.

Full configured Gentoo flags plus mandatory bug define/executed selected
assembler2.15.92.0.2; tools/playbook_bwch_create.py, artifacts
build/playbook-bwch-create and build/playbook-bwch-adoption retain actual
commands/hashes/inventories/symbol/nontext/disassembly/RTL. No exact gain
claimed for this fidelity correction or full-original-profile conclusion.

Retained comparison build300/300, zero failures; only src_pump_v23_bwchdem.c.o
changes and raw-reproduces ratio-only candidate. Constructor canonical bytes
have exactly one changed byte at+194 (0x44→0x48); relocation records unchanged.
Whole-tree882/1852 and87,605 exact bytes unchanged, no exact gains/losses.
Same-order complete300-object partial links remain DIFFERENT (strict exit1):
positioned68,258/943,398, allocated914,142, exact section70/92, symbol394/2907,
relocation1020/18317 unchanged. No complete-object/profile identity claim.

Fixed Gentoo make phase385 passed/0 failed. V23 fixture passes39009+131595+
114326+6=284936 checks, including all five previously failing owned-child
ratio comparisons. Structural14240 references/2723 finding headings and285
suites/10038 static anchors clean. No fuzzing/mutation execution/modern claim.
