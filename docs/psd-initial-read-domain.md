# PSD initial operand read boundary

Two complete-TU cells on the scalar/window winner, pinned d13949ad plus the
documented transforms. Baseline must reproduce that independent control raw.
Retained winner matches all60 original x87 rows and27 branch rows; differences
are integer entry allocation/scheduling and outgoing unsigned conversion zeros.

Original first reads overlap, then length, prepares division operands and then
derives bins. Current bins initialization reads length before overlap in initial
RTL. Test moving bins initialization after frame-count initialization. Keep
frame/pos declarations before division to preserve the independently recovered
home order. No executable write or call intervenes. This is a read-boundary
control, not a search over arbitrary local permutations or scratch cursors.

Prediction: changed initial pseudo/read graph may recover the original entry
allocation/scheduling. Falsifier: entry remains different, original home/x87/
branch graph lost, or unsupported read behavior appears. No further order
controls without another original witness. Fixed profile, full-TU/data audit,
period batch gate and no fuzz/mutation execution.
