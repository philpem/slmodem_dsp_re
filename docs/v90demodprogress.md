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
| 530 | `V90Mapper::process` |
| 517 | `V90Mapper::reset` |
| 504 | `V90Phase4Demodulator::reset` |
| 404 | `V90Mapper::resetNoSpectral` |
| 255 | `V90Phase4Modulator::reset` |
| 180 | `V90BitsToSymbol::process(unsigned char *, unsigned int)` |
| 108 | `V90BitsToSymbol::reset` |
| 96 | `V90Phase4Modulator::setMappingParams` |
| 58 | `V90BitsToSymbol::resetNoSpectral` |

**Nothing can be landed alone.** Every test binary links all of `$(OBJ)`, so one
unwritten callee fails all 92 binaries rather than its own. `progress` reaches
`V90Phase4Demodulator::reset`, which reaches the modulator and mapper chain, so
the batch is the unit.

**The order is bottom-up**, and the bottom is the two `V90Mapper` resets:
their three callees -- `alaw2linear`, `ulaw2linear`, `V90SpectralShaper::reset`
and `::resetSSFilter` -- are ALL WRITTEN today, so they are the only members of
the batch that can be started now.

    V90Mapper::{resetNoSpectral, reset}                  -> unblocks
    V90BitsToSymbol::{resetNoSpectral, reset}            -> unblocks
    V90Mapper::process, V90BitsToSymbol::process
    V90Phase4Modulator::{setMappingParams, reset}
    V90Phase4Modulator::{generateV90Symbol, generateV92Symbol}   6,157 B
    V90Phase4Demodulator::reset
    V90Demodulator::exitPhase3
    V90Demodulator::progress

## Two object-map corrections that fall out of the mapper resets

`include/dsplib/V90Mapper.h` models `+0x020` to `+0x658` as one
`pad_020[0x638]`. It is not opaque:

- **`+0x056` is `short constellation[6][128]`**, 1,536 bytes running to
  `+0x656`. Both resets fill it with `mov %ax,0x56(%ebp,%ebx,2)` where `%ebx`
  is `128 * k + i`, `k` counted 0..5 against `cleared_658[k]` and the stack slot
  holding the base advanced by `subl $0xffffff80` -- add 128 -- once per `k`.
  That leaves `+0x020..+0x055`, 54 bytes, genuinely unmodelled.
- **`+0x700` is a live field, not tail padding.** Both resets write it, and
  `sizeof` is 0x704, so `pad_6fd[7]` is hiding four bytes of it.

## `V90Mapper::resetNoSpectral` is fully decoded

`.text+0x30280`, 404 bytes, 102 instructions. `this` at `0x30(%esp)`,
`mp` at `0x34`, `pcm` at `0x38`; plain cdecl as everywhere here (finding 215).

    +0x004 = mp[0]
    +0x008 = mp[0] - +0x00c                 (+0x00c is read, never written)

then, for k = 0..5:

    cleared_658[k] = mp[+0x604 + 4*k]                    the entry count
    for i = 0 .. cleared_658[k]-1:
            b = ((const unsigned char *)mp)[128*k + i + 4]
            constellation[k][i] = pcm ? alaw2linear((b & 0x7f) ^ 0xd5)
                                      : ulaw2linear((unsigned char)~(b & 0x7f))
    for i = cleared_658[k] .. 127:
            constellation[k][i] = 0

**`pcm` is tested as nonzero, not compared against a value** -- `test %ebx,%ebx
; jne` -- so zero is mu-law and anything else is A-law. The two code
conversions are the G.711 ones the object writes out longhand: mu-law
complements the seven low bits, A-law toggles them against `0xd5`.

The tail-fill runs to 127 unconditionally, so a short constellation leaves the
rest of its row zeroed rather than stale -- which is what a differential test
over never-zeroed storage has to check (findings 223 and 224).

`reset` is the same shape plus `V90SpectralShaper::reset`, the `mp[+0x620]` /
`mp[+0x624]` pair that `V90BitsToSymbol::reset` also reads, and stores to
`+0x658..+0x66c`, `+0x670`, `+0x68c`, `+0x6f8`, `+0x6fc` and `+0x700`.

## What `V90BitsToSymbol` then needs, already read

`include/dsplib/V90BitsToSymbol.h` documented both resets before they could be
written, and the disassembly agrees exactly: `bitsPerFrame = mp[0]`,
`extraSymbols = (6 * mp[+0x624]) / mp[+0x620]` when the divisor is nonzero and
0 otherwise, then `symbolsDone = 0`, `symbolsBlockSize = 0`,
`extraSymbolsPending = 1`. `resetNoSpectral` is the mapper call and
`bitsPerFrame` alone. Both are ~30 lines once the mapper exists.

## The fixture

`test/unit/t_v90modchain.cpp` already builds `V90Mapper`, `V90BitsToSymbol`,
`V90Phase4Modulator` and `V90Modulator` differentially, over seeded rather than
zeroed storage, with the allocator-pointer substitution this class needs
(`mapper` at `+0x00` is owned). Extend it rather than starting a fixture: the
hard part -- constructing both sides by symbol through `asm()` labels -- is done.
