# V34 timing prefilter state and product boundaries

Local pre-compile declaration on5fac8659, findings F11705 reserved. Prior
function-emission-order family is closed (F7800): the26-function order already
matches and timing-prefilter's nonexact body precedes nonexact equalizer cleanup.
That is not justification for another emission-order permutation.

Reference V34TimingPrefilter167 bytes, baseline186. The blob keeps pre_state's
base in one dedicated cursor, saves/replaces slot0 then slot1, and consumes each
packed carry's real/imaginary products before restoring its old state. Current
source accesses t->pre_state[k], saves both old values before both stores, and
keeps each original packed carry through anonymous high-half products. Reference
imaginary accumulator remains in a register, whereas our two accumulators spill.
The stack references alone do not establish an allocator cause.

Before compilation, fully cross four independently bounded two-valued axes:
1. Direct member-indexed state vs explicit int *state=t->pre_state local,
   retaining the indexed k/k+1 accesses (no advancing cursor or counter change).
2. Grouped old0/old1 reads then both stores vs old0 read/store then old1 read/store.
3. Anonymous high-half products vs carry0/carry1 in-place >>=16 then *=c0/c1,
   after each carry's low-half product and before restoring the old state.
4. Both coefficient locals read at loop head vs reading c0 after state exchange
   and c1 after first carry's two accumulator updates. Products/coefficient
   indices/counts/wrapping additions and final packing unchanged.

Sixteen complete v34filters.c cells, including raw baseline. Predict local state
base changes address lifetime, per-slot exchange restores read/store sequence,
in-place carries alter combine's high-product lifetime before allocation, and
coefficient load scope avoids keeping c1 live through carry0. The complete
cross may recover the body. Falsifiers: no initial/combine lifetime change,
raw baseline mismatch, metadata/data/relocation drift, exact sibling regression,
or no exact candidate. Score every body; a preceding function can legitimately
move an unchanged cleanup body's peephole scratches. Do not adopt by net score
alone, and do not freeze a component merely because an intermediate cell loses.

Gentoo GCC3.4.2-r2, saved complete flags, mandatory bug define appended last,
actual executed assembler identity, -da diagnostics and full-TU review.
No header/ABI/flag/padding changes or fuzzing/mutation execution.

## Measured cross and next discriminator

All16 valid cells preserve13/26 exact except the four state-base / early-coefficient
cells, which gain unchanged V34EqualizerCleanUp. Initial-to-postreload cleanup
instructions are unchanged apart from compiler metadata; first actual difference
is peephole2's HI constant scratch (AX instead of CX), then rnreg's two call
argument scratches. This is a whole-TU carrier, not cleanup source recovery.
Late coefficients plus local state base recover the loop's persistent imaginary
accumulator and address/product schedule, but leave169 vs167 bytes. Exchange
and in-place axes cannot close this remaining initial accumulator/spill placement.

Before any new compilation, bound a five-cell initialization discriminator:
raw baseline, state-base positive sibling control, state-base/exchange/late
negative target control, that combined source with imaginary accumulator defined
before real, and that combined source with a shared `acc_re = acc_im = 0x2000`
initialization. The blob initializes the persistent imaginary register first and
then writes the real accumulator directly to its eventual stack slot. Prediction:
initialization changes its constant temporary/lifetime before allocation and
may explain the retained real-first constant copy and reversed spill layout.
Both new spellings form an exhaustive two-candidate local discriminator, not
permission for further declaration-order or register-name enumeration. Neither
matches means this family closes; an exact-set gain alone does not recover the
prefilter. Retain full-TU and raw baseline controls, actual saved commands and -da.

## Outcome and adoption boundary

The two initialization candidates both miss. No further accumulator declaration
or spelling domain is authorized by this result. All 21 prefilter cells and 16
rollback cells pass the complete-TU audit: 26 functions, 48 named data objects,
bindings, allocated nontext bytes and canonical relocation targets. Jump-table
references into .text are compared as unique containing function plus offset,
so legitimate preceding-body length changes are neither errors nor ignored data.
Sixty selected stage records parse; four unchanged cleanup-stage controls and
two positive scratch-selection controls fire as predicted.

Adopt only the independently observed local state-base pointer. The reference
forms t+0x74 once and indexes this state base; our original indexes the parent
object with the field displacement at every access. This source boundary is
supported without the exact-set score. The unchanged cleanup gains EXACT 48/48
through the translation-unit peephole scratch cursor. Four source cells yield
that gain, so neither a unique original spelling nor prefilter completion is
claimed. The prefilter remains 186 versus 167 bytes. The late-coefficient control
is closer (169), with the reference persistent imaginary accumulator and product
schedule, but is retained as an unresolved source lead, not selected on size.
The cleanup gain does not freeze the coefficient-order/profile question.

Replay from historical baseline (archive its 300 production objects first):

    python3 tools/gcc3_v34_rewind_reproduce.py --domain docs/v34-rewind-boundaries.md --baseline-dir build/production-before
    python3 tools/gcc3_v34_prefilter_reproduce.py --domain docs/v34-prefilter-boundaries.md --baseline-dir build/production-before
    python3 tools/gcc3_v34_prefilter_reproduce.py --initialization --domain docs/v34-prefilter-boundaries.md --baseline-dir build/production-before
    python3 tools/gcc3_v34_filter_boundaries_audit.py

The tools compile the complete historical TU, enable reproduction bugs after
configurable flags and preserve every compiler command, dump and source/object
hash. A clean checkout needs the baseline objects built at 5fac8659 and supplied
through --baseline-dir; the adopted object is deliberately not a raw baseline.

Production verification: fresh 300/300 merged-baseline controls; after adoption,
299 unchanged and the sole v34filters object raw-identical to the measured
state-base candidate. Whole-tree exact set 934→935/1852, exact bytes
96,367→96,415 (+48), zero losses. Using the same recovered 300-input order,
partial-link positioned equal bytes 68,680→68,679 /943,398; exact relocation
records 1,026/18,317 and symbol records 394/2,907 unchanged. Both whole-object
comparisons remain DIFFERENT. The one-byte positioned loss is retained evidence:
a function-normalized gain is not a whole-object byte-exact gain.

Validation: make phase J=4 passes 388 period differential tests, 0 failed,
plus structural/provenance gates. Existing t_v34ec exercises prefilter outputs
and whole-state transitions; t_v34eq checks cleanup contents against the blob.
Static anchor verification is measurement only: no mutation/fuzz execution.
The 37 valid cells contain 33 distinct sources and 21 raw objects; repeated
controls and compiler-canonicalized spellings are included in the denominator.
