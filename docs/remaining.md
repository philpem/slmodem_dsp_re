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

| agent | scope | branch | outcome |
|---|---|---|---|
| A | `dp_vpcm_exit`, `dp_call_exit`, `dp_init.c +2` (CID_*, prop_dp_*), `b103.c +2` leaves | `recon/closers-b103` | running |
| B | the 67 `VPcmV34Main.cpp +72` leaves | `recon/vpcm-leaves` | running |
| C | `Beepgen.c`/`Fdspkrnl.c` leaves + `RingDetector_Reset` | `recon/voice-leaves` | running |
| D | `V32mod.c`/`Dialer.c` leaves + fax-named exported API leaves | `recon/fax-api-leaves` | running |
