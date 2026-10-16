# V34 putFrame fixed-group expansion and width cross

Baseline7edfb734, retained Gentoo flags and mandatory bug define unchanged.
Complete src/pump/v34/v34shell.c, current headers must match baseline.

Original putFrame584B at0x57fb0 emits sixteen indirect calls at fixed frame
word offsets2..17, with the fourth group's second call using small_last.
There is no four-group backedge. Retained source rolls these calls into a
four-iteration loop. Original nb comparisons are word-sized at58012/5801c;
small and small_last are sign-extended from words at581e7/581ee.

Eight cells cross fixed literal group calls, short nb, and short small/
small_last against retained forms. This is a finite independently witnessed
source domain, not call/declaration permutations. Preserve call order and
read each frame word immediately before its callback; callbacks may modify
later words. Prediction: expansion removes group backedge and recovers16
static callback sites; width axes recover original word comparisons/narrowing.
Falsifier: expansion alone does not reproduce these operations, or all crossed
cells miss complete byte identity. No score-only adoption; inspect all TU
bodies, bindings, data and canonical relocations. Baseline must match raw.

No flag changes, forced registers, fuzzing or mutation execution. Any adoption
requires final period/structural gate. Header snapshot width lead is separate
and not included in this domain.

Follow-up after the eight width/expansion controls (all misses, expansion
restores16 group calls): cross the expanded short-nb form with (a) main-path
wide-full predicate, matching original subtract/fallthrough versus alternate
load at58191, and (b) width defaults initialized before callback/frame/width
captures, matching original constant definitions preceding these captures.
Four cells; no unrelated statement permutations. Retain first matrix separately.

Further discriminating owner control: original putFrame forms a distinct
base at object+0xa00 (57fe4) and accesses span/remainder/accumulator/widths
through it. Retained shell map flattens this region. Test a header-overlay
union providing a typed 0x18-byte subobject, with the existing flat view
preserved for bystanders. Cross its use in putFrame with original constant
initialization timing, keeping expanded groups, short nb, and main full path.
This is an experimental geometry/owner hypothesis, not established original
structure recovery. Baseline header geometry and all TU bystanders must hold.
Four cells plus raw baseline; no pointer arithmetic source workaround.
