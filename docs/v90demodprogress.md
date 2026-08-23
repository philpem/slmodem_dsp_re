# `V90Demodulator::progress`: the batch, the order, and what is already decoded

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
| 180 | `V90BitsToSymbol::process(unsigned char *, unsigned int)` |
| 108 | `V90BitsToSymbol::reset` -- WRITTEN |
| 96 | `V90Phase4Modulator::setMappingParams` |
| 58 | `V90BitsToSymbol::resetNoSpectral` -- WRITTEN |

**Nothing can be landed alone.** Every test binary links all of `$(OBJ)`, so one
unwritten callee fails all 92 binaries rather than its own. `progress` reaches
`V90Phase4Demodulator::reset`, which reaches the modulator and mapper chain, so
the batch is the unit.

**The order is bottom-up**, and the bottom is the two `V90Mapper` resets:
their four callees -- `alaw2linear`, `ulaw2linear`, `V90SpectralShaper::reset`
and `::resetSSFilter` -- were all written, so they were the only members of the
batch that could be started. **FOUR MORE ARE NOW WRITTEN AND DIFFERENTIALLY
GREEN**: both `V90BitsToSymbol` resets and `V90Mapper::process`, whose closure
was itself alone once the mapper resets landed.

    V90Mapper::{resetNoSpectral, reset}     DONE         -> unblocked
    V90BitsToSymbol::{resetNoSpectral, reset}    DONE
    V90Mapper::process                           DONE
    V90BitsToSymbol::process(unsigned char *, unsigned int)   180 B
    V90Phase4Modulator::{setMappingParams, reset}
    V90Phase4Modulator::{generateV90Symbol, generateV92Symbol}   6,157 B
    V90Phase4Demodulator::reset
    V90Demodulator::exitPhase3
    V90Demodulator::progress

`V90BitsToSymbol::process(unsigned char *, unsigned int)` is the next rung and
is 180 bytes; the other overload, `process(unsigned int &, short *)`, was
already written.

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

## The fixture

`test/unit/t_v90modchain.cpp` already builds `V90Mapper`, `V90BitsToSymbol`,
`V90Phase4Modulator` and `V90Modulator` differentially, over seeded rather than
zeroed storage, with the allocator-pointer substitution this class needs
(`mapper` at `+0x00` is owned). Extend it rather than starting a fixture: the
hard part -- constructing both sides by symbol through `asm()` labels -- is done.
