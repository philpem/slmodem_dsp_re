# V23 diagnostics and return-boundary recovery

Baseline861901c2; fixed Gentoo profile and DSPLIB_REPRODUCE_BUGS. Five functions
recover original behavior/dataflow; no new exact function is claimed.
[Predeclared domain](missing-debug-boundary-domain.md).

## Screen and witnesses

The mnemonic screen covers C data/voice files outside fax, original <=350B;
149 TUs/599 shared bodies yield135 eligible residual copies. Counts are not a proof of dataflow, semantics
or source preimages. Known omitted create call fires; exact dp_v23_init clears.
Original v23_create has an unsigned debug>1 test and original create string/call
before allocation; source carried only a comment. Original process compares
translated datapump status but prints the preserved raw modem status, then
publishes translated status. The two strings and callback boundaries come from
original relocations/disassembly, not neighboring source spelling guesses.

Five full-TU cells/25 shared-body verdicts: baseline, each diagnostic separately,
both, and adjacent ID/modem assignment reversal. Both recoveries are retained;
assignment reversal is rejected (BYTES10→17). An initial audit erroneously
assumed source reversal was raw-inert; direct body comparison refuted that
assumption and the audit was corrected. Source order remains unchanged.

## Caller and callee return ownership

Preserving raw status exposes the wrong short prototype: original process uses
complete EAX, whereas our short V23ModemMain result becomes MOVSWL AX before
printf. Initial RTL already carries HI return plus sign extension of the result
pseudo; changing the signature removes this before allocation/scheduling.
Original modem-main widens short v23FP_rx_progress and directly returns full EAX
from BwChDem_Progress. Both latter declarations/definitions become int together;
receiver remains short. BwChDem return paths all produce extended status-field
values or literal2; its complete TU is raw-inert under this type-only control.
No conflicting TU declarations, function-pointer casts or profile workaround.

Four cells crossed over8 TUs/92 shared-body verdicts preserve all exports, named
data and bystanders; debug recovery adds the two original undefined host imports.
Header-visible V23 filter/dp-init/Rxcid controls are included. Type-only modem-main
SIZE122→94; the missing diagnostics explain much of the rest. Short return in
the two reference fixture declarations is corrected to int as well.

## Lifecycle exposes seven further omitted sites

The extended existing direct lifecycle captures each create/process boundary
at levels1/2/3, compares complete transcripts including callback markers, checks
for capture overflow, and requires original create/raw-zero status messages to
fire above the correct threshold. Unchanged-source negative: period0/1; terminal
4/51016 and host13/68016 failures at levels2 and3, level1 passes. After glue
recovery, first full phase is387/1: remaining2 terminal and11 host transcript
failures per enabled level reveal missing component messages.

Direct object verification finds seven additional original sites: modem state
report; receiver tone count, acquisition deadline and energy-drop deadline;
Bw tone report, no-carrier deadline and waiting-for-signal deadline. Restore
exact messages, CRLF/newline differences, level>1 guards and original field
update/callback order. Receiver count prints the narrowed state/5 after its
increment; modem prints reported→state before reporting publication. Three
complete-TU baseline/candidate pairs/18 shared verdicts recover these sites.
Only component progress/main bodies change; constructors/delete/data unchanged,
except original diagnostic strings and Bw's two newly restored host imports.
Original callback-count/string controls fire (1 modem,3 Bw,3 receiver sites).

Totals43 full-TU compilations/135 shared-body verdicts. Selected four production
objects reproduce independent candidate objects raw, including comment cleanup.
Source/header changes touch five function bodies; no exact gain/loss. Do not
interpret modem-main SIZE2 as near instruction identity: its remaining frame,
state dispatch, loop, register and sibling-call boundaries differ substantially.

## Fixture audit and coverage limits

Existing normal histories compare outputs/whole glue state and reach carrier,
get/put callbacks and status transitions. Add cold-start no-signal and genuine
post-carrier signal-loss histories; no fields are planted. Acquisition needs
3100 frames to cross the original60000ms deadline. The initial1300-frame claim
was a fixture error, not a source failure. Original host retains carrier after
24 seconds of the tested silent tail; asserting timeout there was also wrong.
Keep it as a measured retention control with expected no error/message; do not
change source to satisfy an imagined timeout. Full value/whole-state/transcript
comparisons remain exact for every frame. No cause for that retention is claimed.

Both acquisition histories and terminal signal-loss history produce timeout
outcomes/messages at the measured levels. The host loss history does not exercise
Bw's no-carrier message. Its original conditional site is recovered from object
control/relocation evidence; no claim that all nine sites fire in these histories.
The fixture initially had a missing closing brace; compiler rejection was
apparatus failure, excluded and repaired. No mutations, fuzzing or tolerances.

## Replay

Archive all300 pre-pass canonical objects, .build-config and tc_manifest.txt at
build/v23-debug-before-objects. Census baseline is1082/1852. Pinned header mode
keeps later adoption from changing experiments:

```sh
python3 tools/gcc3_structural_residual_screen.py --objects build/v23-debug-before-objects --census build/sine-cold-layout-census.json --out build/structural-residual-screen.json
python3 tools/gcc3_missing_debug_boundary.py --domain docs/missing-debug-boundary-domain.md --historical-headers --baseline-dir build/v23-debug-before-objects
python3 tools/gcc3_v23_status_width.py --domain docs/missing-debug-boundary-domain.md --historical-headers --baseline-dir build/v23-debug-before-objects
python3 tools/gcc3_v23_lifecycle_debug.py --domain docs/missing-debug-boundary-domain.md --historical-headers --baseline-dir build/v23-debug-before-objects
python3 tools/gcc3_v23_debug_audit.py
make phase J=8
```

Audit checks every cell hash, raw baseline, existing metadata/named data,
restored undefined imports, changed allocated string sections, bystanders,
exact sets and selected raw production. It detects the extra HI extension on
the short-return control and its absence on selected/original assembly, while
initial RTL attributes the difference before allocation. Complete commands and
compiler/selected assembler identities are retained beside results.json.
Final make phase J=8 exits0:388 passed/0 failed, structural checks green,
285 suites/10038 static anchors clean. Direct fixture22 groups/4656926 checks
pass. Final census1082/1852 EXACT/119759 exact original bytes unchanged, no losses;
SIZE670→669, BYTES64→65, REGALLOC31/UNRESOLVED5 unchanged; ratchet green.
Four/300 canonical objects change,296/configuration unchanged. All300 faithful
objects equal canonical raw. Partial .text702012→702380; allocated candidate
917705→918388, positioned equal69377→69537/943398. Both strict original comparisons
remain DIFFERENT/exit1. New strings and callback relocations explain growth;
these are function-level recoveries, not whole-object identity. No modern
portability claim. Refcheck against861901c2:14429 references retain their targets;
final dangling check14429 references/3058 headings is clean. Tool syntax and
whitespace checks pass. Final publication make phase repeats388/0 with the
bounded mismatch-transcript reporter; all22 lifecycle groups remain green.

The next discriminators are modem-main state dispatch/loop ownership and sibling
calls, plus the two receivers' original AGC return-value use versus field reload.
These need independent source/ABI witnesses and complete-TU experiments; the
remaining size distances do not authorize operand/local/history permutations.
