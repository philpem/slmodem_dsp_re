# ADID first-study publication and clearing-stage proof

Baseline90b7f0d1. One unchanged complete TU, 34 common bodies, 17 exact. Selected
diagnostic switches compile successfully and reproduce the retained object raw.
Read-only installed cc1plus observation reproduces container assembly and object
raw, with137 complete scratch searches, six in first-study. No source change or
exact gain/loss; production remains1080/1852 exact. The existing388/0 deciding
gate remains the source validation; no harness, mutation or fuzzing rerun.

## Publication merges after scratch selection

Eleven inspected stages show two byte_280c stores through flow2 and peephole2,
then one from ce3 onward. Immediate-zero UID326 and immediate-one UID345 survive
reload. Peephole2 splits both into a scratch definition and memory store, choosing
reg0/ax for both. Their live set is4/si,6/bp,7/sp,11/st(3). ce3 explicitly reports
cross jumping from block18 to19 with one common instruction. This merges their
now-identical register-to-memory tails. The blob's bl/cl stores are different
tails; its separation does not uniquely imply different source arms.

The [official i386.md](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/config/i386/i386.md)
QI zero/immediate peepholes at17498–17537 explain these splits: sufficiently
long immediate-memory moves use a free byte register under the tuning policy.
[Official toplev.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/toplev.c)
3516–3531 runs crossjump-enabled CFG cleanup before if_convert in the ce3 dump
interval. Calling the merge merely "if-conversion" would misidentify it.
These release sources explain observed Gentoo execution; the Gentoo patch stack
is not claimed inspected.

## Incoming TU scratch state alone is insufficient here

Installed tracing sees updateUrefAlt leave cursor50 and first-study enter at50.
Its six scratch events, in pass traversal order:

| Original UID | Incoming cursor | Selected hard register | Outgoing cursor |
|---|---:|---:|---:|
|533|50|6/bp|16|
|486|16|0/ax|49|
|444|49|6/bp|16|
|345|16|0/ax|49|
|326|49|0/ax|50|
|123|50|0/ax|51|

[Official recog.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/recog.c)
2932–3024 supplies the scalar search model. Validate it against all six observed
selections and cursor returns before using it. Exhausting all53 legal incoming
cursors with the observed allocation order, eligibility and liveness gives the
same six picks in53/53 cases. The first search has only bp eligible and erases
the incoming cursor distinction. This is a bounded model result, not53 altered
compiler runs. No inferior calls or state writes occur.

Therefore moving an intact preceding helper solely to vary the incoming cursor
cannot fix this target under these fixed facts. Changes to liveness, allocation
order, scratch sites or traversal would be a different domain needing evidence.
TU context still matters generally; this particular context-only shortcut is
closed, without declaring source recovery exhausted.

## Clearing order is a separate scheduling residual

Middle-ucode count,sum,square-sum stores retain that order from initial expansion
through block reorder. Peephole2 splits the integer-zero count store into xor
and a register store; renaming later chooses dx. sched2 first moves the SF sum
store before that pair, producing sum,count,square-sum. The blob retains the
count-first sequence. Another source permutation is not justified by this
trace: source already emits count first. The next discriminating evidence is
the scheduling dependency/priority difference around the loop exit and these
three stores, or an evidenced upstream lifetime change affecting the byte
scratch sites. Do not disable a pass or force colors to retain the tails.

## Replay and invalid artifacts

```
python3 tools/gcc3_adid_first_study_stages.py --domain docs/adid-first-study-stage-domain.md
python3 tools/gcc3_mechanism_scratch_observe.py --reproduction-dir build/gcc3-adid-first-study-stages --target porcessFirstStudy --compiler-kind cc1plus --out build/gcc3-adid-first-study-scratch
python3 tools/gcc3_adid_first_study_stage_audit.py
```

Later replay must supply archived production objects/config for90b7f0d1 and
historical headers if required. Artifacts retain complete commands, source/header
hashes, identity including selected assembler, preprocessed TU, assembly, dumps,
object and observation traces. Audit reports one raw-equal TU,11 stages,137
scratch events/six target events and53 modeled entry states.

Failed full-da attempt is preserved under build/adid-first-study-pass-trace:
sandbox attempt exited-31; escalated execution exited27 with period compiler ICE
at source line3042. Its partial dumps and incomplete assembly are invalid and
excluded from these results. The successful bounded diagnostic replay is the
authority. No claim is made about which individual all-dump switch crashes.

Source SHA256 pins: toplev.c
2c08ac68a9f22925e551c72ac2e93b4cbcea542b1b140607b50f7d8b0a24d8d2;
i386.md 2b62f98bc15ccdc268036b57da4afe23f3f9e2c5fcfdda5175758e3dc62719ab;
recog.c 9e1ec6d01c9210a82600263de301021ce7514b84512ab5b7cf21a58a12de0590.
