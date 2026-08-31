# Remaining work — prioritised TU list and live status

**Numbers are measured, not maintained.** Regenerate with `make coverage`,
`python3 tools/service.py` and `python3 tools/worklist.py`; this file records
the *order* (a decision) and the *status* (a ledger). Where a byte count here
disagrees with the tool, the tool is right — see CLAUDE.md on shelf-life.

Measured at `fb4519de` (waves 1–4 merged), 2026-08-31:

```
.text 734,605 bytes / 1,861 symbols
translated 86.0%  (631,530 bytes / 1,544 symbols)   was 76.6% / 1,296
remaining 103,075 bytes /   305 symbols
  DATA MODES             0 sym        0 B   COMPLETE
  fax only             283 sym   78,331 B   <-- 76% of what is left
  voice                 10 sym    6,378 B   Caller ID and ring detect COMPLETE
  no-entry-point leaves 12 sym    2,957 B
```

## Where this stands

**The data modes are complete**, and so are **Caller ID** and **ring detect**.
Merged master is period-green at **315 passed, 0 failed**, with `onedef`,
`banners` (366/366) and `check64` green.

**FAX is now 76% of everything that remains** — 283 symbols against 22 in every
other bucket combined. The project is at the phase boundary README describes:
everything except fax is essentially done, and fax was deferred on VALUE
(SpanDSP already implements Class 1 in the open-source world), not difficulty.

**Voice's last 10 symbols are NOT a leaf problem and NOT independent.**
`detector_create` (411) needs `TONEamode_CFG` and `tone`; `detector_progress`
(814) needs five data symbols; the other eight are blocked on those two and on
each other. **Six of the ten sit in the span labelled `class1tx.c +94`, which
is the FAX span** — a pass that reads "fax is last" as "skip that span" strands
them. That is the span-is-not-a-module rule with real money on it.

## THE DECISION TO DO FAX, TAKEN 2026-08-31

**Fax is no longer last, and this is the reason changing rather than the plan
drifting.** README's order and CLAUDE.md both put fax last, and CLAUDE.md is
explicit that the ground was **VALUE, NOT DIFFICULTY**: it is 283 symbols and
78,331 bytes, larger than everything else remaining put together, and SpanDSP
already implements Class 1 fax in the open-source world, so the marginal worth
of reconstructing it is lower than for anything else here.

That reasoning is still true. What changed is the GOAL. The objective has moved
from "cover the data modes and the services" -- which is now **done**, all of
it -- to **completing the object**. Under the new goal fax is not low-value; it
is the only thing between the tree and a finished reconstruction. Decided
deliberately, and it should be changed back the same way.

**THE STATED REASON FOR GOING NOW WAS WRONG, AND THE CORRECT ONE IS SIZE.** The
proposal was that voice depends on part of fax. It does not:
`closure.py` over voice's ten remaining symbols returns **17 symbols / 6,502
bytes, every one of them voice's own** (ten call, five data, two rodata). What
is true is that six voice symbols SIT IN the span labelled `class1tx.c +94`,
which is the fax span -- and that is the span-is-not-a-module trap this file
opens with, the same one that made the first V.32 estimate wrong in both
directions (F8160). **There is no ordering constraint between voice and fax.**
Fax is next because it is 76% of what is left, not because anything waits on it.

### What fax looks like, measured rather than quoted

| span | bytes |
|---|--:|
| `class1tx.c +94` | 68,409 |
| `class1.c` | 4,146 |
| `voice.c#3 +3` | 3,253 |
| `class1rx.c` | 2,495 |
| `V32mod.c +39` | 28 |

**83 symbols are startable today -- 14,080 bytes with no unwritten dependency**,
and they fall into families that parallelise cleanly rather than one monolith:
the three demodulators (`DemodDataV17`/`V27`/`V29`), their carrier and quality
detectors (`DataCarrierDetect*`, `QualityDetect*`), the V.27 scrambler pair,
`SMC_encoder`, the SGD sequence engine, and the FAXVMI framing layer.
`class1rx.c` has **0** ready, so it is downstream of the rest.

### Two things the fax phase inherits

- **RESTORE `test/unit/t_faxsgd.c` FROM `9b1739ee^` BEFORE ATTEMPTING SGD.**
  Wave 1 wrote the SGD engine, failed 96 of 3,603 checks under the period
  compiler with `det_at` landing megabytes outside the object's own buffer, and
  WITHDREW it rather than commit a structural error. F8497 says plainly that the
  TEST is the asset: it compares `det_at` as an OFFSET rather than a pointer
  value, which is what turned an invisible layout error into a 96-check failure.
  A byte compare of the object would have reported one differing run and named
  no field.
- **The 12 remaining leaves are mostly in these same files**, and 9 of them are
  F8492's link-blocked set. They unblock as their fax referents land, so they
  come free with this phase rather than needing a pass of their own.

## The order

Follows README's agreed order and CLAUDE.md's scheduling doctrine (V.32 is
`Dialer.c +18` **and** `V32mod.c +39` together, never a "Dialer pass"; leaves
are scheduled on their own merit, not as fax prep — F8320; fax is a project
phase, last on purpose).

| # | work | spans involved | size (blob bytes) | status |
|--:|---|---|--:|---|
| 1 | small closers | `dp_init.c +2`, `vpcm.c`, `call.c`, `b103.c +2` leaves | ~1.5 K | **DONE** — wave 1, `eea174a1` |
| 2 | leaves: V.90/V.92 API surface | `VPcmV34Main.cpp +72` | 5,805 | **DONE** — wave 1, `f76eceeb` (no derivations: F8430) |
| 3 | leaves: voice-span utilities | `Beepgen.c +3`, `Fdspkrnl.c +13`, `RingDetector_Reset` | ~4.3 K | **DONE** — wave 1, `688a1fb2` |
| 4 | leaves: V32mod/Dialer + fax-named exported API | `V32mod.c +39`, `Dialer.c +18`, `class1*.c` leaves | ~4.7 K | **DONE** — wave 1, `9b1739ee`; SGD withdrawn (F8497), 9 blocked on the link constraint (F8492) |
| 5 | V.32/V.32bis half-duplex machine | `Dialer.c +18` + `V32mod.c +39` together | 13,344 | **DONE** — wave 2, merge `a6da902b`. 26 symbols, 8 `TxHdx*`, 12 `RxHdx*`, all four `V32*NextState`, 6 tables |
| 6 | V.22/V.22bis/Bell 212 | `V32mod.c +39` + `v22.c` | ~15.9 K | **DONE** — wave 2, merge `41c62864`. 22 symbols, all seven `V22_PROTOCOL` handlers. Lifecycle DECLINED (F8538) |
| 6b | V.22 lifecycle | `v22.c` | 999 | **DONE** — wave 3, merge `ea5dfa6f`. F8538's failure was a TEST-BUFFER defect, not the source (F8607) |
| 7 | V.32 FP layer and dispatch | `Dialer.c +18`, `V32mod.c +39`, `v32.c` | 9,384 | **DONE** — wave 3, merge `e9110d66`. All 25, closure now empty; 14 tables, not the 12 predicted |
| 8 | Caller ID | `cid_*`/`data_*` in `V32mod.c +39` | 5,446 | **DONE** — wave 4, `011b53c5`, period 303/303. 11 functions + `V23_MRF_FILT`; CID closure empty. F8700–F8739, D970–D976 |
| 8b | Ring detect | `RingDetector_*`/`RD_*` in `voice.c#3 +3` | 2,061 | **DONE** — wave 4, `fb4519de`. All 8 symbols; 1,249,563 differential checks, 41/41 mutants caught |
| 9 | **Voice** | `voice.c#3`, `Fdspkrnl.c +13`, `Beepgen.c +3`, `class1tx.c +94` | 6,378 left of 16,685 | **MOSTLY DONE** — wave 4. 41 of 51 symbols written. The last 10 are interdependent, not leaves; six sit in the FAX span |
| 10 | FAX Class 1 | `class1tx.c +94`, `class1.c`, `class1rx.c`, fax arms of `voice.c#3` | 78,331 | **held off** — project phase, last. Restore `t_faxsgd.c` from `9b1739ee^` first (F8497) |
| 11 | the 15 remaining leaves | mostly `class1*.c`; 9 of them are F8492's link-blocked set | 3,167 | unblocks as their referents land — not a wave of its own |

Also on the board, not TU work: three written functions still route arms into
stubs (`VPcmV34Progress` ×5, `vpcm_run` ×5, `v34handshak` ×1 — see
`tools/worklist.py`'s closing section), and the tested-against-blob share is
1.1% with `v34handshak` (61,541 B) the largest untested translated symbol.
Recalibrating the MODERN tier for GCC 14 is its own task (see below).

## Wave 1 ledger (small closers + leaves, four parallel agents)

Finding blocks assigned: A F8410–8429, B F8430–8459, C F8460–8489,
D F8490–8519.

**WAVE 1 IS COMPLETE AND MERGED.** All four agents were killed mid-gate by an
account session limit on 2026-08-30 having committed nothing; the work was
recovered from their locked worktrees, gated one at a time on the period
compiler, and merged. Merged master: **275 passed, 0 failed**.

| agent | scope | commit | outcome |
|---|---|---|---|
| A | `dp_vpcm_exit`, `dp_call_exit`, `dp_init.c +2` (CID_*, prop_dp_*), `b103.c +2` leaves | `eea174a1` | 12 symbols, period 262/262. F8410–F8413 |
| B | the 67 `VPcmV34Main.cpp +72` leaves | `f76eceeb` | all 67, period 265/265. **No derivation record** — F8430 |
| C | `Beepgen.c`/`Fdspkrnl.c` leaves + `RingDetector_Reset` | `688a1fb2` | 20 symbols, period 261/261. F8460–F8466 |
| D | `V32mod.c`/`Dialer.c` leaves + fax-named exported API | `9b1739ee` | period 261/261. SGD **withdrawn** (F8497), 9 link-blocked (F8492). F8490–F8497 |

Merge commits `04e5284d`, `f45e66c7`, `91e1de4d`; the plumbing fix `9e152274`.
`src/service/cid.c` was an add/add conflict — A's `CID_*` wrappers and D's
`cid_*` setters both created it — and was hand-merged keeping both, per the
F700 rule against `checkout --ours`.

**Three things wave 1 established that outlive it.**

- **The link constraint (F8492, F8493).** A `src/` reference to an unwritten
  blob symbol CANNOT link: `symmap.py` renames every defined blob symbol, and
  every test binary links all of `$(OBJ_REPRO)`. This applies to a STORED
  function pointer exactly as to a call, so a leaf pass that checked only
  `R_386_PC32` call relocations would have written nine symbols that fail 90-odd
  binaries at link. Check `dis.py` for BOTH relocation kinds before scheduling.
  The premise "unwritten callees resolve to the blob" is the F214 spike, which
  F215 declined — do not brief an agent with it, as this wave's briefs wrongly did.
- **A span name is not a module name, again.** `GetNextDigit‐
  AndReturnNextState` (895 B, the largest item in D's scope) was ALREADY
  WRITTEN — an inlining-boundary artefact wearing a leaf's name (F8490). Check
  before scheduling by size.
- **The SGD withdrawal is the model for a failure (F8497).** 96 of 3,603
  checks failed with `det_at` landing megabytes outside the object's own
  buffer. It was removed rather than committed, because the defect is
  structural and no arithmetic fix reaches it. The TEST is the asset and is
  recoverable from `9b1739ee^`.

### What wave 1 cost, and the scheduling lesson (for wave 2)

**Do not run N agents that each run `make phase`.** The gate is expensive — the
period tier builds ~200 objects under an i386 container and the suite includes
heavyweight tests (`t_v90cdesign` alone ran 28 minutes of CPU from cold) — and
four concurrent phases on one machine contend for Docker and the CPU rather
than overlapping. Agent A measured **4 objects in 50 minutes at J=1 with five
containers contending**. All four agents spent most of their budget waiting,
and all four died before their first commit.

For wave 2: parallelise the *reading and writing* (which is what subagents are
for — their turns don't accumulate in the parent's window) but **serialise the
gate**, one at a time over a merged tree, or give the agents a cheap inner loop
(`make one T=…`) and let the parent run the single gate.

### `make period` is the gate for this work, and why

Settled 2026-08-30. **`make period` alone**, not `make phase`, for three
reasons that compound:

1. **It is the tier that decides** and the only one with no allow-list —
   CLAUDE.md's own position. Our source and the object, same compiler.
2. **This machine runs GCC 14; the tree was calibrated against GCC 13.**
   `tools/gccdiverge.json` was built for 13, so a modern-tier failure here may
   be an artefact of an uncalibrated compiler rather than a defect. One such
   breakage already bit: `t_v34rx.c` called `ref_V34InitializeImplementation‐
   Specific` without declaring it, which 13 warned about and **14 makes a hard
   error**, so `make phase` could not reach the test tier at all on a clean
   master (fixed in `9e152274`; the suite was swept and it was the only one).
   The hazard is that someone "fixes" `src/` to satisfy GCC 14 and moves the
   reconstruction away from the object — invisible to every test, because
   period would still pass. **Never edit `src/` to satisfy the modern tier.**
3. **`make phase` interleaves its tiers and its log cannot be read by
   position.** At `J>1` the period, modern-`test` and coverage tiers run
   CONCURRENTLY into one stream — three copies of `t_v90cdesign` were observed
   running at once on a 3-core box. A reading of "PASS lines before the first
   `gcc -m32` line are the period tier's" is WRONG, and this pass made that
   mistake before catching it. Attribute a verdict to a compiler by running
   that compiler alone, never by line number.

The modern tier is still the portability check and `make check64` still proves
64-bit cleanliness; recalibrating them for GCC 14 is its own task, not part of
a reconstruction wave.

**Operational notes.** `J` defaults to `nproc/2`, which is 1 on this 3-core
machine — pass `J=3` when the machine is yours (the Makefile says so at line
62). Run the gate under `nohup` so an interrupted shell does not kill it: a
stopped `make` leaves its `docker run` child ALIVE and compiling, which is one
source of the contention above. And a wrapper must exit with `make`'s own
status — `make … > log; echo; tail` reports the exit code of `tail`, and
reported a red gate as exit 0 in this very pass, which is findings F2400 and
F3100 with the gate itself as the victim.

## Wave 2 ledger (V.32 and V.22, two dependency-ordered chains)

Finding blocks: V.22 F8520–8559, V.32 F8560–8599. Both agents used `make one`
as their inner loop and ran NO gate; the parent gated each branch on `make
period` alone, then the merged tree. Merged master **293 passed, 0 failed**.

| chain | commit | outcome |
|---|---|---|
| V.32/V.32bis | merge `a6da902b` (branch head `d97ddb87`) | 26 symbols / 13,344 B, period 266/266. Nothing declined, nothing left blocked in that closure. F8560–F8594 |
| V.22/V.22bis | merge `41c62864` (branch head `f217acae`) | 22 symbols / ~15.9 K, period 268/268. Lifecycle DECLINED (F8538). F8520–F8538 |

### What wave 2 established

- **F8587 is the transferable one, and it bounds a technique this tree
  relies on.** A blob-against-blob dry run CANNOT catch an unplanted
  SUBSCRIPT: both sides read the same out-of-bounds neighbour of the same
  array and agree, so every comparison passes and only a segfault is left to
  chance. It is F134's dead detector again. A fixture must plant every field a
  callee uses as a subscript, not only every field it DEREFERENCES — and the
  test's own anti-vacuity counter was being satisfied by the undefined read,
  so it was reporting coverage the fixture had not arranged. See D955.
- **The V.22 lifecycle was DECLINED, correctly (F8538).** Written, compiling,
  and left uncommitted because its differential test fails and the agent could
  not establish why: ours holds `V22_CLAMP_VALUE` where the blob holds 0, while
  the return, both counts, the transmit samples, all three structs and all 28
  heap regions agree over 18,018 checks. Driving a handler directly gives zero
  divergences, so it is not the handlers. **An exclusion that moves when you
  move it is hiding a defect, not naming one** — scoping the sweep away from
  the failing table entry made the same failure reappear for the next entry.
- **Three facts that survive the decline:** `V22FP_modem` returns the whole
  32-bit word at `fp+0x1c`, not the status byte (an argument in a header
  comment was standing where a measurement was available, and the measurement
  refuted it on the first call); `v22_create`, `v22_delete` and `v22_process`
  are FILE-STATIC in the object, so the committed `v22_delete` is global and
  should not be; and status 4 is the 1200 connect.
- **`tools/bannercheck.py` is new (F8535)** and checks source banners against
  `nm -S`. Seven of nine addresses in one agent's own subagent briefs were
  wrong while every size was right. It found four more stale ones in wave 1's
  `src/fax/class1tx.c`, all off by 0x30, now corrected. **It is not wired into
  `make phase`** — that is a decision someone should make deliberately.
- **Open, and named:** F8533 (transmit path diverges between two graphs after
  ~40 blocks; the probe to run is a stack poison), F8537 (`FPM_TONE_generate`
  returns its count while `fpm_tone.h` declares it `void`; `v22_originate`'s
  NODE_3 divides by `hdx->r32` unguarded and faults in the blob), F8530
  (`fixedrc.c` uses `calloc`/`free` where the object uses
  `sysdep_malloc`/`sysdep_free` — left alone because `calloc` also zeroes).
  Eleven `v22fp.h` field renames are queued with evidence across F8526, F8531,
  F8534 and F8538, deliberately held back while five branches were live.
  `t_v32nsrng` and `t_v32nsloop` have no mutation suite registered.

### THE MERGE DEFECT THIS WAVE PRODUCED, AND THE RULE FROM IT

**Both waves reconstructed the same four symbols** — `V22FP_control`,
`ScramblerOn`, `DescramblerOn` and `V22FP_GetDiagnostics` — because the wave 2
brief did not EXCLUDE what wave 1 had already landed. Each branch was
period-green alone; merged, they were two definitions of one symbol. Git
merged them without a conflict marker, and the failure surfaced only at the
period compiler: first `conflicting types for 'V22FP_control'`, then
`multiple definition` failing **293 of 293 binaries**.

**So: scope a wave against what is WRITTEN, not against the span list**, and
after any multi-branch merge run a whole-tree duplicate sweep —

    for o in build/period/src_*.o; do nm --defined-only "$o" \
        | awk -v f="$o" '$2 ~ /^[TD]$/ {print $3, f}'; done | sort | uniq -d -f0

— rather than chasing one `multiple definition` at a time, which is what this
pass did for three rounds before doing the sweep. Fixed in `b5532c22` and
`6a51f40b`; the typed copies were kept, since the rest of V.22 is built on
`struct v22fp`.

## Wave 3 ledger (V.22 lifecycle, V.32 FP layer) — the data modes closed

Finding blocks: V.22 F8600–8639, V.32 F8640–8679. Merges `e9110d66` (V.32 FP)
and `ea5dfa6f` (V.22 lifecycle). Merged master **299 passed, 0 failed**, and
`onedef`, `banners` (348/348) and `check64` all green.

| chain | commit | outcome |
|---|---|---|
| V.32 FP layer and dispatch | `9c876c1f` | All 25 symbols / 9,384 B, period 297/297. `closure.py` on the V.32 entry points now reports 0 symbols, 0 bytes. 14 tables written, not the 12 F8594 predicted. F8640–F8658, D960, D961 |
| V.22 lifecycle | `d50604eb` | All 9 symbols, period 295/295, storage classes reproducing the object exactly. Eleven queued `v22fp.h` renames applied and proved pure. F8600–F8609, D956 |

### The lesson of this wave: `make one` HID A REAL DEFECT

The V.22 lifecycle was `make one`-green under GCC 14 and **failed the period
gate** at `t_v22modem`, 3,192 of 16,745 checks, `tx sample[8] got 0, reference
5205`. That is the case for gating on period stated as a measurement rather
than a principle, and it is the mirror image of the usual worry: not the modern
compiler inventing a failure, but **hiding one**.

The cause was the TEST's buffer, not `src/` (F8607). `V22FP_modem`'s copy-out
is `for (i = 0; i < *n_rx; i++)` with NO clamp — there is no `cmp $0x64`
anywhere in the object's loop at 0x888a7 — and `*n_rx` goes in as a SAMPLE
count and is meant to come back as a SYMBOL count, so nineteen of fifty-six
poked states leave 160 there and overrun a hundred-entry buffer. **Both sides
overrun identically** (36 cases write 12, one writes 13, nineteen write 160),
which is what proves it is the object reproduced rather than our defect. GCC
14's `.bss` layout absorbed the overrun; 3.4.2 put the transmit buffer in the
way. Recorded as **D956**, reproduced not fixed.

**AND IT RETRACTED AN ELIMINATION, WHICH IS THE PART TO COPY.** The agent had
ruled out a permuted `V22_PROTOCOL` using the transmit samples as the
discriminator — under GCC 14, the compiler that hides exactly that observable.
It withdrew the elimination rather than defending it, which re-opens F8538
honestly. Both its injections had the same flaw; an injection is only as good
as the compiler it runs under.

**F8538 may have declined CORRECT code.** The same failure mode explains its
symptom (`V22_CLAMP_VALUE` where the blob held 0 is what `RxClampV22` leaves in
the first twelve entries, with an unbounded copy-out from a too-small
destination). It is written as an INFERENCE with a named settling measurement —
recover that wave's `V22FP_modem` unchanged, enlarge only its test's
destination, run under `make period` — and explicitly NOT as a finding of fact,
because the code was never committed and re-deriving it would test a different
implementation.

### Other things worth carrying

- **D961** — `v32_data` hands `V32FP_control` an uninitialised stack local one
  block after building the request. Reproduced, not fixed, and **no differential
  test can cover that arm**; both tests clear `Control_Flag` and assert it clear
  rather than pretending to cover it.
- **F8653** — `VTBv32_init` is inlined in the object and called by us, so a
  per-function byte comparison reads 355 bytes short at `V32FP_recreate`. F605's
  case; no divergence to declare.
- **A merge conflict that had to be resolved by taking NEITHER side.** Both
  agents deleted their own `dp_v*_init`/`exit` bridge from
  `test/harness/unwritten.c`, so each branch kept the other's. Taking either
  side entire would have kept a bridge whose symbol is now defined in `src/`,
  and the period link would have said `multiple definition`. See the note now
  in that file.
- **`check64` WAS RED ON MASTER** from the wave-2 merge — seven ungated offset
  assertions in `src/pump/v22/v22.c` under a comment describing a guard that did
  not exist. Fixed in `afb1e9fb`. It was red because this session gated on
  `make period` alone and did not run the structural tiers it had said it would
  keep; `onedef`, `banners` and `check64` are now run on every merge.
- **Mutation suites are the outstanding debt.** `mutsnap.py --check` reports
  0 current, 216 stale, 0 never recorded. The V.22 and V.32 waves added none,
  and `t_v32nsrng`/`t_v32nsloop` still want registering — correctly declined
  until the snapshot is re-recorded, since a registered-but-unrecorded suite
  reads MISSING and fails the gate.
- **D956's fix is available and not taken.** The overrun is a defect in the
  ORIGINAL that we reproduce, so `src/` must keep it for the differential tier —
  but this tree has `DSPLIB_REPRODUCE_BUGS` (six sites today, `fpm_div.c` the
  worked example), where the default build carries the fix and the repro build
  stays faithful. A clamp to the buffer's own hundred entries under
  `#ifndef DSPLIB_REPRODUCE_BUGS` is that shape. D956 currently records the fix
  form as documentation only; revisiting that is a small, self-contained task.

## Wave 4 — Caller ID (done), ring detect and voice (in progress)

CID landed at `011b53c5`, period-green **303 passed, 0 failed**, with `onedef`,
`banners` (359/359) and `refcheck` clean. Eleven functions and one table,
5,446 bytes; `closure.py` over `CID_create`/`CID_process`/`CID_delete` is now
empty. Findings F8700–F8739, deviations D970–D976.

**It declined `_put_silence` (28 B) as OUT OF SCOPE, correctly** — the symbol
sits in the CID span but is the first global of the FAX `class1.c` translation
unit, and `src/fax/**` was fenced for that agent. A span name is not a module
name, and this is the rule being applied rather than quoted.

Two results worth carrying:

- **`mode` is a STATE, not a configuration** (F8735). The object's own string
  names 3 `CID_MESSAGE`, which is the author's word and the strongest class of
  evidence this tree recognises.
- **D976** — `ret = 3` is stored between the `cmp $0x3` and its branch, so the
  automatic mode reports failure the block AFTER it commits to DTMF.
  Reproduced; unreachable from slmodemd because `CID_create` fixes the mode
  at 0.

**All five CID bridges are gone from `test/harness/unwritten.c`**, which now
carries no CID bridge at all.

**Mutation suites were deliberately NOT registered** for the four new binaries,
because a registered suite that cannot be recorded reads MISSING and fails the
gate. The injection ritual was run by hand instead — 8 mutants on `data.c`, 11
on `cid.c`, 37 on `cid_progress` (33 caught, 3 equivalent, **1 real gap found
and closed**). That is the right trade while the snapshot is stale, but it
makes the mutation debt larger: `mutsnap.py --check` is 0 current / 216 stale /
0 never recorded, and a re-record pass is now overdue across V.22, V.32 and CID.

## Wave 4 ledger — Caller ID, ring detect, most of voice

Merges `011b53c5` (CID, period 303/303) and `fb4519de` (ring + voice, period
311/311 on its branch, 315/315 merged). Findings F8700–F8799, deviations
D970–D999.

| work | symbols | bytes | result |
|---|--:|--:|---|
| Caller ID | 11 + 1 table | 5,446 | closure empty. F8700–F8739, D970–D976 |
| Ring detect | 8 | 2,061 | 1,249,563 differential checks; 41/41 mutants caught |
| Voice | 41 of 51 | 10,307 of 16,685 | 444/444 mutants caught over fifteen suites |

### What this wave established

- **Ring detect is a hysteretic zero-crossing counter, not a filter** — three
  states, two comparator levels that swap ROLES (not merely sign) between half
  cycles, a debounce that tightens once locked, one frequency measurement per
  full cycle. Two fields are named from the object's own words (`RD_process`
  prints `freq`/`duration` over exactly what `GetLastRing` returns); the rest is
  usage inference and the header says so per field.
- **A `d`/LOCAL array whose only referent is inside one unwritten function must
  be written WITH that function.** `TONEamode_CFG` was declined on that ground
  and its twelve words recorded verbatim in F8781; `silence_level_table` was
  declined the same way and written later when its reader arrived. This is the
  link constraint (F8492/F8493) in its data form.
- **F8790 — a swapped smoothing weight is invisible to every codegen check AND
  to any one-block fixture.** Multi-block fixtures are not a nicety here.
- **`make one` remains necessary and not sufficient**, and this wave adds a
  second shape of it: `beepgen_sample` carries a `#pragma GCC optimize(
  "unsafe-math-optimizations")` scoped to one function, justified by the OBJECT
  containing `fsin` at 0xad2a8 (F8762, on `fft.cpp`'s F833 precedent) — **and
  GCC 3.4.2 ignores the pragma and calls libm**. The period gate passed anyway,
  so the two paths agree here; the seam is documented rather than assumed away.

### The process defect this wave surfaced, and it is worth more than the code

**A READY SET IS ONLY TRUE OF THE COMMIT IT WAS MEASURED AT, and work in flight
is invisible to it.** Two agents were briefed on `silence_progress` because the
second brief was measured against a branch one commit behind the first agent's
final state. `readyqueue.py` was not wrong — it was answering about the tree it
was given.

It cost nothing and paid twice: the duplicate became an independent second read
that confirmed the committed table byte-for-byte, and it found a comment
claiming a debug line prints 2700/7500/24300 when it prints 2699/7500/24299
(F8747). Correcting that revealed the table was covered by no mutation at all,
and that only one ULP direction per row is catchable — which the second read
ALSO predicted wrongly, and `mutate.py` settled. Three layers of check, and the
tool had the last word over both readings.

**When briefing concurrent agents, re-measure readiness at the commit each brief
is actually written against, and say in the brief which commit that was.**

## The mutation re-record is DEFERRED to a faster machine, and here is what it needs

Attempted 2026-08-31 and stopped deliberately at 11% (1,065 of 10,075
mutations, ~35 minutes) once the full cost was measured: **about 5.5 hours on
this 3-core box.** That is a machine problem, not a method problem, and the run
should be repeated where it is cheap rather than nursed where it is not.

**State: 1 current, 227 stale, 0 never recorded, of 228 registered.** The one
current entry is `ringdet` (41 mutations, 41 caught), recorded complete before
the full pass began. Nothing is MISSING, so the gate is not failing on this —
staleness is a trustworthiness problem, not a red build.

**What the run costs, measured rather than estimated:**

- 10,075 mutations over 228 suites; the median suite is 27 mutations and the
  largest, `v34hstx1`, is 776.
- ~2.2 s per mutation serial — a rebuild and a test run each.
- ~30 mutations/minute at `--jobs 2`.

**THREE THINGS THAT MUST BE TRUE BEFORE IT WILL RUN AT ALL**, each of which
cost an attempt here:

1. **`build/repro` and `build/test` must exist.** A shard copies the whole
   build tree; those two had been deleted (correctly — they had been built with
   non-default `-O3` flags and `make` does not track flag changes) and nothing
   had rebuilt them. A plain `make` restores them.
2. **The shard trees need REAL DISK, not tmpfs.** Each shard `cp -a`s the whole
   build tree, which is **1.1 GB** here — `build/test` alone is 320 test
   binaries statically linked against the blob. On a 1.9 GB `/tmp` tmpfs one
   shard barely fits and two cannot, which is what produced
   `N of N shards died; this run is not a result`. Set `TMPDIR` to a disk-backed
   directory. Budget 1.1 GB per job.
3. **Reap orphaned shard trees after any failed run.** A killed run leaves
   `$TMPDIR/mutate-<pid>-*` behind; the first failure here left ~500 MB.
   `mutate.py`'s `reap_stale_workdirs` removes only those whose pid is gone.

**What is NOT a risk, and was checked rather than assumed:** an interrupted run
does not leave `src/` mutated. Mutants are applied inside the shard COPIES, so
`git status` stayed clean through three aborted runs. The danger that motivated
running it detached does not exist in the sharded path.

**Credit where it is due:** `mutsnap.py` reported nothing from the broken runs
— `COULD NOT RUN -- left stale, not recorded`, and a refusal to summarise. A
tool that emitted a partial snapshot here would have produced something
indistinguishable from a baseline, which is the failure this whole register
exists to prevent (F347).
