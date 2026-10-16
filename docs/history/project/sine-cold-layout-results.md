# SineWave cold-exit layout results

Pin2180f9d0; unchanged Gentoo profile and DSPLIB_REPRODUCE_BUGS. No source
adoption, new exact function, candidate-runtime or original-profile claim.
[Predeclared domain](sine-cold-layout-domain.md).

## Retained layout and pass ownership

The unchanged complete VpcmFloModem compilation reproduces its production
object raw. Retained generate is134B/BYTES50. Every nonbranch instruction and
operand multiplicity matches original, including physical registers. This
multiset observation does not imply identical instruction order or execution.

Prediction dump12.bp assigns the entry guard50/50 (no applicable heuristic),
and loop exit10%. Dump31.bbro traces0,1,3,2 in round1, then4,5 in round1;
final order0,1,3,2,4,5. Entry frequency2000 makes the round1 execution threshold
1000, exactly the zero-trip block's frequency. Its unconditional successor is
wrap. The later34.stack pass adds edge pops/copying; it does not independently
choose the earlier cold placement. Three differing cold/exit jumps remain.

The historical common-loop scalar control is a mechanism comparison, not a
new candidate: its loop-header prediction is64/36, with order0,2,1,3 and7
x87 edge observations. It reads phase before the guard, unlike original. A
source-level entry change can affect prediction and trace selection; this
alone cannot recover the blob's unseen RTL, hints or compilation profile.

## First-iteration owner is rejected

Two full-TU cells/68 shared-body verdicts: unchanged control and the independently
motivated first-iteration owner. Original skips first-iteration phase publication;
the candidate expresses that through i==0, with phase acquisition after guard.
It retains extra hot tests and duplicates block4 as8 during trace connection.
Final order0,2,4,1,3,8,5,6,7;174B/SIZE40, rather than134B/BYTES50. Exact set23/34
is unchanged. Only generate's body changes; symbol metadata/named objects and
33 bystanders are unchanged. Candidate is not runtime-validated or adopted.

Allocated sections also change: target COMDAT and .rodata.cst4. The latter
preserves its52-byte prefix and appends0000c07f, a quiet NaN. LC92 is absent
from generate's33.sched2 and appears as UID219 in34.stack, before the function
begin marker; generated assembly starts with flds LC92. The entry live set
contains x87 reg9. GCC3 reg-stack's convert_regs_entry initializes x87 registers
live at entry with QNaN. This is compiler stack repair, not an author constant.
Its presence alone does not prove a reachable C++ uninitialized use: predicate
history and conservative liveness are separate questions. Neither semantic
correctness nor undefined behavior is concluded from this rejected compilation.

## Diagnostic controls and transfer limit

The reusable tools/gcc3_layout_trace.py preserves prediction IDs/probabilities,
trace rounds/frequencies, final block order, explicit block copies and x87 edge
actions, with stage hashes. Three traces report8 prediction sites/8 traces/19
ordered blocks/29 edge observations. Candidate initially triggered the reader's
coverage refusal because trace connection creates a new block. Reader now
accounts only for explicitly recorded copies whose source was observed.
Known4→8 copy passes; substituted99→8 fails. This was a reader limitation,
not an invalid compiler run. Empty/ambiguous/uncovered observations still fail.

Screen of92 retained small floating-point copies finds exactly1 matching
nonbranch multiset: SineWave. This is a bounded nomination screen, not a ceiling
for all770 residual names. Close the two-cell first-iteration family; no follow-up
hint, goto, register or spelling matrix. Reopen only with a distinct source/TU
witness. Move to structurally different families for source recoveries.

## Source provenance and replay

Primary stock release sources explain mechanisms; installed Gentoo dump evidence
establishes this compilation's actual decisions. Do not equate stock text with
the patched compiler's complete source. Official release3.4.2 files:

- [bb-reorder.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/bb-reorder.c):
  c2bf10cdb6b91bf1559b186c3b9381c4b78c747dfde93214b05d5b5d065e5a59.
  STC rounds branch thresholds400/200/100/0 per mille and execution
  thresholds500/200/50/0 per mille of entry frequency.
- [predict.def](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/predict.def):
  f2ae27633ff465bb9af751a02ae6c8f4e80f8f4d78c2d40038a1a6a8a70c2236.
  Loop header64%; loop exit90% expected not taken.
- [predict.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/predict.c):
  f2b3c596e1afdab31dd807613ed9c4edfa7dd94f5506a5f994aade7061196cfa.
- [reg-stack.c](https://github.com/gcc-mirror/gcc/blob/releases/gcc-3.4.2/gcc/reg-stack.c):
  5af035ff9ef4070fdefb1d5ba787f09d16a8f3efff1c14dfc66e05fbccb120b0.
  convert_regs_entry and non-PIC QNaN construction identify the added load.

Replay at the pinned production tree with pinned headers. The audit also uses
the92-copy screen/archive from the previous expansion-transfer pass; reproduce
it through that pass's [instructions](../compiler/expansion-temporary-transfer-results.md).
Historical common-loop trace uses its earlier driver:

```sh
python3 tools/gcc3_sine_cold_layout.py --domain docs/history/project/sine-cold-layout-domain.md --historical-headers
python3 tools/gcc3_sine_scalar_transfer.py --domain docs/history/compiler/expansion-temporary-transfer-domain.md --historical-headers --baseline-dir build/expansion-transfer-before-objects
python3 tools/gcc3_sine_layout_audit.py
```

Audit requires production raw baseline,68 verdicts, exact predicted trace/copy
controls, the added NaN word and its first stack-stage appearance, matching
original nonbranch multiset, and the92-copy nomination denominator. Artifacts
are build/gcc3-sine-cold-layout/results.json and build/sine-cold-layout-audit.json.
The generic reader CLI accepts --dump-dir, --stem, --header and --out.

## Production validation

Production is unchanged:1082/1852 EXACT,119759 exact original bytes;
SIZE670/BYTES64/REGALLOC31/UNRESOLVED5. Ratchet green. make phase J=8 exits0:
388 passed/0 failed, all structural checks green. This validates retained
production, not the rejected candidate. No mutation or fuzz execution.
All300 faithful inputs equal canonical objects raw. Rebuilt partial object
is raw-identical to retained pre-pass partial: .text702012, allocated candidate
917705, positioned equal69377/943398. Both strict original comparisons remain
DIFFERENT/exit1. These unchanged-production measurements do not validate the
rejected control. Refcheck against2180f9d0 checks14429 references,0 now point
elsewhere. Final dangling check:14429 references/3054 finding headings, zero
dangling/pending/stale entries. Tool syntax/whitespace
checks pass. Existing PR284 CI checks passed before this diagnostic addition.
