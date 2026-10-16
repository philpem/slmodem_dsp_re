# RxClampV32 count/conversion controls

At4188d010 the blob is49B, current60B. A predecrement/countdown control
becomes63B; conversion-before-subtraction is raw-identical60B.
[Count domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947735448)
and [conversion domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947784791)
contain4 compiles/3 distinct sources/2 emissions. Both original full-TU
controls raw-reproduce production. All25 functions/31 global records,
nontext data, binding/type/visibility retained; exact set14/25 unchanged,
only RxClampV32 changes in the countdown cell. No adoption.

The actual symbol_len member is already signed short (v32struct.h).
F8572 separately establishes its signed timer addend. Do not infer unsigned
member type from a movzwl fetch alone. Existing fixed t_v32fpctl tests seven
counts including-1 and zero; proposed exhaustive65536-count runtime probe
was NOT RUN. Candidate runtime, whole-tree census and partial gates NOT RUN.
Tools playbook_rxclamp_count.py/playbook_rxclamp_conversion.py preserve
full Gentoo configuration, mandatory bug define, actual commands and
selected executed assembler identity. Artifacts build/playbook-rxclamp-*.
F11585. Close the tested countdown/conversion family.
