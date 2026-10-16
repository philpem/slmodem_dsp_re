# GetFP_Value bounded divider controls

At4188d010 blob82B/current156B. Reference implements repeated subtraction
with a short quotient counter; source currently uses a ceil-division shortcut.
The reference has no zero-divisor guard: positive remaining value with b=0
never progresses, whereas the source returns zero. This is a static algorithm
fidelity observation, not a hanging runtime test or a claim of modem reachability.

Four predeclared families:
[divider/abs](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947756822),
[lifetime/returns](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947812564),
[negative step](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947871246),
[guard/scoped loop](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5947903291).
18 valid compiles/12 distinct sources/9 complete emissions. Divider cells:
156 baseline,156 unused abs scaffold,136 abs division,76 subtraction,81 abs
subtraction. Lifetime cells:156 baseline,81 staged,83 loop abs,80 ordinary
returns,82 both (BYTES19). Negative-step cells:156 baseline,82 staged,
82 inline negative (raw-identical staged),74 outside negative. Guard cells:
156 baseline,74 staged,82 guarded do,82 guarded while (same object,
BYTES28). No full preimage. Reference negative-absolute/add lowering is
still distinct even when size/loop/return boundaries align.

Every original production control raw-reproduces production; all staged
controls raw-replay their earlier explicitly labelled candidate. All6 defined
functions (4 blob-shared)/6 global records and nontext/type/binding/visibility
preserved; only GetFP_Value changes; exact set2/4 unchanged. Exported
coefficient helpers absent from blob unchanged. No source adoption or
candidate runtime/whole-tree/partial gates. Existing fixed t_fp_math excludes
zero divisors and bounds iteration costs; no claim of total-domain coverage.

Initial negative-step invocation used an incorrect issue-comment URL.
Four compiled cells and original metadata/log are preserved explicitly INVALID
in build/playbook-getfp-negative-step/invalid-domain-metadata and excluded.
All four rerun with the actual predeclared domain URL before interpreting
results. Tools playbook_getfp_{divider,lifetime,negative_step,guarded_loop}.py,
full saved Gentoo flags plus mandatory bug define/selected executed assembler;
artifacts build/playbook-getfp-*. F11586. Close these source families; do not
expand arbitrary declarations or register-specific spellings.
