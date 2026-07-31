# The call-progress state machine

`CALLPROG_Create` builds its state machine at run time, in module-scope
`.bss`, rather than declaring it as static data. Two tables of ten states by
eight events:

```
    .bss+0x20   unsigned char next_state[10][8]
    .bss+0xc0   unsigned char message[10][8]
```

Both are cleared, then every state gets the same three "from anywhere"
transitions, then eleven individual entries are patched in. This document is
the result, extracted from the object rather than transcribed: the offsets
were pulled out of the disassembly by script and converted to `[state][event]`
here.

## The tables

`.` means the cleared default: next state 0, message 0
(`CALLPROG_NO_MESSAGE`).

```
              event 0   1     2       3          4        5     6       7
    state 0     .       .    6/BUSY  6/CONG      .        .    6/ERR    .
    state 1     .     2/DIAL 6/BUSY  6/CONG    6/ERR    6/ERR  6/ERR    .
    state 2     .       .    6/BUSY  6/CONG      .        .    6/ERR    .
    state 3     .     6/ERR  6/BUSY  6/CONG    4/RING   4/RING 6/ERR    .
    state 4     .     6/ERR  6/BUSY  6/CONG    4/(none) 4/(none) 6/ERR  .
    state 5     .     6/ERR  6/BUSY  6/CONG    4/(none) 4/(none) 6/ERR  .
    state 6..9  .       .    6/BUSY  6/CONG      .        .    6/ERR    .
```

Read `6/BUSY` as "go to state 6, report `CALLPROG_BUSY`".

The three columns present in every row are what make the machine tractable:
**busy, congestion and error always go to state 6**, from wherever it is. That
is the terminal state.

## What the numbers mean

The messages are the `CALLPROG_*` codes recovered in phase 3's first commit --
3 is `CALLPROG_DIALING`, 5 `CALLPROG_RINGBACK`, 10 `CALLPROG_BUSY`, 11
`CALLPROG_CONGESTION`, 12 `CALLPROG_ERROR`.

The states are the second enum whose names the object also carries as debug
strings: `CALLPROG_NO_LEGAL_STATE`, `CALLPROG_WAIT_DIAL`, `CALLPROG_DIALING`,
`CALLPROG_WAIT_RING`, `CALLPROG_WAIT_TO_ANSWER`, `CALLPROG_ANSWER_STATE`,
`CALLPROG_END`, `CALLPROG_END_PARTIALLY_STATE`, `CALLPROG_WFS_STATE`,
`CALLPROG_BONGTONE_STATE`. Ten names for ten states, in the order the compiler
emitted them, and state 6 being `CALLPROG_END` fits the table exactly.

**That mapping is not yet proven** -- it rests on the string order matching the
enum order, which is usual but not guaranteed. `CALLPROG_Progress` is what will
confirm it.

## The other module statics

```
    .bss+0x070  unsigned char [10]     cleared, then [4] = 5
    .bss+0x080  int           [10]     cleared
    .bss+0x110  unsigned char [10]     cleared, then [4] = 7
    .bss+0x11a  unsigned char [10]     cleared, then a scattering of 6s,
                                       and [11]=2 [13]=1 [14]=6 [15]=13 [18]=10
    .bss+0x124  unsigned char [10]     cleared
    .bss+0x140  int           [10]     cleared, then six entries taken from
                                       the CALLPROG object's own first seven
                                       words
    .bss+0x168  unsigned char [10]     cleared, then [3]=[4]=[5]=1
    .bss+0x172  unsigned char [10]     cleared, then [1]=[2]=1
    .bss+0x17c  unsigned char [10]     all set to 1
```

Being module statics they have no symbols, so a differential test cannot read
them. They will be verified through `CALLPROG_Progress`, which is the only
thing that consumes them -- the same situation as `AnalyseDialString`, and the
reason Callprog.c should be reconstructed as one unit rather than
create-then-progress.

## Why this is written down before the code

Sixty-odd table writes is exactly the kind of transcription a person gets
subtly wrong, and the error would be a state machine that works for the common
path and diverges on the fifth branch nobody tests. Extracting it by script
first, and keeping the extraction, means the reconstruction can be checked
against this rather than against a memory of the disassembly.
