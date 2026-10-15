# Supported V32/V8 partial path recovery

Current baseline d4c3ffea; fixed Gentoo GCC3.4.2-r2 profile, selected period
assembler and mandatory bug define. Revalidate two existing measured path
controls under the owner's supported-partial-recovery scope. These source
domains remain closed for whole-function identity; no source-spelling sweep.

## Accepted count falls through in V32

Original V32FP_GetCleanedSamples at0x7f910 compares unsigned short count against
160, branches JA to zero publication, and falls through to sign-extension/count
publication. Retained source spells zero first and emits JBE to the accepted
arm. Accepted-count-first restores JA/fallthrough and the original accepted
operations. Both source cells remain47B/SIZE6 against original53B: the original
reloads the owner after count publication, while ours retains the owner. That
missing read is unresolved; no alias cast or forced reload is inserted.

Unsigned count and equality boundary160, both count stores and buffer-pointer
return remain unchanged. Inputs0..65535 partition identically; accepted counts
are0..160 and their signed-short conversion is exact. One target changes, five
canonical bystanders unchanged,2/6 exact unchanged. No runtime bug is claimed.

## One ring publication join in V8

Original V8Process at0x74560 publishes tx_ring_base once, at0x745e0 after the
output and advanced-pointer bound selection. Retained code emits two static
publication sites, one for advance and another for wrap, producing a different
per-sample loop layout. Local tx cursor acquired after tx_avail decrement,
advanced after output, selected/wrapped and then published once restores that
join. It is never held across v8handshak or loop iterations. Symbol cursor and
filter arithmetic stay untouched.

Current two-cell replay remains731B: BYTES233→73,2/7 exact unchanged. The size/
byte score does not decide adoption. Positive detectors verify two→one ring
stores in initial RTL and all seven inspected streams through machine RTL.
Original has one actual machine store too. The five contiguous ring acquire/
output/advance instructions agree exactly with the original, followed by the
same unsigned bound/wrap/join operations; the bound stack home differs. Six
canonical bystanders, all data/metadata/relocations unchanged. Input/pole loads,
symbol-bound operand orientation and zero/stack register homes remain different.
Single static publication is not a claim of a missing dynamic output or a
measured concurrency/race fix. Period tests decide normal component behavior;
universal malformed-pointer/alias fidelity is not claimed.

## Fresh pole-read cross fails at sched2

Original loads pole0x745d9 before ring publication0x745e0 and input0x745e6.
Four cells cross ring join with a local unsigned-short pole read at that
boundary. Plain pole capture gives BYTES233; ring+pole gives BYTES74, while
ring alone is73. All four complete objects differ: equal233 scores do not mean
erasure. The capture changes initial operation age and register ownership.

Nine streams of the combined capture show pole-before-input through block
reorder, then input-before-pole at sched2 and machine RTL. Thus the ordinary
source read boundary is insufficient to restore the emitted order. Decline
pole capture, close its finite cross, preserve the original read-order residual.
No expression/alias/type/profile permutation is justified by this miss alone.

## Complete-TU audit and replay

Eight source cells/54 live common-body verdicts, including repeated baselines
and ring controls across the two reproducers; no exact gains/losses. Every
complete unchanged TU reproduces retained raw output. Only nominated targets
change canonically; all emitted bystanders, metadata, allocated/named data,
BSS and nontext relocation content remain unchanged. Two retained source
functions are supported partial recoveries, not two new exact functions.

```
python3 tools/partial_path_retention.py --domain docs/partial-path-retention-domain.md
python3 tools/v8_ring_pole_capture.py --domain docs/partial-path-retention-domain.md
python3 tools/partial_path_retention_audit.py
make tc J=8
python3 tools/partial_path_retention_audit.py --integrated
make phase J=8
```

Later replay needs archived d4c3ffea objects/config via--baseline-dir and
compatible/historical headers. Saved inputs under build/partial-path-integration
retain the two original objects and configuration. Reproduction artifacts store
complete commands, source/header/object hashes, compiler/assembler identities,
preprocessed source, assembly and dumps; audit rejects missing or inert controls.
Integration additionally requires source/whole-object raw equality to each
independent retained cell. Final gate/census results recorded in the finding.
No fuzz/mutation execution, flag/register changes or modern portability claim.

Integration: both production sources and complete objects equal independent
chosen cells raw. Exactly2/300 objects change,298 unchanged; complete compiler
configuration unchanged. Exact-set ratchet passes1080/1852/119108 bytes with
zero gains/losses;673 SIZE/64 BYTES/30 REGALLOC/5 UNRESOLVED unchanged. Faithful
300-input partial link retains701452 text bytes; strict comparison stays
DIFFERENT/exit1 before and after, no whole-object completion claim.

Initial make phase passed388 period tests/0 failures but structural refs failed
on two detached V32 anchors. Retargeted reject-boundary mutation now uses<160
instead of<=160; the overlong-clamp mutation still substitutes160 for zero.
Both locators scoped and verified unique in the getter, old misleading boundary
label corrected. No mutation execution or snapshot rerecording. Final make phase
J=8 exits0:388/0, all structural checks green;285 suites/10038 static anchors,
zero detached/nonunique/wrong-arm anchors;14426 references/3030 headings clean.
Logs: build/partial-path-phase.log (initial red), partial-path-phase-final.log,
partial-path-tc.log; audits/census beneath build/partial-path-integration and
byteident-partial-path-final.json.
