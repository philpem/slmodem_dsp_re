# Quadrature generator output-lifetime transfer

The F11574 transfer is measured independently on FPM_TONE_generate2, reference
148B versus165B retained. Complete fpm_tone.c revision1dc5210d, unchanged
Gentoo GCC3.4.2-r2/selected assembler2.15.92.0.2, full .build-config flags and
DSPLIB_REPRODUCE_BUGS. Baseline full TU raw-reproduces current production.

Reference has no artificial cos/sin initialization. It tests the old low-word
countdown at0xaae6a/0xaaeac; retained source uses eager sentinel arithmetic in
an int carrier. Reference FPM_phasor defines cos at0xa937d and sin at0xa93b2
unconditionally before return. Zero count consumes neither. Per-store scale
reloads already match and are preserved.

| Source family | Retained loop | Short post-decrement |
| --- | --- | --- |
| cleared outputs |165B/SIZE17|164B/SIZE16|
| callee-defined outputs |149B/SIZE1|148B/EXACT|

Four sources/four emissions, one exact hit. Neither individual change suffices.
All eleven functions/twelve global bindings and data preserved; only generate2
canonical body changes. Five/eleven ->six/eleven exact, no losses, including
retained demod recovery. No change to scale lifetime, output ordering, phase
writeback, return contract, compiler flags or public signatures. The short
post-decrement follows the same word traversal as the retained sentinel loop.

[Predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946625861).
Replay tools/playbook_fpm_tone_pair.py --domain that URL. Build/playbook-fpm-tone-pair
preserves commands, hashes, inventories, canonical changed bodies/relocations
and RTL. Existing fixed fixture includes positive, zero, quadrature consistency
and adequately buffered -1 component calls, with131,335 checks in the negative
count group; no modem-lifecycle negative-count claim. No fuzz/mutation execution.
F11576. Retained validation artifacts: build/playbook-fpm-tone-pair-adoption.


## Retained object measurements

Production full TU raw-reproduces the winning object; only src_dsp_fpm_tone.c.o
changes among300 saved comparison objects. Build300/300, zero failures.
Whole-tree870/1852 ->871/1852,84,766 ->84,914 exact bytes, only generate2
gained/no losses. Before census completed before source adoption. Complete
same-order300-object partial links remain DIFFERENT (strict exit1 each).
Positioned equality68,320 ->68,343/943,398, allocated914,142 ->914,126 bytes.
Exact section70/92 and symbol394/2,907 unchanged; relocation1,018 ->1,019/18,317.
No original whole-TU/whole-object identity claim follows from this function gain.


Fixed Gentoo make phase385 passed/0 failed, final boundary OK
(`/tmp/fpm-tone-pair-phase.log`). Existing generate2 negative group131,335
checks passes, alongside zero/positive/quadrature and retained demod checks.
Structural14,231 references/2,705 findings headings and285 suites/10,038
static anchors clean. No fixture or static-anchor change was needed.

Next evidence-gathering lead is the remaining FPM_TONE_generate sibling:
its source retains an ascending int loop, cached scale/period and artificial
output clears; the blob uses word countdown and per-call/per-sample loads.
The reference reversal comparison is word-sized. Negative traversal and
reversal narrowing need fixed adequately buffered component controls before
any adoption; this ledger does not claim those source differences repaired.
