# V8 stability word carrier and zero-reference guard

Precompile four complete V8global.c cells at856c1ecb: retained int/short delta
carrier crossed with retained/removed zero-reference guard. Original196B versus
retained152B directly executes signed division, then TEST AX/sign branch and
word negation, versus retained zero guard and int absolute-value recognition.
Original arithmetic is duplicated after refresh branches; no forced duplication
or statement permutation. Guard removal is diagnostic until input-domain audit:
source itself documents original division by zero, v8_rxinit sets reference512,
reference then refreshes from gain, and existing component fixture initializes
positive reference. These facts do not prove every gain refresh is nonzero.
A short carrier also differs at delta=-32768, preserving word overflow before
comparison. Treat this as original source/observable word semantics, not just
register fitting. Preserve all other functions, data, symbol metadata and flags;
no runtime/harness or mutation execution. Stop this crossed family on a miss.

Four-cell result152/154/157/177B versus196B, no strict gains. Unguarded short
recovers duplicated division and AX sign branch, but keeps comparison carrier
in EAX. Independent original operand witness: original copies EAX→EDX before
branch, and sign-extends negated AX→EDX while retaining EAX quotient. Predeclare
three-cell follow-up retained baseline, unguarded int predecessor, predecessor
with explicit `(short)delta < 0` predicate. This tests width at comparison rather
than short owner; no other edits or register carriers. Stop on miss.

Predicate follow-up176B versus196B, no gain; both word-owner and predicate-use
controls recover the word sign branch but not the complete original layout.
Close tested guard/carrier/predicate families. Seven full-TU controls preserve
all13 canonical symbols/four tables and eight exact functions; unchanged raw
baseline matches saved period object. No adoption: removal alone has an
unproved refresh-to-zero lifecycle difference; word predicate and carrier have
the signed-short minimum overflow difference separately documented.
