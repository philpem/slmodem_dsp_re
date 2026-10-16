# ADID scheduling packets and reference-code capture

Unchanged90b7f0d1 ADID TU with-fsched-verbose=5 reproduces the retained complete
object raw. It explains why reading priorities alone gives the wrong order.

## Ready ranking is overridden by the target decoder

Middle-entry region26 contains these measured dependency rows:

| UID | Operation | Priority | Outgoing dependencies |
|---|---|---:|---|
|977|zero dx|6|1002,912,978|
|978|count store from dx|5|1002,912|
|497|sum zero store|1|1002|
|508|square-sum zero store|1|1002|
|912|reload high-loop code into dl|5|1002|
|1002|jump to high-loop test|1|none|

The sorted ready list is508,497,977, with977 at the issuing end. Before issue
it changes to508,977,497. The resulting PPRO packet is497,977: float sum store
issues before the higher-priority integer xor. Count follows next cycle; code
reload cannot precede either xor or count because it reuses dx. Issued order
is497,977,978,912,508,1002. None of the three stores depends on another store.

[Official haifa-sched.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/haifa-sched.c)
2380–2386 invokes targetm.sched.reorder after ready_sort.
[Official i386.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.c)
12388–12500 explains PentiumPro decoder reorder: when the decoder is empty and
the highest-priority operation is simple, it searches for a FEW-uop operation
to occupy the first decoder, then packs simple operations into the other slots.
The actual Gentoo packet and reordered ready list corroborate that sequence.
We do not infer the original compiler's ready list from its final binary.

## Four source cells close the capture/ownership domain

Original at0x4147a reloads reference code into edx before count clearing; ours
reloads after clearing and shares dx with the zero scratch. Controls:

| Cell | First-study verdict | Full-object result |
|---|---|---|
|Retained|795B/SIZE16|raw baseline reproduced|
|Capture k=ucode before middle clears; increment capture for high loop|803B/SIZE24|different target only|
|Also use captured k for all three middle addresses|803B/SIZE24|same complete object as prior cell|
|Separate high counter from low counter at capture boundary|803B/SIZE24|same complete object as prior cell|

All three source controls emit an additional reference/index load into ebp,
compute middle index with lea instead of retaining original ecx, then reload
the high counter into edx. Their count-first stores alone do not recover the
original contiguous block. Reject all three; production ADID remains raw equal
to the baseline. Capturing earlier restores a broad load age while introducing
unsupported operations. This is not sufficient partial recovery to adopt.

Four cells/136 common-body verdicts;17/34 exact unchanged;33 canonical bystanders,
symbol metadata, allocated/named data and relocations unchanged. No exact gains
or losses, no runtime/fuzz/mutation execution for declined candidates. No type
casts, source-order permutations, register forcing or option experiments.

Next evidence must identify the original maintained-index lifetime and packet
inputs together, or move to an independently supported function family. The
finite capture domain is closed; don't expand it with more counter names.

Replay:

```
python3 tools/gcc3_adid_first_study_schedule.py --domain docs/history/pcm/adid-first-study-scheduling-domain.md
python3 tools/gcc3_adid_first_study_clear_capture.py --domain docs/history/pcm/adid-first-study-scheduling-domain.md
python3 tools/gcc3_adid_first_study_schedule_audit.py
```

Later replay needs90b7f0d1 production baseline and compatible/historical headers.
Complete command/source/header/object hashes and all verbose dumps retained.
Scheduler sources fetched by tools/batch_scheduler_source_fetch.py; official
release hashes verified3/3, not Gentoo patch-stack verification. i386.c SHA256
bb46f666e8686bf902ae1f9f32fcbfb298259915c1368e669326c89adce34cdc.
