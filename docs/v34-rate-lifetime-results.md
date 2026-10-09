# V34 control-base evidence and receive-rate recovery

Baseline b3665999, Gentoo3.4.2-r2 configured flags and bug reproduction.
The obj+4 investigation did not establish a new control type or extent, but
exposed an independently recoverable value-lifetime difference in a small
accessor. No new struct, overlay, padding, pointer offset or register constraint
is introduced. The existing maps and the two genuine shell decompositions stay.

## What the interior base establishes

Original SetMaxBlockLength0x6500, SetTimeOut0x6e90, SetMinimumSigLevel0x6370,
IndicateK56FlexRateDetermined0xa6e0, SetINFO0dBits0x8020, indicateJaTransmission
0xa410 and Create0xaa70 construct an explicit object+4 base. The six small
accessors plus constructor are the reviewed denominator, not a whole-blob
no-escape proof. Named fields reached through it include ptc, sample_count,
timeout_deadline, rx_energy_floor and the two receiver counters. Original
SetTimeOut uses displacement0x234/0x238 on that base, hence absolute0x238/0x23c.

Create's first memset clears object0 for0xac4c bytes; its second clears the
known receiver at0x264 for0x79c. Neither bounds a separate +4 control type.
Six mangled functions mention tagV34Object*, not an independently named control
parameter. This does NOT prove no subobject existed. It proves the proposed
new map lacks an independent typed-callee/extent witness in the reviewed data.
The earlier indicateJa recovery already established that source anchor lifetime
can matter without determining a struct. Old statements calling the add free
or declaring no second object were too strong; the source comment now records
only the measured base and this limit. Do not infer a new layout from its
register color or create another partial map to obtain an ADD4.

Reproduce the original observations with tools/dis.py BLOB SYMBOL; constructor
snapshot is build/vpcm-create-object.dis. No new speculative map was compiled.

## RX pointer capture crossed with result ownership

The blob loads p3548 and pac18 before role/status guards; source originally
loaded each in its selected arm. The call to const V90Demodulator::getBitRate
is ordinary with returns in the blob and a sibling jump in the baseline.
Preserve its current unsigned prototype/int cast: mangling does not encode a
return type and cannot justify changing it.

| Eager pointers / shared int result | Bytes | Verdict |
| --- | ---: | --- |
| no / no | 74 | SIZE16 |
| yes / no | 84 | SIZE6 |
| no / yes | 92 | SIZE2 |
| yes / yes | 90 | EXACT |

The combined control matches all90 original bytes and the named relocation
against _ZNK14V90Demodulator10getBitRateEv. All56 other full-TU bodies remain
raw-identical. Binding/visibility, allocated data/BSS and canonical nontext
relocations agree; later text positions shift with the target's changed length.
Both pointers are captured, not dereferenced, before the guards. Conditional
callee/object reads remain conditional. Every original guard, fallback,
zero case, type and named field is retained. No syntax-uniqueness claim.

Saved01.rtl confirms both loads precede the first guard only in eager cells.
02.sibling retains one sibling-call marker for both early-return cells and
zero for both shared-result cells. The initial call_placeholder alone does
not identify the selected call form; inspect the deciding sibling pass.
Stage comparator:29 stage pairs/58 streams; initial source divergence01.rtl.
No original optimization profile or dynamic scratch cursor is inferred.

## TX control closes without adoption

TX original fallback loads txbits directly from the object, while source
captures a ratecfg base early. Cross direct fallback and a shared result on
RX's exact seed; repeat its baseline and direct-only objects independently.

| Direct fallback / shared result | TX bytes | Verdict |
| --- | ---: | --- |
| no / no | 217 | SIZE4 |
| yes / no | 202 | SIZE11 |
| no / yes | 236 | SIZE23 |
| yes / yes | 218 | SIZE5 |

Reference213B. These keep RX exact but do not close TX or its frame/owner
residual. Only RX is adopted. No adjacent owner/result/constant permutations
follow from near size. Its earlier half-offset reserve remains unadopted.

## Controls and replay

13 full-TU compiles/741 body verdicts across the three declared domains.
Every raw baseline reproduces production; all full object hashes, commands,
bug defines, functions/bindings, nontext metadata and gain/loss sets are audited.
Three repeated full objects agree independently. Only the named RX/TX bodies
change in controls; winner changes only RX. TX losing/negative cells remain
outside source. The crossed positive and three negative RX controls are the
diagnostic proof, not an empty screen. No mutation/fuzzing execution.

    python3 tools/v34_rxrate_lifetime_reproduce.py --domain docs/v34-rxrate-lifetime-domain.md
    python3 tools/v34_txrate_owner_reproduce.py --domain docs/v34-txrate-owner-domain.md
    python3 tools/v34_txrate_result_reproduce.py --domain docs/v34-txrate-result-domain.md
    python3 tools/v34_rate_lifetime_audit.py

After production adoption, replay from an archived b3665999 baseline/config
with --baseline-dir. Artifacts: build/v34-*-*/results.json,
build/v34-rate-lifetime-audit.json and RX stage-diff.json. FindingsF11882-F11884.

Static apparatus repair: the initial structural run detects nine detached RX
mutation locators after the source rewrite. Retarget the same nine operations
and explicitly scope all ten RX records to this function. The three fallback
mutants now change BOTH mutually exclusive default assignments in one unique
block, retaining their original path coverage rather than mutating only one
branch. Labels and record count stay fixed. Mutation execution/snapshot
re-recording is not performed or credited. Preserve the initial red structural
log as a negative control and run the final repaired gate separately.

## Final validation

Final repaired `make phase J=8`:388 period tests passed/0 failed; structural
boundary green. `make tc J=8`:300 sources/300 objects/0 failed. Complete
production VPcmV34Main object is raw-identical to the adopted RX control.
Static285 suites/10,038 anchors are attached/unique; all10 scoped RX records
and both default assignments in all3 fallback mutants are checked. No mutation
execution or snapshot update was performed. Modern portability was not run.

Final census1,076/1,852 exact,118,266 exact original bytes: ONLY new exact
name VPcmV34GetCurrentRxBitRate, +90 bytes, zero losses against b3665999.
Remainder674 SIZE/66 BYTES/31 REGALLOC/5 UNRESOLVED. Master's1074-name ratchet
passes and remains unchanged. This PR now adds SDMv27_init89B plus RX90B.

Next discrimination: find another original unconditional capture/call-return
boundary and cross the two independently witnessed axes. Do not repeat the
closed TX family or invent an obj+4 control map from the current base evidence.
