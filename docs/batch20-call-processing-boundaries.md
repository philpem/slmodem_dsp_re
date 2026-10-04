# Original call creation and processing boundaries

Baseline93d7eee1. Blob call_create610B has two omitted diagnostics: entry
`call: create...\n`, and non-native-rate `call: create RC: %d <-> %d...\n`
with srate and8000 before creating converters. Its processing helper retains
CALLPROG_Progress's message across output resampling, then compares the old
remembered message, prints `call: process: msg %d --> %d\n` on changes, and
stores the new message. Current source overwrites it before resampling and
omits this diagnostic. This changes observable callback state even at level0.

The inlined processing arm additionally reloads dp->self after calculating
buffer arguments from the outer owner; the retained helper receives only the
cached owner. A dp plus explicit buffer-arguments helper can express that
boundary without pointer casts or invented alias types.

Declare untouched production baseline plus eight complete-TU controls: the
already-proven deletion diagnostic fixed in all eight, crossed with creation
diagnostics, delayed message capture/change diagnostic, and dp-owned helper
with explicit buffers. No parameter/declaration/register permutations. Expect
only create/delete/run canonical bodies to change; audit data strings and
operations-table targets even where layouts move. Stop the family on misses.

The first nine cells retain delete's158B gain; create diagnostics reachSIZE2,
and message+owner boundary reachesSIZE2. Independent create evidence now
bounds a second eight-cell product: unsigned S56 mode comparison (reference
SETBE versus signed SETLE); cfg placed after dialstr in the declaration block
(reference frame cfg+0x10 and dialstr+0x20, current cfg+0x50/dialstr+0x10);
first S56 getter reads st->modem after the external memset rather than the
cached modem formal (reference member reload). All original diagnostics are
fixed; no arbitrary local-name or register variants. Other bodies must remain
identical to the corresponding restored-diagnostic full TU.

The processing candidate also exposes independent queue schema errors: the
reference uses unsigned modulo192 and unsigned clipping comparisons, whereas
current signed head/tail emit signed division and JGE. Buffer-half offsets
are added as bytes in the reference; current short-array /2 arithmetic adds
rounding/sign-correction instructions. Bound four further complete-TU cells:
retained signed cursors versus unsigned head/tail header overlay, and current
sample-indexed buffer expressions versus direct byte-offset addressing.
Restored diagnostics/delayed message/dp-owner handoff remain fixed. Count
stays signed (the reference uses JG for its >=96 test), and active remains a
byte offset without a type claim. Header layout and API remain unchanged.

One independent factoring control remains: reference dial-string selection
carries a single selected-string owner across its digit/length/prefix arms;
the inlined pointer-return helper introduces an extra move on the unchanged
string arm. Declare retained helper versus direct creation-local selected
string with the same two guards and copy, crossed only with retained or
explicit unsigned S56/S16 comparisons (reference SETBE/SBB). All diagnostic
restorations fixed, no other declaration/flag changes. This tests factoring
and actual predicate semantics, not arbitrary virtual-register assignments.
