# FPM_FSD_init clearing counter and trace allocation recovery

At2f58aed6 blob268B/current253B. All three reference clearing loops narrow
increments at0xa7947/0xa7969/0xa79a7, while source shared counter is int.
Trace allocation zero-extends trace_len at0xa79d3; current(unsigned)short
conversion instead sign-extends before unsigned conversion. Clearing trace
length comparisons remain signed. These are independent use-site boundaries.

[Four-cell predeclared cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5948429672):
baseline253B; unsigned trace253B (different object); short counter268B/BYTES1;
combined268B/EXACT. Four distinct complete emissions. Baseline full object
raw-reproduces production. All3 functions/4 globals/nontext/type/binding/
visibility preserved, only init changes,1/3 ->2/3 exact/no losses. Retain
short counter and(unsigned short)trace allocation count, not a field retype.
Config aggregate copy, allocation ordering and promoted2*iir_len loop bound
remain as observed. F11591.

New fixed constructor probes cover trace lengths-32768,-1,0,1,12,160,32767,
compare allocation request sizes and state/cleared buffers. Negative trace
lengths allocate unsigned-word sizes while signed clear loop does not run;
the entire allocated reference/source traces retain allocator fill. These are
constructor component conversion boundaries, not demodulation or modem
reachability claims. Baseline detector demonstrably fails4/33192 checks,
existing init/in-place/Bell103 signal/coverage groups pass. Successful recovery
must also run the previously skipped negative allocated-content checks.
No fuzzing/mutation execution.

Short counter with promoted2*iir_len can wrap for iir_len>=16384, as the blob
explicitly does. Do not silently narrow the bound or widen the counter to
avoid original behavior. This is static instruction/source evidence, not an
executed large-count overflow or lifecycle test. Existing valid fixtures use
small real filter configurations. Preserve the distinction in coverage claims.

Replay tools/playbook_fpm_fsd_init.py, artifacts build/playbook-fpm-fsd-init,
full saved Gentoo configuration plus mandatory bug define and executed selected
assembler2.15.92.0.2. Adoption artifacts build/playbook-fpm-fsd-adoption.

Retained comparison build300/300, zero failures; only src_dsp_fpm_fsd.c.o
changes and raw-reproduces the winning complete object. Whole tree881/1852
->882/1852 exact,87,337 ->87,605 exact bytes, only init gain/no losses.
Same-order complete300-object partial links remain DIFFERENT (strict exit1):
positioned68,275 ->68,258 /943,398, allocated914,126 ->914,142;
exact section70/92, symbol394/2907 unchanged, relocation1021 ->1020 /18317.
Positional reduction retained honestly; no layout/padding score fitting.

Fixed Gentoo make phase385 passed/0 failed, including229798 trace allocation/
clearing checks and existing Bell103 signal/reinit/coverage fixtures. Structural
14240 references/2720 finding headings and285 suites/10038 static anchors
clean. No fuzzing, mutation execution or modern portability claim.
