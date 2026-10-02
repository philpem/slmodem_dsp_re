# Single-character encoding control recovery

Revision1dc5210d, complete encode.c TU under retained .build-config flags,
Gentoo GCC3.4.2-r2 and selected assembler2.15.92.0.2; mandatory
DSPLIB_REPRODUCE_BUGS. Reference cEncodeChar at0xb09b0 is51 bytes versus39
production. It materializes the unsigned byte argument, performs byte additions,
and has two counter-store/return paths. Retained shared int helper updates the
counter through a ternary and emits one common store/return.

[Predeclared six-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5946559188)
crosses retained int helper, unsigned-byte helper and direct unsigned-byte
argument update with ternary versus ordinary if/else counter assignments.

| Carrier/factoring | Ternary | Ordinary counter branches |
| --- | --- | --- |
| retained int helper |39B/SIZE12|51B/EXACT|
| byte helper |39B/SIZE12|51B/EXACT|
| direct byte |39B/SIZE12|51B/EXACT|

Six distinct sources/five complete emissions, three exact hits. Direct ternary
raw-reproduces unchanged source object: direct factoring alone has no effect.
All branch controls recover initial byte materialization, addition lifetimes,
two stores and two returns. Thus byte-helper narrowing is not necessary within
this domain. Three hits do not uniquely identify original helper or carrier.
Chosen direct byte plus ordinary counter branches reproduces the full target
and preserves edprintf's entire canonical body/relocations. Shared helper
controls also change edprintf (342B baseline versus357/341), which remains
non-exact against351B reference; these are not separately adopted.

Both function and three global bindings, key table, all data/BSS sections and
public signatures are preserved. Original TU identity is not claimed: existing
deliberate D39/D40 extensions include a larger encoded buffer and an extra
plaintext-debug global. Counter transitions retain the same key wrap; unsigned
byte compound addition and original char result are equivalent modulo256.
No global or per-file flags, assembly/register spellings or padding changes.

Replay tools/playbook_encode_carrier.py --domain the URL above. Artifacts in
build/playbook-encode-carrier retain baseline replay, commands, source/header/
object hashes, full inventories, disassembly and RTL. Unchanged source raw
reproduces saved production. No fuzzing or mutation execution. F11575.


## Retained object measurements

Complete comparison build300/300, zero failures. The production full TU
raw-reproduces the chosen direct-byte/ordinary-branch object; only encode.c
changes among300 saved baseline objects. Whole-tree869/1852 ->870/1852
exact,84,715 ->84,766 exact bytes, sole gain cEncodeChar/no losses.
Before census finished before reconstruction edit. Complete same-order
partial links remain DIFFERENT (strict exit1 each); positioned equality
68,315 ->68,320/943,398, allocated914,126 ->914,142 bytes. Exact section70/92,
symbol394/2,907 and relocation1,018/18,317 records unchanged. Increased
function alignment is measured; neither target exactness nor five positional
bytes establishes original whole-object identity. Artifacts:
build/playbook-encode-adoption/{before,tree-before.json,tree-after.json,partial}.


Fixed Gentoo make phase passed385/0, with existing t_encode and t_encodeplain
fixtures: all input bytes, key wrap, transcripts, interleaved calls and debug
plaintext controls. This is finite component evidence, not every byte/key
position combination or modem-lifecycle reachability. Gate log
/tmp/encode-adoption-phase.log; fixture logs build/period/t_encode.run.log and
t_encodeplain.run.log. Structural14,231 references/2,704 findings headings and
285 suites/10,038 static anchors clean, no retarget. No new fixture required:
the existing suites exercise both counter arms and shared-key consumers.
