# Reversal energy accumulation boundaries

Declared before compilation, e0052eec complete fpm_tone.c, retained Gentoo
profile and unchanged headers. Original FPM_TONE_find_rev547B uses ADD EAX,ESI
then SUB EDI,ESI at0xab302/304: energy receives the sample contribution before
losing the historical contribution. Retained same-size body subtracts both
contributions first, then adds their difference to a stack-held energy value
at+0x1a4/1a6. This is a different accumulator-use boundary, not a declaration
or register-choice hypothesis. Existing generator/reversal/count domains do
not cover this function's energy operation.

Two cells only: raw baseline; separate energy += sample-square>>15 and
energy -= history-square>>15. Both contributions are bounded to0..32768;
normal state retains signed16 energy. No out-of-contract overflowing int
state is asserted equivalent; saturation, short publication and every other
arithmetic operation remain unchanged. No source adoption unless complete
function is exact and final period gate passes. No flag/header/type changes,
fuzzing/mutation execution, additional permutations or size-only adoption.
Audit all11emitted bodies, data/symbols/BSS/nontext relocations and raw control.
Stop after two cells if no gain.
