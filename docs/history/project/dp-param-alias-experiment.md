# dp_runtime_create: field-type and alias-analysis experiment

Baseline: merged `11d23fd1`, Gentoo GCC 3.4.2-r2. See
`byte-fidelity-baseline.md`. No reconstruction source or retained flags changed.

## Evidence and hypothesis

`src/core/dp_param.c` has three shared functions: two exact and
`dp_runtime_create`, whose 298-byte body differs in 53 bytes. The reference
loads `dsp_info.clock_deviation` before several integer stores to the newly
allocated runtime. The retained build delays that load until after them.
Calls, sizes and the surrounding portions of the function agree.

The reconstruction declares this host field `int`. The vendored original
Smart Link header `third_party/slmodem/modem_defs.h` declares it `long`.
`third_party/slmodem/PROVENANCE.md` records the vendor-header provenance.
The reconstruction header itself documents the intentional substitution to
preserve the four-word ABI on LP64. Under the period i386 compiler both types
are four bytes, but their distinct alias types can affect instruction motion.

Prediction: restoring the host field's `long` type should permit the earlier
load if alias analysis causes the discrepancy. Disabling strict aliasing
should remove that effect. Competing explanations include other declaration
types and scheduling/source-order interactions; neither widths nor a smaller
byte-distance score alone establish the author's complete declaration set.

## Finite domain and validity

First matrix: source member `clock_deviation` as retained `int` or vendor
`long`, crossed with retained flags or `-fno-strict-aliasing`.

Follow-up: repeat both source-member types with destination member
`clockDeviation` changed to `long`, under both profiles. The destination
change is a diagnostic without independent source provenance, not proposed
recovered source. Together these exhaust the two-by-two type pair crossed
with the two profiles: eight cells.

All eight compiled. Experiments use isolated include overlays and unchanged
`dp_param.c`, not modifications to the shared header. The unchanged overlay
path reproduces the baseline object byte for byte. Complete compiler commands
derive retained flags from `.build-config`, explicitly select the published
Gentoo image, and append `DSPLIB_REPRODUCE_BUGS` through the shared helper
after configurable flags. Executed compiler and selected-assembler versions,
generated inputs, compiler logs, object hashes and per-symbol comparisons are
saved under `build/dp-param-experiment/` and `build/dp-param-type-pair/`.

## Results against the original

| Source field | Destination field | Strict aliasing | Exact / shared | Create differing bytes / 298 |
| --- | --- | --- | ---: | ---: |
| int | int | retained | 2 / 3 | 53 |
| long | int | retained | 2 / 3 | 28 |
| int | long | retained | 2 / 3 | 53 |
| long | long | retained | 2 / 3 | 28 |
| either | either | disabled, four cells | 2 / 3 | 53 |

Every no-strict-aliasing object is byte-identical to the retained baseline.
The type changes under retained flags do change emitted code, demonstrating
that the overlay and detector are active. Equal differing-byte counts for
different type pairs do not mean identical bodies: their object hashes and
full comparisons are retained separately.

Both exact bystanders, `dp_param_get` and `dp_runtime_delete`, remain exact
and baseline-identical in all cells. All cells retain the same defined symbol
names, types, bindings, visibility and sizes. `dp_runtime_create` stays BYTES,
not EXACT. The source-`long` control moves the clock load earlier, but remaining
register and scheduling differences are not explained by this finite family.

## Decision and next discriminator

This is a positive alias-analysis mechanism result, not a recovered TU.
The vendor `long` declaration is a credible source candidate independently of
its score. Changing the destination member has no comparable provenance and
is not justified by these controls. Do not add a per-TU no-strict-aliasing
exception: it erases the measured source-type effect and restores the miss.

No candidate was adopted, partially linked or differentially gated. The
latest retained-tree result remains the baseline's 375 period passes and
830 exact functions. There is no claim that a local header overlay validates
all consumers of that shared header.

The next useful step is a shared-header scope assessment of the original
host `long` declaration: compare all affected period TUs, their complete
exact-name membership, nonexact bodies and bindings before retention. The
remaining scheduling difference must be kept visible rather than treated as
solved. Do not repeat the exhausted int/long type-pair matrix.

A native LP64 `long` also changes this host ABI representation's offsets and
size. That is a separate portability consequence to record explicitly, not a
reason to reinterpret the original compiler's source. Any retained header
change needs a clearly defined ABI boundary; modern-compiler validation is
not substituted for the deciding Gentoo period differential gate.
