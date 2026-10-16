# Class1 null-state residual schedule

Baseline `d9995470`, `_tx_nulls_state` 435 bytes, BYTES19. The complete
original and retained bodies agree except for a small argument-preparation
region. Original loads incoming slot five at `0x9d09b`, before the state
publication, and stores argument two before argument three at
`0x9d0ae..0x9d0b2`. Retained loads the incoming slot after state publication
and reverses those two outgoing stores. The previous pointer-capture control
raw-merged its baseline and stays closed.

Declare two complete-TU compilations: raw unchanged control and unchanged
source with diagnostic `-fno-schedule-insns2`. Save every RTL stage. Locate
the incoming-slot load and state publication by UID and memory identity,
then determine whether late scheduling creates the difference. Prediction:
if that pass owns the differing ordering, the pre-scheduler graph and
diagnostic output agree on it. A different full-TU result is not a licence
to retain a per-file flag. Keep every body and any lost exact matches visible.

There is no independent new source-width, owner or API witness here. Do not
change callback signatures, permute declarations, force registers or repeat
local captures merely to move these instructions. This diagnostic either
locates the next responsible stage or closes this reserve for this iteration.

## Result

The unchanged full TU reproduces raw. Both cells compile, covering 12 shared
body verdicts; data, symbol metadata and nontext relocations agree. Removing
late scheduling worsens the target from BYTES19 to BYTES84 and loses the
existing exact scrambled-ones and silence-before-scrambled-ones bodies.
It is diagnostic only and is not retained. Production cDATAtx.c is untouched.

Initial RTL acquires `word4` as UID7 before the state publication UID81.
Global allocation/reload in `25.greg` replaces that carrier with a reload
UID268 after UID81; this already disagrees with the original before late
scheduling. Disabling the late scheduler does not restore the original load
boundary or argument-store order. The UID/memory records for every saved
stage are in `build/recovery-packers-next/null-uid-trace.json`; complete dumps
and command/object controls live in `build/recovery-null-state-schedule/`.

Close this reserve with no source adoption or exact gain. The next experiment
requires an independently supported parameter/use or owner boundary that
changes reload pressure. The earlier local pointer capture remains an inert
negative control, not a reason to change the shared callback API.
