# Expansion-temporary transfer results

Baseline3986addf,1082/1852 exact, fixed Gentoo profile/bug define. This pass
transfers the PSD diagnostic to other functions and retains one supported
partial recovery; no new byte-exact function is claimed.

## Screen and raw controls

778 residual copies/770 names;92 floating copies/90 names up to700 original
bytes. Prologue frame gaps are nominations, not causes. Historical PSD getter
SIZE1 versus current EXACT77B is the screen's positive/negative frame control.
Three complete-TU preparations (VpcmFloModem, FloatFIR, V90Demodulator) raw-reproduce
production and retain73 shared-body verdicts. The existing spectral shaping
native/capture controls were reviewed and not repeated.

Thirteen saved/plain/observed raw-equal compiler observations report55 stack
requests. SineWave:7 observations/31 requests; FloatFIR:1/9 across two overloaded
methods; AGC process-name group:5/15. The generic observer now records assembler
names and function pointers in frame observations to resolve overloaded or
unrelated methods named process. AGC and Descrambler fire as separate groups.
No compiler state is written, no inferior function is called.

## SineWave recovery

Six scalar/sine-return/entry cells,three phase-read controls,two publication
controls:11 retained cells/374 shared-body verdicts. Native Tparam producers
remove the SF4 request through gen_truncxfsf2; integer truncation still needs
HI2/HI2/SI4, plus BLK0 reload. Locals12→8, matching original8. Sinl/sinf emit
identical complete objects within each scalar entry class; another trig
spelling experiment is not warranted.

Retained entry stays SIZE7. Common entry reaches SIZE3/131B. Delaying the first
phase acquisition until the positive count guard, then restoring original i<n
orientation, reaches134B/BYTES50. Its complete nonbranch instruction/operand
multiset matches original, including register names, constants and memory homes.
Its zero-length block sits before the common wrap rather than after return;
three cold/exit jump positions differ. This is a layout residual, not a missing
arithmetic operation. The independently observed publication-before-output
control worsens to SIZE12 and is declined. No hand-positioned goto/padding
controls were used. Scalar/phase-read control is the retained partial recovery.

Only generate changes:33 shared bystanders, symbol metadata, named data and
non-executable allocated data are unchanged. One defining SineWave copy exists
among all300 canonical objects, owned by VpcmFloModem. Production comments are
updated to describe period scalar-mode excess precision, and its complete raw
object equals the independently compiled chosen control despite comment cleanup.
The zero-length step/wrap, unrounded fsin argument, unconditional final advance,
intermediate phase stores and signed-int overflow boundary remain preserved.

FloatFIR also has an SF4 request in both overloads, with five additional SI
reload spills in the buffer method. Original scalar method already reserves4B;
this is not an unexplained extra word and does not justify rewriting its
convolution return arithmetic. No FloatFIR source is changed.

## AGC falsifies a broad scalar rewrite

Unchanged Agc process has SF4 gen_truncdfsf2 plus BLK0; padding expands4 locals
to20 reserved bytes, where original reserves none. Descrambler contributes one
separate BLK0 request. Four native producer/floor cells/124 body verdicts all
retain the AGC request. Both axes reduce SIZE23→15 but create fstps/flds at the
sqrt result absent in the original. A detector clears original/retained baseline
and fires on that candidate. Smaller size does not justify this new narrowing.
Producer controls also change V90Demodulator reset/progress and .rodata.cst4;
full-TU audit reports these coupled callers. No AGC source adopted and no
candidate-runtime claim made. Validated double sqrt helper remains unchanged.

## Integration and conclusion

Fifteen retained source-control cells/498 body verdicts, plus73 preparation
verdicts; repeated two-cell phase runs reproduce final retained controls and
are excluded from distinct-control denominators. Audit checks all body changes,
metadata/data/allocated section changes, source overlay hashes, observation
requests, defining-copy census and selected canonical object.

Exactly1/300 objects changes,299/configuration unchanged. Census remains
1082/1852 exact and119759 original exact bytes, no gains/losses; SIZE671→670,
BYTES63→64,REGALLOC31/UNRESOLVED5 unchanged, ratchet green. make phase J=8 exits0:
388 passed/0 failed, structural checks green,285 suites/10038 static anchors
attached/unique/correct-arm. No static locators needed editing; none executed.

Faithful300-input partial .text702012 unchanged. Generate section127→134B,
positioned equal10→75/134; allocated positioned equal69312→69377/943398,
allocated candidate bytes917698→917705. Both strict comparisons remain
DIFFERENT/exit1. Function residual classification and partial-link byte gains
are separate measurements, not whole-object closure. No modern portability claim.

Close tested SineWave scalar/read/publication and AGC producer/floor domains.
Further SineWave work needs independently evidenced cold-block layout/pass
ownership; further AGC work needs an explanation preserving the original sqrt
precision and reciprocal/multiply graph. The screen has90 small floating names,
not90 actionable extra temporaries, and does not classify larger bodies.

## Replay

```
python3 tools/gcc3_expansion_transfer_screen.py --objects build/expansion-transfer-before-objects
python3 tools/gcc3_expansion_transfer_prepare.py --domain docs/history/compiler/expansion-temporary-transfer-domain.md --historical-headers --baseline-dir build/expansion-transfer-before-objects
python3 tools/gcc3_expansion_transfer_prepare.py --domain docs/history/compiler/expansion-temporary-transfer-domain.md --agc --historical-headers --baseline-dir build/expansion-transfer-before-objects
python3 tools/gcc3_sine_scalar_transfer.py --domain docs/history/compiler/expansion-temporary-transfer-domain.md --historical-headers --baseline-dir build/expansion-transfer-before-objects
python3 tools/gcc3_sine_phase_boundary.py --domain docs/history/compiler/expansion-temporary-transfer-domain.md --historical-headers --baseline-dir build/sine-phase-baseline
python3 tools/gcc3_sine_publication_boundary.py --domain docs/history/compiler/expansion-temporary-transfer-domain.md --historical-headers --baseline-dir build/sine-publication-baseline
python3 tools/gcc3_agc_scalar_transfer.py --domain docs/history/compiler/expansion-temporary-transfer-domain.md --historical-headers --baseline-dir build/expansion-transfer-before-objects
python3 tools/gcc3_expansion_transfer_audit.py
```

Before-objects archive has all300 actual pre-adoption objects/config, verified
against objects-before.json. Phase baseline uses scalar-sinl-entry object;
publication baseline uses phase-after-guard-less object. All overlays read pinned
Git header3986addf so adoption does not silently change experimental inputs.
Read-only observations use gcc3_uref_stack_observe.py --generic-trace with these
reproduction roots and (family,target): (VpcmFloModem,generate), (FloatFIR,process)
and (V90Demodulator,process). Observer output roots are named in the audit.
Retain logs, complete commands, hashes, raw controls and refused attempts.

Artifacts: build/expansion-transfer-screen.json, expansion-transfer-audit.json,
expansion-transfer-integration, expansion-transfer-census.json and
expansion-transfer-phase.log. No fuzzing or mutation execution/profile change.
