# VPCM rate, receiver setup and cadence recovery, 2026-11-05

The wave recovers VPcmV34SetMinMaxBitRates **EXACT348**, moves
VPcmV34InitiateRateRenegotiation from SIZE7 to BYTES39 at native 259 bytes,
retains setupreceiver SIZE59→43, and fixes a constructor/audio-reachable
first-cycle cadence defect at SIZE87→26. Census: **1,138 EXACT / 28 REGALLOC /
72 BYTES / 614 SIZE**, 1,852 symbols, zero unresolved, no EXACT/REGALLOC losses.

Baseline source is `322cf66700830a559325379fab370b3ca6799f90`. Three of 300
production objects change. Their complete 113 emitted bodies include 89 shared
native names and 24 reconstruction helpers; only the four selected functions
change. All exports and source emission order remain stable.

## VPCM: combine the observed owner and scalar lifetimes

> 🧠 **From Hindsight memory (Partial caller and owner-boundary recoveries)** — prior work identified early engine/K56 owners plus direct scalar low/high captures. This supplied a hypothesis, not current validation.

A fresh eight-cell engine-view × early-owner × scalar-capture matrix reproduces
the baseline object literally. Only the combined cell recovers SetMinMax EXACT.
The early session read in Initiate and early K56 read in SetMinMax precede their
native dispatch guards. Preserve the native interior owner at object+4 through
member-derived displacements, and capture the V.90 low/high rate values directly
through the parameter owner before invoking the designer. A parameter-pointer
local is not an interchangeable hypothesis.

The access view uses existing named members via offsetof, not a new guessed
engine type. A period-layout audit checks all 21 converted 32-bit uses across
seven fields: progress, rate_min/max/now/want and the two receiver indicators.
The model does not establish the author's unique original declaration.

Only these two bodies change among 56. All 89 allocated-data code targets
retain owners, function-relative offsets and instruction boundaries; raw text
addresses move. A deliberately interior target is rejected. Both selected
normal/dump objects agree literally. Pair graph nominations fall 6→0, with
nine matched anchors and 20/24 compared relations in the respective functions;
one ambiguous anchor remains in the first.

Initiate's absent-clamp spelling control is literally inert. Its comparison
orientation already differs at initial RTL, combine folds a bound into memory,
and final scheduling changes two disjoint root-field operations. The early
session capture is unused on the handshake path; it does not establish a
callee-induced value bug. Reopen with new owner/type/lifetime evidence rather
than a predicate or register spelling palette.

## Receiver setup: preserve both observation boundaries

Move baud/carrier acquisition after rxinit, and acquire det_coefs at detectorinit
rather than keeping an entry snapshot across earlier calls and publications.
The native witnesses are at 0x5f065/0x5f072 and 0x5f293. The full two-property
cross gives the selected 989-byte body versus native 946.

Native callee review bounds rxinit's buffer/Hilbert and receiver writes; it
finds no rate/descriptor-field write establishing a numerical discrepancy in
the tested component history. This is operation-age recovery, not a claim of
a demonstrated callback or negotiation bug. Only setupreceiver changes among
all 53 emitted bodies, including 24 helpers absent from the native symbol set.
Seven normalized allocated sections and 227 code-table owner/relative targets
remain unchanged; raw absolute target addends shift with text layout.

All four source cells have normal/dump identity. The owner graph compares only
three anchors/two relations with one ambiguity, removing one nomination. That
bounded result is corroboration. The descriptor-lifetime control discriminates
a residual scheduling dependency: both related forms first reorder independent
wrap/increment stores at sched2, after preserving them through bbro. No source
permutation is retained to force that scheduling choice.

## Cadence: a missing rejoin, not merely a delayed field read

Native first-cycle handling increments failures, then rejoins the ordinary
cycle-count matcher guard. The old source's else-if excludes the matcher on
that first cycle. The constructor accepts cycles=1 without clamping it, and
looped matching can compare the valid first on/off period to itself.

A new differential fixture constructs each side normally, supplies 500 ms
silence, 500 ms of 550 Hz audio, then 500 ms silence, and plants no detector or
period state. Three controls distinguish one-cycle looped, one-cycle unrolled,
and three-cycle looped configurations. At sample 8,159 the native looped case
returns DETECTED while the old source returns NOTHING. The old unrolled case
also omits the parameter read; the three-cycle control stays green.

Frozen old-source results: 7,843 failures / 1,560,006 checks for one-cycle looped,
3,841 / 1,560,006 for one-cycle unrolled, and 1,560,006 passing checks for the
three-cycle control. All three pass with the repair. Checks cover per-sample
returns, complete compared component state, parameter traffic and native
admission/verdict anti-vacuity assertions. This proves the tested cadence
component histories, not public modem end-to-end operation.

Restore the common guard and the native finite empty loop after the parameter
read. It decrements a local count; it is not polling for an external update.
An ascending source loop lets GCC's loop pass reproduce its positive guard,
count copy and DEC/JNE form. Six source cells cover the guard × countdown
control and the corresponding ascending-loop pair; normal/dump outputs match.
Selected progress is 1,607 bytes against native 1,633. Only progress changes
among four bodies; exact reset/delete and constructor body remain unchanged.

Its graph grows 14→16 paired anchors and 59→76 relations, with nominations 2→0;
six ambiguous and one unpaired anchor remain. Eight constructor jump-table
destinations retain owners/relative offsets. Anonymous string order changes;
string contents and consumers are audited separately. Five CadenceNames slots
resolve to BUSY/DIAL/CONG/RING/INVALID, with a wrong-name pointer control firing.
No blanket raw-data or complete-CFG equivalence claim follows.

## Validation and mutation retirement

**Final make phase: 399 period tests passed, zero failed, plus structural
checks after mutation-gate retirement.** Five focused binaries pass: t_cadence, t_v34pcmapi, t_v34pcmif, t_v34hshak and
t_v34hsrx4. The cadence cases are constructed/audio histories. VPCM API/IF
fixtures include seeded component graphs and signed extremes, while receiver
fixtures also contain synthetic states; their green results do not establish
negotiated modem reachability. Final gate logs and hashes are in the
[evidence summary](evidence/rate-receiver-cadence-20261105.json).

The owner corrected stale mutation policy during this wave. The initial ten
anchor repairs are dropped. Routine anchor/snapshot gates and mutation-specific
CI self-tests are removed; historical records/tools remain unmaintained. The
period/compiler tier definitions, exact comparison rules and meaningful
regression tests remain intact. AGENTS now generalises address preservation to
interior-owner views, and its mutation instructions follow the retirement.

Frozen source/header/compiler/image identities, matrix cells, actual parsed
object hashes, data controls and reports are under
`/mnt/zfs/dev/slmodem/scratch/rate-receiver-cadence-20261105/`. Parent production,
focused/full gates and old-code regression artifacts are under the active
worktree's `build/rate-wave/`. A parent import initially confused setupreceiver's
source SHA with its object SHA; that assertion was rejected and corrected from
the frozen object before accepting the production comparison.

## Next steps

Audit null_process's signed-word count consumption against its unsigned formal,
including caller bounds and safe direct-component coverage. Then investigate
the remaining V8SetMessage branch/call graph lead. Keep BackwardClean and the
previously fitted SpectralShaper owner domain parked without a fresh witness.
Do not repeat the closed FPM, V22 or Initiate declaration/predicate palettes.
