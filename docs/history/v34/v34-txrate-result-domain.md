# V34 transmit-rate result ownership after RX success

New discrimination on b3665999: RX's crossed result/entry-pointer ownership
recovers an exact body; TX direct ratecfg fallback alone does not (202B vs213B,
saved EBX remains). A final int result might explain the TX register/frame
ownership too. This is a separate result-flow axis, not a pointer synonym or
literal/type permutation. Its actual fallback and guard arithmetic stay fixed.

Five full-TU controls: raw baseline; exact RX seed with TX baseline; then on
that seed TX shared result only, direct fallback only, both. The repeated raw
RX-only and direct-fallback controls must reproduce prior objects exactly.
Nested role/status branches set one int result; each preserved unsigned float
conversion and zero guard keeps its decision. Ordinary ratecfg fallback can
appear in the source's mutually exclusive else arms; review whether compiler
merges them to the original single load. No unguarded fallback value load.

Prediction: shared result removes the unobserved long-lived owner/saved register
or combines with direct fallback. Reject on complete-body differences, preserve
negative controls, close the full 2x2 TX domain. Audit every body/data/export and
relocation; no register, declaration, literal, flags or guard-polarity sweeps.
No mutation/fuzzing execution. Source gate for any independently justified gain.
