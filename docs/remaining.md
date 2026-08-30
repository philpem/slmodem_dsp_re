# Remaining work — prioritised TU list and live status

**Numbers are measured, not maintained.** Regenerate with `make coverage`,
`python3 tools/service.py` and `python3 tools/worklist.py`; this file records
the *order* (a decision) and the *status* (a ledger). Where a byte count here
disagrees with the tool, the tool is right — see CLAUDE.md on shelf-life.

Measured at `6a51f40b` (waves 1 and 2 merged), 2026-08-30:

```
.text 734,605 bytes / 1,861 symbols
translated 82.3%  (604,812 bytes / 1,471 symbols)   was 76.6% / 1,296
remaining 129,793 bytes /   390 symbols
  data modes            16 sym   10,013 B   was 65 sym / 39,166 B
  fax only             283 sym   78,331 B
  voice / CID / ring    67 sym   23,802 B
  no-entry-point leaves 15 sym    3,167 B   was 138 sym / 15,767 B
```

**The data modes are 74% cleared.** Wave 1 took the leaf bucket from 138
symbols to 15; wave 2 took the data-mode bucket from 65 symbols / 39,166 bytes
to **16 / 10,013**. Merged master is period-green at **293 passed, 0 failed** —
master's 258 tests plus 35 new ones across the two waves.

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
| 6b | **V.22 lifecycle — the declined set** | `v22.c` | 999 | **NEXT.** `V22FP_modem` 346, `v22_process` 557, `v22_create` 300, `dp_v22_init`/`exit`, `V22_PROTOCOL`, `v22_ops`, three `.bss` buffers. Written and compiling but NOT committed: the differential test fails and the agent could not establish why (F8538) |
| 7 | **V.32 FP layer and dispatch** | `Dialer.c +18`, `V32mod.c +39`, `v32.c` | 9,384 | **NEXT.** One closure of 25 symbols, no longer all-or-nothing: `V32FP_recreate` (3,733) is blocked on **eight tables totalling 596 bytes and nothing else**. Order: tables → `V32FP_recreate` → FP layer (`_status` 1084, `_control` 776, `_modem` 356, `_create` 169) → `v32_data` 859 → `v32.c`'s five (F8594) |
| 8 | Caller ID + ring detect | `cid_*` in `V32mod.c`, the rest of `voice.c#3`'s `RingDetector_*`/`RD_*` | ~7 K | after data modes. `CID_*` and `RingDetector_Reset` already landed in wave 1 |
| 9 | voice | `voice.c#3`, `Fdspkrnl.c +13`, `Beepgen.c +3` remainders | ~15 K | low priority |
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
