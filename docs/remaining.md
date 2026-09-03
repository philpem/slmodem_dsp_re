# Remaining work — prioritised TU list and live status

**Numbers are measured, not maintained.** Regenerate with `make coverage`,
`python3 tools/service.py` and `python3 tools/worklist.py`; this file records
the *order* (a decision) and the *status* (a ledger). Where a byte count here
disagrees with the tool, the tool is right — see CLAUDE.md on shelf-life.

Measured post-wave-9-merge, 2026-09-03:

```
.text 734,605 bytes / 1,861 symbols
translated 95.8%  (704,001 bytes / 1,818 symbols)   was 76.6% / 1,296
remaining  15,195 bytes /   31 symbols (29 fax + 2 leaves)
  fax only              29 sym   14,151 B   <-- 93% of what remains
  no-entry-point leaves  2 sym    1,044 B
```

Merged master period-green at **365 passed, 0 failed**, onedef/banners/check64
clean, duplicate-symbol sweep clean.

## A correction that overturns three findings: `FIFO_CFG` was never actually
## blocked, and the reasoning that said it was applies to fewer of F9058's
## thirteen ambiguous names than it looked like

**F9020, F9199 and F9356 all declined `FIFO_CFG` on the belief that writing it
in `src/` would be a MULTIPLE DEFINITION at link** — the blob defines the name
twice (a file-local `d` at `.data` 0x83a0 holding {0,300,0}, a global `R` at
`.rodata` 0x9654 holding {0,100,0}), and the reasoning was that `symmap.py`
gives an ambiguous name no `ref_` alias, so the blob's own global copy would
still be called plain `FIFO_CFG` and collide with a new `src/` definition of
the same name.

**That reasoning does not survive checking the built object.** `symmap.py`'s
dedup logic (verified by reading the tool, not by trusting the finding)
refuses to rename a name only when it is AMBIGUOUS AMONG LOCAL SYMBOLS — two
static definitions in two different translation units, where a rename would
pick one at random. `FIFO_CFG` is not that shape: it is one local copy and one
copy that is ALREADY GLOBAL, and the already-global one is unaffected by the
local name sharing its string. `objcopy --redefine-syms` confirmed this
empirically against the real `build/dsplibs_ref.o` — `nm` after the rename
shows BOTH survive as `ref_FIFO_CFG`, one `d` (still local, still TU-scoped,
invisible to external linking) and one `R` (global, externally bindable). A
`src/` definition of `FIFO_CFG` collides with neither, and an external
reference to `ref_FIFO_CFG` can only bind to the global one — proven with a
wrong-copy injection test (planting the LOCAL copy's value, 300, and watching
the test fail on exactly that number) and confirmed **under the period
compiler itself**, not just GCC 14. See finding F9500 for the full derivation.

**What this does NOT say: F9058's general point still stands for the other
twelve.** Some of the thirteen duplicated names ARE genuinely local-vs-local —
`AGCv23_CFG` at two different `.rodata` addresses with different contents is
the example F9058 itself gives — and those really cannot be renamed
unambiguously. `FIFO_CFG`'s local+global shape was the one case among the
thirteen that got the wrong ruling. **Before declining any of the other twelve
on this ground, check which shape it is** — `nm --defined-only` on the blob
and read the case count, do not assume the whole bucket behaves like
`FIFO_CFG` or like `AGCv23_CFG`.

This landed as the FIFO chokepoint in wave 5 (below) and unblocked all four TX
constructors' `FIFO_CFG`/`FIFO_create` dependency in one move.

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
| 9 | Voice | `voice.c#3`, `Fdspkrnl.c +13`, `Beepgen.c +3`, `class1tx.c +94` | 16,685 | **DONE** — wave 5, merge `e1604049`, period 319/319. All 51 symbols; bucket is zero |
| 10 | FAX Class 1 | `class1tx.c +94`, `class1.c`, `class1rx.c`, fax arms of `voice.c#3` | 78,331 | **held off** — project phase, last. Restore `t_faxsgd.c` from `9b1739ee^` first (F8497) |
| 11 | the 15 remaining leaves | mostly `class1*.c`; 9 of them are F8492's link-blocked set | 3,167 | unblocks as their referents land — not a wave of its own |

Also on the board, not TU work: three written functions still route arms into
stubs (`VPcmV34Progress` ×5, `vpcm_run` ×5, `v34handshak` ×1 — see
`tools/worklist.py`'s closing section), and the tested-against-blob share is
**99.0%** -- 1,540 of 1,554 translated symbols are driven against the blob
itself, 631,385 bytes.
Recalibrating the MODERN tier for GCC 14 is its own task (see below).

**THE 1.1% THIS PARAGRAPH USED TO CLAIM WAS AN ARTEFACT, NOT A MEASUREMENT.**
`coverage.py`'s `tested` line needs `build/dsplibs_ref.o` and the test binaries
under `build/test`, and every low reading in this session was taken while
`build/repro` and `build/test` were ABSENT -- deleted, correctly, by the V.22
wave because they had been built with non-default `-O3` flags. The headline
`translated` figure does not depend on those, which is why it stayed right
while `tested` read 1.1% and was quoted repeatedly, including into this file.
Rebuild with a plain `make` before believing either number, and note that the
fourteen currently listed as untested are the just-merged voice symbols whose
test binaries are not yet built -- the SAME artefact, one wave younger. This is
findings F3055/F3110 again: a tool reading an object tree that is not there
reports a clean, plausible and wrong number.

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
        | awk '$2 ~ /^[TD]$/ {print $3}'; done | sort | uniq -d

**PRINT THE NAME ALONE AND COMPARE THE WHOLE OF IT.** The first version of
this snippet printed `name file` and used `uniq -d -f0`, which compares a
FIELD-SKIPPED, effectively truncated key: on long mangled C++ names it reported
FOUR duplicates that were one symbol seen once, all inside a single object. A
sweep whose false positives look exactly like the defect it hunts is worse than
no sweep, because the one time it is right nobody will believe it. If you want
the owning file too, find it in a second pass on the names this prints.

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

## Wave 5 — voice closed (merge `e1604049`, period 319/319)

All ten remaining call symbols and all seven data symbols, 6,502 bytes.
`detector_create`/`detector_progress` in a new `src/service/detector.c` with
the seven data symbols `static` beside them; `voice_*` in a new
`src/service/voicesvc.c`; the four `VOICE_*` in `src/service/voice.c`.
Findings F8800–F8846, deviations D1000–D1025.

**`TONEamode_CFG` was written WITH `detector_create`**, as the previous wave's
decline required — a `d`/LOCAL array whose only referent is inside one
unwritten function. Its twelve words were checked against F8781 AND against the
object's bytes rather than re-derived from the finding alone.

### What this wave found in the ORIGINAL, and could not test

- **D1022 — the blob's `VOICE_process` FAULTS at 8 kHz.** No converters are
  built and `RcFixed_Resample` dereferences NULL four instructions in; it was
  found by running it, when the first `t_voiceproc` segfaulted inside
  `ref_VOICE_process`. Our `fixedrc.c` carries a pre-existing NULL guard the
  object lacks, **so 8 kHz cannot be compared differentially at all** — the
  tests assert the precondition on both sides and drive 9,600 Hz only, which
  F8838 shows is the only rate that rate can actually run.
- **D1020 — the outer loop advances the caller's buffers by the SAMPLE count
  while consuming twice that in bytes.** Reproduced and ASSERTED rather than
  merely noted: the test checks the output byte at `3*block-1` was written,
  `3*block` was not, and `4*block-1` — which a correct pass would reach — was
  not. Reachable as soon as `count > 192`.
- D1021, D1023, D1024, D1025 — a mode reporting itself unknown, cursors
  wrapping 1,536 samples past a 384-sample array at 48 kHz, and an
  uninitialised ABORT argument.

### Two corrections the wave made to its own brief

- **The divisor is 8000, not 1000** (F8833). `0x10624dd3` is GCC's reciprocal
  for both; the SHIFT separates them, and `shr $9` on the high word gives 8000.
  That also explains why the rings do not overflow at 8 or 9.6 kHz — and it
  made D1020 twice as reachable as first estimated.
- **`voice.h`'s `rx_flt`/`tx_flt` are NOT crossed** (F8834). They are named from
  the DSP's side of the host link, and all four buffer names are consistent
  under that reading. No rename was made.

### Process notes

- **A branch moved underneath a running gate.** A subagent re-committed after
  its parent had reported, so the branch head went `afbb9e2a` -> `e225ad5b`
  mid-build. The gate was killed and restarted at the settled head.
  `git diff afbb9e2a e225ad5b` was EMPTY — the content was identical and the
  history was merely reshaped — but a build torn by a checkout underneath it is
  not a result, and that could not be known without checking. **Read the branch
  head at the moment you start a gate, and re-read it before believing the
  verdict.**
- **The merged tree was NOT re-gated, deliberately.** Master differs from the
  gated branch head only by `CLAUDE.md` and `docs/remaining.md`; `git diff
  --name-only` over `src/ include/ test/ tools/ Makefile` is empty, and neither
  file is read by any tier. The 319/0 verdict carries. Recorded because
  "I skipped the gate" needs its reason written down to be checkable.
- The harness gained `modem_recv_from_tty` (F8840). It previously existed only
  as a `ref_` alias whose body was `unexpected()`, which made two of
  `VOICE_process`'s four states untestable. Additive, but it relinks every test
  binary.
- **F8846 — the snapshot is back to 0 current / 228 stale**, because
  `src/service/voice.c` gained functions and `ringdet` went stale with it.
  Nothing is MISSING so the gate is unaffected. Six `voicedpdel.json` mutation
  DESCRIPTORS also needed rewriting for field renames: **a `find` string that
  quotes a renamed field stops matching silently.**

## Disk is a real constraint on this machine, and it bit mid-wave

The fax wave-2 agents ran the volume to **100% — 177 MB free of 35 G** — and one
hit `No space left on device` during a build. The cause is not the source: a
`build/` tree here is **1.5 to 2.3 GB**, because `build/test` holds ~330 test
binaries each statically linked against a 1.2 MB object, and every agent
worktree carries its own.

**Reap merged worktrees promptly.** Five of them held 5.9 GB between them after
their branches were already in master; removing them returned the volume to
84%. The check before removing is `git merge-base --is-ancestor <branch>
master`, never the worktree's age.

Two related traps, both seen this session:

- **The period shard trees are 1.1 GB EACH** and default to `/tmp`, which is a
  1.9 GB tmpfs here — so one barely fits and two cannot. Set `TMPDIR` to real
  disk for any mutation run (see the deferred re-record above).
- **A build that ran out of space mid-link is not a failed build, it is an
  UNKNOWN one.** Check a gate log for `no space left`/`write error` before
  reading its verdict, and re-run rather than trusting it.

## Wave 5 — the FIFO chokepoint, the 48-symbol adapter module, V.17 transmit start

Three agents on disjoint files rather than three parallel attempts at the same
wall, because one shared blocker (`FIFO_CFG`) sat behind all four TX
constructors. Merges: FIFO chokepoint `1bb8db5a`'s parent, adapter module and
V.17 transmit folded into the same merge sequence. All three gated
period-green individually (345, 344, 343 passed / 0 failed); final merged-tree
gate confirms below.

| agent | delivered |
|---|---|
| FIFO chokepoint | `FIFO_CFG`, `FIFO_create`, `V21TX_create` + V.21's whole transmit half-duplex machine (`TxNextStateV21`, `TxHdxStartV21`, `TxHdxIdleV21`, `TxHdxDataV21`) — ~2,450 bytes. **Corrected F9058/F9199/F9356** (above) |
| 48-symbol adapter module | 27 of 40 remaining lowercase forwarders, new `src/fax/faxadapt.c`, one TU in the object's own address order (F9271's ruling followed) |
| V.17 transmit | `SetTxModeV17`, `V17TX_SYM_SIZE`, `SMCv17_CFG` — small but disciplined: declined the rest rather than reach into files it didn't own |

### What this wave established

- **The `FIFO_CFG` correction, above — the wave's main result.**
- **V.21's transmit five are one indivisible unit**, exactly like its receive
  five: `TxNextStateV21` stores handler addresses for all four `TxHdx*`
  states, all four tail-call back into it. No proper subset links
  (F8492/F8493). This differs from V.27/V.29's RECEIVE side, which was NOT one
  unit — indivisibility is decided per machine, not assumed from a sibling.
- **The adapter module's `pack_width`/`unpack_width` split is now
  cross-confirmed**: all four RX creates write only `unpack_width`, matching
  the split `faxvmi.h` already named from the packer/unpacker work.
- **A live mutation is now standard practice for a state machine**: the FIFO
  agent swapped which handler `V21TX_STATE_START` installs and watched
  `t_v21txcreate` fail 82 of 155 checks before reverting — the equivalent of
  F134's ritual applied to a dispatch table rather than a data value.

### What's left, measured

Fax is **104 symbols / 34,898 bytes** — 96% of everything remaining in the
object. The V.17/V.21 transmit chains are open (V.21's constructor still needs
`V17TX_CFG`, `FPM_PPS_CFG`, `PPSv17_*`, the `SMCv17_*` encoder family — a
closed, self-contained batch of 4 functions + 3 tables the V.17 agent
identified but declined to rush, ~792 bytes); V.27/V.29 transmit are
essentially untouched; the remaining 13 adapter symbols are blocked on those
constructors and `*_control` functions; `_init_receiver`/`_init_transmitter`
(1,583 / 1,326 B) are last, each needing >140 symbols and are the natural
closing item once everything else lands.

## Wave 6 — SGD_CTL cleared, V29TX_create complete, V17TX_create's closure to 13

Three agents on disjoint files, chasing the new shared chokepoint `SGD_CTL`
(8 bytes, blocking three TX constructors and both open `TxNextState`
machines) plus two independent small wins. All three gated period-green
individually (348, 348, 349 passed / 0 failed); final merged-tree gate
confirms 351/0 above.

| agent | delivered |
|---|---|
| SGD_CTL + V.29 TX | `SGD_CTL` (genuinely unambiguous, unlike `FIFO_CFG` — a single global `.bss` symbol, no local duplicate at all; F9600 had declined it purely on file ownership) + the FULL `V29TX_create` closure, 20 symbols / 3,843 bytes, including the seven-state TX half-duplex machine as one indivisible unit |
| V.17 TX closure | SMC encoder family (`SMCv17_init`/`encoder_dif`/`encoder_abs`/`encoder_tcm` + 3 tables, 792 B) + PPS shaper tables (`FPM_PPS_CFG` + V.17's own) — `V17TX_create`'s closure narrowed from 27 to 13 symbols, all now blocked on nothing but `SGD_CTL` + `V17TX_CFG` |
| class1.c small wins | `cTOOLS_handle_hdlc_output` (403 B, declined twice by prior agents, resolved this wave), the 5-function null datapump (new `src/fax/nulldp.c`), `states_names`/`status_names` (248 B, author's-own-words tables) |

### What this wave established

- **A genuine SGD_CTL/FIFO_CFG contrast, checked rather than assumed.** The
  SGD_CTL agent was briefed to check whether it was really ambiguous before
  declining anything on F9500's ground — it was not; `nm` shows exactly one
  `SGD_CTL` symbol. F9500's correction was about a specific shape (local +
  already-global), not a blanket "duplicated names are fine now".
- **A cross-branch collision caught and resolved by evidence, not by
  picking a side.** Two concurrent agents independently derived `FPM_PPS_CFG`
  (V.17 agent in `fpm_pps.c`, V.29 agent in a new `fpm_pps_cfg.c`). The V.29
  agent's merge found both derivations agreed byte-for-byte on all 13 field
  values, kept the earlier one, deleted the duplicate, and corrected its own
  finding to point provenance at the survivor — independent confirmation
  treated as evidence rather than noise.
- **Two agents stopped mid-wait on a background monitor that could not
  re-invoke them** (the fork/monitor pattern that has bitten before) and had
  to be resumed explicitly to drive their own builds to completion and commit.
  Worth watching for in every wave: a "waiting for the build" final message
  with no live children behind it is not actually waiting for anything.
- **A disciplined decline on a suspicious call site.** `V17RX_control`
  (131 B) read as ready but its call passes the SAME pointer as two different
  arguments to `V17RX_create`; the agent left it rather than guess what that
  means, flagging it for a careful look rather than a wrong-but-plausible
  commit.
- **`fax_class1_progress` (1,145 B) was correctly declined** rather than
  guessed: it walks through a still-unmodelled offset (`ctx->0x1254`) whose
  meaning depends on structs three OTHER concurrent agents were actively
  extending. The cross-reference is left in `class1.h`'s comments for whoever
  picks it up once the structs settle.

### What's left, measured

Fax is **85 symbols / 30,526 bytes** — 96% of everything remaining. Open
threads: `V17TX_create`/`V27TX_create` (both now just `SGD_CTL` + their own
`*TX_CFG` + their TxHdx machine away), `V29TX_control` (unclaimed, reads
`V29TXP_RATE`, not traced), `V17RX_control` (declined, suspicious call site),
`FAXVMI_status` (down to one blocker, `vxx_status`, which lives in
`faxvmi.c`), the `vxx_*` dispatch table (declined, belongs beside
`vxx_message` in `faxvmi.c`), and `_init_receiver`/`_init_transmitter` last.

## Wave 7 — FAXVMI create/delete chokepoint, V.17 and V.27 transmit machines complete

Three agents on disjoint files, converging by design: `FAXVMI_create`/
`FAXVMI_control` turned out to be blocked on `SetScramblerV27` and
`TxHdxABV17` — exactly the two other agents' targets. All three gated
period-green individually (352, 353, 353 passed / 0 failed); final merged-tree
gate confirms 355/0 above.

| agent | delivered |
|---|---|
| FAXVMI chokepoint | `FAXVMI_CTL`, `vxx_status`, `vxx_delete`, `FAXVMI_delete`, `FAXVMI_status` — 5 symbols / 379 bytes. Re-measured (not assumed) that `FAXVMI_create`/`FAXVMI_control` were UNCHANGED afterward — neither touches the `vxx_status`/`vxx_delete` family |
| V.17 TX | Resolved `V17RX_control`'s suspicious shared-pointer call site as LEGITIMATE (self-referential reinit, confirmed against `V17RX_create`'s prologue) — not a defect. Then the whole `TxNextStateV17` + nine `TxHdx*V17` states + `V17TX_create` batch, 4,145 bytes, one indivisible unit. Found a genuine BLOB SEGFAULT on an untested retrain-transition input (F9901), worked around in the test rather than "fixed" |
| V.27 TX | The FULL `V27TX_create` closure — 42 symbols in one commit, the whole TX half-duplex machine plus 30 rodata tables, then two bonus symbols (`V27TX_modem`, `V27TX_control`) that unblocked once it landed |

### What this wave established

- **A chokepoint found itself.** Neither I nor any agent set out to link these
  three tasks; measuring `FAXVMI_create`'s actual closure revealed the
  dependency on the other two agents' targets. Scoping by measured closure
  rather than by file/module guesswork keeps finding this shape.
- **A "suspicious call site" resolved as correct, with evidence, rather than
  left declined forever.** The previous wave's agent was right to decline
  rather than guess; this wave's agent, alone in the file with time to look
  properly, traced both the call site and the callee's prologue and found the
  self-referential call is exactly what the object does. Declining pending
  more evidence and later confirming it is the intended cycle, not a wasted
  turn.
- **A genuine bug in the BLOB itself** (F9901) — the object segfaults on an
  untested retrain-transition input. Not our defect, not fixed, worked around
  in the fixture with the reason recorded. The same discipline as D956/D1022's
  earlier blob-fault findings.
- **Explicit anti-pattern warnings worked, partially.** Every wave-7 brief
  told agents to drive their own build rather than stop mid-wait on a
  background monitor with no live children. One agent still did it once and
  needed an explicit resume; the other two self-corrected or never hit it.
  Worth repeating in every brief until it stops recurring.

### What's left, measured

Fax is **54 symbols / 21,297 bytes** — 95% of everything remaining. The two
`_init_receiver`/`_init_transmitter` closures are now visibly shrinking each
wave (need counts have been falling: 122→85 and 125→88 over waves 5-6) as
their hundred-plus-symbol dependency trees get cleared from underneath by
other work landing. Expect them to become tractable within one or two more
waves rather than needing a dedicated assault.

## Wave 8 — faxadapt.c's last ready forwarders, class1.c quick wins, `fax_class1_progress` closed on the third try

Two branches landed, gated individually then on the merged tree:

| agent | delivered |
|---|---|
| adapter forwarders (`faxadapt.c`) | 9 symbols / 744 bytes: `v21tx_create`, `v27tx_create`, `v29tx_create` (177/229/196 B), `v27tx_process` (62 B), and five `*_control` one-line forwarders (`v17rx`, `v21tx`, `v21rx`, `v27tx`, `v27rx`, 16 B each). Confirmed by address evidence that `init_vmi_v17tx`/`v27tx`/`v29tx` belong to `class1tx.c`'s span (0x94870-0xac960), not this TU's or `class1rx.c`'s — correctly left for the other agent |
| class1 quick wins | 10 symbols: `_put_silence` (28 B), `_delete_data_rx_modem`/`_delete_data_tx_modem` (149/117 B), `fax_class1_progress` (1,145 B — **declined twice before, F9802; closed this wave** once the per-modulation quality-latch chain resolved through the now-complete V17/V27/V29 RX object headers), `fax_class1_delete` (347 B), `init_vmi_v17tx`/`v27tx`/`v29tx` (address-confirmed above), `_t30_silence_before_tx_state` (194 B), `_tx_silence_before_scrm_ones` (111 B). Two real bugs caught by the differential test rather than by re-reading bytes: a hardcoded `cfg->int_0014` in the TX VMI constructors (F10054) and a mis-tracked register in the silence-before-tx-state return computation (F10056) |

Both gated on `make period` alone (355/0, then 360/0 on the merged tree — the
+5 matches the wave's five new test files exactly). Structural gates
(`onedef`, `bannercheck`, `refcheck`) clean throughout.

**A `docs/coverage.md` partial-build artefact recurred and was caught before
merge (F3055/F3110's known shape).** The adapter branch's own `make coverage`
ran against a tree with only 3 of 357 test binaries built, committing
`tested 0.9%, 8 of 1792` in place of the true ~99.9%. Diagnosed by comparing
the branch's committed figure against master's, confirmed by counting
`build/test`'s entries (3, not ~355), and reverted before gating — the same
discipline every prior wave has needed here. `translated`, unlike `tested`,
does not depend on the test tree being complete and read correctly on both
branches, which is what made the artefact obvious rather than merely
suspicious.

**Both waves' foreground-wait stall recurred once, exactly as documented,
and self-corrected once resumed.** The class1 agent parked itself waiting on
a background monitor for its own test suite with no live children to
re-invoke it; one `SendMessage` telling it to drive the build itself in the
foreground was enough, and it also correctly declined to run `make period`
itself (no docker group membership in its shell) rather than silently skip
the gate or fake a result — flagged the conflict with the resume instruction
instead of picking one side unasked.

**The FAXVMI dispatch-table agent from earlier in this wave never committed
anything** — its worktree was clean, 6 commits behind master, and was
removed with nothing to merge. Its banked address evidence
(`.rodata` 0x9520 `vxx_process`, 0x9560 `vxx_control`, 0x9620 `vxx_create`,
all three sharing the same 13-slot order: `null_*` x5, then `v21tx`,
`v21rx`, `v27tx`, `v27rx`, `v29tx`, `v29rx`, `v17tx`, `v17rx`) is now
directly actionable: with this wave's landings, **`vxx_process` and
`vxx_create` have all 8 real slots present** (`v17tx_control` is the only
`*_control` still missing among the eight, and neither table needs
`*_control`), so both tables and `FAXVMI_create`/`FAXVMI_process` are
unblocked. `vxx_control` still needs `v17tx_control`, `v29tx_control` and
`v29rx_control`, none of which exist yet.

### What's left, measured

Fax is **35 symbols / 17,726 bytes** — 96% of everything remaining. Next
wave: the two now-unblocked FAXVMI dispatch tables plus `FAXVMI_create`/
`FAXVMI_process`, the three missing `*_control` forwarders unblocking
`vxx_control`/`FAXVMI_control`, and `_hdlc_emulate_receive_state` (452 B,
flagged READY in `class1tx.c` by this wave's agent but left for time reasons).

## Wave 9 — FAXVMI dispatch tables close, `V29RX_control` lands, `_hdlc_emulate_receive_state` closes, top-level fax entry points open

Four branches, each independently period-gated then gated again on the fully
merged tree:

| agent | delivered |
|---|---|
| FAXVMI dispatch tables | `FAXVMI_create` (705 B) and `FAXVMI_process` (327 B), plus the `vxx_create`/`vxx_process` 13-slot tables that had blocked them — the last two of the object's six `vxx_*` tables. Confirmed the table layout independently against `ref/slmodemd/dsplibs.o` rather than trusting the brief's inherited evidence |
| control functions | `V29RX_control` (110 B, `src/fax/v29.c`) landed; `V17TX_control`/`V29TX_control` correctly DECLINED as genuinely link-blocked on unwritten `V17TX_create`/`V29TX_create` (413/1,162 B — real construction work, out of scope this pass), full derivations banked for the next agent that picks them up |
| class1tx.c HDLC/data-pump cluster | `_hdlc_emulate_receive_state` (452 B) closed; the other 15 of 16 assigned symbols were traced and found ALL still blocked, on a small shared set of callees (`FAXVMI_process`, `FAXVMI_control`, `_init_receiver`, `_init_transmitter`) rather than on each other — a real bug in the already-committed `cTOOLS_handle_hdlc_output` (wrong terminator-write gating condition) was caught and fixed along the way, by a NEW test disagreeing with the blob rather than by re-reading disassembly |
| top-level fax entry points | `fax_class1_status` (150 B), `FAX_delete` (172 B), `FAX_process` (1,809 B) landed; `fax_class1_create`, `fax_class1_command`, `_init_receiver`, `_answer_tone_state`, `FAX_create`, `FAX_class1_command` all traced and confirmed blocked, five of six on `FAXVMI_create`/`FAXVMI_control` alone. New `include/dsplib/fax.h` opens `struct fax_ctx`, the FAX service's outer session object |

Merged in dependency order (FAXVMI tables first), then the HDLC cluster,
`V29RX_control`, then the top-level entry points last since it depended on
`FAXVMI_create` having landed to correctly diagnose its own blockers.
Individually period-green (361, 361, 343, 363 passed / 0 failed — the third
figure is lower only because that branch was furthest behind master and
built fewer objects); the fully merged tree gates at **365 passed, 0
failed**, onedef/bannercheck/refcheck all clean.

### What this wave established

- **A chokepoint cleared mid-wave and unblocked a sibling agent's work.**
  The class1tx.c cluster agent found all 15 of its remaining symbols
  blocked on `FAXVMI_process`/`FAXVMI_control` — written by a DIFFERENT
  agent in the same wave, landed only after the cluster agent had already
  finished tracing. 11 of the 15 need only `FAXVMI_process` (now landed);
  the other 4 also need `FAXVMI_control`, still blocked. This is the same
  "closure measured, not assumed" discipline as wave 7's FAXVMI chokepoint,
  recurring one layer further down the same call graph.
- **A finding-number collision recurred, twice in one merge sequence, and
  both times because a worktree branched before a sibling's commit landed.**
  Two different agents both wrote a finding numbered F10058 (ac745b11's
  FAXVMI landing and a9c621ca's fax_class1_status), and a THIRD, unrelated
  finding (aa507e2's V29RX_control, self-numbered F9500) collided with a
  pre-existing F9500 about `FIFO_CFG` from wave 5 — the same collision
  class CLAUDE.md's numbering section already names, now with a concrete
  example of a branch reusing an OLD number rather than clashing on a
  fresh one. Both resolved by renumbering the later-merged branch's
  findings (F10104-F10106, F10103) and fixing every in-source
  cross-reference (`grep`, not memory) rather than trusting the branch's
  own citations were still correct after renumbering.
- **The foreground-wait stall recurred on two of four branches this wave**
  (the control-functions agent and the class1tx.c cluster agent), each
  resolved with one `SendMessage` telling the agent to drive its own build
  loop rather than wait on a monitor with no live children. The other two
  branches self-corrected or never hit it. Still not eliminated by briefing
  alone, but the fix is now fast and reliable.
- **Disk exhaustion from over-parallelising the gate, not the agents.**
  Running all four branches' `make period` concurrently filled the disk to
  100% (four ~1.8 GB `build/` trees at once on a ~6 GB-free machine) and
  had to be aborted and re-run sequentially. The agents themselves ran fine
  in parallel — only their GATES compete for disk, and that competition is
  real even though each one individually fits comfortably.

### What's left, measured

Fax is **29 symbols / 14,151 bytes** — 93% of everything remaining. Next
wave: the now-11-of-15-unblocked class1tx.c cluster (needs only
`FAXVMI_process`, already landed), `FAXVMI_control` (338 B, needs
`V17TX_control`/`V29TX_control`/`V29RX_control`), and
`_init_receiver`/`_init_transmitter` (1,583/1,326 B), which between them
unblock most of what remains once either lands.

**CORRECTION, wave 10: `V17TX_create`/`V29TX_create` were NOT still
unwritten — this paragraph's own claim above was already stale the moment
it was written.** Both constructors (and their `V17TX_PPS_SCALE`/
`V29TX_PPS_SCALE` tables) landed in waves 6 and 7, well before wave 9 ran.
The wave-9 control-functions agent declined `V17TX_control`/`V29TX_control`
from a worktree branched 32 commits behind master — its own `grep` for the
constructors genuinely found nothing IN ITS OWN TREE, which was correct for
that tree and wrong about the object as a whole, and this file repeated the
claim without anyone re-checking `master` directly. A wave-10 agent
assigned to write the constructors found the same thing on `master` at the
start of its own turn, made no commits (fast-forward merge only), and
re-measured with `tools/closure.py`: **`V17TX_control` (148 B) and
`V29TX_control` (126 B) are blocked on nothing but themselves now.** Exactly
the shelf-life hazard CLAUDE.md's own `V90Parameters` example warns about,
happening to this file's own prose rather than a source comment.
