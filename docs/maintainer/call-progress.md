# The call-progress state machine

`CALLPROG_Create` builds its state machine at run time, in module-scope
`.bss`, rather than declaring it as static data.

The original file-local symbols name each dimension of the machine. The
following offsets and dimensions describe the reference object's `.bss`:

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

The last two select the cadence detectors: `toneiir_dialtone_table`
and `toneiir_busy_table` say, per state, which of the two cadence detectors
`CALLPROG_Create` made is the one being listened to. That is why only two are
created (finding F55) -- the machine only ever asks about dial tone and busy.

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

The state names also occur in the object's debug strings: `CALLPROG_NO_LEGAL_STATE`, `CALLPROG_WAIT_DIAL`, `CALLPROG_DIALING`,
`CALLPROG_WAIT_RING`, `CALLPROG_WAIT_TO_ANSWER`, `CALLPROG_ANSWER_STATE`,
`CALLPROG_END`, `CALLPROG_END_PARTIALLY_STATE`, `CALLPROG_WFS_STATE`,
`CALLPROG_BONGTONE_STATE`. Ten names for ten states, in the order the compiler
emitted them, and state 6 being `CALLPROG_END` fits the table exactly.

The current `Callprog.c` defines `state_names` in that order, and
`CALLPROG_Progress` uses the state value to index it. The earlier uncertainty
about deriving the mapping from string order is superseded by the completed
implementation. Consult `include/dsplib/callprog_state.h` for enum names.

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
```

### Detector selection depends on configuration

The dial-tone and busy enable tables have two configurations.
`CALLPROG_Create`'s configuration word -- `cfg.w0`, which `call_create`
derives from **S56** -- decides which detector each state listens to, and selects the enable pattern:

```
    w0 = 0  (S56 is 2, 4, ...)      w0 = 1  (S56 is 0, 1 or 3)
    dialtone  0 1 1 0 0 0 0 0 0 0   dialtone  0 0 0 0 0 0 0 0 0 0
    busy      1 1 1 1 1 1 1 1 1 1   busy      1 0 0 1 1 1 1 1 1 1
```

With `w0 = 1` **no state listens for dial tone at all**, and states 1 and 2
listen to nothing whatever. That is blind dialling: the modem waits a fixed
time in `CALLPROG_WAIT_DIAL`, hears nothing because it is not listening, and
leaves only when the state's timeout expires. `CALLPROG_Dial` starts the
machine in state 1 for `w0 = 1` and state 2 for `w0 = 0`.

This was measured, not read: it is why `t_call` sat in `CALLPROG_WAIT_DIAL`
through every input for a while, with a dial-tone detector that asserted
perfectly well when driven directly. Both halves of the split are now
covered by `t_call`, and only the `w0 = 0` half ever puts samples on the
line.

These tables are file-local mutable state. `CALLPROG_Create` builds them
and `CALLPROG_Progress` consumes them. Differential fixtures observe their
effects through processing, messages and transitions; they need not export
the tables as public API. The original extraction and reconstruction plan
are preserved in the [historical record](https://github.com/philpem/slmodem_dsp_re/blob/4169cb3b3108a840a51bca9ebfa086102c4c69c6/docs/callprog_states.md).
