# Cosine tone generator recovery

Baseline9cb6d002, Gentoo GCC3.4.2-r2/selected assembler2.15.92.0.2 and
complete .build-config flags with DSPLIB_REPRODUCE_BUGS. Blob0xaaed0 is125
bytes versus142 production. It initializes phase/inc only, calls
FPM_phasor_demod and reloads scale afterward at0xaaf18. The callee unconditionally
defines cos at0xa9560 before return, and neither caller nor callee reads sin.
Artificial output clears serve no defined read. A short countdown tests the
old low word with increment, unlike the retained compare against-1.

The full source cross is:

| Scale/output lifetime | Retained loop | Short post-decrement |
| --- | --- | --- |
| cached scale / cleared outputs |142B/SIZE17|141B/SIZE16|
| direct scale / cleared outputs |142B/SIZE17|141B/SIZE16|
| cached scale / uninitialized outputs |142B/SIZE17|125B/BYTES88|
| direct scale / uninitialized outputs |126B/SIZE1|125B/EXACT|

An intermediate short declaration with retained eager countdown gives127B/SIZE2;
narrowing alone does not recover old-value testing. Seventeen staged compilations
cover nine distinct sources and complete objects. Eleven functions/twelve global
bindings and data preserved; only generator canonical body changes. Four/eleven
->five/eleven exact, no losses. Raw baseline and staged controls reproduce;
final winner raw-replays the earlier post-decrement result. All125 bytes and
canonical relocation agree, including countdown/output register assignments.
No flags, register declarations, inline assembly or layout padding change.

The retained short post-decrement visits the same count-1 sequence as the
original eager sentinel loop and returns the original count. Cos is initialized
by the callee before every use; zero count reads neither automatic output.
Scale lifetime matches object loads, without claiming an output array spanning
unrelated state members as a valid alias fixture. Fixed tests retain positive,
zero and quadrature coverage and add an adequately buffered -1 component call
(65,535 samples), state/return comparison and guards on both outputs. This
boundary is not a claim about modem lifecycle use of negative counts.

Predeclared domains:
[scale/lifetime](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946354554),
[count width](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946368395),
[loop control](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946390535),
[complete cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946403666).
Replay tools/playbook_fpm_tone_demod.py, _count.py, _loop.py, _cross.py with
--domain matching URL. Corresponding build/playbook-fpm-tone-demod* directories
retain commands, hashes, inventories, changed bodies/relocations and RTL.
Retained validation artifacts live in build/playbook-fpm-tone-demod-adoption.
F11574. No fuzzing or mutation execution.


## Retained validation

The production full TU raw-reproduces the winner; only src_dsp_fpm_tone.c.o
changes among300 saved comparison objects. Complete build300/300, zero failures.
Whole-tree868/1852 ->869/1852 exact;84,590 ->84,715 exact bytes; sole gain
FPM_TONE_generate_demod, no losses. Fixed Gentoo make phase385 passed/0 failed
(`/tmp/fpm-tone-demod-phase.log`), including the new wrapping component group.
References14,229/2,703 findings headings and285 suites/10,038 static anchors
clean, no anchor retargeting. Existing positive/zero/quadrature tests pass.

Complete same-order300-object partial links remain DIFFERENT (strict exit1
for each). Positioned equality68,316 ->68,315/943,398, allocated914,142
->914,126 bytes. Exact section70/92, symbol394/2,907 and relocation1,018/18,317
records unchanged. TU alignment shrinks; canonical function gain does not
imply improved positional whole-object equality. Before census completed
before adoption. Initial audit used an incorrect flattened object filename
and produced no audit verdict; corrected audit confirms full raw replay.
The attempted partial wrapper before its file was created did not run; the
recorded complete links are from the successful corrected command.

Master41a7a017's finding-renumber fix was already an ancestor after fetch;
no further rebase needed and no finding-number collision present.
