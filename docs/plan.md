# A plan for the 961 functions that are left

*Companion to `docs/remaining.md`, which says what is left, and
`make worklist`, which lists it. This says in what order, and why that
order rather than the obvious one.*

*Measured at `93270f9`, after phase 0 landed. Re-run `tools/readyqueue.py`
and `tools/service.py` before trusting any count here; the whole point of the
ordering is that it moves as work lands.*

**The priority is complete data mode. Fax Class 1 is last.** That is the
owner's decision and it is what the phase order implements — but it cannot be
executed by deferring a translation-unit span, and §2 is why.

## 1. The two facts that decide the order

### Most of what is left cannot be started yet

    tools/readyqueue.py

| | symbols | bytes |
|---|--:|--:|
| **READY** — closure needs nothing unwritten but itself | 481 | **105,516** |
| **BLOCKED** — needs 1 or more unwritten symbols first | 471 | 253,156 |

A batch has to be **closed** before it can be committed: every dependency of
every member is in the set or already written. This is not a style
preference. `symmap.py` renames every symbol the blob defines to `ref_*`, so
one unwritten callee is not one failing test — it is an undefined reference
that fails all 92 binaries, and `make` stops at the first, which is
`t_encode` and names nothing involved. Finding 215 considered the escape
hatch (rename only what we define) and **declined** it deliberately; it is
not to be reintroduced.

**There is no bottleneck to unlock.** 105 KB is startable today and exactly
one dependency in the object is worth sequencing around (phase 1). The work
parallelises; the limit is review and machine time, not the graph.

### Data mode and fax are not separate spans

    tools/service.py

| who needs it | symbols | bytes |
|---|--:|--:|
| **data mode** — V.90/V.92/V.34/V.32/V.22/B.103/V.23/V.8/call progress | 285 | **209,067** |
| **fax only** — nothing in data mode reaches it | 286 | 78,718 |
| voice / Caller ID / ring detect only | 70 | 24,467 |
| no entry point reaches it | 311 | 46,420 |

Deferring fax takes **78,718 bytes off the critical path**, a little over a
fifth of what is left. But most of the 311 unreached symbols are data mode by
name — `V92CP::bitsToInfo`, `VPcmV34GetDiagnostics`, the V.90/V.92 CRC
methods — roughly 32,800 bytes the host calls directly rather than through a
datapump. **Complete data mode includes those**, so the target is about
242,000 bytes rather than 209,067, and phase 8 is where they are picked up.

## 2. The trap in "leave fax until last"

`make worklist` groups by translation-unit span, and the span printed as
`class1tx.c +94` is a **bracket over 95 translation units** — `class1tx.c`,
`faxvmi*.c`, `T30frames.c`, `V17rx.c`, `V17tx.c` and so on. Reading that as
"the fax span" and deferring it would block V.32.

**Thirteen symbols in that span, 11,232 bytes, are data mode**: the shared DSP
primitives the fax modulations and the data modulations both use.

| bytes | symbol | bytes | symbol |
|--:|---|--:|---|
| 2,286 | `FPM_SRE_recover` | 574 | `FPM_SRE_init` |
| 2,131 | `FPM_FSE_receive` | 547 | `FPM_TONE_find_rev` |
| 2,051 | `FPM_ECC_cancel` | 259 | `FPM_PPS_init` |
| 1,773 | `VTB_decoder` | 98 | `FPM_ECC_free` |
| 753 | `FPM_PPS_filter` | 61 | `FPM_TONE_kill` |
| 607 | `FPM_ECC_init` | 57 | `FPM_SRE_free` |
| | | 35 | `FPM_PPS_free` |

`FPM_ECC_*` is the **V.32 echo canceller** — the `v32-ecc` branch says so in
its own commit message. `SRE` is timing recovery, `FSE` the equaliser, `PPS`
the pulse shaping, and `VTB_decoder` the trellis decoder. They are phase 5,
they gate phase 6, and they must not travel with fax.

The converse holds too, and is why this was measured rather than assumed:
`V17RX_create`, `V27RX_create`, `V29RX_create`, `TxNextStateV17` and
`faxvmi_hdlc_unframe` are **fax only**. A first attempt at this partition put
all five in *data mode*, by seeding every indirect target as a data entry
point on the theory that `closure.py` cannot follow a function pointer in a
dispatch table. It cannot — but seeding a table's *arms* as roots throws away
the question of who reaches the *table*, and these arms are fax.

The fix is to name each service's own entry points and bulk-seed nothing,
which is what `tools/indirect.py` was written to discover. `service.py`
carries the four names above as a check that exits non-zero if they ever land
in data mode again; bulk-seeding makes it fire.

A `data symbol → the .text it points at` edge was built as well, on the
assumption it would be needed to reach the modulations, and **measured to
change not one number**: `FAX_create` reaches `V17RX_create` by ordinary
calls, through `fax_class1_create`, `FAXVMI_create`, `vxx_create` and
`v17rx_create`. It is not in the tool, and that is recorded so nobody adds it
back without a case that measures differently.

## Phase 0 — land what is already written  ✅ DONE

**Landed 2026-08-15 at `93270f9`.** Both branches merged, `make phase` passed
(phase boundary reached, 0 FAIL), and coverage went 48.5% -> **49.2%**: 900
symbols, 361,453 bytes, which is exactly the 8 symbols / 4,856 bytes the two
branches carried, so nothing was lost in the conflict resolutions.

Three conflicts were resolved by hand rather than by `--ours`, and one number
moved: `cid-dtmf`'s **D302 became D307**, because master had independently
allocated D302 to `FSE_decision_16pt` (itself already renumbered D298 -> D300
-> D302). Its companions D303-D306 were free and did not move. Both sides of
`tools/refcheck.py` had independently fixed the same catastrophic-backtracking
bug; master's unbounded fix won over `cid-dtmf`'s `{0,10}` bound, which would
silently stop rewriting citation lists longer than ten.

| branch | symbols | bytes | |
|---|--:|--:|---|
| `v32-ecc` | 3 | 2,756 | **data mode** — 3 of the 13 shared-DSP symbols above |
| `cid-dtmf` | 5 | 2,100 | Caller ID; phase 9 work, but written already |

The feared `v32-ecc` duplication was a non-event: all six of its files were
**byte-identical** to what the `agent-v32` worktree has staged on
`v32-datapump`, so it was the same work in two places rather than two
readings of it. That worktree's staged copies are now redundant and will
merge as a no-op.

Eighteen merged branches were deleted with them. `worktree-agent-af64acb…`
and `review/nextsteps-2026-08-11` are superseded rather than merged — they
are one commit ahead each and need `-D`, so they were left alone.

## Phase 1 — the one keystone, and the 17 KB behind it

`GenericToneDetector` is the only symbol in the object whose leverage is
worth sequencing around, and it is small.

| write | bytes | frees | bytes freed |
|---|--:|---|--:|
| `GenericToneDetector::reset()` + `::process(float)` | **386** | `V90Phase3Demodulator::getV90Decision`, `::getV92Decision` | **16,995** |
| `GenericToneDetector::process(float*, unsigned)` | 422 | `VPcmV34InitMOH` | 444 |

386 bytes unblock 16,995 — a ratio of 44, and nothing else in the object is
above 1.5. Those two decision methods need `GenericToneDetector::reset` and
`::process(float)` **and nothing else**; they are otherwise ready.

Then take the two decisions themselves. At 8,379 and 8,616 bytes they are
both over the 6 KB wall, so each goes to **its own subagent** per
`docs/largefunctions.md` — the cost there is cumulative context, not
disassembly size, and delegation is the only change that alters the exponent.

**A limit on what "done" means for these two.** `V90Phase3Demodulator` is one
of the five classes with no codegen-tier evidence (finding 1308: ten of
fifteen period-toolchain failures are one C++11 construct in three headers).
The differential tier still applies and still decides; the second tier will
not corroborate it here.

**Writing the class does not clear the stub.** `V34PCM_UNWRITTEN_TONEPROC` is
an inlined arm *inside* `VPcmV34Progress` that calls this detector. That is
phase 4, and phase 1 is a precondition for it rather than a substitute.

## Phase 2 — the V.90/V.92 receive chain's ready set

65,734 bytes ready now, and also the prerequisite set for phase 3. Take the
small classes first: they are cheap, all ready, and they are what
`V90Equalizer::process` (needs 31) and `V90Demodulator::progress` (needs 86)
are waiting on.

    V90SignBitsExtractor  733 B    V90BitsToSymbol      1,476 B
    V90SpectralShaper   2,392 B    V90Mapper            1,451 B
    V90ConstellationPower 1,408 B  V90Demapper          3,633 B
    V90CP               8,730 B    V92BitsToSymbol      1,283 B
    V92Transmitter      2,516 B    V92Modulator         3,119 B

and the three `V90ConstellationDesigner` methods ready today — 4,434, 3,760
and 3,641 bytes, the largest ready items in the object.

`V90Phase4Modulator` is 40 unwritten methods for 11,760 bytes: the highest
symbol count of any class left, and so the best candidate for a parallel
batch of small independent functions.

## Phase 3 — the four large blocked functions

Only startable once phase 2 has landed.

| bytes | function | still needs |
|--:|---|--:|
| 9,364 | `V90Equalizer::process` | 31 |
| 7,276 | `V90Demodulator::progress` | 86 |
| 4,887 | `V90ConstellationDesigner::adjustConstellationsToNewK` | 5 |
| 4,055 | `V92Phase4Modulator::generateSymbol` | 14 |

The lower two are under the 6 KB wall and are ordinary batches;
`adjustConstellationsToNewK` needs only five symbols and is the natural first
of the four. The upper two go to their own subagents.
`V90Demodulator::progress` is the hub — 86 dependencies — and should be
**last of the four**, not first. Re-run `readyqueue.py` before each: that
column moves most as phase 2 lands. `V90Equalizer` is also on finding 1308's
no-codegen-evidence list.

## Phase 4 — the twelve stub sites

Arms inside functions that already exist, link and pass. Invisible to every
per-symbol count, so they will be reported as finished for ever if nobody
schedules them.

- **`VPcmV34Progress`, 7 sites** — `runPcmModem`, `v90RunDemodulator`,
  `qcLineVerification`, `vPcmResetPhase3Modem` and `GenericToneDetector` are
  **inlined**, with no blob symbol of their own. There is nothing separate to
  disassemble: read `tools/dis.py VPcmV34Progress` and place each arm at the
  stub call site in `v34pcmmain.cpp`. The other two, `v90RateReneg` and
  `v90RateRenegSilence`, *are* symbols and appear in `make worklist`.
- **`vpcm_run`, 5 sites** — the same shape inside its 1,662 bytes.
- **`v34handshak`, 1 site** — the table-3 `default:`, **unreachable**, kept so
  a mutation to the range test or the label set lands somewhere. Not work.

## Phase 5 — the shared DSP that V.32 and V.22 need

The thirteen symbols in §2, 11,232 bytes, of which `v32-ecc` already supplies
2,756 if phase 0 landed it. This phase exists only because they live in the
fax span; it is data-mode work and it gates phase 6.

`FPM_SRE_recover` (2,286), `FPM_FSE_receive` (2,131) and `FPM_ECC_cancel`
(2,051) are all **ready today**.

## Phase 6 — V.32 / V.32bis, V.22 / V.22bis, Bell 103, V.23

37,647 bytes in the `V32mod.c +39` span, plus `v32.c` (1,691), `v22.c`
(1,071), `dp_init.c +2` (762) and what is left of `b103.c +2`. The large
items — `V32FP_recreate` (3,733), `v22_originate` (2,655), `V32OrgNextState`
(2,580), `V32AnsNextState` (2,539), `V22FP_create` (2,449) — are all under
the 6 KB wall and are ordinary batches. The `*NextState` arms are independent
of each other, so this parallelises well.

## Phase 7 — dialler and call progress

13,536 data-mode bytes in the `Dialer.c +18` span. Needed to place and
supervise a call, so complete data mode is not complete without it. That span
is mixed the same way the fax one is: `V32FP_recreate` and `DemodDataV32` sit
in it and are V.32, not dialling.

## Phase 8 — the data-mode API the host calls directly

218 symbols / 32,798 bytes that **no entry point reaches** through the link
graph, because the host calls them itself rather than through a datapump:
`V92CP::bitsToInfo` (1,957), `V92CP::evaluateInfo` (1,124),
`VPcmV34GetVisualDiagnostics` (1,023), `V90Phase4Modulator::setRfSymbols`
(1,005), `VPcmV34GetDiagnostics` (821), the `V90CP`/`V92CP`/`V90MP` CRC
methods, and `setParamsInfoFromV92CPUnPck`.

They are absent from every closure, so nothing will remind anyone they exist.
V.92 rate renegotiation and the diagnostics interface are part of what
complete data mode means. **This is the phase most likely to be forgotten.**

## Phase 9 — voice, Caller ID, ring detect, beep

74 symbols / 26,292 bytes, plus the `Beepgen.c +3` span (6,546, of which
3,949 is ready and nothing depends on it — a good batch for a session with
little context left). `cid-dtmf` from phase 0 has already done part of the
Caller ID work.

Placed here because the instruction names fax as last and data mode as first
and says nothing about these; they are small, and none of them blocks data
mode.

## Phase 10 — fax Class 1, last

286 symbols / 78,718 bytes exclusive to fax: `class1tx.c +94` (68,796 once
the shared DSP of phase 5 is removed), `class1.c` (4,146), `class1rx.c`
(2,495) and part of `voice.c#3` (3,253). V.17, V.27ter and V.29 transmit and
receive, the `faxvmi` framing layer, and T.30. It is the largest symbol count
of anything left, which is the shape that parallelises best when its turn
comes.

The `VTB_*` tables (`VTB_BOUND_*`, `VTB_REGION_*`, `VTB_DIFF_TBL`) are one
cluster, not eight independent wins — a byte-exact copy via
`tools/tabdump.py` is what they need, and `docs/fastpass.md` defers the
closed-form derivation to the 8 kHz retarget. `VTB_decoder` itself is phase
5, because V.32 needs it.

## Phase 11 — deliberately last, whatever the metrics say

- **`V90Parameters::loadParams`, 7,894 bytes, needing only two symbols
  (`Vparser_read_float`, `Vparser_read_int`).** It will rank near the top of
  every metric in this document. Findings 860–862 show both its callees are
  three-byte stubs, so the method has no observable behaviour, and
  `tools/vparse.py` already extracts everything it encodes.
- **`v34handshak` microstate 44** (`DET_INFO`, 6,046 bytes), the only part of
  table 3 still open — `docs/v34handshak.md` is the live tracker and says
  what is open is *inside* 44, not beside it.
- **The V.90 answer side.** `VPCMXF_Create` derives its side from a null
  argument its one caller always passes, so the digital-side sender is code
  the blob never enters and **cannot be driven differentially at all**. Only
  the codegen tier applies, and finding 1308 bounds what that is worth for
  these classes. "Done" here can only mean "reads correctly and compiles to
  the same instruction sequence" — say so wherever it is claimed.
- **The tree-wide mutation re-record**, owed once reconstruction stops
  moving: roughly 1 snapshot current of 117.

## What every batch owes, whatever phase it is in

1. **Read from the disassembly**, not a summary. Ghidra is scaffolding and
   never evidence; every line goes through `tools/dis.py` first.
2. **A tier-1 differential test per function.** Non-negotiable. It is not
   overhead — it is what makes writing fast, because each fault is localised
   in one run.
3. **A closed batch.** `python3 tools/closure.py <members> --batch` must say
   CLOSED before commit.
4. **`make phase`, not `make test`** — it runs the period toolchain
   (GCC 3.4.2 in `tools/toolchain/`), the modern build, `check64`, interop
   and the coverage gates. A rejection under 3.4.2 in `src/` is a *finding*,
   not a portability nuisance: the author wrote this for that compiler.
5. **A one-line `docs/deviations.md` entry** for anything that looks wrong,
   marked `unmeasured`. An unrecorded observation is unrecoverable.
6. **Nothing wrong-but-plausible is committed.** If a function cannot be made
   to pass, leave it out and record the attempt.

And two about the machine, not the code: build with **about half the cores,
never `-j$(nproc)`**, and place no bench call while a build or an agent is
running — the DSP is real-time and a loaded machine invalidates the run.

## Re-deriving all of this

    make worklist                                     # what is left
    python3 tools/readyqueue.py                       # what is startable
    python3 tools/service.py                          # data mode vs fax
    python3 tools/closure.py <members> --batch        # is my batch closed
    python3 tools/coverage.py                         # the headline
