# Helper-boundary and owner transfer review

Pin ec575202 and the complete retained Gentoo profile. This is a partial
adoption review of independently recorded original boundaries, without new
spelling, option, layout or register searches. Existing negative measurements
remain valid. The explicit int AGC API and later caller recoveries now form
part of the current baseline.

Seven FPM_MRF callers: DemodDataV32/B103/V17/V21/V27/V29 and cid_modem.
Only remove the signed count conversion at MRF (CID passes original unsigned
count rather than signed ncount). Retain every other count use and the callee's
signed-word narrowing. Four callers V17/V21/V27/V29 independently consume AGC
EAX in the original rather than reloading signal; cross those two boundaries
2x2, holding all calls, argument slots and subsequent stores fixed. Other three
callers have two cells. These are22 complete-TU cells.

TxNextStateV21: baseline and owner reload after the three actual known-state
diagnostic calls, retaining its current state type and default behavior. Audit
all inlined callers and every raw bystander; smaller SIZE is not the criterion.

GenEQTrnSequenceV27: replay the existing eight cells crossing original signed
fill index, postdecrement output cursor and per-output rate reread. Preserve
unsigned caller count and original defects. Countbit15 drives original signed
index wrap/nontermination and is not a safe executed test domain. No new runtime
input history is proposed; compare operands and retain fixed existing component
coverage. No adjacent source expansions after these cells.

Total32 complete-TU cells across9 TUs. Require raw unchanged baselines, source/
header/object hashes, compiler-selected assembler/version and complete commands
with DSPLIB_REPRODUCE_BUGS last. Inspect original operands and all changed bodies,
exports/data/BSS/nontext relocations/call inventories; report exact losses and
all collaterals. Adopt only justified boundaries that pass permitted period
checks; partial fidelity is distinct from full byte identity. No fuzzing or
mutation execution; no weakening tests or original bug behavior.

The first32-cell native audit exposes a distinct V21 value-age problem: direct
member assignment stores EAX to a child pointer captured before AGC, whereas
original a5772 reloads the child after the call and a5778 stores EAX there.
Add exactly two V21 controls capturing the int return in a block local before
evaluating the destination, crossed with the retained/unsigned MRF boundary.
34 complete-TU cells total. This is an independently observed destination-age
witness, not a declaration-order or register permutation.

V22's separate resampler API has the same recorded caller-slot/use distinction:
DemodDataV22 original zero-extends its caller count into the V22_MRF_filter
slot, while the callee consumes a signed word. Review the previous2x2 formal/
caller controls on both current full TUs, eight cells, keeping the callee
countdown short and using an explicit short-at-use conversion under int formal.
All two source/header consumers are in scope; require raw equality for the
entire callee TU under the full-slot formal, no invented prototype mismatch.
No local/index/return-width alternatives after these existing controls.
Combined review denominator42 full-TU cells across11 TUs.

Review two additional published-value boundaries from the earlier contract
batch without its exact-only acceptance criterion. FPM_TONE_find_rev original
adds sample energy before subtracting history energy; replay the two existing
separate-update cells, retaining all widths and bounded component states.
GenericIIR scalar original publishes m_acc at zero and after each product;
replay only baseline/member-acc, without inline/cursor/header/profile variants.
All FloatIIR TU bodies and private template exports remain in the audit.
Existing period comparisons decide whether the retained publication also
preserves original values; no tolerance, fixture change or arbitrary numeric
input expansion is allowed. Four more complete TUs,46 total labelled cells
across13 TUs. No broader IIR inline/type/cursor family is reopened.
