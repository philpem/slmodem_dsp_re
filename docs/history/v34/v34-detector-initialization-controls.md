# V34 detector word counters and post-loop stores

F11607/F11608. At97c06e4e, complete src/pump/v34/detector.c.
[Four-cell word counter cross](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950440495); [five-cell store stage](https://github.com/philpem/slmodem_dsp_re/issues/22#issuecomment-5950470133).
Blob186B narrows inner/outer increments and compares signed words at
0x73681–0x73697; retained int counters emit INC32/CMP32 instead.
Both counters are bounded0..2, so word types preserve outcome.

| Section / tap | Bytes | Verdict |
| --- | --- | --- |
| int / int |172|SIZE14|
| int / short |164|SIZE22|
| short / int |169|SIZE17|
| short / short |186|BYTES18|

Four raw emissions,0/2 exact/no gains/losses. Combined short counters reproduce
prologue and both history-clear loops exactly. Independent post-loop stage
moves armed clear after state assignment, crossed with reversed threshold
assignments; production included as unchanged control. Short-counter cells
all186B, BYTES18/11/26/19 respectively (original/thresholdreverse/statefirst/
both). Five raw emissions; no full hit. This bounded test does not claim
these statement orders are uniquely recoverable from scheduled instructions.

All2 functions/types/binding/visibility/allocated nontext agree; no named data
objects. Untouched tone_detect512B versus blob420B canonical body unchanged.
Full audits retained in both complete-object-audit.json files. Existing fixed
t_v34det immediately compares entire initialized36-byte objects (same static
coefficient pointer), then fixed detector lifecycles; no additional fixture
required for bounded counters. No candidate runtime, source adoption, tree
census or partial-link gates performed. Stop these local domains rather than
permute unrelated fields/declarations/registers; partial instruction recovery
is useful evidence but not a byte-exact gain.

Replay tools/playbook_v34det_counters.py or tools/playbook_v34det_stores.py
--domain <respective linked URL>. Artifacts preserve actual saved full flags,
mandatory DSPLIB_REPRODUCE_BUGS, Gentoo GCC3.4.2-r2 and executed assembler
2.15.92.0.2, commands/source/header hashes/full objects/initial RTL.
No fuzzing or mutation execution.
