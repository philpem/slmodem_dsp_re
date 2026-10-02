# Block-update promoted position carrier

F11604. [Two-cell predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950171832) at397211df,
complete src/dsp/fpm_adeq.c. Cross unchanged production with an int running
position, narrowing only at each history index and wrapped index; preserve
short counters/accumulator and all arithmetic otherwise. Blob0xabd96/0xabda4
carries the position full-width and0xabdca/0xabdda narrows at use. F8163's
inference of a word-width carrier throughout is not established by those
use-site conversions. This observation does not establish a unique source type.

Baseline217B, promoted207B versus blob244B: two raw emissions, no exact gain
or loss,0/3 exact unchanged. Only FPM_block_update changes; FPM_lmsupd128B
and FPM_lmsupd2 169B canonical bodies/relocations unchanged. All3 functions,
imports/exports/types/binding/visibility and allocated nontext preserved;
no named OBJECT data. Complete-object-audit.json and results.json retain
full controls. Promoted subtraction removes per-iteration narrowing but does
not recover the complete loop or allocation/layout regime.

For typed short arguments the running int does not overflow: at most32767
iterations times a step magnitude32768 plus initial magnitude65535 remains
below2^31. This is an arithmetic bound, not runtime reachability evidence.
Existing fixed t_fpm_lmsupd includes ordinary and padded negative-history
cases; padded cases are exploratory component inputs, not service histories.
No candidate runtime, source adoption, tree census or partial-link gate was
run. Production stays884/1852 exact,88,310 exact bytes, last fixed phase385/0.
Close this two-cell family; do not permute accumulator/declarations or fit size.

Replay tools/playbook_block_position.py --domain <linked URL>.
Artifacts build/playbook-block-position preserve actual saved full flags,
mandatory DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2, executed selected
assembler2.15.92.0.2, source/header hashes, commands, full objects and RTL.
No fuzzing or mutation execution.
