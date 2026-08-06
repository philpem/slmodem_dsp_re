# Finishing #60, and what #59 is actually waiting for

A working checkpoint for the tail of the V.90/V.92 work. `docs/v90cpp.md` is
the batch-0-to-3 record and is still right about everything it describes; this
file is batches 4 and 5 and the six C functions behind them, re-planned against
closure numbers that can be trusted.

## Every closure below was computed after finding 245

`tools/closure.py` used to read the addend of a section-relative relocation as
zero, because ELF32 REL has no `r_addend` and `readelf -r` prints no column for
it. That resolved 3,044 relocations to whatever symbol sits at offset 0 of
`.text`, `.data`, `.bss` or `.rodata`, and it both **invented** a six-symbol
phantom cluster in every closure and **hid** real edges to file-local tables.
Fixed edges gained: 223 distinct targets. So:

**Any closure number in `docs/v90cpp.md`, in a task description, or in a
hand-over written before this session is stale in both directions.** Recompute.

The one thing the tool still cannot know: a `.gnu.linkonce.t.*` template member
is reported missing even when a header here defines the template and GCC
inlines it, so `build/src/**/*.o` never references it. `Scrambler<unsigned
char,int>` is the worked example. `Descrambler<int,int>` is **not** — nothing
in `include/` defines it yet, and `V90Phase3Demodulator::reset` needs
`reset(int)` and `resetHistoryIndexes()` from it.

## The corrected size of the job

`callgraph.py` was blind to C++ until this branch's predecessor fixed it: its
"have" set came from grepping `src/**/*.c` for `^name(`, which cannot match
`Class::method(`. Every C++ function already written counted as missing.

    #60 remaining   30 symbols, 6,310 bytes of C++   (batches 4 and 5)
    #59 remaining    6 symbols, 5,379 bytes of C
    prerequisite     1 symbol,  3,278 bytes of C     DILdescriptorPacker
    prerequisite     1 symbol,     32 bytes of data  V34DisconnectThreshTable

The two prerequisites are in **neither** task. They are named here so that
scope is not quietly widened and not quietly missed.

## Ownership is by class, not by closed batch

A closed batch and a source file are different units. `V90Phase3Demodulator`
has one symbol in the `setSessionFlag` chain and another in the `reset` chain;
splitting it by closure would have two worktrees creating the same header, the
same `.cpp` and the same object-size derivation. **One class, one owner.**

| wave | worktree | classes owned | symbols | bytes |
|---|---|---|--:|--:|
| 1 | `w1a_leaves` | `V90ConstellationDesigner` `V90SdDetector` `V90SpectralVerifier` `V92EchoCanceller` `ResamplerTimingOffset` `V90Phase4Modulator` `K56FlexFloModem` | 10 | 261 |
| 1 | `w1b_adi` | `V90AutoDigitalImpDetector`, free `calculateDilLength` | 3 | 810 |
| 1 | `w1c_p2info` | `V90Phase2Info` | 1 | 508 |
| 1 | `w1d_equ` | `V90Equalizer` | 3 | 788 |
| 1 | `w1e_dil` | `DILdescriptorPacker` (prerequisite, C) | 1 | 3,278 |
| 2 | | `V90Modem` `V90Modulator` `V90Demodulator` `V90Phase3Demodulator` `V90Phase4Demodulator` | 7 | 1,523 |
| 3 | | `VPcmFloModem` | 6 | 2,420 |

Wave 2 is one agent because the `setSessionFlag` chain entangles six classes:
`V90Modem::setSessionFlag` calls `V90Demodulator`'s and `V90Modulator`'s, which
call `V90Phase3Demodulator`'s, `V90Phase4Demodulator`'s and
`V90Phase4Modulator`'s. Only the last of those is somebody else's class, and it
is a leaf, so wave 1 can own it outright.

Wave 2 also carries `V90Phase3Demodulator::reset` (801) and
`V90Demodulator::enterPhase3` (448); their whole closure is wave 1.

Wave 3's `VPcmFloModem::enterPhase3` (270) is the only symbol in #60 that
additionally needs `DILdescriptorPacker`. It goes last, so a shortfall there
costs one 270-byte method rather than the batch.

## Findings numbers allocated

    245        closure.py, the missing addend            (landed)
    246-250    w1a_leaves
    251-254    w1b_adi
    255-257    w1c_p2info
    258-261    w1d_equ
    262-266    w1e_dil
    267-272    wave 2
    273-278    wave 3
    279-300    #59's six

238-244 are deliberately unused: `master` was at 237 and a block from 245 was
already promised elsewhere. A gap is cheaper than a seventh collision.

## What #59's six are actually waiting for

Recomputed with the fixed tool. Nothing here is waiting on `v34handshak`;
`datapumpv34` (1,028) is, which is why it is not in this set.

| bytes | function | blocked on |
|--:|---|---|
| 721 | `k56FlexPhase34` | `K56FlexFloModem::getK56FlexJaBits`, `::getK56FlexMpBits` — **wave 1 only** |
| 1,358 | `v90Phase34` | `VPcmFloModem::getV90CpBits`, `::getV90JaBits` — wave 3 |
| 1,401 | `V34SetINFO1aBits` | `VPcmFloModem::getUinfoValue`, `::setPhaseIIinfo`, the `setSessionFlag` chain |
| 1,406 | `VPcmV34InitiateRetrain` | the `setSessionFlag` chain, `VPcmFloModem::setPcmSessionType`, `V90ConstellationDesigner::setMinMaxRates`, `V92EchoCanceller::setEchoDelay`, `K56FlexFloModem::setMinMaxRates`, **and `V34DisconnectThreshTable` (32 B of data, unwritten)** |
| 436 | `V34GiveINFO1dBits` | all of `VPcmV34InitiateRetrain`'s closure, plus itself |
| 57 | `indicateJaTransmission` | everything above plus `DILdescriptorPacker` — it is 57 bytes and it is last |

`k56FlexPhase34` is the one that unblocks earliest: four one-byte K56Flex stubs
and it is free.

## Where wave 1 got to

All five landed and are merged into `v90rest`; `make phase` is green and
coverage went 19.2% -> 19.9% (334 -> 351 symbols, 140,154 -> 145,291 bytes).

| worktree | what landed | findings |
|---|---|---|
| `w1a_leaves` | all ten leaf symbols, seven classes | 246-249 |
| `w1b_adi` | `V90AutoDigitalImpDetector` x2 + `calculateDilLength` | 251-253 |
| `w1c_p2info` | `V90Phase2Info::printInfo` | 255-257 |
| `w1d_equ` | `V90Equalizer` x3 | 258-260 |
| `w1e_dil` | `DILdescriptorPacker` (the prerequisite) | 262-263 |

Mutation suites now registered: `v90adid`, `v90dil`, `dilpack`, `v90equ`,
`v92ec`, `v90rto`, `v90cd`. **#60 is 30 symbols short of done minus these**;
what is left of it is wave 2 (the `setSessionFlag` spine, 7 symbols, 1,523 B)
and wave 3 (`VPcmFloModem`, 6 symbols, 2,420 B), both unblocked now.

Three things wave 1 cost that the next batch should not pay again:

- a fresh worktree has no `third_party/spandsp`, and the failure names
  spandsp rather than the worktree (finding 257);
- a new C++ class header must go in `SKIP_HEADERS` in `tools/offcheck.py`, or
  the offsets gate reports every annotation in the tree as wrong; and its
  `offsetof` assertions must be guarded on
  `__SIZEOF_POINTER__ == 4` or `check64` fails;
- an anti-vacuity check derived from the wrong quantity fails loudly and looks
  like a broken reconstruction. Two of them did (findings 247, 262). Both were
  the test's arithmetic, not the function's.

## Owed once wave 1 is merged: delete the stub `V90Phase2Info`

`include/dsplib/V90PreFilter.h` defines its own `class V90Phase2Info` -- an
opaque `union { unsigned char b[0x1c]; int w[7]; float f[7]; }` sized by
`V90PHASE2INFO_BOUND`, which is a bound from the one offset `autoSelection`
reaches, not a measurement. The real class landed with finding 255 and is
0x24 with named fields. Two definitions of one class is the drift risk the
`#error` in `V90Phase2Info.h` currently guards; the guard is not the fix.

The change is mechanical and every piece of it has been checked:

1. `V90PreFilter.h`: delete the stub and `V90PHASE2INFO_BOUND`, `#include
   "dsplib/V90Phase2Info.h"`. The stub `class V90Parameters` stays -- the real
   header only forward-declares it, and a forward declaration coexists with a
   definition.
2. `V90Phase2Info.h`: drop the `#ifdef DSPLIB_V90PREFILTER_H` `#error`.
3. `V90PreFilter.cpp:120`:
   `*(const float *const *)&phase2->b[0x18]` becomes `phase2->L2`. The pun and
   the named field agree -- the object loads a pointer from +0x18 and
   dereferences six floats, which is what both spellings do.
4. `t_v90prefilter.cpp`: `*(void **)&ph2[side][0x18]` becomes
   `((V90Phase2Info *)ph2[side])->L2`; `snap_ph2` neutralises `L2` by name
   rather than by `w[0x18 / 4]`; the three `V90PHASE2INFO_BOUND` uses become
   `sizeof(V90Phase2Info)`, and `PH2SLOT` with them.

**What it does NOT do is add coverage, and saying so matters.** The compare
grows from 0x1c to 0x24, but `fill()` seeds both sides identically and none of
the five `V90PreFilter` methods writes +0x1c..0x23, so the eight new bytes are
memory neither side touches -- findings 223 and 224 exactly. The win is one
definition instead of two, and named fields instead of `->b[0x18]`.

The one thing to keep: `snap_ph2` replaces the pointer word with a boolean
"does it still point at `meas[side]`", because the two sides hold two
different addresses there and always will. `params` at +0x20 needs no such
treatment -- `setup()` never writes it, so both sides keep the same fill.

## Still not started, and deliberately

#56-#58 (`v34handshak`, 61,541 bytes). #63 comes first: a per-dispatch-case
harness writing the state word directly (`obj+0x3592` microstate, `+0x3594`
rxstate, `+0x3596` txstate) and stepping once, which turns 27.6 KB into 16
independently committable units. Starting the machines without it repeats the
shape finding 220 measured.
