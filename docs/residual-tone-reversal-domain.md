# Tone reversal energy update and sample ownership

Baseline9c020b45, fixed Gentoo profile/current headers. Original547B. At
0xab302/0xab304 original adds shifted new-square to the running energy, then
subtracts shifted old-square. Retained expression computes their difference
first. Original current sample is captured before correlation/saturation and
retained for its square and final history store, including across rev_age stores.

Four full-TU cells cross separate energy +=/-= with explicit short sample
capture at the existing first sample read. Use sample for all three current
sample references, preserving pre-read/filter-call boundaries, short sample
type, full-width unsaturated sums, separate shifted squares, rounding constants,
saturation thresholds, signed-short loop/index narrowing, output/store order.
No type/layout/flag/ABI change or local/declaration/register permutation. Capture
can matter for alias histories: blob's retained sample is the witness; ordinary
disjoint fixtures cannot establish arbitrary input/state alias equivalence.

Raw baseline required; all complete-TU bodies/data/metadata reviewed. Trace the
arithmetic boundary before considering any exact candidate; no size-only
adoption. Miss closes the finite cross. No fuzzing or mutation execution.
