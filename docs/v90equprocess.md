# `V90Equalizer::process` — the complete decode

This document has moved to [docs/history/pcm/v90equprocess.md](history/pcm/v90equprocess.md).
The original path is retained for references in source comments and tooling.

## F0. Signature and frame

See the [complete document](history/pcm/v90equprocess.md).

## F1. Two struct changes this batch owns

See the [complete document](history/pcm/v90equprocess.md).

## F2. Prologue

See the [complete document](history/pcm/v90equprocess.md).

## F3. The symbol loop

See the [complete document](history/pcm/v90equprocess.md).

### F3.1 The fixed-point arm, `0x38fcc`

See the [complete document](history/pcm/v90equprocess.md).

### F3.2 The float arm, `0x390d0`

See the [complete document](history/pcm/v90equprocess.md).

### F3.3 `switch (state)`, `0x390bd`

See the [complete document](history/pcm/v90equprocess.md).

### F3.4 The fixed-point error tail, `0x39272`

See the [complete document](history/pcm/v90equprocess.md).

### F3.5 The float error tail, `0x394e1`

See the [complete document](history/pcm/v90equprocess.md).

## F4. The arms

See the [complete document](history/pcm/v90equprocess.md).

### state 6 — `CHANNEL_VERIFY`, `0x39b21`

See the [complete document](history/pcm/v90equprocess.md).

### state 5 — `FPE`, `0x39b87`

See the [complete document](history/pcm/v90equprocess.md).

### state 4 — `RRN`, `0x39d0c`

See the [complete document](history/pcm/v90equprocess.md).

### state 3 — `DATA`, `0x39d55`

See the [complete document](history/pcm/v90equprocess.md).

### state 2 — `PHASE4`, `0x39f65`

See the [complete document](history/pcm/v90equprocess.md).

### state 1 — `PHASE3`, `0x3a07b`

See the [complete document](history/pcm/v90equprocess.md).

### An observation about the two state fields

See the [complete document](history/pcm/v90equprocess.md).

### state 0 — `RESET`, `0x3a0cb`

See the [complete document](history/pcm/v90equprocess.md).

## F5. The four re-convert blocks

See the [complete document](history/pcm/v90equprocess.md).

## F6. Epilogue, `0x38ea7`

See the [complete document](history/pcm/v90equprocess.md).

## F7. What a suite has to drive

See the [complete document](history/pcm/v90equprocess.md).
