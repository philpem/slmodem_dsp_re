# FPM_TONE_filter: signed-short carriers are insufficient

Baseline9a16b620; no production change. A delegated read-only audit identifies
word-sized instructions absent from retained source; parent inspection confirms
reference0xab3f8 cmpw and0xab3c6/0xab473 incw. Retained locals taps,idx,i are int
although their inputs are signed-short fields/count. Inner counter k is int in
both sides and remains unchanged.

A predeclared four-cell complete-TU cross narrows ring locals taps+idx and outer
counter i separately/together, changing declarations only. Baseline238B/SIZE3;
ring239B/SIZE4; outer239B/SIZE4; both240B/SIZE5 against reference235B.
All4 cells compile, every emission distinct, raw full baseline reproduces.
All11 functions/12 globals survive;4/11 exact unchanged, no gains or losses,
only FPM_TONE_filter changes its canonical body. No source adoption.

Short ring locals restore cmpw, and short outer restores incw. The ring wrap
still branches rather than the reference setl/neg/and sequence. Restoring a
measured operand width does not prove the original local type or recover the
full condition. A source-width explanation and if-conversion mechanism remain
separate questions. This declaration-width family is closed. Reopening requires
new source/pass evidence for the ring conditional; do not vary declaration
order or nearby local aliases to search register allocation.

[Domain](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5945704153).
Replay tools/playbook_fpm_tone_width.py --domain URL. Artifacts under
build/playbook-fpm-tone-width retain actual saved compiler flags, mandatory
DSPLIB_REPRODUCE_BUGS, executed Gentoo GCC3.4.2-r2/assembler2.15.92.0.2 identity,
source/header/object hashes, inventories/bindings and all changed disassembly.
F8169/F8170 describe owner reconstruction/no internal caller; this new domain
does not claim modem-lifecycle reachability. Existing fixed t_fpm_tone includes
filter zero/negative counts but candidate runtime/partial gates NOT RUN because
no candidate is adopted. No fuzzing or mutation execution.
