# Small call-result screen and bounded source crosses

Baseline `511a7c14`, merge of PR281. Production Gentoo: 300 sources, 300 objects,
zero failures; unchanged source/config versus its audited 1076-exact tip.
No new compiler profile; reproduce-bugs define remains enabled.

Read-only screen: 300 TUs, 1886 shared emitted bodies (copies counted), 1102 exact
copies excluded, 550 small nonexact bodies <=650 original bytes, 41 unequal
named direct-transfer multisets excluded, five candidates. Historical RX74B
fires and its current 90B exact body clears. Only named R_386_PC32 E8/E9
transfers are considered; no indirect-call, source, alias or layout inference.
Each nomination requires manual review.

K56FLEX_Create/Delete and _iir_filter_delete are closed ABI/prototype wrappers;
no return/prototype/padding edits. Two previous domains have independent axes
left for a crossed test; do not simply repeat their old singleton controls.

V90Modem::setSessionFlag: original modulator arm is a sibling jump, demodulator
arm an ordinary call/cleanup. Prior switch-first-return alone missed. Source
currently captures side before publishing sessionFlag, although fields cannot
alias and no call intervenes. Cross captured/direct side dispatch with retained
if/else versus asymmetric switch (case0 return, case1 break). Direct access
preserves the same field/type/values; no speculative pointer/parameter capture
or declaration permutation. Review initial RTL and 02.sibling separately;
a load scheduled before a store does not by itself require a source capture.

DialerAbort: original progress guard is unsigned, already observed but declined
alone (147B versus145B). Original error diagnostic is ordinary call/cleanup,
normal diagnostic a sibling jump; source's explicit error return selects a
sibling jump too. Cross retained/unsigned guard with early error return versus
structured if/else common exit. Else must contain ALL original release/normal
print code and keep call/store ordering. No new reads, values, callbacks or
status reachability claims. This is the separate terminal-control discrimination,
not an unsigned-width retry or whole-profile conclusion.

Eight complete-TU controls; each raw baseline must reproduce. Review all bodies,
imports/exports/binding, allocated nontext data/BSS/relocations and exact sets;
inspect the deciding sibling pass and review negative cells. Close this pair
of 2x2 domains on misses, not all possible code. No register constraints,
return-type guesses, widths/literals/flags or synonyms afterwards. Period gate
at batch adoption. No mutation/fuzzing execution.
