# V22 originate: predicate, frame and effect timing (2026-11-05)

The retained partial recovery restores native gap-reset timing, detector branch
orientation and NODE3 output-word consumption. `v22_originate` remains SIZE:
2,505 bytes against native 2,655 (previously 2,495). No EXACT or REGALLOC gain
is claimed. The reconstruction census remains 1,136 EXACT / 28 REGALLOC /
71 BYTES / 617 SIZE over 1,852 symbols.

## Native evidence and retained changes

Native NODE6 calls SignalDetect, checks its result, clears the gap temporary,
then branches on the retained detector result: XOR at `0x8bad0`, TEST at
`0x8bad2`, gap store at `0x8bad4`, JNE at `0x8bad8`. The reconstruction cleared
gap only inside the detected-present arm. Move that clear to the successful
receive/signal block before the detector branch. A local store can be speculated
by a compiler; this is an independently witnessed operation-timing recovery,
not proof of the author's unique lexical spelling or a demonstrated behavioural
failure. Both detector-result arms assign gap before its later consumption.

Write the absent-signal arm first in NODE5 and NODE6 to restore native branch
orientation. In NODE3, publish DemodDataV22 directly to `*rxcount` and test that
word. Native tests AX at `0x8b701` and stores AX at `0x8b704`; the prior shared
`nsym` automatic obscured the output-word lifetime. This also restores denominator,
threshold and publication ages around DIV. NODE4 retains its separate `nsym` use.

## Discriminating controls

| Domain | Result |
| --- | --- |
| Logical/bitwise conjunction × shared verdict owner | All four cells reproduce the baseline object literally. |
| Gap reset timing C × absent-arm orientation D | Baseline/C/D/CD: SIZE160/160/150/150. D alone lacks the native early clear; retain CD together. |
| Output-word consumption E | E alone SIZE160; CDE SIZE150. E restores independent native read/publication ages despite its isolated unchanged grade. |
| Gap type × comparison width | Short baseline SIZE160; int SIZE176; explicit short comparison is baseline-identical; int plus short comparison SIZE165. No type change retained. |
| Consumed quotient plus boolean comparison | SIZE144/134 but introduces SETB/TEST and loses native epochs; rejected. |
| Quotient local with direct comparison | SIZE160/150 but reverses native CMP operands and branch relation; rejected. |

The source delegate compiled 13 distinct successful source cells, plus an
ordinary/dump identity control. Parent width controls have a separate denominator.
Extra High independent review supplied the DIV-home and output-word witnesses;
no token/billing efficiency comparison is available.

The predicate byte at native `esp+0x33` is stored by SETE and immediately
zero-extended, with no other use. It is not evidence of a long-lived source
char. Current short gap already expands to SI producers and a HI comparison.
Native also homes the DIV numerator at `esp+0x38` and quotient at `esp+0x3c`;
the reconstruction homes neither. Native frame is `0x7c`, candidate `0x6c`.
Native and reconstructed answer both have frame `0x6c`, rejecting a global
status-object size explanation. No padding, unused carrier or forced spill was
introduced. Reopen with a new source/ABI/def-use witness; do not repeat these
closed spelling/type domains.

## Validation and provenance

The parent build reproduces the selected full-TU object SHA-256
`f78fdfca15680c06a52257fc529ff052bcc56911807902788ad159a19d58c6c5`.
Only originate changes among ten TU bodies, and only its object changes among
300 compared production objects. Exports, emission inventory and order stay fixed.
Five allocated nontext sections retain masked payload and layout. Two of 58
code-table target offsets move by -4, with unchanged function owners; all 174
native/baseline/candidate targets decode. Raw allocated data and strict target
identity are therefore not unchanged. These checks do not prove full CFG
semantics or protocol reachability.

Focused period differential: three binaries passed (`t_v22org`, `t_v22modem`,
`t_v22dp`). Full `make phase J=8`: **399 passed, zero failed**, plus structural
checks. Fixtures include component histories and synthetic state probes; this
wave adds no public modem reachability claim. No mutation campaign or snapshot
refresh was needed.

Frozen experiment evidence is under
`/mnt/zfs/dev/slmodem/scratch/v22-originate-20261105/` (baseline,
investigation/REPORT.md, per-cell inputs/commands, review and parent-gap).
Parent gate/closure reports are under the active worktree's
`build/v22-originate/`. Baseline revision is
`401a61d6af928a674d318788d70d3ff5efed4097`; native object SHA-256 is
`1f3e56d0dfae1a6aaf4eb6fcc4875a4524905e010d5758114cde288b3cf0b379`.
The new [native graph screen](method/native-graph-screen.md) independently
nominates the baseline gap relation once and the selected candidate zero times.
