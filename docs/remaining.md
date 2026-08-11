# What is reconstructed, and what is left

*Measured at `f0112ff`, after the construction-path run. Every figure here
comes from a tool in `tools/`; the commands are given so they can be re-run
rather than trusted. Nothing in this document is an estimate.*

## 1. Where it stands

    make coverage

| | |
|---|--:|
| `.text` in the blob | 734,605 bytes / 1,861 symbols |
| translated | **38.8%** — 285,048 bytes / 749 symbols |
| driven against the blob | **100%** of what can be — 285,009 bytes, 740 of 749 |
| `make phase` | exit 0, **1,436 PASS, 0 FAIL** |
| codegen tier | 105 of 386 shared symbols identical on instruction sequence |

The gap between the first two rows is the honest one: 38.8% is how much
exists, and 100% is the share of that which some test compares against the
original rather than merely reading well.

## 2. "Done" means a closed closure, not a percentage

`tools/closure.py <entry> --missing` answers a sharper question than coverage
does: *what does this entry point still need that nobody has written?* Four
datapumps — now five — answer zero.

| datapump | unwritten in its closure |
|---|--:|
| Bell 103 / V.21 | **0** |
| V.23 | **0** |
| V.8 negotiation | **0** |
| call progress / dialler | **0** |
| **V.PCM construction** (`dp_vpcm_init` → `vpcm_create` → `VPCMXF_Create` → `VPcmV34Create`) | **0** |

That last row is what this run bought. It was **108 symbols and 19,704
bytes** when the run started. `dp_vpcm_init` now registers a datapump, builds
a `VPcmFloModem`, both `Modem` halves, the modulators, the demodulator, the
equaliser, the message classes and the 53,848-byte V.34 object — all from this
tree's own code, with the blob used only as the thing it is compared against.

Combined with the V.34 data path (README step 2: two endpoints, both ours,
33,600 bit/s each way, BER 0 over 8,000 blocks), **a V.34 modem is now ours
from registration through to carried data.**

## 3. What is left, and the column that should decide the order

    python3 tools/closure.py <entry points> --missing

`exclusive` is the part **only** that entry point needs — bytes no other
entry point would pay for anyway. It is the number that says what a phase
actually costs, and it is invisible in any coverage percentage.

| entry point | closure | exclusive | symbols |
|---|--:|--:|--:|
| **V.PCM run** (V.34 + V.90 + V.92) | 193,980 | **193,980** | 254 |
| **fax Class 1** | 109,268 | 92,392 | 545 |
| **V.32 / V.32bis** | 59,642 | 42,782 | 202 |
| **V.22 / V.22bis** | 32,738 | 31,815 | 113 |
| **voice** | 20,437 | 16,978 | 72 |
| **Caller ID** | 12,207 | 8,168 | 46 |
| **DTMF detect / generate** | 5,596 | **17** | 27 |
| **beep / DTMF generator** | 2,372 | 469 | 6 |

Three things fall out of that column.

**V.PCM cannot be made cheaper by doing something else first.** Every one of
its 193,980 bytes is exclusive: no other entry point shares a single one. It
is the end goal and it is also the only item on this list that no amount of
sequencing reduces.

**DTMF is 17 bytes of its own.** Its closure is 5,596 bytes, of which 5,579
belong to pumps that are already written or are on the list anyway. It is
effectively free the moment its neighbours land, and scheduling it as a
"phase" would misrepresent it by two orders of magnitude.

**Fax Class 1 is the second largest by closure and by exclusive cost**, and
its 545 symbols are the largest symbol count of anything left — many small
functions rather than a few large ones, which is the shape that parallelises
best.

## 4. Inside the largest item

    python3 tools/closure.py dp_vpcm_init vpcm_create VPCMXF_Create \
        VPcmV34Create vpcm_delete VPcmV34Progress \
        VPcmV34GetCurrentRxBitRate VPcmV34GetCurrentTxBitRate \
        VPcmV34GetCurrentSessionDP --missing

The V.PCM **run** path is 254 symbols / 193,980 bytes, entered through
`VPcmV34Progress` (7,278 bytes, itself unwritten). By class, largest first:

| class | bytes | symbols | writable today |
|---|--:|--:|--:|
| `V90ConstellationDesigner` | 20,432 | 6 | 3 |
| `V90Equalizer` (its processing half) | 18,361 | 9 | 5 |
| free functions and data | 17,491 | 26 | 22 |
| `V90Phase3Demodulator` | 17,322 | 7 | 2 |
| `V90AutoDigitalImpDetector` | 16,712 | 20 | 17 |
| `V90Demodulator` | 9,179 | 6 | 2 |
| `V90Phase4Demodulator` | 7,417 | 11 | 4 |
| `V90CP` | 7,257 | 5 | 4 |
| `V90Phase4Modulator` | 7,128 | 7 | 2 |
| `V92Phase4Modulator` | 6,878 | 19 | 9 |
| `V92ModulusEncoder` | 6,814 | 2 | 2 |
| `V90ConnectionEvaluator` | 6,767 | 9 | **9 — the whole class** |
| *(16 further classes, each under 5,200 B)* | ~32,000 | 108 | most |

Every class here already has its constructor, destructor and object map
written and asserted — that was this run's work. What remains is the
*processing*: the methods that run per block. That is a materially easier
starting position than the constructors were, because the layout is settled
and the fixtures exist.

## 5. The 341 symbols no entry point reaches

    python3 tools/closure.py … --missing     (union of all twelve entry sets)

Of the 1,103 unwritten symbols, **762 (375,705 bytes) are reachable** from one
of the twelve entry points above and **341 (59,372 bytes) are not.**

**They are not dead code, and the label matters.** The largest are
`V90Parameters::loadParams` (7,894), `V92CP::bitsToInfo` (1,957),
`V92Parameters::loadParams` (1,384), `V92CP::evaluateInfo` (1,124),
`RingDetector_Process` (1,045), `VPcmV34GetVisualDiagnostics` (1,023),
`V90Phase4Modulator::setRfSymbols` (1,005),
`GetNextDigitAndReturnNextState` (895), `VPcmV34GetDiagnostics` (821).

That is **exported API surface the host calls directly** rather than through a
datapump — diagnostics, parameter loading, ring detection — plus a
ring-detector entry set not enumerated here. A first pass of this measurement
put 413 symbols in this bucket and included `VOICE_process`, `dtmf_modem` and
`cid_modem`; those are real services whose entry points were simply missing
from the list. **Anything counted here should be checked for a missing entry
point before being called unreachable.**

`V90Parameters::loadParams` is the one genuinely-inert member: findings
860–862 show both its callees are three-byte stubs, so the method has no
observable behaviour, and `tools/vparse.py` already extracts everything it
encodes.

## 6. What these numbers do not cover

- **Byte counts are the blob's, not ours.** They size the reading, not the
  writing.
- **`closure.py` is a LINK closure**, deliberately pessimistic about run-time
  reachability: it lists branches nothing takes. It also cannot see members
  GCC inlined out of existence (finding 64).
- **A C++ header's `/* +0xNNN */` comments are checked by nothing.** Every one
  is in `offcheck.py`'s `SKIP_HEADERS`, and `make offsets` counts the same
  annotations with or without them. What pins a C++ layout is the
  `__builtin_offsetof` typedefs in the `.cpp`. "offsets clean" is not "the
  header was validated".
- **The codegen tier is no evidence for five classes**, not weak evidence: ten
  of fifteen period-toolchain failures are one C++11 construct (`enum X : int`)
  in three headers, which excludes `V90Equalizer`, `V90PreFilter`,
  `VPcmFloModem`, `V90Demodulator` and `V90Phase3Demodulator` — the classes
  this run added most to. Finding 1308.
- **The mutation snapshot is stale tree-wide** (roughly 1 current of 117) by
  the owner's decision; the full sweep is deferred. Stale is not MISSING and
  fails no gate, but no entry should be quoted as a baseline until it is
  re-run.

## 7. The order this suggests

1. **V.PCM run — `VPcmV34Progress` and the receive chain's processing
   methods.** 193,980 bytes, all exclusive, and the project's stated end goal.
   Start where `cl=1` is dense: `V90ConnectionEvaluator` (whole class),
   `V90AutoDigitalImpDetector` (17 of 20), the free functions (22 of 26).
   `V90Demodulator`'s own methods stay last — closure 142 is a hub.
2. **V.90 answer side.** Deliberately last within V.PCM: `VPCMXF_Create`
   derives its side from a null argument its one caller always passes, so the
   digital-side sender is code the blob never enters and cannot be driven
   differentially. Only the codegen tier applies, and §6 bounds what that is
   worth here.
3. **Fax Class 1** — 92,392 exclusive over 545 symbols, the best-shaped
   remaining work for parallel batches.
4. **V.32 / V.32bis**, then **V.22 / V.22bis** — 42,782 and 31,815 exclusive.
5. **voice, Caller ID, ring detect** — 16,978 and 8,168 exclusive.
6. **DTMF and the beep generator** — 17 and 469 exclusive. Take them whenever
   their neighbours land; they are not a phase.

And one piece of maintenance that is owed rather than optional: **the
tree-wide mutation re-record**, once reconstruction stops moving.
