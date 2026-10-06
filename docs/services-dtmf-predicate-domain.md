# DTMF eager predicate and accumulator lifetime

Base240481e6. Original dtmf_test computes five acceptance relations eagerly,
using SETAE/NEG/AND. Retained && spells short-circuit evaluation and emits
conditional branches between them. Original three initial FLDZ copies also
precede mode threshold choice, representing max_lo, max_hi and sum live
from entry; retained zeros are introduced at separate later loops.

Four-cell complete-TU cross eager bitwise & of fully parenthesized comparisons
with early initialization of those existing zero float accumulators. Arithmetic
order, constants and all actual output stores remain. No reassociation, no
floating tolerance and no reachability claim for arbitrary NaNs; eager predicates
may change floating exception evaluation, matching original unconditional tests.
Audit standalone function and inlined dtmf_detect, all emitted functions/data.
