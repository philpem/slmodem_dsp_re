# Remaining work — prioritised TU list and live status

**Numbers are measured, not maintained.** Regenerate with `make coverage`,
`python3 tools/service.py` and `python3 tools/worklist.py`; this file records
the *order* (a decision) and the *status* (a ledger). Where a byte count here
disagrees with the tool, the tool is right — see CLAUDE.md on shelf-life.

Measured at `c1ca61af`, 2026-08-30:

```
.text 734,605 bytes / 1,861 symbols
translated 76.6%  (562,394 bytes / 1,296 symbols)
remaining  157,731 bytes / 565 symbols
  data modes            65 sym   39,166 B
  fax only             283 sym   78,331 B
  voice / CID / ring    70 sym   24,467 B
  no-entry-point leaves 138 sym  15,767 B
```

## The order

Follows README's agreed order and CLAUDE.md's scheduling doctrine (V.32 is
`Dialer.c +18` **and** `V32mod.c +39` together, never a "Dialer pass"; leaves
are scheduled on their own merit, not as fax prep — F8320; fax is a project
phase, last on purpose).

| # | work | spans involved | size (blob bytes) | status |
|--:|---|---|--:|---|
| 1 | small closers | `dp_init.c +2`, `vpcm.c`, `call.c`, `b103.c +2` leaves | ~1.5 K | **in progress** (wave 1, 2026-08-30) |
| 2 | leaves: V.90/V.92 API surface | `VPcmV34Main.cpp +72` | 5,805 | **in progress** (wave 1) |
| 3 | leaves: voice-span utilities | `Beepgen.c +3`, `Fdspkrnl.c +13`, `RingDetector_Reset` | ~4.3 K | **in progress** (wave 1) |
| 4 | leaves: V32mod/Dialer + fax-named exported API | `V32mod.c +39`, `Dialer.c +18`, `class1*.c` leaves | ~4.7 K | **in progress** (wave 1) |
| 5 | V.32/V.32bis | `Dialer.c +18` + `V32mod.c +39` together | 25,925 firm / 30,306 ceiling | next — wave 2 |
| 6 | V.22/V.22bis/Bell 212 | `V32mod.c +39` (v22_* ~12 K) + `v22.c` | ~13 K | wave 2, same spans as V.32 |
| 7 | v32.c dispatch | `v32.c` (needs V.32 data tables) | 1,691 | with wave 2 |
| 8 | Caller ID + ring detect | `cid_*` in `V32mod.c`, `CID_*` in `dp_init.c`, `RingDetector_*`/`RD_*` in `voice.c#3` | ~7 K | after data modes |
| 9 | voice | `voice.c#3`, `Fdspkrnl.c +13`, `Beepgen.c +3` remainders | ~15 K | low priority |
| 10 | FAX Class 1 | `class1tx.c +94`, `class1.c`, `class1rx.c`, fax arms of `voice.c#3` | 78,331 | **held off** — project phase, last |

Also on the board, not TU work: three written functions still route arms into
stubs (`VPcmV34Progress` ×5, `vpcm_run` ×5, `v34handshak` ×1 — see
`tools/worklist.py`'s closing section), and the tested-against-blob share is
1.2% with `v34handshak` (61,541 B) the largest untested translated symbol.

## Wave 1 ledger (small closers + leaves, four parallel agents)

Finding blocks assigned: A F8410–8429, B F8430–8459, C F8460–8489,
D F8490–8519.

**WAVE 1 IS STALLED, AND NOTHING FROM IT IS TRUSTED YET.** All four agents
were killed by an account session limit on 2026-08-30, every one of them while
WAITING ON `make phase`. **No agent committed, so no symbol in this wave has
passed the gate** — the work is source on disk, not reconstruction, until a
green phase says otherwise. The worktrees are locked so nothing is reclaimed.

| agent | scope | worktree branch | state |
|---|---|---|---|
| A | `dp_vpcm_exit`, `dp_call_exit`, `dp_init.c +2` (CID_*, prop_dp_*), `b103.c +2` leaves | `worktree-agent-a4b2869817b730940` | **COMMITTED `eea174a1`** — 12 symbols, period green at 262 passed / 0 failed (= master's 258 tests + A's 4). Findings F8410–F8413 |
| B | the 67 `VPcmV34Main.cpp +72` leaves | `worktree-agent-a25c3f9a37c3f9e5c` | uncommitted: 40 files, 3,069 insertions + 2,081 untracked lines; **no findings written yet** (killed as it started them) |
| C | `Beepgen.c`/`Fdspkrnl.c` leaves + `RingDetector_Reset` | `worktree-agent-a4f874eb60f1b56fd` | uncommitted: 7 files, 1,614 insertions + 2,201 untracked lines; findings F8460–F8466 drafted; killed waiting on `t_v90cdesign` |
| D | `V32mod.c`/`Dialer.c` leaves + fax-named exported API leaves | `worktree-agent-a0e1e3bc9a937ef60` | uncommitted: 10 files, 440 insertions + 2,589 untracked lines; findings F8490–F8496 drafted; killed mid-period-tier |

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
