# FPM_MTD_create aggregate/count/allocation recovery

At4188d010 blob184B/current235B. Blob copies three DWORD config words,
uses inc/cwtl and cmpw for the clearing counter, narrows tones*2 before
multiplying by sizeof(short), and lacks a null-return branch after the first
state allocation. These are four source-observable boundaries.

[Predeclared16-cell cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5948058572)
independently crosses aggregate copy, short counter, stored short element count,
and unchecked state allocation. All16 sources produce distinct complete objects.
Only the combined cell is184B/EXACT. Partial aggregate variants range182–191B
without exactness; field-copy variants228–237B. Full-TU unchanged-source
control raw-reproduces production. All3 functions/5 global records,
nontext data, symbol type/binding/visibility preserved; only create changes,
1/3 ->2/3 exact, no losses. Retain the complete observed source family.
No register annotations or changed compiler profile. F11587.

New fixed constructor probes use seven counts (-32768,-16385,0,1,2,7,16383),
compare allocation request sizes and copied/cleared state, and cover default
configuration/self-alias copy. Negative counts exercise component conversion
boundaries, not modem reachability or detector operation; create never reads
the coefficient bank. Forked allocation-failure probes test explicit/default
configurations with successful controls: reference faults with SIGSEGV after
failed first allocation, baseline returns NULL. Core files disabled and alarm
bounds the child; no fuzzing or mutation execution. Against unchanged source
these tests fire:3/32916 allocation-boundary checks and2/8 failure checks fail;
existing13+17+272 setup/detector checks pass. This proves detectors discriminate
before adopting the source. First apparatus attempt stopped on a whole-TU
counter-rewrite assertion before any cell compiled; corrected to function scope.

Replay tools/playbook_fpm_mtd_create.py, artifacts build/playbook-fpm-mtd-create,
full saved Gentoo configuration plus mandatory DSPLIB_REPRODUCE_BUGS,
executed selected assembler2.15.92.0.2. Adoption full-tree/gate records follow.

Retained comparison build300/300, zero failures; only src_dsp_fpm_mtd.c.o
changes, raw-identical to the winning complete experimental object. Census
879/1852 ->880/1852 exact,86,960 ->87,144 exact bytes, only create gain/no losses.
Same-order complete300-object partial links remain DIFFERENT (strict exit1):
positioned68,283 ->68,342 /943,398, allocated914,158 ->914,110;
exact section70/92, symbol394/2907, relocation1020/18317 unchanged. This
recovery does not prove complete object/compiler-profile identity.

Fixed Gentoo make phase385 passed/0 failed. MTD fixture passes13 setup,
17 supplied-state,272 detector,32916 allocation-boundary and8 failure/control
checks. Structural14239 references/2716 finding headings and285 suites/
10038 static anchors clean. No fuzzing, mutation execution or modern
portability claim. Header now describes unchecked allocation failure accurately.
