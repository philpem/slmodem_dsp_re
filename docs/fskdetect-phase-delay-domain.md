# FSK last-phase history transfer and delayed-tap capture

Baseline80da7b0c full src/pump/v34/DPSK.c/current headers, retained Gentoo
flags and mandatory bug define. Macro-bounded loop screen newly includes this
body. Original692B, retained461B. No global/profile changes.

Original third interpolation loop73b50..73b77 reads hist[11-k], transfers
it to hist[12-k], then accumulates with intcoef3[k]. Retained interp_phase
followed by a separate shift misses that combined loop. Original delayed-tap
address is computed once73bb9 before twelve delay/multiply iterations and
used after each line shift. Retained reloads cfg->delay in the loop. Original
all interpolation/outer/delay/LPF counters compare sign-extended words.

Eight cells cross combined third-phase transfer, delayed-tap pointer captured
at the second-stage boundary, and short i/j/k (including interp_phase k used
by the first two interpolation calls). Preserve unsigned modular accumulators,
rounding, descending transfer order and the thirteenth history slot. Keep
input/output/config/offset timings otherwise unchanged. Prediction: combined
loop removes separate shift backedge; capture removes per-iteration delay
member reads; word counters reproduce observed narrowing. This is not complete
original source recovery merely because loop counts or size agree.

Full raw baseline, complete TU bodies/bindings/data/nontext controls required.
Close finite domain if all miss. No fuzzing/mutation, forced registers or
statement permutations beyond these independent graph/read/type witnesses.

Read-age audit rejects the four opening combined-loop spellings as original
preimages: original73b56 reads intcoef3[k] before history transfer73b60; the
opening source writes history then reads the writable coefficient array.
No production change was made. Preserve these controls as unsupported source
hypotheses, not source corrections. Five-cell follow-up (raw baseline plus
four short-counter/delayed-tap crosses) captures coefficient before transfer,
then accumulates its product with the captured history sample. No runtime
alias/reachability claim. This is the existing read-age discipline applied to
combined loops, not a new permission to manufacture snapshots.
