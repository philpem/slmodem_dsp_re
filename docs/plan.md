# A plan for the 330,816 bytes that are left

*Companion to `docs/remaining.md`, which says what is left, and
`make worklist`, which lists it. This says in what order, and why that order
rather than the obvious one.*

*Measured at `ad6c0c1`. **Re-run `tools/readyqueue.py` and `tools/service.py`
before trusting any count here** — the whole point of the ordering is that it
moves as work lands, and every count below moved substantially in a single day.
Seed by REACHABILITY, never by symbol name: a name-matched count was wrong
three separate times in one session (V.34's remainder, V.32's size, and a pad
that turned out to be a double count), each time by enough to change the plan.*

**The priority is complete data mode. Fax Class 1 is last.** That is the
owner's decision and it is what the phase order implements — but it cannot be
executed by deferring a translation-unit span, and §2 is why.

## Where the object stands

The blob is **818,104 bytes**. Reconstructed: **487,288 — 59.6%.**

| | written | left | symbols left |
|---|--:|--:|--:|
| code | 418,677 | **295,147** | 867 |
| data | 68,611 | **35,669** | 409 |

`docs/coverage.md` reports the same tree as 57.9% on a code-only basis. Both
numbers are correct; quote which one you mean.

## 1. The three facts that decide the order

### Most of what is left cannot be started yet

    tools/readyqueue.py

| | symbols | bytes |
|---|--:|--:|
| **READY** — closure needs nothing unwritten but itself | 426 | **85,368** |
| **BLOCKED** — needs one or more unwritten symbols first | 441 | 209,779 |

A batch has to be **closed** before it can be committed: every dependency of
every member is in the set or already written. This is not a style preference.
`symmap.py` renames every symbol the blob defines to `ref_*`, so one unwritten
callee fails *all* the differential binaries at `t_encode`, not just its own.
Finding 215 declined the escape hatch and that ruling stands.

### A few hundred bytes gate tens of thousands

Sorting every unwritten symbol by the total size of the blocked work whose
closure contains it — that is, what it is a NECESSARY condition for:

| cost | necessary for | symbol |
|--:|--:|---|
| **4 B** | 47,516 B | `avg_err_show.0` |
| 2,286 B | 45,385 B | `FPM_SRE_recover` |
| 2,131 B | 45,385 B | `FPM_FSE_receive` |
| 753 B | 42,889 B | `FPM_PPS_filter` |
| **61 B** | 40,291 B | `FPM_TONE_kill` |
| 574 B | 38,542 B | `FPM_SRE_init` |
| 417 B | 30,740 B | `V90SignBitsExtractor::process` |
| 660 B | 30,332 B | `V90Demapper::hardDecision` |
| 408 B | 30,332 B | `V90Demapper::process` |
| 139 B | 30,654 B | `V90Demapper::resetLinearMappStudy` |

**Necessary, not sufficient** — writing `FPM_TONE_kill` does not make 40 KB
ready by itself. But nothing in that 40 KB can be closed without it, so it is
where the order starts. This is the `V90ConstellationPower` shape (1,408 bytes
unlocked 8,869) an order of magnitude larger.

### The unreachable bucket is not dead code

`service.py` reports 265 symbols / 41,143 bytes that no entry point reaches.
**254 of those simply have no DIRECT caller**: they are vtable slots and
dispatch-table targets — `VOICE_process`, `FAX_process`, `V92CP::bitsToInfo`,
both V.34 diagnostics. A direct-call walk cannot see them. Treat the bucket as
real work whose *ordering* is unknown, not as work that can be skipped.

(`tools/indirect.py` is the tool for this and it currently **crashes** on the
`tools/dis.py` shadow — `AttributeError: module 'dis' has no attribute
'COMPILER_FLAG_NAMES'`. The fix is at the top of `tools/whichfield.py` and has
not been applied here. Fixing it is a prerequisite for planning this bucket
properly.)

## 2. The trap in "leave fax until last"

Fax and data mode are not separate translation-unit spans. Thirteen symbols
inside what looks like the fax span are shared DSP that data mode reaches —
`FPM_ECC_cancel` is the V.32 echo canceller. **Partitioning by TU span puts
shared DSP on the wrong side.** `tools/service.py` seeds each service from its
own entry points and follows reachability; that partition is the authority, and
it carries `MUST_BE_FAX`/`MUST_BE_DATA` sanity lists that exit non-zero if it
breaks.

So "fax last" means *fax-only symbols last* — 286 symbols, 78,718 bytes that
nothing in data mode reaches. Anything data mode needs is data-mode work
whatever file it lives in.

| partition | symbols | bytes |
|---|--:|--:|
| **Data mode** V.90/V.92/V.34/V.32/V.22/B.103/V.23/V.8 | 246 | 150,819 |
| Fax only | 286 | 78,718 |
| Voice / Caller ID / ring detect | 70 | 24,467 |
| No direct caller (vtable / dispatch) | 265 | 41,143 |

## 3. Naming: do it INSIDE the batch that owns the struct

This is a decision, not a preference, and it comes from two measurements that
point in opposite directions.

**Renaming concurrently with reconstruction is dangerous.** Finding 3511: three
branches off one commit, **no shared source file and no git conflict**, merged
cleanly and did not compile. One had renamed `struct v22_fse`'s fields on the
evidence of the receive loop; another had written compile-time assertions
against the old names. Each diff was correct against its own base. It failed
loudly only because the reference was a `__builtin_offsetof`; through a
`void *` and an offset constant it would have linked and been silently wrong.

**But deferring the naming loses the evidence.** The strongest evidence for
what a field means is a format string that prints it, and those surface while
reconstructing the function that does the printing. The V.34 accessor batch
produced `f06`→`preemp`, `f25c`→`dmadelay` and `flags_0217`→`v34BaudAllow` as a
by-product — `v34BaudAllow` named by `chkForceBaudRate` *indexing* it 0..5, not
by its five writers. None of that was available to a later pass without redoing
the work. Finding 3303 makes the same point negatively: `v34_shell::pad_000`
looked like 2,560 bytes of opportunity and was a double count of a region
`v34_object` already models — only the batch that knows the struct could tell.

**So: the batch that reconstructs a struct's users names that struct's fields
and flags, in the same branch and the same compile. A standalone naming pass is
run ONLY against a struct no live batch touches** — which is what made
`V90Phase4Modulator` (12,064 bytes, finding 3120) safe: nobody else was writing
against it. Never schedule a naming agent and a reconstruction agent over the
same header.

Naming is free at the codegen tier, so it costs the batch nothing to carry:
`compare.py` did not move by one symbol across 3120's rename. **If it does
move, a TYPE changed, not a name** — investigate rather than accept it.

What is outstanding, and shrinking:

| | remaining |
|---|--:|
| unmodelled struct space | 90 `pad_*` regions, **10,142 bytes** |
| offset-named fields (`short_2800`, `flags_0217`) | 304 distinct |
| bare `fNNNN` names | 189 distinct |
| unnamed single-bit flags | 148 uses |
| type-punned sites — provably mis-modelled | 27 |

Flags are named **by bit value**, never converted to bitfields; CLAUDE.md's
"Naming: fields, and flags" carries the rule and the measurement behind it.

The 27 punned sites are the exception to "name inside the batch": each is a
*provable* modelling error (`*(int *)&o->f25d0` writes four bytes through a
narrower field), they cluster in six files, and they can be corrected as one
standalone batch — but only while no other batch holds those files.

## Phase 0 — landed

`V90ConstellationPower` 10/10 · `V90ConstellationDesigner` 24/24 · the V.34
public accessor surface (21 symbols) · V.32's Viterbi trellis decoder, four
slicers and 13 `DECv32_*` tables (39 symbols) · V.22's equaliser, both slicers,
`V22_FSE_receive` and the datapump object fully tiled (12 symbols) ·
`V90Phase4Modulator`'s 12,064-byte pad named out.

## Phase 1 — the shared DSP keystone

`FPM_SRE_recover`, `FPM_FSE_receive`, `FPM_PPS_filter`, `FPM_SRE_init`,
`FPM_TONE_kill`, and `avg_err_show.0`. **About 6 KB that is a precondition for
about 45 KB**, shared by V.32, V.22 and the V.90 family at once. Nothing else
in the plan has this ratio. Do it first even though it is not the largest
batch, and do it as ONE batch because the closure interlocks.

## Phase 2 — the V.90 demapper cluster  ✅ WRITTEN, on `v90-demapper`

`V90Demapper::hardDecision`, `::process`, `::resetLinearMappStudy`, and
`V90SignBitsExtractor::process`. ~1.6 KB gating ~30 KB of the V.90/V.92 receive
chain. Second-best ratio in the object.

**Written 2026-08-16 on `v90-demapper`, not yet merged.** All four, plus
`V90SignBitsExtractor::applyFrameAction` (197 B), which is not in the batch and
is what `process` calls: the object holds its four arms twice, once as its own
symbol and once inlined (finding 3532), and the period compiler reproduces both
— our `applyFrameAction` is the same 197 bytes with the same mnemonic sequence,
and our `process` is 418 against the blob's 417 with no out-of-line call.
`make phase` green, `compare.py --ratchet` 986→991 compared and 350→351
identical. 1,771 bytes; coverage 57.9% → 58.1%.

Four type corrections came with it and are the reason to read findings 3530 and
3533 before touching this class: two one-byte "flags" are
`SerialDifferentialDecoder<unsigned char>` members and the two heap blocks stop
being `void *`. The naming was carried inside the batch per §3.

**WHAT IT ACTUALLY UNBLOCKED, measured rather than projected**, by running
`readyqueue.py` at `781aff9` and again after:

| | before | after |
|---|--:|--:|
| unwritten call symbols | 867 | 862 |
| READY | 426 / 85,368 B | 423 / 84,277 B |
| BLOCKED | 441 / 209,779 B | 439 / 209,049 B |

**Exactly one symbol became READY: `V90Demodulator::enterDataPhase`, 322 bytes**
— it was blocked by `resetLinearMappStudy` and by nothing else, so 139 bytes
freed 322. The "~30 KB" in the heading is what these four are a NECESSARY
condition for, which §1 already warns is not the same as sufficient, and the
gap is the four `V90Demapper` members still outstanding: `reset`,
`resetNoSpectral`, `linearMappingStudy` and `incrementRBSFramePosition`. What
moved instead is how far the hubs have left to go:

| | before | after |
|---|--:|--:|
| `V90Equalizer::process` (9,364 B) | needs 20 | **needs 16** |
| `V90Demodulator::progress` (7,276 B) | needs 65 | **needs 61** |
| `V90Phase4Demodulator::getV90Decision` (3,095 B) | needs 7 | **needs 3** |
| `V90Phase4Demodulator::getV92Decision` (3,252 B) | needs 12 | **needs 8** |

So the natural next batch is the rest of `V90Demapper` — those four plus
`updateConstelation` — which every one of the four rows above is waiting on.

## Phase 3 — the large ready set

Needs nothing, and 13 KB between them: `V90TRN2Design` (3,767 B),
`V90CP::infoToBits` (2,785), `V92setParamsInfoFromCPUnPck` (2,695),
`V92Transmitter::reset` (2,161), `V90CP::evaluateInfo` (1,986),
`V90SpectralVerifier::checkSpecialSpectralConditions` (1,682). Parallelisable —
they share no closure.

## Phase 4 — V.32's state machine

The 103-symbol mutually-recursive cluster: `V32*NextState`, `RxHdx*`/`TxHdx*`,
`V32FP_*`. **One batch, not several** — the recursion means no proper subset
closes. V.32 is 41.5% written; this is most of the remainder.

## Phase 5 — V.22's remainder

`DemodDataV22` (510 B) is newly READY and was the gate on six functions. What
now blocks the rest is mostly **not V.22-named**: `connect_1200`,
`connect_2400`, `Detect_1s`, `MakeTxData`, `SetRxRate`, `SetTxRate`, `ResetRx`,
`Detect_Retrain`, `Detect_Rmloop2_ACK`. Seed by reachability or this phase
looks smaller than it is.

## Phase 6 — the type-punned sites

27 sites, six files, each provably wrong today. Independent of every other
phase; schedule it when those six files are free.

## Phase 7 — the data-mode API and the two diagnostics

`VPcmV34GetDiagnostics` needs `V90Demodulator::getAT_UD` (418 B);
`VPcmV34GetVisualDiagnostics` needs nine, including
`VPcmFloModem::getConstellation` (469), `::getLinearEqualizer` (155),
`::getDFE` (138). `TAG_DiagnosticResults` is unmodelled and runs to at least
`0x22c`; no allocation site bounds it, so its tail needs declaring the way
`v34_object`'s did. `getAT_UD` carries `"RBS : %d (%d%d%d%d%d%d)"`, which names
a six-bit field — these are naming oracles as well as functions.

## Phase 8 — dialler, call progress, and the vtable bucket

Includes the 254 no-direct-caller symbols. Fix `tools/indirect.py` first.

## Phase 9 — voice, Caller ID, ring detect

70 symbols, 24,467 bytes.

## Phase 10 — fax Class 1, last

286 symbols, 78,718 bytes that nothing in data mode reaches.

## Continuous, not a phase

- **The mutation snapshot is stale** — 131 suites registered, 0 current after
  the last merges. Re-record needs a quiet `src/`.
- **25 diagnostic format strings are genuinely absent** across 18 functions,
  with 13 call sites unresolvable either way. `debugaudit.py --absent` is the
  measure; the per-file "missing" rollup is NOT, and reads ~15× worse than the
  truth because it debits one file and credits another whenever a factored
  helper lands elsewhere (findings 2600, 2950).
- **Finding 3215**: `make phase`'s `prereq` is a prerequisite rather than a
  barrier, so under `-j` it races the interop link. Fails loud. Unfixed only
  because the Makefile was contended.

## What every batch owes, whatever phase it is in

- A closed batch, and `make phase` green — **not `make test`**.
- Its own differential suite, and mutations that are shown to fire. A
  separating-trial counter must count trials that differ in an **observable**
  result; four of five such counters in one batch were measuring a path or a
  constant and proved nothing (findings 3509, 3403). **The mutation
  adjudicates, not the counter.**
- Findings and deviations numbered from a survey across **all branches** at
  commit time — and the merger re-checks, because two agents surveying in the
  same window both claimed D351.
- `refcheck.py` clean. It cannot see other branches, so it is necessary and not
  sufficient.
- Naming carried inside the batch, per §3.

## Re-deriving all of this

    make coverage                  # MUST run first -- a plain `make` no longer
                                   # fills build/src, and seven tools read it
    python3 tools/service.py       # the reachability partition
    python3 tools/readyqueue.py    # READY vs BLOCKED
    python3 tools/worklist.py      # the per-symbol list
    BLOB=<abs> tools/toolchain/compare.py --ratchet

Every one of those refuses on a zero denominator now (findings 3110, 3122).
If one refuses, it is telling you the truth: run `make coverage`.
