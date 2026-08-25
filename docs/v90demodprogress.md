# `V90Demodulator::progress`: the batch, the order, and what is already decoded

> **THE `VPcmV34Main.cpp +72` DATA-MODE SPAN IS NOW EMPTY.**  The twelve
> symbols after this one -- `VPcmFloModem::qcLineVerification`,
> `setDilDescriptor`, `V90Modem::reset`, `VPcmFloModem::vPcmResetPhase3Modem`
> and the eight DIL tables `TO`, `SP`, `TP`, `H`, `REF`, `Lsp`, `Ltp` and `N`,
> 2,516 bytes in all -- are written and differentially green, and
> `tools/service.py` no longer lists that span under DATA at all.  With them
> the four `VPcmV34Progress` entry points are complete and the unwritten
> boundary below it is closed (finding 7606).  Findings **7600-7608**; 7607 is
> the one to read first, a `movzwl` this tree's own header had recorded and
> whose `src/` shipped without it.  What remains under that bracket label is
> 24 symbols that **no entry point reaches**, among them
> `calculateDilLength(DilType, PcmType)` and `getSegmentPointer`.

`.text+0x1ca90`, **7,276 bytes**, 1,700 instructions, 44 distinct callees. The
largest unwritten function in the V.90/V.92 span and the one everything else in
that span waits on. This is the working record; `docs/plan.md` is the order for
the tree as a whole.

## It is a 13-symbol batch, not one function

    python3 tools/closure.py --missing _ZN14V90Demodulator8progressEPiRjPfj
    -> 13 symbols, 16,853 bytes (unwritten only)

| bytes | symbol |
|--:|---|
| 7,276 | `V90Demodulator::progress` |
| 3,922 | `V90Phase4Modulator::generateV92Symbol` |
| 2,235 | `V90Phase4Modulator::generateV90Symbol` |
| 768 | `V90Demodulator::exitPhase3` |
| 530 | `V90Mapper::process` -- WRITTEN |
| 517 | `V90Mapper::reset` -- WRITTEN |
| 504 | `V90Phase4Demodulator::reset` |
| 404 | `V90Mapper::resetNoSpectral` -- WRITTEN |
| 255 | `V90Phase4Modulator::reset` |
| 180 | `V90BitsToSymbol::process(unsigned char *, unsigned int)` -- WRITTEN |
| 108 | `V90BitsToSymbol::reset` -- WRITTEN |
| 96 | `V90Phase4Modulator::setMappingParams` -- WRITTEN |
| 58 | `V90BitsToSymbol::resetNoSpectral` -- WRITTEN |

**Nothing can be landed alone.** Every test binary links all of `$(OBJ)`, so one
unwritten callee fails all 92 binaries rather than its own. `progress` reaches
`V90Phase4Demodulator::reset`, which reaches the modulator and mapper chain, so
the batch is the unit.

**The order is bottom-up**, and the bottom is the two `V90Mapper` resets:
their four callees -- `alaw2linear`, `ulaw2linear`, `V90SpectralShaper::reset`
and `::resetSSFilter` -- were all written, so they were the only members of the
batch that could be started. **SIX MORE ARE NOW WRITTEN AND DIFFERENTIALLY
GREEN**: both `V90BitsToSymbol` resets, `V90Mapper::process`,
`V90BitsToSymbol::process(unsigned char *, unsigned int)` and
`V90Phase4Modulator::setMappingParams`.

    V90Mapper::{resetNoSpectral, reset}     DONE         -> unblocked
    V90BitsToSymbol::{resetNoSpectral, reset}    DONE
    V90Mapper::process                           DONE
    V90BitsToSymbol::process(unsigned char *, unsigned int)   DONE
    V90Phase4Modulator::setMappingParams          DONE
    V90Phase4Modulator::generateV90Symbol          DONE   2,235 B
    V90Phase4Modulator::generateV92Symbol          DONE   3,922 B
    V90Phase4Modulator::reset                     DONE     255 B
    V90Phase4Demodulator::reset                   DONE     504 B
    V90Demodulator::exitPhase3                    DONE     768 B
    V90Demodulator::progress                      DONE   7,276 B

**THE BATCH IS COMPLETE.**  All thirteen symbols are written and
differentially green, and the V.90 receive path closes with
`V90Demodulator::progress` -- 7,276 bytes, 1,708 instructions, 129 calls, four
dispatch tables, and NO `*_notwritten()` stub anywhere in it.  Findings
7450-7460 for the pumps -- 7450 has the two jump tables and 7452 the seven
places the V.92 pump genuinely differs from the V.90 one -- 7470-7477 for the
two resets, 7480-7485 for `exitPhase3`, and 7510-7513 for `progress`.

**WHAT `progress` COST AND WHAT IT DID NOT BUY.**  Its own binary,
`t_v90demprog`, is 1,868 differential checks green, and its suite is **100
mutations, 26 caught, 74 NOT caught** -- the 26 are the prologue, the phase
dispatch and part of the common tail, and the 74 are the `word_3c` arms, kept
rather than deleted because NOT CAUGHT is a measurement and a suite reading 26
of 26 would be a headline with its denominator removed.  The arm is not
plantable: `V90Equalizer::process` clears `stateCount` on entry and refills it
from a chain that bottoms out in the phase 3 demodulator's own state machine,
so the sweep reaches THREE distinct arms of thirty-one (7513).  Driving the
rest is the next pass's work and it needs the equaliser walked to each of its
events -- and 7513 records what happened when this fixture tried to take a
short cut to that.

**BOTH `reset`s COME OUT OF GCC 3.4.2 AT THE BLOB'S OWN SIZE**, 255 and 504
bytes, the modulator's byte for byte and the demodulator's instruction for
instruction with two free scheduling permutations.  That is the structural
check the pumps could not make (7460), and it is finding 7470.

`V90Phase4Modulator::reset` is 255 bytes of sixteen stores, one G.711
expansion with NO cast (7471 -- the class's own `P4M_LEVEL` macro has one and
must not be reused here), one `Scrambler<h,h>::reset(0)` and a loop that runs
whichever pump `sessionFlag` selects, `nofSymbols` times.  Everything the
edges maintain is put back; every field describing a MESSAGE is left alone.

`V90Phase4Demodulator::reset` is 504 bytes and is the whole receiver's entry
point: eleven scalars, both `V90RDetector`s from
`(PHASE4_R_DETECTION_LENGTH, 0x18)`, the CP under V.92 or the MP under V.90,
the demapper, the embedded modulator's own `reset` into TRN2d and its
`setMappingParams`, two diagnostics at two different gates, and a loop over
`getV92Decision` or `getV90Decision`.  The two session arms differ in exactly
one store -- +0x34fc, V.92's alone (7473) -- and the demapper's double guard
is untestable because both pointers are dereferenced unconditionally in the
same function (7474).

Its test is in `t_v90p4ddec` and not in a fixture of its own, because that is
where the demapper, the descrambler, the CP and the MP are already planted
well enough for the decision members to run: a home that could not drive the
loop would leave "the two decision members are swapped" with no witness.

**`setMappingParams` IS AT `.text+0x2d120` AND IS 96 BYTES**, which the table
above always said and a task brief did not: `0x30310` is inside
`V90Mapper::reset`. `nm -S -C` on the blob is what settles an address, every
time.

The remaining `process` overload, `(unsigned char *, unsigned int &, short *)`
at 0x2faa0, is 484 bytes and is NOT in this batch's closure.

## What `V90Demodulator::exitPhase3` turned out to be

768 bytes at 0x1bb50, the hand-over from phase 3 to phase 4, and one straight
line with two conditionals in it: the TRN1d RMS ratio reported through
`%c%d.%08d` and copied into `tagV90AdditionalCPinfo` along with three more
stores, `V90Phase3Demodulator::exitDIL`, the phase 4 entry when phase 3
terminated, `setNofUcodesInTrn2`, the twelve-argument `V90TRN2Design`, the
delayed retrain its failure raises, `displaySpectralParams`, and
`phase4Demodulator->reset(Uinfo, 0, 0, quickConnect)`.

**IT MAKES TWO CALLS IN THE PHASE 4 ARM AND THE INLINING HIDES ONE.**
`V90Demodulator::enterPhase4` is inlined at 0x1bdd0 and its idempotence test's
TAKEN edge lands on `equalizer->enterPhase4()` -- so the source is
`{ enterPhase4(); equalizer->enterPhase4(); }` and not one call.  The first
draft missed it with every branch, store and other call agreeing; what said
otherwise was 174 instructions against the blob's 186.  Finding 7480, and the
lesson is to count instructions before building the fixture, because until the
fixture exists the count is the only witness.

**THE LATCH IS `== 1`, NOT `!= 0`**, and that matters because
`enterChannelVerification` leaves the object at 5.

**THE TWELVE-ARGUMENT DESIGN CALL IS FIVE VIEWS OF ONE DETECTOR** -- four
tables reached as `adid + 0`, `+0x600`, `+0xd00` and `+0x2800`, plus
`getMaxUcode()` returning `&adid->maxUcode[0]` through phase 3 -- three more
scalars out of the same object's tail, and one EMBEDDED field,
`spectralVerifier.word_28` at +0x238, read early and stashed across two calls.
Finding 7482.

**Three header corrections fell out**: `V90Demodulator` +0x240 is
`float trn1dRmsRatio` (the format string names it), `tagV90AdditionalCPinfo`
+0x14 is a `short` and not four bytes of pad, and +0x294 is `quickConnect` on
two independent derivations.  A fourth is a retraction: `V90Demodulator.h`'s
paragraph about CLAIMING `DSPLIB_V90PARAMETERS_H` has been history since task
#116 and a translation unit may hold this header and the NAMED `V90Parameters`
map together -- which `t_v90p4ddec.cpp` now does.  Finding 7481.

### Its test is in `t_v90p4ddec` and the reason is the LAST statement

Not `t_v90demod` and not `t_v90dataph`, although both build a `V90Demodulator`.
`exitPhase3` ends in `phase4Demodulator->reset(...)`, which needs the whole
phase 4 receiver -- demapper, CP, MP, descrambler, the embedded modulator, its
converter and the mapper under it -- and `t_v90p4ddec` is where that was built
for the previous batch.  Sixty lines of shallow wiring there against about two
hundred and fifty of deep wiring anywhere else.  What the member WRITES decides
per-side against shared: four peers it writes (the parameter block, the mapping
block, the detector and the connection evaluator) are snapshotted, restored and
compared rather than duplicated.  Finding 7483.

Counts: **`v90exit3` is 55 mutations, 53 caught, 0 NOT caught, 0 unusable, 2
equivalent.**  The binary's own two suites did not move -- `v90p4ddec` 61 of 74,
`v90p4dreset` 54 of 54 -- because `exitPhase3` calls the phase 4 reset with a
trip count of zero and presents no argument shape `run_p4d_reset` does not
already sweep.

## What the two symbol pumps turned out to be

One `switch` over `state` each, dispatched through a jump table in `.rodata` --
`+0xa94` with twenty-eight entries for the V.90 pump and `+0xb04` with
thirty-one for the V.92 one -- and the two tables are what say WHICH states
each has a `case` for. The middles are disjoint: V.90 dispatches the MP ladder
at 0x04 and 0x0d..0x0f and sends 0x05..0x0c to the default edge, and V.92 does
exactly the reverse against `cpSequenceSymbols`. V.92 also owns the
silence/Rt/Rf ladder at 0x18..0x1e, which the shorter table stops before.

Nine V.90 arms and fourteen V.92 ones share one idiom -- `nofBitsForNextTime`,
a conditional fill, then the drain into a one-`short` slot -- and the arms
CALL the class's existing members rather than repeating them, which a
redundant `cmpl $0xe,0x4(%esi)` at +0x2e0a4 is the evidence for (7451).

**Two header corrections fell out and both are made**: `pad_000c` is a live
`unsigned int` written by three members and read by none of the forty-five, and
the enumeration gains 0x14 and 0x1c, which the header had said were absent
because nothing stored or compared them. Both keep offset names. Finding 7453.

**WHAT THE TWO RESETS RETIRED FROM THE PUMP GRID'S PLANTING IS ONE THING AND
IT IS THE HEADLINE ONE.**  7454 says no member of `V90Phase4Modulator` can put
the object into state 0x0f, 0x14 or 0x1c; `reset`'s third argument is a
`Phase4ModulatorState` stored unexamined, so all thirty-two states are now
reachable through a public member and `run_p4m_reset` drives them that way.
Nothing else in 7454's list can be retired -- `reset` forces zero where the
grid needs a range -- and `setup_pump` was left alone rather than rewired, so
the pump grid re-ran unchanged at 169 caught of 171.  Finding 7475.

Three things the fixture had to be given beyond a seed, each of which read as a
defect in `src/` first: the drain writes whole symbols into a ONE-`short` slot,
so any converter setting that lets it copy two smashes the pump's frame (7455);
a fill straight after a reset can yield NO symbols, because the mapper's
priming countdown swallows them, and the drain then writes nothing at all
(7456); and the scrambler's history is a heap allocation OUTSIDE the object, so
`reset(0)` over allocator-zeroed memory moved nothing that was compared (7457).

## Two object-map corrections that fall out of the mapper resets

**BOTH WERE RE-DERIVED WHEN THE TWO RESETS WERE WRITTEN. One held exactly and
one was too cautious; the paragraph below is what it now says.**

`include/dsplib/V90Mapper.h` modelled `+0x020` to `+0x658` as one
`pad_020[0x638]`. It is not opaque:

- **`+0x056` is `short constellation[6][128]`** -- CONFIRMED -- 1,536 bytes
  running to `+0x656`. Both resets fill it with `mov %ax,0x56(%ebp,%ebx,2)`
  where `%ebx` is `128 * k + i`, `k` counted 0..5 against
  `constellationSize[k]` and the stack slot holding the base advanced by
  `subl $0xffffff80` -- add 128 -- once per `k`. `+0x656` is two bytes of
  alignment before `constellationSize`. The element type is `short` for the
  VALUES and not for the encodings: the object's only load of it, in `process`,
  is a `movzwl` whose upper half is discarded by the next instruction, which is
  finding 614's free case.
- **`+0x700` is a live field, not tail padding** -- CONFIRMED, and settled
  further. It is four bytes, both resets store the constant zero into it with a
  `movl`, the CONSTRUCTOR does not write it, and a sweep of every `0x700(%`
  displacement in `.text` finds no reader anywhere in the object. So the class
  is 0x704 with NO tail padding, and the member is `word_700`. Finding 7102.
- **`+0x020..+0x055` is NOT "genuinely unmodelled"** -- this document's claim,
  and it did not survive. `V90Mapper::process` tiles all 54 bytes as four
  six-entry arrays -- `uint[6]` at `+0x20`, `short[6]` at `+0x38`,
  `unsigned char[6]` at `+0x44`, `short[6]` at `+0x4a` -- and the four bases
  meet exactly at `constellation`'s `+0x56`. **THEY ARE NOW NAMED**, `process`
  being written: `codes`, `levels`, `signs` and `samples`, of which the first
  three are typed by a mangling and `samples` is inference and labelled as
  such. Findings 7103 and 7420.
- **`+0x01c` is `bitsBuffered`**, on the header's own terms -- `process` is the
  member that settles it, and it is the index at which the next input bit goes
  into `buf`. `uint_6f8` was deliberately NOT renamed in the same pass.
  Finding 7421.
- **Five members at `+0x04`..`+0x14` are now named, from `V90Demapper`.** That
  class computes the same five quantities out of the same block by the same
  arithmetic and this tree already names all five, so `cleared_004` and its
  four neighbours are `bitsPerFrame`, `word_08`, `signBitsPerFrame`,
  `signBitGroups` and `signBitGroupSize`. `+0x6fc` is a
  `SerialDifferentialEncoder<unsigned char>` on a mangled `this` in `process`.
  Findings 7100 and 7104.

## Both `V90Mapper` resets are WRITTEN, and the decode below was 152 bytes short

**THE PARAGRAPH THAT USED TO HEAD THIS SECTION CALLED `resetNoSpectral` "fully
decoded" AND ITS READING STOPPED AT THE TAIL-FILL.** The function runs on for
another 152 bytes, 0x30346..0x303de, and every store in that stretch is in
`reset` too. Finding 7101; the corrected split is below and both functions are
now in `src/pump/v90/V90Mapper.cpp`, differentially green.

`.text+0x30280`, 404 bytes, 102 instructions. `this` at `0x30(%esp)`,
`mp` at `0x34`, `pcm` at `0x38`; plain cdecl as everywhere here (finding 215).

    +0x004 = mp[0]
    +0x008 = mp[0] - +0x00c                 (+0x00c is read, never written)

then, for k = 0..5:

    constellationSize[k] = mp[+0x604 + 4*k]              the entry count
    for i = 0 .. constellationSize[k]-1:
            b = ((const unsigned char *)mp)[128*k + i + 4]
            constellation[k][i] = pcm ? alaw2linear((b & 0x7f) ^ 0xd5)
                                      : ulaw2linear((unsigned char)~(b & 0x7f))
    for i = constellationSize[k] .. 127:
            constellation[k][i] = 0

and then the part that was missing, which is the tail BOTH functions share:

    modulusEncoder.field_00..field_14 = constellationSize[0..5]   +0x670..+0x684
    modulusEncoder.field_18           = +0x008                    +0x688
    signEncoder.prev_                 = 0                         +0x6fc
    spectralShaper.resetSSFilter(mp[+0x628], mp[+0x62c],
                                 mp[+0x630], mp[+0x634])          +0x68c
    word_700                          = 0                         +0x700

**`pcm` is tested as nonzero, not compared against a value** -- `test %ebx,%ebx
; jne` -- so zero is mu-law and anything else is A-law. The two code
conversions are the G.711 ones the object writes out longhand: mu-law
complements the seven low bits, A-law toggles them against `0xd5`.

The tail-fill runs to 127 unconditionally, so a short constellation leaves the
rest of its row zeroed rather than stale -- which is what a differential test
over never-zeroed storage has to check (findings 223 and 224).

`reset` is that whole shape with FIVE MORE STORES AND ONE SUBSTITUTION, and the
store list that used to stand here was wrong in both directions -- it credited
`reset` with `+0x670`, `+0x6fc` and `+0x700`, which both make, and described
`+0x670` as one word where it is the entire 0x1c-byte `ModulusEncoder`. What is
`reset`'s alone is

    +0x010 = mp[+0x620]                       shaperSR
    +0x014 = 6 / mp[+0x620], or 0             an UNSIGNED div, not idiv
    +0x00c = 6 - mp[+0x620]                   computed here, read there
    +0x01c = 0
    +0x6f8 = mp[+0x624] when the shaper runs, 0 when it does not

plus `V90SpectralShaper::reset(mp[+0x624], mp[+0x620], the four floats)` in
place of `resetSSFilter` -- **shaperId first and shaperSR second**, which is
the order the two adjacent stack slots at 0x30244 and 0x3024e give and is
silent whenever the two are equal.

## What the fixture had to be given, beyond a seed

`t_v90modchain.cpp` seeds its storage and never zeroes it, and for these two
functions that is NOT sufficient on its own, because the object has to be
CONSTRUCTED before a reset can run over it and the constructor zeroes nine of
the words the resets write. Five mutations proved it: dropping the store to
`cleared_01c`, `uint_6f8`, `signBitGroupSize` or the sign encoder was NOT
CAUGHT, because the field already held the value the store would have left.
The fixture now writes a sentinel over each after construction and before the
call. With that, all 33 of `test/mutations/v90mapper.json` are caught.

## `V90BitsToSymbol`'s pair -- WRITTEN, and the header's decode held exactly

`include/dsplib/V90BitsToSymbol.h` documented both resets before they could be
written, and the disassembly agrees to the instruction: `bitsPerFrame = mp[0]`,
`extraSymbols = (6 * mp[+0x624]) / mp[+0x620]` when the divisor is nonzero and
0 otherwise, then `symbolsDone = 0`, `symbolsBlockSize = 0`,
`extraSymbolsPending = 1`. `resetNoSpectral` is the mapper call and
`bitsPerFrame` alone. Nothing in that paragraph needed correcting.

Two things it did not say, and both are now in the header:

- **The divide is `div` and not `idiv`** -- `f7 f3` at 0x2f919, although
  `V90MappingParams::shaperSR` is declared `int`. No case in `reset_cases` can
  tell, because none has a negative `shaperSR`; `t_v90modchain` has a group of
  eleven that does, and it also drives `6 * shaperId` wrapping 32 bits.
- **`extraSymbols` is the mapper's priming loss**, `shaperId *
  signBitGroupSize`, which `V90Mapper::process` swallows one frame at a time.
  Two classes, one quantity, computed independently -- finding 7422, and the
  fixture asserts the mapper's count against this class's formula. The identity
  needs `shaperSR` to divide six, which every value V.90 uses does; 7422 has
  the counter-example and why it is not pedantry.

## What `V90Mapper::process` turned out to be

530 bytes, and the shape is one loop over the input bits with everything else
inside it. Each byte is one bit, buffered at `bitsBuffered` until
`bitsPerFrame` of them are there; then `ModulusEncoder::progress` turns the
`word_08` bits above the sign bits into six digits, each digit picks a level
out of its own constellation, the sign bits go either through
`V90SpectralShaper::process` in `signBitGroups` groups or -- when there is no
shaper -- through `SerialDifferentialEncoder<unsigned char>::process` one at a
time, and the finished frame is copied out under the priming countdown at
`+0x6f8`. `bitsBuffered` loses `bitsPerFrame` rather than being cleared, which
is what carries a part-filled frame between calls.

**Two of its statements are untestable over any object a `reset` can produce**,
and both are poked by hand rather than left unclaimed: the unconditional
`nofOut += 6 - start` on the partial-copy arm (finding 7423) and the
`bitsBuffered -= bitsPerFrame` that only differs from `= 0` when the buffer
arrives over-full. A third, the strictness of the `<` between the second and
third countdown arms, needs a `shaperSR` that does not divide six.

## What `V90BitsToSymbol::process(unsigned char *, unsigned int)` turned out to be

The FILL, where the other written overload is the drain, and the class has ONE
status alphabet: **1 SIZE_NOT_SET, 2 BUFFER_OVERFLOW, 3 BUFFER_UNDERFLOW**, 0
silent. This one raises 1 and 2, the other 1 and 3, and the object's three
`.rodata` messages are what say so. The body hands the bits to
`mapper->process` with a write pointer of `symbols + symbolsDone`, advances
`symbolsDone` by the count that comes back, and reports 2 -- clamping
`symbolsDone` to `symbolsBlockSize`, not to `nofSymbols` -- when the sum
exceeds `nofSymbols`.

**Status 2 is a report and not a guard**: the mapper has already written by the
time the capacity is looked at, so no `reset`/`process` sequence can raise it
without the write having gone outside the allocation. Finding 7430; the
fixture pokes `nofSymbols` down and drives the exact-fit boundary as well as
the failure.

## What `V90Phase4Modulator::setMappingParams` turned out to be

A null check and two calls into the converter at +0x44: `reset(mp, pcmType)`
with the companding law taken from +0x38 and not from the argument, then
`setSymbolsBlockSize(1)` as a sibling call whose answer is dropped. It stores
NOTHING in the modulator -- `mappingParams` and `mappingParams2` are left as the
constructor set them -- and is `void` because the two exits do not agree on
`%eax`. Finding 7431.

## The fixture

`test/unit/t_v90modchain.cpp` already builds `V90Mapper`, `V90BitsToSymbol`,
`V90Phase4Modulator` and `V90Modulator` differentially, over seeded rather than
zeroed storage, with the allocator-pointer substitution this class needs
(`mapper` at `+0x00` is owned). Extend it rather than starting a fixture: the
hard part -- constructing both sides by symbol through `asm()` labels -- is done.

Three things it grew for the two functions above, each of which the next batch
will want:

- **The debug transcript**, `dsplib_debug_capture_*` with `dsplibs_debug_level`
  and `ref_dsplibs_debug_level` moved together over 0, 1 and 2. Three levels
  and not two: `> 1` and `> 0` differ at exactly one value.
- **A seeded heap buffer.** `V90BitsToSymbol::symbols` is `sysdep_malloc`'d and
  left as the allocator found it, so the two sides' buffers hold two different
  lots of rubbish; both are filled with the same bytes after construction and
  the record of what was put there is what says "nothing was written".
- **The OWNED arm for anything that drives the converter.** A supplied
  `V90BitsToSymbol` is one shared instance and the second side's call runs over
  the first side's result.
