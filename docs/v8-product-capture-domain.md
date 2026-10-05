# V8 product capture: declared before compilation

Base8e914a86, complete V8Dpsk.c, retained Gentoo profile, reproduce-bugs last.
No production source/header edits, runtime/fuzzing/mutation execution or #22
writes; PR263 at97e8a10c remains excluded.

Original first tap captures coefficient a[j] before the sample in both input
and history loops (78d7a before78d84; 78dd8 before78de2). Retained named sample
is already loaded first in01.rtl: prior crossed control sample UIDs102/151,
a coefficient UIDs106/155. This difference precedes allocation. Original later
products reuse the sample. Address operands and actual memory reads matter;
pointer loads alone do not establish coefficient capture.

Exactly four cells: raw baseline; repeated unsigned-index/input-cursor control;
coefficient-first product expressions with repeated sample spelling; a named
first coefficient captured before the retained named sample. Preserve product
and accumulator order a/b/c/d, types, arithmetic, decisions, run helpers and
sample traversal. No declaration/register/slot permutations. Ordinary memory
reads have no intervening writes; valid tap paths and sample values are unchanged.
Repeated sample spelling predicts common-load reuse by CSE; explicit ownership
separates an expression expansion effect from optimization. Inspect every saved
stage for real coefficient/sample load order, not only pseudo names.

Require raw baseline and prior control object repeat; audit all emitted bodies,
exports/binding, allocated data/BSS and nontext relocations. Complete all cells
and stop/reframe; a partial match/size fit is not adoption. Trace initial RTL,
first CSE, loop, combine, scheduling, allocation and final output. If both forms
converge, report that equivalence rather than trying more synonyms. Production
adoption requires complete exactness and a final combined period gate.

## Results and stopping point

All four cells compile and the raw baseline repeats. Prior index/cursor control
raw repeats PR272. Actual bytes: original1100; retained1121; prior control1057;
expression1057; named coefficient1057. The two coefficient-first forms are
full-object raw-identical, but differ from the prior target by36 positional
bytes at the same size. No exact gains/losses. Sixteen emitted-body comparisons
and16original-common verdicts preserve bystanders, symbol records/binding,
allocated data/BSS and canonical nontext relocations. No production adoption.

Initial RTL records coefficient before sample in both product variants. Repeated
inline sample loads are reduced by first CSE: source repetition does not force
repeated machine loads. The durable UID trace follows the first input tap's
actual HI memory reads; combine folds the word load into its sign-extension UID,
so tracing an empty/deleted original UID alone would misclassify elimination.
The coefficient-first pair remains ordered through31.bbro, then33.sched2 puts
sample first and that survives35.mach. The retained control starts sample-first
and stays so. This is an observed pass boundary, not reconstructed original RTL
or a complete account of scheduler costs, liveness or the inline profile.

Four cells ×29parsed instruction streams =116stage observations. The08.gcse
diagnostic contains duplicate UID streams and is explicitly unparsed; the
balanced instruction parser refuses it rather than silently choosing one.
No order claim is inferred from this omitted diagnostic. Known positives are
the initial coefficient-first and retained sample-first loads, with the final
sample-first transition; a register-only replacement is refused as a memory
capture. Initial two-loop evidence is retained in dumps; the durable pass trace
is specifically the first input tap.

The initial generator stopped before compiling any cell because identical
product blocks occur in both loops. Fixed replacements to the sample-specific
loop extent; the rejected no-cell run is preserved under
build/v8-product-capture-generator-refusal and excluded from all denominators.
The domain and candidate family did not change.

This expression/ownership family is closed without adoption. Next useful
discriminator: enable verbose scheduler diagnostics as measurement apparatus
on a repeated coefficient-first TU, require full raw-object equality, and
inspect GCC3's dependency/ready-queue decision for the two traced loads. Read
the corresponding GCC3 scheduler implementation before attributing the order
to an option or inventing an artificial source dependence. No volatile, alias,
register or storage workaround follows from this partial result.

Replay at8e914a86 using archived production objects/config:

```sh
python3 tools/v8_product_capture_reproduce.py --domain docs/v8-product-capture-domain.md --baseline-dir build/production-before
python3 tools/v8_product_capture_audit.py
```

Artifacts build/v8-product-capture/results.json and audit.json hold full commands,
hashes, complete-TU comparisons and the per-stage load UIDs. No runtime gate is
rerun for this tools/findings-only pass; no behavioral/reachability claim.
