# Value-owner source controls: two exact gains

Baseline9e20a346; source/headers/config equal merged736cf61c. Domains declared
before compilation: [SDM](residual-sdm-init-domain.md) and
[V92 owners](residual-value-owner-domain.md). Gentoo3.4.2-r2 production flags,
executed assembler2.15.92.0.2, mandatory reproduce-bugs define. Seventeen
complete-TU cells /134 shared body verdicts; raw baselines reproduce. Every
bystander, binding/visibility/import/export, allocated nontext data, BSS and
nontext relocation stays identical. No exact losses, no near-score adoption.

## SDM finite miss

Both original initializers are86B. The eight-cell cross leaves opening reg
clear/aggregate cfg copy intact, unlike prior opening-store controls. It tests
one existing short nbits capture and shift2-first derived-field computation.

| Cell, in each TU | Bytes | Strict result |
|---|---:|---|
| Baseline |86| BYTES18 |
| Shared nbits |107| SIZE21 |
| Shift2 first |104| SIZE18 |
| Both |109| SIZE23 |

Each control changes only its initializer; both bystanders stay raw-identical.
This closes the declared cross, not all possible initializer preimages. No SDM
source edit is adopted. Runtime semantics are not claimed for undefined shifts.

## V92BitsToSymbol::setSymbolsBlockSize

Original108B. The five-cell family preserves the distinct unsigned overflow
behavior of both rounding arms and keeps the standalone query unchanged.

| Setter query | Counter capture | Result |
|---|---|---|
| Existing visible helper call | Ordinary helper reads | BYTES48 |
| Explicit query using stored member | Ordinary reads | BYTES48, full object identical |
| Explicit query using argument | Ordinary reads | BYTES48 |
| Explicit query using stored member | Pre-store symbolsDone | EXACT108B |
| Explicit query using argument | Pre-store symbolsDone | EXACT108B |

The two exact whole objects are raw-identical. Adopt incoming n with the early
unsigned counter capture: store the block size on every path, compare/subtract
the saved counter, then execute the original arithmetic. Original load before
publish and separate remaining-count copy bound this lifetime hypothesis; neither
plain expansion nor parameter use alone closes it. In01.rtl/24lreg the winning
counter is a user-variable pseudo before the store, whereas ordinary helper
inlining loads an anonymous value after it. Full-TU exact9/10 to10/10; all nine
bystanders unchanged. This is a supported dataflow family, not proof the author
duplicated the query rather than writing a different inline helper.

## V92EchoCanceller::resetEchoHistory

Original62B; four controls:

| params access | Loop-bound owner | Result |
|---|---|---|
| Explicit pointer capture | Member | REGALLOC, raw BYTES12 |
| Direct member | Member | EXACT62B |
| Explicit pointer capture | Shared unsigned local | BYTES15 |
| Direct member | Shared unsigned local | BYTES15 |

Adopt only direct params access. The existing pointer local becomes a reg/v/f
pseudo in01rtl and24lreg, allocated AX in25greg; removing it changes allocation
and recovers the original colors. The compiler may schedule the direct member
load before historyIndex is cleared; distinct members cannot alias. The pointed
parameter value is still read after the clear. A shared length local is a separate
negative axis, not an improvement. Full-TU exact5/15 to6/15, all14 bystanders
unchanged. No layout/width/prototype/register constraint or flag changes.

The V90 counterpart's setter and query are already EXACT; no transfer edit is
made. V90SpectralShaper's pointer/reference anchor domain remains declined under
F7826; a new pointer-capture gain does not reopen that previously fitted domain.

## Static locators and gate

Initial targeted anchorcheck reports six nonunique locators after arithmetic
is duplicated and params capture removed. Repair four existing query-operation
locators with the entire unique query-body context and two echo reset locators
with their original unique length-expression context. Retarget two setter
locators to the same old-block/store-destination defects. Labels/count preserved.
Use fn (the supported trailing-name ownership assertion); earlier function keys
in the session-flag locators were ignored, so correct all six without altering
find/replace operations. Unique contextual anchors remain necessary: fn checks
ownership but does not constrain matching. No mutation execution or snapshot
rerecording. Targeted3suites/133anchors clean; full gate follows separately.

Production Gentoo300/300/0; both complete production objects are raw-identical
to the selected exact controls. New tools parse and the independent17-cell audit
passes. No portability claim. Replay:

```
python3 tools/residual_sdm_init_reproduce.py --domain docs/residual-sdm-init-domain.md
python3 tools/residual_value_owner_reproduce.py --domain docs/residual-value-owner-domain.md
python3 tools/residual_value_owner_audit.py
```

Artifacts: build/residual-sdm-init, build/residual-value-owner and their
134-verdict audit; config/commands/source/header/object hashes and all dumps
preserved. Negative controls remain separate from production.

Final production census: 1079/1852 EXACT, 118507 original bytes, +2 functions
and +170 original bytes against the 1077 baseline; zero exact losses.
`make phase J=8` exits 0: period differential 388 passed / 0 failed; structural
gate clean, including 285 suites / 10038 static mutation anchors and 14417
references. No mutation or fuzzing harness execution; no portability claim.
