# A plan for the 969 functions that are left

*Companion to `docs/remaining.md`, which says what is left, and
`docs/worklist.md`, which lists it. This says in what order, and why that
order rather than the obvious one.*

*Measured at `d367962`. Re-run `tools/readyqueue.py` before trusting any
count here; the whole point of the ordering is that it moves as work lands.*

## The one fact that decides the order

`docs/remaining.md` sec 7 ranks the work by **value** — V.90 first, because
V.PCM is the project's goal and every one of its bytes is exclusive. That is
still true and it is not an order you can execute, because the largest item
in it cannot be started.

    tools/readyqueue.py

| | symbols | bytes |
|---|--:|--:|
| **READY** — closure needs nothing unwritten but itself | 483 | **106,504** |
| **BLOCKED** — needs 1 or more unwritten symbols first | 477 | 257,024 |

A batch has to be **closed** before it can be committed: every dependency of
every member is in the set or already written. This is not a style
preference. `symmap.py` renames every symbol the blob defines to `ref_*`, so
one unwritten callee is not one failing test — it is an undefined reference
that fails all 92 binaries, and `make` stops at the first, which is
`t_encode` and names nothing involved. Finding 215 considered the escape
hatch (rename only what we define) and **declined** it deliberately; it is
not to be reintroduced.

So the plan is a feasibility ordering. Value breaks ties inside it.

### READY bytes, by translation-unit span

| span | ready | blocked |
|---|--:|--:|
| `VPcmV34Main.cpp +72` | 65,734 | 110,596 |
| `class1tx.c +94` (fax) | 19,593 | 67,474 |
| `V32mod.c +39` | 6,237 | 39,834 |
| `Beepgen.c +3` | 3,949 | 2,597 |
| `Fdspkrnl.c +13` | 2,850 | 4,785 |
| `voice.c#3 +3` | 2,407 | 6,966 |
| `b103.c +2` | 2,183 | 2,288 |
| `Dialer.c +18` | 2,011 | 12,591 |
| `class1.c` | 743 | 3,883 |
| `dp_init.c +2` | 699 | 753 |
| `vpcm.c`, `call.c` | 98 | 0 |

**There is no bottleneck to unlock.** 106 KB is startable today across
eleven spans, and exactly one dependency in the object is worth sequencing
around. That is the useful shape: this work parallelises, and the limit is
reviewer attention and machine time rather than the graph.

## Phase 0 — land what is already written

Two branches hold finished, differentially-tested work that `master` lacks:
**8 symbols / 4,856 bytes**, at the cost of a merge.

| branch | symbols | bytes |
|---|--:|--:|
| `cid-dtmf` | 5 | 2,100 |
| `v32-ecc` | 3 | 2,756 |

**`v32-ecc` first needs its duplication resolved.** The `agent-v32` worktree
has all six of its files staged as additions on `v32-datapump` right now.
Read both sides before merging either; finding 700 is what a merge that
compiles but silently drops half a side costs. `worktree-agent-af64acb…` and
`review/nextsteps-2026-08-11` are superseded and want deleting, not merging —
see `remaining.md` sec 5.

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
an inlined arm *inside* `VPcmV34Progress` that calls this detector. It is
phase 4 work, and phase 1 is a precondition for it rather than a substitute.

## Phase 2 — the V.90 receive chain's ready set

65,734 bytes, ready now, and it is also the prerequisite set for the two
largest blocked functions in the project. Take the small classes first: they
are cheap, they are all ready, and they are what `V90Equalizer::process`
(needs 31) and `V90Demodulator::progress` (needs 86) are waiting on.

Roughly in dependency order, all currently ready:

    V90SignBitsExtractor  733 B    V90BitsToSymbol      1,476 B
    V90SpectralShaper   2,392 B    V90Mapper            1,451 B
    V90ConstellationPower 1,408 B  V90Demapper          3,633 B
    V90CP               8,730 B    V92BitsToSymbol      1,283 B
    V92Transmitter      2,516 B    V92Modulator         3,119 B

and the three `V90ConstellationDesigner` methods that are ready today —
4,434, 3,760 and 3,641 bytes, the largest ready items in the object.

`V90Phase4Modulator` is 40 unwritten methods for 11,760 bytes: the highest
symbol count of any class left, which makes it the best candidate for a
parallel batch of small independent functions.

## Phase 3 — the four large blocked functions

Only startable once phase 2 has landed. Each is over 6 KB and each goes to
its own subagent.

| bytes | function | still needs |
|--:|---|--:|
| 9,364 | `V90Equalizer::process` | 31 |
| 7,276 | `V90Demodulator::progress` | 86 |
| 4,887 | `V90ConstellationDesigner::adjustConstellationsToNewK` | 5 |
| 4,055 | `V92Phase4Modulator::generateSymbol` | 14 |

Both of the lower two are under the 6 KB wall, so they are ordinary batches;
`adjustConstellationsToNewK` needs only five symbols and is the natural first
of the four.

`V90Demodulator::progress` is the hub — 86 dependencies — and should be
**last of the four**, not first. Re-run `readyqueue.py` before each: the
"still needs" column is the number that moves most as phase 2 lands.

`V90Equalizer` is also on finding 1308's no-codegen-evidence list.

## Phase 4 — the twelve stub sites

The arms inside functions that already exist, link and pass. These are
invisible to every per-symbol count, so they will never appear on a worklist
and will be reported as finished for ever if nobody schedules them.

- **`VPcmV34Progress`, 7 sites** — `runPcmModem`, `v90RunDemodulator`,
  `qcLineVerification`, `vPcmResetPhase3Modem` and `GenericToneDetector` are
  **inlined**, with no blob symbol of their own. There is nothing separate to
  disassemble: read `tools/dis.py VPcmV34Progress` and place each arm at the
  stub call site in `v34pcmmain.cpp`. The remaining two,
  `v90RateReneg` (`V34PCM_UNWRITTEN_RRN`) and `v90RateRenegSilence`, *are*
  symbols and are in `worklist.md`.
- **`vpcm_run`, 5 sites** — the same shape inside its 1,662 bytes.
- **`v34handshak`, 1 site** — the table-3 `default:`, **unreachable**, and
  deliberately kept so a mutation to the range test or the label set lands
  somewhere. Not work.

## Phase 5 — fax Class 1

`class1tx.c +94` plus `class1.c` and `class1rx.c`: 340 symbols, 94,188 bytes,
of which **20,336 is ready today**. The largest symbol count left and the
shape that parallelises best — many small functions rather than a few large
ones. `FPM_SRE_recover` (2,286), `FPM_FSE_receive` (2,131) and
`FPM_ECC_cancel` (2,051) are the big ready items; note `FPM_ECC_cancel`
arrives free with phase 0's `v32-ecc` merge.

The `VTB_*` tables (`VTB_BOUND_*`, `VTB_REGION_*`, `VTB_DIFF_TBL`) are one
cluster, not eight independent wins — a byte-exact copy via
`tools/tabdump.py` is what they need, and `docs/fastpass.md` defers the
closed-form derivation to the 8 kHz retarget.

## Phase 6 — the remaining pumps and services

In descending exclusive cost: `V32mod.c +39` (46,071), `Dialer.c +18`
(14,602), `voice.c#3` (9,373), `Fdspkrnl.c +13` (7,635), `Beepgen.c +3`
(6,546), `b103.c +2` (4,471), `v32.c`, `dp_init.c`, `v22.c`.

`Beepgen.c` is 3,949 of its 6,546 bytes ready and has no dependants —
a good batch for a session with little context left.

## Phase 7 — last, and deliberately

- **`V90Parameters::loadParams`, 7,894 bytes, needing only two symbols
  (`Vparser_read_float`, `Vparser_read_int`).** It will rank near the top of
  every metric in this document and it should be done **last**: findings
  860–862 show both its callees are three-byte stubs, so the method has no
  observable behaviour, and `tools/vparse.py` already extracts everything it
  encodes.
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

    python3 tools/worklist.py --md docs/worklist.md   # what is left
    python3 tools/readyqueue.py                       # what is startable
    python3 tools/closure.py <members> --batch        # is my batch closed
    python3 tools/coverage.py                         # the headline
