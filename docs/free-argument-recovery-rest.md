# Remaining ignored free arguments and B103 owner lifetimes

F11610/F11611. At71a7aa1a, [eight-cell remaining API cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950785209) and
[four-cell B103 owner lifetime control](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950829223). This is independent extension
of F11609, not a prediction adopted solely from earlier exact-set gains.

FSE/SRE/ECC/PPS callees ignore second incoming scalar slots. Whole-blob direct
relocation census13 calls, all explicitly plant1: FSE andSRE each four callers
(V32FP_delete,V17RX_delete,V27RX_delete,V29RX_delete), ECC one (V32FP_delete),
PPS four (V32FP_delete,V17TX_delete,V27TX_delete,V29TX_delete). No ambiguous
inherited slot or address-taking. Restore conventional int unused formals,
observed literals and matching candidate-only public header overlays. Original
width/name/meaning remain unproved; no fresh/ownership semantics inferred.

Eight regimes cross grouped receiver FSE+SRE, ECC, PPS; complete11TUs,
88 valid compilations,37 function symbols and11 named data objects per regime.
All four callee TUs raw-merge across8 cells; their13 function bodies unchanged.
V32 yields8 distinct emissions and requires all three axes for full exactness;
each other caller TU yields2 emissions. Imports/exports/types/binding/visibility,
all named data bytes/canonical relocations/allocated nontext preserved.
Only seven caller delete bodies change, no bystanders or exact losses.

| Function | Before | Restored | Blob | Result |
| --- | --- | --- | --- | --- |
| V32FP_delete |256|293|293|EXACT|
| V17RX_delete |233|251|251|EXACT|
| V27RX_delete |174|191|193|SIZE2|
| V29RX_delete |202|220|220|EXACT|
| V17TX_delete |98|107|107|EXACT|
| V27TX_delete |98|107|107|BYTES9|
| V29TX_delete |126|135|135|EXACT|

Unmatched V27 bodies retain supported source arguments; that correct call
boundary alone does not establish full source/profile fidelity. Their residual
is initial setup, not missing calls or post-call owner loads: after third RX
MRF call and after first TX PPS call, canonical suffixes match. An independently
bounded direct-owner argument-expression test is a follow-up, not adopted here.

B103 retains15 relocation targets/call order/guards but cached dsp across eight
calls and hdx across two tone calls; blob reloads owners after calls (0x8ef08,
18,32,4c,63,71,82,93,a4 and0x8efdd/8eff8). Cross directdsp anddirecthdx source
expressions, removing corresponding locals only:254/257/270/272B for baseline/
hdx/dsp/both versus272B. Four emissions; only combined full hit B103FP_delete.
Other16 functions canonical bodies/relocations unchanged, including non-exact
create1922B; all3 named data objects/type/binding/allocated nontext preserved.
This is observable original source lifetime evidence, not a claim that ordinary
lifecycle tests exposed a prior behavioral bug. Existing t_b103create exercises
real answer/originate/loopback create/delete, null detector, allocation/free
counts/live zero. t_b103fp's reference-only deletion is not this boundary gate.

Initial B103 generator incorrectly assumed hdx was declared with initializer;
assertion stopped before any candidate compilation. Preserve invalid-generator.py
and invalid-generator.log, exclude invalid run, corrected four-cell rerun valid.
No compiler rejection of reconstructed source is inferred from apparatus error.

Production restores all13 arguments/four formals and fixed test consumers;
B103 uses direct child owners at calls. Supersede active stale omission comments
and historical F8876 for remaining APIs. No flags/register/stack/layout tweaks.
Census, full partial links and fixed phase results are recorded below.

Replay tools/playbook_free_arguments_rest.py and
 tools/playbook_b103_delete_lifetimes.py --domain <respective linked URL>.
Artifacts retain full saved .build-config flags (redundant mandatory define
retained), DSPLIB_REPRODUCE_BUGS appended by shared helper, Gentoo GCC3.4.2-r2
and executed selected assembler2.15.92.0.2, commands/source/header/overlay hashes,
full objects/RTL, changed-body disassembly and per-family complete-object audits.
No fuzzing or mutation execution.

Production verification: all300 period objects rebuilt, eight change; all12
selected consumer TUs raw-match promoted complete experiment cells. Four callee
objects raw-identical. Whole-tree888→894 /1852 exact,88,712→89,990 exact bytes
(+1278), six gains/no exact losses. Before census and all300 before objects,
config/manifest retained. Complete recovered-order partial links remain
DIFFERENT (strict exit1 both): matching positioned bytes68,581→68,614 /943,398;
allocated candidate bytes914,238→914,366; exact section70/92 andsymbol394/2907
records unchanged; exact relocation records1027→1023 /18317. Function gains
coexist with fewer positioned relocation matches after layout moves. Do not
claim monotonic whole-object fidelity or recovered original profile from this.
First period differential substage385 passed/0failed, but full phase was red
on one detached static anchor. Repaired full phase passes385 tests/0failures
and all structural/provenance gates;285 suites/10038 static anchors unique.

The first phase attempt exposed one detached static anchor in v32c_fpsub:
its old FPM_FSE_free call spelling omitted the restored argument. Retarget
find/replace to keep literal1 and preserve the same wrong-offset detector;
no mutation execution. First gate log is preserved as red; corrected full
fixed phase rerun is green. The red structural attempt is not relabelled
from its passing differential substage.
