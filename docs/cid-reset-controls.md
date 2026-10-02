# CID reset control and zero-result experiments

F11624/F11625. Baselinea7dfe334, retained915/1852 exact/94440 exact bytes.
The later CID core FILE is src/service/cidcore/cid.c (ten functions), distinct
from src/service/cid.c's three wrappers with the same basename.

[Two-cell control placement](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5952737363).
Blob cid_reset118B tests automatic mode at0x8fd24/0x8fd27 then duplicates
samples_fill clearing in the nonautomatic0x8fd2b and automatic0x8fd45 arms.
The latter loads fsk0x8fd40 before its clear and mark_conf_step store.
Retained110B source clears once before the mode test. Test retained source
against explicit clears inside both branches, with the automatic clear before
the mark store. Baseline reproduces full object; candidate118B/BYTES8 changes
only cid_reset. Exact6/10 unchanged, no gains/losses. Eight differing rows
contain register choices, but existing alpha control rejects at row25 USE
CONFLICT across the physical backward-entry block. Do not claim a successful
whole-function alpha control or uniquely source-authored store duplication.

[Four-cell return/control cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5952797495).
Both actual blob exits zero EAX after the last receiver call (0x8fd29 and
0x8fd43) and perform no subsequent EAX writes/calls. That is an observed
zero-result ABI property, not proof of the original return declaration.
The reconstruction says void; no original cid_reset declaration was found
in scoped ref/slmodemd sources, and current callers discard its result.
This evidence is weaker than caller result consumption. Cross ordinary int
return0 with the two control placements using isolated matching cid_modem.h
prototype overlays. Baseline/common-void110B/SIZE8; branch-void118B/BYTES8;
common-int112B/SIZE6; branch-int120B/SIZE2. No exact gains/losses, all6/10.
Both repeated void controls raw-reproduce the earlier two-cell experiment.
Six valid compilations across the two domains yield four distinct objects.
No empty-domain/invalid runs, return-type or register permutations.

Complete audits cover ten functions per cell, zero named data objects;
all nine other canonical bodies and symbol type/binding/visibility/imports/
exports and allocated nontext contents/flags/alignment/relocations agree.
No production source/header/test changes adopted. Whole-tree remains915/1852;
no new differential verdict or partial-link improvement claim. These controls
close only the declared control/return family under the retained profile.
A nearer size is not a recovery and failure does not disprove an original
int return, void incidental zero or optimizer-created tail duplication.
Next evidence must concern a distinct source boundary rather than adjacent
condition/return-expression/unsigned/bool spellings.

Fixed t_cidsvc lifecycle has18 constructed pairs (six initial modes×three
CID values), with explicitly dirty sample/receiver state before reset and
forced mode3 clamp when both receivers exist. Those are component probes,
not a proof of public modem history. Separately allocated CID/FSK ownership
is established by create; do not fabricate overlapping ownership or volatile
access to force instruction order. No inferred behavioral bug, fuzzing or
mutation execution. No fixed fixture was needed to authorize a source change,
because none was adopted.

Replay tools/playbook_cid_reset_control.py and tools/playbook_cid_reset_return.py
with their linked --domain URLs. Artifacts build/playbook-cid-reset-control and
build/playbook-cid-reset-return preserve actual complete retained Gentoo
GCC3.4.2-r2 flags, DSPLIB_REPRODUCE_BUGS, executed assembler2.15.92.0.2,
source/header/overlay/command/object hashes, RTL, disassemblies and complete
object audits. Generator controls fire on known baseline input,2/2 and4/4
source variants distinct. No whole-object profile or exhausted-search claim.
