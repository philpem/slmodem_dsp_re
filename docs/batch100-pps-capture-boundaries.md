# PPS source pointer ownership and word-boundary controls

Precompile856c1ecb four cells. FPM_PPS_filter original caches coefficient-I/Q,
map-I/Q, history-I/Q, ring sym/I/Q and scale before count guard. Retained accesses
these owners after guard or inside loop. Cross explicit stable captures with
short increment-before-bound index updates. Original sign-extends incremented
ridx/widx before CMPW limits and SETL/NEG/AND; retained compares promoted +1
before narrowing. Use ordinary in-place conditional clearing as proved noce
lever. No unrelated declaration permutations or flags. Short counter overflow
is distinct original boundary; don't assume current promoted check equivalent
on malformed/out-of-range histories. Complete TU and all bystanders must pass.

First four: retained741B, narrow-only757B, captured-only829B, combined832B;
original753B. Neither original boundary alone suffices. Independent rail-loop
witness: original DEC/JNS and full CMP in all four loops, retained MOVSWL after
every short decrement and TESTW/CMPW. Original second-history cursor is first
cursor +2*taps, retained restarts from hist+taps-1. Declare sixteen-cell cross
of previous four cases × int rail counter (untruncated taps-1 initialization) ×
shared-wrap history cursor. No alternate coefficient order/register spellings.

Rail cross sizes (independent/shared wrap):

| Pointer capture / narrowed indices | short counter | int counter |
| --- | --- | --- |
| neither |741/725|707/691|
| narrow only |757/741|723/707|
| capture only |829/772|795/738|
| both |832/775|810/753|

Complete all-boundaries candidate753B equals reference size, but BYTES420;
alpha comparison rejects229 original versus230 candidate nonpadding instructions.
An exact size is not a grade1 match. Original source/copy/scheduling residual
is not bounded further by this result, so no arbitrary declaration-order search.

All20 complete-TU audits pass with explicitly accounted changes. Three common
symbols, no named data; all allocated nontext bytes/relocations and metadata
unchanged. FPM_PPS_free preserved. All eight shared-wrap cells change previously
exact FPM_PPS_init (source untouched), an emission-context bystander loss;
no other exact loss. Eight opposite cells preserve init. These are diagnostic
controls, not adopted production. No runtime/fuzzing/mutation runs.

Semantic limits: short-before-bound indices differ at word overflow, and shared
history wrap differs for malformed negative indices. Original operand evidence
bounds those alternatives, but no reachable lifecycle claim is made from this
object-only experiment. Cached pointers assume caller does not alias output
into pointer fields; no changed alias behavior is adopted without proof.
