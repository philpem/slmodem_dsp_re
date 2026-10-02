# FPM tone allocation-guard control

F11631. [Predeclared two-cell domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5953406444), baselinea50cc9cf.
Blob FPM_TONE_create malloc0xaace0 copies EAX intoESI0xaace5, sets ownership
and rejoins cfg dispatch0xaaa25 without checking allocation result. Source
adds if(state==NULL)returnNULL after malloc. Remove only this extra guard;
retain supplied-state/allocate dispatch, cfg default, ownership, sizeof and
all child operations. No register/return-type/loop variants.

Baseline669B/SIZE84 against753B blob; unchecked allocation663B/SIZE90. Two
valid compiles/two distinct emissions; unchanged baseline raw-reproduces.
No gains/losses,7/11 exact unchanged; only FPM_TONE_create changes. Complete
11-function/two-data TU audit confirms both named objects, all ten bystanders,
raw nontext/binding/import/export controls unchanged. A larger gap does not
disprove the observed missing blob guard; deleting it does not close the body.
No source change adopted, differential/census/partial claim or global ceiling.

Existing t_fpm_tone covers real allocation and supplied/default configuration;
some cloned internally planted histories are component exploratory probes,
not public modem histories. No allocator-failure crash or fuzzing/mutation
execution, no fixed fixture executed for this unretained cell.
Replay tools/playbook_tone_alloc_guard.py with the domain URL. Artifacts
build/playbook-tone-alloc-guard preserve actual complete Gentoo3.4.2-r2 flags,
bug define, executed assembler2.15.92.0.2, commands/hashes/RTL/disassembly and
fpm_tone-complete-object-audit.json. Generator fires2/2 distinct sources.
Close this guard-only domain; another experiment needs a separate boundary.
