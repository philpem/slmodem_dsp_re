# V34 transmit snapshot short-local cross

Baseline7edfb734 complete V34hshak.c and current headers; Gentoo retained
flags unchanged. Header overlay only for v34hstx1_arms.h.
Original snapshot retains both six-entry loops. First index increment at
672ed movswl %bp,%ecx and compareword at672fa; second index increment67472,
cwtl67473 and compareword67474. Original error-difference intermediate is
narrowed at67316/6732a and tested word-sized at67319/67341. Our declaration
is int sum=0,i,t with explicit casts at assignments to t.

Four cells cross short i and short t, leaving sum as int. Prediction: short i
restores loop-counter narrowing; short t bounds original tests. Explicit t
casts may already make its axis inert. No loop expansion here: object visibly
retains the loops. Compare all common original TU bodies and emitted helpers,
full raw baseline, bindings and data. No other declaration/order/profile axis.
A whole target miss is not a recovered byte-exact function, and source widths
must not be adopted only for nearer size. No fuzzing/mutation execution.
