# Restore ignored MRF/FSD free arguments

F11609. At97c06e4e, [four-TU MRF×FSD cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950545258), then
[all-consumer baseline/restoration validation](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950569932). The source had deliberately
omitted a caller argument because its callee never reads it. That preserves
observable behavior, but does not recover source call boundaries or bytes.
F8876's one-argument inference is superseded for MRF; history is retained.

| Caller | MRF value / relocation | FSD value / relocation |
| --- | --- | --- |
| V32FP_delete |1 /0x7f873|—|
| v23FP_rx_delete |0 /0x86fa3|0 /0x86fb4|
| B103FP_delete |1,1 /0x8ef55,0x8ef6c|1 /0x8ef3e|
| cid_delete |1 /0x8ff07|—|
| V17RX_delete |1 /0x97bac|—|
| V21RX_delete |1 /0x992b2|1 /0x9929b|
| V21TX_delete |1 /0x99615|—|
| V27RX_delete |1 /0x99f5e|—|
| V29RX_delete |1 /0x9b5dd|—|

Whole-blob relocation census:10 MRF calls in9 functions,3 FSD calls. Every
second slot is explicitly written before the call, including V23's XOR-zero
stores; no ambiguous inherited slot, address-taking or other relocation type.
Both callees ignore that slot. Recover conventional int unused second formals,
not a guessed fresh/ownership flag; original width/name/meaning remain unknown.
Varargs/no-prototype cannot be excluded from unread callee alone. Explicit
fixed formals are idiomatic and reproduce every measured caller/callee here.

Initial4TUs×4regimes: MRF alone recovers cid_delete; only MRF+FSD recovers
v23FP_rx_delete. Both callee TUs raw-merge across all regimes. Candidate-only
header overlays record hashes and full commands, production headers unchanged
during experiments; no incompatible function-pointer casts. All-consumer
11TUs×2regimes reviews58 functions and29 named data objects. All symbol types,
binding/visibility, data values/canonical relocations/allocated nontext bytes
preserved. Nine caller objects change; both callee objects remain raw-identical.
Every retained baseline raw-reproduces. No exact loss in either experiment.

| Recovered function | Before | Blob/current |
| --- | --- | --- |
| cid_delete |82B|86B|
| v23FP_rx_delete |77B|89B|
| V21RX_delete |105B|123B|
| V21TX_delete |95B|104B|

Other changed delete bodies improve their call setup but remain non-exact:
B103220→254vs272B,V32247→256vs293B,V17224→233vs251B,
V27166→174vs193B,V29193→202vs220B. Untouched B103FP_create changes1926→1922B
versus2151B, with different scheduling/register choices and one redundant XOR
removed; this is not pure register renaming. It retains all call targets,
field values and conditional paths. All remaining48 canonical function bodies
unchanged. Inspect complete non-exact bystanders rather than claim TU fidelity
from four exact functions alone. Artifacts retain every changed disassembly.

Production: restore both API headers/definitions and all13 source arguments;
adapt six fixed test consumers. Public source API now requires the observed
second scalar; machine calling convention and ignored-value behavior remain.
Correct stale source comments and visibly supersede F8876. Existing fixed
CID/V23/V21 lifecycle tests use real source/reference-created handles and
compare frees; no extra alias fixture is needed for an unread parameter.

Verification: all300 period objects rebuilt, nine change, all11 consumer
objects raw-match promoted complete experiment cells. Whole-tree census
884→888 /1852 exact,88,310→88,712 exact bytes (+402), four gains/no losses.
Before census is the last authoritative unchanged-source census saved with
all300 before objects, config and manifest. Complete recovered-order partial
links remain DIFFERENT (strict exit1 both): matching positioned bytes
68,254→68,581 /943,398; allocated candidate bytes914,142→914,238;
sections70/92 andsymbols394/2907 unchanged; relocations1020→1027 /18317.
This is partial-link progress, not a whole-object equality claim.

Fixed reconstruction gate:385 period differential tests passed,0 failed;
structural/provenance gates clean. Existing CID allocator-balance and V23
create/delete checks pass. Tests compare caller/callee behavior, not the
ignored formal's unknowable original type or name.
No fuzzing or mutation execution. Replay tools/playbook_free_arguments.py
and tools/playbook_free_arguments_all.py --domain <respective linked URL>.
Artifacts build/playbook-free-arguments[-all] include actual complete saved
flags, mandatory DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and executed
assembler2.15.92.0.2, header/source hashes/overlays/commands/full objects/RTL
and per-family complete-object audits. Adoption artifacts preserve before/
after census,300 objects, changed-object list and full partial-link controls.

Next independent domain, not compiled/adopted here: FPM_FSE_free and
FPM_SRE_free each have four explicit literal1 caller slots; ECC has one,
PPS has four. All four callee bodies ignore the second incoming slot.
Grouped receiver FSE+SRE × ECC × PPS would form eight cells, targeting V32,
V17/V27/V29 receive deletes and V17/V27/V29 transmit deletes. B103 calls none
of those four and needs an independent explanation. Reaudit actual literals,
consumer declarations and fixed lifecycle gates before adoption; do not infer
more exact bodies from this wave's success.
