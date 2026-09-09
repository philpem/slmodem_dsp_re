# Finishing #60, and what #59 is actually waiting for

A working checkpoint for the tail of the V.90/V.92 work. `docs/v90cpp.md` is
the batch-0-to-3 record and is still right about everything it describes; this
file is batches 4 and 5 and the six C functions behind them, re-planned against
closure numbers that can be trusted.

## Every closure below was computed after finding F330

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

    330        closure.py, the missing addend            landed, was 245
    331        w1a_leaves' ten leaf symbols              landed, was 246
    247-250    w1a_leaves                                247-249 used
    251-254    w1b_adi                                   251-253 used
    255-257    w1c_p2info                                all used
    258-261    w1d_equ                                   258-260 used
    262-266    w1e_dil                                   262-264 used
    267-272    the setSessionFlag chain                  267-271 used
    273-278    VPcmFloModem                              in flight
    279-284    k56FlexPhase34                            279-283 used
    285-290    the v34handshak step fixture              all used, + D59, D60
    291-296    V90Phase3Demodulator::reset               in flight
    297-300    VPcmFloModem::enterPhase3, which finishes #60
    301-306    v90Phase34
    307-312    V34SetINFO1aBits
    313-318    VPcmV34InitiateRetrain
    319-324    the harness's table 1
    325        the CFG_FLAGS macro collision              landed
    330-331    renumbered out of master's way             landed
    332-337    V34GiveINFO1dBits
    340-349    v34handshak table 1's six small targets
    350-359    v34handshak table 3's 62, 79, 80 + shared arm
    360-369    v34handshak table 2's seven targets
    370-379    v34handshak table 3's middle group
    380-389    indicateJaTransmission                    380-381 used
    390-399    v34handshak microstate 41
    400-409    v34handshak microstate 44
    410-419    v34handshak microstate 46
    420-429    v34handshak table 1's remaining thirteen
    430+       datapumpv34, and whatever v34handshak still halts on

TEN WIDE, NOT SIX, and all ten belong to the batch. 346 was taken out of the
middle of a ten-block whose owner was still running and had used all ten;
finding F346 records that, and it was made by the same person who wrote 330.
    291-296    V90Phase3Demodulator::reset               all used
    297-300    VPcmFloModem::enterPhase3                 all used
    301-320    #59's remaining five
    297-318    #59's remaining five
    319-324    w3_tbl1, table 1 and the fixture's arena     all used, + D61

**245 AND 246 WERE CLAIMED TWICE AND THIS BRANCH'S TWO MOVED.** `master`
used 238 through 246 while this branch was out -- straight through the gap
this file told the session to leave, and two past it. Finding F330 has the
detail. So:

VERIFY AGAINST `origin/master`, NOT AGAINST THIS TABLE AND NOT AGAINST A
HAND-OVER:

    git fetch origin
    git show origin/master:docs/findings.md |
        grep -oE '^### [0-9]+' | grep -oE '[0-9]+' | sort -n | tail -1

Then take a block above BOTH that and the local maximum below. A gap is a bet
on how fast the trunk moves, and this one lost by two.

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
findings F273-278, mutation suite `vpcmflomodem` (48 of 49 caught, 1 recorded
equivalent and measured over 123 million values).

`VPcmFloModem::enterPhase3` (270 B) was the sixth and **has now landed**;
findings F297-300, mutation suite `vpcmep3` (22 mutations, 21 caught, 1
recorded equivalent and shown dead in the object as well as in the source).
**That closes #60: 30 of 30 symbols, 6,310 bytes of C++.** What the batch
settled beyond the method itself:

- `pad_0004` is a `tagV90DILdescriptor` EMBEDDED at +0x004, from three `lea`s
  off `this` before the packer call, and the type from
  `sizeof(tagV90DILdescriptor) == 0x213` landing exactly on +0x217. Finding
  F297, and it also notes that `bitVector`'s 2,700 entries are exactly enough
  for the packer's 2,654-bit worst case.
- `pad_173a[3]` and the first byte of `pad_173e` are now `flags_173a[3]` and
  `flag_173e`, and `flags_0217` is known to be six bytes rather than five
  because the two writers disagree about the sixth. Finding F298.
- `test/harness/v90demfix.h` is the shared demodulator fixture finding F296
  asked for; `t_v90demod.cpp` was converted to it and its mutation suite
  re-run unchanged (30, 28 caught, same two by name). `t_v90p3dreset.cpp` was
  deliberately left alone. Finding F300.

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
finding F231 and the note at the top of this file both warn `callgraph.py`
cannot see and that nothing in `include/` defines yet. It is one 270-byte
method behind 1,320 bytes of somebody else's class, so it goes with them
rather than on its own.

Whoever writes it should read finding F273 (the object
is genuinely 32 KB, and the old "indexes through `this`" sentence in
docs/v90cpp.md is corrected), finding F274 (a `V90Modem` is EMBEDDED at
+0x1758, and `sizeof(V90Modem) == 0x49c0` is asserted in VpcmFloModem.cpp for
exactly that reason), and finding F275 (+0x1760 is a `V90Phase2Info` and
+0x612c a `V92Phase2Info`). `include/dsplib/VPcmFloModem.h` already carries the
class; add fields to it rather than starting a new map, and put anything new
in the mutation suite that is already registered.

`v90Phase34` (1,358 B, #59) is unblocked by this: its two blockers were
`getV90CpBits` and `getV90JaBits`. `V34SetINFO1aBits` (1,401 B) is unblocked
too. `VPcmV34InitiateRetrain` still wants `V34DisconnectThreshTable`.

## `v90Phase34` has landed

1,358 bytes, findings F301-306, mutation suite `v90p34` -- 70 mutations, 64
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
name for +0x24c. All four are now in `v34fsk.h`; finding F302 is the record.

#59's remaining four: `V34SetINFO1aBits` (1,401), `VPcmV34InitiateRetrain`
(1,406, still wanting `V34DisconnectThreshTable`), `V34GiveINFO1dBits` (436)
and `indicateJaTransmission` (57). Findings from 307.
## Where wave 2 got to

All seven symbols. The `setSessionFlag` chain landed first (findings F267-269);
`V90Phase3Demodulator::reset` (801) and `V90Demodulator::enterPhase3` (448)
followed, with `Descrambler<int,int>` -- the one thing `closure.py` reported
missing that really was -- added to `include/dsplib/Scrambler.h` beside the
scrambler it inverts. Findings F291-296.

`V90SessionFlag.h`'s five classes are now three: `V90Phase3Demodulator.h` and
`V90Demodulator.h` carry the two the batch gave real weight to, which is the
split that file asked for. **Both assert their size**, which it said could not
be done -- the displacement scan finding F268 warned about is still wrong, and
the `sysdep_malloc` before the constructor call is exact. 0x42c and 0x298.

Mutation suites `v90p3dreset` (29, 28 caught) and `v90demod` (30, 28 caught);
the three uncaught are all named with what is held fixed, and one of them
corrected a wrong sentence in the source rather than exposing a test gap
(finding F293).

**`tools/closure.py _ZN12VPcmFloModem11enterPhase3Ev --missing` is now one
symbol: itself.** Wave 3 is unblocked with nothing in front of it; finding F296
is the hand-over.

Three things wave 1 cost that the next batch should not pay again:

- a fresh worktree has no `third_party/spandsp`, and the failure names
  spandsp rather than the worktree (finding F257);
- a new C++ class header must go in `SKIP_HEADERS` in `tools/offcheck.py`, or
  the offsets gate reports every annotation in the tree as wrong; and its
  `offsetof` assertions must be guarded on
  `__SIZEOF_POINTER__ == 4` or `check64` fails;
- an anti-vacuity check derived from the wrong quantity fails loudly and looks
  like a broken reconstruction. Two of them did (findings F247, F262). Both were
  the test's arithmetic, not the function's.

## Done: the stub `V90Phase2Info` is gone

`V90PreFilter.h` includes the real header instead of declaring a 0x1c-byte
union; `V90Phase2Info.h` has dropped its `#error`; `autoSelection` reads
`phase2->L2` rather than punning `*(const float *const *)&phase2->b[0x18]`;
`t_v90prefilter` has a `P2()` accessor and sizes its Phase 2 comparison with
`sizeof(V90Phase2Info)`. The stub `class V90Parameters` stays -- that one is
still unmodelled.

It bought hygiene, not coverage, and finding F264 says so: the comparison grew
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
`docs/v34handshak.md` is its manual and findings F285-290 are the record.

It unblocks #57, which is the 27.6 KB and the sixteen units. **It unblocks
#56 too, since `w3_tbl1`.** The per-sample transmit route did not compare and
finding F289 and D60 read that as a property of the object; it was the
fixture's memory layout, and one arena per side closes it. All nineteen of
table 1's reachable targets compare and eighteen of them separate cold.
Findings F319-324, D60 retracted, D61 for the residual.

**Findings F285-290 were taken out of the 279-300 block this file used to
reserve for #59's six.** The block table above has been rewritten to match
what is actually in `docs/findings.md`: 291-296 went to
`V90Phase3Demodulator::reset`, so **#59's remaining five start at 297**, not
at 291.

## Where `V34SetINFO1aBits` got to

Landed. 1,401 bytes, and the batch really was one symbol: after a build,
`tools/closure.py V34SetINFO1aBits --missing` is itself and nothing else.
Findings F307-312, mutation suite `v34info1a` (44 mutations, 40 caught and 4
proved equivalent, two of which had to be replaced by neighbouring mutations
that can fail). Coverage 20.6% -> 20.7%.

It is `src/pump/v34/v34info1a.cpp`, NOT `v34info.c`: it calls
`VPcmFloModem::getUinfoValue`, so the translation unit has to be C++, exactly
as `v34k56.cpp` had to be. The test is `test/unit/t_v34info1a.cpp` and it
stands a whole 32 KB `VPcmFloModem` graph up beside a `struct v34_object`,
with the pointer cycle wired both ways.

Two things it settles for everyone else in #59:

- **`obj->p3548` is a `VPcmFloModem *`** (finding F307). `v34info.c`'s
  `SESSION_VARIANT` and `SESSION_UINFO6` are `info0Layout` and
  `pcmSessionType`; +0x611c is one field meaning "PCM upstream / V.92
  session", written by three functions in three translation units.
- **`struct v34_ratecfg` gained `carrier` at +0x10** and `f06` at +0x06, and
  `struct v34_object` gained `f35a4` at +0x35a4 (finding F311 says why that
  one stays offset-named, and what `VPcmV34InitiateRetrain` should do about
  it).

`V34GiveINFO1dBits` is the next one in this corner; finding F312 is its
hand-over.

## `V34GiveINFO1dBits` has landed

436 bytes, findings F332-337, mutation suite `v34info1d` -- 42 mutations, 41
caught, 1 proved equivalent from the object rather than from the sweep, and no
gaps. Test `test/unit/t_v34info1d.c`, 84,738 checks. Coverage 21.2%, 154,556
bytes, 369 symbols. `make phase` green.

**It is in `src/pump/v34/v34pcmmain.cpp`, and finding F312's rule is why that
took two tries.** It names no mangled symbol, so C compiles it -- but it calls
`VPcmV34InitiateRetrain`, which lives in the `.cpp`, and the Makefile's
`$(SRC)` is every `.c` under `src/`, linked 64-bit with no C++ in it by the six
interop binaries. A `v34info.c` version passed every 32-bit test and failed
`make phase` at `t_spandsp_v23` with an undefined reference. **The rule has a
second half: a `.c` may not call anything defined in a `.cpp`.** Finding F333.

The declaration is in `include/dsplib/v34info.h` beside its three siblings all
the same, which is what `V34SetINFO1aBits` already does.

Two things it settles for the rest of #59:

- **Finding F311's destination question is closed.** Both writers of
  `10000 + 336 * f35a4` store to `obj + 0x254`; 311's "`esi + 0x250`" was a
  `lea 0x254(%ebx),%esi` read as if `esi` were the object. `f35a4` is STILL
  not named, because the remaining objection moved to the readers: only
  `v34handshak` and `datapumpv34` read +0x254 and neither is reconstructed.
  Finding F336.
- **A relocation on a call proves nothing about the translation unit.** Its
  ABSENCE does (finding F306's `getMPrecvdBits` is `LOCAL`); its presence is
  just `GLOBAL` binding. Finding F333.

`indicateJaTransmission` (57 bytes) is what is left of #59 besides
`v34handshak`, and finding F332 is its hand-over: its closure is everything
above plus `DILdescriptorPacker`, all of which is now written.

## `indicateJaTransmission` has landed, and #59 is done

57 bytes, findings F380-381, mutation suite `v34ja` -- 13 mutations, 12 caught,
1 recorded equivalent and no gaps. Test `test/unit/t_v34ja.cpp`, 6,601 checks.
Coverage 21.2%, 154,613 bytes, 370 symbols. `make phase` green.

It is the first batch in this task whose closure was **empty before it
started**: after a build, `tools/closure.py indicateJaTransmission --missing`
is zero symbols and zero bytes, which is what putting it last was for.

    void indicateJaTransmission(void *obj);

Pure dispatch: `VPcmFloModem::enterPhase3` when `obj->v90_receiver > 1`,
otherwise `K56FlexFloModem::enterPhase3FullDuplex` when
`obj->k56flex_receiver > 1`, otherwise nothing. Both tests are signed `> 1`,
it stores nothing anywhere, and it returns nothing. In
`src/pump/v34/v34pcmmain.cpp` by finding F333's rule -- two C++ callees, so the
TU must be C++ -- with the declaration in `v34hshak.h` beside `v90Phase34` and
`k56FlexPhase34`, because its only two callers are inside `v34handshak`.

Two things it settles for whoever writes `v34handshak`:

- **The second arm's INTERIOR is untestable and finding F381 says why**:
  `K56FlexFloModem::enterPhase3FullDuplex` is one byte of code, so its
  condition, its operand, its object and its exclusivity with the first arm
  are one equivalence class. Held fixed: that the callee stays empty. Its
  POSITION is a different matter and is tested -- trying the K56flex arm first
  changes what happens when both receivers are up, and that mutation is caught.
- **The `+ 4` is still an addressing artifact**, now with a differential test
  behind it: the mutation that reads `v90_receiver` and `k56flex_receiver` at
  the literal `obj + 0x248` / `obj + 0x24c` is caught.

**#59's six are complete.** `k56FlexPhase34`, `v90Phase34`, `V34SetINFO1aBits`,
`VPcmV34InitiateRetrain`, `V34GiveINFO1dBits`, `indicateJaTransmission`. With
#60 already closed, the only V.34/V.90 work left is `v34handshak` itself
(61,541 bytes, #56-#58) and `datapumpv34` (1,028) behind it.

## What is still owed, in order — read this first if you are resuming

**Rewritten after the merge to `master`.** Everything the previous version of
this section listed is done: table 1's txstates 21 and 66 landed, the unify and
the offset-macro rename landed as one batch, the field map landed, and the
trace gap it called an open gap is closed. Task numbers do not survive a
session; this section is the durable copy, and every item below is measured,
not planned.

`v90rest` is fully contained in `master` as of `d019957` — `git log
master..v90rest` is empty. Work on `master`.

### F1. Named gaps, each with the measurement that says why it is a gap

  - **Three uncaught mutations in `v34hstx1`**, down from eleven (finding F651).
    `81: the wrap is tested before the counter is stored` is finding F343's
    transfer with no oracle; `67: initdigital is not called` and
    `67: +0x3598 is not set` print nothing at all, so they are unobservable
    rather than merely untested.
  - **The `0x62b5f` txstate reload is right by disassembly and tested by
    nothing** (finding F592). Removing it is NOT CAUGHT by all eight suites,
    because of the 27 direct calls only two force a value and both store it
    first. Filed as `equivalent` carrying that enumeration, so it fails the day
    an arm lands that passes a forced txstate without storing it.
  - **`v34k56` is 15 caught against 10 NOT caught** — 40% of what that suite
    claims to check is unchecked, and the tree-wide census that surfaced it is
    finding F545. That census predates finding F651 and no finding records a
    tree-wide total since.
  - **Twelve C++ translation units still fail under the period compiler**
    (finding F650), so the codegen tier cannot see them at all.

### F2. The field map, continued

Twelve spans became named fields (findings F630-639) and the rest are still
offsets. **The "N of 79 remaining" ratio is not quotable** and finding F639 says
why: the denominator was scraped from macro blocks and at least five of its
values are not object offsets. Take the next spans by measurement, the same
way, and quote the numerator alone.

The busiest remaining case is the one the field map deliberately did not
touch: `hs_get`, `hs_put` and `hs_setstate` take the offset as a **runtime
argument**, so 59 of the busiest accesses have no field to name without
changing a signature that exists precisely to serve all three machines
(finding F632).

### F3. The object, which is where the real work is

```
  .text            734,605 bytes   1,861 symbols
  translated       159,739 bytes     417 symbols     21.7%
  tested           159,718 bytes     412 of 417 that can be
```

By translation-unit span, largest first: `VPcmV34Main.cpp` 290,315 bytes over
739 symbols; `class1tx.c` 89,322 over 332; `V34hshak.c` 61,541 in one symbol —
`v34handshak` itself, still landing one dispatch arm at a time, with an arm
nobody has written calling `abort`; `V32mod.c` 55,694 over 119.

### F4. Housekeeping, if it has not been done

`symmap-scaffold-spike` is unmerged and old (its findings stop at 213). Decide
whether it is wanted before it diverges further.
