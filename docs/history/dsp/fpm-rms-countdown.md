# FPM_rms: countdown and input cursor together

The four-cell domain starts at 74cda31e, using the complete fpm_rms.c TU,
saved `.build-config`, shared Gentoo helper and mandatory
`DSPLIB_REPRODUCE_BUGS`. GCC 3.4.2-r2 and its selected assembler2.15.92.0.2
are executed and recorded. No flag, parameter, constant or arithmetic change.

At 0xa99e0 the blob initializes a countdown; 0xa99fa zero-extends its old
16-bit value before the increment/test. Sample load0xa99e3 and pointer
advance0xa99e6 evidence an input walk. Source instead used an ascending
unsigned index into samples. These independently observable properties
justify crossing two changes rather than searching nearby loop spellings.

| Source | Bytes | Verdict | TU exact |
| --- | --- | --- | --- |
| Ascending index | 63 | SIZE1 | 0/1 |
| Count countdown, indexed input | 67 | SIZE5 | 0/1 |
| Advancing input, ascending bound | 68 | SIZE6 | 0/1 |
| Count countdown, advancing input | 62 | EXACT | 1/1 |

All four cells preserve the single global function and its strong binding.
The unchanged full object raw-reproduces; only FPM_rms changes. Both properties
together reproduce all62 bytes and the canonical FPM_sqrt_dp relocation.
This is an exact source carrier, not a claim of uniquely recovered spelling.

Retain `while (count-- != 0) { int x = *samples++; ... }`. The count remains
unsigned short; its final local wrap at termination is unobservable. Zero
count reads no input, positive counts read exactly that many samples in
ascending order. Scaling before the second multiply, signed right shift,
accumulation/overflow behavior and sqrt result cast stay unchanged.

[Predeclared domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945026072).
Replay `python3 tools/playbook_fpm_rms.py --domain URL`; source hashes,
commands, initial/preallocation dumps, objects and changed disassemblies live
in `build/playbook-fpm-rms/fpm_rms/`. The existing fixed t_fpm_rms checks four
512-count sweeps including full-scale overflow and a separate zero-count case
(2,049 result checks). No fuzzing or mutation execution.

## Adoption validation

Complete build300/300, zero failures. Only src_dsp_fpm_rms.c.o changes and
the complete retained object raw-reproduces the winner. Whole-tree exact
symbols861/1852 ->862/1852, exact bytes83,722 ->83,784; only FPM_rms gained,
zero losses. Artifacts: `build/playbook-fpm-rms-adoption/`, including all300
baseline objects, before/after census JSON and the explicit exact-name delta.

An initial asynchronous census was rejected by the stale-source guard after
the source edit; its log is preserved as invalid-stale-census.log and excluded.
The original source was restored and rebuilt, its raw object checked against
the saved baseline, then the baseline census completed before restoring the
candidate and rebuilding/running the retained census. Reported results come
only from those matching source/build states.

Complete same-order300-object partial links: positioned equality68,473
->68,477 /943,398 reference bytes; candidate allocated bytes914,190 unchanged.
Exact section records70/92, relocation records1,019/18,317 and symbol
records394/2,907 unchanged. Both strict comparisons remain DIFFERENT(exit1).
This is a function-byte recovery, not complete-object identity.

Fixed Gentoo `make phase` passes385/0, structural checks clean. Reference
check14,223 refs/2,683 finding headings; static anchors285 suites/10,038,
none detached/non-unique. Replay-tool syntax and whitespace checks pass.
No fixture change, static-anchor retargeting or modern-portability claim.
