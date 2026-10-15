# ADID phase-grouping initialization and guarded-loop cross

Baseline860732a2 complete V90AutoDigitalImpDetector.cpp/current headers,
production1079/1852, retained Gentoo3.4.2-r2 flags and mandatory bug define.
Use initial RTL -dr, the established ADID workaround for -da ICE; raw unchanged
full TU must reproduce. No original-profile/flag changes.

Target uniteLinMappInfoOfUnsuspectedPhases original601B at41550, retained635B.
All six-entry initialization, five-leader grouping, inner scan, pooling and
publication loops remain in original. Aggregate9/11 backedges do not prove
expansion. Original group initialization41570..4157f precedes count/best local
initialization41586..415a0; retained assigns best/count before clearing groups.
Original outer grouping guard415b0..415c0 and pooling guard415f0..41606 send
accepted work to a body and share the loop increment on rejected paths.
Retained continue/OR forms replicate the accepted pooling increment.

Eight complete-TU cells cross delayed best/count initialization (count1 then
best0 after group clearing), positive grouping guard and positive pooling
guard. No declaration permutation, artificial dependencies or forced registers.
Preserve threshold-before-distance, >= skip on ordered-not-below, cumulative
count bugD283, uninitialized bestGroupD284, short pooled-count truncation,
empty-pool return, all arithmetic, operand order and output read/store ages.
Prediction: positive guards restore shared increments and late initialization
restores original initial definition age. Falsifier: all cells miss complete
identity or change original floating behavior. Target size is not adoption.

Compare every emitted body including nonexact bystanders, metadata/bindings,
named data and canonical nontext. No bystander-only adoption. Final period gate
required for any src change. No fuzzing/mutation execution or issue22 profile
intervention. Prior ADID reset/threshold/verdict domains are not repeated.

Follow-up after eight cells: both positive guards recover601B, and delayed
initialization narrows the literal residual147→61 different bytes. Original
variance finishes4164e before half-add41656; retained schedules half-add before
variance and adds x87 exchanges plus a rounded-value sign extension. Same-TU
updateUref already recovered original code through the independently measured
double-half expression mode (gcc3-uref-stack-results). Cross the three control
forms (baseline, both guards/early locals, both guards/late locals) with double
0.5 in the mean-to-short conversion; keep float mean/variance declarations and
short destination unchanged. Four cells (raw baseline plus three double-half
forms), not operand permutations. A SF constant load does not uniquely identify
literal expression mode because0.5 is exactly representable in both modes.

The double-half/late/both-guard control remains601B/BYTES59, with one extra
rounded-value sign-extension, plus exchanged best/count spill slots. The result
is used only in short mapping stores. Bound a further consumer cross: unsigned
short carrier *after* the existing signed-short conversion, or signed-short
conversion directly at the mapping store, each with float/double half. Five
cells including raw baseline; group/best initialization and both positive
guards fixed. Unsigned carrier does not alter signed float-to-short conversion.
Direct consumers may delay conversion past the loop guard; audit this difference
and do not assume the conversion is always executed or source-equivalent for
floating exceptions. Exchanged spill slots alone are not authorization for
count/best declaration permutations, and none are included.

Direct signed conversion at the store with double half removes the extra
promotion and recovers every instruction/operand except exchanged count/best
spill homes and the pooled-zero definition's placement (601B/BYTES10). Initial
RTL assigns count before best after group clearing; pooling assigns total
before fsum/fsq, whereas original machine order puts floating zeros before
total. Bound a four-cell *first-definition* order diagnostic: best/count
assignments at that same boundary, and floating/total zero assignments at that
same pooling boundary, plus raw baseline. No declarations move and no artificial
uses are added. This is a compiler-history hypothesis, not an inference of
original source order from spill addresses. Prediction: definition order can
explain exchanged homes while scheduler may still emit count's immediate first.
If it misses or changes unrelated bodies, close the domain; no larger
permutation search follows. Full byte identity/data/gate remains required for
source retention, not merely a restored slot mapping.
