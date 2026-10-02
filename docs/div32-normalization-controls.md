# Reciprocal normalization helper and word-index controls

F11605/F11606. At397211df, complete src/dsp/fpm_div32.c.
[Four-cell normalization-helper × word-index domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950298473);
[three-cell guarded-loop stage](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950344801). F1505 observes local output words;
F11392 resolves TU ownership; neither tested this source family.

Blob147B takes local mantissa/count addresses at0xa6cba/0xa6cbe, initializes
count word at0xa6cb1, runs full-width count arithmetic then a word store at
0xa6cd9, stores/reloads mantissa at0xa6ce3/e6, zero-extends word index at0xa6cf4.
Ordinary static-inline normalization helper with local word pointers and int
counter accounts for addresses without volatile/assembly/forced inline.
Word index keeps normalized0..128 including D4 overrun. Final public output
store order and zero/debug path remain unchanged.

| Cell | FPM_div_32 bytes (blob147) |
| --- | --- |
| production |117|
| word index |123|
| normalization helper |131|
| helper + word index |137|
| guarded do/while helper + word index |148|

Four first-stage sources/four raw emissions; three follow-up sources/three
emissions, with production and helper+word repeated as controls. Five distinct
sources overall; no complete exact hit/gain/loss,0/2 unchanged. All symbols,
imports/exports/types/binding/visibility and allocated nontext bytes agree;
no named OBJECT data. Untouched FPM_circ_dotp2 canonical body/relocations remain
179B versus blob195B. Both complete-object-audit.json files record controls.

Guarded stage reflects the blob's skipped final count store when the top bit
is already set. It restores branch/store placement but has42vs45 instructions
after padding removal: blob LEA+MOV loop-counter sequence versus INC, fewer
saved registers/different frame, final count MOVZWL versus blob DWORD load,
and different error epilogue. Its one-byte size difference is not a preimage.
Close these stages without local register/frame/type/declaration permutations.
They support a factoring explanation, not a uniquely recovered original helper
or a proof that source recovery is globally exhausted.

Initial generator inserted helper between int return type and function name;
Gentoo rejected malformed text. Preserve invalid-generator-helper,
invalid-generator-results.json and invalid-generator.log; exclude that run.
Corrected four-cell rerun is valid and raw-reproduces retained baseline.
This was an apparatus defect, not evidence against a source family.

Fixture qualification: t_fpm_div's low16 and high16-only sweeps do NOT reach
every mantissa/shift pair. Valid denom0x40008000 normalizes to0x80010000,
mantissa8001/shift1, absent from both sweeps; denom0x7fffc000 givesFFFF/shift1
and D4. Existing deterministic sweeps/zero/debug/sentinel checks remain useful,
not exhaustive32-bit coverage. Record these as fixed-vector follow-ups before
any future source adoption; no candidate runtime or fixture change here.

No src/include/test/flag changes or candidate differential/census/partial gates.
Production remains884/1852 exact,88,310 exact bytes; last fixed phase385/0.
Replay tools/playbook_div32_normalize.py or tools/playbook_div32_guard.py
--domain <respective linked URL>. Artifacts build/playbook-div32-normalize and
build/playbook-div32-guard save actual full .build-config flags, mandatory
DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and executed selected assembler
2.15.92.0.2, commands/hashes/full objects/initial RTL. No fuzzing/mutations.
