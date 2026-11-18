# Six-step byte-exact recovery wave, 5 November 2026

This wave improves recovery apparatus and records a bounded V.34 structural
investigation. It retains no reconstruction source change and makes no new
exact-function claim. Complete object identity remains open.

The baseline is `4317aa3edb4739bb9d1695330818533cacc94f19`. Work ran in an
isolated checkout, preserving the main checkout's unrelated changes. The
source/header manifest, original source archive, compiler configuration,
image identities, commands, ordered object manifest and 300 object hashes
are retained under `build/six-step-wave/` in the wave checkout.

## Preparation and instruction correction

Nineteen unused clean worktrees were removed. Branches and commits were
retained. Unique ignored build/scratch evidence was moved to
`/mnt/zfs/dev/slmodem/scratch/recovery-wave-20261105/retained-worktree-artifacts/`.
Five worktrees with uncommitted or paused work were preserved. The receipt
`/mnt/zfs/dev/slmodem/scratch/recovery-wave-20261105/cleanup.json` maps every
removed worktree and retained artifact. Two loose `/tmp` records were moved
into that archive; bytecode caches, dependency files and convenience symlinks
in removed checkouts were discarded. No claim is made that the archived
experiment evidence is redundant or that its disk space was reclaimed.

The Playbook's older "What does not work" section still recommended F8003's
`+0x7f` immediate-store filter. Lever 3b records why that filter fails:
epilogue conversions and other patterns also consume scratch, and surviving
stores omit the sites already transformed. The stale instruction is replaced
with the controlled period-build and actual-observation method. A failed
single swap excludes that swap, not TU order generally.

## Step 1: complete-object baseline

| Measurement | Fresh result |
| --- | --- |
| Period/structural gate | 398 period tests passed, zero failed |
| Period link input inventory | 300 objects |
| Function census | 1,132 EXACT, 27 REGALLOC, 74 BYTES, 619 SIZE / 1,852 |
| Exact reference bytes | 131,526 / 720,125 |
| Relocation mismatches / unresolved functions | 0 / 0 |
| Compared sections | 92 reference / 99 candidate |
| Exact section contents | 69 |
| Positioned allocated bytes equal | 72,622 / 943,398 |
| NOBITS bytes | 2,836 reference / 2,840 candidate |
| Exact relocation records | 1,019 / 18,317 |
| Exact symbol records, including FILE and undefined | 458 / 2,931 |
| Strict semantic / literal raw comparison | DIFFERENT / DIFFERENT, exit 1 |

The image used for compilation is
`ghcr.io/philpem/gcc-3.4.2-gentoo2005-docker:latest`, ID
`sha256:fe868cc44a48c36d1130862965729d0b384bc9e0a1908d37891aa5ae07352f16`.
The selected tools report Gentoo GCC 3.4.2-r2 and binutils 2.15.92.0.2.
The partial-link script's local `dsplibs-tc342-gentoo` image has a different
image ID. An explicit second link using the compilation image produces the
same literal object SHA-256:
`3a6b2c33e8d3b296e766154bdc95b053fdf23afe38aca1040e6adc36a8363e7a`.
Both identities and actual commands are recorded; matching banners alone
were not used to equate their link outputs.

## Step 2: bounded loaded-owner provenance

[The owner report](owner-entry-epoch-wave.md) documents first-entry-memory
pointer-path pairing, widths and offsets, conservative CFG joins, and
invalidation by calls, writes, unknown instructions and backedges. Parent
review added caller-owned argument-slot alias controls before integration.
Only private negative frame storage is exempt from that invalidation.

All 28 real-ELF control groups pass normally and under Python `-O`. A fixed
screen covers 72 defining-copy pairs representing 70 distinct names. It
reports zero errors and nominations. Two `initTxSequence` stores become
supported on each side; 21 copies per side still refuse whole-function
analysis. This is a bounded coverage gain, not closure of the owner gap.

## Step 3: V.34 shared continuation

The investigation crosses root-local terminal exits with three private
microstate exits, then independently tests ordinary placement of the existing
table2 body in the root. It preserves the table1 queue-loop boundary.
Native `0x62af1` forms the signed dispatch index; the actual indirect jump is
at `0x62b00`, and its common completion is `0x62a40`.

The eight-cell experiment produces no exact gains or losses across 29
native-common bodies. Root placement restores three indirect dispatches,
but artificial helper emission still moves: an existing wrapper is replaced
and another helper may become outlined. Table count and a smaller size gap
therefore do not justify production adoption. Return-short transport remains
a tested hypothesis about reconstruction helpers, not recovered original
source spelling. The source domain closes with no retained source edit.

The dedicated continuation report records the finite domains, period fixture
boundaries, full-TU effects, rejected alternatives and immutable ledger records.
All eight cells pass 14,391,258 checks each across three period fixtures.
The wrong-terminal-state control fails 88 of 6,297 checks as intended. See
[the continuation report](v34-terminal-continuation-wave.md) for its synthetic
component boundaries and the deliberately incomplete linked-relocation proof.
The next discriminator concerns shared helper closure and live-in ownership;
another terminal-exit spelling palette has no new witness.

## Step 4: final-object debt

`tools/recovery_object_debt.py` delegates identity to `partialcmp.py`. It
captures each input once and inventories name multiplicities, FILE order,
unique OBJECT extents and allocated-section metadata. Six real-ELF controls
pass normally and under `-O`, including an original-path rewrite after capture.
Duplicate local names are refused for extent pairing.

Of 740 common OBJECT names, 727 pair uniquely and have no size, binding,
visibility or section differences; 13 names remain ambiguous. This does not
establish their content equality or explain padding. Defined-name multiplicity
differences contain nine reference-only and 34 candidate-only records. Twenty-one
of the 300 link inputs remain unanchored. The first FILE-order mismatch is
reference `V34.c` versus candidate `V34ARRAY.c`.

The nine reference-only names are `pow` and eight local constants. Current
`src/core/pow.S` deliberately represents FILE records without their payload;
that build-input debt remains distinct from reconstructing modem algorithms.
A control inspected the period image's `libm.a` member `e_pow.o`. It defines
`__ieee754_pow` as FUNC with constants in rodata, unlike native `pow` as
NOTYPE with constants in text. It is not a drop-in original input; no library
member, blob bytes or inferred function extent was copied into source.

Seven allocated sections exist only in the candidate: six GenericIIR template
variants emitted by `FloatIIR.cpp` and `.rodata.cst16`. The latter has producers
in V92EchoCanceller, V90Phase4Demodulator, V90MappingParamsInt, ResamplerTiming
and V90Equalizer. These are ownership leads, not permission to remove required
definitions or narrow arithmetic. `extra-section-owners.json` records them.

## Step 5: compiler value-origin diagnostics

[The compiler report](compiler-defuse-wave.md) documents explicit
pseudo-register reaching definitions inside straight-line segments. Calls,
jumps, labels, barriers and unsupported effects end propagation. Every
correspondence keeps operands literal and explicitly declines to prove
equal input values or semantic equivalence.

All 22 new controls and 27 existing stage controls pass normally and under
`-O`. The CPt caller's combine stream has 128 supported origins out of 977
classified uses, with 849 unknown uses and 327 unclassified occurrences.
A concrete signed-word conversion becomes a frame-halfword read at combine;
the tool reports the resulting memory provenance as unknown.

The unchanged allocation observer was replayed with raw-equal assembly/object
output and a rejecting absent-target control. It observes 10 target / 141 TU
local choices and two target / 46 TU pass entries. Global choices, evictions
and reload decisions remain unimplemented; this wave does not claim a full
allocator model.

## Step 6: integration and acceptance

The original and fixed-root-placement four-cell experiments use the immutable
recovery ledger. Compilation provenance, period results and linked-byte scope
remain separate. Excluding relocation fields from a linked literal check does
not become a complete linked-body proof. No diagnostic source cell is adopted.

Parent validation runs the owner, def/use, object-debt, comparison-session,
ledger, allocation, byte-identity and stage-analysis controls normally and under
`-O`. Source/header and all 300 production-object identities are checked against
the baseline, alongside a final period/structural gate. Exactness remains a
separate measurement from analysis coverage. Mutation execution and snapshot
refresh are not part of this wave.

Final integration passes `make phase J=8`: 398 period tests, zero failures,
with structural checks green and all 10,044 anchors valid across 285 suites.
All 300 period comparison objects remain raw-identical to the baseline, as do
the reconstruction source and headers. The exact set stays at 1,132 names;
strict semantic and raw partial-link checks both still fail. No ratchet was
changed. This is a validated apparatus/evidence wave with zero new exact
functions, not completed byte-exact recovery.

The three worker Git checkouts were removed after integration. Their artifacts
now live under `build/six-step-wave/worker-evidence/`; lightweight compatibility
paths retain recorded absolute artifact/source paths without duplicate Git
checkouts. `worker-cleanup.json` records that mapping. Both continuation-ledger
freshness queries were independently rechecked after this cleanup and remain
empty. The active consolidated wave checkout retains the build and evidence.

Reproduce the object inventory without changing the completion comparator:

```sh
make partial-link J=8
python3 tools/recovery_object_debt.py ref/slmodemd/dsplibs.o \
  build/partial/dsplibs.o --json build/object-debt-fresh.json
python3 tools/recovery_object_debt_test.py
python3 -O tools/recovery_object_debt_test.py
make partial-compare-exact
```

The last command is expected to fail on this revision; it remains the
completion gate, with no normalisation or tolerance added by this wave.

The remaining priorities are preserving unaffected owner facts through
independently modelled instruction effects; CFG/table coverage; evidenced
post-call owner correspondence; production V.34 helper-closure structure;
original library/build-input recovery; and section/symbol/packing convergence.
Complete byte recovery still requires both strict semantic and literal raw
partial-link equality, in addition to the period differential gate.
