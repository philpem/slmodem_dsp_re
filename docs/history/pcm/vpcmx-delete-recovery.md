# VPCMXF typed modem deletion

F11623. Baseline35096c95,914/1852 exact/94331 exact bytes.
[Predeclared three-cell control](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5952569328).

Blob VPCMXF_Delete109B at0xf6c0 captures EBX, inlines six typed member
destructors0xf6d5..0xf71b, then calls sysdep_free0xf723 and returns through
one epilogue. Retained117B wrapper has the same six calls but a sibling free
jump and duplicated epilogue. Its actual class-with-destructor scalar delete
is Playbook lever7's non-sibling family, unlike POD scalar deletion. The old
F7818 C-leaf exclusion is superseded by independently recovered F11430 FILE
provenance: wrapper and declared class destructor belong in VpcmFloModem.cpp.
No file renaming or inferred language change is needed to test this expression.

Three full-TU cells: retained baseline, inline unsized scalar host adapter
after last include only, adapter plus `delete self`. Baseline reproduces full
production object; adapter-only raw-merges. Ordinary class deletion recovers
complete109B wrapper, exact22→23/34 functions, one gain/no losses. All33
bystanders unchanged, including both97B destructor clones, creators and
nonexact bodies. Three named data objects and full symbol type/binding/
visibility/import/export/allocated nontext contents/flags/alignment/relocations
agree. Preserve same-TU destructor definition and all six generated embedded
releases; do not hand-write the six calls or invent an array deletion.

Fixed t_vpcmctor constructs through VPCMXF_Create then deletes real heap
objects through this wrapper, compares allocator counts/live0 and transcript
behavior. It also exercises null deletion as a separate boundary; component
seeding is not end-to-end modem reachability. Its direct destructor-symbol
probes are separately labelled fixtures. Two static anchors preserve missing
null guard and missing destruction defects. Null `delete` keeps a language
guard, so the guard-removal defect must still explicitly call the destructor
and host free. No fuzzing/mutation execution.

Replay tools/playbook_vpcmx_delete.py --domain <linked URL>. Artifacts
build/playbook-vpcmx-delete retain full actual saved flags, mandatory
DSPLIB_REPRODUCE_BUGS/Gentoo GCC3.4.2-r2/executed assembler2.15.92.0.2,
source/header/command/object hashes, RTL, disassemblies and complete object
audit. All300 production objects checked: one changes, raw-identical to the
promoted full cell. Adoption artifacts under build/playbook-vpcmx-delete-adoption.
No modern portability claim.

Whole-tree census914→915/1852,94331→94440 exact bytes, one gain/no losses.
Complete same-order300-object partial links remain DIFFERENT, both strict
exit1. Positioned68568→68697 /943398 (+129), allocated914574→914558 (-16),
exact sections70/92 and symbols394/2907 unchanged; relocations1030→1026
/18317 (-4). Retain this mixed layout result without claiming whole-object
or original-profile identity.

Fixed make phase385 passed/0 failed and all structural gates clean;
285 suites/10038 static anchors zero detached/nonunique. Updated ledger
refcheck14266 references/2752 headings, zero unresolved/stale entries.
