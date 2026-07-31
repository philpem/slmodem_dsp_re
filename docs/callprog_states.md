# The call-progress state machine

`CALLPROG_Create` builds its state machine at run time, in module-scope
`.bss`, rather than declaring it as static data.

**The tables keep their names.** They are file statics, so `nm` reports them
lower-case `b` and `objcopy --redefine-syms` cannot give them `ref_` aliases --
but the symbol table still carries what the author called them, and that names
every dimension of the machine:

```
    0x020  next_state_due_cptd                [10][8]
    0x0c0  message_due_cptd                   [10][8]
    0x070  next_state_due_line_clear_timeout  [10]
    0x110  message_due_line_clear_timeout     [10]
    0x11a  next_state_due_timeout             [10]
    0x124  message_due_timeout                [10]
    0x140  timeout_table                      [10] ints
    0x080  enable_line_clear_timeout          [10] ints
    0x168  automode_table                     [10]
    0x172  toneiir_dialtone_table             [10]
    0x17c  toneiir_busy_table                 [10]
```

So there are **three** ways to leave a state, each with its own next-state and
message pair: the call-progress tone detector (`cptd`), a general timeout, and
a line-clear timeout. The eight-wide dimension of the first pair is the
detector's verdict.

And the last two name what phase 3 has already built: `toneiir_dialtone_table`
and `toneiir_busy_table` say, per state, which of the two cadence detectors
`CALLPROG_Create` made is the one being listened to. That is why only two are
created (finding 55) -- the machine only ever asks about dial tone and busy.

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

It gains support from the timeout table, though: the state that times out with
`CALLPROG_NO_DIAL_TONE` is state 1, and state 1 is `CALLPROG_WAIT_DIAL` under
this mapping. Four of the six timeout messages line up with their state's name
the same way, which is a lot of coincidence to attribute to a wrong ordering.

## The other tables

```
    next_state_due_line_clear_timeout   cleared, then [4] = 5
    message_due_line_clear_timeout      cleared, then [4] = 7
    enable_line_clear_timeout           cleared
```

So exactly one state -- 4 -- has a line-clear timeout at all, and it moves to
state 5 reporting message 7 (`CALLPROG_ANSWER`).

```
    next_state_due_timeout   cleared, then [1]=6 [3]=6 [4]=6 [5]=6 [8]=6 [9]=6
    message_due_timeout      cleared, then [1]=2 [3]=1 [4]=6 [5]=13 [8]=10
```

Six states can time out and all six go to state 6, the terminal one, with a
different message each: 2 is `CALLPROG_NO_DIAL_TONE`, 1 `CALLPROG_NO_RING`, 6
`CALLPROG_NO_ANSWER`, 13 `CALLPROG_ANSWER_STATE_TIMEOUT`, 10 `CALLPROG_BUSY`.
Read down that column and it is the list of ways a call fails to connect.

```
    timeout_table         cleared, then six entries taken from the
                          CALLPROG object's own first seven words
    automode_table        cleared, then [3] = [4] = [5] = 1
    toneiir_dialtone_table  cleared, then [1] = [2] = 1
    toneiir_busy_table      all ten set to 1
```

`toneiir_busy_table` being all ones is the clearest statement in the machine:
**busy tone is listened for in every state**, and dial tone only in states 1
and 2, which is where a modem is waiting to dial.

Being file statics none of these can be read by a differential test, so they
will be verified through `CALLPROG_Progress` -- the only thing that consumes
them, and the reason Callprog.c should be reconstructed as one unit rather
than create-then-progress. The same constraint shaped `AnalyseDialString` and
`GetNextDigitAndReturnNextState`.

## Why this is written down before the code

Sixty-odd table writes is exactly the kind of transcription a person gets
subtly wrong, and the error would be a state machine that works for the common
path and diverges on the fifth branch nobody tests. Extracting it by script
first, and keeping the extraction, means the reconstruction can be checked
against this rather than against a memory of the disassembly.
