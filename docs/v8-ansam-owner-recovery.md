# V8 ANSam typed tone-owner recovery

At5000b4aa, v8_ansaminit is106B against85B reference. The blob forms a typed
tone-subobject pointer at owner+0xda4, writes its six non-envelope-phase
fields through compact offsets, retains that pointer across v8_mpyint, and
writes envelope_phase through the owner. Retained source uses full owner
expressions for all seven fields. Values, widths and scaling arguments agree.

The [declared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5955822738)
keeps production and caches struct v8_tone *t=&v->tone at entry, replacing
only the six observed compact-offset field accesses. Envelope phase remains
owner-relative. All source statement order, stores, argument types and other
functions remain. The pointer belongs to the same embedded object throughout
the external call; no unknown child lifetime or dynamic allocation is added.

The candidate recovers85B EXACT. Only ansaminit changes across nine emitted
functions/one named data object; exact1→2/9, no losses. All eight sibling
bodies/canonical relocations, including v8handshakinit, are unchanged. That
caller spells the seven stores directly in current source; do not claim its
body is an inlining collateral gain from this edit.

All symbol types/bindings/visibility/imports/exports and named coefficient
bytes/offsets agree. Raw .rodata changes in41 relocated jump-table words:
the following v8handshak moves by-16 due to alignment, and all41 entries
still name the same function-interior instruction offset. Nonrelocated bytes,
canonical relocation targets and all other allocated nontext sections agree.
This is explicitly not an unchanged raw-nontext claim. Full audit resolves
each relocation against a unique function interval instead of masking away
its addend without checking its target.

Both valid compiles use actual saved flags with DSPLIB_REPRODUCE_BUGS,
Gentoo GCC3.4.2-r2 and executed assembler2.15.92.0.2. Raw production baseline
reproduces; two distinct raw emissions. No per-file flag/register/store-order
variation is used.

    python3 tools/playbook_v8_ansam_owner.py --domain DOMAIN_URL

Existing fixed t_v8sig initializer fixture makes eight paired calls on live
component objects across gains -16000..12007, comparing complete owners and
reversal enable:30241 checks. Additional fixed ANSam component lifecycle
fixtures start from zeroed owners, call each side's txinit, V21_Init and
ansaminit, then generate/assess the waveform (17 checks per side). These
establish component setup, not a complete public modem history. Synthetic
near-reversal diagnostic probes elsewhere remain labelled synthetic.
No new fixture, fuzzing or mutation execution.

Artifacts: `build/playbook-v8-ansam-owner` (commands, RTL, complete TU and
canonical jump-table audit) and `build/playbook-v8-ansam-owner-adoption`
(all-object promotion, census, full gate and complete partial links).
Production reviews all300 objects; only V8.c changes and raw-matches the
measured candidate. F11653 records the source recovery.

Whole-tree923→924/1852 exact,95083→95168 exact bytes, sole gain/no losses.
Fixed Gentoo make phase386/0 and all structural checks pass; static285 suites/
10038 anchors, none detached/nonunique. Full same-order300-object partial
links remain DIFFERENT (strict exit1 before/after): positioned matches
68594→68448/943398 (-146), allocated914782→914766 (-16). Exact section70/92,
symbol394/2907 and relocation1023/18317 records unchanged. Report the
positioned-layout loss alongside the complete-function gain; this does not
establish whole-object or original-profile identity.
