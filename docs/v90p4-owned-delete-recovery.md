# V90 phase-four owned converter deletion

F11617. Baseline6d8e4a1c,899/1852 exact,90792 exact bytes.
[Predeclared three-cell complete-TU controls](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5951790899).

Blob D1/D2 are94B; the manual member destructor/free bodies were83B. Blob
preserves the evaluated bitsToSymbol pointer across destructor and free.
Current source re-read that member after the destructor call. The typed
V90BitsToSymbol destructor/owned member and externalBitsToSymbol guard support
ordinary scalar delete under the ownership guard. Its own null guard and
one pointer evaluation explain the observed lifetime without forced registers.
F10192-adjacent prior controls tried combined/nested cached-pointer guards,
not this language construct; no cache/declaration/profile sweep is reopened.

Three complete cells: retained source; ordinary inline unsized scalar
operator delete forwarding sysdep_free immediately after the final include,
body retained; same adapter and owned member delete. Adapter-only wholeobject
raw-identical to baseline. Member delete independently recovers both complete
94B bodies including relocations:37→39 exact/53 functions, no losses. All51
other bodies unchanged, zero named data objects. All symbol types/binding/
visibility/imports/exports/defining sections and allocated nontext contents/
flags/alignment/relocations agree; no extra operator import or helper symbol.
This supported expression family is not a unique original declaration or an
ordinary lifecycle bug claim. Preserve automatic Scrambler member destruction
and dangling member/ownership flag, which the object never clears.

The old in-file explanation treated destructor recovery as a precondition for
setRdRtSymbols/setRfSymbols/recivedCPtag recovery. All51 bystanders remain
unchanged after the exact destructor recovery; that is a counterexample to
assuming the fix is sufficient for those residuals. Keep compiler cursor and
other non-exact body stages as separate hypotheses, not a shared solved cause.

Fixed t_v90modchain covers real C1/C2 owned/supplied converter construction
and both D1/D2 releases, including allocator balance and untouched supplied
converter. Manually pre-released then planted NULL/external arms are synthetic
guard probes, not lifecycle reachability. Keep both classes of evidence.
Six static anchors retarget ownership polarity/omission, intrinsic null guard,
destroy/free omission and dangling clear defects. Null-guard removal must
open-code the unguarded destructor/free, since deleting NULL alone remains
safe. No fuzzing or mutation execution, no invented alias fixture.

Replay tools/playbook_v90p4_owned_delete.py --domain <linked URL>.
Artifacts build/playbook-v90p4-owned-delete preserve saved actual complete
flags/mandatory DSPLIB_REPRODUCE_BUGS/Gentoo GCC3.4.2-r2/executed
assembler2.15.92.0.2, source/header/command/object hashes, complete audits and
changed-body disassemblies. All300 period objects rebuilt: exactly one changes and raw-matches the
promoted complete experiment cell. Census899→901/1852,90792→90980 exact
bytes, two gains/no losses. Fixed make phase385 passed/0 failed, all
structural gates clean, static anchors285 suites/10038 zero detached/nonunique.
Complete same-order partial links remain DIFFERENT; positioned68786→68778
/943398 (eight fewer), allocated914510 unchanged; exact sections70/92,
symbols394/2907 and relocations1024/18317 unchanged. Preserve the mixed
layout result, not a whole-object gain claim. Artifacts under
build/playbook-v90p4-owned-delete-adoption. No whole-object/original-profile
identity or modern portability claim.
