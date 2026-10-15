# PSD scalar arithmetic/load ownership control

Pin d13949ad plus the tested loop-expression normalization/float-peak cell
axis-1-1 from search-psd-boundaries. Four PSD full-TU cells: baseline, ordinary
scalar division, reversed window-multiply source operands, both. Fixed profile
and mandatory bug define; baseline must reproduce the independently compiled
axis-1-1 object raw. No cursor/declaration/option/layout permutations.

New witness: baseline635B equals original size and has original guarded
reciprocal/peak fcom, but each output loop duplicates the reciprocal then uses
fmuls memory where original flds bin followed by register fmul. Test ordinary
out[i]/peak and out[i]/frames with existing float logarithm narrowing and all
constants unchanged. Peak is float; frames remains unsigned. This changes
expression modes, so matching loads alone is insufficient: inspect RTL for
extra rounding and period differential must decide adoption. Original does
not narrow division before logarithm/linear store. Preserve invalid/NaN results.

Separately original window loop loads input then fmuls window; candidate loads
window then fmuls input. Test m_window[j]*in[pos+j]. Only these operands swap;
do not change sample/window types, indices, stores or FFT. Multiplication of
normal floats is commutative, but no universal NaN payload claim follows.

Prediction: natural scalar modes and operand expression ordering recover
original x87 operand ownership. Falsifier: expressions fold inert, extra
narrowing, changed period results or original graph not recovered. Full-TU
data/bindings/other bodies must be audited. No fuzz/mutation execution. Report
partial/byte-exact outcomes separately and close this finite domain.
