# EIA6 floating value-use diagnostics

Base2ca02aec, retained1070/1852. Replay the existing four closed EIA6 controls
with additional GCC tree dumps, not new source hypotheses. Exact old source/
object hashes and raw baseline/current-control object equality required before
reading diagnostics. Retained complete Gentoo profile/bug define last and
selected assembler recorded. A rejected dump flag is apparatus failure; do
not modify reconstruction source to accommodate it.

Trace operations, operands and REG_DEAD/REG_UNUSED notes through initial RTL,
combine, global reload, postreload, flow2, sched2 and x87 stack conversion.
Track complete root nodes, excluding prose/snapshot streams and dependency
notes when comparing operation patterns. Identify a value by semantic root/
original pseudo provenance, not numerical hard register alone. Join -dP
assembly to UID where possible and report unsupported/unmapped items.

Known controls: literal expansion removes the copy backedge and unequal
predicate changes the zero-compare graph. These fire independently of
unchanged final grades; tracing must show nonempty fix/multiply/truncate/
comparison observations. Explain stock GCC source rules separately from
actual Gentoo dump observations. No new source, type/register/slot/profile
permutations, mutation/fuzz, V34/shared header/PR263/issue22 writes.

Only a new independent original operand/use/lifetime witness can reopen
source experiments. Otherwise publish the pass boundary and bounded
conclusion, without calling a size gap regalloc-only or exhausted source.
