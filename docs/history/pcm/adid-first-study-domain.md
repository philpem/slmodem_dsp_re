# ADID first study: helper factoring and first-use counters

Baseline9aa118e1, production1080/1852 EXACT. Complete current ADID TU/headers,
fixed Gentoo3.4.2-r2 profile and mandatory bug reproduction. Use saved initial,
combine and reload dumps, no full-dump ICE or compiler option changes.

Original porcessFirstStudy779B retains all loops and inlines the unsigned-count
reciprocal/variance/mapping operations. It forms variance then adds the half,
and retains the half across mean updates. Retained source explicitly repeats
the operations of the earlier updateLinMappMeanAndVar helper, but uses0.5f
and opposite multiplication operand spelling for variance. The standalone helper
already reproduces original code with0.5. Human factoring through that helper
is a distinct inline/caller-cost hypothesis, not merely local statement order.

Six complete-TU cells: explicit float-half, explicit double-half, helper reuse,
each with retained counters or phase/code declarations at first use after the
opening diagnostic. Helper reuse preserves code/reference exclusion and lets
the helper perform the existing zero-count guard. Explicit double-half changes
only that mean conversion, not clamps, trace arguments or variance arithmetic.
Counter declaration controls change scope/creation age, not width or operations;
no permutation search. Preserve both clamp/unordered selections, threshold
evaluation, reference-code exclusions, flag/count rules and clearing read ages.

Prediction: helper's exact arithmetic and definition scope can recover caller
inlined operations and change live ranges; double-half alone isolates expression
mode from factoring. Full-TU bystanders, exact sets, data/metadata and call graph
must be reviewed. A retained partial candidate needs original instruction/operand
or boundary support plus period validation, not a nearer size alone.

No fuzzing/mutation execution, global profile changes or forced registers.

Six-cell follow-through: late counter declarations raw-inert; helper and explicit
double-half produce identical full objects. Double-half recovers original inner
24-instruction mapping/variance block and frame size, but FirstStudy795B versus
779B remains. Opening reuses2.5 across clamp and debug calculation while original
multiplies from memory separately; original upper clamp retains a different join.
Bound five opening cells: raw9aa baseline and four mean-double cells crossing
threshold2.5f/2.5 expression mode and ternary/explicit upper-clamp guard. Preserve
ordered/unordered predicate by writing !(max >= value), not reversing its sense.
Prediction: distinct threshold expression mode can prevent common evaluation
with the float2.5 debug argument, while guard tests join factoring independently.
Do not infer literal precision from the constant load width alone. Use a copied
raw baseline object/config in an isolated baseline-dir while the supported
mean-double change is under integrated validation; no cache drift interpretation.

Opening controls all remain795B/SIZE16. An initial score-only interpretation
called threshold mode and upper guard inert; full-body audit below refutes the
upper-guard part. The other2.5 use reaches a signed
short diagnostic through double-half addition; its multiplication mode is an
independent consumer. Complete that finite literal cross with five cells (raw
baseline plus threshold/diagnostic float/double multiplications, mean-half
double fixed). Distinct arithmetic consumers, not source synonyms or permutations.
Prediction: diagnostic DF multiplication prevents reuse of the threshold's SF
product and recovers original second memory multiplier. Preserve results only
after original operand/boundary review and deciding period gate.

Audit correction before interpretation: equalSIZE16 did not mean equal objects.
Threshold mode at the float consumer is raw-inert, but explicit upper guard
restores the original jae/pop join; diagnostic DF multiplication restores the
original opening evaluation sequence except that upper guard. Initial RTL has
SF/SF products for retained diagnostic versus SF/DF with the double diagnostic.
Compile the remaining crossed combination (two cells including raw baseline),
not another spelling family. Earlier emitted-inert assertions are rejected by
full hashes/operands; retain the measured discrimination and update findings.

Combined diagnostic/upper guard reproduces the original opening and inner block,
but original selection initializes n only after the alt flag admits the scan,
and retains separate byte-zero/byte-one publication arms. Retained source starts
n before the guard and an early continue ends the zero arm; GCC merges those
stores. Four controls (raw baseline, combined, n inside admitted scan, nested
selection with count in that scope) test those observed boundaries. Preserve
integer thresholds, flag exclusion, count updates and output paths. This is not
register-colour forcing or a claim that separate final stores imply unique source.
