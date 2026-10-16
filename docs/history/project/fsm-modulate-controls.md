# FPM_FSM_modulate lifetime/countdown/total controls

[Predeclared16-cell cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5948610574)
at70bf8bc8 tests entry-cached short scale/sample count, unsigned-short outer
pointer/countdown traversal, unsigned-short inner countdown and unsigned-short
total against retained member reads/ascending loops/int total. Calls still
reload state->tone each time; public signed short return unchanged.

Sixteen valid compiles/sources/complete emissions. Production complete
baseline raw-reproduced; all3 functions/4 globals/nontext/type/binding/visibility
preserved; only modulate changes, exact2/3 unchanged. Baseline169B vsblob229B;
closest combined230B, remaining return movswl versus blob DWORD load plus
earlier instruction differences. No source adoption for a size-only result.
Caller audit: B103prc and V21t_int consume unsigned low-word sample counts;
t_fpm_fsm and one t_v21fax site hold short results. Upper EAX bits alone do
not uniquely recover the original public return type. No API change.

No candidate runtime/whole-tree/partial gates RUN. Existing t_fpm_fsm uses
initialized independent oscillators, two continuous/fragmented positive-count
passes and allocation/destruction checks; it is not negative-count/alias
validation of these unadopted candidates. Entry-cache and negative sample
count behavior remain source-fidelity leads requiring bounded component
fixtures if revisited on independent evidence; no modem reachability claim.

Initial16-cell run passed an incorrect issue-comment URL. Entire run preserved
INVALID/excluded in build/playbook-fpm-fsm-modulate/invalid-domain-metadata;
all16 rerun using the actual predeclared domain URL before interpreting results.
Valid artifacts contain actual full saved Gentoo flags plus mandatory bug define,
selected executed assembler2.15.92.0.2, hashes/records/all changed disassembly/RTL.
Tool playbook_fpm_fsm_modulate.py. F11592. Close this four-axis source family;
no arbitrary declaration/register/return spelling expansion based on size.
