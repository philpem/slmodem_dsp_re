# JdNotDetector arm order and the amplitude read-extension screen

Two domains posted before compilation
([#22](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5965974257)),
same worktree/profile as F11671-F11672. One adopted gain, one measured
negative. Artifacts: `build/playbook-amplitude-ext-jdnot-arm/results.json`.

## Adopted: JdNotDetector's reset arm spelled first (75 bytes, EXACT)

The standalone `V90Phase3Demodulator::JdNotDetector` was transcribed with
the increment arm first; the blob's block layout branches on
`symbol != 0` to the increment block and falls through to the reset. The
condition-swapped spelling emits the blob's 75-byte body exactly. Before
compiling, independent corroboration: `getV90Decision`'s inline copy of the
same test already reads `if (bit != 0) jdNotRunLength = 0; else
jdNotRunLength++;` and the v90p3ddec mutation anchors quote that spelling.
The suite's JdNotDetector anchor was retargeted to the adopted spelling with
the fault case preserved (reset/increment swapped is still a real fault);
anchorcheck clean. Census 927 -> 928 / 1852, bytes 95,489 -> 95,564, sole
gain, zero losses; fixed phase 387/0. The ternary spelling did not match.

## Closed: amplitude read-extension casts (no gain, no change)

`sym = (unsigned short)amplitude;` in generateCPt and generateE1u emits a
byte-identical object: the load's extension follows the field's declared
type through nop casts, so a cast at the read cannot move it. The raw
0x40-displacement probe across the TU was also contaminated -- `0x40(%reg)`
matches non-this bases -- so its 12z/7s tally is not evidence about the
field. `short amplitude` stands on its documented (weakest-tier) inference;
no retype, no adoption. The blob's movzwl loads of this field at CPt/E1u
remain an open question tied to whatever lvalue the author read it through.

## Scope

Both spellings are value-identical; no differential or fuzz change. No
fuzzing or mutation execution; the one anchor retarget is static and
recorded above. Canonical verdicts via byteident.py body/verdict only.
