# Service constructor and cadence lifecycle review

Baseline18f4ad68, retained Gentoo profile and bug define. This reviews original
semantic boundaries left in closed exactness domains; no new spelling/flag
search. Nine complete-TU cells: dp_wrapper baseline, minimal sequential failure
with retained lookup, and existing literal creation helper/shared-failure/delete-
first form; Cadence baseline, corrected bank table, interior-on comparison,
both, original switch/map, original switch/map plus interior comparison.

Original wrapper rejects unsupported unequal rates at the first NULL converter
check before reverse dispatch. Source currently skips both checks for mode-1.
Original cadence bank indices6/7 select100_550/350_500 for subindices1..7 and
retain their450_630/100_550 fallbacks otherwise. Source omits both banks.
Original looped matcher also compares interior on durations, not just off.

Before adopting source, fixed constructor tests compare unsupported positive
unequal rates; Cadence DIAL constructor cases cover every index0..7 and every
subindex0..8, plus existing fallback cases. Original and reconstruction get
separate setup/state/filter objects. An actual BUSY constructor with cycles3,
looped matching and broad timing windows then consumes550Hz audio with fixed
on-duration sequences: equal endpoints and a longer middle tone versus the
all-equal positive control. No period arrays, filter state or internal history
are planted. A separating original rejection must be observed before accepting
the missing comparison. Preserve negative runs; do not weaken an existing gate.

Full raw baselines, all bodies/exports/binding/metadata/data/calls/bystanders.
Explicit nontext changes allowed only for removal of invented wrapper lookup
and cadence bank tables and the switch jump-table relocation destinations;
record native original eight-edge observations separately from a bounded
prover refusal. No generic unchanged-data assertion for these cells. No inferred
delay loop, forced registers, volatility, scope/order palettes, fuzzing, mutation
or tolerance changes. Final permitted period and structural checks decide.

## Original default-tone diagnostic and metadata publication

The existing fixture explicitly skips transcript comparison for tones4/9.
Native default dispatch normalises the working type to DIAL at7d501; TYPE reads
that working index at7d86e. The returned name instead reloads the descriptor and
clamps it at7d714 onward, after the report. Thus INVALID metadata and DIAL report
are distinct original values, not an out-of-bounds name-table access.

On the corrected switch/interior context, declare exactly three complete-TU
cells: unchanged context, normalised working tone/TYPE argument only, and that
change plus moving descriptor clamp/name publication after the report. Late
publication alone cannot use an uninitialised c->name for TYPE and is not a valid
independent control. Preserve all other calls/order/data/exports. Extend the
existing constructor transcript comparisons to tones4/9, assert original TYPE
DIAL at levels2/3, keep returned INVALID metadata checked, and preserve the
failing pre-fix transcript run. No new input history, printf tolerance or skip.

One further native descriptor boundary extends that three-cell diagnostic study
by one cell: original7d707..7d75f reads w6 and tone and clears continuous for
extra>0 after the report/usable gate. The source publishes all three earlier.
Add working-tone-late-descriptor, moving that complete observed bundle after
the gate; retain the earlier narrow name-only control as a negative. Four cells
in the diagnostic follow-up, thirteen total/49 shared verdicts. No permutation
of fields or validation arithmetic; the original order determines the bundle.

## Remaining semantic-hold review

The apparent FIFO capacity and frame-reversal stride holds are already adopted
and byte-exact; do not execute their original unsafe wrapping cases or reopen
them. V23's negative-count/no-sample path includes an uninitialised original
capture and supplies no valid new lifecycle witness for that closed domain.

Two still-present medium-fax boundaries are separately eligible for partial
adoption: _tx_scrambled_ones_state snapshots *tx_count at entry instead of just
before FAXVMI_process, and casts tx_bytes_per_block unsigned when the original
compares zero-extended FIFO count against signed32-bit bytes. Four current cells
cross exactly those two uses. faxvmi_hdlc_frame retains a framer owner through
opaque debug/name calls where original reloads it; baseline plus the measured
owner-at-use form, two cells. Six additional full TUs, no declaration/flags or
new protocol input. Compare native operands and complete metadata/data/calls/
bystanders; existing fixed component fixtures and final permitted period gate.
Do not invent a negative-rate context: tx_bytes_per_block's observed writer is
tx_rate/400. Any negative-byte distinction remains a static operand result,
not public reachability or a new executed negative-state claim.
