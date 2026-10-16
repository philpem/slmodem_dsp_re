# Spectral display sign carrier

Four complete-TU cells cross the result type of static `sign_of(long double)`
and its display cache: char/char, int/char, char/int and int/int. The original
four sign arguments are formed by full-width SBB/AND/ADD immediately before
publication; the retained body captures a byte and later sign-extends it.
The seven comparison spellings in F5823 settled the floating operand type;
this domain preserves that comparison, its operand order, all floating
arithmetic, call order and constants. It investigates a separate integer
carrier boundary, without asserting a unique original helper signature.

Require raw baseline reproduction, complete function/data/export/relocation
review and examination of all four sign-producing native sequences and early
RTL. Both possible sign values fit either carrier, including unordered/zero
comparison outcomes. No option, order or register search follows a miss.

After the width-only cross, inspect the original comparison's lifetime: all
four sign comparisons follow the fractional arithmetic, whereas both wide
cache cells retain an early comparison and emit FCOMP rather than FCOMPP.
Expand to three source factorizations crossed with both result/cache widths
(12 labelled cells, 10 unique sources because direct arguments have no cache).
Controls are retained assignments, sign assignment last, and direct helper
arguments. This is the original late comparison witness, not a size-based
permutation search; no other statement ordering is in the domain.

A separate original operand witness is the double scale load: first three
blocks use FLDL .rodata.cst8, the fourth rematerialises the same double on
the stack. All current cells instead load XF scale with FLDT .rodata.cst16.
Cross only coherent DF rather than XF fractional subtraction intermediates,
retaining float input and double scale: 24 labelled cells /20 unique sources.
This is the same mode question previously closed for adjustHalfBaudBpfGain,
but that different helper's null is not a measurement of this function.
Expected constant-pool changes must be itemised rather than hidden.
