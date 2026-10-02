# FixedRC reset contract and memory-call controls

F11630. Baseline a50cc9cf; production916/1852 exact,94,525 exact bytes.
[Four-cell predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5953384554) crosses removal of the added h/state null
return with restoring four external sysdep_memset calls: three kind1 buffers
and the history clear in shared rc_reset_state. Leave all allocation/free,
memmove and Resample behavior untouched. Actual blob dereferences h0xb0e1f
and kind0 state0xb0e34/0xb0e4c without checks; its four clears retain external
calls0xb0e4c/0xb0ebd/0xb0ed7/0xb0eec. Delete's own null guards are authentic.

| Cell | Reset bytes/verdict | Create bytes/verdict |
| --- | --- | --- |
| Baseline |219/SIZE51|699/SIZE70|
| Nonnull |213/SIZE57|705/SIZE64|
| External clears |269/SIZE1|749/SIZE20|
| Both |270/BYTES41|759/SIZE10|

Four valid compiles/four distinct emissions,1/5 common exact each; no gains/
losses. Complete TU has8 functions/22 named data objects. Only Reset/Create
canonical bodies change; all six bystanders and raw allocated nontext/data
are unchanged. Existing exports/types/binding/visibility agree. Wrapper cells
replace undefined memset with undefined sysdep_memset (same type/binding),
an intentional import change, not an imports-identical claim.

Combined Reset restores the complete entry/kind0 structure through the first
41 instruction rows, then the kind1 call preparation differs. Existing alpha
rejects row41 XOR versus MOV; do not label the whole residual pure register
renaming or adopt merely equal270B size. Whole family closes without register,
condition, scope or declaration permutations. No source/profile change adopted
or new differential/census/partial claim. Apparatus audit first incorrectly
expected libc memset to remain imported; corrected control explicitly reviews
its replacement rather than hiding the change.

Fixed t_rcresample creates actual mode2/3 graphs, dirties via processing and
compares800 post-reset samples to fresh graphs. Mode0/1 real create/reset/delete
coverage is distinct from its documented missing-kind1-resample probes.
t_fixedrc alone is only factor/lookup coverage. No null-crash fixture, fuzzing
or mutation execution. No fixture run because no candidate is retained.

Replay tools/playbook_fixedrc_reset.py with the domain URL. Artifacts
build/playbook-fixedrc-reset preserve raw baseline reproduction, actual full
Gentoo3.4.2-r2 config/bug define/executed assembler2.15.92.0.2, commands/hashes,
RTL/disassembly and complete-object-audit.json. Generator fires4/4 sourcecells.

Scope review: Reset's two boundaries alone cannot recover this TU. Create has
separate observable allocation, clearing and dispatch differences (F8530):
blob allocates the common handle after range validation before mode dispatch,
while retained source splits allocation across arms and rejects some modes
before allocation. A future domain requires full reference mode behavior and
ownership review, not another Reset spelling. Resample's documented omitted
kind1 machine remains separate; removing that kind guard alone is forbidden.
