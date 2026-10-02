# V92 echo canceller owned ARMA deletion

F11619. Baseline046d129d,903/1852 exact/91378 exact bytes.
[Predeclared three-cell full-TU control](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5952158174).

Blob D1 captures member arma(+4) into EBX at0x10c16 and passes that same
pointer to FloatARMA D1 at0x10c93 and sysdep_free at0x10c9b. The reconstructed
manual member calls reloaded +4 after destruction. FloatARMA is a typed
nonvirtual class owner. The old source inference that scalar delete cannot
emit two calls was false; F7786 had withdrawn the allocator-name inference
but did not measure this same-TU ordinary member-delete family.

Three cells: retained source; TU-local inline unsized scalar delete adapter
forwarding sysdep_free after the last include; adapter plus `delete arma`
inside the retained non-null guard. Baseline reproduces the production full
object; adapter-only raw-merges. Member delete recovers both195B D1/D2 from
128B,3→5exact/15 functions, no losses. All13 bystander bodies unchanged.
Two named data objects, symbol types/binding/visibility/imports/exports,
allocated nontext contents/flags/alignment/relocations agree across all cells.
Preserve both primitive buffer free-and-clear paths, the debug gate and the
conditional arma=NULL store. No unsupported array or pointer-clear axes,
register/layout forcing or inferred lifecycle behavior bug.

Fixed t_v90leaves run_ec_ctor constructs actual EchoCanceller and FloatARMA
children, drives component state/process and releases real allocations.
run_ec_dtor manually populates FloatARMA member shape and probes guards;
these are synthetic component fixtures, not constructor reachability evidence.
Both destructor clones are covered. Three static anchors preserve missing
clear, missing destruction and clearing before free defects; no fuzzing or
mutation execution.

Replay tools/playbook_v92ec_owned_delete.py --domain <linked URL>. Artifacts
build/playbook-v92ec-owned-delete retain actual complete saved flags,
DSPLIB_REPRODUCE_BUGS/Gentoo GCC3.4.2-r2/executed assembler2.15.92.0.2,
source/header/command/object hashes, RTL, changed-body disassemblies and
complete object audit. Production rebuild changes exactly one of300 objects,
raw-identical to the promoted cell.

Complete same-order300-object partial links remain DIFFERENT, strict exit1
for both controls. Positioned equal bytes68770→68347 /943398 (423 fewer),
allocated bytes914510→914670 (+160). Exact sections70/92 and symbols394/2907
unchanged; exact relocations1024→1021 /18317. Recovering complete functions
therefore does not establish whole-object/profile identity. Adoption artifacts
under build/playbook-v92ec-owned-delete-adoption. No modern portability claim.

Whole-tree census903→905/1852,91378→91768 exact bytes, two gains/no losses.
Fixed make phase385 passed/0 failed and all structural gates clean;
285 suites/10038 static anchors zero detached/nonunique. Updated ledger
refcheck:14257 references,2748 headings, zero unresolved/stale entries.
