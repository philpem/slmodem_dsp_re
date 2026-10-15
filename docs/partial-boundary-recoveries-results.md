# Three partial caller and owner recoveries

The three reviewed source boundaries are adopted from original operand and
value-age evidence. They improve fidelity without making a complete function
byte-exact. Baseline is e8393850 with the fixed Gentoo profile and mandatory
DSPLIB_REPRODUCE_BUGS. [The domain](partial-boundary-recoveries-domain.md)
contains exactly six complete-TU cells: unchanged and one existing boundary
control for each of three translation units. All three raw baselines reproduce.

## V23 receive progress

Original v23FP_rx_progress zero-extends DI at86ff3 and saves it at stack+18;
at870e1/870e9 it reloads that value and passes it to FPM_MRF_filter's count
slot. Signed uses separately retain the signed low word. Source instead
explicitly passed `(short)count` at MRF. Change only that cast to
`(unsigned short)count`, retaining the int input and every other count use.

Sibling RTL now zero-extends the count's HI subregister where baseline
sign-extended it, before allocation. The original callee still narrows its int
formal at use, so recovering outgoing slot bits does not widen its internal
loop. This is an observed handoff correction, not a new range or caller-input
claim. The caller has changed since the earlier negative MRF study through
original diagnostic and AGC-return recovery.

The whole body grows634→649 bytes versus original664: SIZE30→15. Its two exact
TU bystanders remain unchanged and exact. No claim of unique original cast or
formal spelling follows from the recovered outgoing value.

## V21 receive transitions

Original RxNextStateV21 reloads root->hdx immediately after the three known-state
diagnostic calls at a1e55/a1e69/a1e7d; the default diagnostic does not use that
owner for subsequent writes. Source retained the entry owner over these calls.
Reload only on the three actual diagnostic arms, preserving all stores, tests,
state values and the default arm. Native controls distinguish zero versus three
immediate post-call loads, with the original also reporting three.

The out-of-line body shrinks310→293 bytes versus original290: SIZE20→3.
The source-unchanged RxHdxDataV21, RxHdxStartV21 and RxHdxWaitV21 also change
through inlining; their gaps change12→1,24→3 and4→7 bytes respectively. The wait caller
therefore grows three bytes farther from the original size; this is recorded
rather than hidden by the other improvements. Audit their
complete bodies and unchanged direct-call inventories instead of treating them
as unchanged bystanders. Two exact TU bodies remain raw-identical. The near
sizes do not justify further declaration/register/ordering searches.

## HDLC unframing

The original reloads the current framer from the root across CRC/debug calls,
including960e0/96116/96154 before frame access. Source cached its entry framer
throughout. Preserve the original entry scalar snapshots, then access current
`vmi->framer` members for subsequent work and final publication. Original
D1073/D1074/D1075 behaviour, CRC attempts, diagnostics, frame substitutions,
loop types and all comparisons remain fixed.

The bounded native chain diagnostic finds ten candidate parent+24→frame+40
load chains versus zero in baseline; the original has such chains too. It
records complete windows and is not a general def/use proof. The operand
review supports restoring owner lifetime; it does not establish that ordinary
modem callbacks replace the framer, that arbitrary alias histories are
reachable, or that the remaining CFG is reconstructed exactly.

The body grows1368→1519 bytes versus original1577: SIZE209→58. The other TU
body remains canonically identical. The 197-byte merge-string pool reorders
only two literals, preserving the complete NUL-terminated literal multiset;
canonical relocation identities preserve the actual strings rather than their
old offsets. The audit records both pool payloads. No other allocated data,
BSS, metadata or nontext relocation changes.

## Reproduction and audit

```
python3 tools/gcc3_partial_boundary_recoveries.py --domain docs/partial-boundary-recoveries-domain.md
python3 tools/gcc3_partial_boundary_audit.py
```

After header drift, use `--historical-headers` with an archived raw production
baseline via `--baseline-dir`. The reproducer pins e8393850; actual complete
commands, compiler-selected assembler identity, input hashes, objects and
compiler dumps are under build/gcc3-partial-boundary-recoveries.

The audit covers6 complete-TU cells/24 shared verdicts. All metadata, named
storage, nontext relocations and direct-call inventories pass. The only
nontext payload change is the explicitly reviewed HDLC literal pool order.
There are no exact gains or losses in the controls. The three source-unchanged
V21 inlined callers are the only allowed collateral bodies.

Targeted period differential passes3/0 for t_v23rx, t_v21hdx and t_faxunframe.
Existing test inputs and assertions remain; V21 next-state repeats its same
component trials at debug level2 to exercise the changed diagnostic arms.
State/handler comparisons remain hard. This adds no new lifecycle or callback
replacement claim and does not compare diagnostic transcripts. Aggregate results follow below. No fuzzing or mutation execution is part of this review.

## Aggregate validation

All387 permitted period binaries pass with0 failures, explicitly excluding
t_fuzz. Only t_v21hdx adds a debug-level2 repetition of its existing next-state trials;
no input, assertion, tolerance or mutation snapshot is weakened or replaced. Static
anchor validation passes285 suites/10038 locations without any retargeting;
this does not execute mutations. No full-phase or modern-portability claim.

Census remains1085/1852 EXACT/120105 original exact bytes,668 SIZE/65 BYTES/
29 REGALLOC/5 UNRESOLVED; ratchet passes. Only3/300 canonical objects change;
297 remain raw-identical. All three selected objects raw-match their independent
controls, all300 faithful inputs raw-match canonical, and the build profile
is unchanged.

Faithful partial text grows702364→702524 bytes and allocated PROGBITS grows
918366→918526; positioned equality rises69398→69544/943398. Both strict
comparisons remain DIFFERENT/exit1, so this is not whole-object identity.
An initial comparison invoked a nonexistent tools/partialcmp.py and exited2
before comparing anything; its invalid log is preserved separately, and both
measurements above use tools/toolchain/partialcmp.py.

V21's added debug-level2 repetition passes106029 additional state checks,
matching its unchanged106029 quiet checks. Structural targets pass, including
2265 offset annotations,285 suites/10038 static anchors and14446 references
with zero unresolved or repointed citations. No static locator retarget needed.
