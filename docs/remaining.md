# What is reconstructed, and what is left

*Measured at `d367962`. Every figure here comes from a tool in `tools/`; the
commands are given so they can be re-run rather than trusted. Nothing in this
document is an estimate.*

*The per-function enumeration is `docs/worklist.md` (`make worklist`). This
document is the shape of the work, that one is the list, and `docs/plan.md`
is the order to take it in -- which is NOT this document's sec 7 order, because
sec 7 ranks by value and 477 of the 969 cannot be started yet.*

## 1. Where it stands

    make coverage
    make worklist

| | |
|---|--:|
| `.text` in the blob | 734,605 bytes / 1,861 symbols |
| translated | **48.5%** — 356,597 bytes / 892 symbols |
| driven against the blob | **100%** of what can be — 356,574 bytes, 885 of 892 |
| still to write | **969 symbols / 363,528 bytes** |
| `make phase` | exit 0, 1,436 PASS, 0 FAIL *(last recorded; not re-run here)* |
| codegen tier | *not re-measured — see below* |

The codegen row is left unfilled deliberately: `CLAUDE.md` records 92 of 365
shared symbols and the previous revision of this document recorded 105 of 386,
which cannot both be current. `make similarity` settles it and was not run
here, because it needs the period toolchain and a build this measurement did
not otherwise require.

The two byte figures do not sum to `.text`: 356,597 + 363,528 is 720,125, and
the 14,480-byte remainder is alignment padding and zero-size symbols. The
sum-of-symbol-sizes figure is the one to quote for work, because it is what
anyone will actually open.

The previous revision of this document reported 38.8% and 749 symbols,
measured at `f0112ff`, 168 commits back. Every table below has moved.

## 2. The three categories, and why the third is not optional

A symbol-by-symbol count sorts the object into two buckets and there are
three.

| | symbols | bytes |
|---|--:|--:|
| **not written** — the blob defines it, `src/` does not | 969 | 363,528 |
| **written** | 892 | 356,597 |
| **written, with unreconstructed regions** | 3 | *(see below)* |

The third is a subset of the second, not a fourth column, and it is invisible
to every count taken per symbol. `coverage.py` counts a symbol as translated
when "a function of the same name now exists" — a name is the whole test — so
a function that exists, links, and passes its differential test still counts
as done while routing some of its arms into a `*_notwritten()` stub.

Three functions do this today. The convention and its rule — always record
the code, and abort unless a test opted out by name — are finding 547.

| function | blob bytes | live stub sites | where |
|---|--:|--:|---|
| `VPcmV34Progress` | 7,278 | 7 | `src/pump/v34/v34pcmmain.cpp` |
| `vpcm_run` | 1,662 | 5 | `src/pump/v90/vpcm.c` |
| `v34handshak` | 61,541 | 1 | `src/pump/v34/v34hshak.c` |

**`v34handshak`'s one site is the table-3 `default:`, and it is unreachable** —
forty labels over the forty values the range test admits. It is not open work;
it stays so that a mutation to either the range test or the label set lands
somewhere. `docs/v34handshak.md` is the live tracker: tables 1 and 2 are done,
all sixteen table-3 targets are landed, and what is still open is inside
microstate 44 (`DET_INFO`, 6,046 bytes) rather than beside it.

**The other twelve sites are real, and five of them name work with no symbol
to be missing.** `V34PCM_UNWRITTEN_RUNPCM`, `_V90RUN`, `_QCLINE`, `_RESETP3`
and `_TONEPROC` name `runPcmModem`, `v90RunDemodulator`, `qcLineVerification`,
`vPcmResetPhase3Modem` and `GenericToneDetector` — and none of those five is a
symbol in the blob. GCC inlined them into `VPcmV34Progress`, so they cannot
appear in the 969 and cannot appear in any closure. Only `v90RateReneg` and
`v90RateRenegSilence` survive as symbols; both are unwritten and both are in
the list.

    python3 tools/worklist.py            # the stub sites, from the source

## 3. What is left, by translation-unit span

    make worklist

The span is from `tumap.json`, measured from the object. It is better
grouping than a call graph for deciding what to take next: a TU is the unit
the original was written in, and finding 330 is what happens when a graph walk
crosses one — a single call to `edprintf` used to drag `call_op` and the whole
`dp_*_init` family into every closure computed.

| span | symbols | bytes |
|---|--:|--:|
| `VPcmV34Main.cpp +72` | 360 | **176,330** |
| `class1tx.c +94` | 321 | 87,067 |
| `V32mod.c +39` | 92 | 46,071 |
| `Dialer.c +18` | 46 | 14,602 |
| `voice.c#3 +3` | 21 | 9,373 |
| `Fdspkrnl.c +13` | 29 | 7,635 |
| `Beepgen.c +3` | 27 | 6,546 |
| `class1.c` | 14 | 4,626 |
| `b103.c +2` | 24 | 4,471 |
| `class1rx.c` | 5 | 2,495 |
| `v32.c` | 5 | 1,691 |
| `dp_init.c +2` | 9 | 1,452 |
| `v22.c` | 5 | 1,071 |
| `vpcm.c` | 1 | 70 |
| `call.c` | 1 | 28 |
| `pow.S#279 +1` | 9 | 0 |

**The ten largest single functions left are all in `VPcmV34Main.cpp`, and all
ten are V.90 or V.92.** Nothing in fax, V.32 or V.22 comes close — the largest
outside this span is `V32FP_recreate` at 3,733 bytes, which would place
seventh.

| bytes | symbol |
|--:|---|
| 9,364 | `V90Equalizer::process` |
| 8,616 | `V90Phase3Demodulator::getV92Decision` |
| 8,379 | `V90Phase3Demodulator::getV90Decision` |
| 7,894 | `V90Parameters::loadParams` |
| 7,276 | `V90Demodulator::progress` |
| 4,887 | `V90ConstellationDesigner::adjustConstellationsToNewK` |
| 4,434 | `V90ConstellationDesigner::setConstellationToNoise_forceRate` |
| 4,055 | `V92Phase4Modulator::generateSymbol` |
| 3,922 | `V90Phase4Modulator::generateV92Symbol` |
| 3,767 | `V90TRN2Designer::V90TRN2Design` |

Anything over about 6 KB needs `docs/largefunctions.md` before it is started.

## 4. Two corrections to how this was measured before

Both concern `closure.py`, and neither is a defect in it — it answers the
question it was built for, which is "what must this batch define before it
will link". Neither reading survives being used to measure *remaining work*.

### The walk stops at what is already written, so "unreachable" over-counts

`closure.py:299` is `if n in have and n not in roots: continue`, and the
comment above it argues the case: a symbol `src/` already defines cannot leave
anything undefined, and walking through it would import the BLOB's callees
rather than ours. Finding 330.

The consequence for a *coverage* question is that every written function is a
wall. An unwritten symbol reachable only through a written one is reported as
reached by nothing.

**The previous revision's "341 symbols (59,372 bytes) no entry point reaches"
is that artefact and not an unreachability claim.** The worked example is the
largest unwritten function in the object:

    python3 tools/closure.py VPcmV34Progress --missing   # V90Equalizer::process PRESENT
    python3 tools/closure.py <every entry point> --missing   # ABSENT

`V90Equalizer::process` is 9,364 bytes, it is needed, and it is unwritten.
`VPcmV34Progress` is written, so a walk from the entry points stops one call
short of it. The same happens to most of the V.90 receive chain.

To ask "what does the original need behind this entry point", give
`closure.py` the entry point *and* the written functions on the path as roots
— roots are always expanded — or read `docs/worklist.md`, which does not use a
graph at all.

### A function pointer in a table is never followed

The `.rel.data` pass adds D and R symbols, so a pointer to a *function* parked
in a dispatch table or a C++ vtable is not walked. Those pointers are
`R_386_32` against a section symbol with the addend inline, so `objdump`
prints `.text` and names nothing — the same trap as finding 604's strings.

    python3 tools/indirect.py ../slmodemd/dsplibs.o

reports **125 functions reached only that way**, out of 1,922 relocations into
`.text` from data sections; the other 1,759 land mid-function and are switch
jump tables rather than entry points. They include all seven datapump
op-structs —

    call_op  v8_op  vpcm_op  v32_ops  v23_ops  v22_ops  b103_ops

each `{name, create, delete, run}` — the `*NextState` state-machine tables for
B.103 and V.32, the `FSE_decision_*` families, and the `Resampler` vtables.
This is why an entry-point list has to be written down rather than derived
from calls alone: `dp_vpcm_init`'s entire closure is itself and `vpcm_op`,
96 bytes, and every datapump hangs off the far side of that table.

The host-facing API is derivable and is 22 symbols — what the rest of
`slmodemd` leaves undefined and the blob defines:

    CID_{create,delete,process}   dcr_{create,delete,process}
    dp_runtime_{create,delete}    FAX_{create,delete,process}
    FAX_class1_command            prop_dp_{init,exit}
    RD_{create,delete,process}    RD_ring_details
    VOICE_{create,delete,process} VOICE_command

## 5. Work that is written but not on `master`

`docs/worklist.md` is measured against `master`, so anything sitting on an
unmerged branch is counted as still to do. Two branches hold real work, and
between them they are **8 symbols / 4,856 bytes of the 969 / 363,528**. Both
were checked by content, not by branch name: their source files do not exist
on `master` and their symbols are in the unwritten list.

| branch | commits | symbols | bytes | files `master` lacks |
|---|--:|--:|--:|---|
| `cid-dtmf` | 6 | 5 | 2,100 | `cid.h`, `cid_fsd.c`, `cid_mtd.c`, 2 tests |
| `v32-ecc` | 4 | 3 | 2,756 | `fpm_ecc.{c,h}`, `fpm_sre.h`, `v32sre_tables.c`, 2 tests |

`cid-dtmf` is `CID_FSD_demodulate` (1,049), `create_cid_dtmf`, `reset_dtmf`,
`CID_MTD_detect` and `FPM_div_32`. `v32-ecc` is `FPM_ECC_cancel` (2,051),
`FPM_ECC_init` and `FPM_ECC_free`.

**`v32-ecc` overlaps work in flight.** The `agent-v32` worktree has all six of
its files staged as additions on `v32-datapump` right now, so the two are
being landed twice. Whoever merges first should check the other rather than
resolve a conflict blind — finding 700 is what a merge that compiles but
drops half a side costs.

Two further branches are ahead of `master` and are **superseded, not
pending**:

- `worktree-agent-af64acb43cfd06605` — "The V.92 modulator chain", one
  commit, and `master` carries the identical content as `e6fc686`.
  `git cherry` marks it unmerged because the patch-ids differ; the files are
  byte-identical. It has no worktree.
- `review/nextsteps-2026-08-11` — recomputed the queue at 108 symbols /
  19,704 bytes for the construction path, which is exactly what `master`
  then landed.

Everything else is merged: all three V.90/V.92 method branches
(`v90adid-methods`, `v92convenc-methods`, `v92modenc-methods`), both
`ctorpath` branches, `leafsweep-a937`, all four `v22-*`, `v32-datapump`,
`v32-fse`, `task96`/`98`/`99` and every `worktree-agent-*` are ancestors of
`master`. Neither repository has a stash, and the four branches in the outer
`sip-D-modem` repository are all level with its `master`.

    for b in $(git for-each-ref --format='%(refname:short)' refs/heads/); do
        git merge-base --is-ancestor $b master || echo "$b is ahead"
    done

## 6. What these numbers do not cover

- **Byte counts are the blob's, not ours.** They size the reading, not the
  writing.
- **A count per symbol cannot see inside one.** §2 is the part of this that
  has been chased down; there is no tool that proves a written function
  reproduces all of its original's behaviour, only tests that fail when it
  does not. `debugaudit.py --missing` is the nearest per-function view, and
  its own caveat applies — the blob has 262 diagnostic call sites in
  `v34handshak` and we have 1, but a missing `edprintf` is not by itself an
  unreconstructed region, because the level ships at zero and the two behave
  identically (finding 134).
- **A C++ header's `/* +0xNNN */` comments are checked by nothing.** What
  pins a C++ layout is the `__builtin_offsetof` typedefs in the `.cpp`.
- **The codegen tier is no evidence for five classes** — ten of fifteen
  period-toolchain failures are one C++11 construct in three headers, which
  excludes `V90Equalizer`, `V90PreFilter`, `VPcmFloModem`, `V90Demodulator`
  and `V90Phase3Demodulator`. Finding 1308.
- **The mutation snapshot is stale tree-wide** (roughly 1 current of 117) by
  the owner's decision. Stale is not missing and fails no gate, but no entry
  should be quoted as a baseline until it is re-run.
- **This was measured against `build/` as it stood at `d367962`.** The symbol
  tables are the object's and do not move; what a concurrent edit in another
  worktree could change is which functions `src/` defines, so re-run
  `make worklist` rather than quoting a stale list.

## 7. The order this suggests

Unchanged in shape from the previous revision, because the exclusive-cost
argument still holds and V.PCM is still the end goal — but the starting
position inside it has moved a long way.

1. **The V.90 receive chain**, which is now the bulk of `VPcmV34Main.cpp`'s
   176,330 bytes. The constructors, destructors and object maps are written
   and asserted; what is left is the per-block processing, and the four
   largest items in the whole project — `V90Equalizer::process`,
   both `V90Phase3Demodulator` decisions, and `V90Demodulator::progress` —
   are 34 KB of it between them.
2. **The twelve live stub sites in `VPcmV34Progress` and `vpcm_run`**, taken
   with the chain above rather than after it: they sit on the path every V.90
   call takes, and five of them are work that will otherwise never appear on
   any list.

   **These are not twelve functions to open.** There is no `runPcmModem` in
   the object to disassemble — it and the other four inlined names are arms
   *inside* `VPcmV34Progress`'s own 7,278 bytes, so the disassembly to read is
   `tools/dis.py VPcmV34Progress`, and the stub call site in
   `v34pcmmain.cpp` marks where in our version that arm belongs.
   `vpcm_run`'s five are the same shape inside its 1,662 bytes. Only
   `v90RateReneg` and `v90RateRenegSilence` are separate symbols with their
   own entry in `docs/worklist.md`.
3. **Fax Class 1** — `class1tx.c +94`, 321 symbols over 87,067 bytes. The
   largest symbol count left, which is the shape that parallelises best.
4. **V.32 / V.32bis then V.22 / V.22bis** — `V32mod.c +39`, 92 symbols.
5. **Dialler, voice, Caller ID, ring detect, beep** — the `Dialer.c`,
   `voice.c`, `Fdspkrnl.c` and `Beepgen.c` spans, 38 KB together.
6. **`microstate 44` inside `v34handshak`**, per `docs/v34handshak.md`, and
   the tree-wide mutation re-record once reconstruction stops moving.

`V90Parameters::loadParams` (7,894 bytes) is the one genuinely-inert item on
the list: findings 860–862 show both its callees are three-byte stubs, so it
has no observable behaviour, and `tools/vparse.py` already extracts everything
it encodes. It is fourth-largest by bytes and should not be fourth by order.
