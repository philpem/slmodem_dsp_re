# V34 fixed groups and sequential extraction

Pin f420cd89, complete v34shell.c, unchanged headers and retained build-config.
Use Gentoo GCC 3.4.2-r2, its executed assembler and mandatory bug define.
This revisits supported partial adoption; it does not reopen declaration-order
or profile permutations. Parent integrates any supported source change.

Original putFrame has sixteen fixed group calls reading each word immediately
before its callback. Test the existing fixed-group/int-width control alone.
Original getFrame acquires callback once at579dd and all six calls use the saved
stack slot. Test expanded groups plus entry capture independently.

A fresh original operation witness is its successive extraction chain:
57ab6 shifts one, 57acf shifts small, 57ae7 shifts w. Retained source instead
shifts independently by1,1+small,1+small+w. Test successive masked shifts while
preserving all table indexes and callback/publication/read boundaries. The
equivalence domain is legal field widths0..16 and small1..2 (last small0..2),
with total shift1+small+w<=31. No claim covers malformed out-of-range widths,
negative table indexes, arbitrary callbacks or concurrent publication.

Cross sequence off/on with nb short off/on and pos short off/on only in the
expanded/captured getFrame. Original nb comparisons use words at57a33/57a3d;
pos uses signed word comparisons at57a24/57b01/57b6e/57bd4, plus sign extension
at57a4e. These justify independent width axes; small/small_last stay int.
Short pos changes behavior for malformed initial unsigned values>=32768 and
wide arithmetic overflow; original word operations, not broad source arithmetic
equivalence, decide those boundaries. Existing component/lifecycle fixtures
must pass before adoption.

Ten complete-TU cells: raw baseline, put expansion alone, eight get controls.
Prediction: fixed groups restore18/6 static callback counts respectively;
capture restores one get callback acquisition; sequential graph removes the
absolute sum-shift computations and recovers original successive shift rows.
Falsifier for sequence: final machine graph still uses summed absolute counts
or introduces contradictory buffer reads/publications. Width support requires
actual word comparisons, not size improvement. Complete exactness is a separate
verdict; partial adoption requires supported operations and final period gate.

Audit all12 common TU bodies, symbol metadata, named and allocated nontext,
canonical relocations and callers. putFrame expansion previously changed
decodeDepth through TU history, so that collateral is explicitly reviewed.
No src/include edits, fuzzing, mutation execution, register forcing or profile
changes in this experiment. Preserve invalid runs and report denominators.

## Buffer lifetime follow-up (declared after the ten opening cells)

All opening cells compiled/raw replayed; zero exact gains/losses. Successive
shift cells recover four destructive shift chains but still reload bitbuf from
its owner at every group. Original first group acquires the unshifted bitbuf
at57a96 and retains it in stack1c at57aa3. Later no-refill groups use stack1c
at57b19,57b7f,57be5. Refill paths refresh it at57d86/57d93 (second group),
57d64/57d71 (third),57d42/57d4f (fourth), before respective bitpos publications
at57d8c/57d6a/57d48. First group refill57d24 publishes57d28 then jumps to the
common first acquisition57a96. Wide split's special callback57cba remains
separate, return discarded and bitpos reread57cbe; its buffer read57cc5 must
remain untouched. Initial refill loop57c7a only updates position, not a cached
buffer. These are unambiguous original read ages.

Two-cell follow-up uses opening sequential-int control as its existing
reference plus one new buffer-capture control: acquire after first group
callback/publication; retain only between later group callbacks; refresh
immediately after each later callback and before bitpos publication. No capture
crosses a callback without refresh, and no wide-path or initial-loop operation
changes. Eleven unique total TU cells/132 verdicts (the follow-up reference is
already counted). Prediction: owner buffer loads disappear from later normal
group blocks and occur in each callback refill arm, preserving cached values
for no-refill paths. Falsifier: stale buffer crosses a refill, refresh/publication
ages disagree, or a new unconditioned owner read appears. Judge graph/read ages
and complete TU collateral rather than byte distance.
