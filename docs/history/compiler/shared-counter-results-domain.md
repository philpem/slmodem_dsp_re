# Shared receiver counter results

Baseline04eee73f, production Gentoo config and mandatory bug reproduction.
V27RX_eq_train/V27RX_decision and V29RX_eq_train/V29RX_decision each increment
an unsigned-short counter and wrap0x8000 to0x4000. Original normal and cold wrap
paths feed the same sym_count store: V27eq0x9a12d, V27decision0x9a252,
V29eq0x9b82d and the analogous V29decision sink. The source stores separately
in each arm. This is the new F11877 source-sharing hypothesis, not a repeated
branch-order or count-width family.

Two complete TUs, four cells each: unchanged baseline, eq_train conditional
assignment, decision conditional assignment, both. Eight complete compiles.
Keep the existing increment local, unsigned comparison, field widths, restart
constants and all surrounding operand/use order. No new locals, captured
owners, constraints, volatile values, flags or structure edits.

Prediction: one conditional-value assignment changes branch-result pseudos
and/or removes repeated owner uses before allocation. Inspect24lreg/25greg
and final output; if all emissions merge, close this domain rather than trying
more syntax. Raw baseline equality, all emitted function grades, binding/data/
BSS/nontext relocation equality, exact gains/losses and changed bystanders are
required. Adopt only supported complete exact functions; no nearest-size cells.
No fuzzing or mutation execution. Final gates are deferred to batch end if any
source is adopted. PR263's files and issue22's profile work are excluded.

Post-measurement correction: following the cold wrap arms shows separate
immediate stores, with jumps after the normal stores. These four receivers
are therefore NOT common-physical-store nominations for F11877. The finite
arithmetic comparisons remain valid, but cannot support that source inference.
