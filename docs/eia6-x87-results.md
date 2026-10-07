# Live x87 conversions: source mode and late physical death

PR278 merged2ca02aec; production1070/1852 exact,116851 exact original bytes.
No production source/header/flag/mutation metadata change. PR263 and issue22
writes stay excluded. This pass traces EIA6, then tests two small independent
transfers. All valid builds use retained Gentoo3.4.2-r2, its executed selected
assembler2.15.92.0.2 and reproduction define last.

## The new discriminator

GCC's stack pass strips FIX before selecting its source-register handling.
A live XF source with spare x87 capacity is duplicated and given REG_DEAD;
the final SI emitter therefore uses FISTPL. Live SF/DF sources bypass that
rule and can use FISTL. The original EIA6 FISTL follows one load/multiply at
occupancy1 and preserves the input for subsequent uses. This independently
constrains its producer against live XF under the corroborated rule, leaving
SF/DF rather than uniquely selecting a C type. DI conversions always pop
for a separate hardware reason and are outside this discriminator.

This constrains RTL mode under the corroborated retained profile. An unknown
long-double ABI option or TU-specific profile could map a C declaration to
a different mode; this pass does not recover or change those options.

The proof reads three hash-pinned official GCC3.4.2 files and corroborates
the live-XF transformation in four actual Gentoo controls/12streams. The source
rule is stock GCC, not a claim that Gentoo patches are byte-identical. Original
operand/lifetime/stack capacity evidence is required at every transfer site.
[Mechanism, hashes and replay](gcc-x87-eia6-mechanism.md).

Two other forms are decided earlier: register zero folds to SF memory at
20.combine; register scale folds into widened SF memory between22.regmove
and24.lreg. Reg-stack does not cause those operand choices. Do not infer that
the original logical value died merely from a final physical REG_DEAD note.

## Diagnostic replay and tree limitations

Four closed EIA6 controls replay with translation-unit tree dumps. All four
objects reproduce their previous raw object bytes. An initial attempt with
`-fdump-tree-all` was rejected by GCC3.4.2; preserve/exclude
build/eia6-x87-trace-invalid-tree-all and its log. The successful
`-fdump-translation-unit` trees identify the target declaration but omit its
body in all15valid EIA6 cells. No tree-body or SSA evidence is claimed;
operation/lifetime observations come from the nonempty RTL streams.

## Bounded EIA6 source controls

The mode witness reopens a question that the closed copy/predicate family
held fixed. Six complete TUs cross coherent XF/SF/DF producers with retained
or witnessed expanded-copy/unequal graph. Matching initializer/whole lifts,
absolute intrinsic and sign literal follow that precision; original float
constants and separate xf publication remain. All SF/DF recover FISTL.

| Producer and graph | Bytes | Complete grade |
|---|---:|---|
| XF retained |604|SIZE186|
| SF retained |608|SIZE182|
| DF retained |604|SIZE186|
| XF expanded/unequal |792|SIZE2|
| SF expanded/unequal |790|BYTES260|
| DF expanded/unequal |790|BYTES246|

Five further declared controls test the two coherent candidates against
ordinary default-double math:10000.0, sign0.0, fabs, zero0.0. A float-valued
constant pool does not alone establish source expression precision:10000.0
is exactly representable in SF even when the multiplication is DF.

SF with default math recovers all three disputed forms, FISTL/FMULP/FCOMPP,
at790B, but BYTES339. Its scale load is scheduled much earlier than the
original, its magnitude/fraction conversion order differs, and other operands/
homes/copy scheduling remain different. DF default math remains790B BYTES246.
All forms/size matching is partial evidence; none is a byte-exact gain.

Audit15valid EIA6 cells, including4diagnostic replays,240live body grades,
150single RTL streams. Original root patterns, modes, death notes and preceding
UIDs are recorded. Every bystander, binding/import/export/BSS/nontext relocation
is unchanged. Only some predicates add one anonymous zero-float constant;
all other allocated data is unchanged. No period differential claim for these
unadopted hypotheses.

REG_UNUSED notes are recorded alongside deaths. Every selected final floating
root maps by UID to its -dP assembly packet;0unmapped final operations in15
cells. Compiler snapshots/dependency prose are excluded from instruction
pattern parsing and duplicate UID streams refuse interpretation.

Replay tools/eia6_x87_trace_reproduce.py, tools/eia6_x87_type_reproduce.py and
tools/eia6_x87_default_math_reproduce.py with their matching docs/*-domain.md
and `--baseline-dir build/production-before`, then tools/eia6_x87_audit.py.
Recover previous controls under build/eia6-prior-controls from the PR278
replay. Hash-pinned sources can be fetched by gcc_x87_eia6_mechanism.py
`--fetch-source`. The audit refuses changed raw diagnostic controls, missing
roots, duplicate UID streams, unexpected data and empty conversions.

## Two small transfers

Read-only screen:299source paths minus16V34 and sharedvpcm.c exclusions gives
282TUs;44explicit-long-double bodies,29unique emitted mappings,15helper/
unemitted mappings,0ambiguities.14nonexact original<=800B;4SI nonpop mismatches
including EIA6positive and closedVpcmgetUinfo. Two fresh350B beta setters
have live first FISTL at occupancy2 with no earlier calls. Their old four-cell
whole/shift-lifetime domain held types fixed and remains closed.
[Scope, original graphs and replay](gcc-x87-transfer-shortlist.md).

Three new complete Equalizer TUs hold later F11353 MMX casts and every shift/
log/field/guard fixed, varying only diagnostic scaled XF/SF/DF and matching
absolute intrinsic. Both SF/DF recover FISTL and reduce each setter359→351B
versus350original, but neither is EXACT. The masked shift still introduces
AND31 and publication/order/home differences remain; do not remove it to fit
the object or read SIZE1 as one differing byte. No arbitrary types/declarations/
slots/profile controls follow from the remaining gap.

Audit3cells105live body grades,33unchanged bystanders, all data/BSS/bindings/
nontext identities and exact MMX source tails. Replay
tools/beta_x87_mode_reproduce.py with docs/beta-x87-mode-domain.md and retained
baseline, then tools/beta_x87_mode_audit.py. No source adoption.

## Outcome and next discriminator

18valid complete-TU builds345live body grades,0gains/losses, including explicit
replay/overlap controls. Production remains1070/1852. No source changes require
a new period run; no fuzz/mutation runtime or modern portability claim. New
source families are closed absent a further independent original witness.

This unlocks a reusable mode/lifetime discriminator for source types, and
separates combine/reload operand choices from x87 stack conversion. It does
not establish the original C declarations uniquely, or a global byte ceiling.
EIA6's remaining discriminating diagnostic is the scale-load/magnitude/fraction
scheduling graph before stack conversion; beta's later mask/publication graph
is separate. Hold witnessed producers fixed when tracing those graphs rather
than restarting literal/type spelling sweeps.

Final make refs exit0:14383references/2948finding headings,0unresolved/held/
stale;285static suites10038anchors,0detached/nonunique/no-op/wrong-arm. No
mutation execution. All8new Python tools parse; live EIA6/beta audits and
source-rule/transfer detectors pass on their named positive controls.

Follow-up F11852–F11853: independent combine/use, early magnitude lifetime
and actual tail-reload edge evidence closes setParamEia6 strict EXACT790B;
see [the later full-TU controls](eia6-scheduler-results.md). The earlier
finite-domain measurements above remain historical, not a global ceiling.
