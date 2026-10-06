# Wrapper unsigned arithmetic and ring-span capture: declared before compilation

Basee0052eec, full dp_wrapper.c and retained Gentoo profile, bugdefine last.
No header/layout/profile/forced-register edits, no V34/PR263 or #22 writes,
no runtime/fuzz/mutation execution. Old creation/rate-pair domains are separate.
Original run575B (ours573) uses JBE for three minima, JB for total<fragment,
unsigned DIV for both ring-wrap remainders, and reloads fragment/span after
external copies. Retained signed imin/span emits JG/IDIV and caches span at
entry. Initialized wrapper contract bounds fragment/ring indices to positive
small integers; caller count still has signed <=0 guard. No invalid planted
history or negative-fragment equivalence claim.

Exactly four full-TU cells: raw baseline; unsigned helper/local span; witnessed
loop-local span plus both post-copy divisor reloads; both. Preserve callback
result, counters, source sample traversal, ring wrap choices and copied bytes.
Do not infer field signedness uniquely from a promoted division. Header remains
unchanged; this source family uses ordinary unsigned local/formal arithmetic.
Require raw baseline repeat and all emitted-body/export/data/nontext-relocation
review. Complete cross then stop/reframe; only complete exact gains can be
retained after combined final period gate. No size-only adoption.
