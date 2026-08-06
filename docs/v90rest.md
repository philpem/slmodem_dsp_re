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

    245        closure.py, the missing addend            landed
    246-250    w1a_leaves                                246-249 used
    251-254    w1b_adi                                   251-253 used
    255-257    w1c_p2info                                all used
    258-261    w1d_equ                                   258-260 used
    262-266    w1e_dil                                   262-264 used
    267-272    the setSessionFlag chain                  267-271 used
    273-278    VPcmFloModem                              in flight
    279-284    k56FlexPhase34                            279-283 used
    285-290    the v34handshak step fixture              all used, + D59, D60
    291-296    V90Phase3Demodulator::reset               in flight
    297-320    #59's remaining five

VERIFY BEFORE TAKING A BLOCK, do not trust this table:

    grep -oE '^### [0-9]+\.' docs/findings.md | grep -oE '[0-9]+' | sort -n | tail -20

250, 254, 261, 265, 266, 272, 284 are gaps that will stay gaps. A gap is
cheaper than a collision, and this history has had six of the latter.

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

## Where wave 3 got to

Five of `VPcmFloModem`'s six landed: `getUinfoValue` 773, `setPhaseIIinfo` 704,
`getV90CpBits` 417, `getV90JaBits` 158, `setPcmSessionType` 98 -- 2,150 bytes,
findings 273-278, mutation suite `vpcmflomodem` (48 of 49 caught, 1 recorded
equivalent and measured over 123 million values).

`VPcmFloModem::enterPhase3` (270 B) is the sixth and is still outstanding.
**It does call `DILdescriptorPacker`, exactly as this file said** -- that is
in the object's own relocations and is not in doubt:

    $ tools/dis.py ../slmodemd/dsplibs.o _ZN12VPcmFloModem11enterPhase3Ev \
        | grep -o 'R_386_[A-Z0-9]* .*' | sort -u
    R_386_PC32 DILdescriptorPacker
    R_386_PC32 _ZN14V90Demodulator11enterPhase3Ev
    R_386_PC32 edprintf
    R_386_PC32 dsplibs_debug_printf
    ...

What has changed is that `DILdescriptorPacker` is written, so it is no longer
a BLOCKER. `tools/closure.py --missing` reports 1,590 bytes still outstanding
and every one of them belongs to wave 2 or to a weak symbol:

    801  V90Phase3Demodulator::reset(PcmType, unsigned char, ...)
    448  V90Demodulator::enterPhase3()
    270  VPcmFloModem::enterPhase3()
     48  Descrambler<int,int>::reset(int)              (weak, template)
     23  Descrambler<int,int>::resetHistoryIndexes()   (weak, template)

Only `V90Demodulator::enterPhase3` is a direct callee; the other four arrive
through it. **`--missing` is not a call list** -- it filters out everything
already written, so reading it as one is how "it needs DILdescriptorPacker"
would have been contradicted on no evidence.

So: it is sequenced behind **wave 2's two remaining symbols**, not behind the
C prerequisite, and behind the two `Descrambler<int,int>` members that
finding 231 and the note at the top of this file both warn `callgraph.py`
cannot see and that nothing in `include/` defines yet. It is one 270-byte
method behind 1,320 bytes of somebody else's class, so it goes with them
rather than on its own.

Whoever writes it should read finding 273 (the object
is genuinely 32 KB, and the old "indexes through `this`" sentence in
docs/v90cpp.md is corrected), finding 274 (a `V90Modem` is EMBEDDED at
+0x1758, and `sizeof(V90Modem) == 0x49c0` is asserted in VPcmFloModem.cpp for
exactly that reason), and finding 275 (+0x1760 is a `V90Phase2Info` and
+0x612c a `V92Phase2Info`). `include/dsplib/VPcmFloModem.h` already carries the
class; add fields to it rather than starting a new map, and put anything new
in the mutation suite that is already registered.

`v90Phase34` (1,358 B, #59) is unblocked by this: its two blockers were
`getV90CpBits` and `getV90JaBits`. `V34SetINFO1aBits` (1,401 B) is unblocked
too. `VPcmV34InitiateRetrain` still wants `V34DisconnectThreshTable`.

## `v90Phase34` has landed

1,358 bytes, findings 301-306, mutation suite `v90p34` -- 70 mutations, 64
caught, 6 recorded equivalent and **no gaps**, which is what separates it from
its twin: `k56FlexPhase34` left eight uncaught in two arms nothing could
enter, and every arm of this one is reachable because its two bit sources are
real bodies rather than stubs.

It is in `src/pump/v34/v34pcmmain.cpp`, not a file of its own: the call to
`getMPrecvdBits` at .text+0x9fea carries no relocation, which in a non-PIC
object means the same translation unit, and that file IS VPcmV34Main.cpp's
C++ half. Test `test/unit/t_v90p34.cpp`; declaration beside
`k56FlexPhase34`'s in `v34hshak.h`.

Its two diagnostics named four fields the tree did not have -- `period` at
`V34_RATECFG + 2`, bit 2 of `pac3c + 0x50`, the byte at +0xabfe, and
`tx->symcnt` for `f25c0` -- and confirmed `v90_receiver` as the object's own
name for +0x24c. All four are now in `v34fsk.h`; finding 302 is the record.

#59's remaining four: `V34SetINFO1aBits` (1,401), `VPcmV34InitiateRetrain`
(1,406, still wanting `V34DisconnectThreshTable`), `V34GiveINFO1dBits` (436)
and `indicateJaTransmission` (57). Findings from 307.
## Where wave 2 got to

All seven symbols. The `setSessionFlag` chain landed first (findings 267-269);
`V90Phase3Demodulator::reset` (801) and `V90Demodulator::enterPhase3` (448)
followed, with `Descrambler<int,int>` -- the one thing `closure.py` reported
missing that really was -- added to `include/dsplib/Scrambler.h` beside the
scrambler it inverts. Findings 291-296.

`V90SessionFlag.h`'s five classes are now three: `V90Phase3Demodulator.h` and
`V90Demodulator.h` carry the two the batch gave real weight to, which is the
split that file asked for. **Both assert their size**, which it said could not
be done -- the displacement scan finding 268 warned about is still wrong, and
the `sysdep_malloc` before the constructor call is exact. 0x42c and 0x298.

Mutation suites `v90p3dreset` (29, 28 caught) and `v90demod` (30, 28 caught);
the three uncaught are all named with what is held fixed, and one of them
corrected a wrong sentence in the source rather than exposing a test gap
(finding 293).

**`tools/closure.py _ZN12VPcmFloModem11enterPhase3Ev --missing` is now one
symbol: itself.** Wave 3 is unblocked with nothing in front of it; finding 296
is the hand-over.

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

## Done: the stub `V90Phase2Info` is gone

`V90PreFilter.h` includes the real header instead of declaring a 0x1c-byte
union; `V90Phase2Info.h` has dropped its `#error`; `autoSelection` reads
`phase2->L2` rather than punning `*(const float *const *)&phase2->b[0x18]`;
`t_v90prefilter` has a `P2()` accessor and sizes its Phase 2 comparison with
`sizeof(V90Phase2Info)`. The stub `class V90Parameters` stays -- that one is
still unmodelled.

It bought hygiene, not coverage, and finding 264 says so: the comparison grew
from 0x1c to 0x24 but the eight new bytes are memory neither side touches.
What it did surface is a wrong sentence that two declarations of one class had
kept alive -- `autoSelection` reads entry 14 and entries 15..20 of `L2`, not
"its first six entries" -- and correcting it turns into evidence, because
`printInfo` and `autoSelection` then independently stop at index 20 from two
translation units. `test/mutations/v90prefilter.json` was added at the same
time; all four of its mutations are caught.

## Still not started, and deliberately

#56-#58 (`v34handshak`, 61,541 bytes). #63 came first and is **done**: the
per-dispatch-case harness writes the state word directly (`obj+0x3592`
microstate, `+0x3594` rxstate, `+0x3596` txstate) and steps once.
`docs/v34handshak.md` is its manual and findings 285-290 are the record.

It unblocks #57, which is the 27.6 KB and the sixteen units. It does NOT
unblock #56: the per-sample transmit route's result is not a function of the
object (finding 289, D60), and finding out what it reads is that task's first
job rather than the harness's.

**Findings 285-290 were taken out of the 279-300 block this file used to
reserve for #59's six.** The block table above has been rewritten to match
what is actually in `docs/findings.md`: 291-296 went to
`V90Phase3Demodulator::reset`, so **#59's remaining five start at 297**, not
at 291.
