# Restore the wrapper destructor's nonnull contract

F11629. [Predeclared two-cell experiment](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5953263940)
at0cb87c39 removes only dp_wrapper_delete's added null early return.
The blob loads its argument0x5a24 and immediately writes zero through it0x5a28.
The retained source instead accepts null and has a separate return. This is
an actual contract difference, not a register-allocation hypothesis.

Baseline90B reproduces the production object. Removing the guard recovers
complete85B EXACT,0→1/3 common functions, no losses. Both other functions
remain canonically unchanged; one named data object, all allocated nontext
contents, symbol types/binding/visibility/imports/exports agree. Two valid
compilations yield two distinct emissions. Preserve the initial dp_data clear,
both child-pointer guards, calls to RcFixed_Delete and final sysdep_free.
No additional source or flag variants were tried.

Factory partial-allocation unwind passes an already allocated wrapper. Scoped
B103/V23/V22/V32 caller review finds actual allocation lifecycles or guards;
this does not claim that every external caller is known. The header promises
release of a constructed wrapper, not a null no-op. Preserve the blob contract
rather than silently widening it. No null-crash fixture or tolerant comparison.

Fixed t_dp_wrapper contains14 paired component scenarios (12 accepted, two
rejected geometries). Successful cases construct real wrappers and rate
converters, process bulk/ragged/single-sample histories, and delete them. Failed
construction peers are guarded before deletion. The datapump is synthetic;
cleanup occurs after diff_end, so this is normal-lifecycle and processing
coverage, not independent complete teardown accounting. No new test needed
for a sole removal of unreachable-on-valid-input early return; existing gate
must pass on the adopted source. No fuzzing or mutation execution.

Artifacts build/playbook-dpw-delete-guard preserve actual complete Gentoo
GCC3.4.2-r2 flags, DSPLIB_REPRODUCE_BUGS, executed assembler2.15.92.0.2,
source/header/command/object hashes, RTL and full-object audit. Replay
with tools/playbook_dpw_delete_guard.py and the linked --domain URL.
Adoption artifacts build/playbook-dpw-delete-adoption save all300 baseline
objects/config/manifest. Exactly one production object changes and raw-matches
the promoted experiment. No mutation anchors required retargeting.

Fixed Gentoo make phase passes385 tests/0 failures and all structural checks.
Whole-tree915→916/1852,94,440→94,525 exact bytes; sole gain dp_wrapper_delete,
no losses. All300 production objects inspected: exactly one changes and raw
matches the promoted cell. Complete same-order300-object partial links remain
DIFFERENT (strict exit1 before/after). Positioned equal bytes68,697→68,698 /
943,398 (+1); allocated914,558 unchanged because function alignment absorbs
the shorter body. Exact sections70/92, symbols394/2907 and relocations1026/
18317 unchanged. Function recovery does not establish whole-object identity.
No modern portability claim.
