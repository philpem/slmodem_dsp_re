# Two source value-owner controls after UID tracing

Baseline9e20a346. No flags/types/layouts/prototypes/constants change. Review
all full-TU bodies and metadata, not only grade0 membership. No source syntax
uniqueness is claimed; scheduled pre-store loads need not be explicit captures.

V92BitsToSymbol::setSymbolsBlockSize is108B BYTES48. Blob loads symbolsDone
before publishing n, compares/subtracts n in EDX, then copies its remaining
value to ECX for the modulo/divide paths. Ours uses ECX throughout and omits
that copy; alignment also differs. Retained code calls the visible query member,
which GCC inlines; that fact alone cannot identify the original source factoring.
Five cells: unchanged helper call plus four explicit query controls crossed
between stored-member/incoming-parameter block count and ordinary/early
symbolsDone capture. Explicit query is exactly the existing helper arithmetic,
including multiply-before-divide on the divisible arm and wrapping unsigned
arithmetic. Do not merge the two arithmetic forms; they differ on overflow.
No call intervenes in the setter, and symbolsDone cannot alias symbolsBlockSize.
Original flags and frame macro remain, helper itself stays unchanged. This
bounds source dataflow factoring versus current inline context, not original
profile or an assertion that the author duplicated the helper.

V92EchoCanceller::resetEchoHistory is62B REGALLOC. Original length value is
stored to echoLength and reused as the loop bound; filterLength's initial
allocated color differs and some subsequent rows are split at peephole2.
Prior source-definition reorder failed and remains closed. Four cells cross
explicit/direct params-pointer access with member/local unsigned length value
ownership. Pointer load may be scheduled before historyIndex's store without
an explicit source capture; the two are distinct members. Keep pointed-data
read after the historyIndex clear. Local length has echoLength's existing
unsigned type and feeds both unchanged member store and clearing loop. Float
writes cannot alias the unsigned length under the retained aliasing profile.
No lifecycle or recovery claim for invalid buffer aliases/oversized bounds.

Nine full-TU cells. Require raw baselines, data/binding and every bystander
review; examine lreg/greg and peephole/rnreg for actual transformation. Misses
close these finite crosses; no operator/width/return synonyms or score fitting.
Only exact supported source candidates can be adopted, with production census
and batch-end make phase. No mutation/fuzzing execution.
